#include "PluginEditor.h"

//==============================================================================
// PatternGrid
//==============================================================================

PatternGrid::PatternGrid (WillyBeatAudioProcessor& p) : proc (p)
{
    startTimerHz (30);
}

PatternGrid::~PatternGrid()
{
    stopTimer();
}

juce::Colour PatternGrid::velColour (uint8_t vel)
{
    if (vel == 0)   return juce::Colour (0xff1a1d2e);  // off — dark
    if (vel <= 30)  return juce::Colour (0xff2a3060);  // ghost
    if (vel <= 65)  return juce::Colour (0xff3a5090);  // soft
    if (vel <= 90)  return juce::Colour (0xff5578cc);  // medium
    if (vel <= 110) return juce::Colour (0xff7a9fff);  // hard
    return              juce::Colour (0xffb07fff);     // accent
}

void PatternGrid::paint (juce::Graphics& g)
{
    const auto* displayPat = (editTarget != nullptr) ? editTarget
                                                     : proc.getActivePattern();

    const auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff141625));

    const int gridX = kLabelW;
    const int gridW = bounds.getWidth() - gridX;
    const float cellW = (float) gridW / MAX_STEPS;
    const float cellH = (float) bounds.getHeight() / NUM_TRACKS;

    const int playStep = proc.getCurrentStep().load();

    // Current-step column highlight (only in playback mode)
    if (editTarget == nullptr && playStep >= 0 && playStep < MAX_STEPS)
    {
        float cx = gridX + playStep * cellW;
        g.setColour (juce::Colour (0x22ffffff));
        g.fillRect (cx, 0.0f, cellW, (float) bounds.getHeight());
    }

    // Beat separator lines
    g.setColour (juce::Colour (0xff2a2d3e));
    for (int col = 0; col <= MAX_STEPS; col += 4)
        g.drawVerticalLine ((int) (gridX + col * cellW), 0.0f, (float) bounds.getHeight());

    // Cells
    for (int row = 0; row < NUM_TRACKS; ++row)
    {
        float cy = row * cellH;
        for (int col = 0; col < MAX_STEPS; ++col)
        {
            float cx = gridX + col * cellW;
            uint8_t vel = (displayPat != nullptr) ? displayPat->velocities[row][col] : 0;

            juce::Colour fill = velColour (vel);
            if (editTarget == nullptr && col == playStep)
                fill = fill.brighter (0.3f);

            g.setColour (fill);
            g.fillRect (cx + 1.0f, cy + 1.0f, cellW - 2.0f, cellH - 2.0f);
        }
    }

    // Track labels
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    for (int row = 0; row < NUM_TRACKS; ++row)
    {
        float cy = row * cellH;
        g.setColour (juce::Colours::lightgrey);
        g.drawText (kTrackNames[row],
                    0, (int) cy, kLabelW - 4, (int) cellH,
                    juce::Justification::centredRight, true);
    }

    // Row dividers
    g.setColour (juce::Colour (0xff2a2d3e));
    for (int row = 0; row <= NUM_TRACKS; ++row)
        g.drawHorizontalLine ((int) (row * cellH), (float) gridX, (float) bounds.getWidth());

    // Edit-mode cursor outline
    if (editTarget != nullptr)
    {
        g.setColour (juce::Colour (0x55a070ff));
        g.drawRect (getLocalBounds().toFloat(), 1.5f);
    }
}

void PatternGrid::setEditTarget (DrumPattern* target)
{
    editTarget = target;
    repaint();
}

void PatternGrid::mouseDown (const juce::MouseEvent& e)
{
    if (editTarget == nullptr) return;

    const auto bounds = getLocalBounds();
    const float cellW = (float) (bounds.getWidth() - kLabelW) / MAX_STEPS;
    const float cellH = (float) bounds.getHeight() / NUM_TRACKS;

    if (e.x < kLabelW) return;

    int col = (int) ((e.x - kLabelW) / cellW);
    int row = (int) (e.y / cellH);
    if (col < 0 || col >= MAX_STEPS || row < 0 || row >= NUM_TRACKS) return;

    auto& vel = editTarget->velocities[row][col];

    if (e.mods.isRightButtonDown())
    {
        vel = 0;
    }
    else
    {
        // Cycle: off → medium → hard → accent → ghost → soft → off
        static const uint8_t cycle[] = { 0, 80, 100, 120, 25, 55 };
        constexpr int n = 6;
        int cur = 0;
        for (int i = 0; i < n; ++i)
            if (vel == cycle[i]) { cur = i; break; }
        vel = cycle[(cur + 1) % n];
    }

    repaint();
}

void PatternGrid::timerCallback()
{
    if (editTarget != nullptr) return;  // not animating playback in edit mode
    int step = proc.getCurrentStep().load();
    if (step != lastStep)
    {
        lastStep = step;
        repaint();
    }
}

//==============================================================================
// WillyBeatAudioProcessorEditor
//==============================================================================

WillyBeatAudioProcessorEditor::WillyBeatAudioProcessorEditor (WillyBeatAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), grid (p)
{
    // Populate combo boxes
    for (int i = 0; i < (int) Genre::NUM_GENRES; ++i)
        genreBox.addItem (kGenreNames[i], i + 1);
    for (int i = 0; i < (int) PatType::NUM_TYPES; ++i)
        typeBox.addItem (kPatTypeNames[i], i + 1);

    genreAttach  = std::make_unique<CBA> (p.apvts, "genre",   genreBox);
    typeAttach   = std::make_unique<CBA> (p.apvts, "patType", typeBox);
    patIdxAttach    = std::make_unique<SA> (p.apvts, "patIdx",    patIdxSlider);
    gateAttach      = std::make_unique<SA> (p.apvts, "gate",      gateKnob);
    humanizeAttach  = std::make_unique<SA> (p.apvts, "humanize",  humanizeKnob);

    patIdxSlider.setSliderStyle (juce::Slider::IncDecButtons);
    patIdxSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);

    gateKnob.setSliderStyle (juce::Slider::Rotary);
    gateKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 18);

    humanizeKnob.setSliderStyle (juce::Slider::Rotary);
    humanizeKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 18);

    auto labelStyle = [](juce::Label* lbl)
    {
        lbl->setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setJustificationType (juce::Justification::centred);
    };
    for (auto* lbl : { &genreLabel, &typeLabel, &patLabel, &gateLabel, &humanizeLabel })
        labelStyle (lbl);

    loadMidiBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load drum MIDI file", juce::File{}, "*.mid,*.midi");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                    audioProcessor.loadMidiFile (f);
            });
    };

    genVarBtn.onClick  = [this] { audioProcessor.generateVariation(); };
    editBtn.onClick    = [this] { editMode ? exitEditMode (false) : enterEditMode(); };

    // Edit-mode toolbar — add as child components (initially invisible)
    nameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
    nameLabel.setJustificationType (juce::Justification::centredRight);
    nameEditor.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f)));
    nameEditor.setColour (juce::TextEditor::backgroundColourId,  juce::Colour (0xff1e2035));
    nameEditor.setColour (juce::TextEditor::outlineColourId,     juce::Colour (0xff504070));
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffa070ff));
    nameEditor.setColour (juce::TextEditor::textColourId,        juce::Colours::white);

    saveBtn.onClick   = [this] { exitEditMode (true); };
    cancelBtn.onClick = [this] { exitEditMode (false); };
    newPatBtn.onClick = [this]
    {
        editingCopy = DrumPattern{};
        editingCopy.name  = "New Pattern";
        editingCopy.genre = (Genre)  (int) audioProcessor.apvts.getRawParameterValue ("genre")->load();
        editingCopy.type  = (PatType)(int) audioProcessor.apvts.getRawParameterValue ("patType")->load();
        nameEditor.setText (editingCopy.name);
        grid.repaint();
    };
    openFolderBtn.onClick = [this]
    {
        audioProcessor.getPresetsDirectory().startAsProcess();
    };

    juce::Component* editComps[] = { &nameLabel, &nameEditor,
                                      &saveBtn, &cancelBtn, &newPatBtn, &openFolderBtn };
    for (auto* c : editComps)
        addChildComponent (c);

    addAndMakeVisible (grid);
    addAndMakeVisible (genreLabel);  addAndMakeVisible (genreBox);
    addAndMakeVisible (typeLabel);   addAndMakeVisible (typeBox);
    addAndMakeVisible (patLabel);    addAndMakeVisible (patIdxSlider);
    addAndMakeVisible (gateLabel);   addAndMakeVisible (gateKnob);
    addAndMakeVisible (humanizeLabel);
    addAndMakeVisible (humanizeKnob);
    addAndMakeVisible (loadMidiBtn);
    addAndMakeVisible (genVarBtn);
    addAndMakeVisible (editBtn);

    setSize (720, 530);
}

WillyBeatAudioProcessorEditor::~WillyBeatAudioProcessorEditor()
{
    grid.setEditTarget (nullptr);
}

//==============================================================================

void WillyBeatAudioProcessorEditor::enterEditMode()
{
    auto* pat = audioProcessor.getActivePattern();
    if (pat == nullptr) return;

    editingCopy = *pat;
    editMode    = true;

    nameEditor.setText (editingCopy.name, false);
    grid.setEditTarget (&editingCopy);
    editBtn.setButtonText ("Exit Edit");

    juce::Component* editComps1[] = { &nameLabel, &nameEditor,
                                       &saveBtn, &cancelBtn, &newPatBtn, &openFolderBtn };
    for (auto* c : editComps1)
        c->setVisible (true);

    resized();
    repaint();
}

void WillyBeatAudioProcessorEditor::exitEditMode (bool save)
{
    if (save)
    {
        editingCopy.name = nameEditor.getText().trim();
        if (editingCopy.name.isEmpty()) editingCopy.name = "Custom Pattern";
        audioProcessor.saveEditedPattern (editingCopy);
    }

    editMode = false;
    grid.setEditTarget (nullptr);
    editBtn.setButtonText ("Edit");

    juce::Component* editComps2[] = { &nameLabel, &nameEditor,
                                       &saveBtn, &cancelBtn, &newPatBtn, &openFolderBtn };
    for (auto* c : editComps2)
        c->setVisible (false);

    resized();
    repaint();
}

//==============================================================================

void WillyBeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141625));
}

void WillyBeatAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // ── Top controls row ──────────────────────────────────────────────────
    auto topRow = area.removeFromTop (70);

    auto genreArea = topRow.removeFromLeft (130);
    genreLabel.setBounds (genreArea.removeFromTop (20));
    genreBox  .setBounds (genreArea.removeFromTop (28));
    topRow.removeFromLeft (8);

    auto typeArea = topRow.removeFromLeft (120);
    typeLabel.setBounds (typeArea.removeFromTop (20));
    typeBox  .setBounds (typeArea.removeFromTop (28));
    topRow.removeFromLeft (8);

    auto patArea = topRow.removeFromLeft (100);
    patLabel    .setBounds (patArea.removeFromTop (20));
    patIdxSlider.setBounds (patArea.removeFromTop (28));
    topRow.removeFromLeft (8);

    auto gateArea = topRow.removeFromLeft (65);
    gateLabel.setBounds (gateArea.removeFromTop (20));
    gateKnob .setBounds (gateArea);
    topRow.removeFromLeft (8);

    auto humanArea = topRow.removeFromLeft (65);
    humanizeLabel.setBounds (humanArea.removeFromTop (20));
    humanizeKnob .setBounds (humanArea);
    topRow.removeFromLeft (10);

    // Buttons stacked on the right
    {
        auto col = topRow.removeFromLeft (90);
        loadMidiBtn.setBounds (col.removeFromTop (30));
        col.removeFromTop (4);
        genVarBtn  .setBounds (col.removeFromTop (30));
    }
    topRow.removeFromLeft (6);
    editBtn.setBounds (topRow.removeFromLeft (70).withHeight (30));

    area.removeFromTop (4);

    // ── Edit-mode toolbar ─────────────────────────────────────────────────
    if (editMode)
    {
        auto editRow = area.removeFromTop (34);
        nameLabel .setBounds (editRow.removeFromLeft (44));
        editRow.removeFromLeft (4);
        nameEditor.setBounds (editRow.removeFromLeft (180));
        editRow.removeFromLeft (8);
        saveBtn      .setBounds (editRow.removeFromLeft (55).withHeight (26));
        editRow.removeFromLeft (4);
        cancelBtn    .setBounds (editRow.removeFromLeft (55).withHeight (26));
        editRow.removeFromLeft (4);
        newPatBtn    .setBounds (editRow.removeFromLeft (85).withHeight (26));
        editRow.removeFromLeft (4);
        openFolderBtn.setBounds (editRow.removeFromLeft (100).withHeight (26));
        area.removeFromTop (4);
    }
    else
    {
        // Zero-out bounds so hidden components don't intercept clicks
        juce::Component* editComps3[] = { &nameLabel, &nameEditor,
                                           &saveBtn, &cancelBtn, &newPatBtn, &openFolderBtn };
        for (auto* c : editComps3)
            c->setBounds ({});
    }

    // ── Pattern grid fills remaining space ────────────────────────────────
    grid.setBounds (area);
}

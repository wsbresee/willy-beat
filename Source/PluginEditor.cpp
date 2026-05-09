#include "PluginEditor.h"

//==============================================================================
// Helpers
//==============================================================================

// Build a MIDI file from mainPat repeated numBars times.
// The last bar uses fillPat (if addFill && fillPat != nullptr).
// Swing/humanize/feel are baked in; each bar gets a different random seed so
// every loop sounds slightly different.  seed == -1 → new random seed each call.
static juce::File writePatternToMidi (const DrumPattern& mainPat,
                                      const DrumPattern* fillPat,
                                      int          numBars,
                                      bool         addFill,
                                      juce::int64  seed,
                                      float        humanize,
                                      float        swing,
                                      float        feel)
{
    const int    ticksPerStep = 120;    // 480 ticks/QN, 16th = 120
    const int    stepsPerBar  = MAX_STEPS;
    const int    ticksPerBar  = ticksPerStep * stepsPerBar;
    const double swingTicks   = swing   / 100.0 * ticksPerStep * 0.5;
    const double maxFeelTicks = feel    / 100.0 * ticksPerStep * 0.08;
    const double gateFrac     = 0.80;

    juce::int64 usedSeed = (seed < 0)
        ? juce::Random::getSystemRandom().nextInt64()
        : seed;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);

    for (int bar = 0; bar < numBars; ++bar)
    {
        const DrumPattern& pat = (addFill && fillPat && bar == numBars - 1)
                                     ? *fillPat : mainPat;

        // Mix seed with bar index so each repetition sounds different
        juce::int64 barSeed = usedSeed ^ ((juce::int64)(bar + 1) * (juce::int64)0x9e3779b97f4a7c15LL);
        juce::Random barRng (barSeed);

        int numSteps = (pat.numSteps > 0 && pat.numSteps <= MAX_STEPS)
                           ? pat.numSteps : MAX_STEPS;

        for (int col = 0; col < numSteps; ++col)
        {
            // Step tick with swing on odd 16th positions
            double stepTick = (double)(bar * ticksPerBar + col * ticksPerStep);
            if (col % 2 == 1) stepTick += swingTicks;

            for (int row = 0; row < NUM_TRACKS; ++row)
            {
                uint8_t vel = pat.velocities[row][col];
                if (vel == 0) continue;

                // Velocity humanization
                if (humanize > 0.0f)
                {
                    int h   = (int) humanize;
                    int dev = barRng.nextInt (2 * h + 1) - h;
                    vel = (uint8_t) juce::jlimit (1, 127, (int) vel + dev);
                }

                // Timing feel — bell-curve distribution, velocity-weighted
                double noteTick = stepTick;
                if (maxFeelTicks > 0.0)
                {
                    double r = (barRng.nextDouble() + barRng.nextDouble()) * 0.5 - 0.5;
                    double velFactor = 1.0 - 0.4 * ((double) vel / 127.0);
                    noteTick += r * maxFeelTicks * velFactor;
                }

                double onTick  = juce::jmax (0.0, noteTick);
                double offTick = onTick + ticksPerStep * gateFrac;

                seq.addEvent (juce::MidiMessage::noteOn  (10, kTrackNotes[row], vel), onTick);
                seq.addEvent (juce::MidiMessage::noteOff (10, kTrackNotes[row], (uint8_t) 0), offTick);
            }
        }
    }

    seq.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (480);
    midiFile.addTrack (seq);

    juce::String safeName = mainPat.name.replaceCharacters ("/\\:*?\"<>|", "_________").trim();
    if (safeName.isEmpty()) safeName = "pattern";

    auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("WillyBeat_" + safeName + ".mid");

    if (auto os = tempFile.createOutputStream())
        midiFile.writeTo (*os);

    return tempFile;
}

//==============================================================================
// DragStrip
//==============================================================================

void DragStrip::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (hovered ? juce::Colour (0xff251840) : juce::Colour (0xff1a1530));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (juce::Colour (0xff6040a0));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setColour (juce::Colour (0xffaaaacc));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\xa0\xbf  Drag Pattern to DAW  \xe2\xa0\xbf"),
                getLocalBounds(), juce::Justification::centred, false);
}

void DragStrip::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void DragStrip::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }
void DragStrip::mouseDown  (const juce::MouseEvent&) { dragStarted = false; }

void DragStrip::mouseDrag (const juce::MouseEvent&)
{
    if (dragStarted || !onDrag) return;

    auto tempFile = onDrag();
    if (! tempFile.existsAsFile()) return;

    dragStarted = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        { tempFile.getFullPathName() }, false, this,
        [tempFile]() mutable { tempFile.deleteFile(); });
}

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
    if (vel == 0)   return juce::Colour (0xff1a1d2e);
    if (vel <= 30)  return juce::Colour (0xff2a3060);
    if (vel <= 65)  return juce::Colour (0xff3a5090);
    if (vel <= 90)  return juce::Colour (0xff5578cc);
    if (vel <= 110) return juce::Colour (0xff7a9fff);
    return              juce::Colour (0xffb07fff);
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

    if (editTarget == nullptr && playStep >= 0 && playStep < MAX_STEPS)
    {
        float cx = gridX + playStep * cellW;
        g.setColour (juce::Colour (0x22ffffff));
        g.fillRect (cx, 0.0f, cellW, (float) bounds.getHeight());
    }

    g.setColour (juce::Colour (0xff2a2d3e));
    for (int col = 0; col <= MAX_STEPS; col += 4)
        g.drawVerticalLine ((int) (gridX + col * cellW), 0.0f, (float) bounds.getHeight());

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

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    for (int row = 0; row < NUM_TRACKS; ++row)
    {
        float cy = row * cellH;
        g.setColour (juce::Colours::lightgrey);
        g.drawText (kTrackNames[row],
                    0, (int) cy, kLabelW - 4, (int) cellH,
                    juce::Justification::centredRight, true);
    }

    g.setColour (juce::Colour (0xff2a2d3e));
    for (int row = 0; row <= NUM_TRACKS; ++row)
        g.drawHorizontalLine ((int) (row * cellH), (float) gridX, (float) bounds.getWidth());

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
        static const uint8_t cycle[] = { 0, 80, 100, 120, 25, 55 };
        constexpr int n = 6;
        int cur = 0;
        for (int i = 0; i < n; ++i)
            if (vel == cycle[i]) { cur = i; break; }
        vel = cycle[(cur + 1) % n];
    }

    repaint();

    if (onCellChanged)
        onCellChanged();
}

void PatternGrid::timerCallback()
{
    if (editTarget != nullptr) return;
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
    // Pattern selector controls
    for (int i = 0; i < (int) Genre::NUM_GENRES; ++i)
        genreBox.addItem (kGenreNames[i], i + 1);
    for (int i = 0; i < (int) PatType::NUM_TYPES; ++i)
        typeBox.addItem (kPatTypeNames[i], i + 1);

    genreAttach  = std::make_unique<CBA> (p.apvts, "genre",   genreBox);
    typeAttach   = std::make_unique<CBA> (p.apvts, "patType", typeBox);
    patIdxAttach    = std::make_unique<SA> (p.apvts, "patIdx",    patIdxSlider);
    gateAttach      = std::make_unique<SA> (p.apvts, "gate",      gateKnob);
    humanizeAttach  = std::make_unique<SA> (p.apvts, "humanize",  humanizeKnob);
    swingAttach     = std::make_unique<SA> (p.apvts, "swing",     swingKnob);
    feelAttach      = std::make_unique<SA> (p.apvts, "feel",      feelKnob);

    patIdxSlider.setSliderStyle (juce::Slider::IncDecButtons);
    patIdxSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);

    for (auto* k : { &gateKnob, &humanizeKnob, &swingKnob, &feelKnob })
    {
        k->setSliderStyle (juce::Slider::Rotary);
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 18);
    }

    auto labelStyle = [] (juce::Label* lbl)
    {
        lbl->setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setJustificationType (juce::Justification::centred);
    };
    for (auto* lbl : { &genreLabel, &typeLabel, &patLabel,
                       &gateLabel, &humanizeLabel, &swingLabel, &feelLabel })
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

    genVarBtn.onClick = [this] { audioProcessor.generateVariation(); };
    editBtn.onClick   = [this] { editMode ? exitEditMode() : enterEditMode(); };

    // Edit-mode toolbar
    nameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
    nameLabel.setJustificationType (juce::Justification::centredRight);
    nameEditor.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f)));
    nameEditor.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (0xff1e2035));
    nameEditor.setColour (juce::TextEditor::outlineColourId,        juce::Colour (0xff504070));
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffa070ff));
    nameEditor.setColour (juce::TextEditor::textColourId,           juce::Colours::white);

    doneBtn.onClick    = [this] { exitEditMode(); };
    newPatBtn.onClick  = [this]
    {
        editingCopy      = DrumPattern{};
        editingCopy.name = "New Pattern";
        editingCopy.genre = (Genre)   (int) audioProcessor.apvts.getRawParameterValue ("genre")->load();
        editingCopy.type  = (PatType) (int) audioProcessor.apvts.getRawParameterValue ("patType")->load();
        nameEditor.setText (editingCopy.name);
        audioProcessor.autoSavePattern (editingCopy);
        grid.repaint();
    };
    openFolderBtn.onClick = [this] { audioProcessor.getPresetsDirectory().startAsProcess(); };

    juce::Component* editComps[] = { &nameLabel, &nameEditor,
                                     &doneBtn, &newPatBtn, &openFolderBtn };
    for (auto* c : editComps)
        addChildComponent (c);

    // Export / drag controls
    exportBarsBox.addItem ("1 bar",   1);
    exportBarsBox.addItem ("2 bars",  2);
    exportBarsBox.addItem ("4 bars",  3);
    exportBarsBox.addItem ("8 bars",  4);
    exportBarsBox.addItem ("16 bars", 5);
    exportBarsBox.setSelectedId (3);  // default: 4 bars

    auto exportLabelStyle = [] (juce::Label* lbl)
    {
        lbl->setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setJustificationType (juce::Justification::centredRight);
    };
    exportLabelStyle (&barsLabel);
    exportLabelStyle (&seedLabel);

    seedEditor.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    seedEditor.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (0xff1e2035));
    seedEditor.setColour (juce::TextEditor::outlineColourId,        juce::Colour (0xff504070));
    seedEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffa070ff));
    seedEditor.setColour (juce::TextEditor::textColourId,           juce::Colours::white);
    seedEditor.setTextToShowWhenEmpty ("random", juce::Colour (0xff666699));
    seedEditor.setInputRestrictions (18, "0123456789");

    fillToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffaaaacc));

    // Wire up the drag strip
    dragStrip.onDrag = [this]() -> juce::File
    {
        auto* pat = audioProcessor.getActivePattern();
        if (! pat) return {};

        int         numBars  = getBarsFromCombo();
        bool        addFill  = fillToggle.getToggleState();
        juce::int64 seed     = getSeedFromEditor();

        const DrumPattern* fillPat = addFill ? findFill (*pat) : nullptr;

        float humanize = audioProcessor.apvts.getRawParameterValue ("humanize")->load();
        float swing    = audioProcessor.apvts.getRawParameterValue ("swing")->load();
        float feel     = audioProcessor.apvts.getRawParameterValue ("feel")->load();

        return writePatternToMidi (*pat, fillPat, numBars, addFill, seed,
                                   humanize, swing, feel);
    };

    addAndMakeVisible (dragStrip);
    addAndMakeVisible (grid);
    addAndMakeVisible (genreLabel);     addAndMakeVisible (genreBox);
    addAndMakeVisible (typeLabel);      addAndMakeVisible (typeBox);
    addAndMakeVisible (patLabel);       addAndMakeVisible (patIdxSlider);
    addAndMakeVisible (gateLabel);      addAndMakeVisible (gateKnob);
    addAndMakeVisible (humanizeLabel);  addAndMakeVisible (humanizeKnob);
    addAndMakeVisible (swingLabel);     addAndMakeVisible (swingKnob);
    addAndMakeVisible (feelLabel);      addAndMakeVisible (feelKnob);
    addAndMakeVisible (loadMidiBtn);
    addAndMakeVisible (genVarBtn);
    addAndMakeVisible (editBtn);
    addAndMakeVisible (barsLabel);      addAndMakeVisible (exportBarsBox);
    addAndMakeVisible (fillToggle);
    addAndMakeVisible (seedLabel);      addAndMakeVisible (seedEditor);

    setSize (760, 562);
}

WillyBeatAudioProcessorEditor::~WillyBeatAudioProcessorEditor()
{
    grid.setEditTarget (nullptr);
    grid.onCellChanged = nullptr;
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

    // Auto-save every cell edit
    grid.onCellChanged = [this] { autoSaveCurrentEdit(); };

    juce::Component* editComps[] = { &nameLabel, &nameEditor,
                                     &doneBtn, &newPatBtn, &openFolderBtn };
    for (auto* c : editComps)
        c->setVisible (true);

    resized();
    repaint();
}

void WillyBeatAudioProcessorEditor::exitEditMode()
{
    // Final save (handles any name change committed in the text field)
    editingCopy.name = nameEditor.getText().trim();
    if (editingCopy.name.isEmpty()) editingCopy.name = "Custom Pattern";
    audioProcessor.saveEditedPattern (editingCopy);

    editMode = false;
    grid.setEditTarget (nullptr);
    grid.onCellChanged = nullptr;
    editBtn.setButtonText ("Edit");

    juce::Component* editComps[] = { &nameLabel, &nameEditor,
                                     &doneBtn, &newPatBtn, &openFolderBtn };
    for (auto* c : editComps)
        c->setVisible (false);

    resized();
    repaint();
}

void WillyBeatAudioProcessorEditor::autoSaveCurrentEdit()
{
    audioProcessor.autoSavePattern (editingCopy);
}

//==============================================================================

int WillyBeatAudioProcessorEditor::getBarsFromCombo() const
{
    switch (exportBarsBox.getSelectedId())
    {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 8;
        case 5: return 16;
        default: return 4;
    }
}

juce::int64 WillyBeatAudioProcessorEditor::getSeedFromEditor() const
{
    auto text = seedEditor.getText().trim();
    if (text.isEmpty()) return -1;
    return (juce::int64) text.getLargeIntValue();
}

const DrumPattern* WillyBeatAudioProcessorEditor::findFill (const DrumPattern& mainPat) const
{
    for (auto type : { PatType::SmallFill, PatType::BigFill })
    {
        auto fills = audioProcessor.getLibrary().get (mainPat.genre, type);
        if (! fills.empty()) return fills[0];
    }
    return nullptr;
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

    auto genreArea = topRow.removeFromLeft (115);
    genreLabel.setBounds (genreArea.removeFromTop (20));
    genreBox  .setBounds (genreArea.removeFromTop (28));
    topRow.removeFromLeft (6);

    auto typeArea = topRow.removeFromLeft (105);
    typeLabel.setBounds (typeArea.removeFromTop (20));
    typeBox  .setBounds (typeArea.removeFromTop (28));
    topRow.removeFromLeft (6);

    auto patArea = topRow.removeFromLeft (86);
    patLabel    .setBounds (patArea.removeFromTop (20));
    patIdxSlider.setBounds (patArea.removeFromTop (28));
    topRow.removeFromLeft (6);

    auto gateArea = topRow.removeFromLeft (56);
    gateLabel.setBounds (gateArea.removeFromTop (20));
    gateKnob .setBounds (gateArea);
    topRow.removeFromLeft (5);

    auto humanArea = topRow.removeFromLeft (56);
    humanizeLabel.setBounds (humanArea.removeFromTop (20));
    humanizeKnob .setBounds (humanArea);
    topRow.removeFromLeft (5);

    auto swingArea = topRow.removeFromLeft (56);
    swingLabel.setBounds (swingArea.removeFromTop (20));
    swingKnob .setBounds (swingArea);
    topRow.removeFromLeft (5);

    auto feelArea = topRow.removeFromLeft (56);
    feelLabel.setBounds (feelArea.removeFromTop (20));
    feelKnob .setBounds (feelArea);
    topRow.removeFromLeft (8);

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
        doneBtn      .setBounds (editRow.removeFromLeft (55).withHeight (26));
        editRow.removeFromLeft (4);
        newPatBtn    .setBounds (editRow.removeFromLeft (85).withHeight (26));
        editRow.removeFromLeft (4);
        openFolderBtn.setBounds (editRow.removeFromLeft (100).withHeight (26));
        area.removeFromTop (4);
    }
    else
    {
        juce::Component* editComps[] = { &nameLabel, &nameEditor,
                                         &doneBtn, &newPatBtn, &openFolderBtn };
        for (auto* c : editComps)
            c->setBounds ({});
    }

    // ── Export controls row ───────────────────────────────────────────────
    {
        auto exportRow = area.removeFromBottom (26);
        area.removeFromBottom (4);

        barsLabel    .setBounds (exportRow.removeFromLeft (34).withHeight (20).withY (exportRow.getY() + 3));
        exportBarsBox.setBounds (exportRow.removeFromLeft (72).reduced (0, 2));
        exportRow.removeFromLeft (10);
        fillToggle   .setBounds (exportRow.removeFromLeft (90));
        exportRow.removeFromLeft (10);
        seedLabel    .setBounds (exportRow.removeFromLeft (36).withHeight (20).withY (exportRow.getY() + 3));
        seedEditor   .setBounds (exportRow.removeFromLeft (110).reduced (0, 3));
    }

    // ── Drag strip at bottom ──────────────────────────────────────────────
    dragStrip.setBounds (area.removeFromBottom (26));
    area.removeFromBottom (4);

    // ── Pattern grid fills remaining space ────────────────────────────────
    grid.setBounds (area);
}

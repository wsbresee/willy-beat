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
    if (vel == 0)  return juce::Colour (0xff1a1d2e);  // off — dark
    if (vel <= 30) return juce::Colour (0xff2a3060);  // ghost
    if (vel <= 65) return juce::Colour (0xff3a5090);  // soft
    if (vel <= 90) return juce::Colour (0xff5578cc);  // medium
    if (vel <= 110)return juce::Colour (0xff7a9fff);  // hard
    return             juce::Colour (0xffb07fff);     // accent
}

void PatternGrid::paint (juce::Graphics& g)
{
    const auto* pat = proc.getActivePattern();

    const auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff141625));

    constexpr int labelW = 68;
    constexpr int numCols = MAX_STEPS;
    constexpr int numRows = NUM_TRACKS;
    const int gridX = labelW;
    const int gridW = bounds.getWidth() - labelW;
    const float cellW = (float) gridW / numCols;
    const float cellH = (float) bounds.getHeight() / numRows;

    const int playStep = proc.getCurrentStep().load();

    // Column highlight for current step
    if (playStep >= 0 && playStep < numCols)
    {
        float cx = gridX + playStep * cellW;
        g.setColour (juce::Colour (0x20ffffff));
        g.fillRect (cx, 0.0f, cellW, (float) bounds.getHeight());
    }

    // Beat separator lines (every 4 steps)
    g.setColour (juce::Colour (0xff2a2d3e));
    for (int col = 0; col <= numCols; col += 4)
    {
        float x = gridX + col * cellW;
        g.drawVerticalLine ((int) x, 0.0f, (float) bounds.getHeight());
    }

    // Cells
    for (int row = 0; row < numRows; ++row)
    {
        float cy = row * cellH;
        for (int col = 0; col < numCols; ++col)
        {
            float cx = gridX + col * cellW;
            uint8_t vel = (pat != nullptr) ? pat->velocities[row][col] : 0;

            juce::Colour fill = velColour (vel);

            // Brighten the active step cell
            if (col == playStep)
                fill = fill.brighter (0.3f);

            g.setColour (fill);
            g.fillRect (cx + 1.0f, cy + 1.0f, cellW - 2.0f, cellH - 2.0f);
        }
    }

    // Track labels
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    for (int row = 0; row < numRows; ++row)
    {
        float cy = row * cellH;
        g.setColour (juce::Colours::lightgrey);
        g.drawText (kTrackNames[row],
                    0, (int) cy, labelW - 4, (int) cellH,
                    juce::Justification::centredRight, true);
    }

    // Row dividers
    g.setColour (juce::Colour (0xff2a2d3e));
    for (int row = 0; row <= numRows; ++row)
        g.drawHorizontalLine ((int) (row * cellH), (float) gridX, (float) bounds.getWidth());
}

void PatternGrid::timerCallback()
{
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
    // Populate combo boxes — item IDs must be 1-based for APVTS attachment
    for (int i = 0; i < (int) Genre::NUM_GENRES; ++i)
        genreBox.addItem (kGenreNames[i], i + 1);
    for (int i = 0; i < (int) PatType::NUM_TYPES; ++i)
        typeBox.addItem (kPatTypeNames[i], i + 1);

    // APVTS attachments
    genreAttach  = std::make_unique<CBA> (p.apvts, "genre",   genreBox);
    typeAttach   = std::make_unique<CBA> (p.apvts, "patType", typeBox);
    patIdxAttach = std::make_unique<SA>  (p.apvts, "patIdx",  patIdxSlider);
    gateAttach   = std::make_unique<SA>  (p.apvts, "gate",    gateKnob);

    // Pattern index slider
    patIdxSlider.setSliderStyle (juce::Slider::IncDecButtons);
    patIdxSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);

    // Gate knob
    gateKnob.setSliderStyle (juce::Slider::Rotary);
    gateKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 18);

    // Labels
    for (auto* lbl : { &genreLabel, &typeLabel, &patLabel, &gateLabel })
    {
        lbl->setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
    }

    loadMidiBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load drum MIDI file", juce::File{}, "*.mid,*.midi");
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                    audioProcessor.loadMidiFile (f);
            });
    };

    genVarBtn.onClick = [this]
    {
        audioProcessor.generateVariation();
    };

    addAndMakeVisible (grid);
    addAndMakeVisible (genreBox);
    addAndMakeVisible (typeBox);
    addAndMakeVisible (patIdxSlider);
    addAndMakeVisible (gateKnob);
    addAndMakeVisible (loadMidiBtn);
    addAndMakeVisible (genVarBtn);

    setSize (720, 490);
}

WillyBeatAudioProcessorEditor::~WillyBeatAudioProcessorEditor() {}

void WillyBeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141625));
}

void WillyBeatAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // ---- top control strip ----
    auto topRow = area.removeFromTop (70);

    // Genre
    auto genreArea = topRow.removeFromLeft (140);
    genreLabel.setBounds (genreArea.removeFromTop (20));
    genreBox  .setBounds (genreArea.removeFromTop (28));

    topRow.removeFromLeft (8);

    // Type
    auto typeArea = topRow.removeFromLeft (120);
    typeLabel.setBounds (typeArea.removeFromTop (20));
    typeBox  .setBounds (typeArea.removeFromTop (28));

    topRow.removeFromLeft (8);

    // Pattern #
    auto patArea = topRow.removeFromLeft (110);
    patLabel    .setBounds (patArea.removeFromTop (20));
    patIdxSlider.setBounds (patArea.removeFromTop (28));

    topRow.removeFromLeft (8);

    // Gate knob (square-ish)
    auto gateArea = topRow.removeFromLeft (70);
    gateLabel.setBounds (gateArea.removeFromTop (20));
    gateKnob .setBounds (gateArea);

    topRow.removeFromLeft (12);

    // Buttons stacked on the right
    auto btnArea = topRow;
    loadMidiBtn.setBounds (btnArea.removeFromTop (30));
    btnArea.removeFromTop (6);
    genVarBtn  .setBounds (btnArea.removeFromTop (30));

    // ---- pattern grid fills the rest ----
    area.removeFromTop (4);
    grid.setBounds (area);
}

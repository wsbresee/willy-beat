#include "PluginEditor.h"

//==============================================================================
// TagChipBar — multi-select fuzzy tag picker
//==============================================================================

TagChipBar::TagChipBar()
{
    addAndMakeVisible (input);
    // Make the inline input visually merge with the chip-bar's own outline.
    input.setColour (juce::TextEditor::backgroundColourId,     juce::Colours::transparentBlack);
    input.setColour (juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
    input.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    input.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    input.setTextToShowWhenEmpty ("type to search...", WillyBeatLookAndFeel::textMuted);
    input.addListener (this);
    input.setIndents (4, 2);
}

void TagChipBar::setAvailableTags (const juce::StringArray& all)
{
    availableTags = all;
}

void TagChipBar::setSelectedTags (const juce::StringArray& sel)
{
    selectedTags = sel;
    layoutChips();
    repaint();
}

void TagChipBar::resized()
{
    layoutChips();
}

void TagChipBar::layoutChips()
{
    chips.clear();
    auto area = getLocalBounds().reduced (2);
    const int h = area.getHeight();
    const int pad = 4;

    juce::Font f (juce::FontOptions{}.withHeight (11.0f));

    int x = area.getX() + pad;
    const int closeW = 14;

    for (const auto& tag : selectedTags)
    {
        int textW = (int) std::ceil (f.getStringWidthFloat (tag));
        int chipW = textW + 8 + closeW;

        // Reserve at least 60 px on the right for the search input
        if (x + chipW > area.getRight() - 64) break;

        Chip c;
        c.tag      = tag;
        c.bounds   = { x, area.getY() + 2, chipW, h - 4 };
        c.closeBox = { c.bounds.getRight() - closeW, c.bounds.getY(), closeW, c.bounds.getHeight() };
        chips.push_back (c);
        x += chipW + 4;
    }

    int inputW = area.getRight() - x;
    if (inputW < 50) inputW = 50;
    input.setBounds (x, area.getY() + 2, inputW, h - 4);
}

void TagChipBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (WillyBeatLookAndFeel::bgPanel);
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour (input.hasKeyboardFocus (true)
                    ? WillyBeatLookAndFeel::accent
                    : WillyBeatLookAndFeel::border);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));

    for (const auto& chip : chips)
    {
        auto cb = chip.bounds.toFloat();

        // Flat lavender chip
        g.setColour (WillyBeatLookAndFeel::accent);
        g.fillRoundedRectangle (cb, 3.0f);

        g.setColour (juce::Colours::white);
        auto textArea = chip.bounds.withTrimmedRight (chip.closeBox.getWidth());
        g.drawText (chip.tag, textArea.reduced (6, 0),
                    juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText ("x", chip.closeBox, juce::Justification::centred, false);
    }
}

void TagChipBar::mouseDown (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < chips.size(); ++i)
    {
        if (chips[i].closeBox.contains (e.getPosition()))
        {
            auto removed = chips[i].tag;
            selectedTags.removeString (removed);
            layoutChips();
            repaint();
            if (onTagsChanged) onTagsChanged();
            return;
        }
    }
    input.grabKeyboardFocus();
}

void TagChipBar::textEditorReturnKeyPressed (juce::TextEditor&)
{
    commitSearch();
}

void TagChipBar::textEditorEscapeKeyPressed (juce::TextEditor& te)
{
    te.clear();
}

void TagChipBar::commitSearch()
{
    auto query = input.getText().trim();
    if (query.isEmpty()) return;

    auto match = findFuzzyMatch (query);
    if (match.isEmpty())
    {
        // Fallback: accept the literal typed value (lets users introduce new tags)
        match = query;
    }

    if (! selectedTags.contains (match))
    {
        selectedTags.add (match);
        layoutChips();
        repaint();
        if (onTagsChanged) onTagsChanged();
    }
    input.clear();
}

juce::String TagChipBar::findFuzzyMatch (const juce::String& query) const
{
    auto qLow = query.toLowerCase();

    for (const auto& tag : availableTags)
        if (tag.equalsIgnoreCase (query) && ! selectedTags.contains (tag))
            return tag;

    for (const auto& tag : availableTags)
        if (tag.toLowerCase().startsWith (qLow) && ! selectedTags.contains (tag))
            return tag;

    for (const auto& tag : availableTags)
        if (tag.toLowerCase().contains (qLow) && ! selectedTags.contains (tag))
            return tag;

    return {};
}

//==============================================================================
// MIDI export helper
//==============================================================================

static juce::File writePatternToMidi (const DrumPattern& mainPat,
                                      const DrumPattern* fillPat,
                                      int          numBars,
                                      int          fillStart,
                                      int          fillMid,
                                      int          fillEnd,
                                      juce::int64  seed,
                                      float        humanize,
                                      float        swing,
                                      float        feel)
{
    const int    ticksPerStep = 120;
    const int    stepsPerBar  = MAX_STEPS;
    const double swingTicks   = swing   / 100.0 * ticksPerStep * 0.5;
    const double maxFeelTicks = feel    / 100.0 * ticksPerStep * 0.08;
    const double gateFrac     = 0.80;

    juce::int64 usedSeed = (seed < 0)
        ? juce::Random::getSystemRandom().nextInt64()
        : seed;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);

    const int totalSteps = numBars * stepsPerBar;

    // Cap fill regions so they don't overlap.  Start fill takes priority over
    // end fill when they would conflict.
    fillStart = juce::jlimit (0, totalSteps, fillStart);
    fillEnd   = juce::jlimit (0, totalSteps - fillStart, fillEnd);

    const int startFillEnd = fillStart;                // [0, startFillEnd)  = start fill
    const int endFillStart = totalSteps - fillEnd;     // [endFillStart, totalSteps)
    const int middleStart  = startFillEnd;
    const int middleEnd    = endFillStart;
    const int middleSize   = juce::jmax (0, middleEnd - middleStart);

    // Pick mid-fill positions deterministically from the seed.  These are
    // scattered within [middleStart, middleEnd) and replaced with their
    // matching position in the fill pattern.
    std::vector<bool> midFillMask (totalSteps, false);
    if (fillPat != nullptr && fillMid > 0 && middleSize > 0)
    {
        const int n = juce::jmin (fillMid, middleSize);
        juce::Random pickerRng (usedSeed ^ (juce::int64) 0xdeadbeefcafe1234LL);

        std::vector<int> positions;
        positions.reserve ((size_t) middleSize);
        for (int i = middleStart; i < middleEnd; ++i)
            positions.push_back (i);

        // Fisher-Yates shuffle (deterministic via pickerRng)
        for (int i = (int) positions.size() - 1; i > 0; --i)
        {
            int j = pickerRng.nextInt (i + 1);
            std::swap (positions[(size_t) i], positions[(size_t) j]);
        }
        for (int i = 0; i < n; ++i)
            midFillMask[(size_t) positions[(size_t) i]] = true;
    }

    for (int bar = 0; bar < numBars; ++bar)
    {
        juce::int64 barSeed = usedSeed ^ ((juce::int64)(bar + 1) * (juce::int64)0x9e3779b97f4a7c15LL);
        juce::Random barRng (barSeed);

        for (int col = 0; col < stepsPerBar; ++col)
        {
            const int  globalStep = bar * stepsPerBar + col;

            const bool inStartFill = (fillPat != nullptr && globalStep <  startFillEnd);
            const bool inEndFill   = (fillPat != nullptr && globalStep >= endFillStart);
            const bool inMidFill   = midFillMask[(size_t) globalStep];
            const bool useFill     = (inStartFill || inEndFill || inMidFill);

            const DrumPattern& pat = useFill ? *fillPat : mainPat;

            int patCol;
            if (inStartFill)
                patCol = globalStep;                                         // head of fill
            else if (inEndFill)
                patCol = (stepsPerBar - fillEnd) + (globalStep - endFillStart); // tail of fill
            else if (inMidFill)
                patCol = globalStep % stepsPerBar;                            // matching position
            else
                patCol = col;

            double stepTick = (double)(globalStep * ticksPerStep);
            if (col % 2 == 1) stepTick += swingTicks;

            for (int row = 0; row < NUM_TRACKS; ++row)
            {
                uint8_t vel = pat.velocities[row][patCol];
                if (vel == 0) continue;

                if (humanize > 0.0f)
                {
                    int h   = (int) humanize;
                    int dev = barRng.nextInt (2 * h + 1) - h;
                    vel = (uint8_t) juce::jlimit (1, 127, (int) vel + dev);
                }

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
    // Match the Generate button: accent-filled idle, brighter on hover.
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (hovered ? WillyBeatLookAndFeel::accentBright
                         : WillyBeatLookAndFeel::accent);
    g.fillRoundedRectangle (b, 4.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions{}
                              .withHeight (13.0f)
                              .withStyle ("Bold")));
    g.drawText ("Drag to DAW",
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
// MiniPatternView
//==============================================================================

MiniPatternView::MiniPatternView (WillyBeatAudioProcessor& p) : proc (p)
{
    startTimerHz (15);
}

MiniPatternView::~MiniPatternView()
{
    stopTimer();
}

void MiniPatternView::timerCallback()
{
    repaint();
}

void MiniPatternView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (WillyBeatLookAndFeel::bgRecess);
    g.fillRoundedRectangle (bounds, 3.0f);

    const auto* pat = (editTarget != nullptr) ? editTarget : proc.getActivePattern();
    if (pat == nullptr)
    {
        g.setColour (WillyBeatLookAndFeel::border);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
        return;
    }

    auto inner = bounds.reduced (3.0f);
    const float cellW = inner.getWidth()  / (float) MAX_STEPS;
    const float cellH = inner.getHeight() / (float) NUM_TRACKS;

    // Play cursor
    const int playStep = proc.getCurrentStep().load();
    if (playStep >= 0 && playStep < MAX_STEPS)
    {
        float cx = inner.getX() + (float) playStep * cellW;
        g.setColour (WillyBeatLookAndFeel::accent.withAlpha (0.18f));
        g.fillRect (cx, inner.getY(), cellW, inner.getHeight());
    }

    // Cells — only draw active hits, padding kept implicit by inner reduction.
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        for (int s = 0; s < MAX_STEPS; ++s)
        {
            auto vel = pat->velocities[t][s];
            if (vel == 0) continue;
            float x = inner.getX() + (float) s * cellW;
            float y = inner.getY() + (float) t * cellH;
            g.setColour (PatternGrid::velColour (vel));
            g.fillRect (x + 0.5f, y + 0.5f, cellW - 1.0f, cellH - 1.0f);
        }
    }

    g.setColour (WillyBeatLookAndFeel::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
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
    if (vel == 0)   return juce::Colour (0xff181a30);   // empty
    if (vel <= 30)  return juce::Colour (0xff2d3672);   // ghost
    if (vel <= 65)  return juce::Colour (0xff4859a6);   // soft
    if (vel <= 90)  return juce::Colour (0xff6b85d6);   // medium
    if (vel <= 110) return juce::Colour (0xff8aaeff);   // hard
    return              juce::Colour (0xffc08aff);      // accent
}

void PatternGrid::paint (juce::Graphics& g)
{
    const auto* displayPat = (editTarget != nullptr) ? editTarget
                                                     : proc.getActivePattern();

    const auto bounds = getLocalBounds();
    g.fillAll (WillyBeatLookAndFeel::bgRecess);

    const int   gridX = kLabelW;
    const int   gridW = bounds.getWidth() - gridX;
    const float cellW = (float) gridW / MAX_STEPS;
    const float cellH = (float) bounds.getHeight() / NUM_TRACKS;

    // Play cursor — always show regardless of edit state
    const int playStep = proc.getCurrentStep().load();
    if (playStep >= 0 && playStep < MAX_STEPS)
    {
        float cx = (float) gridX + (float) playStep * cellW;
        g.setColour (WillyBeatLookAndFeel::accent.withAlpha (0.16f));
        g.fillRect (cx, 0.0f, cellW, (float) bounds.getHeight());
    }

    // Beat-marker vertical lines every 4 16ths
    g.setColour (WillyBeatLookAndFeel::border);
    for (int col = 0; col <= MAX_STEPS; col += 4)
        g.drawVerticalLine ((int) ((float) gridX + (float) col * cellW),
                            0.0f, (float) bounds.getHeight());

    for (int row = 0; row < NUM_TRACKS; ++row)
    {
        float cy = (float) row * cellH;
        for (int col = 0; col < MAX_STEPS; ++col)
        {
            float cx = (float) gridX + (float) col * cellW;
            uint8_t vel = (displayPat != nullptr) ? displayPat->velocities[row][col] : 0;

            juce::Colour fill = velColour (vel);
            if (col == playStep)
                fill = fill.brighter (0.3f);

            g.setColour (fill);
            g.fillRect (cx + 1.0f, cy + 1.0f, cellW - 2.0f, cellH - 2.0f);
        }
    }

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    for (int row = 0; row < NUM_TRACKS; ++row)
    {
        float cy = (float) row * cellH;
        g.setColour (WillyBeatLookAndFeel::textSecondary);
        g.drawText (kTrackNames[row],
                    0, (int) cy, kLabelW - 6, (int) cellH,
                    juce::Justification::centredRight, true);
    }

    g.setColour (WillyBeatLookAndFeel::border);
    for (int row = 0; row <= NUM_TRACKS; ++row)
        g.drawHorizontalLine ((int) ((float) row * cellH),
                              (float) gridX, (float) bounds.getWidth());
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

    int col = (int) ((float) (e.x - kLabelW) / cellW);
    int row = (int) ((float) e.y / cellH);
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
        onCellChanged (row, col);
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
    : AudioProcessorEditor (&p), audioProcessor (p), grid (p), miniGrid (p)
{
    setLookAndFeel (&lookAndFeel);

    // ── Genre tag chip-bar ────────────────────────────────────────────────
    refreshTagSelector();
    tagBar.setTooltip ("Filter patterns by genre tag. Type a partial name and press Enter to add the best match  -  Rock, Backbeat, Trap, etc. Click x on a chip to remove. Patterns matching ANY selected tag are in scope; density augmentation also draws from this pool.");
    tagBar.onTagsChanged = [this]
    {
        auto tags = tagBar.getSelectedTags();
        lastKnownTags = tags;
        audioProcessor.setSelectedGenreTags (tags);
    };

    // ── Pattern index slider ──────────────────────────────────────────────
    patIdxAttach = std::make_unique<SA> (p.apvts, "patIdx", patIdxSlider);
    patIdxSlider.setSliderStyle (juce::Slider::IncDecButtons);
    patIdxSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);
    patIdxSlider.setTooltip ("Step through the patterns matching your selected tags.");

    // ── Knob row ──────────────────────────────────────────────────────────
    gateAttach     = std::make_unique<SA> (p.apvts, "gate",      gateKnob);
    humanizeAttach = std::make_unique<SA> (p.apvts, "humanize",  humanizeKnob);
    swingAttach    = std::make_unique<SA> (p.apvts, "swing",     swingKnob);
    feelAttach     = std::make_unique<SA> (p.apvts, "feel",      feelKnob);
    densityAttach  = std::make_unique<SA> (p.apvts, "density",   densityKnob);

    // JUCE LookAndFeel_V4 default rotary sweep — matches the fill rotaries
    // exactly so the knob direction is consistent across the whole UI.
    constexpr float kRotaryStart =  -juce::MathConstants<float>::pi * 5.0f / 6.0f;
    constexpr float kRotaryEnd   =   juce::MathConstants<float>::pi * 5.0f / 6.0f;

    auto setupRotary = [] (juce::Slider& k, int textBoxW, int textBoxH)
    {
        k.setSliderStyle (juce::Slider::Rotary);
        k.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
        k.setRotaryParameters (kRotaryStart, kRotaryEnd, true);
    };

    for (auto* k : { &gateKnob, &humanizeKnob, &swingKnob, &feelKnob, &densityKnob })
        setupRotary (*k, 48, 18);

    gateKnob    .setTooltip ("Gate  -  note length as a percentage of the step duration. Lower = staccato, higher = legato.");
    humanizeKnob.setTooltip ("Humanize  -  random velocity variation per hit, in MIDI velocity units.");
    swingKnob   .setTooltip ("Swing  -  delays the off-beat 16th notes for a shuffle/jazz feel. ~67% lands on triplets.");
    feelKnob    .setTooltip ("Feel  -  per-note timing jitter, bell-curved. Louder hits stay closer to the grid.");
    densityKnob .setTooltip ("Density  -  0-100% filters quiet hits by velocity threshold; 100-200% adds extra hits at empty slots, drawn from same-tag patterns (most-shared slots first).");

    auto labelStyle = [] (juce::Label* lbl)
    {
        lbl->setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
        lbl->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        lbl->setJustificationType (juce::Justification::centred);
    };
    for (auto* lbl : { &genreLabel, &patLabel,
                       &gateLabel, &humanizeLabel, &swingLabel, &feelLabel, &densityLabel })
        labelStyle (lbl);

    // ── Generate button (the primary action) ──────────────────────────────
    genBtn.onClick = [this] { audioProcessor.generateComposite(); refreshTagSelector(); };
    genBtn.setColour (juce::TextButton::buttonColourId,  WillyBeatLookAndFeel::accent);
    genBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    genBtn.setTooltip ("Generate a brand-new pattern from the selected genre tags. Each click produces a different mix.");

    // ── Collapse / expand toggle ──────────────────────────────────────────
    collapseBtn.onClick = [this] { toggleCompactMode(); };
    collapseBtn.setColour (juce::TextButton::buttonColourId,  WillyBeatLookAndFeel::bgPanel);
    collapseBtn.setColour (juce::TextButton::textColourOffId, WillyBeatLookAndFeel::textPrimary);
    collapseBtn.setTooltip ("Collapse / expand the editor.");

    // ── Name bar ──────────────────────────────────────────────────────────
    nameLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    nameLabel.setJustificationType (juce::Justification::centredRight);

    nameEditor.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f)));
    nameEditor.setTooltip ("Rename the current pattern. Press Enter or click away to save.");

    // Keep editingCopy.name in sync while typing; commit on Return or focus-loss
    nameEditor.onTextChange = [this]
    {
        editingCopy.name = nameEditor.getText().trim();
    };
    nameEditor.onReturnKey = [this] { saveNameChange(); };
    nameEditor.onFocusLost = [this] { saveNameChange(); };

    newPatBtn.onClick = [this]
    {
        editingCopy = DrumPattern{};
        editingCopy.name = "New Pattern";
        auto selTags = audioProcessor.getSelectedGenreTags();
        if (selTags.isEmpty()) editingCopy.genres.add ("Custom");
        else                   editingCopy.genres = selTags;
        editingCopy.type = PatType::Regular;
        fullPattern = editingCopy;

        nameEditor.setText (editingCopy.name, false);
        patternTagBar.setSelectedTags (editingCopy.genres);
        audioProcessor.saveEditedPattern (fullPattern);
        refreshTagSelector();
    };
    openFolderBtn.onClick = [this] { audioProcessor.getPresetsDirectory().startAsProcess(); };

    newPatBtn    .setTooltip ("Create a new blank pattern with the currently selected tags.");
    openFolderBtn.setTooltip ("Open the WillyBeat presets folder in Finder.");

    importMidiBtn.setTooltip ("Import a MIDI file as a new pattern. Channel-10 GM drum notes are mapped to tracks; the imported pattern uses the same drag-to-DAW pipeline (humanize, swing, feel, fills) as built-in patterns.");
    importMidiBtn.onClick = [this]
    {
        midiChooser = std::make_unique<juce::FileChooser> (
            "Import MIDI pattern", juce::File{}, "*.mid;*.midi");

        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles;

        midiChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                audioProcessor.loadMidiFile (file);
        });
    };

    editPatternBtn.setTooltip ("Open the full edit view (channel labels + per-cell editing).");
    editPatternBtn.onClick = [this] { if (compactMode) toggleCompactMode(); };

    // ── Per-pattern tag editor ───────────────────────────────────────────
    patternTagsLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    patternTagsLabel.setJustificationType (juce::Justification::centredRight);
    patternTagBar.setAvailableTags (audioProcessor.getLibrary().getGenres());
    patternTagBar.setTooltip ("Edit the tags attached to THIS pattern. Changes save immediately and update the library.");
    patternTagBar.onTagsChanged = [this]
    {
        auto newTags = patternTagBar.getSelectedTags();
        editingCopy.genres = newTags;
        fullPattern.genres = newTags;
        audioProcessor.saveEditedPattern (fullPattern);
        // Refresh both chip bars in case a brand-new tag was just introduced.
        patternTagBar.setAvailableTags (audioProcessor.getLibrary().getGenres());
        refreshTagSelector();
    };

    // ── Export / drag controls ────────────────────────────────────────────
    exportBarsBox.addItem ("1 bar",   1);
    exportBarsBox.addItem ("2 bars",  2);
    exportBarsBox.addItem ("4 bars",  3);
    exportBarsBox.addItem ("8 bars",  4);
    exportBarsBox.addItem ("16 bars", 5);
    exportBarsBox.setSelectedId (3);

    auto exportLabelStyle = [] (juce::Label* lbl)
    {
        lbl->setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
        lbl->setJustificationType (juce::Justification::centredRight);
    };
    exportLabelStyle (&barsLabel);
    exportLabelStyle (&seedLabel);

    seedEditor.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    seedEditor.setTextToShowWhenEmpty ("random", WillyBeatLookAndFeel::textMuted);
    seedEditor.setInputRestrictions (18, "0123456789");
    seedEditor.setTooltip ("Optional fixed random seed for reproducible exports. Leave blank to get fresh randomness on every drag.");

    exportBarsBox.setTooltip ("Number of bars to render when dragging the pattern to the DAW.");

    // ── Fill rotaries (Start / Mid / End) ─────────────────────────────────
    fillStartAttach = std::make_unique<SA> (p.apvts, "fillStart", fillStartKnob);
    fillMidAttach   = std::make_unique<SA> (p.apvts, "fillMid",   fillMidKnob);
    fillEndAttach   = std::make_unique<SA> (p.apvts, "fillSteps", fillEndKnob);

    // Same helper as the macro row — direction & style identical.
    setupRotary (fillStartKnob, 32, 14);
    setupRotary (fillMidKnob,   32, 14);
    setupRotary (fillEndKnob,   32, 14);

    fillStartKnob.setTooltip ("Fill at start  -  number of leading 16th notes drawn from a fill pattern (head of the fill). 0 = none, 16 = full bar.");
    fillMidKnob  .setTooltip ("Mid-pattern fill  -  number of 16th notes scattered through the middle of the export, drawn from fill content. Deterministic per seed.");
    fillEndKnob  .setTooltip ("Fill at end  -  number of trailing 16th notes drawn from a fill pattern (tail of the fill). Lands on the next downbeat.");

    auto fillLabelStyle = [] (juce::Label& l)
    {
        l.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
        l.setJustificationType (juce::Justification::centred);
    };
    fillLabelStyle (fillStartLabel);
    fillLabelStyle (fillMidLabel);
    fillLabelStyle (fillEndLabel);

    // ── Drag strip — exports current editingCopy ──────────────────────────
    dragStrip.onDrag = [this]() -> juce::File
    {
        if (editingCopy.name.isEmpty()) return {};

        int         numBars   = getBarsFromCombo();
        int         fillStart = (int) audioProcessor.apvts.getRawParameterValue ("fillStart")->load();
        int         fillMid   = (int) audioProcessor.apvts.getRawParameterValue ("fillMid")  ->load();
        int         fillEnd   = (int) audioProcessor.apvts.getRawParameterValue ("fillSteps")->load();
        juce::int64 seed      = getSeedFromEditor();

        // Build a density-aware fill (loaded from disk, with same density as main)
        DrumPattern fillPattern;
        const DrumPattern* fillPtr = nullptr;
        if (fillStart > 0 || fillMid > 0 || fillEnd > 0)
        {
            fillPattern = buildFillPatternForExport (seed);
            if (fillPattern.name.isNotEmpty())
                fillPtr = &fillPattern;
        }

        float humanize = audioProcessor.apvts.getRawParameterValue ("humanize")->load();
        float swing    = audioProcessor.apvts.getRawParameterValue ("swing")->load();
        float feel     = audioProcessor.apvts.getRawParameterValue ("feel")->load();

        return writePatternToMidi (editingCopy, fillPtr, numBars,
                                   fillStart, fillMid, fillEnd,
                                   seed, humanize, swing, feel);
    };
    dragStrip.setTooltip ("Drag this strip into your DAW's MIDI track to drop the current pattern. Honours bar count, fills, density, and the random seed.");

    // ── Grid — always in edit mode ────────────────────────────────────────
    grid.onCellChanged = [this] (int t, int s) { autoSaveCurrentEdit (t, s); };

    // ── Initialise full + filtered patterns from the active pattern ───────
    lastDensity = audioProcessor.apvts.getRawParameterValue ("density")->load() / 100.0f;
    if (auto* pat = audioProcessor.getActivePattern())
    {
        fullPattern      = pat->sourceFile.existsAsFile()
                              ? PatternLibrary::loadFromFile (pat->sourceFile)
                              : *pat;
        editingCopy      = fullPattern;
        lastKnownPattern = pat;
        applyDensityToEditingCopy();
        audioProcessor.getLibrary().updatePattern (editingCopy);
        nameEditor.setText (editingCopy.name, false);
        patternTagBar.setSelectedTags (editingCopy.genres);
    }
    grid.setEditTarget (&editingCopy);
    grid.setTooltip ("Click a cell to cycle its velocity (off  ->  medium  ->  hard  ->  accent  ->  ghost  ->  soft). Right-click clears it. Edits auto-save and the playback follows immediately.");

    // ── Add everything ────────────────────────────────────────────────────
    addAndMakeVisible (dragStrip);
    addAndMakeVisible (grid);
    addAndMakeVisible (miniGrid);
    addAndMakeVisible (editPatternBtn);
    addAndMakeVisible (importMidiBtn);
    miniGrid       .setVisible (false);
    editPatternBtn .setVisible (false);
    miniGrid.setEditTarget (&editingCopy);
    miniGrid.setTooltip ("Mini view of the active pattern. Click 'Edit Pattern' to open the full grid.");
    addAndMakeVisible (genreLabel);     addAndMakeVisible (tagBar);
    addAndMakeVisible (patLabel);       addAndMakeVisible (patIdxSlider);
    addAndMakeVisible (gateLabel);      addAndMakeVisible (gateKnob);
    addAndMakeVisible (humanizeLabel);  addAndMakeVisible (humanizeKnob);
    addAndMakeVisible (swingLabel);     addAndMakeVisible (swingKnob);
    addAndMakeVisible (feelLabel);      addAndMakeVisible (feelKnob);
    addAndMakeVisible (densityLabel);   addAndMakeVisible (densityKnob);
    addAndMakeVisible (genBtn);
    addAndMakeVisible (collapseBtn);
    addAndMakeVisible (nameLabel);      addAndMakeVisible (nameEditor);
    addAndMakeVisible (newPatBtn);      addAndMakeVisible (openFolderBtn);
    addAndMakeVisible (patternTagsLabel); addAndMakeVisible (patternTagBar);
    addAndMakeVisible (barsLabel);       addAndMakeVisible (exportBarsBox);
    addAndMakeVisible (fillStartLabel);  addAndMakeVisible (fillStartKnob);
    addAndMakeVisible (fillMidLabel);    addAndMakeVisible (fillMidKnob);
    addAndMakeVisible (fillEndLabel);    addAndMakeVisible (fillEndKnob);
    addAndMakeVisible (seedLabel);       addAndMakeVisible (seedEditor);

    // Poll for active-pattern changes at 10 Hz
    startTimerHz (10);

    setSize (760, 736);
}

WillyBeatAudioProcessorEditor::~WillyBeatAudioProcessorEditor()
{
    stopTimer();
    grid.setEditTarget (nullptr);
    grid.onCellChanged = nullptr;
    setLookAndFeel (nullptr);
}

//==============================================================================
// Timer — detect when the active pattern changes and reload editingCopy
//==============================================================================

void WillyBeatAudioProcessorEditor::timerCallback()
{
    auto* pat = audioProcessor.getActivePattern();

    // Pattern switched — reload pristine fullPattern from disk (the library
    // entry may have been mutated by density augmentation).
    if (pat != lastKnownPattern)
    {
        lastKnownPattern = pat;
        if (pat != nullptr)
        {
            fullPattern = pat->sourceFile.existsAsFile()
                              ? PatternLibrary::loadFromFile (pat->sourceFile)
                              : *pat;
            editingCopy = fullPattern;
            applyDensityToEditingCopy();
            audioProcessor.getLibrary().updatePattern (editingCopy);
            if (! nameEditor.hasKeyboardFocus (true))
                nameEditor.setText (editingCopy.name, false);
            patternTagBar.setSelectedTags (editingCopy.genres);
            grid.repaint();
        }
    }

    // Density knob moved — re-derive editingCopy and push to library for playback
    float density = audioProcessor.apvts.getRawParameterValue ("density")->load() / 100.0f;
    if (! juce::approximatelyEqual (density, lastDensity))
    {
        lastDensity = density;
        applyDensityToEditingCopy();
        audioProcessor.getLibrary().updatePattern (editingCopy);
        grid.repaint();
    }

    // Genre tags changed externally (DAW state restore, navigateToPattern, etc.)
    auto curTags = audioProcessor.getSelectedGenreTags();
    if (curTags != lastKnownTags)
    {
        lastKnownTags = curTags;
        tagBar.setSelectedTags (curTags);
    }
}

//==============================================================================

void WillyBeatAudioProcessorEditor::autoSaveCurrentEdit (int track, int step)
{
    // A direct cell edit becomes the canonical value in fullPattern.  Only
    // this cell is synced — other density-hidden cells in fullPattern stay
    // intact so they can reappear when density is turned back up.
    if (track >= 0 && track < NUM_TRACKS && step >= 0 && step < MAX_STEPS)
        fullPattern.velocities[track][step] = editingCopy.velocities[track][step];

    editingCopy.name = nameEditor.getText().trim();
    if (editingCopy.name.isEmpty()) editingCopy.name = "Custom Pattern";
    fullPattern.name       = editingCopy.name;
    fullPattern.genres     = editingCopy.genres;
    fullPattern.type       = editingCopy.type;

    // Persist the unfiltered fullPattern so density changes stay reversible.
    audioProcessor.autoSavePattern (fullPattern);
    editingCopy.sourceFile = fullPattern.sourceFile;

    // Re-derive editingCopy in case the edit fell into the augmented region,
    // then push the live (filtered/augmented) version into the library so
    // processBlock plays exactly what's on screen.
    applyDensityToEditingCopy();
    editingCopy.velocities[track][step] = fullPattern.velocities[track][step];
    audioProcessor.getLibrary().updatePattern (editingCopy);
}

// Shared density logic — works for both the main pattern and fill patterns.
// density is 0.0 to 2.0 (≤1 filters by velocity threshold, >1 augments from
// scope-tagged source patterns at empty slots).  scopeTags is the user's
// selected genre tags — if non-empty, augmentation only pulls from patterns
// matching at least one of those tags.
static void applyDensity (DrumPattern& target,
                          const DrumPattern& src,
                          float density,
                          const PatternLibrary& library,
                          const juce::StringArray& scopeTags,
                          bool restrictToSameType)
{
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (int s = 0; s < MAX_STEPS; ++s)
            target.velocities[t][s] = src.velocities[t][s];

    if (density <= 1.0f)
    {
        uint8_t minVel = (uint8_t) ((1.0f - density) * 127.0f);
        for (int t = 0; t < NUM_TRACKS; ++t)
            for (int s = 0; s < MAX_STEPS; ++s)
            {
                uint8_t v = target.velocities[t][s];
                if (v > 0 && v < minVel)
                    target.velocities[t][s] = 0;
            }
    }
    else
    {
        const float excess = density - 1.0f;

        // Source pool: patterns matching any scope tag (or all patterns if
        // no scope is set), excluding `src` itself.
        auto matches = scopeTags.isEmpty()
                          ? std::vector<const DrumPattern*>{}
                          : library.getByTags (scopeTags);

        std::vector<const DrumPattern*> sources;
        if (scopeTags.isEmpty())
        {
            for (const auto& p : library.all())
                if (p.sourceFile != src.sourceFile)
                    if (! restrictToSameType || p.type == src.type)
                        sources.push_back (&p);
        }
        else
        {
            for (auto* cp : matches)
                if (cp->sourceFile != src.sourceFile)
                    if (! restrictToSameType || cp->type == src.type)
                        sources.push_back (cp);
        }

        if (sources.empty())
            for (const auto& p : library.all())
                if (p.sourceFile != src.sourceFile)
                    if (! restrictToSameType || p.type == src.type)
                        sources.push_back (&p);

        if (sources.empty())
        {
            target.computeDensity();
            return;
        }

        struct Slot { int track, step, popularity; uint8_t vel; };
        std::vector<Slot> slots;
        slots.reserve (NUM_TRACKS * MAX_STEPS);

        for (int t = 0; t < NUM_TRACKS; ++t)
            for (int s = 0; s < MAX_STEPS; ++s)
            {
                if (src.velocities[t][s] != 0) continue;

                int pop = 0, velSum = 0;
                for (auto* sp : sources)
                {
                    uint8_t v = sp->velocities[t][s];
                    if (v > 0) { ++pop; velSum += v; }
                }
                if (pop > 0)
                {
                    uint8_t avgVel = (uint8_t) (velSum / pop);
                    slots.push_back ({ t, s, pop, avgVel });
                }
            }

        std::sort (slots.begin(), slots.end(),
                   [] (const Slot& a, const Slot& b)
                   {
                       if (a.popularity != b.popularity) return a.popularity > b.popularity;
                       if (a.track      != b.track)      return a.track      < b.track;
                       return a.step < b.step;
                   });

        int numToAdd = (int) std::round (excess * (float) slots.size());
        for (int i = 0; i < numToAdd && i < (int) slots.size(); ++i)
            target.velocities[slots[i].track][slots[i].step] = slots[i].vel;
    }

    target.computeDensity();
}

void WillyBeatAudioProcessorEditor::applyDensityToEditingCopy()
{
    applyDensity (editingCopy, fullPattern, lastDensity,
                  audioProcessor.getLibrary(),
                  audioProcessor.getSelectedGenreTags(),
                  /*restrictToSameType=*/false);
}

DrumPattern WillyBeatAudioProcessorEditor::buildFillPatternForExport (juce::int64 seed) const
{
    auto* fillSrc = findFill (fullPattern, seed);
    if (fillSrc == nullptr) return DrumPattern{};

    // Always read the canonical (unfiltered) version from disk so density
    // augmentation in the live library doesn't pollute the fill.
    DrumPattern fillFull = fillSrc->sourceFile.existsAsFile()
                              ? PatternLibrary::loadFromFile (fillSrc->sourceFile)
                              : *fillSrc;

    DrumPattern result = fillFull;
    // For fills, restrict augmentation sources to other fills so the result
    // stays "fill-like" rather than gaining backbeat hits from regular patterns.
    applyDensity (result, fillFull, lastDensity,
                  audioProcessor.getLibrary(),
                  audioProcessor.getSelectedGenreTags(),
                  /*restrictToSameType=*/true);
    return result;
}

void WillyBeatAudioProcessorEditor::saveNameChange()
{
    auto newName = nameEditor.getText().trim();
    if (newName.isEmpty()) newName = "Custom Pattern";
    if (newName == editingCopy.name && editingCopy.sourceFile.existsAsFile()) return;

    editingCopy.name = newName;
    fullPattern.name = newName;
    audioProcessor.saveEditedPattern (fullPattern);
    refreshTagSelector();
}

void WillyBeatAudioProcessorEditor::refreshTagSelector()
{
    auto allTags = audioProcessor.getLibrary().getGenres();
    tagBar       .setAvailableTags (allTags);
    patternTagBar.setAvailableTags (allTags);

    auto sel = audioProcessor.getSelectedGenreTags();
    lastKnownTags = sel;
    tagBar.setSelectedTags (sel);
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

const DrumPattern* WillyBeatAudioProcessorEditor::findFill (const DrumPattern& pat,
                                                             juce::int64 seed) const
{
    // Prefer fills matching the user's selected scope; fall back to the
    // pattern's own tags, then any fill in the library.
    auto scope = audioProcessor.getSelectedGenreTags();
    if (scope.isEmpty()) scope = pat.genres;

    auto isFill = [] (PatType t) { return t == PatType::SmallFill || t == PatType::BigFill; };

    std::vector<const DrumPattern*> fills;
    if (! scope.isEmpty())
        for (auto* p : audioProcessor.getLibrary().getByTags (scope))
            if (isFill (p->type)) fills.push_back (p);

    if (fills.empty())
        for (const auto& p : audioProcessor.getLibrary().all())
            if (isFill (p.type)) fills.push_back (&p);

    if (fills.empty()) return nullptr;

    // Vary the fill the same way the main pattern does: a fixed export seed
    // makes the choice reproducible; an empty seed rerolls each drag.
    juce::Random rng (seed >= 0 ? seed
                                : juce::Random::getSystemRandom().nextInt64());
    return fills[(size_t) rng.nextInt ((int) fills.size())];
}

//==============================================================================

void WillyBeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (WillyBeatLookAndFeel::bgWindow);

    // Title bar
    auto titleArea = getLocalBounds().removeFromTop (30);
    g.setColour (WillyBeatLookAndFeel::accent);
    g.setFont (juce::Font (juce::FontOptions{}
                              .withHeight (16.0f)
                              .withStyle ("Bold")));
    g.drawFittedText ("WillyBeat", titleArea, juce::Justification::centred, 1);

    // Hairline below the title
    g.setColour (WillyBeatLookAndFeel::border);
    g.drawHorizontalLine (30, 16.0f, (float) getWidth() - 16.0f);
}

void WillyBeatAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // ── Title row + collapse toggle in the top-right corner ───────────────
    {
        auto titleRow = area.removeFromTop (34);
        auto box = titleRow.removeFromRight (28);
        collapseBtn.setBounds (box.withSizeKeepingCentre (24, 22));
    }

    // ── Row A: genre tags + pat selector + Generate + Drag-to-DAW ────────
    auto rowA = area.removeFromTop (42);

    // Right-aligned primary actions: Generate, then Drag (so Drag sits
    // immediately to Generate's right).
    {
        auto dragCol = rowA.removeFromRight (140);
        dragStrip.setBounds (dragCol.withHeight (34).withY (rowA.getY() + 4));
        rowA.removeFromRight (6);

        auto genCol = rowA.removeFromRight (130);
        genBtn.setBounds (genCol.withHeight (34).withY (rowA.getY() + 4));
        rowA.removeFromRight (10);
    }

    // Tag chip-bar takes the left side.
    auto tagsArea = rowA.removeFromLeft (260);
    genreLabel.setBounds (tagsArea.removeFromTop (18));
    tagBar    .setBounds (tagsArea.removeFromTop (24));
    rowA.removeFromLeft (8);

    auto patArea = rowA.removeFromLeft (86);
    patLabel    .setBounds (patArea.removeFromTop (18));
    patIdxSlider.setBounds (patArea.removeFromTop (24));

    // In compact mode, show a read-only thumbnail in the lower-left and
    // surface Edit-Pattern + Import-MIDI buttons.
    if (compactMode)
    {
        area.removeFromTop (8);
        auto miniRow = area.removeFromTop (84);

        auto buttonsCol = miniRow.removeFromRight (160);
        editPatternBtn.setBounds (buttonsCol.removeFromTop (32).reduced (4, 4));
        importMidiBtn .setBounds (buttonsCol.removeFromTop (32).reduced (4, 4));

        miniGrid.setBounds (miniRow.reduced (0, 4));
        return;
    }

    area.removeFromTop (4);

    // ── Row B: macro knobs ────────────────────────────────────────────────
    auto rowB = area.removeFromTop (110);
    {
        int sectionW = rowB.getWidth() / 5;
        juce::Label*  labels[] = { &gateLabel, &humanizeLabel, &swingLabel, &feelLabel, &densityLabel };
        juce::Slider* knobs[]  = { &gateKnob,  &humanizeKnob,  &swingKnob,  &feelKnob,  &densityKnob };
        for (int i = 0; i < 5; ++i)
        {
            auto section = rowB.removeFromLeft (i < 4 ? sectionW : rowB.getWidth());
            labels[i]->setBounds (section.removeFromTop (18));
            knobs[i]->setBounds (section);
        }
    }

    area.removeFromTop (4);

    // ── Name bar ──────────────────────────────────────────────────────────
    {
        auto nameRow = area.removeFromTop (28);
        nameLabel .setBounds (nameRow.removeFromLeft (44));
        nameRow.removeFromLeft (4);
        nameEditor.setBounds (nameRow.removeFromLeft (200).reduced (0, 2));
        nameRow.removeFromLeft (8);
        newPatBtn    .setBounds (nameRow.removeFromLeft (90) .withHeight (24).withY (nameRow.getY() + 2));
        nameRow.removeFromLeft (4);
        openFolderBtn.setBounds (nameRow.removeFromLeft (100).withHeight (24).withY (nameRow.getY() + 2));
        nameRow.removeFromLeft (4);
        importMidiBtn.setBounds (nameRow.removeFromLeft (100).withHeight (24).withY (nameRow.getY() + 2));
    }

    area.removeFromTop (4);

    // ── Per-pattern tag editor row ────────────────────────────────────────
    {
        auto tagRow = area.removeFromTop (26);
        patternTagsLabel.setBounds (tagRow.removeFromLeft (44));
        tagRow.removeFromLeft (4);
        patternTagBar   .setBounds (tagRow.reduced (0, 1));
    }

    area.removeFromTop (6);

    // ── Export row (now ABOVE the grid, so users see export config first) ─
    {
        auto exportRow = area.removeFromTop (70);

        auto centred = [&] (juce::Rectangle<int> r) {
            int yOff = (exportRow.getHeight() - 24) / 2;
            return r.withHeight (24).withY (exportRow.getY() + yOff);
        };
        auto centredLabel = [&] (juce::Rectangle<int> r) {
            int yOff = (exportRow.getHeight() - 20) / 2;
            return r.withHeight (20).withY (exportRow.getY() + yOff + 1);
        };

        barsLabel    .setBounds (centredLabel (exportRow.removeFromLeft (34)));
        exportBarsBox.setBounds (centred      (exportRow.removeFromLeft (72)));
        exportRow.removeFromLeft (12);

        // Three fill rotaries: Start / Mid / End — 70-px columns, label above.
        auto layoutFillKnob = [&] (juce::Label& lbl, juce::Slider& knob)
        {
            auto col = exportRow.removeFromLeft (70);
            lbl .setBounds (col.removeFromTop (14));
            knob.setBounds (col);
            exportRow.removeFromLeft (4);
        };
        layoutFillKnob (fillStartLabel, fillStartKnob);
        layoutFillKnob (fillMidLabel,   fillMidKnob);
        layoutFillKnob (fillEndLabel,   fillEndKnob);
        exportRow.removeFromLeft (10);

        seedLabel  .setBounds (centredLabel (exportRow.removeFromLeft (36)));
        seedEditor .setBounds (centred      (exportRow.removeFromLeft (110)));
    }

    area.removeFromTop (4);

    // ── Pattern grid fills the remaining middle ───────────────────────────
    grid.setBounds (area);
}

void WillyBeatAudioProcessorEditor::toggleCompactMode()
{
    compactMode = ! compactMode;
    collapseBtn.setButtonText (compactMode ? "+" : "-");

    const bool show = ! compactMode;
    juce::Component* hideInCompact[] = {
        &grid,
        &gateLabel,      &gateKnob,
        &humanizeLabel,  &humanizeKnob,
        &swingLabel,     &swingKnob,
        &feelLabel,      &feelKnob,
        &densityLabel,   &densityKnob,
        &nameLabel,      &nameEditor,
        &newPatBtn,      &openFolderBtn,  &importMidiBtn,
        &patternTagsLabel, &patternTagBar,
        &barsLabel,      &exportBarsBox,
        &fillStartLabel, &fillStartKnob,
        &fillMidLabel,   &fillMidKnob,
        &fillEndLabel,   &fillEndKnob,
        &seedLabel,      &seedEditor
    };
    for (auto* c : hideInCompact)
        c->setVisible (show);

    miniGrid       .setVisible (! show);
    editPatternBtn .setVisible (! show);
    // Import MIDI is useful in both modes; show it in compact too.
    importMidiBtn  .setVisible (true);

    setSize (760, compactMode ? 184 : 736);
}

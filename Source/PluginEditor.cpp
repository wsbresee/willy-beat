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
    input.addKeyListener (this);  // intercept backspace before the editor consumes it
    input.setIndents (4, 2);
}

bool TagChipBar::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // Backspace with no pending input pops the rightmost chip - same
    // pattern Gmail / Slack / chips-UIs use.
    if (key.getKeyCode() == juce::KeyPress::backspaceKey
        && input.getText().isEmpty()
        && ! selectedTags.isEmpty())
    {
        selectedTags.remove (selectedTags.size() - 1);
        layoutChips();
        repaint();
        if (onTagsChanged) onTagsChanged();
        return true;
    }
    return false;
}

void TagChipBar::setAvailableTags (const juce::StringArray& all)
{
    availableTags = all;
    vectorIndex.setKnownTags (all);
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

    // First chance: the editor gets to handle the raw query (e.g., jump
    // straight to a pattern when the input matches a pattern name exactly).
    // If the hook claims the query, clear the input and skip everything else.
    if (query.isNotEmpty() && onRawInputHandled && onRawInputHandled (query))
    {
        input.clear();
        return;
    }

    // Add a tag only when something was actually typed AND it resolves to a
    // tag that isn't already selected. Everything else (empty input, exact
    // duplicate) still falls through to onTagSubmitted so Enter ALWAYS
    // signals "regenerate" - that's the user's mental model for the key.
    if (query.isNotEmpty())
    {
        auto match = findFuzzyMatch (query);
        if (match.isEmpty()) match = query;  // literal fallback for new tags

        if (! selectedTags.contains (match))
        {
            selectedTags.add (match);
            layoutChips();
            repaint();
            if (onTagsChanged) onTagsChanged();
        }
        input.clear();
    }

    if (onTagSubmitted) onTagSubmitted();
}

juce::String TagChipBar::findFuzzyMatch (const juce::String& query) const
{
    // 1. Exact / prefix / substring match wins outright - cheaper than the
    //    embedding lookup and matches obvious typing intent.
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

    // 2. Semantic vector search via Apple's NaturalLanguage embedding.
    //    Lets "metal" match "Heavy Metal", "edm" match "Electronic",
    //    "sad slow" find "Lofi" or "Ambient", and so on. Falls back to
    //    nothing (empty) if no tag clears the similarity threshold.
    if (vectorIndex.isAvailable())
    {
        auto match = vectorIndex.findBestMatch (query, selectedTags);
        if (match.isNotEmpty()) return match;
    }

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

void MiniPatternView::mouseDown (const juce::MouseEvent&)
{
    if (onClick) onClick();
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

    // Velocity badge: animated fade in/out (~120 ms each way), driven by
    // badgeAlpha that the timer eases toward badgeTarget. Empty cells
    // never reach a non-zero target so they stay invisible.
    if (badgeRow >= 0 && badgeCol >= 0 && badgeVel > 0 && badgeAlpha > 0.001f)
    {
        const float alpha = badgeAlpha;
        // Draw the velocity number directly on the cell itself. A faint
        // dark scrim keeps the digit readable on lighter cell colors.
        juce::Rectangle<float> cell ((float) gridX + (float) badgeCol * cellW,
                                      (float) badgeRow * cellH,
                                      cellW, cellH);

        g.setColour (juce::Colours::black.withAlpha (alpha * 0.40f));
        g.fillRect (cell);

        const float fontH = juce::jlimit (10.0f, 14.0f, cellH - 4.0f);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.setFont (juce::Font (juce::FontOptions{}
                                   .withHeight (fontH)
                                   .withStyle ("Bold")));
        g.drawText (juce::String (badgeVel), cell,
                    juce::Justification::centred, false);
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
    if (e.x < kLabelW) return;

    const auto bounds = getLocalBounds();
    const float cellW = (float) (bounds.getWidth() - kLabelW) / MAX_STEPS;
    const float cellH = (float) bounds.getHeight() / NUM_TRACKS;

    int col = (int) ((float) (e.x - kLabelW) / cellW);
    int row = (int) ((float) e.y / cellH);
    if (col < 0 || col >= MAX_STEPS || row < 0 || row >= NUM_TRACKS) return;

    auto& vel = editTarget->velocities[row][col];

    if (e.mods.isRightButtonDown())
    {
        // Right-click clears the cell immediately and skips any further
        // handling - signal that by leaving dragRow at -1.
        vel = 0;
        dragRow = dragCol = -1;
        dragMoved = false;
        dragStartVel = 0;

        badgeRow    = row;
        badgeCol    = col;
        badgeVel    = 0;
        badgeTarget = 0.0f;
        pendingBadge.valid = false;

        repaint();
        if (onCellChanged) onCellChanged (row, col);
        return;
    }

    // Left-click toggles the cell: empty cells get a max-velocity accent,
    // filled cells are cleared. A subsequent drag then tunes the new
    // velocity by y-delta starting from whatever we just placed.
    vel = (vel == 0) ? (uint8_t) 120 : (uint8_t) 0;

    dragRow      = row;
    dragCol      = col;
    dragMoved    = false;
    dragStartVel = vel;

    badgeRow    = row;
    badgeCol    = col;
    badgeVel    = (int) vel;
    badgeTarget = (vel > 0) ? 1.0f : 0.0f;
    pendingBadge.valid = false;

    repaint();
    if (onCellChanged) onCellChanged (row, col);
}

void PatternGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (editTarget == nullptr || dragRow < 0) return;
    if (e.mods.isRightButtonDown())            return;

    // Vertical drag: up = louder, down = quieter. Two velocity units per
    // pixel so the full range covers ~64 px of vertical travel.
    const int dy        = e.getDistanceFromDragStartY();
    const int newVel    = juce::jlimit (0, 127, dragStartVel - dy * 2);
    auto&     vel       = editTarget->velocities[dragRow][dragCol];

    if ((int) vel == newVel) return;

    vel         = (uint8_t) newVel;
    dragMoved   = true;

    // Keep the velocity badge mirroring the drag so the number tracks the
    // colour change live, not just on release.
    badgeRow    = dragRow;
    badgeCol    = dragCol;
    badgeVel    = newVel;
    badgeTarget = (newVel > 0) ? 1.0f : 0.0f;

    repaint();
    if (onCellChanged) onCellChanged (dragRow, dragCol);
}

void PatternGrid::mouseUp (const juce::MouseEvent&)
{
    dragRow = dragCol = -1;
    dragMoved = false;
}

// Shared helper: figure out which cell (if any) is under (x, y), update the
// badge target there, and pick the appropriate fade target. Empty cells set
// the badge to fade out so we never sit on a "0" indicator.
void PatternGrid::updateBadgeAt (int x, int y)
{
    auto fadeOutOnly = [&]
    {
        pendingBadge.valid = false;
        if (badgeTarget != 0.0f) badgeTarget = 0.0f;
    };

    if (editTarget == nullptr || x < kLabelW)
    {
        fadeOutOnly();
        return;
    }

    const auto bounds = getLocalBounds();
    const float cellW = (float) (bounds.getWidth() - kLabelW) / MAX_STEPS;
    const float cellH = (float) bounds.getHeight() / NUM_TRACKS;

    int col = (int) ((float) (x - kLabelW) / cellW);
    int row = (int) ((float) y / cellH);
    if (col < 0 || col >= MAX_STEPS || row < 0 || row >= NUM_TRACKS)
    {
        fadeOutOnly();
        return;
    }

    const int newVel = (int) editTarget->velocities[row][col];

    // Same cell as before: update velocity in place (covers drag/scroll
    // while hovering) and let the badge target follow.
    if (row == badgeRow && col == badgeCol)
    {
        badgeVel    = newVel;
        badgeTarget = (newVel > 0) ? 1.0f : 0.0f;
        pendingBadge.valid = false;
        repaint();
        return;
    }

    scrollAccum = 0.0f;

    // Different cell: fade the current badge out, then swap the new cell
    // in once alpha hits 0 (handled by the timer). Empty cells skip the
    // swap step - we just fade out.
    if (newVel <= 0)
    {
        fadeOutOnly();
        return;
    }

    pendingBadge = { row, col, newVel, true };
    badgeTarget  = 0.0f;
}

void PatternGrid::mouseEnter (const juce::MouseEvent& e) { updateBadgeAt (e.x, e.y); }
void PatternGrid::mouseExit  (const juce::MouseEvent&)   { badgeTarget = 0.0f; }
void PatternGrid::mouseMove  (const juce::MouseEvent& e) { updateBadgeAt (e.x, e.y); }

void PatternGrid::mouseWheelMove (const juce::MouseEvent& e,
                                  const juce::MouseWheelDetails& w)
{
    if (editTarget == nullptr || e.x < kLabelW) return;

    const auto bounds = getLocalBounds();
    const float cellW = (float) (bounds.getWidth() - kLabelW) / MAX_STEPS;
    const float cellH = (float) bounds.getHeight() / NUM_TRACKS;

    int col = (int) ((float) (e.x - kLabelW) / cellW);
    int row = (int) ((float) e.y / cellH);
    if (col < 0 || col >= MAX_STEPS || row < 0 || row >= NUM_TRACKS) return;

    // Reset the partial-step accumulator whenever the cursor jumps to a
    // different cell so steps don't carry over across cells.
    if (row != badgeRow || col != badgeCol)
        scrollAccum = 0.0f;

    // ~64 velocity units per "click" of the wheel. Trackpad deltas are
    // continuous, so we keep the fractional remainder in scrollAccum.
    const float gain = w.isReversed ? -1.0f : 1.0f;
    scrollAccum += w.deltaY * gain * 64.0f;
    const int delta = (int) scrollAccum;
    scrollAccum -= (float) delta;
    if (delta == 0) return;

    auto& vel = editTarget->velocities[row][col];
    const int newVel = juce::jlimit (0, 127, (int) vel + delta);
    if (newVel == (int) vel) return;

    vel        = (uint8_t) newVel;
    badgeRow   = row;
    badgeCol   = col;
    badgeVel   = newVel;
    badgeTarget = (newVel > 0) ? 1.0f : 0.0f;

    repaint();
    if (onCellChanged) onCellChanged (row, col);
}

void PatternGrid::timerCallback()
{
    int step = proc.getCurrentStep().load();
    bool needRepaint = (step != lastStep);
    if (needRepaint) lastStep = step;

    // Ease badgeAlpha toward badgeTarget. ~80 ms each direction at 30 Hz
    // (kFadeStep == 0.4 ≈ 2.5 ticks to traverse 0↔1). Quick enough that
    // a cross-cell fade-out → fade-in transition feels brisk while still
    // being a real animation.
    constexpr float kFadeStep = 0.40f;
    if (badgeAlpha < badgeTarget)
    {
        badgeAlpha = juce::jmin (badgeTarget, badgeAlpha + kFadeStep);
        needRepaint = true;
    }
    else if (badgeAlpha > badgeTarget)
    {
        badgeAlpha = juce::jmax (badgeTarget, badgeAlpha - kFadeStep);
        needRepaint = true;
    }

    // Old badge has finished fading out and a new cell is waiting — swap
    // it in and start fading up.
    if (badgeAlpha <= 0.001f && pendingBadge.valid)
    {
        badgeRow    = pendingBadge.row;
        badgeCol    = pendingBadge.col;
        badgeVel    = pendingBadge.vel;
        badgeTarget = 1.0f;
        pendingBadge.valid = false;
        needRepaint = true;
    }

    if (needRepaint) repaint();
}

//==============================================================================
// WillyBeatAudioProcessorEditor
//==============================================================================

WillyBeatAudioProcessorEditor::WillyBeatAudioProcessorEditor (WillyBeatAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), grid (p), miniGrid (p)
{
    setLookAndFeel (&lookAndFeel);

    // ── Tag chip-bar (also serves as the active pattern's tag editor) ────
    refreshTagSelector();
    tagBar.setTooltip ("Tags for the active pattern. Type a partial name and press Enter to add the best match. Click x on a chip to remove. Tags drive Generate, density augmentation, and fill matching.");
    tagBar.onTagsChanged = [this]
    {
        auto tags = tagBar.getSelectedTags();
        lastKnownTags = tags;
        audioProcessor.setSelectedGenreTags (tags);

        // Persist the edit on the active pattern so the chip bar is also a
        // per-pattern tag editor. Skip when there's no pattern loaded yet.
        if (fullPattern.name.isNotEmpty())
        {
            editingCopy.genres = tags;
            fullPattern.genres = tags;
            audioProcessor.autoSavePattern (fullPattern);
            // Refresh available tags in case a brand-new one was introduced.
            tagBar.setAvailableTags (audioProcessor.getLibrary().getGenres());
        }
    };
    // Exact pattern-name match short-circuits the tag flow entirely and
    // navigates to that saved pattern. Lets users type e.g. "Funky Drummer"
    // and land on the canonical preset rather than generating something new.
    tagBar.onRawInputHandled = [this] (const juce::String& q) -> bool
    {
        return audioProcessor.navigateToPatternByName (q);
    };

    // Enter on the chip-bar input commits the tag (if any) and ALWAYS
    // regenerates. The source pool is built by vector-expanding the current
    // chip selection into the ~12 nearest neighbours, then randomly
    // picking a small handful to drive makeComposite. The new pattern is
    // still saved with the user's exact chip selection as its genres, so
    // the chip bar doesn't drift between presses.
    tagBar.onTagSubmitted = [this]
    {
        auto selected = tagBar.getSelectedTags();
        const auto& vi = tagBar.getVectorIndex();

        if (selected.isEmpty() || ! vi.isAvailable())
        {
            audioProcessor.generateComposite();
        }
        else
        {
            auto pool = vi.findNearestN (selected, 12);
            if (pool.isEmpty()) pool = selected; // graceful fallback

            // Random subset of size 3 (or pool size, whichever is smaller).
            // Partial Fisher-Yates so each Enter draws a fresh combination.
            juce::Random rng (juce::Random::getSystemRandom().nextInt64());
            const int wanted = juce::jmin (3, pool.size());
            juce::StringArray picks;
            for (int i = 0; i < wanted; ++i)
            {
                int j = i + rng.nextInt (pool.size() - i);
                auto tmp = pool[i];
                pool.set (i, pool[j]);
                pool.set (j, tmp);
                picks.add (pool[i]);
            }
            audioProcessor.generateComposite (picks, selected);
        }
        refreshTagSelector();
    };

    // ── Pattern index slider ──────────────────────────────────────────────
    patIdxAttach = std::make_unique<SA> (p.apvts, "patIdx", patIdxSlider);
    patIdxSlider.setSliderStyle (juce::Slider::IncDecButtons);
    patIdxSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);
    patIdxSlider.setTooltip ("Step through saved patterns. Generate appends a new pattern at the end - go back one slot to see your previous generation.");

    // ── Knob row ──────────────────────────────────────────────────────────
    gateAttach     = std::make_unique<SA> (p.apvts, "duration",      gateKnob);
    humanizeAttach = std::make_unique<SA> (p.apvts, "dynamics",  humanizeKnob);
    swingAttach    = std::make_unique<SA> (p.apvts, "swing",     swingKnob);
    feelAttach     = std::make_unique<SA> (p.apvts, "slop",      feelKnob);
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

    gateKnob    .setTooltip ("Duration  -  note length as a percentage of the step duration. Lower = staccato, higher = legato.");
    humanizeKnob.setTooltip ("Dynamics  -  random velocity variation per hit, in MIDI velocity units. Higher = more loudness contrast between hits.");
    swingKnob   .setTooltip ("Swing  -  delays the off-beat 16th notes for a shuffle/jazz feel. ~67% lands on triplets.");
    feelKnob    .setTooltip ("Slop  -  random per-note timing offset, bell-curved. Louder hits stay closer to the grid. Higher = looser feel.");
    densityKnob .setTooltip ("Density  -  0-100% removes hits one-by-one in importance order (low-velocity 16th-offbeat hits first; downbeats and loud backbeats last). 100-200% adds extra hits at empty slots, drawn from same-tag patterns (most-shared slots first).");

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

    // ── Internal preview audio toggle ────────────────────────────────────
    soundBtn.setClickingTogglesState (true);
    soundBtn.setColour (juce::TextButton::buttonColourId,    WillyBeatLookAndFeel::bgPanel);
    soundBtn.setColour (juce::TextButton::buttonOnColourId,  WillyBeatLookAndFeel::accent);
    soundBtn.setColour (juce::TextButton::textColourOffId,   WillyBeatLookAndFeel::textPrimary);
    soundBtn.setColour (juce::TextButton::textColourOnId,    juce::Colours::white);
    soundBtn.setTooltip ("Internal drum-sound preview. When on, WillyBeat plays simple synthesized drums through its audio output as the DAW transport rolls. Default off so the plugin defers to a downstream sampler.");
    soundAttach = std::make_unique<BA> (p.apvts, "internalSound", soundBtn);

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

    fillSectionLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f)));
    fillSectionLabel.setJustificationType (juce::Justification::centred);
    fillSectionLabel.setColour (juce::Label::textColourId, WillyBeatLookAndFeel::textPrimary);

    // ── Drag strip — exports current editingCopy ──────────────────────────
    dragStrip.onDrag = [this]() -> juce::File
    {
        if (editingCopy.name.isEmpty()) return {};

        int         numBars   = getBarsFromCombo();
        int         fillStart = (int) audioProcessor.apvts.getRawParameterValue ("fillStart")->load();
        int         fillMid   = (int) audioProcessor.apvts.getRawParameterValue ("fillMid")  ->load();
        int         fillEnd   = (int) audioProcessor.apvts.getRawParameterValue ("fillSteps")->load();
        // Fresh randomness on every drag - the seed editor is gone, so we
        // always reroll humanize/swing/feel/fill placement.
        juce::int64 seed      = juce::Random::getSystemRandom().nextInt64();

        // Build a density-aware fill (loaded from disk, with same density as main)
        DrumPattern fillPattern;
        const DrumPattern* fillPtr = nullptr;
        if (fillStart > 0 || fillMid > 0 || fillEnd > 0)
        {
            fillPattern = buildFillPatternForExport (seed);
            if (fillPattern.name.isNotEmpty())
                fillPtr = &fillPattern;
        }

        float humanize = audioProcessor.apvts.getRawParameterValue ("dynamics")->load();
        float swing    = audioProcessor.apvts.getRawParameterValue ("swing")->load();
        float feel     = audioProcessor.apvts.getRawParameterValue ("slop")->load();

        return writePatternToMidi (editingCopy, fillPtr, numBars,
                                   fillStart, fillMid, fillEnd,
                                   seed, humanize, swing, feel);
    };
    dragStrip.setTooltip ("Drag this strip into your DAW's MIDI track to drop the current pattern. Honours bar count, fills, density, and re-rolls humanize/swing/feel placement on every drag.");

    // ── Grid — always in edit mode ────────────────────────────────────────
    grid.onCellChanged = [this] (int t, int s) { autoSaveCurrentEdit (t, s); };

    // ── Initialise full + filtered patterns from the active pattern ───────
    // Tags are intentionally NOT pulled from the initial pattern here: a
    // fresh plugin instance should open with an empty chip bar so the user
    // chooses their own scope. Subsequent patIdx changes (or Generate) DO
    // populate the chip bar from the active pattern via the timer.
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
    }
    // Clear any persisted scope so the chip bar starts truly empty on open.
    audioProcessor.setSelectedGenreTags ({});
    lastKnownTags.clear();
    tagBar.setSelectedTags ({});
    grid.setEditTarget (&editingCopy);
    grid.setTooltip ("Click an empty cell to place a max-velocity (120) hit; click a filled cell to clear it. Click and drag vertically to tune the velocity (up = louder, down = quieter). Scroll over a cell for finer steps. Right-click also clears. Edits auto-save and playback follows immediately.");

    // ── Add everything ────────────────────────────────────────────────────
    addAndMakeVisible (dragStrip);
    addAndMakeVisible (grid);
    addAndMakeVisible (miniGrid);
    miniGrid.setVisible (false);
    miniGrid.setEditTarget (&editingCopy);
    miniGrid.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    miniGrid.setTooltip ("Mini view of the active pattern. Click to open the full grid.");
    miniGrid.onClick = [this] { if (compactMode) toggleCompactMode(); };
    addAndMakeVisible (genreLabel);     addAndMakeVisible (tagBar);
    addAndMakeVisible (patLabel);       addAndMakeVisible (patIdxSlider);
    addAndMakeVisible (gateLabel);      addAndMakeVisible (gateKnob);
    addAndMakeVisible (humanizeLabel);  addAndMakeVisible (humanizeKnob);
    addAndMakeVisible (swingLabel);     addAndMakeVisible (swingKnob);
    addAndMakeVisible (feelLabel);      addAndMakeVisible (feelKnob);
    addAndMakeVisible (densityLabel);   addAndMakeVisible (densityKnob);
    addAndMakeVisible (genBtn);
    addAndMakeVisible (collapseBtn);
    addAndMakeVisible (soundBtn);
    addAndMakeVisible (barsLabel);       addAndMakeVisible (exportBarsBox);
    addAndMakeVisible (fillSectionLabel);
    addAndMakeVisible (fillStartLabel);  addAndMakeVisible (fillStartKnob);
    addAndMakeVisible (fillMidLabel);    addAndMakeVisible (fillMidKnob);
    addAndMakeVisible (fillEndLabel);    addAndMakeVisible (fillEndKnob);

    // Poll for active-pattern changes at 10 Hz
    startTimerHz (10);

    setSize (760, 678);
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
            // Pattern changed (patIdx moved, Generate fired, etc.) — pull the
            // chip bar over to the new pattern's tags and broadcast them as
            // the active scope.
            tagBar.setSelectedTags (editingCopy.genres);
            lastKnownTags = editingCopy.genres;
            audioProcessor.setSelectedGenreTags (editingCopy.genres);
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

    if (density < 1.0f)
    {
        // Importance-based hit removal: score every active hit, sort the
        // least-important to the front, then silence the first N where N
        // = (1 - density) * totalHits. Score combines velocity with a
        // beat-position tier so ghost notes and 16th offbeats fall away
        // first while downbeat / backbeat hits survive.
        struct Hit { int track, step; int importance; };
        std::vector<Hit> hits;
        hits.reserve (NUM_TRACKS * MAX_STEPS);

        auto tierBonus = [] (int step) -> int
        {
            if (step % 16 == 0) return 80; // bar downbeat
            if (step %  4 == 0) return 40; // quarter beats (incl. backbeats)
            if (step %  2 == 0) return 10; // 8th-note positions
            return 0;                      // 16th-note offbeats
        };

        for (int t = 0; t < NUM_TRACKS; ++t)
            for (int s = 0; s < MAX_STEPS; ++s)
                if (auto v = target.velocities[t][s])
                    hits.push_back ({ t, s, (int) v * 2 + tierBonus (s) });

        std::sort (hits.begin(), hits.end(),
                   [] (const Hit& a, const Hit& b)
                   {
                       if (a.importance != b.importance) return a.importance < b.importance;
                       if (a.track      != b.track)      return a.track      < b.track;
                       return a.step < b.step;
                   });

        const int numHits     = (int) hits.size();
        const int numToRemove = numHits - (int) std::round (density * (float) numHits);
        for (int i = 0; i < numToRemove && i < numHits; ++i)
            target.velocities[hits[i].track][hits[i].step] = 0;
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

void WillyBeatAudioProcessorEditor::refreshTagSelector()
{
    auto allTags = audioProcessor.getLibrary().getGenres();
    tagBar.setAvailableTags (allTags);

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

    // Drop-zone overlay while a MIDI file is dragged over the editor.
    if (midiDragHovered)
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (WillyBeatLookAndFeel::accent.withAlpha (0.12f));
        g.fillRect (b);
        g.setColour (WillyBeatLookAndFeel::accent);
        g.drawRect (b, 2.5f);
        g.setFont (juce::Font (juce::FontOptions{}
                                  .withHeight (18.0f)
                                  .withStyle ("Bold")));
        g.drawFittedText ("Drop MIDI to import (first 4 bars)",
                          getLocalBounds(),
                          juce::Justification::centred, 1);
    }
}

// Cubase exports dragged MIDI parts as temp files with varying extensions
// (sometimes .mid, sometimes nothing, sometimes a UUID name). We accept
// any file the host hands us, then sniff its first four bytes for the
// Standard MIDI File magic ("MThd") before treating it as MIDI.

static bool looksLikeStandardMidi (const juce::File& f)
{
    if (! f.existsAsFile()) return false;
    juce::FileInputStream in (f);
    if (! in.openedOk()) return false;
    char hdr[4] = {};
    if (in.read (hdr, 4) != 4) return false;
    return hdr[0] == 'M' && hdr[1] == 'T' && hdr[2] == 'h' && hdr[3] == 'd';
}

static void logDragEvent (const juce::String& label, const juce::StringArray& files)
{
    auto logFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("WillyBeat/drag.log");
    logFile.getParentDirectory().createDirectory();
    juce::String line;
    line << juce::Time::getCurrentTime().toString (true, true, true, true)
         << "  " << label << "  files=" << files.size();
    for (auto& f : files) line << "\n    " << f;
    line << "\n";
    logFile.appendText (line);
}

bool WillyBeatAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    // Be permissive at this stage — we'll validate the SMF header at drop
    // time. Returning true here also enables the visual hover feedback.
    return ! files.isEmpty();
}

void WillyBeatAudioProcessorEditor::fileDragEnter (const juce::StringArray& files, int, int)
{
    logDragEvent ("enter", files);
    if (isInterestedInFileDrag (files))
    {
        midiDragHovered = true;
        repaint();
    }
}

void WillyBeatAudioProcessorEditor::fileDragExit (const juce::StringArray& files)
{
    logDragEvent ("exit", files);
    midiDragHovered = false;
    repaint();
}

void WillyBeatAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    logDragEvent ("drop", files);
    midiDragHovered = false;
    repaint();

    // Import the first file that has either a MIDI extension or an SMF
    // magic header. The processor's loadMidiFile handles parsing,
    // quantizing, 4-bar cap, save, and navigation.
    for (auto& path : files)
    {
        juce::File f { path };
        const bool extLooksRight = path.endsWithIgnoreCase (".mid")
                                || path.endsWithIgnoreCase (".midi");
        if (! extLooksRight && ! looksLikeStandardMidi (f)) continue;
        if (! f.existsAsFile()) continue;

        audioProcessor.loadMidiFile (f);
        break;
    }
}

void WillyBeatAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // ── Title row + collapse toggle + audio toggle (top-right) ───────────
    {
        auto titleRow    = area.removeFromTop (34);
        auto collapseBox = titleRow.removeFromRight (28);
        collapseBtn.setBounds (collapseBox.withSizeKeepingCentre (24, 22));
        titleRow.removeFromRight (4);
        auto soundBox    = titleRow.removeFromRight (72);
        soundBtn.setBounds (soundBox.withSizeKeepingCentre (68, 22));
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

    // In compact mode: clickable thumbnail on the left, 5 mini macro
    // rotaries in the middle, three fill rotaries on the right.
    if (compactMode)
    {
        area.removeFromTop (8);
        auto miniRow = area.removeFromTop (84);

        auto fillCol = miniRow.removeFromRight (165);
        {
            juce::Label*  fLabels[] = { &fillStartLabel, &fillMidLabel, &fillEndLabel };
            juce::Slider* fKnobs[]  = { &fillStartKnob,  &fillMidKnob,  &fillEndKnob  };
            int colW = fillCol.getWidth() / 3;
            for (int i = 0; i < 3; ++i)
            {
                auto col = fillCol.removeFromLeft (i < 2 ? colW : fillCol.getWidth());
                fLabels[i]->setBounds (col.removeFromTop (14));
                fKnobs[i] ->setBounds (col);
            }
        }

        miniRow.removeFromRight (6);

        auto miniGridArea = miniRow.removeFromLeft (260);
        miniGrid.setBounds (miniGridArea.reduced (0, 4));

        miniRow.removeFromLeft (4);

        // 5 mini macro rotaries spread across whatever's left. Each rotary
        // keeps its built-in wheel-scroll handling so they can be edited
        // without expanding the editor.
        // Order: Duration, Dynamics, Slop, Swing, Density - clusters the
        // three variation knobs (Dynamics/Slop/Swing) together.
        juce::Label*  labels[] = { &gateLabel, &humanizeLabel, &feelLabel,
                                   &swingLabel, &densityLabel };
        juce::Slider* knobs[]  = { &gateKnob,  &humanizeKnob,  &feelKnob,
                                   &swingKnob, &densityKnob };
        int colW = miniRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
        {
            auto col = miniRow.removeFromLeft (i < 4 ? colW : miniRow.getWidth());
            labels[i]->setBounds (col.removeFromTop (14));
            knobs[i] ->setBounds (col);
        }
        return;
    }

    area.removeFromTop (4);

    // ── Row B: macro knobs ────────────────────────────────────────────────
    auto rowB = area.removeFromTop (110);
    {
        int sectionW = rowB.getWidth() / 5;
        juce::Label*  labels[] = { &gateLabel, &humanizeLabel, &feelLabel, &swingLabel, &densityLabel };
        juce::Slider* knobs[]  = { &gateKnob,  &humanizeKnob,  &feelKnob,  &swingKnob,  &densityKnob };
        for (int i = 0; i < 5; ++i)
        {
            auto section = rowB.removeFromLeft (i < 4 ? sectionW : rowB.getWidth());
            labels[i]->setBounds (section.removeFromTop (18));
            knobs[i]->setBounds (section);
        }
    }

    area.removeFromTop (6);

    // ── Export row (now ABOVE the grid, so users see export config first) ─
    {
        auto exportRow = area.removeFromTop (88);

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

        // Three fill rotaries grouped under a single "Fill" section header.
        // Sub-labels per knob just read "Start" / "Mid" / "End".
        auto fillArea = exportRow.removeFromLeft (3 * 70 + 2 * 4);
        fillSectionLabel.setBounds (fillArea.removeFromTop (18));

        auto layoutFillKnob = [&] (juce::Label& lbl, juce::Slider& knob)
        {
            auto col = fillArea.removeFromLeft (70);
            lbl .setBounds (col.removeFromTop (14));
            knob.setBounds (col);
            fillArea.removeFromLeft (4);
        };
        layoutFillKnob (fillStartLabel, fillStartKnob);
        layoutFillKnob (fillMidLabel,   fillMidKnob);
        layoutFillKnob (fillEndLabel,   fillEndKnob);
    }

    area.removeFromTop (4);

    // ── Pattern grid fills the remaining middle ───────────────────────────
    grid.setBounds (area);
}

void WillyBeatAudioProcessorEditor::toggleCompactMode()
{
    compactMode = ! compactMode;
    collapseBtn.setButtonText (compactMode ? "+" : "-");

    // Macro rotaries, fill rotaries, and their labels stay visible in
    // compact mode so the user can still wheel-edit them; the big grid,
    // the bars selector, and the "Fill" section header hide.
    const bool show = ! compactMode;
    juce::Component* hideInCompact[] = {
        &grid,
        &barsLabel, &exportBarsBox,
        &fillSectionLabel
    };
    for (auto* c : hideInCompact)
        c->setVisible (show);

    miniGrid.setVisible (! show);

    // In compact mode each rotary needs its own "Fill"-prefixed label
    // because there's no section header. In full mode the header carries
    // that word and the sub-labels read just Start / Mid / End.
    fillStartLabel.setText (compactMode ? "Fill Start" : "Start", juce::dontSendNotification);
    fillMidLabel  .setText (compactMode ? "Fill Mid"   : "Mid",   juce::dontSendNotification);
    fillEndLabel  .setText (compactMode ? "Fill End"   : "End",   juce::dontSendNotification);

    setSize (760, compactMode ? 184 : 678);
}

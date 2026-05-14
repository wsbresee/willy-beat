#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DrumwrightLookAndFeel.h"
#include "TagVectorIndex.h"


// Width of the fixed track-label column shown to the left of the grid.
inline constexpr int kPatternLabelColW = 68;

// Minimum horizontal pixels per grid cell. When the pattern would need
// more width than the viewport allows, the grid scrolls.
inline constexpr int kPatternMinCellW = 18;

// ─── TrackLabels ─────────────────────────────────────────────────────────
// Fixed (non-scrolling) column rendering the kTrackNames[] vertically.
class TrackLabels : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
};

// ─── GridViewport ────────────────────────────────────────────────────────
// Same as juce::Viewport but ignores vertical-only wheel events so the
// grid's own mouseWheelMove (velocity adjustment) doesn't fight the
// viewport's scroll behaviour. Horizontal wheel deltas (trackpad
// two-finger swipe) still scroll the viewport.
class GridViewport : public juce::Viewport
{
public:
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;
};

// Slider that accumulates scroll-wheel input and fires at most one integer
// step per gesture, so coarse-range knobs (fill 0-16) don't jump wildly.
class SlowScrollSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& w) override
    {
        scrollAccum += w.deltaY;
        constexpr float kThreshold = 0.15f;
        if (std::abs (scrollAccum) >= kThreshold)
        {
            // Pass a synthetic single-step event so JUCE handles direction.
            auto single = w;
            single.deltaY = scrollAccum > 0.0f ? kThreshold : -kThreshold;
            single.deltaX = 0.0f;
            scrollAccum = 0.0f;
            juce::Slider::mouseWheelMove (e, single);
        }
    }
private:
    float scrollAccum = 0.0f;
};

// Label that supports double-click-to-edit without changing the cursor on hover,
// so the appearance is identical to a plain label when not being edited.
class KnobLabel : public juce::Label
{
public:
    using juce::Label::Label;
    void mouseEnter (const juce::MouseEvent&) override {}
    void mouseExit  (const juce::MouseEvent&) override {}
};

// Minimal click-aware Label used for the "Swing 16 / Swing 8" toggle.
class ClickableLabel : public juce::Label
{
public:
    using juce::Label::Label;
    std::function<void()> onClick;

    void mouseDown  (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f, 1.5f);
        g.setColour (hovered ? DrumwrightLookAndFeel::accent.withAlpha (0.18f)
                             : DrumwrightLookAndFeel::bgPanel);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (hovered ? DrumwrightLookAndFeel::accentBright
                             : DrumwrightLookAndFeel::accent.withAlpha (0.75f));
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
        g.setColour (findColour (juce::Label::textColourId));
        g.setFont (getFont());
        g.drawText (getText(), getLocalBounds(), getJustificationType(), true);
    }

private:
    bool hovered = false;
};

// Left/right chevron arrow button for pattern navigation.
// Draws a rounded-rect box with a chevron matching the combo-box dropdown arrow style.
class PatNavButton : public juce::Button
{
public:
    explicit PatNavButton (bool isLeft_) : juce::Button ({}), isLeft (isLeft_) {}

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();

        juce::Path arrow;
        if (isLeft)
        {
            arrow.startNewSubPath (cx + 2.0f, cy - 4.0f);
            arrow.lineTo          (cx - 2.5f, cy);
            arrow.lineTo          (cx + 2.0f, cy + 4.0f);
        }
        else
        {
            arrow.startNewSubPath (cx - 2.0f, cy - 4.0f);
            arrow.lineTo          (cx + 2.5f, cy);
            arrow.lineTo          (cx - 2.0f, cy + 4.0f);
        }

        g.setColour (isButtonDown      ? DrumwrightLookAndFeel::accentBright
                   : isMouseOverButton ? DrumwrightLookAndFeel::accent
                                       : DrumwrightLookAndFeel::accent.withAlpha (0.55f));
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

private:
    bool isLeft;
};

// ─── DragStrip ───────────────────────────────────────────────────────────────
class DragStrip : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    std::function<juce::File()> onDrag;

    void paint      (juce::Graphics& g)         override;
    void mouseDown  (const juce::MouseEvent&)   override;
    void mouseDrag  (const juce::MouseEvent&)   override;
    void mouseEnter (const juce::MouseEvent&)   override;
    void mouseExit  (const juce::MouseEvent&)   override;

private:
    bool hovered     = false;
    bool dragStarted = false;
};

// ─── PatternGrid ─────────────────────────────────────────────────────────────
class PatternGrid : public juce::Component,
                    public juce::SettableTooltipClient,
                    public juce::Timer
{
public:
    explicit PatternGrid (DrumwrightAudioProcessor& p);
    ~PatternGrid() override;

    void paint           (juce::Graphics& g)         override;
    void timerCallback   ()                           override;
    void mouseDown       (const juce::MouseEvent& e)  override;
    void mouseDrag       (const juce::MouseEvent& e)  override;
    void mouseUp         (const juce::MouseEvent& e)  override;
    void mouseMove       (const juce::MouseEvent& e)  override;
    void mouseEnter      (const juce::MouseEvent& e)  override;
    void mouseExit       (const juce::MouseEvent& e)  override;
    void mouseWheelMove  (const juce::MouseEvent& e,
                          const juce::MouseWheelDetails& w)  override;

    void setEditTarget (DrumPattern* target);

    // Width in pixels the grid needs to honour `kPatternMinCellW` for the
    // current pattern. If smaller than viewportW it returns viewportW so
    // the grid fills available space; otherwise scrolling kicks in.
    int  getNaturalWidth (int viewportW) const;

    // Fired after a cell edit; passes the track and tick that changed.
    std::function<void(int track, int tick)> onHitChanged;

    // Fired before the first edit within a single mouse-down gesture,
    // giving the owner a chance to snapshot state for undo.
    std::function<void()> onBeforeEdit;

private:
    struct Layout;
    Layout computeLayout (const DrumPattern& pat) const;
    bool   cellAt (int x, int y, int& outRow, int& outCell, const Layout& L) const;
    void   updateBadgeAt (int x, int y);

    DrumwrightAudioProcessor& proc;
    DrumPattern*             editTarget = nullptr;
    int                      lastStep   = -1;

    // Drag-to-set-velocity state
    int  dragRow       = -1;
    int  dragCol       = -1;
    int  dragStartVel  = 0;
    bool dragMoved     = false;
    bool deferredClear = false; // true when mouseDown found a filled cell; clear deferred to mouseUp

    // Scroll-velocity badge state: which cell was most recently scrolled,
    // its current velocity, and when the last scroll happened (ms since
    // boot). The timer animates fade-out.
    int       badgeRow      = -1;
    int       badgeCol      = -1;
    int       badgeVel      = 0;
    float     scrollAccum   = 0.0f; // partial-step accumulator for fine trackpad input
    float     badgeAlpha    = 0.0f; // current opacity, animated by the timer
    float     badgeTarget   = 0.0f; // target opacity (0 or 1) set by mouse events

    // When the cursor moves to a different cell, we queue the new cell
    // here and drive the current badge's alpha to 0. Once it reaches 0
    // the timer swaps in the queued cell and fades it back up - a real
    // crossfade rather than a teleport.
    struct PendingBadge { int row = -1; int col = -1; int vel = 0; bool valid = false; };
    PendingBadge pendingBadge;

public:
    static juce::Colour velColour (uint8_t vel);
};

// ─── MiniPatternView ─────────────────────────────────────────────────────────
// Compact, read-only thumbnail of the active pattern. No labels, no clicks.
class MiniPatternView : public juce::Component,
                        public juce::SettableTooltipClient,
                        public juce::Timer
{
public:
    explicit MiniPatternView (DrumwrightAudioProcessor& p);
    ~MiniPatternView() override;

    void paint        (juce::Graphics& g)         override;
    void timerCallback()                           override;
    void mouseDown    (const juce::MouseEvent& e) override;

    void setEditTarget (DrumPattern* target) { editTarget = target; repaint(); }

    // Fired when the user clicks the thumbnail. Editor wires this to
    // expand back to the full grid.
    std::function<void()> onClick;

private:
    DrumwrightAudioProcessor& proc;
    DrumPattern*             editTarget = nullptr;
};

// ─── DrumwrightAudioProcessorEditor ───────────────────────────────────────────
class DrumwrightAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer,
                                      public juce::FileDragAndDropTarget
{
public:
    explicit DrumwrightAudioProcessorEditor (DrumwrightAudioProcessor&);
    ~DrumwrightAudioProcessorEditor() override;

    void paint               (juce::Graphics&)          override;
    void resized             ()                          override;
    void timerCallback       ()                          override;
    bool keyPressed          (const juce::KeyPress& key) override;
    void mouseDoubleClick    (const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter          (const juce::StringArray& files, int x, int y) override;
    void fileDragExit           (const juce::StringArray& files) override;

private:
    DrumwrightAudioProcessor& audioProcessor;

    DrumwrightLookAndFeel lookAndFeel;
    juce::TooltipWindow  tooltipWindow { this, 2800 };

    DragStrip       dragStrip;
    PatternGrid     grid;
    TrackLabels     trackLabels;
    GridViewport    gridViewport;
    MiniPatternView miniGrid;

    // ── Pattern selector row ─────────────────────────────────────────────
    juce::Label    genreLabel   { {}, "TAGS" };
    juce::Label    patLabel     { {}, "PATTERN" };

    juce::TextEditor tagInput;
    TagVectorIndex   tagVectorIndex;
    juce::Slider     patIdxSlider;   // hidden; APVTS attachment point only
    PatNavButton     patPrevBtn { true  };
    PatNavButton     patNextBtn { false };
    juce::Label      patNameLabel;

    juce::TextButton clearBtn    { "Clear" };
    juce::TextButton genBtn      { "Generate" };
    juce::TextButton collapseBtn { "-" };
    juce::TextButton soundBtn    { "Audio" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> patIdxAttach;
    std::unique_ptr<BA> soundAttach;

    // ── Knob row ─────────────────────────────────────────────────────────
    KnobLabel  gateLabel      { {}, "DURATION" };
    KnobLabel  humanizeLabel  { {}, "DYNAMICS" };
    KnobLabel  swingLabel     { {}, "SWING" };
    KnobLabel  feelLabel      { {}, "SLOP" };
    KnobLabel  densityLabel   { {}, "DENSITY" };

    juce::Slider gateKnob;
    juce::Slider humanizeKnob;
    juce::Slider swingKnob;
    juce::Slider feelKnob;
    juce::Slider densityKnob;

    std::unique_ptr<SA>  gateAttach;
    std::unique_ptr<SA>  humanizeAttach;
    std::unique_ptr<SA>  swingAttach;
    std::unique_ptr<SA>  feelAttach;
    std::unique_ptr<SA>  densityAttach;

    // ── Pattern shape + export controls ─────────────────────────────────
    juce::Label      timeSigLabel { {}, "TIME" };
    juce::ComboBox   timeSigBox;

    juce::Label      barsLabel    { {}, "BARS" };
    juce::ComboBox   barsBox;

    juce::Label      gridLabel    { {}, "GRID" };
    juce::ComboBox   gridBox;

    juce::Label      humanizeSectionLabel { {}, "HUMANIZE" };
    juce::Label      fillSectionLabel     { {}, "FILL" };
    KnobLabel        fillStartLabel  { {}, "START" };
    SlowScrollSlider fillStartKnob;
    std::unique_ptr<SA> fillStartAttach;

    KnobLabel        fillMidLabel    { {}, "MID" };
    SlowScrollSlider fillMidKnob;
    std::unique_ptr<SA> fillMidAttach;

    KnobLabel        fillEndLabel    { {}, "END" };
    SlowScrollSlider fillEndKnob;
    std::unique_ptr<SA> fillEndAttach;


    // ── Live edit state ──────────────────────────────────────────────────
    DrumPattern             editingCopy;       // density-filtered view shown in grid
    DrumPattern             fullPattern;       // unfiltered source of truth (in memory)
    const DrumPattern*      lastKnownPattern = nullptr;
    float                   lastDensity      = -1.0f;

    void autoSaveCurrentEditAtTick (int track, int tick);
    void applyDensityToEditingCopy();
    void refreshTagSelector();
    void syncShapeCombos();
    void updateGridLayout();
    void applyTimeSig (int newNum, int newDen);
    void toggleCompactMode();
    juce::String knobLabelRestingText (juce::Label*) const;

    // Flash-on-edit: maps each knob label → ms timestamp at which to revert
    juce::HashMap<juce::Label*, juce::int64> labelFlashEnd;

    bool compactMode = false;
    bool midiDragHovered = false;
    int  rowSepY = 0;   // y-position of the hairline between rowA and knobs

    juce::StringArray lastKnownTags;

    // Last host-reported time signature we've applied to the pattern.
    // Used to mirror DAW time-sig changes into the active pattern.
    int lastHostTsNum = 0;
    int lastHostTsDen = 0;

    // Undo history for grid edits (Cmd+Z). Stores fullPattern snapshots taken
    // before each mouse-down gesture, oldest first.
    std::deque<DrumPattern> undoHistory;
    static constexpr int    kMaxUndoDepth = 32;

    void undoLastGridEdit();
    void renameCurrentPattern (const juce::String& newName);

    // Set true when genBtn fires, cleared when the new pattern is detected.
    bool isGenerating = false;

    // Set by genBtn.onClick to apply the current UI time-sig/bars/grid to
    // the newly-generated pattern instead of letting syncShapeCombos reset them.
    bool    pendingGenerate   = false;
    int     pendingTsNum      = 4;
    int     pendingTsDen      = 4;
    int     pendingBars       = 1;
    GridSub pendingGridSub    = GridSub::Sixteenth;

    int         getBarsFromCombo() const;
    void        capturePendingShape();
    const DrumPattern* findFill (const DrumPattern& pat, juce::int64 seed) const;
    DrumPattern buildFillPatternForExport (juce::int64 seed) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumwrightAudioProcessorEditor)
};

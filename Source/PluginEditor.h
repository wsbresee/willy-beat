#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WillyBeatLookAndFeel.h"
#include "TagVectorIndex.h"

// ─── TagChipBar ──────────────────────────────────────────────────────────────
// Multi-select genre tags with fuzzy search.  Shows currently-selected tags
// as clickable "chips" followed by a search input; pressing Enter on the
// input fuzzy-matches against the available tags and adds the best match.
class TagChipBar : public juce::Component,
                   public juce::SettableTooltipClient,
                   public juce::TextEditor::Listener,
                   public juce::KeyListener
{
public:
    TagChipBar();
    ~TagChipBar() override = default;

    std::function<void()> onTagsChanged;
    // Fires only when the user commits a tag via Enter in the input box.
    // Used by the editor to trigger Generate as soon as you finish typing.
    std::function<void()> onTagSubmitted;
    // Fires with the trimmed raw query on Enter BEFORE any tag is added.
    // Return true to signal the input was handled (e.g., matched a
    // pattern name and the editor navigated to it) — the chip-bar then
    // clears the input and skips the normal add/submit flow.
    std::function<bool (const juce::String&)> onRawInputHandled;

    void setAvailableTags (const juce::StringArray& all);
    void setSelectedTags  (const juce::StringArray& sel);
    juce::StringArray getSelectedTags() const { return selectedTags; }

    void paint    (juce::Graphics&)            override;
    void resized  ()                           override;
    void mouseDown(const juce::MouseEvent& e)  override;

    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override;

    const TagVectorIndex& getVectorIndex() const { return vectorIndex; }

private:
    juce::TextEditor   input;
    juce::StringArray  availableTags;
    juce::StringArray  selectedTags;
    TagVectorIndex     vectorIndex;

    struct Chip { juce::String tag; juce::Rectangle<int> bounds; juce::Rectangle<int> closeBox; };
    std::vector<Chip> chips;

    void layoutChips();
    void commitSearch();
    juce::String findFuzzyMatch (const juce::String& query) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagChipBar)
};

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
        g.setColour (hovered ? WillyBeatLookAndFeel::accent.withAlpha (0.18f)
                             : WillyBeatLookAndFeel::bgPanel);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (hovered ? WillyBeatLookAndFeel::accentBright
                             : WillyBeatLookAndFeel::accent.withAlpha (0.75f));
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
        g.setColour (findColour (juce::Label::textColourId));
        g.setFont (getFont());
        g.drawText (getText(), getLocalBounds(), getJustificationType(), true);
    }

private:
    bool hovered = false;
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
    explicit PatternGrid (WillyBeatAudioProcessor& p);
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

private:
    struct Layout;
    Layout computeLayout (const DrumPattern& pat) const;
    bool   cellAt (int x, int y, int& outRow, int& outCell, const Layout& L) const;
    void   updateBadgeAt (int x, int y);

    WillyBeatAudioProcessor& proc;
    DrumPattern*             editTarget = nullptr;
    int                      lastStep   = -1;

    // Drag-to-set-velocity state
    int dragRow      = -1;
    int dragCol      = -1;
    int dragStartVel = 0;
    bool dragMoved   = false;

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
    explicit MiniPatternView (WillyBeatAudioProcessor& p);
    ~MiniPatternView() override;

    void paint        (juce::Graphics& g)         override;
    void timerCallback()                           override;
    void mouseDown    (const juce::MouseEvent& e) override;

    void setEditTarget (DrumPattern* target) { editTarget = target; repaint(); }

    // Fired when the user clicks the thumbnail. Editor wires this to
    // expand back to the full grid.
    std::function<void()> onClick;

private:
    WillyBeatAudioProcessor& proc;
    DrumPattern*             editTarget = nullptr;
};

// ─── WillyBeatAudioProcessorEditor ───────────────────────────────────────────
class WillyBeatAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer,
                                      public juce::FileDragAndDropTarget
{
public:
    explicit WillyBeatAudioProcessorEditor (WillyBeatAudioProcessor&);
    ~WillyBeatAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;
    void timerCallback()           override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter          (const juce::StringArray& files, int x, int y) override;
    void fileDragExit           (const juce::StringArray& files) override;

private:
    WillyBeatAudioProcessor& audioProcessor;

    WillyBeatLookAndFeel lookAndFeel;
    juce::TooltipWindow  tooltipWindow { this, 1800 };

    DragStrip       dragStrip;
    PatternGrid     grid;
    TrackLabels     trackLabels;
    GridViewport    gridViewport;
    MiniPatternView miniGrid;

    // ── Pattern selector row ─────────────────────────────────────────────
    juce::Label    genreLabel   { {}, "Tags" };
    juce::Label    patLabel     { {}, "Pattern" };

    TagChipBar     tagBar;
    juce::Slider   patIdxSlider;

    juce::TextButton clearBtn    { "Clear" };
    juce::TextButton genBtn      { "Generate" };
    juce::TextButton collapseBtn { "-" };
    juce::TextButton soundBtn    { "Audio" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> patIdxAttach;
    std::unique_ptr<BA> soundAttach;

    // ── Knob row ─────────────────────────────────────────────────────────
    KnobLabel  gateLabel      { {}, "Duration" };
    KnobLabel  humanizeLabel  { {}, "Dynamics" };
    KnobLabel  swingLabel     { {}, "Swing" };
    KnobLabel  feelLabel      { {}, "Slop" };
    KnobLabel  densityLabel   { {}, "Density" };

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
    juce::Label      timeSigLabel { {}, "Time" };
    juce::ComboBox   timeSigBox;

    juce::Label      barsLabel    { {}, "Bars" };
    juce::ComboBox   barsBox;

    juce::Label      gridLabel    { {}, "Grid" };
    juce::ComboBox   gridBox;

    juce::Label      humanizeSectionLabel { {}, "Humanize" };
    juce::Label      fillSectionLabel     { {}, "Fill" };
    KnobLabel        fillStartLabel  { {}, "Start" };
    SlowScrollSlider fillStartKnob;
    std::unique_ptr<SA> fillStartAttach;

    KnobLabel        fillMidLabel    { {}, "Mid" };
    SlowScrollSlider fillMidKnob;
    std::unique_ptr<SA> fillMidAttach;

    KnobLabel        fillEndLabel    { {}, "End" };
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

    juce::StringArray lastKnownTags;

    // Last host-reported time signature we've applied to the pattern.
    // Used to mirror DAW time-sig changes into the active pattern.
    int lastHostTsNum = 0;
    int lastHostTsDen = 0;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessorEditor)
};

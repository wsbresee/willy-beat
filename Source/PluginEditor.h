#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WillyBeatLookAndFeel.h"

// ─── TagChipBar ──────────────────────────────────────────────────────────────
// Multi-select genre tags with fuzzy search.  Shows currently-selected tags
// as clickable "chips" followed by a search input; pressing Enter on the
// input fuzzy-matches against the available tags and adds the best match.
class TagChipBar : public juce::Component,
                   public juce::SettableTooltipClient,
                   public juce::TextEditor::Listener
{
public:
    TagChipBar();
    ~TagChipBar() override = default;

    std::function<void()> onTagsChanged;
    // Fires only when the user commits a tag via Enter in the input box.
    // Used by the editor to trigger Generate as soon as you finish typing.
    std::function<void()> onTagSubmitted;

    void setAvailableTags (const juce::StringArray& all);
    void setSelectedTags  (const juce::StringArray& sel);
    juce::StringArray getSelectedTags() const { return selectedTags; }

    void paint    (juce::Graphics&)            override;
    void resized  ()                           override;
    void mouseDown(const juce::MouseEvent& e)  override;

    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

private:
    juce::TextEditor   input;
    juce::StringArray  availableTags;
    juce::StringArray  selectedTags;

    struct Chip { juce::String tag; juce::Rectangle<int> bounds; juce::Rectangle<int> closeBox; };
    std::vector<Chip> chips;

    void layoutChips();
    void commitSearch();
    juce::String findFuzzyMatch (const juce::String& query) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagChipBar)
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
    void mouseWheelMove  (const juce::MouseEvent& e,
                          const juce::MouseWheelDetails& w)  override;

    void setEditTarget (DrumPattern* target);

    // Fired after a cell edit; passes the track and step that changed.
    std::function<void(int track, int step)> onCellChanged;

private:
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
    uint32_t  badgeFireMs   = 0;
    float     scrollAccum   = 0.0f; // partial-step accumulator for fine trackpad input

    static constexpr int kLabelW = 68;

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

    void setEditTarget (DrumPattern* target) { editTarget = target; repaint(); }

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
    juce::TooltipWindow  tooltipWindow { this, 700 };

    DragStrip       dragStrip;
    PatternGrid     grid;
    MiniPatternView miniGrid;

    // ── Pattern selector row ─────────────────────────────────────────────
    juce::Label    genreLabel   { {}, "Tags" };
    juce::Label    patLabel     { {}, "Pattern #" };

    TagChipBar     tagBar;
    juce::Slider   patIdxSlider;

    juce::TextButton genBtn      { "Generate" };
    juce::TextButton collapseBtn { "-" };
    juce::TextButton soundBtn    { "Audio" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> patIdxAttach;
    std::unique_ptr<BA> soundAttach;

    // ── Knob row ─────────────────────────────────────────────────────────
    juce::Label  gateLabel      { {}, "Gate %" };
    juce::Label  humanizeLabel  { {}, "Humanize" };
    juce::Label  swingLabel     { {}, "Swing" };
    juce::Label  feelLabel      { {}, "Feel" };
    juce::Label  densityLabel   { {}, "Density" };

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

    // ── Always-visible name / utility bar ───────────────────────────────
    juce::Label      nameLabel     { {}, "Name:" };
    juce::TextEditor nameEditor;
    juce::TextButton newPatBtn     { "New Pattern" };
    juce::TextButton openFolderBtn { "Open Folder" };
    juce::TextButton importMidiBtn { "Import MIDI" };
    juce::TextButton editPatternBtn { "Edit Pattern" };

    std::unique_ptr<juce::FileChooser> midiChooser;

    // ── Export / drag controls ───────────────────────────────────────────
    juce::Label      barsLabel       { {}, "Bars:" };
    juce::ComboBox   exportBarsBox;

    juce::Label      fillStartLabel  { {}, "Start" };
    juce::Slider     fillStartKnob;
    std::unique_ptr<SA> fillStartAttach;

    juce::Label      fillMidLabel    { {}, "Mid" };
    juce::Slider     fillMidKnob;
    std::unique_ptr<SA> fillMidAttach;

    juce::Label      fillEndLabel    { {}, "End" };
    juce::Slider     fillEndKnob;
    std::unique_ptr<SA> fillEndAttach;

    juce::Label      seedLabel       { {}, "Seed:" };
    juce::TextEditor seedEditor;

    // ── Live edit state ──────────────────────────────────────────────────
    DrumPattern             editingCopy;       // density-filtered view shown in grid
    DrumPattern             fullPattern;       // unfiltered source of truth (in memory)
    const DrumPattern*      lastKnownPattern = nullptr;
    float                   lastDensity      = -1.0f;

    void autoSaveCurrentEdit (int track, int step);
    void saveNameChange();
    void applyDensityToEditingCopy();
    void refreshTagSelector();
    void toggleCompactMode();

    bool compactMode = false;
    bool midiDragHovered = false;

    juce::StringArray lastKnownTags;

    int         getBarsFromCombo() const;
    juce::int64 getSeedFromEditor() const;
    const DrumPattern* findFill (const DrumPattern& pat, juce::int64 seed) const;
    DrumPattern buildFillPatternForExport (juce::int64 seed) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessorEditor)
};

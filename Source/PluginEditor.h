#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─── DragStrip ───────────────────────────────────────────────────────────────
// Bottom bar the user drags out of the plugin to drop a MIDI file into the DAW.
// The caller sets onDrag to a lambda that builds and returns the temp file.
class DragStrip : public juce::Component
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
class PatternGrid : public juce::Component, public juce::Timer
{
public:
    explicit PatternGrid (WillyBeatAudioProcessor& p);
    ~PatternGrid() override;

    void paint        (juce::Graphics& g)       override;
    void timerCallback()                         override;
    void mouseDown    (const juce::MouseEvent& e) override;

    void setEditTarget (DrumPattern* target);

    // Called on the message thread after every cell edit in edit mode.
    std::function<void()> onCellChanged;

private:
    WillyBeatAudioProcessor& proc;
    DrumPattern*             editTarget = nullptr;
    int                      lastStep   = -1;

    static constexpr int kLabelW = 68;

    static juce::Colour velColour (uint8_t vel);
};

// ─── WillyBeatAudioProcessorEditor ───────────────────────────────────────────
class WillyBeatAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit WillyBeatAudioProcessorEditor (WillyBeatAudioProcessor&);
    ~WillyBeatAudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    WillyBeatAudioProcessor& audioProcessor;

    DragStrip   dragStrip;
    PatternGrid grid;

    // ── Always-visible controls ──────────────────────────────────────────
    juce::Label    genreLabel     { {}, "Genre" };
    juce::Label    typeLabel      { {}, "Type" };
    juce::Label    patLabel       { {}, "Pattern #" };
    juce::Label    gateLabel      { {}, "Gate %" };
    juce::Label    humanizeLabel  { {}, "Humanize" };
    juce::Label    swingLabel     { {}, "Swing" };
    juce::Label    feelLabel      { {}, "Feel" };

    juce::ComboBox genreBox;
    juce::ComboBox typeBox;
    juce::Slider   patIdxSlider;
    juce::Slider   gateKnob;
    juce::Slider   humanizeKnob;
    juce::Slider   swingKnob;
    juce::Slider   feelKnob;

    juce::TextButton loadMidiBtn { "Load MIDI" };
    juce::TextButton genVarBtn   { "Variation" };
    juce::TextButton editBtn     { "Edit" };

    using CBA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA  = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<CBA> genreAttach;
    std::unique_ptr<CBA> typeAttach;
    std::unique_ptr<SA>  patIdxAttach;
    std::unique_ptr<SA>  gateAttach;
    std::unique_ptr<SA>  humanizeAttach;
    std::unique_ptr<SA>  swingAttach;
    std::unique_ptr<SA>  feelAttach;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // ── Edit-mode toolbar (hidden when not editing) ──────────────────────
    juce::Label      nameLabel    { {}, "Name:" };
    juce::TextEditor nameEditor;
    juce::TextButton doneBtn      { "Done" };
    juce::TextButton newPatBtn    { "New Pattern" };
    juce::TextButton openFolderBtn{ "Open Folder" };

    bool        editMode = false;
    DrumPattern editingCopy;

    void enterEditMode();
    void exitEditMode();
    void autoSaveCurrentEdit();

    // ── Export / drag controls ───────────────────────────────────────────
    juce::Label      barsLabel    { {}, "Bars:" };
    juce::ComboBox   exportBarsBox;
    juce::ToggleButton fillToggle { "Fill at end" };
    juce::Label      seedLabel    { {}, "Seed:" };
    juce::TextEditor seedEditor;

    int  getBarsFromCombo() const;
    juce::int64 getSeedFromEditor() const;
    const DrumPattern* findFill (const DrumPattern& mainPat) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessorEditor)
};

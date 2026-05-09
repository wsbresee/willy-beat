#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─── DragStrip ───────────────────────────────────────────────────────────────
// Bottom bar the user can drag out of the plugin to drop a MIDI file into the
// DAW's arrangement view.
class DragStrip : public juce::Component
{
public:
    explicit DragStrip (WillyBeatAudioProcessor& p);

    void paint      (juce::Graphics& g)           override;
    void mouseDown  (const juce::MouseEvent& e)   override;
    void mouseDrag  (const juce::MouseEvent& e)   override;
    void mouseEnter (const juce::MouseEvent& e)   override;
    void mouseExit  (const juce::MouseEvent& e)   override;

private:
    WillyBeatAudioProcessor& proc;
    bool hovered     = false;
    bool dragStarted = false;
};

// ─── PatternGrid ─────────────────────────────────────────────────────────────
// Draws the 16-step × 10-track velocity grid.  In edit mode it also accepts
// mouse clicks to cycle the velocity of individual cells.
class PatternGrid : public juce::Component, public juce::Timer
{
public:
    explicit PatternGrid (WillyBeatAudioProcessor& p);
    ~PatternGrid() override;

    void paint (juce::Graphics& g) override;
    void timerCallback() override;

    // Edit mode: set a working copy to display and modify.  Pass nullptr to
    // leave edit mode and go back to displaying the active pattern.
    void setEditTarget (DrumPattern* target);

    void mouseDown (const juce::MouseEvent& e) override;

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
    juce::TextButton saveBtn      { "Save" };
    juce::TextButton cancelBtn    { "Cancel" };
    juce::TextButton newPatBtn    { "New Pattern" };
    juce::TextButton openFolderBtn{ "Open Folder" };

    bool        editMode = false;
    DrumPattern editingCopy;

    void enterEditMode();
    void exitEditMode (bool save);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessorEditor)
};

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Draws the 16-step × 10-track velocity grid and highlights the playing step.
class PatternGrid : public juce::Component,
                    public juce::Timer
{
public:
    explicit PatternGrid (WillyBeatAudioProcessor& p);
    ~PatternGrid() override;
    void paint (juce::Graphics& g) override;
    void timerCallback() override;

private:
    WillyBeatAudioProcessor& proc;
    int lastStep = -1;

    static juce::Colour velColour (uint8_t vel);
};

//==============================================================================

class WillyBeatAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit WillyBeatAudioProcessorEditor (WillyBeatAudioProcessor&);
    ~WillyBeatAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WillyBeatAudioProcessor& audioProcessor;

    PatternGrid grid;

    juce::Label    genreLabel  { {}, "Genre" };
    juce::Label    typeLabel   { {}, "Type" };
    juce::Label    patLabel    { {}, "Pattern #" };
    juce::Label    gateLabel   { {}, "Gate %" };

    juce::ComboBox genreBox;
    juce::ComboBox typeBox;
    juce::Slider   patIdxSlider;
    juce::Slider   gateKnob;

    juce::TextButton loadMidiBtn { "Load MIDI" };
    juce::TextButton genVarBtn   { "Variation" };

    using CBA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SA  = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<CBA> genreAttach;
    std::unique_ptr<CBA> typeAttach;
    std::unique_ptr<SA>  patIdxAttach;
    std::unique_ptr<SA>  gateAttach;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessorEditor)
};

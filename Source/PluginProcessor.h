#pragma once
#include <JuceHeader.h>
#include "DrumPattern.h"
#include "PatternLibrary.h"
#include "VariationGenerator.h"

class WillyBeatAudioProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    WillyBeatAudioProcessor();
    ~WillyBeatAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi()  const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void parameterChanged (const juce::String& id, float value) override;

    // --- public interface for the editor ---
    PatternLibrary& getLibrary() { return library; }
    const DrumPattern* getActivePattern() const { return activePattern; }
    std::atomic<int>& getCurrentStep()    { return currentStep; }

    // Import a MIDI file and store it as a user pattern
    bool loadMidiFile (const juce::File& file);
    // Generate a variation of the active pattern and store it
    void generateVariation();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void selectPattern();

    PatternLibrary    library;
    juce::Random      rng;
    VariationGenerator generator { rng };

    const DrumPattern* activePattern = nullptr;

    // Sequencer state
    std::atomic<int> currentStep { 0 };
    long lastFiredStep = -1;
    bool wasPlaying    = false;

    // Note-off queue
    struct ActiveNote { int note; long offAtSample; };
    std::vector<ActiveNote> activeNotes;
    long absoluteSample = 0;

    void killAllNotes (juce::MidiBuffer& midi, int offset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessor)
};

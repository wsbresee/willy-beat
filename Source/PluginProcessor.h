#pragma once
#include <JuceHeader.h>
#include "DrumPattern.h"
#include "PatternLibrary.h"
#include "VariationGenerator.h"
#include "StockDrums.h"

class WillyBeatAudioProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    WillyBeatAudioProcessor();
    ~WillyBeatAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

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
    PatternLibrary&    getLibrary()       { return library; }
    const DrumPattern* getActivePattern() const { return activePattern; }
    std::atomic<int>&  getCurrentStep()   { return currentStep; }

    juce::File getPresetsDirectory() const;

    // Import a MIDI file and store it as a user preset
    bool loadMidiFile (const juce::File& file);

    // Generate a variation of the active pattern and store it as a preset
    void generateVariation();

    // Generate a composite pattern from same-genre patterns and store it
    void generateComposite();

    // Save an edited pattern (from the in-plugin editor) to disk and reload
    void saveEditedPattern (const DrumPattern& p);

    // Write the pattern file immediately without reloading the library.
    // Used for auto-save on every cell edit so edits survive plugin crashes.
    void autoSavePattern (DrumPattern& p);

    juce::AudioProcessorValueTreeState apvts;

    // ── Multi-tag genre selection (stored as a CSV property in apvts.state) ─
    juce::StringArray getSelectedGenreTags() const;
    void              setSelectedGenreTags (const juce::StringArray& tags);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void selectPattern();
    void navigateToPattern (const DrumPattern& p);
    void reloadLibrary();

    PatternLibrary    library;
    juce::Random      rng;
    VariationGenerator generator { rng };

    // Separate RNG for audio thread (only accessed from processBlock)
    juce::Random audioRng { juce::Random::getSystemRandom().nextInt64() };

    const DrumPattern* activePattern = nullptr;

    std::atomic<int> currentStep { 0 };
    long lastFiredStep = -1;
    bool wasPlaying    = false;

    struct ActiveNote { int note; long offAtSample; };
    std::vector<ActiveNote> activeNotes;
    long absoluteSample = 0;

    StockDrums stockDrums;

    void killAllNotes (juce::MidiBuffer& midi, int offset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WillyBeatAudioProcessor)
};

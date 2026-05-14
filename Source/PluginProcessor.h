#pragma once
#include <JuceHeader.h>
#include "DrumPattern.h"
#include "PatternLibrary.h"
#include "VariationGenerator.h"
#include "StockDrums.h"

class DrumwrightAudioProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    DrumwrightAudioProcessor();
    ~DrumwrightAudioProcessor() override;

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
    const DrumPattern* getActivePattern() const { return activePattern.load(); }
    std::atomic<int>&  getCurrentStep()   { return currentStep; }
    std::atomic<int>&  getCurrentTick()   { return currentTick; }   // tick within the active pattern

    // Most recent time signature reported by the host. (0, 0) when the
    // host hasn't surfaced one (e.g., the standalone build).
    std::atomic<int>&  getHostTimeSigNum() { return hostTsNum; }
    std::atomic<int>&  getHostTimeSigDen() { return hostTsDen; }

    juce::File getPresetsDirectory() const;

    // Import a MIDI file and store it as a user preset
    bool loadMidiFile (const juce::File& file);

    // Navigate to a saved pattern by exact case-insensitive name match.
    // Returns true if found and selected.
    bool navigateToPatternByName (const juce::String& name);

    // Generate a composite pattern from same-genre patterns and store it.
    // Defaults to using the user's currently-selected tags for both the
    // source pool and the new pattern's saved genres. The editor uses the
    // overload to pass a vector-expanded source pool while still saving
    // the pattern with the user's original chip-bar selection.
    void generateComposite();
    void generateComposite (const juce::StringArray& sourcePool,
                            const juce::StringArray& assignedGenres);

    // Save an edited pattern (from the in-plugin editor) to disk and reload
    void saveEditedPattern (const DrumPattern& p);

    // Write the pattern file immediately without reloading the library.
    // Used for auto-save on every cell edit so edits survive plugin crashes.
    void autoSavePattern (DrumPattern& p);

    // Delete every .beat file in the presets directory, reload the (now-empty)
    // library, and reset patIdx to 0. Used for the Shift+Clear factory reset.
    void resetAllPatterns();

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

    std::atomic<const DrumPattern*> activePattern { nullptr };

    std::atomic<int> currentStep { 0 };
    std::atomic<int> currentTick { 0 };
    std::atomic<int> hostTsNum   { 0 };
    std::atomic<int> hostTsDen   { 0 };
    long   lastFiredAbsTick[NUM_TRACKS] = {};   // per-track last-fired absolute tick (re-fire guard)
    double lastPPQEnd = -1.0;                   // ppqEnd from last playing block, for rewind detection
    bool   wasPlaying = false;

    struct ActiveNote { int note; long offAtSample; };
    std::vector<ActiveNote> activeNotes;
    long absoluteSample = 0;

    StockDrums stockDrums;

    void killAllNotes (juce::MidiBuffer& midi, int offset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumwrightAudioProcessor)
};

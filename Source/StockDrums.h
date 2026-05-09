#pragma once
#include <JuceHeader.h>

// Tiny synthesized drum kit used for in-plugin pattern preview.
// Each GM channel-10 note triggers a short envelope-shaped voice;
// the kit is intentionally low-fidelity but distinct enough to scrub
// patterns without loading a real sampler.
class StockDrums
{
public:
    void prepare (double sampleRate);

    // Trigger from MIDI note-on. velocity is 0..1.
    void noteOn (int gmNote, float velocity);

    // Render into a stereo buffer (additive — caller pre-clears).
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // Clear all sounding voices (e.g. on transport stop).
    void allNotesOff();

private:
    enum class Kind { Kick, Snare, Rim, Clap, ClosedHat, OpenHat, Crash, Ride, TomH, TomM, TomL, Cowbell, Other };
    static Kind classify (int gmNote);

    struct Voice
    {
        Kind   kind   = Kind::Other;
        bool   active = false;
        double phase  = 0.0;     // generic phase counter (samples)
        double freq   = 0.0;     // for tonal voices, current freq (Hz)
        double pitchEnvSamples = 0.0;
        double ampEnvSamples   = 0.0;
        float  velocity = 1.0f;
        juce::Random rng { juce::Random::getSystemRandom().nextInt64() };
    };

    static constexpr int kMaxVoices = 16;
    std::array<Voice, kMaxVoices> voices;

    double sr = 44100.0;

    float renderVoice (Voice& v);
};

#include "StockDrums.h"
#include "DrumPattern.h"

void StockDrums::prepare (double sampleRate)
{
    sr = sampleRate;
    allNotesOff();
}

void StockDrums::allNotesOff()
{
    for (auto& v : voices) v.active = false;
}

StockDrums::Kind StockDrums::classify (int gmNote)
{
    switch (gmNote)
    {
        case 35: case GM::KICK:                  return Kind::Kick;
        case GM::RIM:                            return Kind::Rim;
        case GM::SNARE: case 40:                 return Kind::Snare;
        case GM::CLAP:                           return Kind::Clap;
        case GM::HIHAT_C: case 44:               return Kind::ClosedHat;
        case GM::HIHAT_O:                        return Kind::OpenHat;
        case GM::CRASH:   case 57:               return Kind::Crash;
        case GM::RIDE:    case 53: case 59:      return Kind::Ride;
        case GM::TOM_H:   case 50:               return Kind::TomH;
        case GM::TOM_M:                          return Kind::TomM;
        case GM::TOM_L:   case 45:               return Kind::TomL;
        case GM::COWBELL:                        return Kind::Cowbell;
        default:                                 return Kind::Other;
    }
}

void StockDrums::noteOn (int gmNote, float velocity)
{
    auto kind = classify (gmNote);

    Voice* slot = nullptr;
    for (auto& v : voices)
        if (! v.active) { slot = &v; break; }
    if (slot == nullptr)
        slot = &voices[0]; // steal oldest

    slot->kind     = kind;
    slot->active   = true;
    slot->phase    = 0.0;
    slot->velocity = juce::jlimit (0.0f, 1.0f, velocity);
    slot->rng      = juce::Random (juce::Random::getSystemRandom().nextInt64());

    // Per-kind envelope lengths (in samples) and starting frequency.
    switch (kind)
    {
        case Kind::Kick:      slot->freq = 80.0;   slot->ampEnvSamples = sr * 0.30;  slot->pitchEnvSamples = sr * 0.05;  break;
        case Kind::Snare:     slot->freq = 200.0;  slot->ampEnvSamples = sr * 0.18;  slot->pitchEnvSamples = sr * 0.02;  break;
        case Kind::Rim:       slot->freq = 1200.0; slot->ampEnvSamples = sr * 0.05;  slot->pitchEnvSamples = sr * 0.005; break;
        case Kind::Clap:      slot->freq = 0.0;    slot->ampEnvSamples = sr * 0.20;  slot->pitchEnvSamples = 0.0;        break;
        case Kind::ClosedHat: slot->freq = 8000.0; slot->ampEnvSamples = sr * 0.05;  slot->pitchEnvSamples = 0.0;        break;
        case Kind::OpenHat:   slot->freq = 7000.0; slot->ampEnvSamples = sr * 0.30;  slot->pitchEnvSamples = 0.0;        break;
        case Kind::Crash:     slot->freq = 5000.0; slot->ampEnvSamples = sr * 1.20;  slot->pitchEnvSamples = 0.0;        break;
        case Kind::Ride:      slot->freq = 6000.0; slot->ampEnvSamples = sr * 0.60;  slot->pitchEnvSamples = 0.0;        break;
        case Kind::TomH:      slot->freq = 220.0;  slot->ampEnvSamples = sr * 0.30;  slot->pitchEnvSamples = sr * 0.10;  break;
        case Kind::TomM:      slot->freq = 160.0;  slot->ampEnvSamples = sr * 0.35;  slot->pitchEnvSamples = sr * 0.12;  break;
        case Kind::TomL:      slot->freq = 100.0;  slot->ampEnvSamples = sr * 0.40;  slot->pitchEnvSamples = sr * 0.15;  break;
        case Kind::Cowbell:   slot->freq = 800.0;  slot->ampEnvSamples = sr * 0.15;  slot->pitchEnvSamples = 0.0;        break;
        case Kind::Other:     slot->freq = 440.0;  slot->ampEnvSamples = sr * 0.10;  slot->pitchEnvSamples = 0.0;        break;
    }
}

float StockDrums::renderVoice (Voice& v)
{
    if (! v.active) return 0.0f;

    const double t = v.phase / sr;
    const double envT = v.phase / v.ampEnvSamples;
    if (envT >= 1.0)
    {
        v.active = false;
        return 0.0f;
    }

    // Exponential amplitude envelope (sharper attack/decay than linear).
    const float amp = std::exp (-3.5f * (float) envT) * v.velocity;

    float sample = 0.0f;
    auto noise = [&]() { return v.rng.nextFloat() * 2.0f - 1.0f; };

    switch (v.kind)
    {
        case Kind::Kick:
        {
            // 80 Hz -> 35 Hz pitch sweep with a brief click on top.
            double freqEnv = std::exp (-v.phase / v.pitchEnvSamples);
            double f       = 35.0 + (80.0 - 35.0) * freqEnv;
            sample = std::sin (2.0 * juce::MathConstants<double>::pi * f * t) * 0.9f;
            if (envT < 0.02) sample += noise() * 0.4f * (float) (1.0 - envT * 50.0);
            break;
        }
        case Kind::Snare:
        {
            double freqEnv = std::exp (-v.phase / v.pitchEnvSamples);
            double f       = 180.0 + 120.0 * freqEnv;
            float tonal    = std::sin (2.0 * juce::MathConstants<double>::pi * f * t) * 0.4f;
            float body     = noise() * 0.7f;
            sample = tonal + body;
            break;
        }
        case Kind::Rim:
        {
            sample = std::sin (2.0 * juce::MathConstants<double>::pi * v.freq * t) * 0.6f
                   + noise() * 0.3f;
            break;
        }
        case Kind::Clap:
        {
            // Three slightly delayed noise bursts before the long tail.
            float burst = 0.0f;
            const double bursts[] = { 0.0, 0.012, 0.024 };
            for (double b : bursts)
                if (t >= b && t < b + 0.008)
                    burst += noise();
            sample = burst * 0.7f + noise() * 0.25f;
            break;
        }
        case Kind::ClosedHat:
        case Kind::OpenHat:
        {
            // Metallic hat: sum of high inharmonic squares + filtered noise.
            float metallic = 0.0f;
            const double ratios[] = { 1.0, 1.34, 1.79, 2.22, 2.65, 3.05 };
            for (double r : ratios)
            {
                double f  = v.freq * r;
                double ph = 2.0 * juce::MathConstants<double>::pi * f * t;
                metallic += std::sin (ph) > 0.0 ? 0.10f : -0.10f;
            }
            sample = metallic + noise() * 0.25f;
            break;
        }
        case Kind::Crash:
        case Kind::Ride:
        {
            float metallic = 0.0f;
            const double ratios[] = { 1.0, 1.41, 1.78, 2.20 };
            for (double r : ratios)
            {
                double f  = v.freq * r;
                metallic += std::sin (2.0 * juce::MathConstants<double>::pi * f * t) * 0.10f;
            }
            sample = metallic + noise() * 0.20f;
            break;
        }
        case Kind::TomH:
        case Kind::TomM:
        case Kind::TomL:
        {
            double freqEnv = std::exp (-v.phase / v.pitchEnvSamples);
            double f       = v.freq * (0.5 + 0.5 * freqEnv);
            sample = std::sin (2.0 * juce::MathConstants<double>::pi * f * t) * 0.85f;
            break;
        }
        case Kind::Cowbell:
        {
            sample = std::sin (2.0 * juce::MathConstants<double>::pi * v.freq * t) * 0.5f
                   + std::sin (2.0 * juce::MathConstants<double>::pi * v.freq * 1.5 * t) * 0.4f;
            break;
        }
        case Kind::Other:
            sample = std::sin (2.0 * juce::MathConstants<double>::pi * v.freq * t) * 0.5f;
            break;
    }

    v.phase += 1.0;
    return sample * amp;
}

void StockDrums::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int numCh = buffer.getNumChannels();
    if (numCh == 0) return;

    for (int n = 0; n < numSamples; ++n)
    {
        float mix = 0.0f;
        for (auto& v : voices)
            if (v.active) mix += renderVoice (v);

        // Soft clip + gain trim so dense patterns don't blow up.
        mix = std::tanh (mix * 0.4f);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addSample (ch, startSample + n, mix);
    }
}

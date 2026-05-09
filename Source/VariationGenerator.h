#pragma once
#include "DrumPattern.h"

// Generates new patterns from existing ones algorithmically.
// All methods return a new DrumPattern — inputs are never modified.
class VariationGenerator
{
public:
    explicit VariationGenerator (juce::Random& rng) : rng (rng) {}

    // Small variations: ghost notes, hihat swaps, velocity humanisation
    DrumPattern makeVariance (const DrumPattern& source) const;

    // Replace the last 4 steps with a descending tom fill
    DrumPattern makeSmallFill (const DrumPattern& source) const;

    // Replace most of the bar with a dense snare + tom run
    DrumPattern makeBigFill (const DrumPattern& source) const;

    // Analyse an imported pattern and generate a "clean" version
    DrumPattern humanise (const DrumPattern& source) const;

private:
    juce::Random& rng;

    void addGhostNotes     (DrumPattern& p) const;
    void varyHihat         (DrumPattern& p) const;
    void displaceSomeKicks (DrumPattern& p) const;
    void humaniseVelocities(DrumPattern& p) const;
};

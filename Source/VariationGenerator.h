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

private:
    juce::Random& rng;

    void addGhostNotes     (DrumPattern& p) const;
    void varyHihat         (DrumPattern& p) const;
    void displaceSomeKicks (DrumPattern& p) const;
    void humaniseVelocities(DrumPattern& p) const;
};

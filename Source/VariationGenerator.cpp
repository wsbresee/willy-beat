#include "VariationGenerator.h"

DrumPattern VariationGenerator::makeVariance (const DrumPattern& src) const
{
    DrumPattern p = src;
    p.name = src.name + " (Var)";
    p.type = PatType::Variance;

    addGhostNotes      (p);
    varyHihat          (p);
    displaceSomeKicks  (p);
    humaniseVelocities (p);

    return p;
}

//==============================================================================

void VariationGenerator::addGhostNotes (DrumPattern& p) const
{
    // For each hard snare hit, maybe add a ghost one step before
    for (int s = 1; s < MAX_STEPS; ++s)
    {
        if (p.velocities[TR_SNARE][s] >= 80 && p.velocities[TR_SNARE][s-1] == 0)
            if (rng.nextFloat() < 0.5f)
                p.velocities[TR_SNARE][s-1] = 20 + rng.nextInt (15);
    }
}

void VariationGenerator::varyHihat (DrumPattern& p) const
{
    // Randomly open a closed hihat on an "and" (even step 2,6,10,14)
    static const int ands[] = { 2, 6, 10, 14 };
    for (int s : ands)
    {
        if (p.velocities[TR_HIHAT_C][s] > 0 && rng.nextFloat() < 0.3f)
        {
            p.velocities[TR_HIHAT_O][s] = p.velocities[TR_HIHAT_C][s];
            p.velocities[TR_HIHAT_C][s] = 0;
        }
    }
}

void VariationGenerator::displaceSomeKicks (DrumPattern& p) const
{
    // Shift one kick hit by 1 step forward with 25% probability
    for (int s = 0; s < MAX_STEPS - 1; ++s)
    {
        if (p.velocities[TR_KICK][s] > 0 && p.velocities[TR_KICK][s+1] == 0)
        {
            if (rng.nextFloat() < 0.25f)
            {
                p.velocities[TR_KICK][s+1] = p.velocities[TR_KICK][s];
                p.velocities[TR_KICK][s]   = 0;
                break; // displace at most one kick per call
            }
        }
    }
}

void VariationGenerator::humaniseVelocities (DrumPattern& p) const
{
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (int s = 0; s < MAX_STEPS; ++s)
            if (p.velocities[t][s] > 0)
            {
                int v = (int) p.velocities[t][s] + rng.nextInt (11) - 5; // ±5
                p.velocities[t][s] = (uint8_t) juce::jlimit (1, 127, v);
            }
}

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

DrumPattern VariationGenerator::makeSmallFill (const DrumPattern& src) const
{
    DrumPattern p = src;
    p.name = src.name + " (Sm.Fill)";
    p.type = PatType::SmallFill;

    const int fillStart = 12; // last 4 steps

    // Silence hihats and ride in the fill region
    for (int s = fillStart; s < MAX_STEPS; ++s)
    {
        p.velocities[TR_HIHAT_C][s] = 0;
        p.velocities[TR_HIHAT_O][s] = 0;
        p.velocities[TR_RIDE][s]    = 0;
    }

    // Descending tom fill: H → M → L over last 4 steps
    static const int tomTracks[3] = { TR_TOM_H, TR_TOM_M, TR_TOM_L };
    for (int i = 0; i < 3; ++i)
    {
        int step = fillStart + i; // steps 12, 13, 14
        p.velocities[tomTracks[i]][step] = 100;
    }

    // Snare accent on the last step
    p.velocities[TR_SNARE][15] = 120;

    return p;
}

DrumPattern VariationGenerator::makeBigFill (const DrumPattern& src) const
{
    DrumPattern p = src;
    p.name = src.name + " (BigFill)";
    p.type = PatType::BigFill;

    const int fillStart = 4; // fill from beat 2 onward

    // Keep only beat-1 kick; clear everything else
    for (int s = fillStart; s < MAX_STEPS; ++s)
        for (int t = 0; t < NUM_TRACKS; ++t)
            p.velocities[t][s] = 0;

    // Clear hihat/ride entirely
    for (int s = 0; s < MAX_STEPS; ++s)
    {
        p.velocities[TR_HIHAT_C][s] = 0;
        p.velocities[TR_HIHAT_O][s] = 0;
        p.velocities[TR_RIDE][s]    = 0;
    }

    // Beat 2: snare accent
    p.velocities[TR_SNARE][4] = 110;

    // Steps 7–15: rolling tom run descending
    static const int descToms[3] = { TR_TOM_H, TR_TOM_M, TR_TOM_L };
    for (int s = 7; s < MAX_STEPS; ++s)
    {
        int tomIdx = (s - 7) / 3;
        if (tomIdx > 2) tomIdx = 2;
        p.velocities[descToms[tomIdx]][s] = (s % 2 == 0) ? 100 : 80;
    }

    // Accent snare on steps 8 and 12
    p.velocities[TR_SNARE][8]  = 120;
    p.velocities[TR_SNARE][12] = 120;

    // Crash on beat 1 to mark section start
    p.velocities[TR_CRASH][0] = 120;

    return p;
}

DrumPattern VariationGenerator::humanise (const DrumPattern& src) const
{
    DrumPattern p = src;
    p.name = src.name + " (Hum)";
    humaniseVelocities (p);
    addGhostNotes (p);
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

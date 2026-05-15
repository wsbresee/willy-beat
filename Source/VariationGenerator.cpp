#include "VariationGenerator.h"
#include <algorithm>

DrumPattern VariationGenerator::makeVariance (const DrumPattern& src) const
{
    DrumPattern p = src;
    p.type = PatType::Variance;

    addGhostNotes      (p);
    varyHihat          (p);
    displaceSomeKicks  (p);
    humaniseVelocities (p);

    return p;
}

//==============================================================================

namespace {
    bool hasTick (const std::vector<DrumHit>& track, int tick)
    {
        for (const auto& h : track)
            if (h.tick == tick) return true;
        return false;
    }

    void sortHits (std::vector<DrumHit>& v)
    {
        std::sort (v.begin(), v.end(), [] (const DrumHit& a, const DrumHit& b) {
            return a.tick < b.tick;
        });
    }
}

void VariationGenerator::addGhostNotes (DrumPattern& p) const
{
    // For each loud snare hit, maybe add a quiet ghost one 16th before it.
    const int ghostStep = PPQN / 4;
    auto& snare = p.hits[TR_SNARE];
    std::vector<DrumHit> toAdd;
    for (const auto& h : snare)
    {
        const int ghostTick = h.tick - ghostStep;
        if (h.velocity >= 80 && ghostTick >= 0
                && !hasTick (snare, ghostTick) && rng.nextFloat() < 0.5f)
            toAdd.push_back ({ ghostTick, (uint8_t) (20 + rng.nextInt (15)) });
    }
    for (const auto& g : toAdd)
        snare.push_back (g);
    sortHits (snare);
}

void VariationGenerator::varyHihat (DrumPattern& p) const
{
    // Randomly swap closed-hihat hits on off-beats to open hihat.
    auto& hhc = p.hits[TR_HIHAT_C];
    auto& hho = p.hits[TR_HIHAT_O];

    for (auto it = hhc.begin(); it != hhc.end(); )
    {
        const bool isOffBeat = (it->tick % (PPQN)) != 0;
        if (isOffBeat && rng.nextFloat() < 0.3f)
        {
            if (!hasTick (hho, it->tick))
                hho.push_back (*it);
            it = hhc.erase (it);
        }
        else ++it;
    }
    sortHits (hho);
}

void VariationGenerator::displaceSomeKicks (DrumPattern& p) const
{
    // Shift one kick hit one 16th forward with 25% probability.
    const int stepTicks  = PPQN / 4;
    const int patLen     = p.totalTicks();
    auto& kicks = p.hits[TR_KICK];
    for (auto& h : kicks)
    {
        if (rng.nextFloat() < 0.25f)
        {
            const int newTick = h.tick + stepTicks;
            if (newTick < patLen && !hasTick (kicks, newTick))
            {
                h.tick = newTick;
                break;
            }
        }
    }
    sortHits (kicks);
}

void VariationGenerator::humaniseVelocities (DrumPattern& p) const
{
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (auto& h : p.hits[t])
        {
            int v = (int) h.velocity + rng.nextInt (11) - 5;
            h.velocity = (uint8_t) juce::jlimit (1, 127, v);
        }
}

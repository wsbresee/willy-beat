#pragma once
#include <JuceHeader.h>

#include <algorithm>

// Standard GM MIDI drum note numbers (channel 10)
namespace GM
{
    constexpr int KICK      = 36;
    constexpr int RIM       = 37;
    constexpr int SNARE     = 38;
    constexpr int CLAP      = 39;
    constexpr int HIHAT_C   = 42;
    constexpr int HIHAT_O   = 46;
    constexpr int TOM_H     = 48;
    constexpr int TOM_M     = 43;
    constexpr int TOM_L     = 41;
    constexpr int CRASH     = 49;
    constexpr int RIDE      = 51;
    constexpr int COWBELL   = 56;
}

// Track slot indices — fixed order used throughout
enum TrackSlot
{
    TR_KICK = 0,
    TR_SNARE,
    TR_HIHAT_C,
    TR_HIHAT_O,
    TR_RIDE,
    TR_CRASH,
    TR_TOM_H,
    TR_TOM_M,
    TR_TOM_L,
    TR_RIM,
    NUM_TRACKS
};

static const int kTrackNotes[NUM_TRACKS] = {
    GM::KICK, GM::SNARE, GM::HIHAT_C, GM::HIHAT_O,
    GM::RIDE, GM::CRASH, GM::TOM_H,   GM::TOM_M,
    GM::TOM_L, GM::RIM
};

static const char* kTrackNames[NUM_TRACKS] = {
    "Kick", "Snare", "HH Cls", "HH Opn",
    "Ride", "Crash", "Tom H", "Tom M",
    "Tom L", "Rim"
};

enum class PatType { Regular = 0, Variance, SmallFill, BigFill, NUM_TYPES };

static const char* kPatTypeNames[] = { "Regular", "Variance", "Sm. Fill", "Big Fill" };

static const char* kTrackFileKeys[NUM_TRACKS] = {
    "kick", "snare", "hihat_c", "hihat_o",
    "ride", "crash", "tom_h",   "tom_m",
    "tom_l", "rim"
};

// ─── Tick resolution ──────────────────────────────────────────────────────
// 96 PPQN is the LCM that lets 16ths (24t), 8th triplets (32t), 16th
// triplets (16t), and 32nds (12t) all land on integer ticks. Quarter = 96.
constexpr int PPQN = 96;

// Grid subdivisions the editor can render.
enum class GridSub
{
    Eighth         = 0,   // 2 cells / quarter, 48 ticks/cell
    EighthTriplet  = 1,   // 3 cells / quarter, 32 ticks/cell
    Sixteenth      = 2,   // 4 cells / quarter, 24 ticks/cell  (v1 default)
    SixteenthTriplet = 3, // 6 cells / quarter, 16 ticks/cell
    ThirtySecond   = 4,   // 8 cells / quarter, 12 ticks/cell
    NUM_GRID_SUBS
};

static const char* kGridSubNames[] = { "8th", "8th Triplet", "16th", "16th Triplet", "32nd" };
static const char* kGridSubFileKeys[] = { "8th", "8tr", "16th", "16tr", "32nd" };

inline int gridSubCellTicks (GridSub g)
{
    switch (g)
    {
        case GridSub::Eighth:           return PPQN / 2;
        case GridSub::EighthTriplet:    return PPQN / 3;
        case GridSub::Sixteenth:        return PPQN / 4;
        case GridSub::SixteenthTriplet: return PPQN / 6;
        case GridSub::ThirtySecond:     return PPQN / 8;
        default:                        return PPQN / 4;
    }
}

inline bool gridSubIsTriplet (GridSub g)
{
    return g == GridSub::EighthTriplet || g == GridSub::SixteenthTriplet;
}

// Legacy step count — kept for any code still operating on a 16-step grid
// during the v1 → v2 transition.
constexpr int MAX_STEPS = 16;

// ─── DrumHit ──────────────────────────────────────────────────────────────
struct DrumHit
{
    int     tick;       // 0-based absolute tick within the pattern
    uint8_t velocity;   // 1..127 (we don't store 0-velocity entries)
};

// ─── DrumPattern ──────────────────────────────────────────────────────────
// Tick-based representation. Patterns carry their own time signature,
// bar count, and per-track hit lists, freeing the engine from the old
// fixed 16-step / 4/4 / 1-bar assumption.
//
// The legacy `velocities[track][step]` array is still present as a
// backward-compat shim so consumers can be migrated incrementally. Call
// syncLegacyFromHits() after editing hits[]; syncHitsFromLegacy() when
// the legacy grid was edited. Both are no-ops for patterns whose shape
// doesn't fit the legacy 16-step / 4/4 / 1-bar mould.
struct DrumPattern
{
    juce::String      name;
    juce::StringArray genres;
    PatType           type        = PatType::Regular;
    float             density     = 0.0f;
    bool              isComposite = false;
    juce::File        sourceFile;

    // ── Pattern shape ────────────────────────────────────────────────────
    int timeSigNum = 4;   // numerator (4 for 4/4, 6 for 6/8, ...)
    int timeSigDen = 4;   // denominator: must be a power of 2 (1, 2, 4, 8, 16)
    int bars       = 1;   // pattern length in bars (1..8)

    // Editor view: which grid subdivision is the natural display for this
    // pattern. Not data-bearing - hits[] always stores raw ticks - but
    // remembered so the editor opens patterns on their authored grid.
    GridSub gridSub = GridSub::Sixteenth;

    // Per-track hit lists, sorted by tick. Tick units are PPQN at the
    // pattern's resolution.
    std::vector<DrumHit> hits[NUM_TRACKS];

    // ── Legacy 16-step shim (deprecated; will be removed) ────────────────
    int     numSteps = MAX_STEPS;
    uint8_t velocities[NUM_TRACKS][MAX_STEPS] = {};

    // ── Derived quantities ───────────────────────────────────────────────
    int totalTicks() const
    {
        // bars × (timeSigNum / timeSigDen) × 4 × PPQN
        return bars * timeSigNum * 4 * PPQN / timeSigDen;
    }

    int ticksPerBeat() const   // ticks per denominator unit
    {
        return PPQN * 4 / timeSigDen;
    }

    void recomputeDensity()
    {
        int active = 0;
        for (int t = 0; t < NUM_TRACKS; ++t)
            active += (int) hits[t].size();
        const int sixteenthsInPattern = totalTicks() / 24;
        const int total = NUM_TRACKS * juce::jmax (1, sixteenthsInPattern);
        // Cap at 1.0: a fine-subdivision pattern can hold more hits than
        // the 16th-cell denominator suggests; the density signal we want
        // is just "more or less full".
        const float raw = (total > 0) ? (float) active / (float) total : 0.0f;
        density = juce::jlimit (0.0f, 1.0f, raw);
    }

    // ── Backward-compat sync helpers ─────────────────────────────────────
    // Project tick-based hits[] onto a 16-step legacy grid. Lossy when
    // the pattern doesn't fit 1 bar of 4/4 16ths.
    void syncLegacyFromHits()
    {
        numSteps = MAX_STEPS;
        for (int t = 0; t < NUM_TRACKS; ++t)
            for (int s = 0; s < MAX_STEPS; ++s)
                velocities[t][s] = 0;

        const int stepTicks = totalTicks() / MAX_STEPS;
        if (stepTicks <= 0) return;
        for (int t = 0; t < NUM_TRACKS; ++t)
            for (const auto& h : hits[t])
            {
                const int step = h.tick / stepTicks;
                if (step >= 0 && step < MAX_STEPS)
                    velocities[t][step] = (uint8_t) juce::jmax<int> (velocities[t][step], h.velocity);
            }
    }

    // Build hits[] from the legacy 16-step grid. Assumes 4/4 1-bar.
    void syncHitsFromLegacy()
    {
        timeSigNum = 4;
        timeSigDen = 4;
        bars       = 1;
        const int stepTicks = (numSteps > 0) ? totalTicks() / numSteps : 0;
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            hits[t].clear();
            for (int s = 0; s < numSteps; ++s)
                if (velocities[t][s] > 0)
                    hits[t].push_back ({ s * stepTicks, velocities[t][s] });
        }
    }

    // Legacy row decoder used by the v1 parser. v2 parser populates hits[]
    // directly.
    void setRow (int track, const char* s)
    {
        for (int i = 0; i < MAX_STEPS && s[i]; ++i)
        {
            switch (s[i])
            {
                case 'g': velocities[track][i] = 25;  break;
                case 's': velocities[track][i] = 55;  break;
                case 'm': velocities[track][i] = 80;  break;
                case 'h': velocities[track][i] = 100; break;
                case 'a': velocities[track][i] = 120; break;
                default:  velocities[track][i] = 0;   break;
            }
        }
    }

    // For code paths that still read computeDensity().
    void computeDensity()
    {
        // Prefer the tick-aware path when hits[] is populated.
        bool anyHits = false;
        for (int t = 0; t < NUM_TRACKS; ++t)
            if (! hits[t].empty()) { anyHits = true; break; }

        if (anyHits) { recomputeDensity(); return; }

        const int total  = NUM_TRACKS * numSteps;
        int active = 0;
        for (int t = 0; t < NUM_TRACKS; ++t)
            for (int s = 0; s < numSteps; ++s)
                if (velocities[t][s] > 0) ++active;
        density = (total > 0) ? (float) active / (float) total : 0.0f;
    }
};

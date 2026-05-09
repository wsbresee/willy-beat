#pragma once
#include <JuceHeader.h>

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

enum class Genre { Rock = 0, HipHop, Funk, Electronic, Jazz, Latin, NUM_GENRES };
enum class PatType { Regular = 0, Variance, SmallFill, BigFill, NUM_TYPES };

static const char* kGenreNames[]  = { "Rock", "Hip-Hop", "Funk", "Electronic", "Jazz", "Latin" };
static const char* kPatTypeNames[] = { "Regular", "Variance", "Sm. Fill", "Big Fill" };

constexpr int MAX_STEPS = 16;

struct DrumPattern
{
    juce::String name;
    Genre        genre;
    PatType      type;
    int          numSteps = MAX_STEPS;
    bool         isUserPattern = false;

    // velocities[track][step] — 0 means off
    uint8_t velocities[NUM_TRACKS][MAX_STEPS] = {};

    // Decode one pattern row from a string:
    //   '.' = off
    //   'g' = ghost  (vel 25)
    //   's' = soft   (vel 55)
    //   'm' = medium (vel 80)
    //   'h' = hard   (vel 100)
    //   'a' = accent (vel 120)
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
};

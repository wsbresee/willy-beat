#include "PatternLibrary.h"

// Helper: build a DrumPattern from raw string rows
// rows order: kick, snare, hihat_c, hihat_o, ride, crash, tomH, tomM, tomL, rim
static DrumPattern make (const char* name, Genre g, PatType t,
                         const char* kick,    const char* snare,
                         const char* hihatC,  const char* hihatO,
                         const char* ride,    const char* crash,
                         const char* tomH,    const char* tomM,
                         const char* tomL,    const char* rim)
{
    DrumPattern p;
    p.name  = name;
    p.genre = g;
    p.type  = t;
    p.setRow (TR_KICK,    kick);
    p.setRow (TR_SNARE,   snare);
    p.setRow (TR_HIHAT_C, hihatC);
    p.setRow (TR_HIHAT_O, hihatO);
    p.setRow (TR_RIDE,    ride);
    p.setRow (TR_CRASH,   crash);
    p.setRow (TR_TOM_H,   tomH);
    p.setRow (TR_TOM_M,   tomM);
    p.setRow (TR_TOM_L,   tomL);
    p.setRow (TR_RIM,     rim);
    return p;
}

// Shorthand: 16-char "off" row
#define OFF "................"

//==============================================================================

void PatternLibrary::loadBuiltins()
{
    // Step reference (16 steps = 1 bar of 16th notes):
    //   Beat:  1  e  +  a  2  e  +  a  3  e  +  a  4  e  +  a
    //   Step:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15

    using G = Genre;
    using T = PatType;

    // ================================================================
    // ROCK
    // ================================================================

    // Two solid kicks on 1 and 3, snare on 2+4, 8th-note hihats
    patterns.push_back (make ("Rock Basic", G::Rock, T::Regular,
        "a.......a.......",   // kick:    1, 3
        "....a.......a...",   // snare:   2, 4
        "m.m.m.m.m.m.m.m.",  // hihat_c: 8ths
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Kick on 1, and-of-2, 3 — more momentum
    patterns.push_back (make ("Rock Drive", G::Rock, T::Regular,
        "a.....h.a.......",   // kick:    1, and-of-2(6), 3
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Half-time feel: snare lands on beat 3 only
    patterns.push_back (make ("Rock Half-Time", G::Rock, T::Regular,
        "a.m.....a.m.....",   // kick:    1, and-of-1, 3, and-of-3
        "........a.......",   // snare:   beat 3 only
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Open hihat on the "and of 4" for a little air
    patterns.push_back (make ("Rock Open Hat", G::Rock, T::Variance,
        "a.......a.......",
        "....a.......a...",
        "m.m.m.m.m.m.m...",  // hihat_c stops before step 14
        "..............m.",  // hihat_o: and-of-4
        OFF, OFF, OFF, OFF, OFF, OFF));

    // Snare roll into bar + descending toms on beats 3-4
    patterns.push_back (make ("Rock Small Fill", G::Rock, T::SmallFill,
        "a.......a.......",
        "....a.......h.h.",  // snare fill at 12, 14
        "m.m.m.m.........",  // hihat only first half
        OFF, OFF, OFF,
        "........h.......",  // tomH: step 8
        "..........h.....",  // tomM: step 10
        "............h...",  // tomL: step 12
        OFF));

    // Full-bar tom run, crash on 1
    patterns.push_back (make ("Rock Big Fill", G::Rock, T::BigFill,
        "a...............",
        "....h...h.......",
        OFF, OFF, OFF,
        "a...............",  // crash: 1
        ".......mhm......",  // tomH: 7,8,9
        "..........hm....",  // tomM: 10,11
        "............hmhm",  // tomL: 12,13,14,15
        OFF));

    // ================================================================
    // HIP-HOP
    // ================================================================

    // Classic boom-bap: kick on 1, and-of-3; snare on 2+4; 8th hihats
    patterns.push_back (make ("Boom Bap", G::HipHop, T::Regular,
        "a.......a.m.....",   // kick: 1, 3, and-of-3(10)
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Trap-adjacent: sparse kick, half-time snare, 16th hihats
    patterns.push_back (make ("Trap Feel", G::HipHop, T::Regular,
        "a...........a...",   // kick: 1, 4
        "........a.......",   // snare: beat 3 (half-time)
        "mmmmmmmmmmmmmmmm",   // hihat_c: 16th notes
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // More syncopated kick, open hat for feel
    patterns.push_back (make ("Hip-Hop Groove", G::HipHop, T::Variance,
        "a.m.....a.......",   // kick: 1, and-of-1, 3
        "....a.......a...",
        "m.m.m.m.m.m.m...",
        "..............m.",   // hihat_o: and-of-4
        OFF, OFF, OFF, OFF, OFF, OFF));

    // Short snare flam into next bar
    patterns.push_back (make ("Hip-Hop Sm. Fill", G::HipHop, T::SmallFill,
        "a.......a.......",
        "....a.......hhhh",   // snare roll last 4 steps
        "m.m.m.m.........",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Rolling snare fill, crash landing
    patterns.push_back (make ("Hip-Hop Big Fill", G::HipHop, T::BigFill,
        "a...............",
        "....h.h.h.h.h.h.",   // snare fills second half
        OFF, OFF, OFF,
        "a...............",   // crash
        OFF, OFF, OFF, OFF));

    // ================================================================
    // FUNK
    // ================================================================

    // Classic funk: syncopated kick, ghost snare, 16th hihats
    patterns.push_back (make ("Funk Basic", G::Funk, T::Regular,
        "a..m....a.......",   // kick: 1, a-of-1(3), 3
        "g...a.g.g...a.g.",   // snare: ghosts + mains on 2, 4
        "mmmmmmmmmmmmmmmm",   // hihat_c: 16th notes
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // JB-style: kick on 1 + and-of-3, 16ths, heavy ghost game
    patterns.push_back (make ("Funk JB", G::Funk, T::Regular,
        "a.......m.m.....",   // kick: 1, and-of-3(10), e-of-4(9)
        "g...a.gg....a.g.",   // denser ghosts
        "mmmmmmmmmmmmmmmm",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Looser funk with open hihats on the and's
    patterns.push_back (make ("Funk Loose", G::Funk, T::Variance,
        "a...m...a.......",   // kick: 1, e-of-2(4)→ beat2? No: step4=beat2, step5=e-of-2. Let me use step 4.
        "g...a.g.g...a...",
        "m.m.m...m.m.m...",   // hihat_c: avoids 6,10,14
        "......m.....m.m.",   // hihat_o: 6, 10, 14
        OFF, OFF, OFF, OFF, OFF, OFF));

    // Last 4 steps replaced with snare + tom fill
    patterns.push_back (make ("Funk Sm. Fill", G::Funk, T::SmallFill,
        "a..m....a.......",
        "g...a...........",   // ghost on 1, accent on 2, silent during fill
        "mmmmmmmm........",
        OFF, OFF, OFF,
        "........h.h.....",   // tomH
        "..........h.h...",   // tomM
        "............h.h.",   // tomL
        OFF));

    patterns.push_back (make ("Funk Big Fill", G::Funk, T::BigFill,
        "a...............",
        "....h.h.h.......",
        OFF, OFF, OFF,
        "a...............",
        ".......mhm......",
        "..........hm....",
        "............hmhm",
        OFF));

    // ================================================================
    // ELECTRONIC
    // ================================================================

    // Four-on-the-floor: kick every beat, snare on 2+4, 8th hihats
    patterns.push_back (make ("EDM 4-on-Floor", G::Electronic, T::Regular,
        "a...a...a...a...",   // kick: every beat
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Driving techno: 4otf + 16th hihats
    patterns.push_back (make ("Techno Drive", G::Electronic, T::Regular,
        "a...a...a...a...",
        "....a.......a...",
        "mmmmmmmmmmmmmmmm",   // 16th hihats
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    // Open hihats on the and's, quarter-note closed
    patterns.push_back (make ("EDM Open Hat", G::Electronic, T::Variance,
        "a...a...a...a...",
        "....a.......a...",
        "m...m...m...m...",   // hihat_c: quarter notes
        "..m...m...m...m.",   // hihat_o: and's
        OFF, OFF, OFF, OFF, OFF, OFF));

    patterns.push_back (make ("EDM Sm. Fill", G::Electronic, T::SmallFill,
        "a...a...a.......",
        "....a.......h.h.",
        "m.m.m.m.........",
        OFF, OFF,
        "............a...",   // crash on beat 4
        OFF, OFF, OFF, OFF));

    patterns.push_back (make ("EDM Big Fill", G::Electronic, T::BigFill,
        "a...a...........",
        "....a...h.h.h.h.",
        OFF, OFF, OFF,
        "a...............",
        OFF, OFF, OFF, OFF));

    // ================================================================
    // JAZZ
    // ================================================================

    // Classic jazz ride: ding-ding-da-ding on ride, hihat pedal 2+4
    patterns.push_back (make ("Jazz Ride", G::Jazz, T::Regular,
        "s...............",   // feathered kick on beat 1
        "....s.......s...",   // soft snare (brushes) on 2+4
        OFF,
        OFF,
        "m...m.m.m...m.m.",  // ride: 1, 2, and-2, 3, 4, and-4
        OFF, OFF, OFF, OFF,
        "....m.......m..."));  // rim = hihat pedal on 2+4

    // Walking ride + more active kick + ghost snares
    patterns.push_back (make ("Jazz Groove", G::Jazz, T::Regular,
        "s.......m.......",   // kick: 1, 3 (feathered)
        "g...s.g.g...s.g.",   // brush-style ghost snare
        OFF, OFF,
        "m...m.m.m...m.m.",
        OFF, OFF, OFF, OFF,
        "....m.......m..."));

    // More open, brushes feel — ride swings harder
    patterns.push_back (make ("Jazz Brush", G::Jazz, T::Variance,
        "s...............",
        "g.g.s.g.g.g.s.g.",   // very ghost-heavy
        OFF, OFF,
        "m.m.m.m.m.m.m.m.",   // 8th-note ride
        OFF, OFF, OFF, OFF,
        "....m.......m..."));

    // Tom fill with jazz phrasing
    patterns.push_back (make ("Jazz Fill", G::Jazz, T::BigFill,
        "s...............",
        "....h...h.......",
        OFF, OFF,
        "m...m.m.........",   // ride first half
        "a...............",   // crash on 1
        "........h.......",
        "..........h.....",
        "............h.h.",
        OFF));

    // ================================================================
    // LATIN
    // ================================================================

    // Bossa Nova: clave on rim, kick on 1+3, ride 8ths, hihat pedal 2+4
    patterns.push_back (make ("Bossa Nova", G::Latin, T::Regular,
        "m.......m.......",   // kick: 1, 3
        OFF,
        OFF, OFF,
        "m.m.m.m.m.m.m.m.",  // ride: 8th notes
        OFF, OFF, OFF, OFF,
        "m..m..m.....m.m."));  // rim: 3-2 son clave approximation

    // Samba feel: busier kick, 16th hihats, clave rim
    patterns.push_back (make ("Samba", G::Latin, T::Regular,
        "m.m.m.m.m.m.m.m.",   // kick: 8th notes (surdo)
        "..g...g...g...g.",   // ghost snare between kicks
        "mmmmmmmmmmmmmmmm",   // hihat_c: 16th notes
        OFF, OFF, OFF, OFF, OFF, OFF,
        "m..m..m.....m.m."));  // rim: clave

    // Tighter bossa with more snare presence
    patterns.push_back (make ("Bossa Groove", G::Latin, T::Variance,
        "m.......m.......",
        "g...s.g.....s...",   // ghost + soft snare hits
        OFF, OFF,
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF,
        "m..m..m.....m.m."));

    // Latin big fill with timbale-style tom run
    patterns.push_back (make ("Latin Fill", G::Latin, T::BigFill,
        "m...............",
        "....h...h.......",
        OFF, OFF, OFF,
        "a...............",
        ".......mhm......",
        "..........hm....",
        "............hmhm",
        "m..m............"));
}

//==============================================================================

PatternLibrary::PatternLibrary()
{
    loadBuiltins();
}

std::vector<const DrumPattern*> PatternLibrary::get (Genre genre, PatType type) const
{
    std::vector<const DrumPattern*> result;
    for (auto& p : patterns)
        if (p.genre == genre && p.type == type)
            result.push_back (&p);
    return result;
}

void PatternLibrary::addUserPattern (DrumPattern p)
{
    p.isUserPattern = true;
    patterns.push_back (std::move (p));
}

std::unique_ptr<juce::XmlElement> PatternLibrary::toXml() const
{
    auto root = std::make_unique<juce::XmlElement> ("UserPatterns");
    for (auto& p : patterns)
    {
        if (!p.isUserPattern) continue;
        auto* pe = root->createNewChildElement ("Pattern");
        pe->setAttribute ("name",  p.name);
        pe->setAttribute ("genre", (int) p.genre);
        pe->setAttribute ("type",  (int) p.type);
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            juce::String row;
            for (int s = 0; s < MAX_STEPS; ++s)
                row += juce::String (p.velocities[t][s]) + " ";
            pe->setAttribute ("track" + juce::String (t), row.trim());
        }
    }
    return root;
}

void PatternLibrary::fromXml (const juce::XmlElement& xml)
{
    // Remove previously-loaded user patterns
    patterns.erase (std::remove_if (patterns.begin(), patterns.end(),
                                    [] (auto& p) { return p.isUserPattern; }),
                    patterns.end());

    for (auto* pe : xml.getChildIterator())
    {
        DrumPattern p;
        p.name          = pe->getStringAttribute ("name", "User");
        p.genre         = (Genre) pe->getIntAttribute ("genre", 0);
        p.type          = (PatType) pe->getIntAttribute ("type", 0);
        p.isUserPattern = true;
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            juce::StringArray tokens;
            tokens.addTokens (pe->getStringAttribute ("track" + juce::String (t)), false);
            for (int s = 0; s < MAX_STEPS && s < tokens.size(); ++s)
                p.velocities[t][s] = (uint8_t) tokens[s].getIntValue();
        }
        patterns.push_back (p);
    }
}

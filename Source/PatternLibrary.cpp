#include "PatternLibrary.h"

// Build a DrumPattern from 10 character-encoded rows.
// genres is a comma-separated list of tags, e.g. "Hip-Hop, Boom Bap"
// Velocity codes: . off  g ghost(25)  s soft(55)  m medium(80)  h hard(100)  a accent(120)
static DrumPattern make (const char* name, const char* genres, PatType t,
                         const char* kick,   const char* snare,
                         const char* hihatC, const char* hihatO,
                         const char* ride,   const char* crash,
                         const char* tomH,   const char* tomM,
                         const char* tomL,   const char* rim)
{
    DrumPattern p;
    p.name = name;
    p.type = t;

    juce::StringArray tags;
    tags.addTokens (genres, ",", "");
    for (auto& tag : tags)
        if (tag.trim().isNotEmpty())
            p.genres.add (tag.trim());

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
    p.computeDensity();
    return p;
}

#define OFF "................"

//==============================================================================
// Built-in patterns
//==============================================================================

std::vector<DrumPattern> PatternLibrary::builtinPatterns()
{
    std::vector<DrumPattern> result;

    using T = PatType;

    // Beat reference (16 steps = 1 bar of 16th notes):
    //   1  e  +  a  2  e  +  a  3  e  +  a  4  e  +  a
    //   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15

    // ================================================================
    // ROCK
    // ================================================================

    result.push_back (make ("Rock Basic", "Rock", T::Regular,
        "a.......a.......",
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Rock Drive", "Rock", T::Regular,
        "a.....h.a.......",
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Rock Half-Time", "Rock", T::Regular,
        "a.m.....a.m.....",
        "........a.......",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Rock Open Hat", "Rock", T::Variance,
        "a.......a.......",
        "....a.......a...",
        "m.m.m.m.m.m.m...",
        "..............m.",
        OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Rock Small Fill", "Rock", T::SmallFill,
        "a.......a.......",
        "....a.......h.h.",
        "m.m.m.m.........",
        OFF, OFF, OFF,
        "........h.......",
        "..........h.....",
        "............h...",
        OFF));

    result.push_back (make ("Rock Big Fill", "Rock", T::BigFill,
        "a...............",
        "....h...h.......",
        OFF, OFF, OFF,
        "a...............",
        ".......mhm......",
        "..........hm....",
        "............hmhm",
        OFF));

    // ================================================================
    // HIP-HOP
    // ================================================================

    result.push_back (make ("Boom Bap", "Hip-Hop, Boom Bap", T::Regular,
        "a.......a.m.....",
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Trap Feel", "Hip-Hop, Trap", T::Regular,
        "a...........a...",
        "........a.......",
        "mmmmmmmmmmmmmmmm",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Hip-Hop Groove", "Hip-Hop", T::Variance,
        "a.m.....a.......",
        "....a.......a...",
        "m.m.m.m.m.m.m...",
        "..............m.",
        OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Hip-Hop Sm. Fill", "Hip-Hop", T::SmallFill,
        "a.......a.......",
        "....a.......hhhh",
        "m.m.m.m.........",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Hip-Hop Big Fill", "Hip-Hop", T::BigFill,
        "a...............",
        "....h.h.h.h.h.h.",
        OFF, OFF, OFF,
        "a...............",
        OFF, OFF, OFF, OFF));

    // ================================================================
    // FUNK
    // ================================================================

    result.push_back (make ("Funk Basic", "Funk, Soul", T::Regular,
        "a..m....a.......",
        "g...a.g.g...a.g.",
        "mmmmmmmmmmmmmmmm",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Funk JB", "Funk, Soul", T::Regular,
        "a.......m.m.....",
        "g...a.gg....a.g.",
        "mmmmmmmmmmmmmmmm",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Funk Loose", "Funk", T::Variance,
        "a...m...a.......",
        "g...a.g.g...a...",
        "m.m.m...m.m.m...",
        "......m.....m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Funk Sm. Fill", "Funk", T::SmallFill,
        "a..m....a.......",
        "g...a...........",
        "mmmmmmmm........",
        OFF, OFF, OFF,
        "........h.h.....",
        "..........h.h...",
        "............h.h.",
        OFF));

    result.push_back (make ("Funk Big Fill", "Funk", T::BigFill,
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

    result.push_back (make ("EDM 4-on-Floor", "Electronic, EDM, House", T::Regular,
        "a...a...a...a...",
        "....a.......a...",
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("Techno Drive", "Electronic, Techno", T::Regular,
        "a...a...a...a...",
        "....a.......a...",
        "mmmmmmmmmmmmmmmm",
        OFF, OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("EDM Open Hat", "Electronic, EDM", T::Variance,
        "a...a...a...a...",
        "....a.......a...",
        "m...m...m...m...",
        "..m...m...m...m.",
        OFF, OFF, OFF, OFF, OFF, OFF));

    result.push_back (make ("EDM Sm. Fill", "Electronic, EDM", T::SmallFill,
        "a...a...a.......",
        "....a.......h.h.",
        "m.m.m.m.........",
        OFF, OFF,
        "............a...",
        OFF, OFF, OFF, OFF));

    result.push_back (make ("EDM Big Fill", "Electronic", T::BigFill,
        "a...a...........",
        "....a...h.h.h.h.",
        OFF, OFF, OFF,
        "a...............",
        OFF, OFF, OFF, OFF));

    // ================================================================
    // JAZZ
    // ================================================================

    result.push_back (make ("Jazz Ride", "Jazz", T::Regular,
        "s...............",
        "....s.......s...",
        OFF,
        OFF,
        "m...m.m.m...m.m.",
        OFF, OFF, OFF, OFF,
        "....m.......m..."));

    result.push_back (make ("Jazz Groove", "Jazz, Swing", T::Regular,
        "s.......m.......",
        "g...s.g.g...s.g.",
        OFF, OFF,
        "m...m.m.m...m.m.",
        OFF, OFF, OFF, OFF,
        "....m.......m..."));

    result.push_back (make ("Jazz Brush", "Jazz, Swing", T::Variance,
        "s...............",
        "g.g.s.g.g.g.s.g.",
        OFF, OFF,
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF,
        "....m.......m..."));

    result.push_back (make ("Jazz Fill", "Jazz", T::BigFill,
        "s...............",
        "....h...h.......",
        OFF, OFF,
        "m...m.m.........",
        "a...............",
        "........h.......",
        "..........h.....",
        "............h.h.",
        OFF));

    // ================================================================
    // LATIN
    // ================================================================

    result.push_back (make ("Bossa Nova", "Latin, Bossa Nova", T::Regular,
        "m.......m.......",
        OFF,
        OFF, OFF,
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF,
        "m..m..m.....m.m."));

    result.push_back (make ("Samba", "Latin, Samba", T::Regular,
        "m.m.m.m.m.m.m.m.",
        "..g...g...g...g.",
        "mmmmmmmmmmmmmmmm",
        OFF, OFF, OFF, OFF, OFF, OFF,
        "m..m..m.....m.m."));

    result.push_back (make ("Bossa Groove", "Latin, Bossa Nova", T::Variance,
        "m.......m.......",
        "g...s.g.....s...",
        OFF, OFF,
        "m.m.m.m.m.m.m.m.",
        OFF, OFF, OFF, OFF,
        "m..m..m.....m.m."));

    result.push_back (make ("Latin Fill", "Latin", T::BigFill,
        "m...............",
        "....h...h.......",
        OFF, OFF, OFF,
        "a...............",
        ".......mhm......",
        "..........hm....",
        "............hmhm",
        "m..m............"));

    return result;
}

//==============================================================================
// File I/O helpers
//==============================================================================

int PatternLibrary::trackFromKey (const juce::String& key)
{
    auto k = key.trim().toLowerCase();
    if (k == "kick")                              return TR_KICK;
    if (k == "snare")                             return TR_SNARE;
    if (k == "hihat_c" || k == "hihatc")          return TR_HIHAT_C;
    if (k == "hihat_o" || k == "hihato")          return TR_HIHAT_O;
    if (k == "ride")                              return TR_RIDE;
    if (k == "crash")                             return TR_CRASH;
    if (k == "tom_h"   || k == "tomh")            return TR_TOM_H;
    if (k == "tom_m"   || k == "tomm")            return TR_TOM_M;
    if (k == "tom_l"   || k == "toml")            return TR_TOM_L;
    if (k == "rim")                               return TR_RIM;
    return -1;
}

PatType PatternLibrary::typeFromString (const juce::String& s)
{
    auto lower = s.trim().toLowerCase().removeCharacters (" -_.");
    if (lower == "regular")                        return PatType::Regular;
    if (lower == "variance")                       return PatType::Variance;
    if (lower == "smfill" || lower == "smallfill") return PatType::SmallFill;
    if (lower == "bigfill")                        return PatType::BigFill;
    return PatType::Regular;
}

juce::File PatternLibrary::patternFile (const juce::File& dir, const DrumPattern& p)
{
    auto safe = p.name.replaceCharacters ("/\\:*?\"<>|", "_________");
    return dir.getChildFile (safe + ".beat");
}

bool PatternLibrary::patternToFile (const DrumPattern& p, const juce::File& f)
{
    juce::String text;
    text << "# WillyBeat Preset\n";
    text << "name:    " << p.name << "\n";
    text << "genres:  " << p.genres.joinIntoString (", ") << "\n";
    text << "type:    " << kPatTypeNames[(int) p.type] << "\n";
    text << "density: " << juce::String (p.density, 4) << "\n";
    text << "\n";
    text << "# velocity codes:  . off   g ghost(25)   s soft(55)   m medium(80)   h hard(100)   a accent(120)\n";

    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        juce::String row;
        for (int s = 0; s < MAX_STEPS; ++s)
        {
            uint8_t v = p.velocities[t][s];
            if      (v == 0)    row += '.';
            else if (v <= 30)   row += 'g';
            else if (v <= 65)   row += 's';
            else if (v <= 90)   row += 'm';
            else if (v <= 110)  row += 'h';
            else                row += 'a';
        }
        juce::String key (kTrackFileKeys[t]);
        while (key.length() < 8) key += ' ';
        text << key << row << "\n";
    }

    return f.replaceWithText (text);
}

DrumPattern PatternLibrary::patternFromFile (const juce::File& f)
{
    DrumPattern p;
    p.sourceFile = f;

    juce::StringArray lines;
    lines.addLines (f.loadFileAsString());

    for (const auto& rawLine : lines)
    {
        auto line = rawLine.trim();
        if (line.startsWithChar ('#') || line.isEmpty())
            continue;

        if (line.containsChar (':'))
        {
            auto key = line.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
            auto val = line.fromFirstOccurrenceOf (":", false, false).trim();

            if (key == "name")
            {
                p.name = val;
            }
            else if (key == "genres")
            {
                p.genres.clear();
                juce::StringArray tags;
                tags.addTokens (val, ",", "");
                for (auto& tag : tags)
                    if (tag.trim().isNotEmpty())
                        p.genres.add (tag.trim());
            }
            else if (key == "genre")   // legacy single-genre key
            {
                if (p.genres.isEmpty() && val.trim().isNotEmpty())
                    p.genres.add (val.trim());
            }
            else if (key == "type")
            {
                p.type = typeFromString (val);
            }
            else if (key == "density")
            {
                p.density = val.getFloatValue();
            }
        }
        else
        {
            // "trackkey  pattern..."
            int spaceIdx = line.indexOfAnyOf (" \t");
            if (spaceIdx < 1) continue;

            auto trackKey   = line.substring (0, spaceIdx).trim();
            auto patternStr = line.substring (spaceIdx).trim();

            int track = trackFromKey (trackKey);
            if (track >= 0 && patternStr.isNotEmpty())
                p.setRow (track, patternStr.toRawUTF8());
        }
    }

    if (p.name.isEmpty())
        p.name = f.getFileNameWithoutExtension();

    // Recompute density from actual velocities (overrides stored value)
    p.computeDensity();
    return p;
}

//==============================================================================
// Public API
//==============================================================================

void PatternLibrary::installDefaultsTo (const juce::File& dir)
{
    dir.createDirectory();
    for (const auto& p : builtinPatterns())
        patternToFile (p, patternFile (dir, p));
}

void PatternLibrary::loadFromDirectory (const juce::File& dir)
{
    patterns.clear();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.beat");
    files.sort();
    for (const auto& f : files)
    {
        auto p = patternFromFile (f);
        if (p.name.isNotEmpty())
            patterns.push_back (std::move (p));
    }
}

juce::File PatternLibrary::savePattern (const DrumPattern& p, const juce::File& dir)
{
    dir.createDirectory();
    auto f = patternFile (dir, p);
    patternToFile (p, f);
    return f;
}

DrumPattern PatternLibrary::loadFromFile (const juce::File& f)
{
    return patternFromFile (f);
}

void PatternLibrary::updatePattern (const DrumPattern& p)
{
    for (auto& stored : patterns)
    {
        if (stored.sourceFile == p.sourceFile && p.sourceFile.existsAsFile())
        {
            stored = p;
            return;
        }
    }
}

juce::StringArray PatternLibrary::getGenres() const
{
    juce::StringArray result;
    for (const auto& p : patterns)
        for (const auto& tag : p.genres)
            if (!result.contains (tag))
                result.add (tag);
    result.sort (true);
    return result;
}

std::vector<const DrumPattern*> PatternLibrary::getByTag (const juce::String& tag,
                                                           bool fuzzy) const
{
    std::vector<const DrumPattern*> result;
    if (tag.isEmpty())
    {
        for (const auto& p : patterns)
            result.push_back (&p);
        return result;
    }

    auto tagLow = tag.toLowerCase();
    for (const auto& p : patterns)
    {
        for (const auto& g : p.genres)
        {
            auto gLow  = g.toLowerCase();
            bool match = fuzzy ? (gLow.contains (tagLow) || tagLow.contains (gLow))
                               : (gLow == tagLow);
            if (match) { result.push_back (&p); break; }
        }
    }
    return result;
}

std::vector<const DrumPattern*> PatternLibrary::getByTags (const juce::StringArray& tags) const
{
    std::vector<const DrumPattern*> result;
    if (tags.isEmpty())
    {
        for (const auto& p : patterns)
            result.push_back (&p);
        return result;
    }

    for (const auto& p : patterns)
    {
        bool match = false;
        for (const auto& wanted : tags)
        {
            for (const auto& got : p.genres)
                if (got.equalsIgnoreCase (wanted)) { match = true; break; }
            if (match) break;
        }
        if (match) result.push_back (&p);
    }
    return result;
}

DrumPattern PatternLibrary::makeComposite (const juce::StringArray& tags,
                                           PatType type,
                                           juce::int64 seed) const
{
    auto matches = getByTags (tags);

    // Filter by type — fall back to all matches, then to the whole library.
    std::vector<const DrumPattern*> typed;
    for (auto* p : matches)
        if (p->type == type) typed.push_back (p);
    if (typed.empty()) typed = matches;
    if (typed.empty())
        for (const auto& p : patterns)
            if (p.type == type) typed.push_back (&p);
    if (typed.empty()) return DrumPattern{};

    juce::Random rng (seed);
    DrumPattern result = *typed[0];

    // Independently pick a random source pattern for each track row.  With
    // many sources this produces an effectively unique mix per seed.
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        auto* src = typed[(size_t) rng.nextInt ((int) typed.size())];
        for (int s = 0; s < MAX_STEPS; ++s)
            result.velocities[t][s] = src->velocities[t][s];
    }

    result.genres = tags;
    if (result.genres.isEmpty()) result.genres.add ("Generated");
    result.type = type;
    // Caller sets a unique result.name before saving.
    result.name = "Generated";
    result.sourceFile = juce::File();   // force a new file rather than overwrite
    result.computeDensity();
    return result;
}

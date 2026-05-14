#include "PatternLibrary.h"
#include "BinaryData.h"

namespace
{
    // ─── Curated genre clusters ──────────────────────────────────────────
    // Tags appearing in the same cluster are treated as semantically similar.
    // Clusters can overlap (e.g. "Hard Rock" lives in both rock + metal) so
    // the relation is transitive enough to bridge nearby families.  Add tags
    // here to teach the matcher new associations.
    const std::vector<juce::StringArray> kGenreClusters = {
        // Rock / heavy
        { "Rock", "Classic Rock", "Hard Rock", "Soft Rock", "Indie Rock",
          "Alt Rock", "Alternative", "Garage Rock", "Punk", "Punk Rock",
          "Pop Punk", "Post-Punk", "Grunge", "Prog Rock", "Heartland Rock",
          "Arena Rock", "Folk Rock", "Yacht Rock", "Math Rock", "Emo",
          "Emo Revival", "Heavy Metal", "Hair Metal", "NWOBHM" },
        // Metal subgenres
        { "Metal", "Heavy Metal", "Hard Rock", "Death Metal", "Thrash",
          "Thrash Metal", "Black Metal", "Doom Metal", "Power Metal",
          "Groove Metal", "Nu-Metal", "Alt Metal", "Prog Metal", "NWOBHM",
          "Emo", "Punk Rap" },
        // Hip-Hop / rap
        { "Hip-Hop", "Rap", "Boom Bap", "Trap", "Drill", "UK Drill",
          "Brooklyn Drill", "G-Funk", "West Coast", "East Coast",
          "Conscious Hip-Hop", "Cloud Rap", "Mumble Rap", "Emo Rap",
          "Punk Rap", "Phonk", "Pop Rap", "Crunk", "Lo-fi", "Chillhop",
          "Gangsta Rap", "Country Rap", "Hip-Hop Sample" },
        // House family
        { "House", "Tech House", "Deep House", "Acid House", "Italo House",
          "French House", "French Touch", "Progressive House",
          "Tropical House", "90s House", "Hi-NRG", "Disco" },
        // Techno
        { "Techno", "Detroit Techno", "Acid Techno", "Industrial Techno",
          "Berlin Minimal", "Bleep", "Industrial" },
        // Trance / hardcore-adjacent
        { "Trance", "Progressive Trance", "Hardstyle", "Hardcore",
          "Speedcore", "Dream Trance", "Big Room", "EDM" },
        // Drum and Bass / Jungle
        { "DnB", "Drum and Bass", "Liquid DnB", "Atmospheric DnB", "Jungle",
          "Drill n Bass", "Drumstep", "Neurofunk", "Atmospheric" },
        // UK Garage / Bass / 2-Step
        { "UK Garage", "2-Step", "Future Garage", "Bassline", "Dubstep" },
        // Dubstep / wonky
        { "Dubstep", "Brostep", "Wonky", "Future Garage", "Bassline",
          "Glitch Hop" },
        // Big Beat / Breaks
        { "Big Beat", "Breakbeat", "Breaks" },
        // IDM / experimental
        { "IDM", "Glitch", "Glitch Hop", "Ambient", "Ambient Techno",
          "Drone", "Industrial", "Experimental", "Avant Pop" },
        // Footwork / Club
        { "Footwork", "Juke", "Jersey Club", "Baltimore Club" },
        // Synthwave family
        { "Synthwave", "Outrun", "Darksynth", "Vaporwave", "Future Funk",
          "Future Bass", "80s", "New Wave" },
        // Disco / Italo / Hi-NRG
        { "Disco", "Nu-Disco", "Italo Disco", "Eurodisco", "Hi-NRG",
          "French Touch", "French House", "Funk" },
        // Soul / Funk / R&B
        { "Soul", "R&B", "Funk", "P-Funk", "Neo-Soul", "Motown", "Disco",
          "Blue-Eyed Soul", "Soul Jazz", "Soul Blues", "New Orleans" },
        // Jazz family
        { "Jazz", "Cool Jazz", "Bebop", "Swing", "Big Band", "Fusion",
          "Acid Jazz", "Spiritual Jazz", "Soul Jazz", "Modal Jazz",
          "Latin Jazz", "Vocal Jazz" },
        // Pop family
        { "Pop", "Dance Pop", "Synth-Pop", "Electropop", "Indie Pop",
          "Hyperpop", "Bedroom Pop", "Dream Pop", "Pop Rock", "Pop Rap",
          "Boy Band", "Bubblegum Pop", "Art Pop", "Avant Pop",
          "Tropical House", "Maximalist Pop", "Alt Pop" },
        // Shoegaze / dream / slowcore
        { "Shoegaze", "Dream Pop", "Slowcore", "Ethereal Wave",
          "Bedroom Pop", "Indie" },
        // Latin
        { "Latin", "Latin Rock", "Salsa", "Reggaeton", "Bossa Nova",
          "Samba", "Cumbia", "Bachata", "Mambo", "Brazilian", "Latin Jazz",
          "Bolivian Folk" },
        // Afro / world
        { "Afrobeat", "Afrobeats", "Juju" },
        // Reggae / dub
        { "Reggae", "Roots Reggae", "Dub", "Dancehall", "Ska" },
        // Country / folk / Americana
        { "Country", "Country Rap", "Folk", "Folk Rock", "Bluegrass",
          "Americana", "Heartland Rock" },
        // Blues
        { "Blues", "Delta Blues", "Chicago Blues", "Soul Blues",
          "Blues Rock", "Hard Rock" },
        // Trip-hop / downtempo
        { "Trip-Hop", "Downtempo", "Instrumental Hip-Hop" },
        // Indie / alternative crossover
        { "Indie", "Indie Rock", "Indie Pop", "Indietronica", "Dance Punk",
          "Alternative", "Alt Rock", "Alt Pop", "Alt Metal" },
        // Vocal / gospel
        { "Gospel", "Soul" },
    };

    static juce::StringArray tokenize (const juce::String& s)
    {
        juce::StringArray tokens;
        tokens.addTokens (s, " -_/&", "");
        tokens.removeEmptyStrings();
        return tokens;
    }
}

bool PatternLibrary::tagsAreSimilar (const juce::String& a, const juce::String& b)
{
    if (a.equalsIgnoreCase (b)) return true;

    auto aLow = a.toLowerCase().trim();
    auto bLow = b.toLowerCase().trim();
    if (aLow.isEmpty() || bLow.isEmpty()) return false;

    // Substring either way (catches "Rock" / "Hard Rock" / "Punk Rock").
    if (aLow.contains (bLow) || bLow.contains (aLow)) return true;

    // Shared meaningful token (catches "Rock and Roll" / "Rock", but skips
    // tiny linkers like "and" / "of").
    auto aTokens = tokenize (aLow);
    auto bTokens = tokenize (bLow);
    for (const auto& at : aTokens)
        for (const auto& bt : bTokens)
            if (at == bt && at.length() > 2)
                return true;

    // Curated cluster co-membership (catches "Rock" / "Metal", "Trap" / "Drill").
    for (const auto& cluster : kGenreClusters)
    {
        bool aIn = false, bIn = false;
        for (const auto& c : cluster)
        {
            if (! aIn && c.equalsIgnoreCase (a)) aIn = true;
            if (! bIn && c.equalsIgnoreCase (b)) bIn = true;
            if (aIn && bIn) return true;
        }
    }

    return false;
}

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
    p.syncHitsFromLegacy();
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
    text << "# Drumwright Preset v2\n";
    text << "version: 2\n";
    text << "name:    " << p.name << "\n";
    text << "genres:  " << p.genres.joinIntoString (", ") << "\n";
    text << "type:    " << kPatTypeNames[(int) p.type] << "\n";
    text << "timesig: " << p.timeSigNum << "/" << p.timeSigDen << "\n";
    text << "bars:    " << p.bars << "\n";
    text << "grid:    " << kGridSubFileKeys[(int) p.gridSub] << "\n";
    text << "density: " << juce::String (p.density, 4) << "\n";
    if (p.isComposite)
        text << "composite: true\n";
    text << "\n";
    text << "# Hits per track as \"tick=velocity\" pairs. PPQN = 96.\n";

    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        juce::String key (kTrackFileKeys[t]);
        while (key.length() < 8) key += ' ';
        text << key;
        if (! p.hits[t].empty())
        {
            text << " ";
            for (size_t i = 0; i < p.hits[t].size(); ++i)
            {
                if (i > 0) text << ", ";
                text << p.hits[t][i].tick << "=" << (int) p.hits[t][i].velocity;
            }
        }
        text << "\n";
    }

    return f.replaceWithText (text);
}

DrumPattern PatternLibrary::patternFromFile (const juce::File& f)
{
    DrumPattern p;
    p.sourceFile = f;

    juce::StringArray lines;
    lines.addLines (f.loadFileAsString());

    bool sawV2Marker = false;

    for (const auto& rawLine : lines)
    {
        auto line = rawLine.trim();
        if (line.startsWithChar ('#') || line.isEmpty())
            continue;

        if (line.containsChar (':'))
        {
            auto key = line.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
            auto val = line.fromFirstOccurrenceOf (":", false, false).trim();

            if (key == "version")
            {
                sawV2Marker = (val.getIntValue() >= 2);
            }
            else if (key == "name")
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
            else if (key == "genre")
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
            else if (key == "composite")
            {
                auto v = val.toLowerCase();
                p.isComposite = (v == "true" || v == "1" || v == "yes");
            }
            else if (key == "timesig")
            {
                const int slash = val.indexOfChar ('/');
                if (slash > 0)
                {
                    p.timeSigNum = val.substring (0, slash).getIntValue();
                    p.timeSigDen = val.substring (slash + 1).getIntValue();
                }
            }
            else if (key == "bars")
            {
                p.bars = juce::jmax (1, val.getIntValue());
            }
            else if (key == "grid")
            {
                const auto v = val.trim();
                for (int i = 0; i < (int) GridSub::NUM_GRID_SUBS; ++i)
                    if (v == kGridSubFileKeys[i])
                    { p.gridSub = (GridSub) i; break; }
            }
            else
            {
                // Could be a v2 track line "kick: 0=120, 192=80" — fall
                // through to the track-line handling below.
                const int trackId = trackFromKey (key);
                if (trackId >= 0)
                {
                    juce::StringArray pairs;
                    pairs.addTokens (val, ",", "");
                    for (auto& pair : pairs)
                    {
                        auto t = pair.trim();
                        const int eq = t.indexOfChar ('=');
                        if (eq <= 0) continue;
                        const int tick = t.substring (0, eq).getIntValue();
                        const int vel  = t.substring (eq + 1).getIntValue();
                        if (vel > 0 && vel <= 127)
                            p.hits[trackId].push_back ({ tick, (uint8_t) vel });
                    }
                }
            }
        }
        else
        {
            // Space-separated track line. v2 form: "kick  0=100, 96=80, ..."
            //                          v1 form: "kick  h.......h.......".
            int spaceIdx = line.indexOfAnyOf (" \t");
            if (spaceIdx < 1) continue;

            auto trackKey  = line.substring (0, spaceIdx).trim();
            auto remainder = line.substring (spaceIdx).trim();

            const int trackId = trackFromKey (trackKey);
            if (trackId < 0 || remainder.isEmpty()) continue;

            if (remainder.containsChar ('='))
            {
                // v2: tick=velocity pairs
                juce::StringArray pairs;
                pairs.addTokens (remainder, ",", "");
                for (auto& pair : pairs)
                {
                    auto t = pair.trim();
                    const int eq = t.indexOfChar ('=');
                    if (eq <= 0) continue;
                    const int tick = t.substring (0, eq).getIntValue();
                    const int vel  = t.substring (eq + 1).getIntValue();
                    if (vel > 0 && vel <= 127)
                        p.hits[trackId].push_back ({ tick, (uint8_t) vel });
                }
            }
            else
            {
                // v1: letter-coded grid row
                p.setRow (trackId, remainder.toRawUTF8());
            }
        }
    }

    if (p.name.isEmpty())
        p.name = f.getFileNameWithoutExtension();

    // For v1 files (no version marker, hits[] empty), promote the legacy
    // 16-step grid into the tick representation.
    if (! sawV2Marker)
    {
        bool anyHits = false;
        for (int t = 0; t < NUM_TRACKS && ! anyHits; ++t)
            anyHits = ! p.hits[t].empty();
        if (! anyHits) p.syncHitsFromLegacy();
    }

    // Always keep the legacy shim in sync so existing 16-step consumers
    // keep working through the transition.
    p.syncLegacyFromHits();
    p.computeDensity();
    return p;
}

//==============================================================================
// Public API
//==============================================================================

void PatternLibrary::installDefaultsTo (const juce::File& dir)
{
    dir.createDirectory();

   #if JUCE_INCLUDED_PRESET_BUNDLE
    // Unpack the embedded starter bank (generated by Tools/pack_presets.py
    // at build time). Format: magic(4) version(4) count(4) pad(4) then for
    // each entry: name_len(4) name(UTF-8) data_len(4) data(UTF-8 .beat text).
    const uint8_t* data = reinterpret_cast<const uint8_t*> (BinaryData::PresetBundle_bin);
    const int      size = BinaryData::PresetBundle_binSize;

    auto readU32 = [&] (int off) -> uint32_t
    {
        if (off + 4 > size) return 0;
        uint32_t v = 0;
        std::memcpy (&v, data + off, 4);
        return v;
    };

    if (readU32 (0) == 0x50425442u && readU32 (4) == 1)
    {
        const uint32_t count = readU32 (8);
        int off = 16;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (off + 4 > size) break;
            const uint32_t nameLen = readU32 (off);  off += 4;
            if (off + (int) nameLen + 4 > size) break;
            juce::String name (reinterpret_cast<const char*> (data + off), (size_t) nameLen);
            off += (int) nameLen;
            const uint32_t dataLen = readU32 (off);  off += 4;
            if (off + (int) dataLen > size) break;
            juce::String text (reinterpret_cast<const char*> (data + off), (size_t) dataLen);
            off += (int) dataLen;

            auto safe = name.replaceCharacters ("/\\:*?\"<>|", "_________");
            auto f = dir.getChildFile (safe + ".beat");
            if (! f.existsAsFile())
                f.replaceWithText (text);
        }
        return;
    }
   #endif

    // Fallback: write patterns from the hardcoded C++ list.
    for (const auto& p : builtinPatterns())
        patternToFile (p, patternFile (dir, p));
}

void PatternLibrary::loadFromDirectory (const juce::File& dir)
{
    patterns.clear();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.beat");

    // Sort by last-modification time, oldest first. This makes the patIdx
    // slider a slot-history timeline: built-in presets land at the front,
    // user-edited and freshly generated patterns gather at the end. Going
    // back one slot reliably returns to the previous generation.
    std::sort (files.begin(), files.end(),
               [] (const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() < b.getLastModificationTime(); });

    for (const auto& f : files)
    {
        auto p = patternFromFile (f);
        if (p.name.isNotEmpty())
            patterns.push_back (std::move (p));
    }

    // Pin the blank pattern (zero hits across all tracks) to slot 0 so the
    // plugin reliably opens to an empty grid regardless of file name or
    // mtime. Identified by content rather than name so a rename pass
    // (e.g. v1→v2 migration) doesn't break it.
    auto isBlank = [] (const DrumPattern& p)
    {
        for (int t = 0; t < NUM_TRACKS; ++t)
            if (! p.hits[t].empty()) return false;
        return true;
    };
    auto blank = std::find_if (patterns.begin(), patterns.end(), isBlank);
    if (blank != patterns.end() && blank != patterns.begin())
        std::rotate (patterns.begin(), blank, blank + 1);
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

void PatternLibrary::renamePattern (const juce::File& oldFile,
                                    const juce::String& newName,
                                    const juce::File& newFile)
{
    for (auto& stored : patterns)
    {
        if (stored.sourceFile == oldFile)
        {
            stored.name       = newName;
            stored.sourceFile = newFile;
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
            bool match = fuzzy ? tagsAreSimilar (g, tag)
                               : g.toLowerCase() == tagLow;
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

    // A pattern matches if any of its tags is semantically similar to any
    // of the user's selected tags (substring + token + cluster expansion).
    for (const auto& p : patterns)
    {
        bool match = false;
        for (const auto& wanted : tags)
        {
            for (const auto& got : p.genres)
                if (tagsAreSimilar (wanted, got)) { match = true; break; }
            if (match) break;
        }
        if (match) result.push_back (&p);
    }
    return result;
}

// ─── Composite modifier helpers ──────────────────────────────────────────────

struct CompositeModifiers
{
    float velScale    = 1.0f;   // >1 = louder, <1 = quieter
    float densityBias = 0.0f;   // >0 = prefer denser, <0 = prefer sparser  (-1..+1)
};

static CompositeModifiers parseModifiers (const juce::StringArray& tags,
                                          juce::StringArray& cleanTags)
{
    // Each entry: {velScaleMultiplier, densityBiasDelta}.
    // Words in this table are stripped from the tag list so they don't pollute
    // semantic tag matching; their effects are applied to the final pattern.
    static const std::map<juce::String, std::pair<float, float>> kMods
    {
        { "loud",     {1.35f,  0.00f} },
        { "louder",   {1.40f,  0.00f} },
        { "hard",     {1.25f,  0.20f} },
        { "heavy",    {1.25f,  0.30f} },
        { "punchy",   {1.20f,  0.10f} },
        { "big",      {1.30f,  0.10f} },
        { "powerful", {1.30f,  0.10f} },
        { "soft",     {0.65f,  0.00f} },
        { "quiet",    {0.60f,  0.00f} },
        { "quieter",  {0.55f,  0.00f} },
        { "gentle",   {0.55f, -0.20f} },
        { "subtle",   {0.60f, -0.20f} },
        { "light",    {0.70f, -0.20f} },
        { "crazy",    {1.20f,  1.00f} },
        { "wild",     {1.20f,  1.00f} },
        { "insane",   {1.30f,  1.00f} },
        { "intense",  {1.20f,  0.80f} },
        { "dense",    {1.00f,  1.00f} },
        { "full",     {1.00f,  0.80f} },
        { "busy",     {1.00f,  0.80f} },
        { "complex",  {1.00f,  0.70f} },
        { "thick",    {1.10f,  0.70f} },
        { "thin",     {0.90f, -1.00f} },
        { "sparse",   {1.00f, -1.00f} },
        { "minimal",  {1.00f, -1.00f} },
        { "simple",   {1.00f, -0.80f} },
        { "basic",    {1.00f, -0.70f} },
        { "open",     {1.00f, -0.70f} },
        { "stripped", {0.90f, -0.90f} },
        { "bare",     {0.90f, -0.90f} },
    };

    CompositeModifiers mods;
    cleanTags.clear();

    for (const auto& tag : tags)
    {
        juce::StringArray words;
        words.addTokens (tag, " \t", "");
        juce::StringArray kept;

        for (const auto& w : words)
        {
            const juce::String lw = w.toLowerCase().trim();
            if (lw.isEmpty()) continue;
            auto it = kMods.find (lw);
            if (it != kMods.end())
            {
                mods.velScale    = juce::jlimit (0.3f, 2.5f,  mods.velScale    * it->second.first);
                mods.densityBias = juce::jlimit (-1.0f, 1.0f, mods.densityBias + it->second.second);
            }
            else
            {
                kept.add (w);
            }
        }

        if (! kept.isEmpty())
            cleanTags.add (kept.joinIntoString (" "));
    }

    if (cleanTags.isEmpty())   // pure-modifier query — keep originals for semantic search
        cleanTags = tags;

    return mods;
}

// ─────────────────────────────────────────────────────────────────────────────

DrumPattern PatternLibrary::makeComposite (const juce::StringArray& tags,
                                           PatType type,
                                           juce::int64 seed) const
{
    constexpr int kMinSources = 5;

    // Strip modifier words (loud, quiet, dense, sparse …) from the tags before
    // semantic matching. The modifiers are applied to the final pattern instead.
    juce::StringArray cleanTags;
    const CompositeModifiers mods = parseModifiers (tags, cleanTags);

    auto isUsable = [&] (const DrumPattern& p)
    {
        // Skip prior generated/composite patterns to avoid feedback-loop
        // pollution where new generations are sourced from old ones.
        return ! p.isComposite;
    };

    auto contains = [] (const std::vector<const DrumPattern*>& v, const DrumPattern* p)
    {
        return std::find (v.begin(), v.end(), p) != v.end();
    };

    std::vector<const DrumPattern*> sources;

    // Pass 1: tag-matched, same type, non-composite.
    auto matches = getByTags (cleanTags);
    for (auto* p : matches)
        if (isUsable (*p) && p->type == type)
            sources.push_back (p);

    // Pass 2: tag-matched, any type, non-composite (relaxes type filter).
    if ((int) sources.size() < kMinSources)
        for (auto* p : matches)
            if (isUsable (*p) && ! contains (sources, p))
                sources.push_back (p);

    // Pass 3: fuzzy tag matches (substring, case-insensitive).
    if ((int) sources.size() < kMinSources && ! cleanTags.isEmpty())
        for (const auto& tag : cleanTags)
        {
            auto fuzzy = getByTag (tag, /*fuzzy=*/true);
            for (auto* p : fuzzy)
                if (isUsable (*p) && p->type == type && ! contains (sources, p))
                    sources.push_back (p);
        }

    // Pass 4: any non-composite pattern of the right type.
    if ((int) sources.size() < kMinSources)
        for (const auto& p : patterns)
            if (isUsable (p) && p.type == type && ! contains (sources, &p))
                sources.push_back (&p);

    // Pass 5: any non-composite pattern (last resort).
    if ((int) sources.size() < kMinSources)
        for (const auto& p : patterns)
            if (isUsable (p) && ! contains (sources, &p))
                sources.push_back (&p);

    if (sources.empty()) return DrumPattern{};

    juce::Random rng (seed);

    // ── Density-bias sorting ───────────────────────────────────────────────
    // When the query includes density modifiers (dense/sparse/crazy/minimal…),
    // sort the whole source list so denser or sparser patterns float to the top.
    // We do this before shape selection so the most-popular shape is computed
    // over the biased ordering, and compat[0] (the anchor) is already a good fit.
    if (std::abs (mods.densityBias) > 0.15f)
    {
        std::stable_sort (sources.begin(), sources.end(),
            [&] (const DrumPattern* a, const DrumPattern* b)
            {
                return mods.densityBias > 0.0f
                    ? a->density > b->density   // dense: high first
                    : a->density < b->density;  // sparse: low first
            });
    }

    // ── Shape selection: prefer the shape backed by the most sources ──────
    // Avoids cases where sources[0] is a 2-bar outlier that leaves compat
    // with only 1 pattern, producing identical output on every generate.
    using ShapeKey = std::tuple<int, int, int>;  // {tsNum, tsDen, bars}
    std::map<ShapeKey, int> shapeCounts;
    for (auto* p : sources)
        shapeCounts[{ p->timeSigNum, p->timeSigDen, p->bars }]++;

    auto bestShapeIt = std::max_element (shapeCounts.begin(), shapeCounts.end(),
        [] (const auto& a, const auto& b) { return a.second < b.second; });

    DrumPattern result;
    result.timeSigNum = std::get<0> (bestShapeIt->first);
    result.timeSigDen = std::get<1> (bestShapeIt->first);
    result.bars       = std::get<2> (bestShapeIt->first);
    result.gridSub    = GridSub::Sixteenth;
    for (auto* p : sources)
        if (p->timeSigNum == result.timeSigNum && p->timeSigDen == result.timeSigDen
            && p->bars == result.bars)
        { result.gridSub = p->gridSub; break; }

    // ── Compat pool: sources matching the chosen shape ─────────────────────
    std::vector<const DrumPattern*> compat;
    compat.reserve (sources.size());
    for (auto* p : sources)
        if (p->timeSigNum == result.timeSigNum && p->timeSigDen == result.timeSigDen
            && p->bars == result.bars)
            compat.push_back (p);
    if (compat.empty()) compat.push_back (sources[0]);

    // ── Expand compat pool when it's too small ─────────────────────────────
    // Pull in any additional non-composite pattern with the same shape so
    // there's always enough variety to draw from.
    constexpr int kMinCompat = 3;
    if ((int) compat.size() < kMinCompat)
    {
        for (const auto& p : patterns)
            if (isUsable (p) && ! contains (compat, &p)
                && p.timeSigNum == result.timeSigNum
                && p.timeSigDen == result.timeSigDen
                && p.bars       == result.bars)
            {
                compat.push_back (&p);
                if ((int) compat.size() >= kMinCompat * 3) break;
            }
    }

    // ── Canonical (full-density) cache ────────────────────────────────────
    // In-memory copies may hold density-filtered hits; reading from disk
    // ensures composites always draw from full-velocity source patterns.
    std::map<const DrumPattern*, DrumPattern> canonical;
    for (auto* p : compat)
        if (p->sourceFile.existsAsFile())
            canonical.emplace (p, PatternLibrary::loadFromFile (p->sourceFile));

    auto getHits = [&] (const DrumPattern* src, int t) -> const std::vector<DrumHit>&
    {
        auto it = canonical.find (src);
        return (it != canonical.end()) ? it->second.hits[t] : src->hits[t];
    };

    // ── Per-track mixing with guaranteed variety ───────────────────────────
    // compat[0] = anchor (best semantic match). Randomly pick a source per
    // track, but guarantee at least kMinVariantTracks come from a non-anchor
    // source so consecutive generates always sound distinctly different.
    constexpr int kMinVariantTracks = 3;

    if ((int) compat.size() >= 2)
    {
        // Shuffle track indices so forced variants are distributed randomly.
        std::vector<int> trackOrder (NUM_TRACKS);
        for (int i = 0; i < NUM_TRACKS; ++i) trackOrder[i] = i;
        for (int i = NUM_TRACKS - 1; i > 0; --i)
        {
            int j = rng.nextInt (i + 1);
            std::swap (trackOrder[i], trackOrder[j]);
        }

        std::vector<const DrumPattern*> trackSrc (NUM_TRACKS, compat[0]);
        int variantsAssigned = 0;

        for (int idx = 0; idx < NUM_TRACKS; ++idx)
        {
            int t             = trackOrder[idx];
            int remaining     = NUM_TRACKS - idx;
            int variantsLeft  = kMinVariantTracks - variantsAssigned;
            bool forceVariant = variantsLeft >= remaining;

            if (forceVariant || rng.nextFloat() < 0.5f)
            {
                int srcIdx  = 1 + rng.nextInt ((int) compat.size() - 1);
                trackSrc[t] = compat[srcIdx];
                ++variantsAssigned;
            }
        }

        for (int t = 0; t < NUM_TRACKS; ++t)
            result.hits[t] = getHits (trackSrc[t], t);
    }
    else
    {
        for (int t = 0; t < NUM_TRACKS; ++t)
            result.hits[t] = getHits (compat[0], t);
    }

    // ── Velocity scale + jitter ──────────────────────────────────────────────
    // Apply the loudness modifier (if any), then add ±12 jitter so consecutive
    // generates from a small pool still feel dynamically different.
    constexpr int kJitter = 12;
    for (int t = 0; t < NUM_TRACKS; ++t)
        for (auto& h : result.hits[t])
        {
            float scaled   = juce::jlimit (1.0f, 127.0f, (float) h.velocity * mods.velScale);
            int   jittered = (int) std::round (scaled) + rng.nextInt (2 * kJitter + 1) - kJitter;
            h.velocity     = (uint8_t) juce::jlimit (1, 127, jittered);
        }

    result.genres      = tags;
    result.type        = type;
    result.isComposite = true;
    result.name        = "Generated";
    result.sourceFile  = juce::File();
    result.syncLegacyFromHits();
    result.recomputeDensity();
    return result;
}

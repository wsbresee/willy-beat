#pragma once
#include "DrumPattern.h"

class PatternLibrary
{
public:
    PatternLibrary() = default;

    // Write every built-in preset as a .beat file to dir (called once on first run)
    void installDefaultsTo (const juce::File& dir);

    // Clear patterns and reload all *.beat files found in dir
    void loadFromDirectory (const juce::File& dir);

    // Write one pattern to dir/<name>.beat, returns the file written
    juce::File savePattern (const DrumPattern& p, const juce::File& dir);

    // Update the in-memory copy of a pattern (matched by sourceFile).
    // Call this after autoSave so the library pointer stays current.
    void updatePattern (const DrumPattern& p);

    // Return all loaded patterns
    const std::vector<DrumPattern>& all() const { return patterns; }

    // Return all distinct genre tags across loaded patterns (sorted alphabetically)
    juce::StringArray getGenres() const;

    // Return patterns whose genre tags include `tag`.
    // If tag is empty, returns all patterns.
    // fuzzy=true: accept any tag that contains or is contained in `tag`.
    std::vector<const DrumPattern*> getByTag (const juce::String& tag,
                                              bool fuzzy = false) const;

    // Return patterns matching ANY of the given tags (case-insensitive exact match).
    // Empty input returns all patterns.
    std::vector<const DrumPattern*> getByTags (const juce::StringArray& tags) const;

    // Generate a composite pattern by picking each track row from a random
    // source pattern matching ANY of the given tags.  seed controls which mix.
    // Empty tags = draw from the entire library.
    DrumPattern makeComposite (const juce::StringArray& tags, PatType type,
                               juce::int64 seed) const;

    // Parse a single .beat file directly from disk (pristine, ignores library cache).
    static DrumPattern loadFromFile (const juce::File& f);

private:
    std::vector<DrumPattern> patterns;

    static std::vector<DrumPattern> builtinPatterns();
    static DrumPattern patternFromFile (const juce::File& f);
    static bool patternToFile (const DrumPattern& p, const juce::File& f);
    static juce::File patternFile (const juce::File& dir, const DrumPattern& p);

    static int     trackFromKey  (const juce::String& key);
    static PatType typeFromString (const juce::String& s);
};

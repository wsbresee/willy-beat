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

    // Query
    const std::vector<DrumPattern>& all() const { return patterns; }
    std::vector<const DrumPattern*> get (Genre genre, PatType type) const;

private:
    std::vector<DrumPattern> patterns;

    static std::vector<DrumPattern> builtinPatterns();
    static DrumPattern patternFromFile (const juce::File& f);
    static bool patternToFile (const DrumPattern& p, const juce::File& f);
    static juce::File patternFile (const juce::File& dir, const DrumPattern& p);

    static int    trackFromKey    (const juce::String& key);
    static Genre  genreFromString (const juce::String& s);
    static PatType typeFromString (const juce::String& s);
};

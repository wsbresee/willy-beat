#pragma once
#include "DrumPattern.h"

class PatternLibrary
{
public:
    PatternLibrary();

    // All patterns (built-in + user-loaded)
    const std::vector<DrumPattern>& all() const { return patterns; }

    // Filtered by genre + type
    std::vector<const DrumPattern*> get (Genre genre, PatType type) const;

    // Add a user-loaded pattern (e.g. from MIDI import)
    void addUserPattern (DrumPattern p);

    // Serialise/deserialise user patterns to/from XML (for plugin state)
    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    std::vector<DrumPattern> patterns;
    void loadBuiltins();
};

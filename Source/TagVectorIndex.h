#pragma once
#include <JuceHeader.h>

// Semantic tag matcher backed by Apple's NaturalLanguage framework. Embeds
// each known tag once via NLEmbedding's sentence model, then finds the best
// match for an arbitrary user query by cosine similarity in the same space.
//
// macOS-only. Build on Windows would need a different embedder (ONNX
// MiniLM, etc.); the editor uses isAvailable() to skip vector search when
// no embedder is present.
class TagVectorIndex
{
public:
    TagVectorIndex();
    ~TagVectorIndex();

    bool isAvailable() const;

    // Precompute embeddings for the full tag vocabulary. Cheap — sub-ms
    // per tag — so safe to call on every library refresh.
    void setKnownTags (const juce::StringArray& tags);

    // Return the tag whose embedding is closest to the query, or an empty
    // string when no tag clears the similarity threshold (or when the
    // embedder is unavailable). Restricts results to `excluded`-free tags
    // so already-selected chips don't get re-suggested.
    juce::String findBestMatch (const juce::String& query,
                                const juce::StringArray& excluded) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagVectorIndex)
};

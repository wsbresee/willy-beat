#pragma once
#include <JuceHeader.h>

// Semantic tag matcher backed by Apple's NaturalLanguage framework. Embeds
// each known tag via NLEmbedding's sentence model and matches arbitrary
// user queries by cosine similarity.
//
// Thread model: setKnownTags() returns immediately and builds the index
// on a background thread, sharing work across instances via a process-wide
// embedding cache. Search calls are safe to invoke before the build
// finishes; entries that haven't been embedded yet are skipped.
//
// macOS-only. Build on Windows would need a different embedder; the
// editor uses isAvailable() to skip vector search when none is present.
class TagVectorIndex
{
public:
    TagVectorIndex();
    ~TagVectorIndex();

    bool isAvailable() const;
    bool isReady()     const;

    // Queue a vocabulary build on a background thread. Cheap to call -
    // most embeddings are served from a process-wide cache. A previously
    // running build is joined before the new one starts.
    void setKnownTags (const juce::StringArray& tags);

    // Return the tag whose embedding is closest to the query, or an empty
    // string when no tag clears the similarity threshold (or when the
    // embedder is unavailable). `excluded` tags are skipped.
    juce::String findBestMatch (const juce::String& query,
                                const juce::StringArray& excluded) const;

    // Return up to `maxResults` tags nearest to the union of `seeds`'s
    // embeddings. Seeds come first; the rest fill by descending similarity.
    juce::StringArray findNearestN (const juce::StringArray& seeds,
                                    int maxResults) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TagVectorIndex)
};

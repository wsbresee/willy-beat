#import <Foundation/Foundation.h>
#import <NaturalLanguage/NaturalLanguage.h>

#include "TagVectorIndex.h"
#include <vector>
#include <cmath>

struct TagVectorIndex::Impl
{
    NLEmbedding*                    embedding = nil;
    juce::StringArray               tags;
    std::vector<std::vector<double>> vectors;
};

TagVectorIndex::TagVectorIndex() : impl (std::make_unique<Impl>())
{
    if (@available (macOS 11.0, *))
        impl->embedding = [NLEmbedding sentenceEmbeddingForLanguage: NLLanguageEnglish];
}

TagVectorIndex::~TagVectorIndex() = default;

bool TagVectorIndex::isAvailable() const
{
    return impl->embedding != nil;
}

static std::vector<double> embedString (NLEmbedding* embedder, const juce::String& s)
{
    std::vector<double> out;
    if (embedder == nil || s.isEmpty()) return out;

    @autoreleasepool
    {
        NSString* nss = [NSString stringWithUTF8String: s.toRawUTF8()];
        if (nss == nil) return out;
        NSArray<NSNumber*>* vec = [embedder vectorForString: nss];
        if (vec == nil) return out;
        out.reserve ((size_t) vec.count);
        for (NSNumber* n in vec)
            out.push_back (n.doubleValue);
    }
    return out;
}

void TagVectorIndex::setKnownTags (const juce::StringArray& tags)
{
    impl->tags = tags;
    impl->vectors.clear();
    impl->vectors.reserve ((size_t) tags.size());
    for (const auto& tag : tags)
        impl->vectors.push_back (embedString (impl->embedding, tag));
}

static double cosine (const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.empty() || b.empty() || a.size() != b.size()) return -1.0;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na == 0.0 || nb == 0.0) return -1.0;
    return dot / (std::sqrt (na) * std::sqrt (nb));
}

juce::String TagVectorIndex::findBestMatch (const juce::String& query,
                                            const juce::StringArray& excluded) const
{
    if (impl->embedding == nil || query.isEmpty()) return {};

    auto qvec = embedString (impl->embedding, query);
    if (qvec.empty()) return {};

    // Cosine threshold below which we'd rather return nothing than suggest
    // an unrelated tag. Empirically NLEmbedding sentence vectors land in
    // the 0.3-0.6 range for "loosely related" pairs, ≥ ~0.7 for tight
    // synonyms.
    constexpr double kMinSimilarity = 0.55;

    double bestScore = kMinSimilarity;
    int    bestIdx   = -1;
    for (size_t i = 0; i < impl->vectors.size(); ++i)
    {
        if (excluded.contains (impl->tags[(int) i])) continue;
        double s = cosine (qvec, impl->vectors[i]);
        if (s > bestScore) { bestScore = s; bestIdx = (int) i; }
    }
    return bestIdx >= 0 ? impl->tags[bestIdx] : juce::String();
}

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

juce::StringArray TagVectorIndex::findNearestN (const juce::StringArray& seeds,
                                                int maxResults) const
{
    juce::StringArray out;
    if (impl->embedding == nil || seeds.isEmpty() || maxResults <= 0) return out;

    // Pre-embed every seed so we score candidates against the BEST seed
    // match (not an averaged centroid — distinct seeds shouldn't smear).
    std::vector<std::vector<double>> seedVecs;
    seedVecs.reserve ((size_t) seeds.size());
    for (const auto& s : seeds)
        seedVecs.push_back (embedString (impl->embedding, s));

    // Seeds always come first — they're the user's intent.
    for (const auto& s : seeds)
    {
        if (out.size() >= maxResults) break;
        if (impl->tags.contains (s) && ! out.contains (s))
            out.add (s);
    }

    // Score non-seed candidates by max cosine against any seed.
    struct Cand { int idx; double score; };
    std::vector<Cand> cands;
    cands.reserve (impl->vectors.size());

    for (size_t i = 0; i < impl->vectors.size(); ++i)
    {
        if (out.contains (impl->tags[(int) i])) continue;
        double best = -1.0;
        for (const auto& sv : seedVecs)
        {
            double s = cosine (sv, impl->vectors[i]);
            if (s > best) best = s;
        }
        if (best > 0.0) cands.push_back ({ (int) i, best });
    }

    std::sort (cands.begin(), cands.end(),
               [] (const Cand& a, const Cand& b) { return a.score > b.score; });

    // Floor: anything below this similarity is too unrelated to include.
    constexpr double kMinSimilarity = 0.55;
    while (! cands.empty() && cands.back().score < kMinSimilarity)
        cands.pop_back();

    // Gap-aware sizing. Walk the sorted candidates; if there's a big drop
    // between consecutive scores, cut there. Yields:
    //   - exact tag with no close neighbours  → seeds only (cands empty
    //     after floor, or the first non-seed is far below the cliff)
    //   - tight cluster of 2-5 close neighbours → that cluster
    //   - long tail of similar scores → up to maxResults
    constexpr double kGapThreshold = 0.10;

    const int budget = juce::jmax (0, maxResults - out.size());
    int cut = juce::jmin (budget, (int) cands.size());
    for (int i = 1; i < cut; ++i)
    {
        const double gap = cands[(size_t) i - 1].score - cands[(size_t) i].score;
        if (gap > kGapThreshold) { cut = i; break; }
    }

    for (int i = 0; i < cut; ++i)
        out.add (impl->tags[cands[(size_t) i].idx]);

    return out;
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

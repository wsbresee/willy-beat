#import <Foundation/Foundation.h>
#import <NaturalLanguage/NaturalLanguage.h>

#include "TagVectorIndex.h"
#include "BinaryData.h"

#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// ─── Latin-only safety check ────────────────────────────────────────────
// Apple's English NLEmbedding crashes inside vectorForString: when handed
// mixed-script tags (e.g. CJK + Latin). We only support English, so
// anything outside ASCII printable / Latin supplements is filtered out
// before reaching the framework.
static bool isLatinSafe (const juce::String& s)
{
    for (auto c : s)
    {
        const auto cp = (juce_wchar) c;
        if (cp < 0x20)                       return false;
        if (cp <= 0x7E)                      continue;
        if (cp >= 0x00A0 && cp <= 0x024F)    continue;
        return false;
    }
    return true;
}

// ─── Process-wide embedding cache + mutex ────────────────────────────────
// Multiple plugin instances may be created concurrently (host plugin scan
// running alongside the GUI instance). NLEmbedding is not safe to call
// concurrently from multiple threads, and re-embedding identical strings
// is wasteful, so we cache results across the whole process.
namespace {

std::mutex& cacheMutex()
{
    static std::mutex m;
    return m;
}

std::mutex& nlMutex()
{
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, std::vector<double>>& cache()
{
    static std::unordered_map<std::string, std::vector<double>> c;
    return c;
}

NLEmbedding* sharedEmbedding()
{
    // The file isn't compiled with ARC, so the autoreleased object that
    // sentenceEmbeddingForLanguage: hands back must be explicitly retained
    // before we cache it in a function-local static. Without this retain,
    // the object is freed when the surrounding autoreleasepool drains and
    // the next vectorForString: call dereferences a dangling pointer.
    static NLEmbedding* e = []() -> NLEmbedding* {
        if (@available (macOS 11.0, *))
        {
            NLEmbedding* tmp = [NLEmbedding sentenceEmbeddingForLanguage: NLLanguageEnglish];
            return [tmp retain];
        }
        return nil;
    }();
    return e;
}

// Lazy: parse the baked-binary blob the first time someone asks for an
// embedding. Blob format (little-endian):
//   uint32 magic = 'EBTB' (Embedded Baked TagBlob)
//   uint32 version = 1
//   uint32 count
//   uint32 vecdim
//   For each entry:
//     uint32 taglen, taglen bytes (UTF-8, no NUL), vecdim * float32
void ensureBakedLoaded()
{
    static std::once_flag once;
    std::call_once (once, []
    {
        const char* data = nullptr;
        int sizeBytes = 0;

       #if JUCE_INCLUDED_BAKED_EMBEDDINGS
        data      = BinaryData::BakedEmbeddings_bin;
        sizeBytes = BinaryData::BakedEmbeddings_binSize;
       #endif
        if (data == nullptr || sizeBytes < 16) return;

        auto readU32 = [&] (size_t off) -> uint32_t
        {
            uint32_t v = 0;
            std::memcpy (&v, data + off, 4);
            return v;
        };

        if (readU32 (0) != 0x42544245u /* 'EBTB' */) return; // wrong magic
        if (readU32 (4) != 1)                       return; // wrong version

        const uint32_t count  = readU32 (8);
        const uint32_t vecDim = readU32 (12);
        size_t off = 16;

        std::lock_guard<std::mutex> lock (cacheMutex());
        auto& c = cache();
        c.reserve (c.size() + count);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (off + 4 > (size_t) sizeBytes) return;
            const uint32_t taglen = readU32 (off);
            off += 4;
            if (off + taglen + vecDim * 4u > (size_t) sizeBytes) return;
            std::string tag ((const char*) data + off, taglen);
            off += taglen;
            std::vector<double> v;
            v.reserve (vecDim);
            for (uint32_t k = 0; k < vecDim; ++k)
            {
                float f = 0.0f;
                std::memcpy (&f, data + off, 4);
                v.push_back ((double) f);
                off += 4;
            }
            c.emplace (std::move (tag), std::move (v));
        }
    });
}

std::vector<double> embedFresh (const juce::String& s)
{
    std::vector<double> out;
    if (sharedEmbedding() == nil || s.isEmpty()) return out;
    if (! isLatinSafe (s)) return out;

    @autoreleasepool
    {
        NSString* nss = [NSString stringWithUTF8String: s.toRawUTF8()];
        if (nss == nil) return out;
        // Serialise NLEmbedding calls - Apple's framework is not safe
        // under concurrent access (see Cubase crash trace).
        std::lock_guard<std::mutex> lock (nlMutex());
        NSArray<NSNumber*>* vec = [sharedEmbedding() vectorForString: nss];
        if (vec == nil) return out;
        out.reserve ((size_t) vec.count);
        for (NSNumber* n in vec)
            out.push_back (n.doubleValue);
    }
    return out;
}

std::vector<double> embedCached (const juce::String& s)
{
    const auto trimmed = s.trim();
    if (trimmed.isEmpty()) return {};

    ensureBakedLoaded();

    const std::string key = trimmed.toStdString();
    {
        std::lock_guard<std::mutex> lock (cacheMutex());
        auto it = cache().find (key);
        if (it != cache().end()) return it->second;
    }

    auto v = embedFresh (trimmed);
    if (! v.empty())
    {
        std::lock_guard<std::mutex> lock (cacheMutex());
        cache().emplace (key, v);
    }
    return v;
}

double cosine (const std::vector<double>& a, const std::vector<double>& b)
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

} // namespace

// ─── Impl ────────────────────────────────────────────────────────────────
struct TagVectorIndex::Impl
{
    juce::StringArray              tags;
    std::vector<std::vector<double>> vectors;
    mutable std::mutex             localMutex;
    std::atomic<bool>              ready { false };
    std::atomic<bool>              cancelBuild { false };
    std::thread                    builder;

    ~Impl()
    {
        cancelBuild = true;
        if (builder.joinable()) builder.join();
    }
};

TagVectorIndex::TagVectorIndex() : impl (std::make_unique<Impl>()) {}
TagVectorIndex::~TagVectorIndex() = default;

bool TagVectorIndex::isAvailable() const { return sharedEmbedding() != nil; }
bool TagVectorIndex::isReady()     const { return impl->ready.load(); }

void TagVectorIndex::setKnownTags (const juce::StringArray& tags)
{
    impl->cancelBuild = true;
    if (impl->builder.joinable()) impl->builder.join();
    impl->cancelBuild = false;

    {
        std::lock_guard<std::mutex> lock (impl->localMutex);
        impl->tags = tags;
        impl->vectors.assign ((size_t) tags.size(), {});
        impl->ready = false;
    }

    // Snapshot tags for the worker so it never reads impl->tags after a
    // re-entrant setKnownTags() resets them.
    juce::StringArray tagsCopy = tags;
    impl->builder = std::thread ([this, tagsCopy = std::move (tagsCopy)]() mutable
    {
        std::vector<std::vector<double>> built;
        built.reserve ((size_t) tagsCopy.size());
        for (const auto& t : tagsCopy)
        {
            if (impl->cancelBuild.load()) return;
            built.push_back (embedCached (t));
        }
        {
            std::lock_guard<std::mutex> lock (impl->localMutex);
            if (impl->cancelBuild.load()) return;
            impl->vectors = std::move (built);
        }
        impl->ready = true;
    });
}

juce::String TagVectorIndex::findBestMatch (const juce::String& query,
                                            const juce::StringArray& excluded) const
{
    if (sharedEmbedding() == nil || query.isEmpty()) return {};

    auto qvec = embedCached (query);
    if (qvec.empty()) return {};

    constexpr double kMinSimilarity = 0.55;
    double bestScore = kMinSimilarity;
    int    bestIdx   = -1;

    std::lock_guard<std::mutex> lock (impl->localMutex);
    for (size_t i = 0; i < impl->vectors.size(); ++i)
    {
        if (impl->vectors[i].empty()) continue;            // not built yet
        if (excluded.contains (impl->tags[(int) i])) continue;
        double s = cosine (qvec, impl->vectors[i]);
        if (s > bestScore) { bestScore = s; bestIdx = (int) i; }
    }
    return bestIdx >= 0 ? impl->tags[bestIdx] : juce::String();
}

juce::StringArray TagVectorIndex::findNearestN (const juce::StringArray& seeds,
                                                int maxResults) const
{
    juce::StringArray out;
    if (sharedEmbedding() == nil || seeds.isEmpty() || maxResults <= 0) return out;

    std::vector<std::vector<double>> seedVecs;
    seedVecs.reserve ((size_t) seeds.size());
    for (const auto& s : seeds)
        seedVecs.push_back (embedCached (s));

    std::lock_guard<std::mutex> lock (impl->localMutex);

    // Seeds always come first.
    for (const auto& s : seeds)
    {
        if (out.size() >= maxResults) break;
        if (impl->tags.contains (s) && ! out.contains (s))
            out.add (s);
    }

    struct Cand { int idx; double score; };
    std::vector<Cand> cands;
    cands.reserve (impl->vectors.size());

    for (size_t i = 0; i < impl->vectors.size(); ++i)
    {
        if (impl->vectors[i].empty()) continue;            // not built yet
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

    constexpr double kMinSimilarity = 0.55;
    while (! cands.empty() && cands.back().score < kMinSimilarity)
        cands.pop_back();

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

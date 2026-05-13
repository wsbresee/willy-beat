// BakeTagEmbeddings - Build-time tool that scans a folder of .beat presets,
// embeds every unique tag through Apple's NLEmbedding sentence model, and
// writes a binary blob the plugin reads at startup. With the blob shipped
// inside the binary, plugin instances never need to call NLEmbedding for a
// known tag - critical for sandboxed hosts (Cubase) that don't tolerate
// the multi-second synchronous build the live embedder takes.
//
// Usage: BakeTagEmbeddings <preset_dir> <output.bin>
//
// Output format (little-endian):
//   uint32 magic   = 'EBTB' (0x42544245)
//   uint32 version = 1
//   uint32 count
//   uint32 vecdim
//   For each entry:
//     uint32 taglen, taglen UTF-8 bytes, vecdim * float32

#import <Foundation/Foundation.h>
#import <NaturalLanguage/NaturalLanguage.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool isLatinSafe (const std::string& s)
{
    // Same predicate as TagVectorIndex.mm so we don't bake a tag that the
    // plugin would refuse to embed at runtime.
    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = (unsigned char) s[i];
        uint32_t cp = 0;
        size_t   len = 0;
        if (c < 0x80)            { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else return false;
        if (i + len > s.size()) return false;
        for (size_t k = 1; k < len; ++k)
            cp = (cp << 6) | (s[i + k] & 0x3F);
        if (cp < 0x20)                  return false;
        if (cp > 0x7E && (cp < 0x00A0 || cp > 0x024F)) return false;
        i += len;
    }
    return true;
}

static std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

static std::set<std::string> collectTags (const fs::path& dir)
{
    std::set<std::string> out;
    std::regex genres ("^genres:\\s*(.*)$");
    for (auto& entry : fs::directory_iterator (dir))
    {
        if (entry.path().extension() != ".beat") continue;
        std::ifstream f (entry.path());
        std::string   line;
        while (std::getline (f, line))
        {
            std::smatch m;
            if (! std::regex_match (line, m, genres)) continue;
            std::string rest = m[1].str();
            size_t start = 0;
            while (start <= rest.size())
            {
                size_t end = rest.find (',', start);
                if (end == std::string::npos) end = rest.size();
                std::string tag = trim (rest.substr (start, end - start));
                if (! tag.empty()) out.insert (std::move (tag));
                start = end + 1;
            }
            break;
        }
    }
    return out;
}

static std::vector<double> embed (NLEmbedding* e, const std::string& s)
{
    std::vector<double> out;
    if (e == nil || s.empty() || ! isLatinSafe (s)) return out;
    @autoreleasepool
    {
        NSString* nss = [NSString stringWithUTF8String: s.c_str()];
        if (nss == nil) return out;
        NSArray<NSNumber*>* v = [e vectorForString: nss];
        if (v == nil) return out;
        out.reserve ((size_t) v.count);
        for (NSNumber* n in v) out.push_back (n.doubleValue);
    }
    return out;
}

static void writeU32 (std::ofstream& f, uint32_t v)
{
    f.write ((const char*) &v, 4);
}

int main (int argc, char** argv)
{
    if (argc != 3)
    {
        std::fprintf (stderr, "usage: %s <preset_dir> <output.bin>\n", argv[0]);
        return 1;
    }
    fs::path presetDir = argv[1];
    fs::path outFile   = argv[2];

    if (! fs::is_directory (presetDir))
    {
        std::fprintf (stderr, "not a directory: %s\n", presetDir.c_str());
        return 2;
    }

    auto tags = collectTags (presetDir);
    std::fprintf (stderr, "BakeTagEmbeddings: %zu unique tags from %s\n",
                  tags.size(), presetDir.c_str());

    NLEmbedding* embedder = nil;
    if (@available (macOS 11.0, *))
        embedder = [NLEmbedding sentenceEmbeddingForLanguage: NLLanguageEnglish];
    if (embedder == nil)
    {
        std::fprintf (stderr, "no English NLEmbedding available\n");
        return 3;
    }

    std::vector<std::pair<std::string, std::vector<double>>> entries;
    entries.reserve (tags.size());
    uint32_t vecDim = 0;
    int skipped = 0;
    for (const auto& t : tags)
    {
        auto v = embed (embedder, t);
        if (v.empty()) { ++skipped; continue; }
        if (vecDim == 0) vecDim = (uint32_t) v.size();
        if (v.size() != vecDim) { ++skipped; continue; } // sanity check
        entries.emplace_back (t, std::move (v));
    }

    std::fprintf (stderr, "BakeTagEmbeddings: embedded %zu, skipped %d, vecdim=%u\n",
                  entries.size(), skipped, vecDim);

    fs::create_directories (outFile.parent_path());
    std::ofstream f (outFile, std::ios::binary);
    if (! f)
    {
        std::fprintf (stderr, "cannot open %s for writing\n", outFile.c_str());
        return 4;
    }

    writeU32 (f, 0x42544245u); // 'EBTB' little-endian
    writeU32 (f, 1);
    writeU32 (f, (uint32_t) entries.size());
    writeU32 (f, vecDim);
    for (const auto& [tag, vec] : entries)
    {
        writeU32 (f, (uint32_t) tag.size());
        f.write (tag.data(), (std::streamsize) tag.size());
        for (double d : vec)
        {
            float fl = (float) d;
            f.write ((const char*) &fl, 4);
        }
    }
    f.close();

    std::fprintf (stderr, "BakeTagEmbeddings: wrote %s\n", outFile.c_str());
    return 0;
}

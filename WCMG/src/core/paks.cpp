// Paks scanner: every IoStore container and legacy pak under Content/Paks, the cooked
// packages each one carries, which game packages a mod replaces and where two mods
// fight over the same package.
//
// The .utoc layout follows UE 5.6.1 (Core/Internal/IO/IoStore.h, Private/IO/IoStore.cpp
// FIoStoreTocResource::Read): a 144-byte FIoStoreTocHeader, then chunk ids, offsets,
// perfect-hash seeds, overflow indices, compression blocks, method names, optional
// signatures, the directory index and the chunk metas. Only the table is read, never
// the .ucas.
#include "scanners.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace wcmg {
namespace {

constexpr std::array<unsigned char, 16> toc_magic{'-','=','=','-','-','=','=','-','-','=','=','-','-','=','=','-'};
constexpr size_t toc_header_bytes = 144;
constexpr size_t chunk_id_bytes = 12;
constexpr size_t offset_length_bytes = 10;
constexpr size_t block_entry_bytes = 12;
constexpr uint8_t toc_version_directory_index = 2;  // EIoStoreTocVersion::DirectoryIndex, oldest the engine accepts
constexpr uint8_t toc_version_latest = 8;           // ReplaceIoChunkHashWithIoHash, latest in 5.6.1
constexpr uint8_t chunk_export_bundle = 1;          // EIoChunkType::ExportBundleData, one per package
constexpr uint8_t flag_encrypted = 1 << 1, flag_signed = 1 << 2, flag_indexed = 1 << 3;
constexpr uint32_t max_chunks = 4u << 20;
constexpr uint32_t max_blocks = 64u << 20;
constexpr uint32_t max_directory_index = 256u << 20;
constexpr size_t max_names_per_container = 20000;
constexpr size_t max_override_names = 200;
constexpr size_t max_override_hints = 30;
constexpr size_t max_collisions = 500;
constexpr size_t max_stray = 100;
constexpr size_t max_files = 20000;

uint32_t le32(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint64_t le64(const unsigned char* p) { return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32; }

bool istarts(const std::string& s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    return true;
}
bool iequals(const std::string& a, std::string_view b) { return a.size() == b.size() && istarts(a, b); }
std::string generic_utf8(const fs::path& p) { const auto s = p.generic_u8string(); return std::string(s.begin(), s.end()); }

// FString::EndsWith(TEXT("_P")) in FPakPlatformFile ignores case.
bool patch_stem(const std::string& stem) {
    return stem.size() > 2 && stem[stem.size() - 2] == '_' && (stem.back() == 'P' || stem.back() == 'p');
}

void append_utf8(std::string& out, uint32_t c) {
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xC0 | c >> 6); out += char(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { out += char(0xE0 | c >> 12); out += char(0x80 | (c >> 6 & 0x3F)); out += char(0x80 | (c & 0x3F)); }
    else { out += char(0xF0 | c >> 18); out += char(0x80 | (c >> 12 & 0x3F)); out += char(0x80 | (c >> 6 & 0x3F)); out += char(0x80 | (c & 0x3F)); }
}

// Bounds-checked reader over the directory index buffer (FArchive layout, little endian).
struct Reader {
    const unsigned char* data;
    size_t size, pos = 0;
    bool ok = true;
    bool need(size_t n) { if (!ok || n > size - pos) ok = false; return ok; }
    uint32_t u32() { if (!need(4)) return 0; auto v = le32(data + pos); pos += 4; return v; }
    // FString: int32 length including the terminator; negative means UTF-16.
    std::string fstring() {
        const auto n = int32_t(u32());
        if (!ok || n == 0) return {};
        std::string out;
        if (n > 0) {
            if (n > 65536 || !need(size_t(n))) { ok = false; return {}; }
            for (int32_t i = 0; i + 1 < n; ++i) append_utf8(out, data[pos + i]);
            pos += size_t(n);
        } else {
            const int64_t units = -int64_t(n);
            if (units > 65536 || !need(size_t(units) * 2)) { ok = false; return {}; }
            for (int64_t i = 0; i + 1 < units; ++i) {
                uint32_t c = uint32_t(data[pos + 2 * i]) | uint32_t(data[pos + 2 * i + 1]) << 8;
                if (c >= 0xD800 && c < 0xDC00 && i + 2 < units) {
                    const uint32_t lo = uint32_t(data[pos + 2 * i + 2]) | uint32_t(data[pos + 2 * i + 3]) << 8;
                    if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); ++i; }
                }
                append_utf8(out, c);
            }
            pos += size_t(units) * 2;
        }
        return out;
    }
};

std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return a.back() == '/' ? a + b : a + "/" + b;
}

// "../../../MortalShell2/Content/Sparta/UI/WBP_X.uasset" -> "/Game/Sparta/UI/WBP_X", the
// way the engine names mounted packages. Engine and plugin content get their own roots.
std::string package_name(std::string path) {
    while (path.starts_with("../")) path.erase(0, 3);
    while (path.starts_with("/")) path.erase(0, 1);
    const auto dot = path.rfind('.');
    if (dot != std::string::npos && path.find('/', dot) == std::string::npos) path.resize(dot);
    const auto content = path.find("/Content/");
    if (content == std::string::npos) return "/" + path;
    const std::string head = path.substr(0, content), rest = path.substr(content + 9);
    if (head.find('/') == std::string::npos) return iequals(head, "Engine") ? "/Engine/" + rest : "/Game/" + rest;
    // .../Plugins/<...>/<Plugin>/Content/...: the mount root is the plugin's name.
    if (head.find("Plugins/") != std::string::npos) return "/" + head.substr(head.rfind('/') + 1) + "/" + rest;
    return "/" + path;
}

struct TocRead {
    std::string error;                   // non-empty: the table did not parse
    uint8_t version = 0, flags = 0;
    uint32_t chunks = 0, partitions = 1;
    uint64_t needed_ucas = 0;            // end of the furthest compression block
    std::vector<uint64_t> package_ids;   // sorted, unique
    std::vector<std::pair<uint64_t, std::string>> named;  // export-bundle package id -> package name
    bool index_present = false, index_encrypted = false;
    std::string index_error;
};

TocRead read_toc(const fs::path& utoc, uint64_t file_size) {
    TocRead r;
    std::ifstream f(utoc, std::ios::binary);
    if (!f) { r.error = "cannot open"; return r; }
    std::array<unsigned char, toc_header_bytes> h{};
    f.read(reinterpret_cast<char*>(h.data()), h.size());
    if (!f) { r.error = "shorter than its header"; return r; }
    if (!std::equal(toc_magic.begin(), toc_magic.end(), h.begin())) { r.error = "bad magic, not an IoStore table"; return r; }
    r.version = h[16];
    const uint32_t header_size = le32(&h[20]), chunks = le32(&h[24]), blocks = le32(&h[28]), block_size = le32(&h[32]);
    const uint32_t method_count = le32(&h[36]), method_length = le32(&h[40]), index_size = le32(&h[48]);
    const uint32_t partitions = le32(&h[52]);
    r.flags = h[80];
    const uint32_t seeds = le32(&h[84]), overflow = le32(&h[96]);
    if (r.version < toc_version_directory_index) { r.error = "table version " + std::to_string(r.version) + " is too old for UE5"; return r; }
    if (r.version > toc_version_latest) { r.error = "table version " + std::to_string(r.version) + " is newer than UE 5.6 reads"; return r; }
    if (header_size != toc_header_bytes) { r.error = "unexpected header size " + std::to_string(header_size); return r; }
    if (block_size != block_entry_bytes) { r.error = "unexpected compression block entry size"; return r; }
    if (chunks > max_chunks || blocks > max_blocks || index_size > max_directory_index || method_count > 256 || method_length > 1024) {
        r.error = "implausible table counts"; return r;
    }
    r.chunks = chunks;
    r.partitions = r.version >= 3 ? std::max<uint32_t>(partitions, 1) : 1;
    // Only versions that carry the perfect hash fields have them (IoStore.cpp, Read).
    const uint64_t seed_count = r.version >= 4 ? seeds : 0, overflow_count = r.version >= 5 ? overflow : 0;
    const uint64_t before_index = toc_header_bytes + uint64_t(chunks) * (chunk_id_bytes + offset_length_bytes) +
        (seed_count + overflow_count) * 4 + uint64_t(blocks) * block_entry_bytes + uint64_t(method_count) * method_length;
    if (before_index > file_size) { r.error = "truncated: the table needs more bytes than the file has"; return r; }

    std::vector<unsigned char> ids(size_t(chunks) * chunk_id_bytes);
    if (chunks) {
        f.read(reinterpret_cast<char*>(ids.data()), std::streamsize(ids.size()));
        if (!f) { r.error = "truncated chunk table"; return r; }
    }
    for (uint32_t i = 0; i < chunks; ++i) {
        const auto* e = ids.data() + size_t(i) * chunk_id_bytes;
        if (e[11] == chunk_export_bundle) r.package_ids.push_back(le64(e));
    }
    std::sort(r.package_ids.begin(), r.package_ids.end());
    r.package_ids.erase(std::unique(r.package_ids.begin(), r.package_ids.end()), r.package_ids.end());

    // Compression blocks: 40-bit offset, 24-bit compressed size. Read in slices so a
    // game container's half a million blocks never sit in memory at once.
    f.seekg(std::streamoff(toc_header_bytes + uint64_t(chunks) * (chunk_id_bytes + offset_length_bytes) + (seed_count + overflow_count) * 4));
    {
        std::vector<unsigned char> slice;
        uint32_t left = blocks;
        while (left) {
            const uint32_t n = std::min<uint32_t>(left, 65536);
            slice.resize(size_t(n) * block_entry_bytes);
            f.read(reinterpret_cast<char*>(slice.data()), std::streamsize(slice.size()));
            if (!f) { r.error = "truncated compression block table"; return r; }
            for (uint32_t i = 0; i < n; ++i) {
                const auto* b = slice.data() + size_t(i) * block_entry_bytes;
                const uint64_t offset = le64(b) & ((uint64_t(1) << 40) - 1);
                const uint64_t compressed = (le32(b + 4) >> 8) & 0xFFFFFF;
                r.needed_ucas = std::max(r.needed_ucas, offset + compressed);
            }
            left -= n;
        }
    }
    uint64_t pos = before_index;
    if (r.flags & flag_signed) {
        f.seekg(std::streamoff(pos));
        std::array<unsigned char, 4> hs{};
        f.read(reinterpret_cast<char*>(hs.data()), 4);
        if (!f) { r.error = "truncated signature block"; return r; }
        const auto hash_size = int32_t(le32(hs.data()));
        if (hash_size < 0 || hash_size > 4096) { r.error = "implausible signature size"; return r; }
        pos += 4 + uint64_t(hash_size) * 2 + uint64_t(blocks) * 20;
    }
    if (!(r.flags & flag_indexed) || index_size == 0) return r;
    if (pos + index_size > file_size) { r.index_error = "directory index runs past the end of the file"; return r; }
    r.index_present = true;
    if (r.flags & flag_encrypted) { r.index_encrypted = true; return r; }

    std::vector<unsigned char> index(index_size);
    f.seekg(std::streamoff(pos));
    f.read(reinterpret_cast<char*>(index.data()), std::streamsize(index.size()));
    if (!f) { r.index_error = "cannot read the directory index"; return r; }
    Reader in{index.data(), index.size()};
    const std::string mount = in.fstring();
    const uint32_t dir_count = in.u32();
    if (!in.ok || dir_count > index.size() / 16) { r.index_error = "bad directory entries"; return r; }
    struct Dir { uint32_t name, child, sibling, file; };
    std::vector<Dir> dirs(dir_count);
    for (auto& d : dirs) { d.name = in.u32(); d.child = in.u32(); d.sibling = in.u32(); d.file = in.u32(); }
    const uint32_t file_count = in.u32();
    if (!in.ok || file_count > index.size() / 12) { r.index_error = "bad file entries"; return r; }
    struct File { uint32_t name, next, user; };
    std::vector<File> files(file_count);
    for (auto& fe : files) { fe.name = in.u32(); fe.next = in.u32(); fe.user = in.u32(); }
    const uint32_t string_count = in.u32();
    if (!in.ok || string_count > index.size() / 4) { r.index_error = "bad string table"; return r; }
    std::vector<std::string> strings(string_count);
    for (auto& s : strings) s = in.fstring();
    if (!in.ok) { r.index_error = "truncated string table"; return r; }

    // Same walk as FIoDirectoryIndexReaderImpl::IterateDirectoryIndex, iterative, and
    // each directory visited once so a corrupt index cannot loop.
    constexpr uint32_t none = ~uint32_t(0);
    auto str = [&](uint32_t i) -> const std::string* { return i < strings.size() ? &strings[i] : nullptr; };
    std::vector<char> seen(dirs.size(), 0);
    std::vector<std::pair<uint32_t, std::string>> stack;
    if (!dirs.empty()) stack.emplace_back(0, std::string());
    while (!stack.empty()) {
        auto [d, path] = std::move(stack.back());
        stack.pop_back();
        if (d >= dirs.size() || seen[d]) continue;
        seen[d] = 1;
        size_t steps = 0;
        for (uint32_t fi = dirs[d].file; fi != none && fi < files.size() && steps++ <= files.size(); fi = files[fi].next) {
            const auto* name = str(files[fi].name);
            const uint32_t toc_index = files[fi].user;
            if (!name || toc_index >= chunks) continue;
            const auto* e = ids.data() + size_t(toc_index) * chunk_id_bytes;
            if (e[11] != chunk_export_bundle) continue;
            r.named.emplace_back(le64(e), package_name(join_path(join_path(mount, path), *name)));
        }
        steps = 0;
        for (uint32_t c = dirs[d].child; c != none && c < dirs.size() && steps++ <= dirs.size(); c = dirs[c].sibling) {
            const auto* name = str(dirs[c].name);
            stack.emplace_back(c, join_path(path, name ? *name : std::string()));
        }
    }
    return r;
}

// CSS recognises its outfits by MortalShell2/Content/CSS/Packages/<id>/manifest.json in
// the .pak (native/src/packages.cpp). The index sits between IndexOffset and the
// footer for both the V8 flat index and the V10+ full directory index, so a byte
// search over that range finds the entry without parsing either layout.
bool pak_has_css_manifest(const fs::path& pak, uint64_t size) {
    if (size < 64) return false;
    std::ifstream f(pak, std::ios::binary);
    if (!f) return false;
    const uint64_t tail_len = std::min<uint64_t>(size, 512);
    std::vector<unsigned char> tail(tail_len);
    f.seekg(std::streamoff(size - tail_len));
    f.read(reinterpret_cast<char*>(tail.data()), std::streamsize(tail_len));
    if (!f) return false;
    // FPakInfo: ... bEncryptedIndex(1) Magic(4) Version(4) IndexOffset(8) IndexSize(8) ...
    for (size_t i = tail_len - 4; i-- > 1;) {
        if (le32(&tail[i]) != 0x5A6F12E1u) continue;
        if (i + 4 + 4 + 16 > tail_len) continue;
        if (tail[i - 1] != 0) return false;  // encrypted index: unknown, not CSS-made
        const uint64_t offset = le64(&tail[i + 8]), index = le64(&tail[i + 16]);
        const uint64_t footer_start = size - tail_len + (i - 1);
        if (offset >= footer_start || index == 0 || index > footer_start - offset) return false;
        const uint64_t span = std::min<uint64_t>(footer_start - offset, 64ull << 20);
        std::string bytes(size_t(span), '\0');
        f.clear();
        f.seekg(std::streamoff(offset));
        f.read(bytes.data(), std::streamsize(span));
        if (!f) return false;
        return bytes.find("Content/CSS/Packages/") != std::string::npos && bytes.find("manifest.json") != std::string::npos;
    }
    return false;
}

// "/Game/Sparta/Characters/Shells/KnightLady/Art/SK_X" -> "Characters/Shells/KnightLady".
// Asset-type folders (Art, Mesh, Textures...) say nothing about what the mod touches, so
// the hint stops before them.
std::string override_hint(const std::string& name) {
    static const std::set<std::string> generic = {"art", "mesh", "meshes", "materials", "material", "textures", "texture",
        "tex", "animations", "animation", "anims", "anim", "blueprints", "blueprint", "bp", "fx", "vfx", "audio", "sounds",
        "data", "physics", "rig", "rigs", "skeleton", "skeletons", "mat", "mats", "instances"};
    std::string rest = name;
    if (rest.starts_with("/Game/")) rest.erase(0, 6);
    else if (rest.starts_with("/")) rest.erase(0, 1);
    if (istarts(rest, "Sparta/")) rest.erase(0, 7);
    const auto slash = rest.rfind('/');
    if (slash == std::string::npos) return "(root)";
    rest.resize(slash);
    std::string hint;
    size_t start = 0, parts = 0;
    while (start <= rest.size() && parts < 3) {
        const auto end = std::min(rest.find('/', start), rest.size());
        const std::string part = rest.substr(start, end - start);
        if (part.empty() || (parts > 0 && generic.contains(lower(part)))) break;
        hint += (hint.empty() ? "" : "/") + part;
        ++parts;
        start = end + 1;
    }
    return hint.empty() ? "(root)" : hint;
}

struct Found {
    fs::path folder;
    std::string stem;
    fs::path pak, utoc, ucas;
    uint64_t pak_bytes = 0, utoc_bytes = 0, ucas_bytes = 0;
    FileTime when{};
    int depth = 0;         // directories below Paks/
    std::string top;       // first folder under Paks/, "" when directly inside
};

}  // namespace

PakInfo scan_paks(const Paths& paths, const Progress& progress) {
    PakInfo info;
    try {
        const fs::path root = paths.paks;
        std::error_code ec;
        if (root.empty() || !fs::is_directory(root, ec)) {
            info.error = "Paks folder not found: " + path_utf8(root);
            return info;
        }
        if (progress) progress("Listing containers in Paks");
        std::map<std::string, Found> found;  // key: lower(relative folder) + '|' + lower(stem)
        std::vector<std::pair<fs::path, int>> dirs{{root, 0}};
        size_t file_count = 0;
        while (!dirs.empty() && file_count < max_files) {
            auto [dir, depth] = dirs.back();
            dirs.pop_back();
            fs::directory_iterator it(dir, ec), end;
            if (ec) { if (info.error.empty()) info.error = "cannot list " + path_utf8(dir) + ": " + ec.message(); ec.clear(); continue; }
            for (; it != end && file_count < max_files; it.increment(ec)) {
                if (ec) break;
                const fs::path p = it->path();
                const auto st = it->symlink_status(ec);
                if (ec) { ec.clear(); continue; }
                const fs::path rel = p.lexically_relative(root);
                const std::string top = depth == 0 ? std::string() : path_utf8(*rel.begin());
                const bool in_mod_folder = iequals(top, "~mods") || iequals(top, "LogicMods");
                if (fs::is_directory(st)) {
                    if (depth < 3) dirs.emplace_back(p, depth + 1);
                    continue;
                }
                if (!fs::is_regular_file(p, ec)) { ec.clear(); continue; }
                ++file_count;
                const std::string ext = lower(path_utf8(p.extension()));
                if (ext != ".pak" && ext != ".utoc" && ext != ".ucas") {
                    if (in_mod_folder && info.stray_files.size() < max_stray) info.stray_files.push_back(generic_utf8(rel));
                    continue;
                }
                const std::string stem = path_utf8(p.stem());
                const std::string key = lower(generic_utf8(p.parent_path().lexically_relative(root))) + "|" + lower(stem);
                auto& c = found[key];
                c.folder = p.parent_path();
                c.stem = stem;
                c.depth = depth;
                c.top = top;
                const uint64_t bytes = fs::file_size(p, ec);
                if (ec) ec.clear();
                const auto when = fs::last_write_time(p, ec);
                // FileTime{} is not "the past" on every library (libstdc++ puts it in 2174),
                // so an unset value is tested for explicitly.
                if (!ec && (c.when == FileTime{} || when > c.when)) c.when = when;
                ec.clear();
                if (ext == ".pak") { c.pak = p; c.pak_bytes = bytes; }
                else if (ext == ".utoc") { c.utoc = p; c.utoc_bytes = bytes; }
                else { c.ucas = p; c.ucas_bytes = bytes; }
            }
            ec.clear();
        }
        if (file_count >= max_files && info.error.empty()) info.error = "stopped listing after " + std::to_string(max_files) + " files";

        std::unordered_set<uint64_t> game_ids;
        std::unordered_map<uint64_t, std::string> game_names;
        std::vector<std::unordered_map<uint64_t, std::string>> own_names;  // parallel to containers, mods only
        // Game containers first so the baseline exists before mods are compared.
        std::vector<Found*> order;
        for (auto& [key, c] : found) order.push_back(&c);
        auto game_container = [](const Found& c) {
            return c.depth == 0 && (istarts(c.stem, "pakchunk") || iequals(c.stem, "global"));
        };
        std::stable_partition(order.begin(), order.end(), [&](const Found* c) { return game_container(*c); });

        for (const Found* src : order) {
            const Found& c = *src;
            if (progress) progress("Reading " + c.stem);
            PakContainer pc;
            pc.stem = c.stem;
            pc.folder = c.folder;
            pc.pak = c.pak; pc.utoc = c.utoc; pc.ucas = c.ucas;
            pc.pak_bytes = c.pak_bytes; pc.utoc_bytes = c.utoc_bytes; pc.ucas_bytes = c.ucas_bytes;
            pc.when = c.when;
            pc.is_patch = patch_stem(c.stem);
            pc.is_game = game_container(c);
            const bool in_mod_folder = iequals(c.top, "~mods") || iequals(c.top, "LogicMods");
            pc.is_mod = !pc.is_game && (in_mod_folder || pc.is_patch);
            if (pc.is_patch && !in_mod_folder) {
                const auto rel = c.folder.lexically_relative(paths.paks);
                info.mods_outside_mods_folder.push_back(c.depth == 0 ? c.stem : generic_utf8(rel / c.stem));
            }
            std::vector<std::string> problems;
            auto fail = [&](std::string why) { pc.complete = false; problems.push_back(std::move(why)); };
            const bool has_pak = !c.pak.empty(), has_utoc = !c.utoc.empty(), has_ucas = !c.ucas.empty();
            if (has_utoc && !has_ucas) fail("utoc without ucas");
            if (has_ucas && !has_utoc) fail("ucas without utoc");
            if (!has_pak && (has_utoc || has_ucas)) fail("no .pak beside the utoc/ucas, the engine never mounts it");
            if (has_pak && !has_utoc && !has_ucas && !pc.is_game) problems.push_back("pak only, no cooked assets");
            if (has_pak && c.pak_bytes == 0) fail("zero bytes: .pak");
            if (has_utoc && c.utoc_bytes == 0) fail("zero bytes: .utoc");
            if (has_ucas && c.ucas_bytes == 0) fail("zero bytes: .ucas");

            std::unordered_map<uint64_t, std::string> names_here;
            if (has_utoc && c.utoc_bytes > 0) {
                TocRead t = read_toc(c.utoc, c.utoc_bytes);
                if (!t.error.empty()) fail("unreadable table: " + t.error);
                else {
                    pc.chunks = t.chunks;
                    pc.package_ids = std::move(t.package_ids);
                    if (t.index_encrypted) problems.push_back("encrypted container, package names unreadable");
                    if (!t.index_error.empty()) problems.push_back("directory index: " + t.index_error);
                    if (has_ucas && t.partitions == 1 && c.ucas_bytes < t.needed_ucas)
                        fail("ucas truncated: " + std::to_string(c.ucas_bytes) + " bytes, table needs " + std::to_string(t.needed_ucas));
                    std::sort(t.named.begin(), t.named.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
                    for (auto& [id, name] : t.named) {
                        if (name.starts_with("/Game/CSSX/")) pc.cssx_package = true;
                        if (pc.names.size() < max_names_per_container) pc.names.push_back(name);
                        if (pc.is_game) game_names.emplace(id, name);
                        else names_here.emplace(id, std::move(name));
                    }
                }
            }
            if (pc.is_game) game_ids.insert(pc.package_ids.begin(), pc.package_ids.end());
            else if (has_pak && c.pak_bytes > 0) pc.css_package = pak_has_css_manifest(c.pak, c.pak_bytes);

            if (!pc.is_game) {
                std::set<std::string> hints;
                for (const auto id : pc.package_ids) {
                    if (!game_ids.contains(id)) continue;
                    ++pc.overrides_game;
                    const std::string* name = nullptr;
                    if (auto g = game_names.find(id); g != game_names.end()) name = &g->second;
                    else if (auto o = names_here.find(id); o != names_here.end()) name = &o->second;
                    if (!name) continue;
                    if (pc.override_names.size() < max_override_names) pc.override_names.push_back(*name);
                    if (hints.size() < max_override_hints) hints.insert(override_hint(*name));
                }
                std::sort(pc.override_names.begin(), pc.override_names.end());
                pc.override_hints.assign(hints.begin(), hints.end());
            }
            for (size_t i = 0; i < problems.size(); ++i) pc.problem += (i ? "; " : "") + problems[i];
            info.containers.push_back(std::move(pc));
            own_names.push_back(std::move(names_here));
        }
        info.game_package_ids = game_ids.size();

        // Same package id in two or more mod containers: only one of them wins at load.
        std::unordered_map<uint64_t, std::vector<size_t>> carriers;
        for (size_t i = 0; i < info.containers.size(); ++i) {
            if (!info.containers[i].is_mod) continue;
            for (const auto id : info.containers[i].package_ids) carriers[id].push_back(i);
        }
        for (auto& [id, list] : carriers) {
            if (list.size() < 2) continue;
            PakCollision col;
            col.package_id = id;
            if (auto g = game_names.find(id); g != game_names.end()) col.name = g->second;
            for (const auto i : list) {
                col.containers.push_back(info.containers[i].stem);
                if (col.name.empty())
                    if (auto o = own_names[i].find(id); o != own_names[i].end()) col.name = o->second;
            }
            info.mod_collisions.push_back(std::move(col));
        }
        std::sort(info.mod_collisions.begin(), info.mod_collisions.end(), [](const PakCollision& a, const PakCollision& b) {
            if (a.containers != b.containers) return a.containers < b.containers;
            return a.name != b.name ? a.name < b.name : a.package_id < b.package_id;
        });
        if (info.mod_collisions.size() > max_collisions) info.mod_collisions.resize(max_collisions);
        std::sort(info.stray_files.begin(), info.stray_files.end());
    } catch (const std::exception& e) {
        info.error = std::string("paks scan failed: ") + e.what();
    } catch (...) {
        info.error = "paks scan failed";
    }
    return info;
}

}  // namespace wcmg

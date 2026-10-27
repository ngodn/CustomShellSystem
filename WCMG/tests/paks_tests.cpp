#include "check.hpp"
#include "scanners.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>

namespace fs = std::filesystem;
using namespace wcmg;

namespace {
void put32(std::string& b, uint32_t v) { for (int i = 0; i < 4; ++i) b += char(v >> (8 * i)); }
void put64(std::string& b, uint64_t v) { for (int i = 0; i < 8; ++i) b += char(v >> (8 * i)); }
void put_fstring(std::string& b, const std::string& s) { put32(b, uint32_t(s.size() + 1)); b += s; b += '\0'; }

struct SynthChunk { std::string path; uint64_t id; uint8_t type = 1; };

// The directory index the way FIoDirectoryIndexWriter lays it out: root entry 0,
// directories created on demand, files prepended to their directory's list.
std::string directory_index(const std::string& mount, const std::vector<SynthChunk>& chunks) {
    struct Dir { uint32_t name = ~0u, child = ~0u, sibling = ~0u, file = ~0u; };
    struct File { uint32_t name = ~0u, next = ~0u, user = 0; };
    std::vector<Dir> dirs(1);
    std::vector<File> files;
    std::vector<std::string> strings;
    auto name_index = [&](const std::string& s) {
        auto it = std::find(strings.begin(), strings.end(), s);
        if (it != strings.end()) return uint32_t(it - strings.begin());
        strings.push_back(s);
        return uint32_t(strings.size() - 1);
    };
    for (uint32_t ci = 0; ci < chunks.size(); ++ci) {
        std::string rel = chunks[ci].path.substr(mount.size());
        uint32_t dir = 0;
        size_t slash;
        while ((slash = rel.find('/')) != std::string::npos) {
            const uint32_t n = name_index(rel.substr(0, slash));
            uint32_t c = dirs[dir].child, found = ~0u;
            for (; c != ~0u; c = dirs[c].sibling) if (dirs[c].name == n) { found = c; break; }
            if (found == ~0u) {
                Dir d; d.name = n; d.sibling = dirs[dir].child;
                dirs.push_back(d);
                found = uint32_t(dirs.size() - 1);
                dirs[dir].child = found;
            }
            dir = found;
            rel.erase(0, slash + 1);
        }
        File f; f.name = name_index(rel); f.next = dirs[dir].file; f.user = ci;
        files.push_back(f);
        dirs[dir].file = uint32_t(files.size() - 1);
    }
    std::string b;
    put_fstring(b, mount);
    put32(b, uint32_t(dirs.size()));
    for (auto& d : dirs) { put32(b, d.name); put32(b, d.child); put32(b, d.sibling); put32(b, d.file); }
    put32(b, uint32_t(files.size()));
    for (auto& f : files) { put32(b, f.name); put32(b, f.next); put32(b, f.user); }
    put32(b, uint32_t(strings.size()));
    for (auto& s : strings) put_fstring(b, s);
    return b;
}

void write_file(const fs::path& p, const std::string& bytes) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), std::streamsize(bytes.size()));
}

std::string utoc_bytes(const std::vector<SynthChunk>& chunks, uint32_t block_end, uint8_t flags = 0x08,
                       const std::string& mount = "../../../") {
    const std::string index = (flags & 0x08) ? directory_index(mount, chunks) : std::string();
    std::string b = "-==--==--==--==-";
    b += char(8); b += char(0); b += char(0); b += char(0);
    put32(b, 144);                          // TocHeaderSize
    put32(b, uint32_t(chunks.size()));      // TocEntryCount
    put32(b, 1);                            // TocCompressedBlockEntryCount
    put32(b, 12);                           // TocCompressedBlockEntrySize
    put32(b, 0); put32(b, 32);              // compression method count/length
    put32(b, 65536);                        // CompressionBlockSize
    put32(b, uint32_t(index.size()));       // DirectoryIndexSize
    put32(b, 1);                            // PartitionCount
    put64(b, 0x1234);                       // ContainerId
    b += std::string(16, '\0');             // EncryptionKeyGuid
    b += char(flags); b += char(0); b += char(0); b += char(0);
    put32(b, 0);                            // perfect hash seeds
    put64(b, ~0ull);                        // PartitionSize
    put32(b, 0); put32(b, 0);               // chunks without perfect hash, reserved
    b += std::string(40, '\0');
    for (auto& c : chunks) { put64(b, c.id); b += char(0); b += char(0); b += char(0); b += char(c.type); }
    b += std::string(10 * chunks.size(), '\0');
    // One compression block: 5-byte offset 0, 3-byte compressed size, 3-byte uncompressed size + method.
    put64(b, uint64_t(block_end & 0xFFFFFF) << 40);
    put32(b, block_end & 0xFFFFFF);
    b += index;
    return b;
}

// A V8B pak whose flat index names the given files; only the footer and index matter here.
std::string pak_bytes(const std::vector<std::string>& files) {
    std::string b(64, 'x');
    const uint64_t offset = b.size();
    std::string index;
    put_fstring(index, "../../../");
    put32(index, uint32_t(files.size()));
    for (auto& f : files) {
        put_fstring(index, f);
        put64(index, 0); put64(index, 4); put64(index, 4); put32(index, 0);
        index += std::string(20, '\0'); index += char(0); put32(index, 0);
    }
    b += index;
    b += std::string(16, '\0'); b += char(0);
    put32(b, 0x5A6F12E1); put32(b, 8); put64(b, offset); put64(b, index.size());
    b += std::string(20, '\0'); b += std::string(160, '\0');
    return b;
}

void mod(const fs::path& dir, const std::string& stem, const std::vector<SynthChunk>& chunks, bool css = false,
         uint32_t ucas = 4096, uint32_t needed = 4096, uint8_t flags = 0x08) {
    write_file(dir / (stem + ".pak"), pak_bytes(css ? std::vector<std::string>{"MortalShell2/Content/CSS/Packages/test/manifest.json"}
                                                    : std::vector<std::string>{"MortalShell2/Content/Other/readme.txt"}));
    write_file(dir / (stem + ".utoc"), utoc_bytes(chunks, needed, flags));
    write_file(dir / (stem + ".ucas"), std::string(ucas, '\0'));
}

const PakContainer* find(const PakInfo& info, const std::string& stem) {
    for (auto& c : info.containers) if (c.stem == stem) return &c;
    return nullptr;
}
bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

void synthetic() {
    const fs::path base = wcmg_test::scratch_dir() / "paks";
    fs::remove_all(base);
    const fs::path paks = base / "MortalShell2/Content/Paks";
    const std::string g = "../../../MortalShell2/Content/Sparta/";
    const uint64_t A = 0xA1, B = 0xB2, C = 0xC3, S = 0x55, X = 0x99, Y = 0x77;
    (void)C;
    mod(paks, "pakchunk0-Windows", {{g + "Characters/Shells/KnightLady/Art/SK_KnightLady.uasset", A},
                                    {g + "Characters/Shells/KnightLady/Art/SK_KnightLady.ubulk", A, 2},
                                    {g + "UI/WBP_HUD.uasset", B}, {g + "Core/Characters/Player/BP_Player.uasset", C}});
    write_file(paks / "global.utoc", utoc_bytes({{"../../../global", 1, 5}}, 16, 0));
    write_file(paks / "global.ucas", std::string(16, '\0'));
    const fs::path m = paks / "~mods";
    mod(m / "ModA_P", "ModA_P", {{g + "Characters/Shells/KnightLady/Art/SK_KnightLady.uasset", A},
                                 {"../../../MortalShell2/Content/ModA/Foo.uasset", X},
                                 {"../../../MortalShell2/Content/Shared/Thing.uasset", S}}, true);
    mod(m / "ModB_P", "ModB_P", {{"../../../MortalShell2/Content/Shared/Thing.uasset", S},
                                 {g + "UI/WBP_HUD.uasset", B}});
    mod(m / "CinePack_P", "CinePack_P", {{"../../../MortalShell2/Content/CSSX/CINE/MPC_Cine.uasset", Y}});
    // utoc without ucas
    write_file(m / "Broken_P/Broken_P.pak", pak_bytes({}));
    write_file(m / "Broken_P/Broken_P.utoc", utoc_bytes({{"../../../MortalShell2/Content/B/X.uasset", 0x1001}}, 16));
    // legacy pak only
    write_file(m / "Legacy_P/Legacy_P.pak", pak_bytes({"MortalShell2/Content/Legacy/A.uasset"}));
    // garbage table
    write_file(m / "Bad_P/Bad_P.pak", pak_bytes({}));
    write_file(m / "Bad_P/Bad_P.utoc", std::string(200, 'z'));
    write_file(m / "Bad_P/Bad_P.ucas", std::string(16, '\0'));
    // ucas shorter than the table says
    mod(m / "Trunc_P", "Trunc_P", {{"../../../MortalShell2/Content/T/A.uasset", 0x2001}}, false, 100, 5000);
    // encrypted index
    mod(m / "Enc_P", "Enc_P", {{"../../../MortalShell2/Content/E/A.uasset", 0x3001}}, false, 4096, 4096, 0x0A);
    // zero-byte pak beside a good pair
    mod(m / "Zero_P", "Zero_P", {{"../../../MortalShell2/Content/Z/A.uasset", 0x4001}});
    write_file(m / "Zero_P/Zero_P.pak", "");
    // utoc/ucas with no pak
    write_file(m / "NoPak_P/NoPak_P.utoc", utoc_bytes({{"../../../MortalShell2/Content/N/A.uasset", 0x5001}}, 16));
    write_file(m / "NoPak_P/NoPak_P.ucas", std::string(16, '\0'));
    // strays and a mod outside ~mods
    write_file(m / "readme.txt", "hi");
    write_file(m / "ModA_P/ModA_P.zip", "PK");
    write_file(paks / "LogicMods/notes.sig", "s");
    mod(paks, "Loose_P", {{"../../../MortalShell2/Content/L/A.uasset", 0x6001}});
    mod(paks / "backup", "Old_P", {{"../../../MortalShell2/Content/O/A.uasset", 0x7001}});

    Paths p;
    p.paks = paks;
    int calls = 0;
    const PakInfo info = scan_paks(p, [&](const std::string&) { ++calls; });
    CHECK(info.error.empty());
    CHECK(calls >= 10);
    CHECK_EQ(info.game_package_ids, size_t(3));  // global carries no export bundles

    const auto* game = find(info, "pakchunk0-Windows");
    CHECK(game && game->is_game && !game->is_mod && !game->is_patch && game->complete);
    CHECK(game && game->chunks == 4 && game->package_ids.size() == 3);
    CHECK(game && has(game->names, "/Game/Sparta/UI/WBP_HUD"));
    const auto* global = find(info, "global");
    CHECK(global && global->is_game && global->package_ids.empty());

    const auto* a = find(info, "ModA_P");
    CHECK(a && a->is_mod && a->is_patch && a->complete && a->problem.empty());
    CHECK(a && a->css_package && !a->cssx_package);
    CHECK(a && std::is_sorted(a->package_ids.begin(), a->package_ids.end()) && a->package_ids.size() == 3);
    CHECK(a && a->overrides_game == 1);
    CHECK(a && has(a->override_names, "/Game/Sparta/Characters/Shells/KnightLady/Art/SK_KnightLady"));
    CHECK(a && has(a->override_hints, "Characters/Shells/KnightLady"));
    CHECK(a && has(a->names, "/Game/ModA/Foo"));
    const auto* b = find(info, "ModB_P");
    CHECK(b && !b->css_package && b->overrides_game == 1 && has(b->override_hints, "UI"));
    const auto* cine = find(info, "CinePack_P");
    CHECK(cine && cine->cssx_package && !cine->css_package);

    CHECK_EQ(info.mod_collisions.size(), size_t(1));
    if (!info.mod_collisions.empty()) {
        CHECK_EQ(info.mod_collisions[0].package_id, S);
        CHECK_EQ(info.mod_collisions[0].name, std::string("/Game/Shared/Thing"));
        CHECK(info.mod_collisions[0].containers == std::vector<std::string>({"ModA_P", "ModB_P"}));
    }

    const auto* broken = find(info, "Broken_P");
    CHECK(broken && !broken->complete && broken->problem.find("utoc without ucas") != std::string::npos);
    CHECK(broken && broken->package_ids.size() == 1);  // the table still reads
    const auto* legacy = find(info, "Legacy_P");
    CHECK(legacy && legacy->complete && legacy->problem == "pak only, no cooked assets");
    const auto* bad = find(info, "Bad_P");
    CHECK(bad && !bad->complete && bad->problem.find("unreadable table: bad magic") != std::string::npos);
    const auto* trunc = find(info, "Trunc_P");
    CHECK(trunc && !trunc->complete && trunc->problem.find("ucas truncated") != std::string::npos);
    const auto* enc = find(info, "Enc_P");
    CHECK(enc && enc->complete && enc->problem.find("encrypted") != std::string::npos && enc->names.empty());
    CHECK(enc && enc->package_ids.size() == 1);
    const auto* zero = find(info, "Zero_P");
    CHECK(zero && !zero->complete && zero->problem.find("zero bytes: .pak") != std::string::npos);
    const auto* nopak = find(info, "NoPak_P");
    CHECK(nopak && !nopak->complete && nopak->problem.find("no .pak") != std::string::npos);

    CHECK(has(info.stray_files, "~mods/readme.txt"));
    CHECK(has(info.stray_files, "~mods/ModA_P/ModA_P.zip"));
    CHECK(has(info.stray_files, "LogicMods/notes.sig"));
    CHECK_EQ(info.stray_files.size(), size_t(3));
    CHECK(has(info.mods_outside_mods_folder, "Loose_P"));
    CHECK(has(info.mods_outside_mods_folder, "backup/Old_P"));
    const auto* loose = find(info, "Loose_P");
    CHECK(loose && loose->is_mod && !loose->is_game);

    Paths none;
    none.paks = base / "nope";
    CHECK(!scan_paks(none).error.empty());
}

void real() {
    auto dir = wcmg_test::env_path("WCMG_GAME_DIR");
    if (!dir) { std::printf("paks: WCMG_GAME_DIR not set, real-data checks skipped\n"); return; }
    Paths p;
    p.game_root = *dir;
    p.project = *dir / "MortalShell2";
    p.paks = p.project / "Content/Paks";
    using clock = std::chrono::steady_clock;
    std::vector<std::pair<std::string, clock::time_point>> marks;
    const auto t0 = clock::now();
    const PakInfo info = scan_paks(p, [&](const std::string& s) { marks.emplace_back(s, clock::now()); });
    const auto t1 = clock::now();
    marks.emplace_back("end", t1);
    std::map<std::string, double> ms;
    for (size_t i = 0; i + 1 < marks.size(); ++i)
        if (marks[i].first.starts_with("Reading "))
            ms[marks[i].first.substr(8)] = std::chrono::duration<double, std::milli>(marks[i + 1].second - marks[i].second).count();

    CHECK(info.error.empty());
    if (!info.error.empty()) std::printf("error: %s\n", info.error.c_str());
    size_t mods = 0, games = 0;
    std::printf("\n%-40s %7s %6s %6s %6s %4s %4s  %s\n", "mod container", "chunks", "pkgs", "names", "ovr", "css", "cssx", "hints / problem");
    for (auto& c : info.containers) {
        if (c.is_game) { ++games; continue; }
        ++mods;
        std::string hints;
        for (auto& h : c.override_hints) hints += (hints.empty() ? "" : ", ") + h;
        std::printf("%-40s %7u %6zu %6zu %6u %4s %4s  %s%s%s\n", c.stem.c_str(), c.chunks, c.package_ids.size(), c.names.size(),
                    c.overrides_game, c.css_package ? "yes" : "", c.cssx_package ? "yes" : "", hints.c_str(),
                    c.problem.empty() ? "" : "  PROBLEM: ", c.problem.c_str());
        CHECK(c.complete);
        CHECK(c.chunks > 0 && !c.package_ids.empty() && !c.names.empty());
        if (c.stem.starts_with("CSSX_")) CHECK(c.cssx_package);
    }
    std::vector<const PakContainer*> big;
    for (auto& c : info.containers) if (c.is_game) big.push_back(&c);
    std::sort(big.begin(), big.end(), [](auto* a, auto* b) { return a->chunks > b->chunks; });
    std::printf("\ngame containers: %zu, game package ids: %zu, mod containers: %zu, collisions: %zu, strays: %zu, outside ~mods: %zu\n",
                games, info.game_package_ids, mods, info.mod_collisions.size(), info.stray_files.size(), info.mods_outside_mods_folder.size());
    for (size_t i = 0; i < big.size() && i < 3; ++i)
        std::printf("  %-22s chunks %6u packages %6zu names %6zu  parse %.1f ms\n", big[i]->stem.c_str(), big[i]->chunks,
                    big[i]->package_ids.size(), big[i]->names.size(), ms[big[i]->stem]);
    for (size_t i = 0; i < info.mod_collisions.size() && i < 15; ++i) {
        auto& col = info.mod_collisions[i];
        std::string who;
        for (auto& s : col.containers) who += (who.empty() ? "" : " + ") + s;
        std::printf("  collision %016llx %s  [%s]\n", (unsigned long long)col.package_id, col.name.c_str(), who.c_str());
    }
    std::printf("total scan %.0f ms\n", std::chrono::duration<double, std::milli>(t1 - t0).count());
    CHECK(games >= 8);
    CHECK(info.game_package_ids > 50000);
    CHECK(mods >= 30);
}
}  // namespace

int main() {
    synthetic();
    real();
    return wcmg_test::finish("paks");
}

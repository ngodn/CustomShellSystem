#include "check.hpp"
#include "scanners.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>

namespace fs = std::filesystem;
using namespace wcmg;

namespace {
void write_file(const fs::path& p, const std::string& bytes) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), std::streamsize(bytes.size()));
}
bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

void synthetic() {
    const fs::path base = wcmg_test::scratch_dir() / "manifest";
    fs::remove_all(base);
    const fs::path sparta = base / "steamapps/common/Sparta";
    const std::string ts = "\t2026-10-22T04:02:23.845Z\r\n";
    write_file(sparta / "Manifest_UFSFiles_Win64.txt",
               "Engine/Content/Slate/Packed.png" + ts +            // inside a container, never loose
               "MortalShell2/Content/Paks/pakchunk0-Windows.pak" + ts +
               "MortalShell2/Content/Paks/pakchunk1-Windows.pak" + ts);  // shipped but deleted
    write_file(sparta / "Manifest_NonUFSFiles_Win64.txt",
               "MortalShell2/Binaries/Win64/MortalShell2-Win64-Shipping.exe" + ts +
               "Engine/Binaries/Win64/tbb12.dll" + ts +             // missing
               "MortalShell2/Content/Movies/Intro.mp4" + ts);
    write_file(sparta / "Manifest_DebugFiles_Win64.txt", "MortalShell2/Binaries/Win64/MortalShell2-Win64-Shipping.pdb" + ts);
    write_file(sparta / "MortalShell2/Binaries/Win64/MortalShell2-Win64-Shipping.exe", "hello");
    write_file(sparta / "MortalShell2/Content/Movies/Intro.mp4", "m");
    write_file(sparta / "MortalShell2/Binaries/Win64/dwmapi.dll", "proxy");             // foreign
    write_file(sparta / "MortalShell2/Binaries/Win64/ue4ss/UE4SS.dll", "x");            // mod location
    write_file(sparta / "MortalShell2/Binaries/Win64/ue4ss/Mods/A/dlls/main.dll", "x"); // mod location
    write_file(sparta / "MortalShell2/Binaries/Win64/vkd3d-proton.cache", "c");
    write_file(sparta / "MortalShell2/Binaries/Win64/Other.pdb", "p");
    write_file(sparta / "MortalShell2/Binaries/Win64/Backup/old.dll", "b");             // foreign, deeper
    write_file(sparta / "MortalShell2/Content/Paks/pakchunk0-Windows.pak", "p");
    write_file(sparta / "MortalShell2/Content/Paks/pakchunk0-Windows.utoc", "p");       // game, not listed
    write_file(sparta / "MortalShell2/Content/Paks/pakchunk0-Windows.ucas", "p");
    write_file(sparta / "MortalShell2/Content/Paks/global.utoc", "g");
    write_file(sparta / "MortalShell2/Content/Paks/global.ucas", "g");
    write_file(sparta / "MortalShell2/Content/Paks/pakchunk9-Windows.utoc", "x");       // foreign: not shipped
    write_file(sparta / "MortalShell2/Content/Paks/Loose_P.pak", "x");                  // foreign
    write_file(sparta / "MortalShell2/Content/Paks/~mods/A_P/A_P.pak", "x");
    write_file(sparta / "MortalShell2/Content/Paks/LogicMods/B.pak", "x");
    write_file(sparta / "Engine/Binaries/Win64/d3d11.dll", "x");                        // foreign
    write_file(base / "steamapps/appmanifest_2584270.acf",
               "\"AppState\"\n{\n\t\"appid\"\t\t\"2584270\"\n\t\"LastUpdated\"\t\t\"1792723736\"\n\t\"buildid\"\t\t\"26147827\"\n"
               "\t\"InstalledDepots\"\n\t{\n\t\t\"2584271\"\n\t\t{\n\t\t\t\"manifest\"\t\t\"123\"\n\t\t}\n\t}\n}\n");

    Paths p;
    p.game_root = sparta;
    p.project = sparta / "MortalShell2";
    int calls = 0;
    const ManifestInfo m = scan_manifest(p, [&](const std::string&) { ++calls; });
    CHECK(calls >= 3);
    CHECK(m.present);
    CHECK_EQ(m.manifest_entries, size_t(6));
    CHECK_EQ(m.missing_total, size_t(2));
    CHECK(has(m.missing, "Engine/Binaries/Win64/tbb12.dll"));
    CHECK(has(m.missing, "MortalShell2/Content/Paks/pakchunk1-Windows.pak"));
    CHECK(!has(m.missing, "Engine/Content/Slate/Packed.png"));
    CHECK_EQ(m.foreign_total, size_t(5));
    CHECK(has(m.foreign, "MortalShell2/Binaries/Win64/dwmapi.dll"));
    CHECK(has(m.foreign, "MortalShell2/Binaries/Win64/Backup/old.dll"));
    CHECK(has(m.foreign, "MortalShell2/Content/Paks/pakchunk9-Windows.utoc"));
    CHECK(has(m.foreign, "MortalShell2/Content/Paks/Loose_P.pak"));
    CHECK(has(m.foreign, "Engine/Binaries/Win64/d3d11.dll"));
    // shallow first
    if (m.foreign.size() == 5) CHECK_EQ(m.foreign.back(), std::string("MortalShell2/Binaries/Win64/Backup/old.dll"));
    CHECK(m.replaced_binaries.empty());
    CHECK_EQ(m.steam_build_id, std::string("26147827"));
    CHECK_EQ(m.steam_last_updated, std::string("1792723736"));
    CHECK_EQ(m.exe_sha256, std::string("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"));
    CHECK_EQ(m.exe_bytes, uint64_t(5));

    // An explicit app manifest path wins over the guess.
    write_file(base / "other.acf", "\"AppState\"\n{\n\t\"buildid\"\t\t\"42\"\n}\n");
    p.steam_app_manifest = base / "other.acf";
    CHECK_EQ(scan_manifest(p).steam_build_id, std::string("42"));

    Paths none;
    none.game_root = base / "nope";
    const ManifestInfo n = scan_manifest(none);
    CHECK(!n.present && n.manifest_entries == 0 && n.exe_sha256.empty());
}

void real() {
    auto dir = wcmg_test::env_path("WCMG_GAME_DIR");
    if (!dir) { std::printf("manifest: WCMG_GAME_DIR not set, real-data checks skipped\n"); return; }
    Paths p;
    p.game_root = *dir;
    p.project = *dir / "MortalShell2";
    p.win64 = p.project / "Binaries/Win64";
    const auto t0 = std::chrono::steady_clock::now();
    const ManifestInfo m = scan_manifest(p);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("\nmanifest: present %d, entries %zu, missing %zu, foreign %zu, build %s, updated %s, exe %llu bytes sha256 %s (%.0f ms)\n",
                int(m.present), m.manifest_entries, m.missing_total, m.foreign_total, m.steam_build_id.c_str(),
                m.steam_last_updated.c_str(), (unsigned long long)m.exe_bytes, m.exe_sha256.c_str(), ms);
    for (auto& s : m.missing) std::printf("  missing %s\n", s.c_str());
    for (size_t i = 0; i < m.foreign.size() && i < 40; ++i) std::printf("  foreign %s\n", m.foreign[i].c_str());
    CHECK(m.present);
    CHECK(m.manifest_entries > 100000);
    CHECK(!m.steam_build_id.empty());
    CHECK_EQ(m.exe_sha256.size(), size_t(64));
    CHECK(has(m.foreign, "MortalShell2/Binaries/Win64/dwmapi.dll"));  // the UE4SS proxy on this machine
}
}  // namespace

int main() {
    synthetic();
    real();
    return wcmg_test::finish("manifest");
}

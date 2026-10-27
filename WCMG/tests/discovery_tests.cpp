// Discovery: the KeyValues parser, Steam library walking on a fake root, every
// accepted form of a user pick, and (when WCMG_GAME_DIR is set) the real install.
#include "check.hpp"
#include "discovery_private.hpp"
#include "scanners.hpp"
#include <fstream>

namespace fs = std::filesystem;
using namespace wcmg;

static void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

// steamapps/common/<dir>/MortalShell2/Binaries/Win64/<exe> plus an empty ue4ss/Mods.
static fs::path make_fake_game(const fs::path& library, const std::string& dir) {
    auto root = library / "steamapps" / "common" / dir;
    auto win64 = root / "MortalShell2" / "Binaries" / "Win64";
    write(win64 / "MortalShell2-Win64-Shipping.exe", "");
    write(root / "MortalShell2.exe", "");
    fs::create_directories(win64 / "ue4ss" / "Mods" / "SomeMod" / "Scripts");
    fs::create_directories(root / "MortalShell2" / "Content" / "Paks");
    return root;
}

static void test_keyvalues() {
    auto kv = detail::parse_keyvalues(
        "\"A\"\n{\n  // comment\n  \"k\"\t\t\"v\"\n  \"inner\" { \"x\" \"1\" [$WIN32] }\n  bare value\n}\n");
    CHECK_EQ(kv.size(), 3u);
    if (kv.size() == 3) {
        CHECK_EQ(kv[0].keys.size(), 2u);
        CHECK_EQ(kv[0].keys[1], std::string("k"));
        CHECK_EQ(kv[0].value, std::string("v"));
        CHECK_EQ(kv[1].keys.size(), 3u);
        CHECK_EQ(kv[1].keys[1], std::string("inner"));
        CHECK_EQ(kv[1].value, std::string("1"));
        CHECK_EQ(kv[2].keys[1], std::string("bare"));
        CHECK_EQ(kv[2].value, std::string("value"));
    }
    // Odd spacing, braces on the same line, escaped backslashes and quotes.
    auto libs = detail::parse_libraryfolders(
        "\"libraryfolders\"{\"0\"{\"path\"   \"C:\\\\Games\\\\SteamLibrary\" \"label\" \"a \\\"b\\\"\"\n"
        "\"apps\"{ \"2584270\" \"123\" \"10\" \"5\" }}\n"
        "  \"1\"\n  {\n\t\"path\"\t\"D:\\\\Other Lib\"\n  }\n}");
    CHECK_EQ(libs.size(), 2u);
    if (libs.size() == 2) {
        CHECK_EQ(path_utf8(libs[0].path), std::string("C:\\Games\\SteamLibrary"));
        CHECK_EQ(libs[0].apps.size(), 2u);
        CHECK(!libs[0].apps.empty() && libs[0].apps[0] == "2584270");
        CHECK_EQ(path_utf8(libs[1].path), std::string("D:\\Other Lib"));
        CHECK(libs[1].apps.empty());
    }
    // Pre-2021 format: numeric keys map straight to paths.
    auto old = detail::parse_libraryfolders("\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\t\"1600000000\"\n"
                                            "\t\"ContentStatsID\"\t\t\"-123\"\n\t\"1\"\t\t\"E:\\\\SteamLibrary\"\n}\n");
    CHECK_EQ(old.size(), 1u);
    if (!old.empty()) CHECK_EQ(path_utf8(old[0].path), std::string("E:\\SteamLibrary"));
    // Unterminated input must not hang or crash.
    CHECK(detail::parse_keyvalues("\"a\" { \"b\" \"c").size() <= 1);

    CHECK_EQ(detail::acf_value("\"AppState\"\n{\n\t\"appid\"\t\t\"2584270\"\n\t\"installdir\"\t\t\"Sparta\"\n"
                               "\t\"UserConfig\" { \"installdir\" \"nope\" }\n}\n", "InstallDir"),
             std::string("Sparta"));
}

static void test_steam_roots(const fs::path& base) {
    auto steam = base / "steam-root";
    auto lib = base / "library one";
    auto lib2 = base / "library-two";
    fs::create_directories(steam / "steamapps");
    make_fake_game(lib, "Sparta");
    write(lib / "steamapps" / "appmanifest_2584270.acf",
          "\"AppState\"\n{\n\t\"appid\"\t\t\"2584270\"\n\t\"installdir\"\t\t\"Sparta\"\n\t\"buildid\"\t\t\"26147827\"\n}\n");
    // A second library whose acf names a different install dir.
    make_fake_game(lib2, "Mortal Shell II");
    write(lib2 / "steamapps" / "appmanifest_2584270.acf", "\"AppState\" { \"installdir\" \"Mortal Shell II\" }");
    // Steam escapes backslashes on Windows; forward slashes here keep the test portable.
    write(steam / "steamapps" / "libraryfolders.vdf",
          "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + path_utf8(steam) + "\"\n\t\t\"apps\" { \"10\" \"1\" }\n\t}\n"
          "\t\"1\" { \"path\" \"" + path_utf8(lib2) + "\" }\n"
          "\t\"2\"\n\t{\n\t\t\"path\"\t\t\"" + path_utf8(lib) + "\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"2584270\"\t\t\"1\"\n\t\t}\n\t}\n}\n");

    // The same root twice must not duplicate hits.
    auto hits = detail::discover_from_steam_roots({steam, steam});
    CHECK_EQ(hits.size(), 2u);
    if (hits.size() == 2) {
        // The library that lists the app id comes first.
        CHECK(fs::equivalent(hits[0].game_root, lib / "steamapps" / "common" / "Sparta"));
        CHECK(fs::equivalent(hits[0].steam_app_manifest, lib / "steamapps" / "appmanifest_2584270.acf"));
        CHECK(hits[0].how_found.rfind("Steam library ", 0) == 0);
        CHECK(validate_paths(hits[0], nullptr));
        CHECK_EQ(hits[1].game_root.filename().string(), std::string("Mortal Shell II"));
        CHECK(validate_paths(hits[1], nullptr));
        // No Proton prefix exists for the fake install; saved still points somewhere sane.
        CHECK(!hits[0].saved.empty());
    }
    CHECK(detail::discover_from_steam_roots({base / "does-not-exist"}).empty());

    // Proton prefix in the game's own library is picked as Saved.
    auto saved = lib / "steamapps" / "compatdata" / "2584270" / "pfx" / "drive_c" / "users" / "steamuser" / "AppData" /
                 "Local" / "MortalShell2" / "Saved";
    fs::create_directories(saved / "Crashes");
    auto p = paths_from_user_pick(lib / "steamapps" / "common" / "Sparta");
    CHECK(p.has_value());
    if (p) CHECK(fs::equivalent(p->saved, saved));
}

static void test_user_picks(const fs::path& base) {
    auto lib = base / "picklib";
    auto root = make_fake_game(lib, "Sparta");
    auto win64 = root / "MortalShell2" / "Binaries" / "Win64";
    const fs::path picks[] = {
        root,
        fs::path(path_utf8(root) + "/"),  // trailing separator
        root / "MortalShell2",
        root / "MortalShell2" / "Binaries",
        win64,
        win64 / "MortalShell2-Win64-Shipping.exe",
        root / "MortalShell2.exe",
        win64 / "ue4ss",
        win64 / "ue4ss" / "Mods",
        win64 / "ue4ss" / "Mods" / "SomeMod" / "Scripts",
        lib,                               // the library
        lib / "steamapps",
        lib / "steamapps" / "common",
    };
    for (auto& pick : picks) {
        auto p = paths_from_user_pick(pick);
        CHECK(p.has_value());
        if (!p) { std::fprintf(stderr, "  pick was %s\n", path_utf8(pick).c_str()); continue; }
        CHECK(fs::equivalent(p->game_root, root));
        CHECK(fs::equivalent(p->win64, win64));
        CHECK(fs::equivalent(p->ue4ss, win64 / "ue4ss"));
        CHECK(fs::equivalent(p->mods, win64 / "ue4ss" / "Mods"));
        CHECK_EQ(p->paks.filename().string(), std::string("Paks"));
        CHECK_EQ(p->how_found, std::string("user pick"));
        CHECK(validate_paths(*p, nullptr));
    }
    CHECK(!paths_from_user_pick(base / "nothing-here").has_value());
    CHECK(!paths_from_user_pick(fs::path()).has_value());
    fs::create_directories(base / "empty-dir");
    CHECK(!paths_from_user_pick(base / "empty-dir").has_value());

    std::string why;
    Paths bad;
    bad.win64 = base / "empty-dir";
    CHECK(!validate_paths(bad, &why));
    CHECK(why.find("MortalShell2-Win64-Shipping.exe") != std::string::npos);
}

static void test_real_install() {
    auto game = wcmg_test::env_path("WCMG_GAME_DIR");
    if (!game) { std::printf("discovery: WCMG_GAME_DIR not set, real-install checks skipped\n"); return; }
    auto p = paths_from_user_pick(*game);
    CHECK(p.has_value());
    if (!p) return;
    std::string why;
    CHECK(validate_paths(*p, &why));
    CHECK(fs::is_directory(p->saved));
    CHECK(fs::is_regular_file(p->steam_app_manifest));
    CHECK(fs::is_directory(p->mods));
    if (auto want = wcmg_test::env_path("WCMG_SAVED_DIR")) CHECK(fs::equivalent(p->saved, *want));
    std::printf("discovery: real install %s\n  saved %s\n  acf %s\n", path_utf8(p->game_root).c_str(),
                path_utf8(p->saved).c_str(), path_utf8(p->steam_app_manifest).c_str());
    // A Steam root on this machine should also find it.
    auto all = discover_installs();
    bool found = false;
    for (auto& q : all) {
        std::printf("  discovered %s (%s)\n", path_utf8(q.game_root).c_str(), q.how_found.c_str());
        std::error_code ec;
        if (fs::equivalent(q.game_root, p->game_root, ec)) found = true;
    }
    CHECK(found);
}

int main() {
    auto base = wcmg_test::scratch_dir() / "discovery";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base);
    test_keyvalues();
    test_steam_roots(base);
    test_user_picks(base);
    test_real_install();
    return wcmg_test::finish("discovery");
}

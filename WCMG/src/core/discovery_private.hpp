#pragma once
// Internal seams of discovery.cpp, shared with src/win/discovery_win.cpp and the
// tests. Nothing here is part of the public scanner contract.
#include "model.hpp"
#include <string>
#include <utility>
#include <vector>

namespace wcmg::detail {

// A place worth checking that is not a Steam library: a registry uninstall entry,
// an Epic manifest, a guessed drive folder. `how` ends up in Paths::how_found.
struct Candidate {
    fs::path dir;
    std::string how;
};

// Platform hooks. On Windows they come from src/win/discovery_win.cpp; a build
// that does not link that file (wcmg_cli.exe) falls back to empty defaults.
std::vector<fs::path> platform_steam_roots();
std::vector<Candidate> platform_extra_candidates();
bool platform_running_under_wine();

// Valve KeyValues text, flattened: every "key" "value" pair with the chain of
// block names above it. Blocks themselves produce no entry.
struct KvEntry {
    std::vector<std::string> keys;  // outermost first, the value's own key last
    std::string value;
};
std::vector<KvEntry> parse_keyvalues(const std::string& text);

struct SteamLibrary {
    fs::path path;
    std::vector<std::string> apps;   // app ids from the library's "apps" block
};
std::vector<SteamLibrary> parse_libraryfolders(const std::string& vdf_text);

// The value of a top-level key in an appmanifest (installdir, buildid, ...).
std::string acf_value(const std::string& acf_text, const std::string& key);

// Every library of every root, checked for the game. Steam hits only.
std::vector<Paths> discover_from_steam_roots(const std::vector<fs::path>& roots);

// UTF-8 text to a path without going through the ANSI code page on Windows.
fs::path path_from_utf8(const std::string&);
// An environment variable as a path, empty when unset.
fs::path env_path(const char* name);
}

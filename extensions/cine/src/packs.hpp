#pragma once
// CINE packs: each preset group ships as its own UE mod, Content/Paks/~mods/CINE_<Pack>_eins0fx_P,
// holding /Game/CSSX/CINE/<Pack>/DT_CINE_Pack (rows: manifest, preset.<id>, recipe.<id>, style.<id>;
// the JSON text is in each row's DevComment) and its music as SoundWaves under <Pack>/Audio/.
// A folder is only a candidate: a pack counts once the engine loads its table and the manifest
// validates. Paks mount at game start, so new or removed packs need a restart.
#include "content.hpp"
#include <optional>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cine {

using Json = cssx::Json;

struct PackAudio {
    std::string asset;     // full object path of the SoundWave
    double seconds = 0;    // duration, for checks against the preset's timeline
};

struct Pack {
    std::string pack;      // folder token, e.g. "IrisOut"
    std::string name;      // display name, e.g. "Iris Out"
    std::string version, song, default_preset;
    std::vector<std::string> presets;          // display order
    std::map<std::string, PackAudio> audio;    // audio id -> asset
    std::optional<CoreContent> style;          // validated core plus this pack's partial rows
    std::vector<std::string> problems;        // bad optional rows do not discard the scenes
    std::map<std::string, Json> rows;          // every row's parsed JSON, by row name
};

// Pure helpers (host-tested).
constexpr const char* pack_prefix = "CINE_";
constexpr const char* pack_suffix = "_eins0fx_P";
// The pack token of a ~mods folder name, or "" when the name is not a CINE pack folder.
std::string pack_token(const std::string& folder);
// Candidates: ~mods sub-folders named like a pack that hold all three container files.
std::vector<std::string> find_pack_folders(const std::filesystem::path& mods, std::vector<std::string>& problems);
std::string table_path(const std::string& pack);
// Validates a manifest against its folder token; throws std::invalid_argument with the reason.
Pack parse_manifest(const Json& manifest, const std::string& token);

// Engine side: reads one pack through the CSSX bridge. Throws with a reason a player can act on.
Pack read_pack(const cssx::Client& host, const std::string& token);

// <game>/Content/Paks/~mods from the running executable; empty off Windows.
std::filesystem::path mods_folder();

} // namespace cine

#pragma once
// 1.0.0-beta.6: "Use Non-CSS Mod". A replacement mod is an IoStore container without CSS
// metadata that ships game assets at their original paths, so the engine draws the mod's copy
// of a shell, enemy or NPC. The container names its packages only by id (CityHash64 of the
// lowercase UTF-16 package path), so CSS carries a table of the ids under every character
// folder it can wear from (catalog/replacement-targets.json, built by
// tools/build_replacement_targets.py) and reads nothing but the container's chunk-id table.
#include "data.hpp"

namespace css {
// The folders of the shipped table, with every package id under them sorted for lookup.
struct ReplacementTable {
    std::vector<std::string> folders;                 // "/Game/.../Characters/<Group>/<Name>/"
    std::vector<std::pair<uint64_t,uint32_t>> ids;    // package id -> index into folders, sorted by id
    std::vector<std::pair<uint64_t,std::string>> shared;   // package id -> name of a shared asset CSS depends on, sorted by id
    static ReplacementTable load(const fs::path&);    // throws on a malformed document
    const std::string* folder(uint64_t id) const;
    const std::string* shared_asset(uint64_t id) const;
};
// The longest listed folder that contains an object path such as /Game/A/B/SK_X.SK_X.
const std::string* replacement_folder(const std::vector<std::string>& folders,const std::string& object_path);
// Package ids of every export bundle in a .utoc: the header and the chunk-id table only, a few
// KB for a mod, never the compressed payload. Throws on anything that is not a small IoStore
// table of contents.
struct UtocPackages { std::vector<uint64_t> ids; uint32_t chunks=0; };
UtocPackages read_utoc_packages(const fs::path& utoc);
// The engine's own rule for what overrides base content (FPakPlatformFile::Mount): a pak
// named *_P is a patch and mounts above everything else. The game's containers are not, and a
// mod without the suffix has no promise of winning, so only patches are candidates.
bool patch_container(const fs::path& pak);
// Every ignored patch pak whose container table carries a package under a listed folder, or
// overrides a shared asset (recorded as a conflict, with no folders and so no outfit).
std::vector<ReplacementMod> scan_replacements(const std::vector<fs::path>& ignored_paks,
    const ReplacementTable&,Json* diagnostics);
std::string mod_outfit_id(const std::string& stem);
std::string mod_display_name(const std::string& stem);
// Replaces the css.mod.* outfits with one per replacement mod, a variant per wearable look
// (css.npc.* roster and the official shells, once discovered) in a folder the mod touches.
// Safe to call whenever the catalog's roster changes.
void rebuild_replacement_outfits(Catalog&);
// Runs the table, the scan of the ignored paks recorded in the catalog diagnostics and the
// rebuild. A missing or broken table is recorded in the diagnostics and costs only this row.
void discover_replacements(Catalog&,const fs::path& table);
}

#include "packs.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace cine {
namespace fs = std::filesystem;
namespace {
constexpr size_t max_packs = 64, max_rows = 256, max_presets = 128, max_audio = 64;

bool token_ok(const std::string& t) {
    return !t.empty() && t.size() <= 48 && std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isalnum(c); });
}
bool id_ok(const std::string& id) {
    return !id.empty() && id.size() <= 64 &&
           std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '-' || c == '_'; });
}
std::string text(const Json& j, const char* key, size_t limit, bool optional = false) {
    if (!j.contains(key)) { if (optional) return {}; throw std::invalid_argument(std::string("manifest has no ") + key); }
    if (!j[key].is_string()) throw std::invalid_argument(std::string("manifest ") + key + " is not text");
    auto s = j[key].get<std::string>();
    if (s.size() > limit) throw std::invalid_argument(std::string("manifest ") + key + " is too long");
    return s;
}
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
// A row's JSON from its DevComment; null when the row is missing.
Json row_json(const cssx::Client& host, const Json& table, const std::string& row) {
    const auto r = host.request({{"op", "table.row"}, {"target", table}, {"row", row}});
    if (r.is_null()) return nullptr;
    if (!r.is_object() || !r.contains("DevComment") || !r["DevComment"].is_string())
        throw std::runtime_error("row " + row + " is not a CINE row (wrong table type)");
    try { return Json::parse(r["DevComment"].get<std::string>()); }
    catch (const std::exception&) { throw std::runtime_error("row " + row + " is not valid JSON"); }
}
}

std::string pack_token(const std::string& folder) {
    const std::string prefix = pack_prefix, suffix = pack_suffix;
    if (folder.size() <= prefix.size() + suffix.size() || folder.compare(0, prefix.size(), prefix) != 0 ||
        folder.compare(folder.size() - suffix.size(), suffix.size(), suffix) != 0) return {};
    auto token = folder.substr(prefix.size(), folder.size() - prefix.size() - suffix.size());
    return token_ok(token) ? token : std::string{};
}

std::vector<std::string> find_pack_folders(const fs::path& mods, std::vector<std::string>& problems) {
    std::vector<std::string> found;
    std::error_code ec;
    if (mods.empty() || !fs::is_directory(mods, ec)) return found;
    for (const auto& entry : fs::directory_iterator(mods, ec)) {
        if (!entry.is_directory(ec)) continue;
        const auto folder = entry.path().filename().string();
        const auto token = pack_token(folder);
        if (token.empty()) continue;
        bool complete = true;
        for (const char* ext : {".pak", ".ucas", ".utoc"})
            if (!fs::is_regular_file(entry.path() / (folder + ext), ec)) complete = false;
        if (!complete) { problems.push_back(folder + ": the .pak, .ucas or .utoc file is missing"); continue; }
        found.push_back(token);
        if (found.size() >= max_packs) { problems.push_back("more than 64 CINE packs; the rest are skipped"); break; }
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::string table_path(const std::string& pack) {
    return "/Game/CSSX/CINE/" + pack + "/DT_CINE_Pack.DT_CINE_Pack";
}

Pack parse_manifest(const Json& m, const std::string& token) {
    if (!m.is_object()) throw std::invalid_argument("manifest is not an object");
    if (!m.contains("pack_schema") || !m["pack_schema"].is_number_integer() || m["pack_schema"].get<int>() != 1)
        throw std::invalid_argument("pack format is newer than this CINE; update CINE");
    Pack p;
    p.pack = text(m, "pack", 48);
    if (p.pack != token) throw std::invalid_argument("manifest names pack " + p.pack + ", folder says " + token);
    p.name = text(m, "name", 64);
    p.version = text(m, "version", 16);
    p.song = text(m, "song", 128, true);
    if (!m.contains("presets") || !m["presets"].is_array() || m["presets"].empty() || m["presets"].size() > max_presets)
        throw std::invalid_argument("manifest lists no presets");
    for (const auto& id : m["presets"]) {
        if (!id.is_string() || !id_ok(id.get<std::string>())) throw std::invalid_argument("manifest has an invalid preset id");
        if (std::count(p.presets.begin(), p.presets.end(), id.get<std::string>())) throw std::invalid_argument("manifest lists a preset twice");
        p.presets.push_back(id.get<std::string>());
    }
    const std::string root = "/Game/CSSX/CINE/" + token + "/";
    if (m.contains("audio")) {
        if (!m["audio"].is_object() || m["audio"].size() > max_audio) throw std::invalid_argument("manifest audio is not an object");
        for (const auto& [id, a] : m["audio"].items()) {
            if (!id_ok(id) || !a.is_object()) throw std::invalid_argument("manifest has an invalid audio entry");
            PackAudio audio;
            audio.asset = text(a, "asset", 256);
            if (audio.asset.compare(0, root.size(), root) != 0) throw std::invalid_argument("audio " + id + " is outside the pack's own folder");
            if (a.contains("seconds")) {
                if (!a["seconds"].is_number()) throw std::invalid_argument("audio " + id + " has no length");
                audio.seconds = a["seconds"].get<double>();
                if (!std::isfinite(audio.seconds) || audio.seconds <= 0 || audio.seconds > 3600) throw std::invalid_argument("audio " + id + " has an invalid length");
            }
            p.audio.emplace(id, std::move(audio));
        }
    }
    if (m.contains("requires") && m["requires"].is_object() && m["requires"].contains("cine")) {
        const auto need = m["requires"]["cine"];
        if (!need.is_string()) throw std::invalid_argument("manifest requires an unreadable CINE version");
    }
    return p;
}

Pack read_pack(const cssx::Client& host, const std::string& token) {
    const auto table = host.request({{"op", "load"}, {"path", table_path(token)}});
    if (!is_handle(table)) throw std::runtime_error("the game did not load it (restart the game after installing; reinstall if it stays)");
    const auto manifest = row_json(host, table, "manifest");
    if (manifest.is_null()) throw std::runtime_error("it has no manifest row");
    Pack pack = parse_manifest(manifest, token);
    const auto names = host.request({{"op", "table.rows"}, {"target", table}});
    if (!names.is_array() || names.size() > max_rows) throw std::runtime_error("it has too many rows");
    for (const auto& n : names) {
        if (!n.is_string()) continue;
        const auto name = n.get<std::string>();
        if (name == "manifest") continue;
        const bool wanted = name.rfind("preset.", 0) == 0 || name.rfind("recipe.", 0) == 0 || name.rfind("style.", 0) == 0;
        if (!wanted) continue;
        auto value = row_json(host, table, name);
        if (!value.is_null()) pack.rows.emplace(name, std::move(value));
    }
    for (const auto& id : pack.presets)
        if (!pack.rows.contains("preset." + id)) throw std::runtime_error("the manifest lists " + id + " but the pack has no such preset");
    return pack;
}

fs::path mods_folder() {
#ifdef _WIN32
    std::wstring exe(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, exe.data(), DWORD(exe.size()));
        if (n == 0) return {};
        if (n < exe.size()) { exe.resize(n); break; }
        if (exe.size() >= 32768) return {};
        exe.resize(exe.size() * 2);
    }
    // <game>/Binaries/Win64/<exe>  ->  <game>/Content/Paks/~mods
    const fs::path game = fs::path(exe).parent_path().parent_path().parent_path();
    std::error_code ec;
    if (!fs::is_directory(game / "Content" / "Paks", ec)) return {};
    return game / "Content" / "Paks" / "~mods";
#else
    return {};
#endif
}

} // namespace cine

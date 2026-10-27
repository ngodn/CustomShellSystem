// CustomShellSystem, CSSX and CCS: version files, active core, loader hash,
// logs and state leftovers. Key names come from the installed files and the
// tools that write them (tools/css.py, CCS/tools/ccs.py, the CSSX installer).
#include "scanners.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

namespace wcmg {
namespace {

using json = nlohmann::json;

bool is_file(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }
bool is_dir(const fs::path& p) { std::error_code ec; return fs::is_directory(p, ec); }
FileTime mtime(const fs::path& p) { std::error_code ec; auto t = fs::last_write_time(p, ec); return ec ? FileTime{} : t; }

// A broken or missing json file gives a null value, never an exception.
json load_json(const fs::path& p) {
    if (!is_file(p)) return json();
    try {
        return json::parse(read_text(p, 32 * 1024 * 1024));
    } catch (...) {
        return json();
    }
}

std::string str_field(const json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return "";
    const auto& v = j[key];
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    return "";
}

std::vector<fs::path> list_dir(const fs::path& d) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!is_dir(d)) return out;
    for (fs::directory_iterator it(d, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
        out.push_back(it->path());
    std::sort(out.begin(), out.end());
    return out;
}

// "ccs_core-1.0.0-alpha.5-bf62ce23e214.dll" -> "1.0.0-alpha.5"; drops a trailing
// build hash (8+ hex chars) or "-r<digits>" suffix when present.
std::string version_from_core_name(const std::string& file) {
    auto stem = path_utf8(fs::path(file).stem());
    auto dash = stem.find('-');
    if (dash == std::string::npos) return "";
    auto rest = stem.substr(dash + 1);
    auto is_hex_tail = [](const std::string& s) {
        if (s.size() < 8) return false;
        for (char c : s) if (!std::isxdigit((unsigned char)c)) return false;
        return true;
    };
    for (int pass = 0; pass < 2; ++pass) {
        auto last = rest.rfind('-');
        if (last == std::string::npos) break;
        auto tail = rest.substr(last + 1);
        bool rev = tail.size() > 1 && tail[0] == 'r' && std::all_of(tail.begin() + 1, tail.end(), [](char c) { return std::isdigit((unsigned char)c); });
        if (is_hex_tail(tail) || rev) rest = rest.substr(0, last);
        else break;
    }
    if (rest.empty() || !std::isdigit((unsigned char)rest[0])) return "";
    return rest;
}

// Strips the line prefix each log uses so the classifier sees the message.
std::string message_of(const std::string& line) {
    size_t i = 0;
    // CSS: "<tick> text"
    while (i < line.size() && std::isdigit((unsigned char)line[i])) ++i;
    if (i > 0 && i < line.size() && line[i] == ' ' && i >= 6) return line.substr(i + 1);
    // CSSX: "2026-09-21T15:35:19.734Z text"
    if (line.size() > 20 && std::isdigit((unsigned char)line[0]) && line[4] == '-' && line[10] == 'T') {
        auto sp = line.find(' ');
        if (sp != std::string::npos) return line.substr(sp + 1);
    }
    return line;
}

bool is_warning(const std::string& msg) {
    auto low = lower(msg);
    return low.rfind("warning", 0) == 0 || low.find("warning:") != std::string::npos;
}

bool is_error(const std::string& msg) {
    auto low = lower(msg);
    for (const char* w : {"error", "fail", "exception", "crash"})
        if (low.find(w) != std::string::npos) return true;
    return false;
}

void read_our_log(OurModInfo& m, const char* session_marker, std::vector<std::string>& lines) {
    if (!is_file(m.log)) return;
    m.log_when = mtime(m.log);
    lines = tail_lines(m.log, 200000, 24 * 1024 * 1024);
    // Every launch appends; the errors that matter for the latest crash are the
    // ones since the last loader start.
    size_t start = 0;
    int sessions = 0;
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].find(session_marker) != std::string::npos) { start = i; ++sessions; }
    int all_errors = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        auto msg = message_of(lines[i]);
        bool warn = is_warning(msg);
        bool err = !warn && is_error(msg);
        if (err) ++all_errors;
        if (i < start) continue;
        if (warn && m.warnings.size() < 100) m.warnings.push_back(lines[i]);
        if (err && m.errors.size() < 100) m.errors.push_back(lines[i]);
    }
    size_t from = lines.size() > 30 ? lines.size() - 30 : 0;
    m.tail.assign(lines.begin() + (std::ptrdiff_t)from, lines.end());
    m.notes["log_sessions"] = std::to_string(sessions);
    if (sessions > 0) m.notes["log_scope"] = "errors and warnings since the last start (line " + std::to_string(start + 1) + " of " + std::to_string(lines.size()) + ")";
    m.notes["log_errors_all_sessions"] = std::to_string(all_errors);
}

struct Spec {
    const char* name;
    const char* log;
    const char* session_marker;
};

OurModInfo scan_one(const Paths& paths, const Spec& s, std::vector<std::string>& lines) {
    OurModInfo m;
    m.name = s.name;
    m.folder = paths.mods / s.name;
    m.installed = is_dir(m.folder);
    if (!m.installed) return m;

    auto release = load_json(m.folder / "release.json");
    if (is_file(m.folder / "release.json") && release.is_null()) m.notes["release.json"] = "unreadable or broken json";
    m.release_version = str_field(release, "version");
    if (release.is_object() && release.contains("dev") && release["dev"].is_boolean() && release["dev"].get<bool>())
        m.notes["release_dev_build"] = "true";
    if (auto rev = str_field(release, "ue4ss_revision"); !rev.empty()) m.notes["pinned_ue4ss_revision"] = rev;
    if (auto sha = str_field(release, "ue4ss_dll_sha256"); !sha.empty()) m.notes["pinned_ue4ss_sha256"] = lower(sha);

    auto core = load_json(m.folder / "core.json");
    if (is_file(m.folder / "core.json") && core.is_null()) m.notes["core.json"] = "unreadable or broken json";
    m.active_core = str_field(core, "file");
    if (m.active_core.empty()) m.active_core = str_field(release, "core");

    // Cores live in cores/ (CSS) or core/ (CSSX, CCS); check both.
    for (const char* d : {"cores", "core"}) {
        for (const auto& p : list_dir(m.folder / d)) {
            if (lower(path_utf8(p.extension())) == ".dll" && is_file(p)) m.cores.push_back(path_utf8(p.filename()));
        }
        if (!m.active_core.empty() && is_file(m.folder / d / fs::path(std::u8string(m.active_core.begin(), m.active_core.end()))))
            m.active_core_present = true;
    }
    m.notes["cores_installed"] = std::to_string(m.cores.size());

    if (m.release_version.empty()) {
        auto v = version_from_core_name(m.active_core);
        if (!v.empty()) {
            m.release_version = v;
            m.notes["version_source"] = "active core file name (no release.json)";
        }
    } else {
        m.notes["version_source"] = "release.json";
    }

    // Loader hash: loader-contract.json (CSS) first, then release.json.
    auto loader = m.folder / "dlls" / "main.dll";
    if (is_file(loader)) m.loader_sha_actual = sha256_file(loader);
    auto contract = load_json(m.folder / "loader-contract.json");
    std::string source;
    if (auto h = str_field(contract, "dll_sha256"); !h.empty()) { m.loader_sha_recorded = lower(h); source = "loader-contract.json dll_sha256"; }
    if (m.loader_sha_recorded.empty()) {
        if (auto h = str_field(release, "loader_sha256"); !h.empty()) { m.loader_sha_recorded = lower(h); source = "release.json loader_sha256"; }
    }
    std::string release_files_sha;
    if (release.is_object() && release.contains("files") && release["files"].is_object()) {
        for (const auto& key : {std::string("dlls/main.dll"), std::string(s.name) + "/dlls/main.dll"}) {
            if (release["files"].contains(key) && release["files"][key].is_string()) {
                release_files_sha = lower(release["files"][key].get<std::string>());
                break;
            }
        }
    }
    // release.json is what a player installed, so its file hash outranks loader-contract.json,
    // which is a development aid that goes stale between dev reloads.
    if (!release_files_sha.empty()) {
        if (!m.loader_sha_recorded.empty() && m.loader_sha_recorded != release_files_sha)
            m.notes["loader_contract"] = "loader-contract.json is stale (differs from release.json); harmless for players";
        m.loader_sha_recorded = release_files_sha;
        source = "release.json files[dlls/main.dll]";
    }
    if (!source.empty()) m.notes["loader_sha_source"] = source;
    m.loader_mismatch = !m.loader_sha_recorded.empty() && !m.loader_sha_actual.empty() && m.loader_sha_recorded != m.loader_sha_actual;
    if (!is_file(loader)) m.notes["loader"] = "dlls/main.dll missing";

    m.log = m.folder / s.log;
    try { read_our_log(m, s.session_marker, lines); } catch (...) {}

    // state/*.corrupt-* are files the mod set aside after failing to parse them.
    for (const auto& p : list_dir(m.folder / "state")) {
        if (path_utf8(p.filename()).find(".corrupt-") != std::string::npos) ++m.corrupt_state_files;
    }
    m.notes["enabled_txt"] = is_file(m.folder / "enabled.txt") ? "true" : "false";
    return m;
}

} // namespace

std::vector<OurModInfo> scan_our_mods(const Paths& paths) {
    static const Spec specs[] = {
        {"CustomShellSystem", "CSS.log", "CSS loader created"},
        {"CSSX", "CSSX.log", "CSSX loader"},
        {"CCS", "CCS.log", "CCS loader created"},
    };
    std::vector<OurModInfo> out;
    for (const auto& s : specs) {
        OurModInfo m;
        std::vector<std::string> lines;
        try {
            m = scan_one(paths, s, lines);
        } catch (...) {
            m.name = s.name;
            m.folder = paths.mods / s.name;
            m.notes["scan"] = "stopped early on an unreadable file";
        }
        try {
            if (m.installed && m.name == "CustomShellSystem") {
                // Last "CSS catalog: 27 outfits; 34 pak files scanned; 0 rejected" line.
                for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                    auto p = it->find("CSS catalog: ");
                    if (p != std::string::npos) { m.notes["catalog"] = trim(it->substr(p + 13)); break; }
                }
                for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                    auto p = it->find("Core activated: ");
                    if (p != std::string::npos) { m.notes["last_core_activated"] = trim(it->substr(p + 16)); break; }
                }
            }
            if (m.installed && m.name == "CSSX") {
                m.dev_channel_enabled = is_file(m.folder / "dev" / "enabled.txt");
                for (const auto& p : list_dir(m.folder / "extensions"))
                    if (is_dir(p)) m.extensions.push_back(path_utf8(p.filename()));
                auto settings = load_json(m.folder / "settings.json");
                if (settings.is_object()) m.notes["settings_keys"] = std::to_string(settings.size());
                else if (is_file(m.folder / "settings.json")) m.notes["settings.json"] = "unreadable or broken json";
                if (!m.extensions.empty()) {
                    std::string all;
                    for (const auto& e : m.extensions) all += (all.empty() ? "" : ", ") + e;
                    m.notes["extensions"] = all;
                }
                for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                    auto p = it->find("Core activated: ");
                    if (p != std::string::npos) { m.notes["last_core_activated"] = trim(it->substr(p + 16)); break; }
                }
            }
            if (m.installed && m.name == "CCS") {
                auto catalog = load_json(m.folder / "catalog.json");
                if (catalog.is_object() && catalog.contains("moves") && catalog["moves"].is_array())
                    m.notes["catalog_moves"] = std::to_string(catalog["moves"].size());
                else if (is_file(m.folder / "catalog.json")) m.notes["catalog.json"] = "no moves list (unreadable or different schema)";
                if (auto gb = str_field(catalog, "game_build"); !gb.empty()) m.notes["catalog_game_build"] = gb;
                auto settings = load_json(m.folder / "settings.json");
                if (settings.is_object()) m.notes["settings_keys"] = std::to_string(settings.size());
                else if (is_file(m.folder / "settings.json")) m.notes["settings.json"] = "unreadable or broken json";
                for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                    auto p = it->find("CCS core loaded: ");
                    if (p != std::string::npos) { m.notes["last_core_loaded"] = trim(it->substr(p + 17)); break; }
                }
            }
        } catch (...) {}
        out.push_back(std::move(m));
    }
    return out;
}

} // namespace wcmg

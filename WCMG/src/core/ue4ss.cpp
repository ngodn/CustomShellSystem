// UE4SS install, settings, mods list, the log and the proxy loader.
// The mod loading rules mirror UE4SSProgram.cpp (start_mods / discover_mods) of
// the 3.0.1 build the game runs: mods.txt lines with ';' are skipped, spaces are
// removed, "Name : 1" starts the mod, and enabled.txt starts it regardless.
#include "scanners.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <set>

namespace wcmg {

// Shared with system.cpp (declared there), kept out of the public headers.
namespace scan_detail {

const std::vector<std::string>& proxy_dll_names() {
    static const std::vector<std::string> names = {
        "dwmapi.dll", "xinput1_3.dll", "dinput8.dll", "version.dll", "winmm.dll",
        "dxgi.dll", "d3d11.dll", "d3d12.dll", "winhttp.dll"};
    return names;
}

// "Section.Key" -> value. Later duplicates win. With ue_style, UE array prefixes
// are handled the way the engine reads a user ini: '+' adds (kept as the key),
// '-' and '!' remove or clear (skipped here).
std::map<std::string, std::string> parse_ini_text(const std::string& text, bool ue_style) {
    std::map<std::string, std::string> out;
    std::string section;
    size_t pos = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) pos = 3;
    while (pos < text.size()) {
        auto nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        auto line = trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[') {
            auto close = line.find(']');
            section = trim(line.substr(1, close == std::string::npos ? std::string::npos : close - 1));
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = trim(line.substr(0, eq));
        auto value = trim(line.substr(eq + 1));
        if (ue_style && !key.empty()) {
            if (key[0] == '-' || key[0] == '!') continue;
            if (key[0] == '+' || key[0] == '.') key = trim(key.substr(1));
        }
        if (key.empty()) continue;
        out[section.empty() ? key : section + "." + key] = value;
    }
    return out;
}

std::map<std::string, std::string> parse_ini_file(const fs::path& p, bool ue_style) {
    try {
        return parse_ini_text(read_text(p, 8 * 1024 * 1024), ue_style);
    } catch (...) {
        return {};
    }
}

namespace {
uint32_t rd32(const std::string& b, size_t off) {
    if (off + 4 > b.size()) return 0;
    return (uint32_t)(unsigned char)b[off] | (uint32_t)(unsigned char)b[off + 1] << 8 |
           (uint32_t)(unsigned char)b[off + 2] << 16 | (uint32_t)(unsigned char)b[off + 3] << 24;
}
uint16_t rd16(const std::string& b, size_t off) {
    if (off + 2 > b.size()) return 0;
    return (uint16_t)((unsigned char)b[off] | (unsigned char)b[off + 1] << 8);
}
}

// Names in the PE import and delay-import directories. Empty when not a PE.
std::vector<std::string> pe_imports(const std::string& b) {
    std::vector<std::string> out;
    if (b.size() < 0x40 || b[0] != 'M' || b[1] != 'Z') return out;
    uint32_t nt = rd32(b, 0x3C);
    if (nt + 24 > b.size() || std::memcmp(b.data() + nt, "PE\0\0", 4) != 0) return out;
    uint16_t sections = rd16(b, nt + 6);
    uint16_t opt_size = rd16(b, nt + 20);
    size_t opt = nt + 24;
    uint16_t magic = rd16(b, opt);
    size_t dirs = opt + (magic == 0x20b ? 112 : 96);
    uint32_t dir_count = rd32(b, opt + (magic == 0x20b ? 108 : 92));
    size_t sec_table = opt + opt_size;
    auto rva_to_off = [&](uint32_t rva) -> size_t {
        for (uint16_t i = 0; i < sections && i < 96; ++i) {
            size_t s = sec_table + (size_t)i * 40;
            uint32_t vsize = rd32(b, s + 8), va = rd32(b, s + 12), raw_size = rd32(b, s + 16), raw = rd32(b, s + 20);
            uint32_t span = std::max(vsize, raw_size);
            if (rva >= va && rva < va + span) {
                size_t off = (size_t)raw + (rva - va);
                return off < b.size() ? off : 0;
            }
        }
        return 0;
    };
    auto read_name = [&](uint32_t rva) {
        std::string n;
        size_t off = rva_to_off(rva);
        if (!off) return n;
        while (off < b.size() && b[off] && n.size() < 260) n += b[off++];
        return n;
    };
    if (dir_count > 1) {
        uint32_t imp = rd32(b, dirs + 8);
        if (size_t off = imp ? rva_to_off(imp) : 0) {
            for (int i = 0; i < 512 && off + 20 <= b.size(); ++i, off += 20) {
                uint32_t name_rva = rd32(b, off + 12);
                if (!name_rva && !rd32(b, off) && !rd32(b, off + 16)) break;
                auto n = read_name(name_rva);
                if (!n.empty()) out.push_back(n);
            }
        }
    }
    if (dir_count > 13) {
        uint32_t dimp = rd32(b, dirs + 13 * 8);
        if (size_t off = dimp ? rva_to_off(dimp) : 0) {
            for (int i = 0; i < 512 && off + 32 <= b.size(); ++i, off += 32) {
                uint32_t name_rva = rd32(b, off + 4);
                if (!name_rva) break;
                auto n = read_name(name_rva);
                if (!n.empty()) out.push_back(n);
            }
        }
    }
    return out;
}

// The UE4SS proxy loads UE4SS.dll at runtime (LoadLibrary), so the import table
// alone does not show it; the file's strings do.
bool is_ue4ss_loader(const fs::path& dll) {
    try {
        auto bytes = read_text(dll, 64 * 1024 * 1024);
        if (bytes.empty()) return false;
        for (const auto& n : pe_imports(bytes))
            if (lower(n) == "ue4ss.dll") return true;
        auto low = lower(bytes);
        if (low.find("ue4ss.dll") != std::string::npos) return true;
        static const char wide[] = "u\0e\0004\0s\0s\0.\0d\0l\0l\0";
        if (low.find(std::string(wide, sizeof wide - 1)) != std::string::npos) return true;
    } catch (...) {}
    return false;
}

} // namespace scan_detail

namespace {
using scan_detail::parse_ini_file;

bool is_dir(const fs::path& p) { std::error_code ec; return fs::is_directory(p, ec); }
bool is_file(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }
uint64_t size_of(const fs::path& p) { std::error_code ec; auto s = fs::file_size(p, ec); return ec ? 0 : s; }
FileTime mtime(const fs::path& p) { std::error_code ec; auto t = fs::last_write_time(p, ec); return ec ? FileTime{} : t; }

std::string ext_lower(const fs::path& p) { return lower(path_utf8(p.extension())); }

constexpr size_t kMaxErrors = 200, kMaxWarnings = 200, kMaxLuaErrors = 100, kTail = 40;
// Hashing every DLL of a developer install (hundreds of stale core builds) would
// take minutes; past these limits the rest get sizes but no hash.
constexpr size_t kMaxHashedDllsPerMod = 48;
constexpr uint64_t kMaxHashedBytesPerMod = 768ull * 1024 * 1024;
constexpr size_t kMaxWalkEntries = 60000;

bool is_ts_at(const std::string& s, size_t i) {
    if (i + 21 > s.size() || s[i] != '[') return false;
    auto d = [&](size_t k) { return std::isdigit((unsigned char)s[i + k]) != 0; };
    return d(1) && d(2) && d(3) && d(4) && s[i + 5] == '-' && d(6) && d(7) && s[i + 8] == '-' && d(9) && d(10) &&
           s[i + 11] == ' ' && d(12) && d(13) && s[i + 14] == ':' && d(15) && d(16) && s[i + 17] == ':' && d(18) && d(19) &&
           (s[i + 20] == '.' || s[i + 20] == ']');
}

// UE4SS sometimes writes two entries on one physical line (a print without a
// newline followed by the next entry), so split at every embedded timestamp.
std::vector<Ue4ssLogLine> split_log(const std::string& text) {
    std::vector<Ue4ssLogLine> out;
    size_t pos = 0;
    while (pos <= text.size()) {
        auto nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        pos = nl + 1;
        std::vector<size_t> cuts{0};
        for (size_t i = 1; i < line.size(); ++i)
            if (line[i] == '[' && is_ts_at(line, i)) cuts.push_back(i);
        for (size_t c = 0; c < cuts.size(); ++c) {
            size_t end = c + 1 < cuts.size() ? cuts[c + 1] : line.size();
            std::string piece = line.substr(cuts[c], end - cuts[c]);
            Ue4ssLogLine l;
            if (is_ts_at(piece, 0)) {
                auto close = piece.find(']');
                l.time = piece.substr(1, close - 1);
                l.text = piece.substr(close + 1);
                if (!l.text.empty() && l.text[0] == ' ') l.text.erase(0, 1);
            } else {
                l.text = piece;
            }
            out.push_back(std::move(l));
        }
        if (nl >= text.size()) break;
    }
    while (!out.empty() && out.back().time.empty() && trim(out.back().text).empty()) out.pop_back();
    return out;
}

bool is_ident(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// "FArchiveState::ArIsError = 0x29": UE4SS's member offset dump, not an error.
bool is_offset_dump(const std::string& t) {
    auto sep = t.find("::");
    if (sep == std::string::npos || sep == 0) return false;
    for (size_t i = 0; i < sep; ++i) if (!is_ident(t[i])) return false;
    size_t i = sep + 2, start = i;
    while (i < t.size() && is_ident(t[i])) ++i;
    if (i == start) return false;
    return t.compare(i, 5, " = 0x") == 0;
}

std::string quoted_name(const std::string& t, size_t from) {
    auto a = t.find('\'', from);
    if (a == std::string::npos) return "";
    auto b = t.find('\'', a + 1);
    if (b == std::string::npos) return "";
    return t.substr(a + 1, b - a - 1);
}

void read_log(Ue4ssInfo& info) {
    const size_t cap = 48 * 1024 * 1024;
    std::string text;
    uint64_t sz = size_of(info.log);
    if (sz > cap) {
        // Keep the header (version lines) and the most recent part.
        auto head = read_text(info.log, 256 * 1024);
        head = head.substr(0, head.rfind('\n') == std::string::npos ? head.size() : head.rfind('\n') + 1);
        std::ifstream in(info.log, std::ios::binary);
        in.seekg((std::streamoff)(sz - (cap - head.size())));
        std::string rest((size_t)(cap - head.size()), '\0');
        in.read(rest.data(), (std::streamsize)rest.size());
        rest.resize((size_t)in.gcount());
        auto nl = rest.find('\n');
        text = head + (nl == std::string::npos ? rest : rest.substr(nl + 1));
    } else {
        text = read_text(info.log, cap);
    }
    auto lines = split_log(text);
    std::set<std::string> started;
    bool in_traceback = false;
    for (const auto& l : lines) {
        const auto& t = l.text;
        auto low = lower(t);
        if (info.version_line.empty() && t.find("UE4SS - v") != std::string::npos) {
            info.version_line = trim(t);
            auto g = t.find("Git SHA #");
            if (g != std::string::npos) {
                size_t i = g + 9, j = i;
                while (j < t.size() && std::isxdigit((unsigned char)t[j])) ++j;
                info.git_sha = t.substr(i, j - i);
            }
        }
        if (info.build_config.empty()) {
            auto b = t.find("UE4SS Build Configuration:");
            if (b != std::string::npos) info.build_config = trim(t.substr(b + 26));
        }
        for (const char* marker : {"Starting C++ mod '", "Starting Lua mod '", "Mod '"}) {
            auto m = t.find(marker);
            if (m == std::string::npos) continue;
            if (std::strcmp(marker, "Mod '") == 0 && t.find("has enabled.txt, starting mod") == std::string::npos) continue;
            auto name = quoted_name(t, m);
            if (!name.empty() && started.insert(name).second) info.started_mods.push_back(name);
        }
        if (t.find("[HashTables] Self test passed") != std::string::npos || t.find("[Lua]") != std::string::npos ||
            t.find("FName constructor verified") != std::string::npos)
            info.log_reached_game_thread = true;

        // A traceback's frames are continuation lines without a timestamp.
        bool continuation = l.time.empty();
        if (!continuation) in_traceback = false;
        bool lua_err = false;
        if (in_traceback && continuation) lua_err = true;
        if (low.find("stack traceback") != std::string::npos) { lua_err = true; in_traceback = true; }
        if (t.find("[Lua]") != std::string::npos && (low.find("error") != std::string::npos || low.find("traceback") != std::string::npos))
            lua_err = true;
        if (low.find("error executing script") != std::string::npos || low.find("error loading script") != std::string::npos ||
            low.find("failed to execute main script") != std::string::npos || low.find("main.lua' not found") != std::string::npos ||
            low.find("no traceback") != std::string::npos) {
            lua_err = true;
            in_traceback = true;
        }
        if (lua_err && info.lua_errors.size() < kMaxLuaErrors) info.lua_errors.push_back(l);

        if (!is_offset_dump(t)) {
            bool err = false;
            for (const char* w : {"error", "exception", "crash", "failed", "failure", "fatal"})
                if (low.find(w) != std::string::npos) { err = true; break; }
            if (err && info.errors.size() < kMaxErrors) info.errors.push_back(l);
            if (low.find("warning") != std::string::npos && info.warnings.size() < kMaxWarnings) info.warnings.push_back(l);
        }
    }
    size_t from = lines.size() > kTail ? lines.size() - kTail : 0;
    info.tail.assign(lines.begin() + (std::ptrdiff_t)from, lines.end());
}

struct ModsTxt {
    bool present = false;
    std::vector<std::string> order;
    std::map<std::string, bool> state;
};

ModsTxt read_mods_txt(const fs::path& p) {
    ModsTxt m;
    if (!is_file(p)) return m;
    m.present = true;
    auto text = read_text(p, 4 * 1024 * 1024);
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text.erase(0, 3);
    size_t pos = 0;
    while (pos < text.size()) {
        auto nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // UE4SS skips any line holding ';' and any line of 4 chars or fewer.
        if (line.find(';') != std::string::npos || line.size() <= 4) continue;
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\t'), line.end());
        auto first = line.find(':');
        if (first == std::string::npos) continue;
        auto name = line.substr(0, first);
        auto value = line.substr(line.rfind(':') + 1);
        if (name.empty()) continue;
        bool on = !value.empty() && value[0] == '1';
        auto it = m.state.find(name);
        if (it == m.state.end()) {
            m.order.push_back(name);
            m.state[name] = on;
        } else if (on) {
            it->second = true;  // a later "1" line still starts a mod an earlier "0" line skipped
        }
    }
    return m;
}

std::map<std::string, bool> read_mods_json(const fs::path& p) {
    std::map<std::string, bool> out;
    if (!is_file(p)) return out;
    try {
        auto j = nlohmann::json::parse(read_text(p, 4 * 1024 * 1024));
        if (!j.is_array()) return out;
        for (const auto& e : j) {
            if (!e.is_object() || !e.contains("mod_name")) continue;
            auto name = e["mod_name"].is_string() ? e["mod_name"].get<std::string>() : std::string();
            bool on = e.contains("mod_enabled") && e["mod_enabled"].is_boolean() && e["mod_enabled"].get<bool>();
            if (!name.empty()) out[name] = on;
        }
    } catch (...) {}
    return out;
}

fs::path resolve_against(const fs::path& base, const std::string& value) {
    fs::path p(std::u8string(value.begin(), value.end()));
    if (p.is_relative()) p = base / p;
    return p.lexically_normal();
}

bool in_list_ci(const std::vector<std::string>& list, const std::string& name) {
    auto n = lower(name);
    for (const auto& x : list) if (lower(x) == n) return true;
    return false;
}

Ue4ssMod scan_mod_folder(const fs::path& folder, const ModsTxt& txt, const std::map<std::string, bool>& json,
                         const Knowledge& k) {
    Ue4ssMod m;
    m.folder = folder;
    m.name = path_utf8(folder.filename());
    // UE4SS names a mod after the folder stem and matches mods.txt exactly.
    std::string key = path_utf8(folder.stem());
    m.has_scripts = is_file(folder / "Scripts" / "main.lua") || is_file(folder / "scripts" / "main.lua");
    m.has_dll = is_file(folder / "dlls" / "main.dll");
    m.enabled_txt = is_file(folder / "enabled.txt");
    if (auto it = txt.state.find(key); it != txt.state.end()) m.mods_txt = it->second;
    if (auto it = json.find(key); it != json.end()) m.mods_json = it->second;
    bool loadable = is_dir(folder / "Scripts") || is_dir(folder / "scripts") || is_dir(folder / "dlls");
    bool wanted = m.enabled_txt || (m.mods_txt && *m.mods_txt) || (!txt.present && m.mods_json && *m.mods_json);
    m.effectively_enabled = loadable && wanted;
    m.builtin = in_list_ci(k.builtin_mods, m.name);
    for (const char* ours : {"CustomShellSystem", "CSSX", "CCS"})
        if (m.name == ours) m.ours = true;

    struct DllEntry { fs::path p; uint64_t bytes; FileTime when; };
    std::vector<DllEntry> dlls;
    std::error_code ec;
    size_t seen = 0;
    bool any_file = false;
    fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (++seen > kMaxWalkEntries) break;
        std::error_code fec;
        if (!it->is_regular_file(fec)) continue;
        const auto& p = it->path();
        auto when = mtime(p);
        // FileTime{} is the "unset" value, and on libstdc++ its epoch lies after
        // every real mtime, so it cannot seed a max().
        if (!any_file || when > m.newest_file) m.newest_file = when;
        any_file = true;
        auto e = ext_lower(p);
        if (e == ".dll") dlls.push_back({p, size_of(p), when});
        else if (e == ".lua") { m.lua_files.push_back(p); m.lua_bytes += size_of(p); }
    }
    auto main_dll = (folder / "dlls" / "main.dll").lexically_normal();
    std::sort(dlls.begin(), dlls.end(), [&](const DllEntry& a, const DllEntry& b) {
        bool am = a.p.lexically_normal() == main_dll, bm = b.p.lexically_normal() == main_dll;
        if (am != bm) return am;
        if (a.when != b.when) return a.when > b.when;
        return a.p < b.p;
    });
    uint64_t hashed_bytes = 0;
    for (size_t i = 0; i < dlls.size(); ++i) {
        m.dlls.push_back(dlls[i].p);
        m.dll_bytes.push_back(dlls[i].bytes);
        bool hash = i < kMaxHashedDllsPerMod && hashed_bytes + dlls[i].bytes <= kMaxHashedBytesPerMod;
        if (i == 0) hash = true;
        m.dll_sha256.push_back(hash ? sha256_file(dlls[i].p) : std::string());
        if (hash) hashed_bytes += dlls[i].bytes;
    }
    std::sort(m.lua_files.begin(), m.lua_files.end());
    return m;
}

} // namespace

Ue4ssInfo scan_ue4ss(const Paths& paths, const Knowledge& k) {
    Ue4ssInfo info;
    try {
        // Current layout is Win64/ue4ss/; the legacy layout put everything in Win64/.
        fs::path root = paths.ue4ss;
        if (!is_file(root / "UE4SS.dll") && !paths.win64.empty() && is_file(paths.win64 / "UE4SS.dll")) root = paths.win64;
        info.dll = root / "UE4SS.dll";
        info.installed = is_file(info.dll);
        if (info.installed) {
            info.dll_bytes = size_of(info.dll);
            info.dll_sha256 = sha256_file(info.dll);
        }

        // Proxy loader beside the exe.
        for (const auto& name : scan_detail::proxy_dll_names()) {
            auto p = paths.win64 / name;
            if (!is_file(p)) continue;
            if (info.proxy_dll.empty() && scan_detail::is_ue4ss_loader(p)) info.proxy_dll = name;
            else info.other_proxy_dlls.push_back(name);
        }

        // Settings.
        fs::path settings = root / "UE4SS-settings.ini";
        if (!is_file(settings) && is_file(paths.win64 / "UE4SS-settings.ini")) settings = paths.win64 / "UE4SS-settings.ini";
        info.settings = parse_ini_file(settings, false);
        auto setting = [&](const char* key) -> std::string {
            auto it = info.settings.find(key);
            return it == info.settings.end() ? std::string() : it->second;
        };
        info.gui_console_enabled = setting("Debug.GuiConsoleEnabled") == "1" || lower(setting("Debug.GuiConsoleEnabled")) == "true";
        info.graphics_api = setting("Debug.GraphicsAPI");

        // Mods directory: Overrides.ModsFolderPath wins, relative to the ue4ss dir.
        fs::path mods_dir = paths.mods.empty() ? root / "Mods" : paths.mods;
        if (auto o = trim(setting("Overrides.ModsFolderPath")); !o.empty()) mods_dir = resolve_against(root, o);
        else if (!is_dir(mods_dir) && is_dir(paths.win64 / "Mods")) mods_dir = paths.win64 / "Mods";

        fs::path mods_txt_path = mods_dir / "mods.txt";
        if (auto c = trim(setting("Overrides.ControllingModsTxt")); !c.empty()) mods_txt_path = resolve_against(root, c);
        auto txt = read_mods_txt(mods_txt_path);
        info.mods_txt_order = txt.order;
        auto json = read_mods_json(mods_dir / "mods.json");

        if (is_dir(mods_dir)) {
            std::vector<fs::path> folders;
            std::error_code ec;
            for (fs::directory_iterator it(mods_dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
                std::error_code dec;
                if (!it->is_directory(dec)) continue;
                if (lower(path_utf8(it->path().filename())) == "shared") continue;  // UE4SS's shared Lua libraries, not a mod
                folders.push_back(it->path());
            }
            std::sort(folders.begin(), folders.end(), [](const fs::path& a, const fs::path& b) {
                return lower(path_utf8(a.filename())) < lower(path_utf8(b.filename()));
            });
            for (const auto& f : folders) {
                try {
                    auto m = scan_mod_folder(f, txt, json, k);
                    if (!m.lua_files.empty()) scan_lua_mod(m);
                    info.mods.push_back(std::move(m));
                } catch (...) {
                    Ue4ssMod m;
                    m.name = path_utf8(f.filename());
                    m.folder = f;
                    m.risk_notes.push_back("could not read this mod folder");
                    info.mods.push_back(std::move(m));
                }
            }
        }

        // Log.
        info.log = root / "UE4SS.log";
        if (is_file(info.log)) {
            info.log_when = mtime(info.log);
            try { read_log(info); } catch (...) {}
        }

        // Crash dumps UE4SS wrote itself.
        std::error_code ec;
        std::vector<fs::path> dump_dirs{root};
        if (root != paths.win64 && !paths.win64.empty()) dump_dirs.push_back(paths.win64);
        for (const auto& d : dump_dirs) {
            if (!is_dir(d)) continue;
            for (fs::directory_iterator it(d, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
                auto n = lower(path_utf8(it->path().filename()));
                std::error_code fec;
                if (it->is_regular_file(fec) && n.rfind("crash_", 0) == 0 && ext_lower(it->path()) == ".dmp") {
                    info.crash_dumps.push_back(it->path());
                }
                if (d == root && ext_lower(it->path()) == ".usmap") info.usmap_present = true;
            }
        }
        std::sort(info.crash_dumps.begin(), info.crash_dumps.end(), [](const fs::path& a, const fs::path& b) { return mtime(a) > mtime(b); });
        info.cxx_header_dump_present = is_dir(root / "CXXHeaderDump");
    } catch (...) {
        // Return what was gathered; the orchestrator has no exception to record here by design.
    }
    return info;
}

} // namespace wcmg

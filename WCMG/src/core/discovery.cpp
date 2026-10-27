// Finding the game: Steam libraries (portable), the user's pick, and the Saved
// folder that belongs to an install. The Windows registry, Epic and drive probes
// live in src/win/discovery_win.cpp behind the hooks in discovery_private.hpp.
#include "discovery_private.hpp"
#include "scanners.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>

#ifdef _WIN32
// The hooks are plain C symbols so the linker can fall back to the empty
// defaults below when discovery_win.cpp is not part of the executable.
extern "C" void wcmg_win_steam_roots(std::vector<wcmg::fs::path>* out);
extern "C" void wcmg_win_extra_candidates(std::vector<wcmg::detail::Candidate>* out);
extern "C" int wcmg_win_under_wine();
extern "C" void wcmg_win_steam_roots_none(std::vector<wcmg::fs::path>*) {}
extern "C" void wcmg_win_extra_candidates_none(std::vector<wcmg::detail::Candidate>*) {}
extern "C" int wcmg_win_under_wine_none() { return 0; }
#pragma comment(linker, "/alternatename:wcmg_win_steam_roots=wcmg_win_steam_roots_none")
#pragma comment(linker, "/alternatename:wcmg_win_extra_candidates=wcmg_win_extra_candidates_none")
#pragma comment(linker, "/alternatename:wcmg_win_under_wine=wcmg_win_under_wine_none")
#endif

namespace wcmg {
namespace {
constexpr const char* kAppId = "2584270";
constexpr const char* kExe = "MortalShell2-Win64-Shipping.exe";

bool is_file(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }
bool is_dir(const fs::path& p) { std::error_code ec; return fs::is_directory(p, ec); }
bool has_exe(const fs::path& win64) { return is_file(win64 / kExe); }

// Drops a trailing separator so parent_path() climbs instead of standing still.
fs::path tidy(fs::path p) {
    std::error_code ec;
    auto abs = fs::absolute(p, ec);
    if (!ec) p = abs;
    p = p.lexically_normal();
    p.make_preferred();
    while (!p.has_filename() && p.has_parent_path() && p != p.root_path()) p = p.parent_path();
    return p;
}

// Under Wine a Unix path from HOME or a vdf has to go through drive Z:.
fs::path host_path(const std::string& s) {
#ifdef _WIN32
    if (!s.empty() && s[0] == '/' && detail::platform_running_under_wine()) return detail::path_from_utf8("Z:" + s).make_preferred();
    return detail::path_from_utf8(s).make_preferred();
#else
    return detail::path_from_utf8(s);
#endif
}

// For text a person reads: native separators, unlike path_utf8.
std::string native_utf8(const fs::path& p) {
    auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
}

std::string dedupe_key(const fs::path& p) {
    std::error_code ec;
    auto c = fs::weakly_canonical(p, ec);
    auto s = path_utf8(ec ? p : c);
    while (s.size() > 1 && s.back() == '/') s.pop_back();
#ifdef _WIN32
    s = lower(s);
#endif
    return s;
}

fs::path compatdata_saved(const fs::path& steamapps) {
    return steamapps / "compatdata" / kAppId / "pfx" / "drive_c" / "users" / "steamuser" / "AppData" / "Local" /
           "MortalShell2" / "Saved";
}

std::vector<fs::path> home_steam_roots() {
    std::vector<fs::path> out;
    const char* home = std::getenv("HOME");
    // Wine hides the Unix HOME from Windows programs but passes it on under this name.
    if (!home || !*home) home = std::getenv("WINE_HOST_HOME");
    if (!home || !*home) return out;
    std::string h = home;
    for (const char* rel : {"/.local/share/Steam", "/.steam/steam", "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
                            "/snap/steam/common/.local/share/Steam"})
        out.push_back(host_path(h + rel));
    return out;
}

fs::path find_saved(const fs::path& game_root) {
    std::vector<fs::path> wine_side, windows_side;
    // Proton keeps the prefix in the library that holds the game.
    wine_side.push_back(compatdata_saved(game_root.parent_path().parent_path()));
    for (auto& root : home_steam_roots()) wine_side.push_back(compatdata_saved(root / "steamapps"));
    if (auto local = detail::env_path("LOCALAPPDATA"); !local.empty())
        windows_side.push_back(local / "MortalShell2" / "Saved");

    std::vector<fs::path> order;
#ifdef _WIN32
    bool windows_first = !detail::platform_running_under_wine();
#else
    bool windows_first = false;
#endif
    if (windows_first) { order = windows_side; order.insert(order.end(), wine_side.begin(), wine_side.end()); }
    else { order = wine_side; order.insert(order.end(), windows_side.begin(), windows_side.end()); }
    order.push_back(game_root / "MortalShell2" / "Saved");
    for (auto& p : order)
        if (is_dir(p)) return p;
    return order.front();  // not there yet; the scanners report it as missing
}

// Climbs from whatever the user picked to the folder holding MortalShell2/.
std::optional<fs::path> find_game_root(fs::path p) {
    p = tidy(p);
    if (is_file(p)) p = p.parent_path();
    if (!is_dir(p)) return std::nullopt;
    fs::path cur = p;
    for (int i = 0; i < 7; ++i) {
        if (has_exe(cur / "MortalShell2" / "Binaries" / "Win64")) return cur;
        if (has_exe(cur / "Binaries" / "Win64")) return cur.parent_path();
        if (has_exe(cur)) return cur.parent_path().parent_path().parent_path();
        auto up = cur.parent_path();
        if (up == cur || up.empty()) break;
        cur = up;
    }
    // A pick one or two levels too high: the library, steamapps or common.
    for (const char* sub : {"Sparta", "common/Sparta", "steamapps/common/Sparta"}) {
        auto root = p / fs::path(sub);
        if (has_exe(root / "MortalShell2" / "Binaries" / "Win64")) return root;
    }
    return std::nullopt;
}

Paths make_paths(const fs::path& game_root) {
    Paths r;
    r.game_root = game_root;
    r.project = game_root / "MortalShell2";
    r.win64 = r.project / "Binaries" / "Win64";
    r.ue4ss = r.win64 / "ue4ss";
    // Old UE4SS layouts put UE4SS.dll and Mods/ straight into Win64.
    if (!is_dir(r.ue4ss) && is_file(r.win64 / "UE4SS.dll")) r.ue4ss = r.win64;
    r.mods = r.ue4ss / "Mods";
    r.paks = r.project / "Content" / "Paks";
    r.saved = find_saved(game_root);
    auto acf = game_root.parent_path().parent_path() / (std::string("appmanifest_") + kAppId + ".acf");
    if (is_file(acf)) r.steam_app_manifest = acf;
    r.running_under_wine = detail::platform_running_under_wine();
    return r;
}

void add_unique(std::vector<Paths>& out, std::set<std::string>& seen, Paths p) {
    if (seen.insert(dedupe_key(p.game_root)).second) out.push_back(std::move(p));
}
}  // namespace

namespace detail {
#ifdef _WIN32
std::vector<fs::path> platform_steam_roots() { std::vector<fs::path> v; wcmg_win_steam_roots(&v); return v; }
std::vector<Candidate> platform_extra_candidates() { std::vector<Candidate> v; wcmg_win_extra_candidates(&v); return v; }
bool platform_running_under_wine() {
    static const bool wine = wcmg_win_under_wine() != 0;
    return wine;
}
#else
std::vector<fs::path> platform_steam_roots() { return {}; }
std::vector<Candidate> platform_extra_candidates() { return {}; }
bool platform_running_under_wine() { return false; }
#endif

fs::path path_from_utf8(const std::string& s) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

fs::path env_path(const char* name) {
#ifdef _WIN32
    std::wstring wname(name, name + std::char_traits<char>::length(name));
    const wchar_t* v = _wgetenv(wname.c_str());
    if (!v || !*v) return {};
    return fs::path(v);
#else
    const char* v = std::getenv(name);
    if (!v || !*v) return {};
    return fs::path(v);
#endif
}

std::vector<KvEntry> parse_keyvalues(const std::string& text) {
    enum class T { Str, Open, Close, Cond, End };
    size_t i = 0;
    std::string tok;
    auto next = [&]() -> T {
        for (;;) {
            while (i < text.size() && std::isspace((unsigned char)text[i])) ++i;
            if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
                while (i < text.size() && text[i] != '\n') ++i;
                continue;
            }
            break;
        }
        if (i >= text.size()) return T::End;
        char c = text[i];
        if (c == '{') { ++i; return T::Open; }
        if (c == '}') { ++i; return T::Close; }
        tok.clear();
        if (c == '"') {
            ++i;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    char e = text[i + 1];
                    if (e == '\\' || e == '"') tok += e;
                    else if (e == 'n') tok += '\n';
                    else if (e == 't') tok += '\t';
                    else { tok += '\\'; tok += e; }
                    i += 2;
                } else tok += text[i++];
            }
            if (i < text.size()) ++i;  // closing quote; an unterminated string just ends the file
            return T::Str;
        }
        while (i < text.size() && !std::isspace((unsigned char)text[i]) && text[i] != '{' && text[i] != '}' && text[i] != '"')
            tok += text[i++];
        if (tok.size() >= 2 && tok.front() == '[' && tok.back() == ']') return T::Cond;
        return T::Str;
    };

    std::vector<KvEntry> out;
    std::vector<std::string> stack;
    std::string pending_key;
    bool have_key = false;
    for (T t = next(); t != T::End; t = next()) {
        switch (t) {
        case T::Cond: break;  // platform conditionals like [$WIN32]; nothing here needs them
        case T::Open:
            stack.push_back(have_key ? pending_key : std::string());
            have_key = false;
            break;
        case T::Close:
            if (!stack.empty()) stack.pop_back();
            have_key = false;
            break;
        case T::Str:
            if (!have_key) { pending_key = tok; have_key = true; }
            else {
                KvEntry e;
                e.keys = stack;
                e.keys.push_back(pending_key);
                e.value = tok;
                out.push_back(std::move(e));
                have_key = false;
            }
            break;
        case T::End: break;
        }
    }
    return out;
}

std::vector<SteamLibrary> parse_libraryfolders(const std::string& vdf_text) {
    std::vector<std::string> order;
    std::map<std::string, SteamLibrary> by_id;
    auto lib = [&](const std::string& id) -> SteamLibrary& {
        if (!by_id.count(id)) order.push_back(id);
        return by_id[id];
    };
    auto numeric = [](const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
    };
    for (auto& e : parse_keyvalues(vdf_text)) {
        auto n = e.keys.size();
        if (n >= 3 && lower(e.keys[n - 1]) == "path") lib(e.keys[n - 2]).path = host_path(e.value);
        else if (n >= 4 && lower(e.keys[n - 2]) == "apps") lib(e.keys[n - 3]).apps.push_back(e.keys[n - 1]);
        else if (n == 2 && numeric(e.keys[1]) && !e.value.empty() && !numeric(e.value))
            lib(e.keys[1]).path = host_path(e.value);  // pre-2021 format: "1" "D:\\SteamLibrary"
    }
    std::vector<SteamLibrary> out;
    for (auto& id : order)
        if (!by_id[id].path.empty()) out.push_back(by_id[id]);
    return out;
}

std::string acf_value(const std::string& acf_text, const std::string& key) {
    auto want = lower(key);
    for (auto& e : parse_keyvalues(acf_text))
        if (e.keys.size() == 2 && lower(e.keys[1]) == want) return e.value;
    return "";
}

std::vector<Paths> discover_from_steam_roots(const std::vector<fs::path>& roots) {
    std::vector<fs::path> libraries;
    std::set<std::string> seen_libs;
    auto add_lib = [&](const fs::path& p) {
        if (!p.empty() && is_dir(p) && seen_libs.insert(dedupe_key(p)).second) libraries.push_back(p);
    };
    for (auto& root : roots) {
        // Libraries that list the game come first.
        std::vector<SteamLibrary> parsed;
        for (auto vdf : {root / "steamapps" / "libraryfolders.vdf", root / "config" / "libraryfolders.vdf"})
            if (is_file(vdf)) {
                auto libs = parse_libraryfolders(read_text(vdf, 4 * 1024 * 1024));
                parsed.insert(parsed.end(), libs.begin(), libs.end());
            }
        std::stable_partition(parsed.begin(), parsed.end(), [](const SteamLibrary& l) {
            return std::find(l.apps.begin(), l.apps.end(), kAppId) != l.apps.end();
        });
        for (auto& l : parsed) add_lib(l.path);
        add_lib(root);
    }

    std::vector<Paths> out;
    std::set<std::string> seen;
    for (auto& lib : libraries) {
        auto steamapps = lib / "steamapps";
        auto acf = steamapps / (std::string("appmanifest_") + kAppId + ".acf");
        std::vector<std::string> dirs;
        if (is_file(acf)) {
            auto installdir = acf_value(read_text(acf, 1024 * 1024), "installdir");
            if (!installdir.empty()) dirs.push_back(installdir);
        }
        if (std::find(dirs.begin(), dirs.end(), "Sparta") == dirs.end()) dirs.push_back("Sparta");
        for (auto& d : dirs) {
            auto root = steamapps / "common" / path_from_utf8(d);
            if (!has_exe(root / "MortalShell2" / "Binaries" / "Win64")) continue;
            auto p = make_paths(root);
            if (is_file(acf)) p.steam_app_manifest = acf;
            p.how_found = "Steam library " + native_utf8(lib);
            add_unique(out, seen, std::move(p));
        }
    }
    return out;
}
}  // namespace detail

std::optional<Paths> paths_from_user_pick(const fs::path& any_dir_or_exe) {
    if (any_dir_or_exe.empty()) return std::nullopt;
    auto root = find_game_root(any_dir_or_exe);
    if (!root) return std::nullopt;
    auto p = make_paths(*root);
    p.how_found = "user pick";
    return p;
}

std::vector<Paths> discover_installs() {
    auto roots = home_steam_roots();
    auto win = detail::platform_steam_roots();
    roots.insert(roots.begin(), win.begin(), win.end());
    auto found = detail::discover_from_steam_roots(roots);

    std::vector<Paths> out;
    std::set<std::string> seen;
    for (auto& p : found) add_unique(out, seen, std::move(p));
    for (auto& c : detail::platform_extra_candidates()) {
        auto p = paths_from_user_pick(c.dir);
        if (!p || !validate_paths(*p, nullptr)) continue;
        p->how_found = c.how;
        add_unique(out, seen, std::move(*p));
    }
    return out;
}

bool validate_paths(const Paths& p, std::string* why) {
    if (p.win64.empty()) {
        if (why) *why = "No game folder given.";
        return false;
    }
    if (has_exe(p.win64)) return true;
    if (why) *why = std::string(kExe) + " not found under " + path_utf8(p.win64);
    return false;
}
}  // namespace wcmg

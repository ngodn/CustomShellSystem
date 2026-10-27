// Rough risk scan of a UE4SS Lua mod: counts call sites of the UE4SS APIs that
// tend to matter in a crash (hooks, game-thread work, raw memory, native DLLs).
// It tokenizes enough Lua to skip comments and keep string literals apart from
// code; it does not parse. False positives are fine, this feeds a heuristic.
#include "scanners.hpp"
#include <algorithm>
#include <cctype>
#include <set>

namespace wcmg {
namespace {

enum class Tok { Ident, String, Number, Punct };
struct Token { Tok kind; std::string text; };

bool ident_start(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
bool ident_char(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// Level of a long bracket "[==[" at s[i], or -1 when s[i] does not open one.
int long_bracket_level(const std::string& s, size_t i) {
    if (i >= s.size() || s[i] != '[') return -1;
    size_t j = i + 1;
    int level = 0;
    while (j < s.size() && s[j] == '=') { ++level; ++j; }
    return (j < s.size() && s[j] == '[') ? level : -1;
}

size_t skip_long_bracket(const std::string& s, size_t i, int level, std::string* body) {
    size_t open_len = (size_t)level + 2;
    std::string close = "]" + std::string((size_t)level, '=') + "]";
    auto end = s.find(close, i + open_len);
    if (end == std::string::npos) {
        if (body) *body = s.substr(i + open_len);
        return s.size();
    }
    if (body) *body = s.substr(i + open_len, end - i - open_len);
    return end + close.size();
}

std::vector<Token> tokenize(const std::string& s) {
    std::vector<Token> out;
    size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (std::isspace((unsigned char)c)) { ++i; continue; }
        if (c == '-' && i + 1 < s.size() && s[i + 1] == '-') {
            int level = long_bracket_level(s, i + 2);
            if (level >= 0) { i = skip_long_bracket(s, i + 2, level, nullptr); continue; }
            auto nl = s.find('\n', i);
            i = nl == std::string::npos ? s.size() : nl + 1;
            continue;
        }
        if (c == '"' || c == '\'') {
            std::string v;
            size_t j = i + 1;
            while (j < s.size() && s[j] != c && s[j] != '\n') {
                if (s[j] == '\\' && j + 1 < s.size()) {
                    char e = s[j + 1];
                    v += e == 'n' ? '\n' : e == 't' ? '\t' : e;
                    j += 2;
                    continue;
                }
                v += s[j++];
            }
            out.push_back({Tok::String, v});
            i = j < s.size() ? j + 1 : j;
            continue;
        }
        if (c == '[') {
            int level = long_bracket_level(s, i);
            if (level >= 0) {
                std::string body;
                i = skip_long_bracket(s, i, level, &body);
                out.push_back({Tok::String, body});
                continue;
            }
        }
        if (ident_start(c)) {
            size_t j = i;
            while (j < s.size() && ident_char(s[j])) ++j;
            out.push_back({Tok::Ident, s.substr(i, j - i)});
            i = j;
            continue;
        }
        if (std::isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '.')) ++j;
            out.push_back({Tok::Number, s.substr(i, j - i)});
            i = j;
            continue;
        }
        out.push_back({Tok::Punct, std::string(1, c)});
        ++i;
    }
    return out;
}

bool ends_with_ci(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return lower(s.substr(s.size() - suffix.size())) == lower(suffix);
}

bool starts_with(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

bool is_hook_api(const std::string& n) {
    return (starts_with(n, "Register") && ends_with_ci(n, "Hook")) || n == "RegisterCustomEvent" || n == "NotifyOnNewObject";
}

bool is_game_thread_api(const std::string& n) {
    static const std::set<std::string> names = {
        "ExecuteInGameThread", "ExecuteWithDelay", "ExecuteAsync", "LoopAsync",
        "ExecuteInGameThreadWithDelay", "ExecuteInGameThreadAfterFrames",
        "LoopInGameThreadWithDelay", "LoopInGameThreadAfterFrames"};
    return names.count(n) != 0;
}

bool is_raw_memory_api(const std::string& n) {
    static const std::set<std::string> names = {
        "GetAddress", "ReadMemory", "WriteMemory", "GetValuePtr", "ContainerPtrToValuePtr",
        "GetStructAddress", "GetPropertyAddress", "GetBaseAddress", "GetArrayDataAddress", "GetArrayAddress"};
    return names.count(n) != 0;
}

bool per_frame_function(const std::string& fn) {
    auto colon = fn.rfind(':');
    std::string f = colon == std::string::npos ? fn : fn.substr(colon + 1);
    return f == "ReceiveTick" || f == "Tick" || f == "K2_Tick" || f == "TickComponent" || f == "ReceiveDrawHUD" ||
           f == "OnPaint" || ends_with_ci(f, "Tick");
}

double number_value(const std::string& t) {
    try { return std::stod(t); } catch (...) { return -1; }
}

} // namespace

void scan_lua_mod(Ue4ssMod& mod) {
    try {
        mod.hooks = mod.game_thread_calls = mod.raw_memory_sites = mod.native_loads = mod.shell_sites = 0;
        mod.hooked_functions.clear();
        mod.risk_notes.clear();

        if (mod.lua_files.empty() && !mod.folder.empty()) {
            std::error_code ec;
            size_t seen = 0;
            for (fs::recursive_directory_iterator it(mod.folder, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec)) {
                if (++seen > 60000) break;
                std::error_code fec;
                if (it->is_regular_file(fec) && lower(path_utf8(it->path().extension())) == ".lua") {
                    mod.lua_files.push_back(it->path());
                    mod.lua_bytes += it->file_size(fec);
                }
            }
            std::sort(mod.lua_files.begin(), mod.lua_files.end());
        }

        std::vector<std::string> notes;
        auto note = [&](const std::string& n) {
            if (notes.size() < 20 && std::find(notes.begin(), notes.end(), n) == notes.end()) notes.push_back(n);
        };
        std::set<std::string> hooked_seen;
        std::vector<std::string> dll_literals;
        int write_sites = 0, ffi_sites = 0;
        int native_load_calls = 0;

        for (const auto& file : mod.lua_files) {
            std::string text;
            try { text = read_text(file, 2 * 1024 * 1024); } catch (...) { continue; }
            auto toks = tokenize(text);
            for (size_t i = 0; i < toks.size(); ++i) {
                const auto& t = toks[i];
                if (t.kind == Tok::String) {
                    if (ends_with_ci(t.text, ".dll")) dll_literals.push_back(t.text);
                    if (t.text.find("LoadLibrary") != std::string::npos) ++mod.native_loads;
                    continue;
                }
                if (t.kind != Tok::Ident) continue;
                const Token* prev = i > 0 ? &toks[i - 1] : nullptr;
                const Token* prev2 = i > 1 ? &toks[i - 2] : nullptr;
                const Token* next = i + 1 < toks.size() ? &toks[i + 1] : nullptr;
                bool member = prev && prev->kind == Tok::Punct && (prev->text == "." || prev->text == ":");
                std::string owner = member && prev2 && prev2->kind == Tok::Ident ? prev2->text : std::string();
                bool definition = prev && prev->kind == Tok::Ident && prev->text == "function";
                if (member && prev2 && prev2->kind == Tok::Ident && i > 2 && toks[i - 3].kind == Tok::Ident && toks[i - 3].text == "function")
                    definition = true;

                if (t.text == "ffi" && next && next->kind == Tok::Punct && next->text == ".") { ++ffi_sites; continue; }

                bool call = next && ((next->kind == Tok::Punct && (next->text == "(" || next->text == "{")) || next->kind == Tok::String);
                if (!call || definition) continue;

                // First argument when it is a literal.
                const Token* arg = nullptr;
                if (next->kind == Tok::String) arg = next;
                else if (next->text == "(" && i + 2 < toks.size()) arg = &toks[i + 2];
                std::string str_arg = arg && arg->kind == Tok::String ? arg->text : std::string();
                std::string num_arg = arg && arg->kind == Tok::Number ? arg->text : std::string();

                const auto& n = t.text;
                if (!member && is_hook_api(n)) {
                    ++mod.hooks;
                    if (n == "RegisterHook" && !str_arg.empty()) {
                        if (hooked_seen.insert(str_arg).second && mod.hooked_functions.size() < 50) mod.hooked_functions.push_back(str_arg);
                        if (per_frame_function(str_arg)) {
                            bool every_actor = str_arg.find("Engine.Actor:") != std::string::npos;
                            note("RegisterHook on " + str_arg + " runs every frame" + (every_actor ? " for every actor" : ""));
                        }
                    }
                    if (n == "NotifyOnNewObject" && !str_arg.empty()) {
                        if (str_arg == "/Script/Engine.Actor") note("NotifyOnNewObject on /Script/Engine.Actor fires for every actor spawned");
                        else if (str_arg == "/Script/CoreUObject.Object") note("NotifyOnNewObject on /Script/CoreUObject.Object fires for every object created");
                    }
                } else if (!member && is_game_thread_api(n)) {
                    ++mod.game_thread_calls;
                    if (!num_arg.empty() && (n == "LoopAsync" || n == "LoopInGameThreadWithDelay")) {
                        double ms = number_value(num_arg);
                        if (ms >= 0 && ms < 16) note(n + " with " + num_arg + " ms interval");
                    }
                } else if (is_raw_memory_api(n)) {
                    ++mod.raw_memory_sites;
                    if (n == "WriteMemory") ++write_sites;
                } else if (n == "loadlib" && owner == "package") {
                    ++mod.native_loads;
                    ++native_load_calls;
                } else if (n == "require" && !member && ends_with_ci(str_arg, ".dll")) {
                    ++mod.native_loads;
                    ++native_load_calls;
                } else if (n == "require" && !member && str_arg == "ffi") {
                    ++ffi_sites;
                } else if (starts_with(n, "LoadLibrary")) {
                    ++mod.native_loads;
                    ++native_load_calls;
                } else if ((n == "execute" && owner == "os") || (n == "popen" && owner == "io")) {
                    ++mod.shell_sites;
                }
            }
        }
        mod.raw_memory_sites += ffi_sites;

        if (native_load_calls > 0 || mod.native_loads > 0) {
            // Name the DLL by matching the literals against what is in the folder.
            std::vector<std::string> named;
            for (const auto& lit : dll_literals) {
                auto base = lower(path_utf8(fs::path(lit).filename()));
                auto slash = lit.find_last_of("\\/");
                if (slash != std::string::npos) base = lower(lit.substr(slash + 1));
                std::string shown = lit;
                for (const auto& d : mod.dlls) {
                    if (lower(path_utf8(d.filename())) == base) {
                        std::error_code ec;
                        auto rel = fs::relative(d, mod.folder, ec);
                        shown = ec ? path_utf8(d.filename()) : path_utf8(rel);
                        break;
                    }
                }
                if (std::find(named.begin(), named.end(), shown) == named.end()) named.push_back(shown);
            }
            if (named.empty()) note("loads a native DLL from Lua");
            for (const auto& d : named) note("loads a native DLL from Lua: " + d);
        }
        if (write_sites > 0) note("writes raw memory");
        if (ffi_sites > 0) note("uses the LuaJIT ffi for raw memory access");
        if (mod.raw_memory_sites > write_sites + ffi_sites && write_sites == 0)
            note("reads raw object addresses (" + std::to_string(mod.raw_memory_sites - ffi_sites) + " GetAddress-style sites)");
        if (mod.shell_sites > 0) note("runs shell commands (os.execute / io.popen)");
        if (mod.hooks >= 25) note("registers " + std::to_string(mod.hooks) + " hooks");
        mod.risk_notes = std::move(notes);
    } catch (...) {
        if (mod.risk_notes.size() < 20) mod.risk_notes.push_back("Lua scan stopped early on an unreadable file");
    }
}

} // namespace wcmg

// Report writers: plain text for the window and the console, Markdown for the
// bundle and for pasting into a forum post, JSON for tools. All three read the
// same Report; none of them touches the disk.
#include "scanners.hpp"
#include <nlohmann/json.hpp>
#include <map>
#include <sstream>

namespace wcmg {
namespace {
using Json = nlohmann::json;

std::string time_now(std::chrono::system_clock::time_point t) {
    auto tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

std::string upper_first(std::string s) {
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    return s;
}

// Shared structure for text and Markdown; `md` toggles the heading and list syntax.
struct Writer {
    const Report& r;
    bool md;
    std::ostringstream o;

    void h1(const std::string& s) { o << (md ? "# " : "") << s << "\n" << (md ? "" : std::string(s.size(), '=') + "\n") << "\n"; }
    void h2(const std::string& s) { o << (md ? "## " : "") << s << "\n" << (md ? "" : std::string(s.size(), '-') + "\n") << "\n"; }
    void h3(const std::string& s) { o << (md ? "### " : "") << s << "\n\n"; }
    void p(const std::string& s) { if (!s.empty()) o << s << "\n\n"; }
    void li(const std::string& s) { o << (md ? "- " : "  * ") << s << "\n"; }
    void kv(const std::string& k, const std::string& v) { if (!v.empty()) li((md ? "**" + k + "**" : k) + ": " + v); }
    void end_list() { o << "\n"; }
    void code(const std::string& s) {
        if (md) o << "```\n" << s << (s.ends_with("\n") ? "" : "\n") << "```\n\n";
        else { std::istringstream in(s); std::string line; while (std::getline(in, line)) o << "    " << line << "\n"; o << "\n"; }
    }

    void write() {
        const auto& e = r.evidence;
        h1("Who Crashed My Game");
        p(r.generated_by + ", scanned " + time_now(e.scanned_at) + ". Game folder: " + path_utf8(e.paths.game_root) + " (" + e.paths.how_found + ").");
        h2("Verdict");
        p(r.verdict);

        h2("Findings");
        if (r.findings.empty()) p("Nothing to report.");
        for (const auto& f : r.findings) {
            h3("[" + upper_first(severity_name(f.severity)) + ", " + std::to_string(f.confidence) + "%] " + f.title);
            kv("Blame", f.blame);
            end_list();
            p(f.summary);
            p(f.details);
            if (!f.evidence.empty()) {
                p(md ? "Evidence:" : "Evidence:");
                std::string ev;
                for (const auto& l : f.evidence) ev += l + "\n";
                code(ev);
            }
            if (!f.advice.empty()) p((md ? "**What to do:** " : "What to do: ") + f.advice);
        }

        h2("Crash reports");
        if (e.crashes.empty()) p("None found under " + path_utf8(e.paths.saved / "Crashes") + ".");
        else {
            p(std::to_string(e.crashes.size()) + " report(s), newest first.");
            for (const auto& c : e.crashes) {
                std::string line = format_time(c.when) + "  " + c.crash_type + "  " + c.error_message;
                if (c.seconds_since_start >= 0) line += "  (" + std::to_string(c.seconds_since_start) + " s after launch)";
                li(line);
            }
            end_list();
            const auto& c = e.crashes.front();
            h3("Newest crash in detail");
            kv("Folder", path_utf8(c.folder.filename()));
            kv("Engine", c.engine_version);
            // A GPU crash's "exception" is the crash reporter raising it on purpose, so it says nothing.
            if (c.crash_type != "GPUCrash")
                kv("Exception", c.exception_module.empty() ? "" : c.exception_module + " at 0x" + [&] { char b[32]; std::snprintf(b, sizeof b, "%llx", (unsigned long long)c.exception_address); return std::string(b); }());
            if (!c.gpu_breadcrumb_tail.empty()) { std::string t; for (const auto& s : c.gpu_breadcrumb_tail) t += (t.empty() ? "" : " > ") + s; kv("Active GPU pass", t); }
            for (const char* k : {"Player.Shell", "Player.Level", "Player.Location", "RHI.RHIName", "RHI.AdapterName", "RHI.UserDriverVersion", "DLSSG.Active", "Settings.Upscaler", "Settings.FrameInterpolationEnabled", "Session.SecondsInLevel"})
                if (auto it = c.props.find(k); it != c.props.end()) kv(k, it->second);
            end_list();
            if (!c.frames.empty()) {
                std::string s;
                for (const auto& f : c.frames) { char b[48]; std::snprintf(b, sizeof b, " + 0x%llx", (unsigned long long)f.offset); s += f.module + b + "\n"; }
                p("Call stack from the crash context (top first):");
                code(s);
            } else if (!c.minidump_frames.empty()) {
                std::string s;
                size_t n = 0;
                for (const auto& f : c.minidump_frames) { if (n++ == 24) break; char b[48]; std::snprintf(b, sizeof b, " + 0x%llx", (unsigned long long)f.offset); s += f.module + b + (f.from_stack_scan ? "  (scanned)" : "") + "\n"; }
                p("No call stack in the crash context; candidate return addresses scanned from the minidump (not a proven chain):");
                code(s);
            }
            if (!c.loaded_modules.empty()) {
                std::vector<std::string> notable;
                std::map<std::string, int> seen;
                for (const auto& m : c.loaded_modules) {
                    if (++seen[lower(m)] > 1) continue;
                    auto l = lower(m);
                    if (l.find("ue4ss") != std::string::npos || l.find("_core") != std::string::npos || l == "main.dll" || l.find("mods") != std::string::npos
                        || l.find("overlay") != std::string::npos || l.find("rtss") != std::string::npos || l.find("discord") != std::string::npos || l.find("reshade") != std::string::npos || l.find("sl.") == 0 || l.find("nvngx") == 0)
                        notable.push_back(m);
                }
                kv("Modules in the process", std::to_string(c.loaded_modules.size()) + (notable.empty() ? "" : "; notable: " + [&] { std::string s; for (const auto& n : notable) s += (s.empty() ? "" : ", ") + n; return s; }()));
                end_list();
            }
            if (!c.minidump_error.empty()) p("Minidump: " + c.minidump_error);
        }

        h2("UE4SS");
        const auto& u = e.ue4ss;
        if (!u.installed) p("Not installed (no UE4SS.dll under " + path_utf8(e.paths.ue4ss) + ").");
        else {
            kv("Build", u.version_line.empty() ? "unknown (no version line in the log)" : u.version_line);
            kv("DLL", format_bytes(u.dll_bytes) + ", sha256 " + u.dll_sha256);
            kv("Loader", u.proxy_dll.empty() ? "none found beside the exe" : u.proxy_dll);
            if (!u.other_proxy_dlls.empty()) { std::string s; for (const auto& d : u.other_proxy_dlls) s += (s.empty() ? "" : ", ") + d; kv("Other injected DLLs", s); }
            kv("Log", path_utf8(u.log.filename()) + " from " + format_time(u.log_when) + ", " + std::to_string(u.errors.size()) + " error line(s), " + std::to_string(u.lua_errors.size()) + " Lua error line(s)");
            kv("Graphics API for the GUI console", u.graphics_api);
            end_list();
            h3("Mods");
            for (const auto& m : u.mods) {
                if (m.name == "shared") continue;
                std::string kind = m.has_dll && m.has_scripts ? "Lua + C++" : m.has_dll ? "C++" : m.has_scripts ? "Lua" : "no entry point";
                std::string state = m.effectively_enabled ? "enabled" : "disabled";
                if (m.enabled_txt) state += " (enabled.txt)";
                std::string line = m.name + ": " + kind + ", " + state + (m.builtin ? ", UE4SS built-in" : "") + (m.ours ? ", eins0fx" : "");
                if (m.hooks || m.game_thread_calls || m.raw_memory_sites) line += ", hooks " + std::to_string(m.hooks) + ", game-thread calls " + std::to_string(m.game_thread_calls) + ", raw memory " + std::to_string(m.raw_memory_sites);
                if (m.newest_file != FileTime{}) line += ", newest file " + format_time(m.newest_file);
                li(line);
            }
            end_list();
        }

        h2("CSS, CSSX and CCS");
        bool any = false;
        for (const auto& m : e.ours) {
            if (!m.installed) continue;
            any = true;
            std::string line = m.name + " " + (m.release_version.empty() ? "(version unknown)" : m.release_version);
            if (!m.active_core.empty()) line += ", core " + m.active_core + (m.active_core_present ? "" : " (missing)");
            if (m.loader_mismatch) line += ", loader mismatch";
            line += ", log " + std::to_string(m.errors.size()) + " error(s), " + std::to_string(m.warnings.size()) + " warning(s)";
            if (m.corrupt_state_files) line += ", " + std::to_string(m.corrupt_state_files) + " corrupt state file(s)";
            if (!m.extensions.empty()) { line += ", extensions: "; for (size_t i = 0; i < m.extensions.size(); ++i) line += (i ? ", " : "") + m.extensions[i]; }
            li(line);
            for (const auto& [k, v] : m.notes) li("  " + k + ": " + v);
        }
        if (!any) p("None installed.");
        else end_list();

        h2("Pak containers");
        size_t game = 0, mods = 0;
        for (const auto& c : e.paks.containers) (c.is_game ? game : mods)++;
        p(std::to_string(game) + " game container(s), " + std::to_string(mods) + " mod container(s), " + std::to_string(e.paks.game_package_ids) + " game packages known, " + std::to_string(e.paks.mod_collisions.size()) + " collision(s) between mods.");
        for (const auto& c : e.paks.containers) {
            if (!c.is_mod) continue;
            std::string line = c.stem + ": " + std::to_string(c.package_ids.size()) + " package(s), " + format_bytes(c.pak_bytes + c.utoc_bytes + c.ucas_bytes);
            if (c.when != FileTime{}) line += ", " + format_time(c.when);
            if (c.css_package) line += ", CSS outfit";
            if (c.cssx_package) line += ", CSSX";
            if (c.overrides_game) line += ", overrides " + std::to_string(c.overrides_game) + " game package(s)";
            if (!c.complete) line += ", INCOMPLETE";
            if (!c.problem.empty()) line += ", " + c.problem;
            li(line);
        }
        end_list();
        if (!e.paks.error.empty()) p("Scan note: " + e.paks.error);

        h2("Game files");
        const auto& m = e.manifest;
        if (!m.present) p("No build manifest found; file checks skipped.");
        else {
            kv("Steam build", m.steam_build_id.empty() ? "unknown" : m.steam_build_id);
            kv("Executable", format_bytes(m.exe_bytes) + ", " + format_time(m.exe_when) + ", sha256 " + m.exe_sha256);
            kv("Manifest entries", std::to_string(m.manifest_entries));
            kv("Missing", std::to_string(m.missing_total));
            kv("Extra files", std::to_string(m.foreign_total));
            end_list();
            if (!m.foreign.empty()) {
                std::string s; size_t n = 0;
                for (const auto& f : m.foreign) { if (n++ == 15) { s += "... " + std::to_string(m.foreign_total - 15) + " more\n"; break; } s += f + "\n"; }
                p("Extra files:"); code(s);
            }
            if (!m.missing.empty()) { std::string s; for (const auto& f : m.missing) s += f + "\n"; p("Missing files (first " + std::to_string(m.missing.size()) + "):"); code(s); }
        }

        h2("System");
        const auto& s = e.system;
        kv("OS", s.os);
        kv("CPU", s.cpu);
        kv("GPU", s.gpu + (s.gpu_driver.empty() ? "" : ", driver " + s.gpu_driver));
        kv("RHI", s.rhi);
        if (s.total_ram) kv("RAM", format_bytes(s.total_ram));
        kv("Game under Wine/Proton", s.game_ran_under_wine ? "yes" : "");
        kv("WCMG under Wine", e.paths.running_under_wine ? "yes" : "");
        if (!s.overlays_seen.empty()) { std::string t; for (const auto& x : s.overlays_seen) t += (t.empty() ? "" : ", ") + x; kv("Overlays seen in crashes", t); }
        for (const auto& n : s.notes) li(n);
        end_list();
        if (!s.engine_ini.empty()) { std::string t; for (const auto& [k, v] : s.engine_ini) t += k + "=" + v + "\n"; p("Engine.ini:"); code(t); }

        if (!e.scan_errors.empty()) { h2("Scan errors"); for (const auto& x : e.scan_errors) li(x); end_list(); }
        p("Knowledge file " + e.knowledge.version + ".");
    }
};

Json finding_json(const Finding& f) {
    return {{"id", f.id}, {"severity", severity_name(f.severity)}, {"confidence", f.confidence}, {"title", f.title}, {"blame", f.blame},
            {"summary", f.summary}, {"details", f.details}, {"evidence", f.evidence}, {"advice", f.advice}};
}
}

std::string report_text(const Report& r) { Writer w{r, false, {}}; w.write(); return w.o.str(); }
std::string report_markdown(const Report& r) { Writer w{r, true, {}}; w.write(); return w.o.str(); }

std::string report_json(const Report& r) {
    const auto& e = r.evidence;
    Json j;
    j["generated_by"] = r.generated_by;
    j["scanned_at"] = time_now(e.scanned_at);
    j["verdict"] = r.verdict;
    j["paths"] = {{"game_root", path_utf8(e.paths.game_root)}, {"saved", path_utf8(e.paths.saved)}, {"how_found", e.paths.how_found}, {"running_under_wine", e.paths.running_under_wine}};
    j["findings"] = Json::array();
    for (const auto& f : r.findings) j["findings"].push_back(finding_json(f));
    j["crashes"] = Json::array();
    for (const auto& c : e.crashes) {
        Json cj = {{"folder", path_utf8(c.folder.filename())}, {"when", format_time(c.when)}, {"type", c.crash_type}, {"error", c.error_message},
                   {"seconds_since_start", c.seconds_since_start}, {"engine_version", c.engine_version}, {"callstack_hash", c.callstack_hash},
                   {"exception_module", c.exception_module}, {"exception_address", c.exception_address}, {"gpu_breadcrumbs", c.gpu_breadcrumb_tail},
                   {"loaded_modules", c.loaded_modules}, {"props", c.props}, {"minidump_error", c.minidump_error}};
        cj["frames"] = Json::array();
        for (const auto& f : c.frames) cj["frames"].push_back({{"module", f.module}, {"base", f.base}, {"offset", f.offset}});
        cj["scanned_frames"] = Json::array();
        for (const auto& f : c.minidump_frames) cj["scanned_frames"].push_back({{"module", f.module}, {"base", f.base}, {"offset", f.offset}, {"scanned", f.from_stack_scan}});
        j["crashes"].push_back(cj);
    }
    const auto& u = e.ue4ss;
    Json uj = {{"installed", u.installed}, {"dll_sha256", u.dll_sha256}, {"dll_bytes", u.dll_bytes}, {"proxy_dll", u.proxy_dll}, {"other_proxy_dlls", u.other_proxy_dlls},
               {"version_line", u.version_line}, {"git_sha", u.git_sha}, {"settings", u.settings}, {"mods_txt_order", u.mods_txt_order}, {"log", path_utf8(u.log)},
               {"log_when", format_time(u.log_when)}, {"started_mods", u.started_mods}, {"log_reached_game_thread", u.log_reached_game_thread}};
    auto lines = [](const std::vector<Ue4ssLogLine>& v) { Json a = Json::array(); for (const auto& l : v) a.push_back(l.time + " " + l.text); return a; };
    uj["errors"] = lines(u.errors); uj["warnings"] = lines(u.warnings); uj["lua_errors"] = lines(u.lua_errors); uj["tail"] = lines(u.tail);
    uj["crash_dumps"] = Json::array();
    for (const auto& d : u.crash_dumps) uj["crash_dumps"].push_back(path_utf8(d.filename()));
    uj["mods"] = Json::array();
    for (const auto& m : u.mods) {
        Json mj = {{"name", m.name}, {"lua", m.has_scripts}, {"dll", m.has_dll}, {"enabled", m.effectively_enabled}, {"enabled_txt", m.enabled_txt}, {"builtin", m.builtin}, {"ours", m.ours},
                   {"hooks", m.hooks}, {"game_thread_calls", m.game_thread_calls}, {"raw_memory_sites", m.raw_memory_sites}, {"native_loads", m.native_loads},
                   {"hooked_functions", m.hooked_functions}, {"risk_notes", m.risk_notes}, {"newest_file", format_time(m.newest_file)}, {"lua_files", m.lua_files.size()}};
        mj["dlls"] = Json::array();
        for (size_t i = 0; i < m.dlls.size(); ++i) mj["dlls"].push_back({{"file", path_utf8(m.dlls[i].filename())}, {"bytes", i < m.dll_bytes.size() ? m.dll_bytes[i] : 0}, {"sha256", i < m.dll_sha256.size() ? m.dll_sha256[i] : ""}});
        uj["mods"].push_back(mj);
    }
    j["ue4ss"] = uj;
    j["ours"] = Json::array();
    for (const auto& m : e.ours)
        j["ours"].push_back({{"name", m.name}, {"installed", m.installed}, {"release_version", m.release_version}, {"active_core", m.active_core}, {"active_core_present", m.active_core_present},
                             {"cores", m.cores}, {"loader_mismatch", m.loader_mismatch}, {"errors", m.errors}, {"warnings", m.warnings}, {"tail", m.tail},
                             {"corrupt_state_files", m.corrupt_state_files}, {"dev_channel_enabled", m.dev_channel_enabled}, {"extensions", m.extensions}, {"notes", m.notes}});
    j["paks"] = {{"game_package_ids", e.paks.game_package_ids}, {"stray_files", e.paks.stray_files}, {"mods_outside_mods_folder", e.paks.mods_outside_mods_folder}, {"error", e.paks.error}};
    j["paks"]["containers"] = Json::array();
    for (const auto& c : e.paks.containers)
        j["paks"]["containers"].push_back({{"stem", c.stem}, {"folder", path_utf8(c.folder)}, {"pak_bytes", c.pak_bytes}, {"utoc_bytes", c.utoc_bytes}, {"ucas_bytes", c.ucas_bytes},
                                           {"is_patch", c.is_patch}, {"is_game", c.is_game}, {"is_mod", c.is_mod}, {"complete", c.complete}, {"problem", c.problem}, {"css_package", c.css_package},
                                           {"cssx_package", c.cssx_package}, {"chunks", c.chunks}, {"packages", c.package_ids.size()}, {"names_known", c.names.size()},
                                           {"overrides_game", c.overrides_game}, {"override_names", c.override_names}, {"override_hints", c.override_hints}, {"when", format_time(c.when)}});
    j["paks"]["collisions"] = Json::array();
    for (const auto& c : e.paks.mod_collisions) j["paks"]["collisions"].push_back({{"package_id", c.package_id}, {"name", c.name}, {"containers", c.containers}});
    const auto& m = e.manifest;
    j["manifest"] = {{"present", m.present}, {"entries", m.manifest_entries}, {"missing", m.missing}, {"missing_total", m.missing_total}, {"foreign", m.foreign}, {"foreign_total", m.foreign_total},
                     {"replaced_binaries", m.replaced_binaries}, {"steam_build_id", m.steam_build_id}, {"steam_last_updated", m.steam_last_updated}, {"exe_sha256", m.exe_sha256}, {"exe_bytes", m.exe_bytes}, {"exe_when", format_time(m.exe_when)}};
    const auto& s = e.system;
    j["system"] = {{"os", s.os}, {"game_ran_under_wine", s.game_ran_under_wine}, {"cpu", s.cpu}, {"gpu", s.gpu}, {"gpu_driver", s.gpu_driver}, {"rhi", s.rhi}, {"total_ram", s.total_ram},
                   {"engine_ini", s.engine_ini}, {"game_user_settings", s.game_user_settings}, {"dlss_frame_gen_forced", s.dlss_frame_gen_forced}, {"fsr_frame_interp", s.fsr_frame_interp},
                   {"overlays_seen", s.overlays_seen}, {"notes", s.notes}};
    j["scan_errors"] = e.scan_errors;
    j["knowledge_version"] = e.knowledge.version;
    return j.dump(2);
}
}

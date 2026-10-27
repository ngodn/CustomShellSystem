#pragma once
// The scanner entry points. Each one reads part of the installation and fills its
// slice of Evidence. They must never throw out of these functions: catch inside,
// record the problem in the result (or Evidence::scan_errors via the orchestrator)
// and return what was parsed so far. They do not open the game's process and do
// not need the game closed, but they treat every file as possibly in use.
#include "model.hpp"

namespace wcmg {

// crash_context.cpp: Saved/Crashes/UECC-* folders, newest first, at most `limit`.
std::vector<CrashReport> scan_crash_reports(const Paths&, size_t limit = 20);
CrashReport parse_crash_context(const fs::path& runtime_xml);   // one folder's xml

// minidump.cpp: a Windows minidump (UEMinidump.dmp or UE4SS crash_*.dmp).
struct Minidump {
    std::vector<DumpModule> modules;          // in stream order
    uint32_t exception_thread_id = 0;
    uint32_t exception_code = 0;
    uint64_t exception_address = 0;
    uint64_t exception_rip = 0, exception_rsp = 0;   // from the thread CONTEXT when present
    std::vector<uint64_t> exception_params;
    std::string system_info;                  // "Windows 10.0.19045 x64", from SystemInfoStream
    std::string error;                        // non-empty when the file did not parse
    bool ok() const { return error.empty(); }
    // Module holding `address`, or nullptr.
    const DumpModule* module_at(uint64_t address) const;
    // "name 0xbase + offset" for a frame; empty module when unknown.
    Frame frame_for(uint64_t address) const;
};
Minidump read_minidump(const fs::path&);
// Scans the exception thread's stack memory (as captured in the dump) for qwords
// that point into any loaded module's executable range, top of stack first. The
// first element is the faulting RIP when the context is present. This is a
// heuristic unwind: it lists candidates, it does not prove the call chain.
std::vector<Frame> scan_stack(const Minidump&, const fs::path& dump_file, size_t max_frames = 64);
// Fills CrashReport::loaded_modules / minidump_frames / exception_* from the dump.
void attach_minidump(CrashReport&, const fs::path& dump_file);

// ue4ss.cpp: UE4SS install, settings, mods list, the log, the proxy DLL.
Ue4ssInfo scan_ue4ss(const Paths&, const Knowledge&);
// lua_scan.cpp: fills the risk counters of one mod from its Scripts/.
void scan_lua_mod(Ue4ssMod&);
// our_mods.cpp: CustomShellSystem, CSSX, CCS specifics.
std::vector<OurModInfo> scan_our_mods(const Paths&);

// paks.cpp: every container in Paks/, ~mods/, LogicMods/ with package ids and collisions.
PakInfo scan_paks(const Paths&, const Progress& = {});
// manifest.cpp: shipped file list vs disk, Steam build id, exe hash.
ManifestInfo scan_manifest(const Paths&, const Progress& = {});

// system.cpp: config inis, crash-context derived hardware facts, live Win32 facts when built for Windows.
SystemInfo scan_system(const Paths&, const std::vector<CrashReport>&, const Knowledge&);

// discovery.cpp: find the game. Portable part reads Steam's libraryfolders.vdf and
// appmanifest; the Windows part adds the registry and common drive roots.
std::vector<Paths> discover_installs();                 // every candidate, best first
std::optional<Paths> paths_from_user_pick(const fs::path& any_dir_or_exe); // Sparta, MortalShell2, Win64 or the exe
bool validate_paths(const Paths&, std::string* why);    // true when the exe exists at paths.win64

// diagnose.cpp: runs everything and the rules.
Report diagnose(const Paths&, const std::optional<fs::path>& knowledge_override, const Progress& = {});
// rules.cpp
std::vector<Finding> run_rules(const Evidence&);
std::string write_verdict(const Evidence&, const std::vector<Finding>&);
// report.cpp
std::string report_text(const Report&);       // plain text for the GUI pane and the console
std::string report_markdown(const Report&);   // for the zip and for pasting into Nexus/GitHub
std::string report_json(const Report&);
// collect.cpp: writes the support bundle. Returns the zip path or throws.
fs::path collect_bundle(const Report&, const fs::path& zip_out, const Progress& = {});
}

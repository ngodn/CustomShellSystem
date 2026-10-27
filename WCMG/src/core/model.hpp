#pragma once
// WCMG, "Who Crashed My Game": the data every scanner fills and the rules read.
//
// Everything in this header is plain data, portable between the Windows build
// and the Linux test/CLI build. Scanners only ever append to Evidence; they never
// decide what a crash means. That job belongs to rules.cpp, which turns Evidence
// into Findings. Keep the structs boring: strings, numbers, vectors, no handles.
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wcmg {
namespace fs = std::filesystem;
using FileTime = fs::file_time_type;

// Where everything lives for one installation. Discovery fills this from the
// Steam library (or the user's pick); every scanner takes it read-only.
struct Paths {
    fs::path game_root;      // .../Sparta  (holds Manifest_*.txt, Engine/, MortalShell2/)
    fs::path project;        // game_root/MortalShell2
    fs::path win64;          // project/Binaries/Win64
    fs::path ue4ss;          // win64/ue4ss
    fs::path mods;           // ue4ss/Mods
    fs::path paks;           // project/Content/Paks
    fs::path saved;          // %LOCALAPPDATA%/MortalShell2/Saved (or the Proton prefix copy)
    fs::path steam_app_manifest; // steamapps/appmanifest_2584270.acf when found
    bool running_under_wine = false; // the diagnosis process itself, not the game
    std::string how_found;   // "steam registry", "steam library vdf", "user pick", ...
};

enum class Severity { Info, Low, Medium, High, Critical };
const char* severity_name(Severity);

// A frame of a UE crash call stack or a scanned minidump stack: module + offset.
struct Frame {
    std::string module;      // "MortalShell2-Win64-Shipping", "UE4SS", "css_core-...", "main"
    uint64_t base = 0;
    uint64_t offset = 0;
    bool from_stack_scan = false; // true when found by scanning stack memory, not a real unwind
};

// One Saved/Crashes/UECC-* folder. Parsed from CrashContext.runtime-xml and the
// UEMinidump.dmp beside it.
struct CrashReport {
    fs::path folder;
    std::string guid;
    FileTime when{};                       // folder mtime
    std::string crash_type;                // "Crash", "GPUCrash", "Assert", "Ensure", "Hang", "OOM"
    std::string error_message;             // "Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x..."
    bool is_assert = false, is_ensure = false, is_stall = false, is_oom = false;
    int seconds_since_start = -1;
    std::string engine_version;            // "5.6.1-0+++Sparta-Depot+Main+CL97404+1875"
    std::string build_version;             // "++Sparta-Depot+Main+CL97404+1875-CL-0"
    std::vector<Frame> frames;             // PCallStack, top first
    std::string callstack_hash;
    std::vector<std::string> gpu_breadcrumb_tail; // last few active markers, innermost last
    std::map<std::string, std::string> props;     // every other flat tag: RHI.*, DLSSG.*, Settings.*, Platform*, Memory*, Player.*
    bool has_minidump = false;
    fs::path minidump;
    uint64_t minidump_bytes = 0;
    // Filled from the minidump when it parses:
    std::vector<std::string> loaded_modules;      // file names only, in load order
    std::vector<Frame> minidump_frames;           // exception-thread frames, scanned
    std::string exception_module;                 // module holding the faulting instruction
    uint64_t exception_code = 0, exception_address = 0;
    std::string minidump_error;                   // why the dump did not parse, if so
};

// A minidump's loaded module, when the GUI wants to list them.
struct DumpModule {
    std::string name;        // full path as recorded in the dump
    uint64_t base = 0, size = 0;
    uint32_t timestamp = 0, checksum = 0;
    std::string file_version; // "3.0.1.0" when the VS_FIXEDFILEINFO is present
};

// One directory under ue4ss/Mods.
struct Ue4ssMod {
    std::string name;
    fs::path folder;
    bool has_scripts = false;      // Scripts/main.lua present
    bool has_dll = false;          // dlls/main.dll present
    bool enabled_txt = false;      // enabled.txt present (always loads, ignores mods.txt)
    std::optional<bool> mods_txt;  // state in mods.txt, if listed
    std::optional<bool> mods_json; // state in mods.json, if listed
    bool effectively_enabled = false;
    bool builtin = false;          // shipped with UE4SS (Keybinds, ConsoleEnablerMod, ...)
    bool ours = false;             // CustomShellSystem, CSSX, CCS
    std::vector<fs::path> dlls;    // every .dll under the folder, with main.dll first
    std::vector<std::string> dll_sha256;  // parallel to dlls
    std::vector<uint64_t> dll_bytes;      // parallel to dlls
    std::vector<fs::path> lua_files;
    uint64_t lua_bytes = 0;
    FileTime newest_file{};        // most recent mtime under the folder
    // Lua risk scan, per mod:
    int hooks = 0;                 // RegisterHook / NotifyOnNewObject / ... call sites
    int game_thread_calls = 0;     // ExecuteInGameThread / ExecuteWithDelay / LoopAsync sites
    int raw_memory_sites = 0;      // GetAddress / ReadMemory / WriteMemory style sites
    int native_loads = 0;          // package.loadlib / require of a .dll
    int shell_sites = 0;           // os.execute / io.popen
    std::vector<std::string> hooked_functions;   // "/Script/Engine.PlayerController:ClientRestart"
    std::vector<std::string> risk_notes;         // human readable, one per notable site
};

struct Ue4ssLogLine { std::string time; std::string text; };

struct Ue4ssInfo {
    bool installed = false;        // ue4ss/UE4SS.dll present
    fs::path dll;
    uint64_t dll_bytes = 0;
    std::string dll_sha256;
    std::string proxy_dll;         // "dwmapi.dll" (the loader in Win64), empty when none found
    std::vector<std::string> other_proxy_dlls;   // dxgi.dll, d3d11.dll, version.dll ... beside the exe
    std::string version_line;      // "UE4SS - v3.0.1 Beta #0 - Git SHA #97b7e501"
    std::string git_sha;           // "97b7e501"
    std::string build_config;      // "Game__Shipping__Win64 (MSVC)"
    std::map<std::string, std::string> settings;  // "General.UseCache" -> "1" (section.key)
    std::vector<std::string> mods_txt_order;      // names in mods.txt order
    std::vector<Ue4ssMod> mods;
    fs::path log;
    FileTime log_when{};
    bool log_from_this_boot = false;              // log newer than the newest crash report
    std::vector<Ue4ssLogLine> errors;             // lines with error/exception/crash/failed
    std::vector<Ue4ssLogLine> warnings;
    std::vector<Ue4ssLogLine> lua_errors;         // "[Lua]" lines that are errors or tracebacks
    std::vector<Ue4ssLogLine> tail;               // last 40 lines
    std::vector<std::string> started_mods;        // "Starting C++ mod 'X'" / "Mod 'X' has enabled.txt"
    bool log_reached_game_thread = false;         // saw the engine tick hook / first game-thread work
    std::vector<fs::path> crash_dumps;            // ue4ss/crash_*.dmp
    bool cxx_header_dump_present = false;
    bool usmap_present = false;
    bool gui_console_enabled = false;
    std::string graphics_api;                     // Debug.GraphicsAPI
};

// CSS, CSSX and CCS specifics: version files, logs, state leftovers.
struct OurModInfo {
    std::string name;              // "CustomShellSystem", "CSSX", "CCS"
    bool installed = false;
    fs::path folder;
    std::string release_version;   // from release.json
    std::string active_core;       // from core.json
    bool active_core_present = false;  // the core dll named in core.json exists
    std::vector<std::string> cores;     // dlls in cores/ or core/
    std::string loader_sha_recorded, loader_sha_actual; // loader-contract.json vs dlls/main.dll
    bool loader_mismatch = false;
    fs::path log;
    FileTime log_when{};
    std::vector<std::string> errors;    // lines with error/fail/exception/crash
    std::vector<std::string> warnings;  // "warning:" lines
    std::vector<std::string> tail;      // last 30 lines
    int corrupt_state_files = 0;        // state.json.corrupt-*
    bool dev_channel_enabled = false;   // CSSX dev/enabled.txt
    std::map<std::string, std::string> notes;  // anything else worth showing (catalog counts, extensions)
    std::vector<std::string> extensions;       // CSSX extensions installed
};

// One IoStore container or legacy pak in Paks/, ~mods/ or LogicMods/.
struct PakContainer {
    std::string stem;              // "CSS_EvePeony_eins0fx_P"
    fs::path folder;
    fs::path pak, utoc, ucas;      // empty when absent
    uint64_t pak_bytes = 0, utoc_bytes = 0, ucas_bytes = 0;
    bool is_patch = false;         // *_P
    bool is_game = false;          // pakchunkN-Windows / global
    bool is_mod = false;           // under ~mods or LogicMods, or a _P outside the game set
    bool complete = true;          // utoc+ucas pair present when either is present; pak present
    std::string problem;           // "utoc without ucas", "zero bytes", "unreadable table", ...
    bool css_package = false;      // carries CSS metadata (a CSS outfit)
    bool cssx_package = false;     // /Game/CSSX/ assets
    uint32_t chunks = 0;
    std::vector<uint64_t> package_ids;   // export bundle package ids, sorted, unique
    std::vector<std::string> names;      // from the directory index, when present
    uint32_t overrides_game = 0;         // package ids also present in the game's own containers
    std::vector<std::string> override_names;  // names of overridden game packages (when the index names them)
    std::vector<std::string> override_hints;  // folder hints "Characters/Shells/KnightLady" when names are known
    FileTime when{};
};

struct PakCollision {
    uint64_t package_id = 0;
    std::string name;              // when known
    std::vector<std::string> containers;   // stems that all carry it (2+)
};

struct PakInfo {
    std::vector<PakContainer> containers;
    std::vector<PakCollision> mod_collisions;      // same package in 2+ mod containers
    size_t game_package_ids = 0;
    std::vector<std::string> stray_files;          // non-pak files under Paks/~mods
    std::vector<std::string> mods_outside_mods_folder;  // _P containers directly in Paks/
    std::string error;
};

struct ManifestInfo {
    bool present = false;
    size_t manifest_entries = 0;
    std::vector<std::string> missing;      // in manifest, not on disk (capped)
    std::vector<std::string> foreign;      // on disk, not in manifest, outside known mod locations (capped)
    std::vector<std::string> replaced_binaries;  // Engine/ or Binaries/ files differing in size from the manifest's recorded size when known
    size_t missing_total = 0, foreign_total = 0;
    std::string steam_build_id;            // appmanifest buildid
    std::string steam_last_updated;        // unix seconds as text
    std::string exe_sha256;                // MortalShell2-Win64-Shipping.exe
    uint64_t exe_bytes = 0;
    FileTime exe_when{};
};

struct SystemInfo {
    std::string os;                        // "Windows 10 (21H1) [10.0.19045.5796]" or "Linux + Wine"
    bool game_ran_under_wine = false;      // PlatformIsRunningWine from the newest crash
    std::string cpu, gpu, gpu_driver, rhi; // from the newest crash context, or live Win32 when none
    uint64_t total_ram = 0;
    std::map<std::string, std::string> engine_ini;        // Saved/Config/Windows/Engine.ini keys
    std::map<std::string, std::string> game_user_settings;// Saved/Config/Windows/GameUserSettings.ini keys
    bool dlss_frame_gen_forced = false;    // r.Streamline.DLSSG.Enable=1 in Engine.ini
    bool fsr_frame_interp = false;
    std::vector<std::string> overlays_seen;  // GameOverlayRenderer64, RTSSHooks64, DiscordHook64, ReShade, SpecialK ... from minidump module lists
    std::vector<std::string> notes;
};

// Author-maintained knowledge shipped with the exe (known.json, embedded, and an
// optional known.json beside the exe that overrides it). Lets the verdict name a
// game build or a mod version that players report problems with, without any
// network access.
struct KnownBuild { std::string build_id; std::string date; std::string note; };
struct KnownUe4ss { std::string sha256; std::string version; std::string note; bool supported = true; };
struct KnownMod {
    std::string match;           // substring matched against mod folder / container stem, case-insensitive
    std::string display;         // "UUEPBS (Nexus 1234)"
    std::string note;            // known issue text
    std::string affected_versions; // free text
    Severity severity = Severity::Info;
};
struct Knowledge {
    std::string version;         // of the knowledge file
    std::vector<KnownBuild> builds;
    std::vector<KnownUe4ss> ue4ss;
    std::vector<KnownMod> mods;
    std::vector<std::string> overlay_modules;   // dll names of overlays/injectors worth flagging
    std::vector<std::string> builtin_mods;      // UE4SS's own mod folders
};
Knowledge load_knowledge(const std::optional<fs::path>& override_file); // embedded defaults + override

struct Evidence {
    Paths paths;
    Knowledge knowledge;
    std::vector<CrashReport> crashes;      // newest first
    Ue4ssInfo ue4ss;
    std::vector<OurModInfo> ours;
    PakInfo paks;
    ManifestInfo manifest;
    SystemInfo system;
    std::vector<std::string> scan_errors;  // scanner exceptions, shown but never fatal
    std::chrono::system_clock::time_point scanned_at{};
};

struct Finding {
    std::string id;            // stable rule id, "crash.mod-frame", "pak.incomplete"
    Severity severity = Severity::Info;
    int confidence = 50;       // 0..100, how sure the rule is that this is THE cause
    std::string title;         // one line
    std::string blame;         // "CSS_EvePeony_eins0fx_P", "UE4SS", "GPU driver", "game", "unknown"
    std::string summary;       // two or three sentences for the list pane
    std::string details;       // the long explanation, multi paragraph
    std::vector<std::string> evidence;  // short quoted facts
    std::string advice;        // what to do next
};

struct Report {
    Evidence evidence;
    std::vector<Finding> findings;   // sorted by severity then confidence
    std::string verdict;             // one paragraph: the most probable cause
    std::string generated_by;        // "WCMG 1.0.0"
};

using Progress = std::function<void(const std::string& step)>;

// Helpers shared by scanners (implemented in util.cpp).
std::string sha256_file(const fs::path&);          // lowercase hex, "" on failure
std::string read_text(const fs::path&, size_t max_bytes = 64 * 1024 * 1024);
std::vector<std::string> tail_lines(const fs::path&, size_t count, size_t max_bytes = 8 * 1024 * 1024);
std::string path_utf8(const fs::path&);
std::string lower(std::string);
std::string trim(std::string);
std::string format_time(FileTime);
std::string format_bytes(uint64_t);
bool icontains(const std::string& hay, const std::string& needle);
}

// scan_ue4ss, scan_lua_mod, scan_our_mods and scan_system against a synthetic
// install (always) and the real one (when WCMG_GAME_DIR / WCMG_SAVED_DIR are set).
#include "check.hpp"
#include "scanners.hpp"
#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;
using namespace wcmg;

namespace {

fs::path fixture_root() {
    // Scratch output lives in the project's work/ folder when the source path is known.
    fs::path src(__FILE__);
    if (src.is_absolute() && fs::exists(src.parent_path().parent_path() / "CMakeLists.txt"))
        return src.parent_path().parent_path() / "work" / "ue4ss" / "fixture";
    return wcmg_test::scratch_dir() / "ue4ss-fixture";
}

void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

const Ue4ssMod* find_mod(const Ue4ssInfo& u, const std::string& name) {
    for (const auto& m : u.mods) if (m.name == name) return &m;
    return nullptr;
}
const OurModInfo* find_ours(const std::vector<OurModInfo>& v, const std::string& name) {
    for (const auto& m : v) if (m.name == name) return &m;
    return nullptr;
}
bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }
bool any_contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) if (x.find(s) != std::string::npos) return true;
    return false;
}
bool any_line_contains(const std::vector<Ue4ssLogLine>& v, const std::string& s) {
    for (const auto& x : v) if (x.text.find(s) != std::string::npos) return true;
    return false;
}

Knowledge test_knowledge() {
    Knowledge k;
    k.builtin_mods = {"Keybinds", "shared"};
    k.overlay_modules = {"GameOverlayRenderer64.dll", "RTSSHooks64.dll", "dxgi.dll", "d3d12.dll", "version.dll"};
    return k;
}

Paths make_paths(const fs::path& root, const fs::path& saved) {
    Paths p;
    p.game_root = root / "Sparta";
    p.project = p.game_root / "MortalShell2";
    p.win64 = p.project / "Binaries" / "Win64";
    p.ue4ss = p.win64 / "ue4ss";
    p.mods = p.ue4ss / "Mods";
    p.paks = p.project / "Content" / "Paks";
    p.saved = saved;
    return p;
}

const char* kLuaMain = R"lua(
-- RegisterHook("/Script/Fake.InComment:Nope", f) must not count
--[[ block comment
RegisterHook("/Script/Fake.InBlock:Nope", f)
]]
local nativeDir = "native"
local dllPath = nativeDir .. "\\Foo.dll"
if type(package.loadlib) ~= "function" then print("RegisterHook is only a string here") end
local ok = package.loadlib(dllPath, "*")
RegisterHook("/Script/Engine.Actor:ReceiveTick", function(self) end)
RegisterHook("/Script/Engine.PlayerController:ClientRestart", function(self) end)
RegisterHook("/Script/Engine.PlayerController:ClientRestart", function(self) end)
NotifyOnNewObject("/Script/Engine.Actor", function(o) end)
RegisterBeginPlayPostHook(function(a) end)
LoopAsync(0, function() return false end)
ExecuteWithDelay(500, function() end)
ExecuteInGameThread(function() end)
local addr = obj:GetAddress()
local function RegisterHook(x) end
os.execute("echo hi")
local s = [[ long string with RegisterHook("/Script/X.Y:Z") inside ]]
)lua";

const char* kUe4ssLog =
    "[2026-10-27 06:09:57.8964813] Console created\n"
    "[2026-10-27 06:09:57.9732516] UE4SS - v3.0.1 Beta #0 - Git SHA #97b7e501\n"
    "[2026-10-27 06:09:57.9733067] UE4SS Build Configuration: Game__Shipping__Win64 (MSVC)\n"
    "[2026-10-27 06:09:57.9830200] Starting C++ mod 'CppMod'\n"
    "[2026-10-27 06:09:57.9832596] Mod 'CustomShellSystem' has enabled.txt, starting mod.\n"
    "[2026-10-27 06:10:01.5519099] FArchiveState::ArIsError = 0x29\n"
    "[2026-10-27 06:10:01.5512075] FWorldContext::PendingMapChangeFailureDescription = 0x1C8\n"
    "[2026-10-27 06:10:01.4092704] WARNING: VTable and scan addresses differ for UGameEngine::Tick.\n"
    "[2026-10-27 06:10:01.6089433] Starting Lua mod 'LuaMod'\n"
    "[2026-10-27 06:10:01.7123534] [Lua] [LuaMod] hello[2026-10-27 06:10:01.7124996] Mod 'Glued' has enabled.txt, starting mod.\n"
    "[2026-10-27 06:10:02.0000000] Error executing script: Mods/LuaMod/Scripts/main.lua:12: attempt to index a nil value\r\n"
    "stack traceback:\n"
    "\t[C]: in function 'error'\n"
    "\tmain.lua:12: in main chunk\n"
    "[2026-10-27 06:10:03.0000000] Failed to load something important\n"
    "[2026-10-27 06:10:04.9345344] [HashTables] Self test passed\n";

void synthetic_ue4ss_and_lua() {
    auto root = fixture_root() / "basic";
    std::error_code ec;
    fs::remove_all(root, ec);
    auto saved = root / "Saved";
    auto p = make_paths(root, saved);
    write(p.win64 / "MortalShell2-Win64-Shipping.exe", "MZ");
    // The UE4SS proxy names UE4SS.dll in UTF-16; a ReShade style dxgi.dll does not.
    write(p.win64 / "dwmapi.dll", std::string("MZ\0\0junk", 8) + std::string("U\0E\0004\0S\0S\0.\0d\0l\0l\0", 18));
    write(p.win64 / "dxgi.dll", "MZ ReShade overlay");
    write(p.ue4ss / "UE4SS.dll", "MZ ue4ss core");
    write(p.ue4ss / "UE4SS-settings.ini",
          "[Overrides]\nModsFolderPath =\n\n[General]\n; comment = 1\nUseCache = 1\n\n[Debug]\nGuiConsoleEnabled = 1\nGraphicsAPI = dx11 \n");
    write(p.ue4ss / "UE4SS.log", kUe4ssLog);
    write(p.ue4ss / "crash_2026_10_27.dmp", "MDMP");
    write(p.ue4ss / "Game-5.6.usmap", "x");
    fs::create_directories(p.ue4ss / "CXXHeaderDump");
    write(p.mods / "mods.txt", "LuaMod : 1\nOffMod : 0\n; Commented : 1\n\nKeybinds : 1\n");
    write(p.mods / "mods.json", R"([{"mod_name":"Keybinds","mod_enabled":true},{"mod_name":"OffMod","mod_enabled":true}])");
    write(p.mods / "LuaMod" / "Scripts" / "main.lua", kLuaMain);
    write(p.mods / "LuaMod" / "native" / "Foo.dll", "MZ foo");
    write(p.mods / "CppMod" / "dlls" / "main.dll", "MZ cpp main");
    write(p.mods / "CppMod" / "dlls" / "helper.dll", "MZ helper");
    write(p.mods / "CppMod" / "enabled.txt", "");
    write(p.mods / "OffMod" / "Scripts" / "main.lua", "print('off')\n");
    write(p.mods / "Unlisted" / "Scripts" / "main.lua", "print('nobody starts me')\n");
    write(p.mods / "Keybinds" / "Scripts" / "main.lua", "RegisterKeyBind(Key.F1, function() end)\n");
    write(p.mods / "shared" / "Types.lua", "-- shared lib\n");

    auto u = scan_ue4ss(p, test_knowledge());
    CHECK(u.installed);
    CHECK_EQ(u.dll_bytes, (uint64_t)13);
    CHECK_EQ(u.dll_sha256.size(), (size_t)64);
    CHECK_EQ(u.proxy_dll, std::string("dwmapi.dll"));
    CHECK(has(u.other_proxy_dlls, "dxgi.dll"));
    CHECK_EQ(u.settings["General.UseCache"], std::string("1"));
    CHECK(u.settings.find("General.; comment") == u.settings.end());
    CHECK(u.gui_console_enabled);
    CHECK_EQ(u.graphics_api, std::string("dx11"));
    CHECK_EQ(u.mods_txt_order.size(), (size_t)3);
    CHECK(!has(u.mods_txt_order, "Commented"));
    CHECK(find_mod(u, "shared") == nullptr);
    CHECK_EQ(u.mods.size(), (size_t)5);

    auto lua = find_mod(u, "LuaMod");
    CHECK(lua != nullptr);
    if (lua) {
        CHECK(lua->has_scripts && !lua->has_dll && !lua->enabled_txt);
        CHECK(lua->mods_txt && *lua->mods_txt);
        CHECK(lua->effectively_enabled);
        CHECK(!lua->builtin && !lua->ours);
        CHECK_EQ(lua->lua_files.size(), (size_t)1);
        CHECK(lua->lua_bytes > 0);
        CHECK_EQ(lua->dlls.size(), (size_t)1);
        // 3 RegisterHook + NotifyOnNewObject + RegisterBeginPlayPostHook; comments, strings and the local definition do not count.
        CHECK_EQ(lua->hooks, 5);
        CHECK_EQ(lua->game_thread_calls, 3);
        CHECK_EQ(lua->raw_memory_sites, 1);
        CHECK_EQ(lua->native_loads, 1);
        CHECK_EQ(lua->shell_sites, 1);
        CHECK_EQ(lua->hooked_functions.size(), (size_t)2);
        CHECK(has(lua->hooked_functions, "/Script/Engine.Actor:ReceiveTick"));
        CHECK(has(lua->risk_notes, "RegisterHook on /Script/Engine.Actor:ReceiveTick runs every frame for every actor"));
        CHECK(has(lua->risk_notes, "loads a native DLL from Lua: native/Foo.dll"));
        CHECK(has(lua->risk_notes, "LoopAsync with 0 ms interval"));
        CHECK(any_contains(lua->risk_notes, "NotifyOnNewObject on /Script/Engine.Actor"));
        CHECK(any_contains(lua->risk_notes, "shell"));
    }
    auto cpp = find_mod(u, "CppMod");
    CHECK(cpp != nullptr);
    if (cpp) {
        CHECK(cpp->has_dll && cpp->enabled_txt && !cpp->mods_txt);
        CHECK(cpp->effectively_enabled);
        CHECK_EQ(cpp->dlls.size(), (size_t)2);
        if (!cpp->dlls.empty()) CHECK_EQ(cpp->dlls.front().filename().string(), std::string("main.dll"));
        CHECK_EQ(cpp->dll_sha256.size(), cpp->dlls.size());
        CHECK_EQ(cpp->dll_bytes.size(), cpp->dlls.size());
        CHECK(cpp->newest_file != FileTime{});
    }
    auto off = find_mod(u, "OffMod");
    CHECK(off && off->mods_txt && !*off->mods_txt && off->mods_json && *off->mods_json && !off->effectively_enabled);
    auto unlisted = find_mod(u, "Unlisted");
    CHECK(unlisted && !unlisted->mods_txt && !unlisted->effectively_enabled);
    auto kb = find_mod(u, "Keybinds");
    CHECK(kb && kb->builtin && kb->effectively_enabled && kb->hooks == 0);

    // Log.
    CHECK_EQ(u.version_line, std::string("UE4SS - v3.0.1 Beta #0 - Git SHA #97b7e501"));
    CHECK_EQ(u.git_sha, std::string("97b7e501"));
    CHECK_EQ(u.build_config, std::string("Game__Shipping__Win64 (MSVC)"));
    CHECK(has(u.started_mods, "CppMod"));
    CHECK(has(u.started_mods, "LuaMod"));
    CHECK(has(u.started_mods, "CustomShellSystem"));
    CHECK(has(u.started_mods, "Glued"));
    CHECK(any_line_contains(u.errors, "Error executing script"));
    CHECK(any_line_contains(u.errors, "Failed to load something"));
    CHECK(!any_line_contains(u.errors, "ArIsError"));
    CHECK(!any_line_contains(u.errors, "PendingMapChangeFailureDescription"));
    CHECK_EQ(u.warnings.size(), (size_t)1);
    CHECK_EQ(u.lua_errors.size(), (size_t)4);
    if (u.lua_errors.size() == 4) {
        CHECK_EQ(u.lua_errors[0].time, std::string("2026-10-27 06:10:02.0000000"));
        CHECK(u.lua_errors[0].text.rfind("Error executing script", 0) == 0);
        CHECK(u.lua_errors[0].text.back() != '\r');
        CHECK(u.lua_errors[1].time.empty() && u.lua_errors[1].text == "stack traceback:");
        CHECK(u.lua_errors[3].text.find("main chunk") != std::string::npos);
    }
    CHECK(!u.errors.empty() && !u.errors.front().time.empty());
    CHECK_EQ(u.tail.size(), (size_t)17);  // 16 physical lines, one holds two entries
    CHECK(u.log_reached_game_thread);
    CHECK(!u.log_from_this_boot);
    CHECK(u.log_when != FileTime{});
    CHECK_EQ(u.crash_dumps.size(), (size_t)1);
    CHECK(u.cxx_header_dump_present);
    CHECK(u.usmap_present);

    // scan_lua_mod on its own finds the files when the list is empty.
    Ue4ssMod solo;
    solo.folder = p.mods / "LuaMod";
    scan_lua_mod(solo);
    CHECK_EQ(solo.lua_files.size(), (size_t)1);
    CHECK_EQ(solo.hooks, 5);
    // A missing folder neither throws nor finds anything.
    Ue4ssMod ghost;
    ghost.folder = root / "does-not-exist";
    scan_lua_mod(ghost);
    CHECK_EQ(ghost.hooks, 0);
}

void synthetic_overrides() {
    auto root = fixture_root() / "override";
    std::error_code ec;
    fs::remove_all(root, ec);
    auto p = make_paths(root, root / "Saved");
    write(p.ue4ss / "UE4SS.dll", "MZ");
    write(p.ue4ss / "UE4SS-settings.ini", "[Overrides]\nModsFolderPath = ../AltMods\n");
    write(p.mods / "Ignored" / "dlls" / "main.dll", "MZ");
    write(p.win64 / "AltMods" / "Elsewhere" / "dlls" / "main.dll", "MZ");
    write(p.win64 / "AltMods" / "Elsewhere" / "enabled.txt", "");
    // No mods.txt here, so the legacy mods.json decides.
    write(p.win64 / "AltMods" / "JsonOnly" / "Scripts" / "main.lua", "print(1)\n");
    write(p.win64 / "AltMods" / "mods.json", R"([{"mod_name":"JsonOnly","mod_enabled":true}])");
    write(p.win64 / "AltMods" / "Broken" / "Scripts" / "main.lua", "print(2)\n");
    auto u = scan_ue4ss(p, Knowledge{});
    CHECK(find_mod(u, "Ignored") == nullptr);
    auto e = find_mod(u, "Elsewhere");
    CHECK(e && e->effectively_enabled);
    auto j = find_mod(u, "JsonOnly");
    CHECK(j && j->mods_json && *j->mods_json && j->effectively_enabled);
    auto b = find_mod(u, "Broken");
    CHECK(b && !b->effectively_enabled);
    CHECK(u.proxy_dll.empty());
    CHECK(u.version_line.empty());

    // Nothing at all: no throw, nothing installed.
    auto none = scan_ue4ss(make_paths(root / "nothing", root / "nothing"), Knowledge{});
    CHECK(!none.installed && none.mods.empty());
}

void synthetic_our_mods() {
    auto root = fixture_root() / "ours";
    std::error_code ec;
    fs::remove_all(root, ec);
    auto p = make_paths(root, root / "Saved");
    auto css = p.mods / "CustomShellSystem";
    write(css / "release.json", R"({"version": "9.9.9", "ue4ss_revision": "97b7e501", "files": {"dlls/main.dll": "00"}})");
    write(css / "core.json", R"({"abi": 1, "file": "css_core-9.9.9.dll"})");
    write(css / "cores" / "css_core-9.9.9.dll", "MZ core");
    write(css / "cores" / "css_core-old.dll", "MZ old");
    write(css / "dlls" / "main.dll", "MZ loader");
    write(css / "loader-contract.json", R"({"abi": 2, "dll_sha256": "DEADBEEF"})");
    write(css / "state" / "state.json", "{}");
    write(css / "state" / "state.json.corrupt-1", "x");
    write(css / "state" / "state.json.corrupt-2", "x");
    write(css / "CSS.log",
          "1000001 CSS loader created; engine untouched until Unreal initialization\n"
          "1000002 Core load failed: old session\n"
          "1000003 CSS loader created; engine untouched until Unreal initialization\n"
          "1000004 CSS catalog: 27 outfits; 34 pak files scanned; 0 rejected\n"
          "1000005 CSS warning: package ignored\n"
          "1000006 Exception while applying outfit\n");
    auto cssx = p.mods / "CSSX";
    write(cssx / "release.json", "{ this is not json");
    write(cssx / "core.json", R"({"file": "cssx_core-1.3.1-dev-3b5ad8e32e03-r95851.dll"})");
    write(cssx / "dlls" / "main.dll", "MZ cssx");
    write(cssx / "dev" / "enabled.txt", "");
    write(cssx / "settings.json", R"({"a": 1, "b": 2})");
    fs::create_directories(cssx / "extensions" / "eins0fx.traverse");
    write(cssx / "CSSX.log",
          "2026-09-21T15:35:19.734Z CSSX loader 1.0.0 created at X\n"
          "2026-09-21T15:50:43.735Z warning: CSSX tab attached to the Player Menu at index 2\n"
          "2026-09-21T15:51:00.000Z Core load failed (126): cssx_core-1.1.2.dll\n");

    auto ours = scan_our_mods(p);
    CHECK_EQ(ours.size(), (size_t)3);
    auto c = find_ours(ours, "CustomShellSystem");
    CHECK(c && c->installed);
    if (c) {
        CHECK_EQ(c->release_version, std::string("9.9.9"));
        CHECK_EQ(c->active_core, std::string("css_core-9.9.9.dll"));
        CHECK(c->active_core_present);
        CHECK_EQ(c->cores.size(), (size_t)2);
        // release.json's file hash outranks the stale loader-contract.json hash.
        CHECK(c->loader_sha_recorded != std::string("deadbeef"));
        CHECK_EQ(c->loader_sha_actual.size(), (size_t)64);
        CHECK(c->loader_mismatch);
        CHECK(c->notes.count("loader_contract") == 1);
        CHECK_EQ(c->corrupt_state_files, 2);
        CHECK_EQ(c->errors.size(), (size_t)1);  // the old session's failure is out of scope
        CHECK_EQ(c->warnings.size(), (size_t)1);
        CHECK_EQ(c->tail.size(), (size_t)6);
        CHECK_EQ(c->notes.at("catalog"), std::string("27 outfits; 34 pak files scanned; 0 rejected"));
        CHECK_EQ(c->notes.at("log_sessions"), std::string("2"));
        CHECK_EQ(c->notes.at("loader_sha_source"), std::string("release.json files[dlls/main.dll]"));
    }
    auto x = find_ours(ours, "CSSX");
    CHECK(x && x->installed);
    if (x) {
        CHECK(x->release_version == "1.3.1-dev");  // broken release.json, version from the core name
        CHECK(!x->active_core_present);
        CHECK(x->dev_channel_enabled);
        CHECK(has(x->extensions, "eins0fx.traverse"));
        CHECK_EQ(x->notes.at("settings_keys"), std::string("2"));
        CHECK(x->notes.count("release.json"));
        CHECK_EQ(x->warnings.size(), (size_t)1);
        CHECK_EQ(x->errors.size(), (size_t)1);
        CHECK(!x->loader_mismatch);
    }
    auto ccs = find_ours(ours, "CCS");
    CHECK(ccs && !ccs->installed && ccs->folder.filename() == "CCS");
}

void synthetic_system() {
    auto root = fixture_root() / "system";
    std::error_code ec;
    fs::remove_all(root, ec);
    auto saved = root / "Saved";
    auto p = make_paths(root, saved);
    write(saved / "Config" / "Windows" / "Engine.ini",
          ";METADATA=(Diff=true, UseCommands=true)\n[ConsoleVariables]\nr.Streamline.Load.DLSSG=0\nr.Streamline.DLSSG.Enable=1\n"
          "r.FidelityFX.FI.Enabled=0\nr.FidelityFX.FI.Enabled=1\n");
    write(saved / "Config" / "Windows" / "GameUserSettings.ini",
          ";METADATA=(Diff=true, UseCommands=true)\n[ScalabilityGroups]\nsg.ResolutionQuality=87\n"
          "[/Script/Sparta.SpartaGameUserSettings]\nResolutionSizeX=2560\nResolutionSizeY=1440\nFullscreenMode=1\n");
    write(p.win64 / "dwmapi.dll", "MZ loads UE4SS.dll");
    write(p.win64 / "dxgi.dll", "MZ ReShade");

    CrashReport c;
    c.props["Misc.OSVersionMajor"] = "Windows 10 (21H1) [10.0.19045.5796]";
    c.props["PlatformFullName"] = "Win64 [Windows 10 (21H1) [10.0.19045.5796]  64b]";
    c.props["PlatformIsRunningWine"] = "true";
    c.props["Misc.CPUBrand"] = "12th Gen Intel(R) Core(TM) i5-12400F";
    c.props["Misc.PrimaryGPUBrand"] = "NVIDIA GeForce RTX 3060";
    c.props["RHI.UserDriverVersion"] = "999.99";
    c.props["RHI.DriverDate"] = "10-27-2026";
    c.props["RHI.RHIName"] = "D3D12";
    c.props["RHI.FeatureLevel"] = "SM6";
    c.props["MemoryStats.TotalPhysical"] = "67185004544";
    c.loaded_modules = {"MortalShell2-Win64-Shipping.exe", "d3d12.dll", "dxgi.dll", "dwmapi.dll", "GameOverlayRenderer64.dll", "kernel32.dll"};
    CrashReport older;
    older.loaded_modules = {"RTSSHooks64.dll", "GameOverlayRenderer64.dll"};

    auto s = scan_system(p, {c, older}, test_knowledge());
    CHECK_EQ(s.engine_ini["ConsoleVariables.r.Streamline.DLSSG.Enable"], std::string("1"));
    CHECK(s.engine_ini.count("ConsoleVariables.r.FidelityFX.FI.Enabled"));
    CHECK_EQ(s.engine_ini["ConsoleVariables.r.FidelityFX.FI.Enabled"], std::string("1"));  // last duplicate wins
    for (const auto& [k, v] : s.engine_ini) CHECK(k.find("METADATA") == std::string::npos);
    CHECK(s.dlss_frame_gen_forced);
    CHECK(s.fsr_frame_interp);
    CHECK_EQ(s.game_user_settings["ScalabilityGroups.sg.ResolutionQuality"], std::string("87"));
    CHECK_EQ(s.os, std::string("Windows 10 (21H1) [10.0.19045.5796]"));
    CHECK(s.game_ran_under_wine);
    CHECK_EQ(s.cpu, std::string("12th Gen Intel(R) Core(TM) i5-12400F"));
    CHECK_EQ(s.gpu, std::string("NVIDIA GeForce RTX 3060"));
    CHECK_EQ(s.gpu_driver, std::string("999.99 (10-27-2026)"));
    CHECK_EQ(s.rhi, std::string("D3D12 SM6"));
    CHECK_EQ(s.total_ram, (uint64_t)67185004544ull);
    // dxgi.dll counts because a non-UE4SS copy sits beside the exe; d3d12.dll is the system one.
    CHECK(has(s.overlays_seen, "GameOverlayRenderer64.dll"));
    CHECK(has(s.overlays_seen, "RTSSHooks64.dll"));
    CHECK(has(s.overlays_seen, "dxgi.dll"));
    CHECK(!has(s.overlays_seen, "d3d12.dll"));
    CHECK(!has(s.overlays_seen, "dwmapi.dll"));
    CHECK_EQ(s.overlays_seen.size(), (size_t)3);
    CHECK(has(s.notes, "Engine.ini forces DLSS Frame Generation on"));
    CHECK(has(s.notes, "GameUserSettings resolution scale 87"));

    // No saved dir, no crashes: empty but no throw.
    auto empty = scan_system(make_paths(root / "none", root / "none"), {}, Knowledge{});
    CHECK(empty.engine_ini.empty() && !empty.dlss_frame_gen_forced);
}

std::string yn(bool b) { return b ? "yes" : "no"; }

void real_install() {
    auto game = wcmg_test::env_path("WCMG_GAME_DIR");
    if (!game) { std::printf("ue4ss: WCMG_GAME_DIR not set, real-install checks skipped\n"); return; }
    auto saved = wcmg_test::env_path("WCMG_SAVED_DIR");
    Paths p;
    p.game_root = *game;
    p.project = p.game_root / "MortalShell2";
    p.win64 = p.project / "Binaries" / "Win64";
    p.ue4ss = p.win64 / "ue4ss";
    p.mods = p.ue4ss / "Mods";
    p.paks = p.project / "Content" / "Paks";
    if (saved) p.saved = *saved;
    auto k = load_knowledge(std::nullopt);

    auto t0 = std::chrono::steady_clock::now();
    auto u = scan_ue4ss(p, k);
    auto t1 = std::chrono::steady_clock::now();
    auto ours = scan_our_mods(p);
    auto t2 = std::chrono::steady_clock::now();
    std::vector<CrashReport> crashes;
    if (saved) crashes = scan_crash_reports(p, 5);
    auto s = scan_system(p, crashes, k);
    auto ms = [](auto a, auto b) { return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };

    CHECK(u.installed);
    CHECK_EQ(u.git_sha, std::string("97b7e501"));
    CHECK_EQ(u.proxy_dll, std::string("dwmapi.dll"));
    CHECK(u.mods.size() >= 15);
    CHECK(u.cxx_header_dump_present);
    CHECK(u.usmap_present);
    CHECK(!u.started_mods.empty());
    for (const char* name : {"CustomShellSystem", "CSSX", "CCS"}) {
        auto m = find_mod(u, name);
        CHECK(m && m->ours && m->effectively_enabled);
        auto o = find_ours(ours, name);
        CHECK(o && o->installed && !o->release_version.empty());
        if (o) CHECK(!o->active_core.empty() && o->active_core_present);
    }
    auto uu = find_mod(u, "UUEPBS");
    CHECK(uu != nullptr);
    if (uu) {
        CHECK(uu->native_loads > 0 || uu->dlls.size() > 0);
        CHECK(uu->hooks > 0);
    }
    if (saved) CHECK(s.dlss_frame_gen_forced);

    std::printf("\nUE4SS %s | %s | proxy %s | others [", u.version_line.c_str(), u.build_config.c_str(), u.proxy_dll.c_str());
    for (const auto& o : u.other_proxy_dlls) std::printf(" %s", o.c_str());
    std::printf(" ] | dll %s sha %.16s\n", format_bytes(u.dll_bytes).c_str(), u.dll_sha256.c_str());
    std::printf("log: %zu errors, %zu warnings, %zu lua errors, reached game thread %s, started %zu mods, %zu crash dumps\n",
                u.errors.size(), u.warnings.size(), u.lua_errors.size(), yn(u.log_reached_game_thread).c_str(), u.started_mods.size(),
                u.crash_dumps.size());
    std::printf("scan times: ue4ss %lld ms, our mods %lld ms\n\n", ms(t0, t1), ms(t1, t2));
    std::printf("%-24s %-8s %-8s %5s %5s %6s %4s %4s %4s  %s\n", "mod", "kind", "enabled", "dlls", "lua", "hooks", "gt", "raw", "nat", "notes");
    for (const auto& m : u.mods) {
        std::string kind = m.has_dll && m.has_scripts ? "C++/Lua" : m.has_dll ? "C++" : m.has_scripts ? "Lua" : "other";
        std::string tag = m.ours ? " ours" : m.builtin ? " builtin" : "";
        std::string en = m.effectively_enabled ? (m.enabled_txt ? "txt" : "mods.txt") : "off";
        std::printf("%-24s %-8s %-8s %5zu %5zu %6d %4d %4d %4d %s%s\n", m.name.c_str(), kind.c_str(), en.c_str(), m.dlls.size(),
                    m.lua_files.size(), m.hooks, m.game_thread_calls, m.raw_memory_sites, m.native_loads, tag.c_str(),
                    m.risk_notes.empty() ? "" : (" | " + m.risk_notes.front()).c_str());
    }
    std::printf("\n%-18s %-14s %-48s %-7s %-6s %5s %5s %s\n", "our mod", "version", "core", "present", "loader", "errs", "warns", "corrupt");
    for (const auto& o : ours) {
        std::printf("%-18s %-14s %-48s %-7s %-6s %5zu %5zu %d\n", o.name.c_str(), o.release_version.c_str(), o.active_core.c_str(),
                    yn(o.active_core_present).c_str(), o.loader_sha_recorded.empty() ? "n/a" : (o.loader_mismatch ? "DIFF" : "match"),
                    o.errors.size(), o.warnings.size(), o.corrupt_state_files);
        for (const auto& [key, v] : o.notes) std::printf("    %s = %s\n", key.c_str(), v.c_str());
    }
    std::printf("\nsystem: os '%s' wine %s | cpu '%s' | gpu '%s' | driver '%s' | rhi '%s' | ram %s\n", s.os.c_str(),
                yn(s.game_ran_under_wine).c_str(), s.cpu.c_str(), s.gpu.c_str(), s.gpu_driver.c_str(), s.rhi.c_str(),
                format_bytes(s.total_ram).c_str());
    std::printf("dlss fg forced %s, fsr fi %s, %zu engine.ini keys, %zu gus keys, overlays [", yn(s.dlss_frame_gen_forced).c_str(),
                yn(s.fsr_frame_interp).c_str(), s.engine_ini.size(), s.game_user_settings.size());
    for (const auto& o : s.overlays_seen) std::printf(" %s", o.c_str());
    std::printf(" ]\n");
    for (const auto& n : s.notes) std::printf("  note: %s\n", n.c_str());
    std::printf("\n");
}

} // namespace

int main() {
    synthetic_ue4ss_and_lua();
    synthetic_overrides();
    synthetic_our_mods();
    synthetic_system();
    real_install();
    return wcmg_test::finish("ue4ss");
}

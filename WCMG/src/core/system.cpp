// Config inis, hardware facts from the newest crash context, overlays seen in
// the crash dumps' module lists, and live Win32 facts when there is no crash.
#include "scanners.hpp"
#include <algorithm>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wcmg {

namespace scan_detail {  // defined in ue4ss.cpp
std::map<std::string, std::string> parse_ini_file(const fs::path& p, bool ue_style);
bool is_ue4ss_loader(const fs::path& dll);
}

namespace {

// Crash context tags may be stored with or without their group prefix.
std::string prop(const CrashReport& c, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        auto it = c.props.find(k);
        if (it != c.props.end() && !trim(it->second).empty()) return trim(it->second);
    }
    for (const char* k : keys) {
        std::string want = lower(k);
        for (const auto& [key, value] : c.props) {
            auto lk = lower(key);
            if ((lk == want || (lk.size() > want.size() && lk.compare(lk.size() - want.size(), want.size(), want) == 0 &&
                                lk[lk.size() - want.size() - 1] == '.')) && !trim(value).empty())
                return trim(value);
        }
    }
    return "";
}

bool truthy(const std::string& v) {
    auto l = lower(trim(v));
    return l == "1" || l == "true" || l == "yes" || l == "on";
}

// A cvar may sit under [SystemSettings], [ConsoleVariables] or a device profile section.
std::string cvar(const std::map<std::string, std::string>& ini, const std::string& name) {
    std::string found;
    auto want = lower(name);
    for (const auto& [key, value] : ini) {
        auto lk = lower(key);
        if (lk == want || (lk.size() > want.size() && lk.compare(lk.size() - want.size(), want.size(), want) == 0 &&
                           lk[lk.size() - want.size() - 1] == '.'))
            found = value;
    }
    return found;
}

std::string ini_value(const std::map<std::string, std::string>& ini, const std::string& key) {
    auto it = ini.find(key);
    return it == ini.end() ? std::string() : it->second;
}

// Windows system DLL names that also appear in the overlay list because
// injectors drop proxies with these names. The system copy is in every game's
// module list, so only a copy beside the exe counts.
bool is_system_dll_name(const std::string& lname) {
    static const std::set<std::string> names = {
        "d3d11.dll", "dxgi.dll", "d3d12.dll", "opengl32.dll", "winmm.dll", "version.dll",
        "dinput8.dll", "xinput1_3.dll", "dwmapi.dll", "winhttp.dll"};
    return names.count(lname) != 0;
}

std::string file_name_lower(const std::string& module) {
    auto slash = module.find_last_of("\\/");
    return lower(slash == std::string::npos ? module : module.substr(slash + 1));
}

#ifdef _WIN32
void live_windows_facts(SystemInfo& s) {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        if (s.os.empty()) {
            if (auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))) {
                RTL_OSVERSIONINFOW v{};
                v.dwOSVersionInfoSize = sizeof v;
                if (fn(&v) == 0) {
                    const char* name = v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000 ? "Windows 11" :
                                       v.dwMajorVersion == 10 ? "Windows 10" : "Windows";
                    s.os = std::string(name) + " [" + std::to_string(v.dwMajorVersion) + "." + std::to_string(v.dwMinorVersion) + "." +
                           std::to_string(v.dwBuildNumber) + "]";
                }
            }
        }
        using WineVersionFn = const char*(__cdecl*)();
        if (auto wine = reinterpret_cast<WineVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "wine_get_version")))) {
            const char* ver = wine();
            s.notes.push_back(std::string("running under Wine ") + (ver ? ver : "(unknown version)"));
        }
    }
    if (s.total_ram == 0) {
        MEMORYSTATUSEX m{};
        m.dwLength = sizeof m;
        if (GlobalMemoryStatusEx(&m)) s.total_ram = m.ullTotalPhys;
    }
    if (s.cpu.empty()) {
        HKEY key;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) == ERROR_SUCCESS) {
            wchar_t buf[256];
            DWORD bytes = sizeof buf, type = 0;
            if (RegQueryValueExW(key, L"ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &bytes) == ERROR_SUCCESS && type == REG_SZ) {
                buf[255] = 0;
                int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
                if (n > 1) {
                    std::string out((size_t)n - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, buf, -1, out.data(), n, nullptr, nullptr);
                    s.cpu = trim(out);
                }
            }
            RegCloseKey(key);
        }
    }
}
#endif

} // namespace

SystemInfo scan_system(const Paths& paths, const std::vector<CrashReport>& crashes, const Knowledge& k) {
    SystemInfo s;
    try {
        auto cfg = paths.saved / "Config" / "Windows";
        s.engine_ini = scan_detail::parse_ini_file(cfg / "Engine.ini", true);
        s.game_user_settings = scan_detail::parse_ini_file(cfg / "GameUserSettings.ini", true);

        s.dlss_frame_gen_forced = truthy(cvar(s.engine_ini, "r.Streamline.DLSSG.Enable")) || truthy(cvar(s.engine_ini, "r.Streamline.Load.DLSSG"));
        s.fsr_frame_interp = truthy(cvar(s.engine_ini, "r.FidelityFX.FI.Enabled"));
        if (s.dlss_frame_gen_forced) s.notes.push_back("Engine.ini forces DLSS Frame Generation on");
        if (s.fsr_frame_interp) s.notes.push_back("Engine.ini turns FSR Frame Interpolation on");
        if (truthy(cvar(s.engine_ini, "t.Streamline.Reflex.Enable"))) s.notes.push_back("Engine.ini turns NVIDIA Reflex on");
        if (!s.engine_ini.empty()) s.notes.push_back("Engine.ini sets " + std::to_string(s.engine_ini.size()) + " value(s)");

        auto gus = [&](const std::string& key) {
            auto v = ini_value(s.game_user_settings, "/Script/Sparta.SpartaGameUserSettings." + key);
            if (v.empty()) v = ini_value(s.game_user_settings, "/Script/Engine.GameUserSettings." + key);
            return v;
        };
        if (auto rq = ini_value(s.game_user_settings, "ScalabilityGroups.sg.ResolutionQuality"); !rq.empty())
            s.notes.push_back("GameUserSettings resolution scale " + rq);
        if (auto x = gus("ResolutionSizeX"), y = gus("ResolutionSizeY"); !x.empty() && !y.empty()) {
            std::string mode = gus("FullscreenMode");
            const char* mode_name = mode == "0" ? "fullscreen" : mode == "1" ? "borderless" : mode == "2" ? "windowed" : "";
            s.notes.push_back("GameUserSettings resolution " + x + "x" + y + (*mode_name ? std::string(" ") + mode_name : std::string()));
        }
        if (truthy(gus("bUseDynamicResolution"))) s.notes.push_back("GameUserSettings dynamic resolution on");

        if (!crashes.empty()) {
            const auto& c = crashes.front();
            s.os = prop(c, {"Misc.OSVersionMajor", "PlatformFullName"});
            s.game_ran_under_wine = truthy(prop(c, {"PlatformIsRunningWine"}));
            s.cpu = prop(c, {"Misc.CPUBrand", "CPUBrand"});
            s.gpu = prop(c, {"Misc.PrimaryGPUBrand", "RHI.AdapterName"});
            auto drv = prop(c, {"RHI.UserDriverVersion"});
            auto date = prop(c, {"RHI.DriverDate"});
            s.gpu_driver = drv.empty() ? date : (date.empty() ? drv : drv + " (" + date + ")");
            auto rhi = prop(c, {"RHI.RHIName"});
            auto fl = prop(c, {"RHI.FeatureLevel"});
            s.rhi = rhi.empty() ? fl : (fl.empty() ? rhi : rhi + " " + fl);
            try {
                auto ram = prop(c, {"MemoryStats.TotalPhysical"});
                if (!ram.empty()) s.total_ram = std::stoull(ram);
            } catch (...) {}
            if (s.game_ran_under_wine) s.notes.push_back("the game ran under Wine or Proton (PlatformIsRunningWine)");
        }

        // Overlays and injectors in any crash's module list.
        std::vector<std::string> overlay_lc;
        for (const auto& o : k.overlay_modules) overlay_lc.push_back(lower(o));
        std::set<std::string> seen;
        auto beside_exe_non_loader = [&](const std::string& lname) {
            std::error_code ec;
            auto p = paths.win64 / lname;
            return !paths.win64.empty() && fs::is_regular_file(p, ec) && !scan_detail::is_ue4ss_loader(p);
        };
        std::map<std::string, bool> beside_cache;
        for (const auto& c : crashes) {
            for (const auto& mod : c.loaded_modules) {
                auto lname = file_name_lower(mod);
                bool hit = false;
                for (const auto& o : overlay_lc)
                    if (!o.empty() && lname.find(o) != std::string::npos) { hit = true; break; }
                if (!hit) continue;
                if (is_system_dll_name(lname)) {
                    auto it = beside_cache.find(lname);
                    if (it == beside_cache.end()) it = beside_cache.emplace(lname, beside_exe_non_loader(lname)).first;
                    if (!it->second) continue;
                }
                auto shown = mod.substr(mod.find_last_of("\\/") == std::string::npos ? 0 : mod.find_last_of("\\/") + 1);
                if (seen.insert(lname).second) s.overlays_seen.push_back(shown);
            }
        }

#ifdef _WIN32
        if (crashes.empty()) live_windows_facts(s);
        else {
            // Still worth knowing whether the diagnosis itself runs under Wine.
            if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"); ntdll && GetProcAddress(ntdll, "wine_get_version"))
                s.notes.push_back("this diagnosis is running under Wine");
        }
#endif
        if (crashes.empty() && s.os.empty() && paths.running_under_wine) s.os = "Linux + Wine";
    } catch (...) {
    }
    return s;
}

} // namespace wcmg

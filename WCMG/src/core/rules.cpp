// The rules: every Finding WCMG can produce, in one place, so the reasoning is
// reviewable. Each rule reads Evidence and appends zero or more Findings. Rules
// never touch the disk except to re-open a minidump for module paths, which the
// scanners already proved readable.
#include "scanners.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <cstdlib>
#include <sstream>

namespace wcmg {
namespace {

std::string join(const std::vector<std::string>& v, const std::string& sep, size_t cap = 0) {
    std::string out;
    size_t n = 0;
    for (const auto& s : v) {
        if (cap && n == cap) { out += sep + "..."; break; }
        if (!out.empty()) out += sep;
        out += s; ++n;
    }
    return out;
}

std::string hex(uint64_t v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "0x%llx", (unsigned long long)v);
    return buf;
}

// System modules a crash passes through on the way down; never the cause by themselves.
bool runtime_module(const std::string& m) {
    static const char* names[] = {"vcruntime140", "vcruntime140_1", "msvcp140", "ucrtbase", "kernel32", "kernelbase",
                                  "ntdll", "user32", "gdi32", "win32u", "sechost", "rpcrt4", "combase", "ole32", "normaliz", "advapi32", "shell32"};
    auto l = lower(m);
    for (auto n : names) if (l == n) return true;
    return false;
}

bool game_module(const std::string& m) { return lower(m) == "mortalshell2-win64-shipping"; }

// GPU and platform modules: a frame there means the driver or the graphics layer died.
bool gpu_module(const std::string& m) {
    auto l = lower(m);
    static const char* names[] = {"nvwgf2umx", "nvgpucomp64", "nvapi64", "nvcuda", "d3d12", "d3d12core", "d3d11", "dxgi", "dxcore",
                                  "amdxc64", "amdvlk64", "atidxx64", "aticfx64", "igd12umd64", "igd10iumd64", "vkd3d-proton", "dxvk", "winevulkan", "vulkan-1",
                                  "sl.interposer", "sl.dlss_g", "sl.common", "nvngx_dlssg", "nvngx_dlss", "sl.reflex", "amd_fidelityfx"};
    for (auto n : names) if (l == n || l.starts_with(std::string(n) + ".")) return true;
    return false;
}

struct ModuleOwner {
    std::string label;      // "CSS core", "UE4SS", "mod folder X"
    std::string blame;      // what the finding blames
    bool is_mod = false;
    bool ours = false;
};

// Decides who owns a stack-frame module. Needs the minidump's module paths to
// tell the three main.dll loaders apart, which the UE stack prints as "main".
ModuleOwner owner_of(const Frame& f, const std::map<uint64_t, std::string>& base_to_path, const Evidence& ev) {
    ModuleOwner o;
    auto l = lower(f.module);
    std::string path;
    if (auto it = base_to_path.find(f.base); it != base_to_path.end()) path = lower(it->second);
    auto under_mods = [&](const std::string& p) -> std::string {
        // ".../ue4ss/mods/<name>/..." -> name
        auto pos = p.find("/mods/");
        if (pos == std::string::npos) pos = p.find("\\mods\\");
        if (pos == std::string::npos) return "";
        auto start = pos + 6;
        auto end = p.find_first_of("/\\", start);
        return end == std::string::npos ? p.substr(start) : p.substr(start, end - start);
    };
    if (l == "ue4ss") { o.label = "UE4SS"; o.blame = "UE4SS"; o.is_mod = true; return o; }
    if (l.starts_with("css_core")) { o.label = "CSS core"; o.blame = "CustomShellSystem"; o.is_mod = true; o.ours = true; return o; }
    if (l.starts_with("cssx_core")) { o.label = "CSSX core"; o.blame = "CSSX"; o.is_mod = true; o.ours = true; return o; }
    if (l.starts_with("ccs_core")) { o.label = "CCS core"; o.blame = "CCS"; o.is_mod = true; o.ours = true; return o; }
    if (!path.empty()) {
        auto name = under_mods(path);
        if (!name.empty()) {
            // Restore the folder's real capitalisation from the scan.
            std::string shown = name;
            for (const auto& m : ev.ue4ss.mods) if (lower(m.name) == name) { shown = m.name; o.ours = m.ours; break; }
            o.label = "mod " + shown + " (" + f.module + ".dll)"; o.blame = shown; o.is_mod = true; return o;
        }
    }
    if (l == "main") { o.label = "a UE4SS C++ mod loader (main.dll)"; o.blame = "a C++ mod"; o.is_mod = true; return o; }
    for (const auto& m : ev.ue4ss.mods)
        for (const auto& dll : m.dlls)
            if (lower(path_utf8(dll.stem())) == l) { o.label = "mod " + m.name + " (" + f.module + ".dll)"; o.blame = m.name; o.is_mod = true; o.ours = m.ours; return o; }
    if (gpu_module(l)) { o.label = "graphics driver or DLSS/FSR layer (" + f.module + ")"; o.blame = "GPU driver"; return o; }
    if (game_module(l)) { o.label = "the game itself"; o.blame = "game"; return o; }
    if (runtime_module(l)) { o.label = "Windows runtime (" + f.module + ")"; o.blame = "runtime"; return o; }
    o.label = f.module; o.blame = f.module;
    return o;
}

std::string frame_text(const Frame& f) {
    return f.module + " + " + hex(f.offset) + (f.from_stack_scan ? " (stack scan)" : "");
}

const KnownMod* known_mod(const Knowledge& k, const std::string& name) {
    for (const auto& m : k.mods) if (!m.match.empty() && icontains(name, m.match)) return &m;
    return nullptr;
}

struct Rules {
    const Evidence& ev;
    std::vector<Finding> out;

    void add(Finding f) { out.push_back(std::move(f)); }

    // ---------- crash reports ----------
    void crashes() {
        if (ev.crashes.empty()) {
            Finding f;
            f.id = "crash.none"; f.severity = Severity::Info; f.confidence = 30;
            f.title = "No Unreal crash reports found";
            f.blame = "unknown";
            f.summary = "The game did not leave a crash report in Saved/Crashes. Either it has not crashed since the folder was last cleaned, it died before Unreal's crash handler could run, or it was killed from outside (driver reset, Windows, the user).";
            f.details = "Unreal writes a UECC-* folder with a crash context and a minidump whenever its own crash handler catches the exception. Three kinds of death skip that: a crash during UE4SS startup before the engine is up (look at UE4SS.log and ue4ss/crash_*.dmp), a GPU timeout that Windows resolves by killing the process, and a hard freeze the player ended with Task Manager. The other sections below still check mods, containers and files, so a cause can still show up.";
            if (!ev.ue4ss.crash_dumps.empty()) f.evidence.push_back("UE4SS wrote " + std::to_string(ev.ue4ss.crash_dumps.size()) + " crash_*.dmp file(s) in the ue4ss folder");
            f.advice = "Reproduce the crash once, then run WCMG again. If the game vanishes with no report at all, check UE4SS.log's last lines and the Windows Event Viewer (Application log, source Application Error).";
            add(f);
            return;
        }
        const auto& newest = ev.crashes.front();
        // How many of the recent crashes repeat the same stack.
        std::map<std::string, int> hashes;
        for (const auto& c : ev.crashes) if (!c.callstack_hash.empty()) ++hashes[c.callstack_hash];
        int repeats = newest.callstack_hash.empty() ? 1 : hashes[newest.callstack_hash];
        int gpu_count = 0;
        for (const auto& c : ev.crashes) if (c.crash_type == "GPUCrash") ++gpu_count;

        if (newest.crash_type == "GPUCrash") gpu_crash(newest, gpu_count);
        else if (newest.is_oom) oom(newest);
        else stack_crash(newest, repeats);

        if (newest.seconds_since_start >= 0 && newest.seconds_since_start < 90 && newest.crash_type != "GPUCrash") {
            Finding f;
            f.id = "crash.startup"; f.severity = Severity::Medium; f.confidence = 55;
            f.title = "The newest crash happened during startup";
            f.blame = "mods or containers";
            f.summary = "The game died " + std::to_string(newest.seconds_since_start) + " seconds after launch. Startup crashes almost always come from something that loads at boot: a pak container, a UE4SS mod initialising, or a changed game file.";
            f.details = "In the first minute the engine mounts every pak and IoStore container, UE4SS scans the executable and starts every enabled mod, and the main menu map loads. A crash in that window rarely depends on what the player was doing. Check the container and mod findings below first; a broken or incomplete container and a mod built for another UE4SS version are the two common causes.";
            f.evidence.push_back("SecondsSinceStart = " + std::to_string(newest.seconds_since_start));
            f.advice = "Move the newest mod out, launch, repeat until it starts. Containers first (Content/Paks/~mods), then UE4SS mods.";
            add(f);
        }

        if (auto it = newest.props.find("Session.SecondsInLevel"); it != newest.props.end()) {
            int in_level = std::atoi(it->second.c_str());
            if (in_level >= 0 && in_level < 20 && newest.seconds_since_start > 90) {
                Finding f;
                f.id = "crash.level-load"; f.severity = Severity::Medium; f.confidence = 50;
                f.title = "The crash came " + std::to_string(in_level) + " s after entering a level";
                f.blame = "assets loading for that level";
                f.summary = "Unreal's session counter says the player had just arrived in a level (" + (newest.props.count("Player.Level") ? newest.props.at("Player.Level") : std::string("unknown")) + "). Right after a level load the engine streams meshes, materials and shaders for everything in view, including every outfit and replacement mod it needs there. Content that fails in that burst is the usual cause.";
                f.details = "Replacement containers for enemies or NPCs that appear in this level, an outfit worn by the player with materials cooked for an older build, or shader compilation under a driver that rejects a permutation all show up as a crash in the first seconds of a level. Compare the levels of the recent crashes: the same level each time points at content placed there; any level points at the player's own outfit or a driver.";
                f.evidence.push_back("Session.SecondsInLevel=" + it->second + ", Player.Level=" + (newest.props.count("Player.Level") ? newest.props.at("Player.Level") : ""));
                f.advice = "Change the worn outfit to the game's own shell and enter the same level again; then test with replacement containers removed.";
                add(f);
            }
        }

        // Crashes started after a game update, with mod files older than the update.
        after_update();

        if (repeats >= 3) {
            Finding f;
            f.id = "crash.repeat"; f.severity = Severity::Info; f.confidence = 60;
            f.title = "The same crash repeated " + std::to_string(repeats) + " times";
            f.blame = "see the top finding";
            f.summary = "The newest crash shares its call-stack hash with " + std::to_string(repeats - 1) + " earlier report(s). That is one bug hitting again and again, not random instability, so the top finding deserves the attention.";
            f.evidence.push_back("PCallStackHash " + newest.callstack_hash);
            f.advice = "Fix the top finding first; the older reports with the same hash will stop with it.";
            add(f);
        }
        if (gpu_count >= 3 && newest.crash_type != "GPUCrash") {
            Finding f;
            f.id = "crash.gpu-history"; f.severity = Severity::Low; f.confidence = 50;
            f.title = std::to_string(gpu_count) + " of the recent crashes were GPU crashes";
            f.blame = "GPU driver";
            f.summary = "Even though the newest crash is a normal exception, most of the history is the GPU dying. Two different problems may be present; see the system findings for frame generation and overlays.";
            f.advice = "Treat the GPU crashes separately: driver, frame generation, overlays, overclock.";
            add(f);
        }
    }

    void gpu_crash(const CrashReport& c, int gpu_count) {
        Finding f;
        f.id = "crash.gpu"; f.severity = Severity::High; f.confidence = 70;
        f.title = "The GPU crashed (device removed), not the CPU side";
        f.blame = "GPU driver";
        f.summary = "The newest report is a GPU crash dump: the graphics driver reported the device lost while rendering. Mods that only run on the CPU (UE4SS Lua, wardrobe logic) cannot cause this directly; the usual causes are the driver, DLSS Frame Generation or FSR frame interpolation, an overclock, overlays that hook D3D12, or a shader from a mod's material that the driver rejects.";
        std::string pass = c.gpu_breadcrumb_tail.empty() ? "" : c.gpu_breadcrumb_tail.back();
        std::ostringstream d;
        d << "Unreal records GPU breadcrumbs, markers the render thread leaves on the command queue. The innermost marker still active when the device died tells which pass was running. ";
        if (!pass.empty()) d << "Here it was: " << join(c.gpu_breadcrumb_tail, " > ") << ".\n\n";
        else d << "No active marker survived in this report.\n\n";
        auto prop = [&](const char* k) { auto it = c.props.find(k); return it == c.props.end() ? std::string() : it->second; };
        auto dlssg_active = prop("DLSSG.Active"), dlssg_enable = prop("DLSSG.Enable"), fi = prop("Settings.FrameInterpolationEnabled"), upscaler = prop("Settings.Upscaler"), wine = prop("PlatformIsRunningWine");
        bool fg_active = dlssg_active == "1" || dlssg_active == "true" || fi == "1" || fi == "true";
        bool fg_enabled = dlssg_enable == "1" || dlssg_enable == "true";
        if (fg_active) {
            f.confidence = 82;
            d << "Frame generation was active at the time (DLSSG.Active=" << dlssg_active << ", FrameInterpolation=" << fi << "). Generated frames run on a separate driver path that is the most common source of device-removed crashes in UE 5.6 titles, especially when forced on through Engine.ini.\n\n";
            f.evidence.push_back("DLSSG.Active=" + dlssg_active + ", Settings.FrameInterpolationEnabled=" + fi);
        } else if (fg_enabled) {
            f.confidence = 74;
            d << "DLSS Frame Generation was requested (DLSSG.Enable=" << dlssg_enable << ") but not active at the moment of the crash (DLSSG.Active=" << dlssg_active << "). The Streamline layer still sits between the engine and the driver when it is loaded, so it stays on the suspect list.\n\n";
            f.evidence.push_back("DLSSG.Enable=" + dlssg_enable + ", DLSSG.Active=" + dlssg_active);
        }
        if (ev.system.dlss_frame_gen_forced) { f.evidence.push_back("Engine.ini forces r.Streamline.DLSSG.Enable=1"); f.confidence = std::max(f.confidence, 85); }
        if (wine == "1" || wine == "true" || ev.system.game_ran_under_wine) {
            d << "The game ran under Wine/Proton, where D3D12 is translated by vkd3d-proton. A device-removed there is often the Vulkan driver or vkd3d, and a Proton version change fixes it more often than a mod change.\n\n";
            f.evidence.push_back("PlatformIsRunningWine=1");
        }
        if (!ev.system.overlays_seen.empty()) {
            d << "Overlay or capture DLLs were inside the process: " << join(ev.system.overlays_seen, ", ") << ". They hook the swap chain and are a known GPU-crash trigger with frame generation.\n\n";
            f.evidence.push_back("overlays in process: " + join(ev.system.overlays_seen, ", ", 6));
        }
        // Mod materials: a CSS outfit material cooked for another shader build can hang the GPU.
        bool css_outfits = false;
        for (const auto& p : ev.paks.containers) if (p.css_package) css_outfits = true;
        if (css_outfits && (icontains(pass, "BasePass") || icontains(pass, "Shadow") || icontains(pass, "Skin") || icontains(pass, "Nanite") || icontains(pass, "Material"))) {
            d << "Outfit packages are installed and the dying pass draws geometry, so a material from a package cooked for an older game build is a candidate too. Grey or black skin on the outfit is the visual symptom of that case.\n\n";
            f.evidence.push_back("CSS outfit containers present and the active pass is " + pass);
        }
        d << "Across the recent reports, " << gpu_count << " of " << ev.crashes.size() << " were GPU crashes.";
        f.details = d.str();
        f.evidence.push_back("CrashType=GPUCrash, " + c.error_message);
        if (!pass.empty()) f.evidence.push_back("active GPU pass: " + pass);
        if (auto drv = prop("RHI.UserDriverVersion"); !drv.empty()) {
            bool placeholder = drv.starts_with("999") || wine == "1" || wine == "true";
            f.evidence.push_back("driver " + drv + " " + prop("RHI.DriverDate") + ", " + prop("RHI.AdapterName") + (placeholder ? " (Wine reports a placeholder driver version; the real one is the host Vulkan driver)" : ""));
        }
        f.advice = "In order: turn frame generation off (and delete the r.Streamline lines from Engine.ini if they are there), close overlays (Discord, RTSS, GeForce overlay, OBS), remove any GPU overclock, update or clean-install the GPU driver. If it only happens with one outfit worn, that outfit's package needs a rebuild for this game build.";
        add(f);
    }

    void oom(const CrashReport& c) {
        Finding f;
        f.id = "crash.oom"; f.severity = Severity::High; f.confidence = 80;
        f.title = "The game ran out of memory";
        f.blame = "memory";
        auto prop = [&](const char* k) { auto it = c.props.find(k); return it == c.props.end() ? std::string() : it->second; };
        f.summary = "Unreal flagged the crash as out-of-memory. Physical RAM, the page file, or a leak in a mod that creates objects every frame.";
        f.details = "Peak used physical: " + prop("MemoryStats.PeakUsedPhysical") + " bytes of " + prop("MemoryStats.TotalPhysical") + " total; the failing allocation asked for " + prop("MemoryStats.OOMAllocationSize") + " bytes. High-resolution outfit textures and many containers raise the baseline; a Lua mod that spawns objects or hooks ReceiveTick without cleaning up raises it over time.";
        f.evidence.push_back("MemoryStats.bIsOOM=1, OOMAllocationSize=" + prop("MemoryStats.OOMAllocationSize"));
        f.advice = "Raise the page file, close other programs, and watch the game's memory in Task Manager while playing: a steady climb means a leak, test with Lua mods disabled.";
        add(f);
    }

    void stack_crash(const CrashReport& c, int repeats) {
        // Module paths from the dump let us tell which main.dll is which.
        std::map<uint64_t, std::string> base_to_path;
        if (c.has_minidump) {
            auto dump = read_minidump(c.minidump);
            if (dump.ok()) for (const auto& m : dump.modules) base_to_path[m.base] = m.name;
        }
        const std::vector<Frame>& frames = !c.frames.empty() ? c.frames : c.minidump_frames;
        // First frame that is not plain runtime plumbing.
        const Frame* top = nullptr;
        for (const auto& fr : frames) if (!runtime_module(fr.module)) { top = &fr; break; }
        std::vector<std::pair<const Frame*, ModuleOwner>> mod_frames;
        for (const auto& fr : frames) {
            auto o = owner_of(fr, base_to_path, ev);
            if (o.is_mod) mod_frames.push_back({&fr, o});
        }
        bool gpu_frame = false;
        for (const auto& fr : frames) if (gpu_module(fr.module)) gpu_frame = true;

        Finding f;
        f.evidence.push_back(c.error_message);
        if (!frames.empty()) {
            std::string stack;
            size_t n = 0;
            for (const auto& fr : frames) { if (n++ == 12) { stack += "\n  ..."; break; } stack += "\n  " + frame_text(fr); }
            f.evidence.push_back("call stack (top first):" + stack);
        }
        if (!c.exception_module.empty()) f.evidence.push_back("faulting instruction in " + c.exception_module + " at " + hex(c.exception_address));
        else if (c.has_minidump && c.minidump_error.empty() && c.exception_address) f.evidence.push_back("faulting address " + hex(c.exception_address) + " lies outside every loaded module (a jump through a bad pointer); the first real frame below it is what matters");

        if (top && owner_of(*top, base_to_path, ev).is_mod) {
            auto o = owner_of(*top, base_to_path, ev);
            f.id = "crash.mod-frame"; f.severity = Severity::Critical; f.confidence = repeats >= 2 ? 92 : 85;
            f.title = "The crash happened inside " + o.label;
            f.blame = o.blame;
            f.summary = "The top of the call stack, after the C runtime's exception plumbing, is code that belongs to " + o.label + ". The faulting instruction executed in that module, which is as direct as crash evidence gets.";
            std::ostringstream d;
            d << "Unreal's crash handler recorded the call stack of the thread that raised the exception. Frames are module plus offset; without symbols the function names are unknown, but the module is certain. ";
            d << "The first frame below VCRUNTIME/kernel32 is " << frame_text(*top) << ", owned by " << o.label << ".\n\n";
            if (lower(top->module) == "ue4ss") {
                d << "UE4SS itself is on top. That usually means one of three things: a Lua mod raised an error that UE4SS could not contain (see the Lua findings), a hook UE4SS installed for a mod fired on an object the game already freed, or this UE4SS build does not match the game build (its signature scan found the wrong address). ";
                if (!ev.ue4ss.lua_errors.empty()) d << "UE4SS.log does contain Lua errors, which strengthens the first explanation. ";
                if (mod_frames.size() > 1) { d << "Deeper in the stack: "; for (size_t i = 1; i < mod_frames.size() && i < 4; ++i) d << mod_frames[i].second.label << "; "; d << "so the hook that UE4SS was servicing belonged to that mod.\n\n"; }
                else d << "\n\n";
            } else if (o.ours) {
                d << "This is one of eins0fx's mods. Its own log (see the CSS/CSSX/CCS section) will show the last action before the crash; include it in the bundle when reporting.\n\n";
            } else {
                d << "Disable this mod and reproduce. If the crash stops, report the stack above to the mod's author with this report attached.\n\n";
            }
            d << "Address " << hex(c.exception_address) << ": ";
            if (c.exception_address < 0x10000) d << "a near-null read or write, which is a null pointer plus a field offset. The code assumed an object existed and it did not (a freed actor, a missing component, a mesh that failed to load).";
            else if (c.exception_address == 0xffffffffffffffffull) d << "an all-ones address, typical of a -1 index or an invalid handle used as a pointer.";
            else d << "a non-null address, so the pointer was stale or corrupted rather than missing (use after free, or a struct read with the wrong layout for this game build).";
            f.details = d.str();
            f.advice = o.ours
                ? "Update " + o.blame + " to the latest release first; if it is current, send the ZIP bundle with the steps that trigger it."
                : "Disable " + o.blame + " (remove its enabled.txt or set it to 0 in mods.txt, or move its folder out of ue4ss/Mods) and play again. One run without it answers the question.";
            if (auto k = known_mod(ev.knowledge, o.blame)) f.evidence.push_back("known: " + k->display + ": " + k->note);
            add(f);
        } else if (!mod_frames.empty()) {
            // Game code on top, but a mod is on the path: the mod handed the engine something bad.
            auto o = mod_frames.front().second;
            f.id = "crash.mod-in-stack"; f.severity = Severity::High; f.confidence = 65;
            f.title = "Game code crashed while running on behalf of " + o.label;
            f.blame = o.blame;
            f.summary = "The faulting frame is the game's own code, but " + o.label + " is further down the same call stack. The engine was executing something that module asked for (a function call, a hook, a property write) and tripped on the data it was given.";
            std::ostringstream d;
            d << "Top frame: " << (top ? frame_text(*top) : "none") << ". Mod frames on the stack: ";
            for (const auto& [fr, ow] : mod_frames) d << ow.label << " at " << frame_text(*fr) << "; ";
            d << "\n\nThis pattern fits a mod that calls into the engine with a wrong argument, a stale object, or a struct laid out for a different game build. It also fits a UE4SS hook that fires after the engine freed the object. The game by itself does not crash with a mod on the stack, so the mod is the thing to remove first. If the crash stays without it, the game or a pak container is the cause instead.";
            f.details = d.str();
            f.advice = "Disable " + o.blame + " first. If the crash continues, the next suspects are the pak containers and the game files (see below).";
            add(f);
        } else if (gpu_frame) {
            f.id = "crash.gpu-frame"; f.severity = Severity::High; f.confidence = 65;
            f.title = "The exception came out of the graphics driver";
            f.blame = "GPU driver";
            f.summary = "A frame of the call stack is inside the graphics driver or the DLSS/FSR layer. The driver faulted on the CPU side instead of removing the device, which is the same family of problem as a GPU crash.";
            f.details = "See the system findings for frame generation, overlays and Wine. Driver reinstall and frame generation off are the two moves that resolve most of these.";
            f.advice = "Frame generation off, overlays closed, clean driver install.";
            add(f);
        } else {
            f.id = "crash.game-frame"; f.severity = Severity::Medium; f.confidence = 45;
            f.title = "The crash is in game code with no mod on the stack";
            f.blame = "game or a pak container";
            f.summary = "Every frame belongs to MortalShell2-Win64-Shipping. That does not clear mods: a replaced asset (mesh, animation, material, Blueprint) from a pak container crashes inside engine code with nothing of the mod on the stack, because the engine is the one reading the bad data.";
            std::ostringstream d;
            d << "Address " << hex(c.exception_address) << (c.exception_address < 0x10000 ? " (near-null, a missing object)" : " (a stale or corrupt pointer)") << ". ";
            size_t mod_containers = 0;
            for (const auto& p : ev.paks.containers) if (p.is_mod) ++mod_containers;
            d << mod_containers << " mod container(s) are installed. Containers that override the game's own packages are the first suspects for this shape of crash; the container findings list them. A game bug is possible too: compare with the game's own community reports for this build (" << c.engine_version << ").";
            if (ev.ue4ss.installed) d << "\n\nUE4SS is installed but not on this stack, so its mods were not executing at the moment of the crash. A mod can still have set up the situation earlier (changed a mesh, hidden a component), which is why the next step is still a run without mods.";
            f.details = d.str();
            f.advice = "Run once with Content/Paks/~mods moved aside. If it survives, put containers back in halves until the crash returns.";
            add(f);
        }
    }

    void after_update() {
        if (ev.manifest.exe_when == FileTime{}) return;
        std::vector<std::string> old_mods, old_paks;
        for (const auto& m : ev.ue4ss.mods) {
            if (m.builtin || !m.effectively_enabled) continue;
            if (m.newest_file != FileTime{} && m.newest_file < ev.manifest.exe_when) old_mods.push_back(m.name);
        }
        for (const auto& p : ev.paks.containers) if (p.is_mod && p.when != FileTime{} && p.when < ev.manifest.exe_when) old_paks.push_back(p.stem);
        bool crashes_after = false;
        for (const auto& c : ev.crashes) if (c.when > ev.manifest.exe_when) crashes_after = true;
        if (!crashes_after || (old_mods.empty() && old_paks.empty())) return;
        Finding f;
        f.id = "crash.after-update"; f.severity = Severity::Medium; f.confidence = 55;
        f.title = "Mods predate the game's last update";
        f.blame = "outdated mods";
        f.summary = "The game executable is dated " + format_time(ev.manifest.exe_when) + (ev.manifest.steam_build_id.empty() ? "" : " (Steam build " + ev.manifest.steam_build_id + ")") + " and crashes exist after that date. " + std::to_string(old_mods.size()) + " UE4SS mod(s) and " + std::to_string(old_paks.size()) + " container(s) were installed before the update, so none of them has been confirmed against this build.";
        f.details = "A game patch changes the executable's layout and can change cooked asset formats and shader hashes. C++ mods that read engine structs by offset, UE4SS builds whose signatures no longer match, and outfit packages whose materials reference shader hashes that the patch removed all break in ways that look like random crashes or grey models.\n\nOlder UE4SS mods: " + join(old_mods, ", ", 20) + "\nOlder containers: " + join(old_paks, ", ", 20);
        for (const auto& kb : ev.knowledge.builds) if (kb.build_id == ev.manifest.steam_build_id) f.evidence.push_back("build " + kb.build_id + " (" + kb.date + "): " + kb.note);
        f.advice = "Check each mod's page for an update that mentions this game build. For CSS outfit packages, the package author must rebuild against the new build if models turn grey.";
        add(f);
    }

    // ---------- UE4SS ----------
    void ue4ss() {
        const auto& u = ev.ue4ss;
        if (!u.installed) {
            if (!u.mods.empty() || !ev.ours.empty()) {
                bool any_ours = false;
                for (const auto& o : ev.ours) if (o.installed) any_ours = true;
                if (any_ours) {
                    Finding f;
                    f.id = "ue4ss.missing"; f.severity = Severity::High; f.confidence = 70;
                    f.title = "UE4SS.dll is missing but UE4SS mods are installed";
                    f.blame = "UE4SS install";
                    f.summary = "The ue4ss folder has mods in it, but no UE4SS.dll. Nothing in ue4ss/Mods loads without it, so CSS, CSSX and CCS are inert.";
                    f.advice = "Reinstall UE4SS for Mortal Shell II (the NO AOB build) with the game closed.";
                    add(f);
                }
            }
            return;
        }
        if (u.proxy_dll.empty()) {
            Finding f;
            f.id = "ue4ss.no-proxy"; f.severity = Severity::Medium; f.confidence = 60;
            f.title = "No UE4SS loader DLL beside the game executable";
            f.blame = "UE4SS install";
            f.summary = "UE4SS.dll is present but none of the proxy DLLs (dwmapi.dll for the MS2 build) sits next to MortalShell2-Win64-Shipping.exe, so the game never loads it. Mods will silently not run.";
            f.advice = "Copy dwmapi.dll from the UE4SS package into Binaries/Win64.";
            add(f);
        }
        // Build identity.
        const KnownUe4ss* known = nullptr;
        for (const auto& k : ev.knowledge.ue4ss) if (k.sha256 == u.dll_sha256) known = &k;
        if (known && !known->supported) {
            Finding f;
            f.id = "ue4ss.unsupported-build"; f.severity = Severity::High; f.confidence = 75;
            f.title = "This UE4SS build is not the one the native mods expect";
            f.blame = "UE4SS";
            f.summary = "UE4SS.dll is " + known->version + ". " + known->note + " C++ mods import functions by name from UE4SS.dll; a different build exports different names or different struct layouts, and the result is a failed load or a crash in the first hook.";
            f.evidence.push_back("UE4SS.dll sha256 " + u.dll_sha256);
            f.advice = "Install the UE4SS build named on the CSS page (3.0.1 g97b7e501, NO AOB).";
            add(f);
        } else if (!known) {
            Finding f;
            f.id = "ue4ss.unknown-build"; f.severity = Severity::Low; f.confidence = 40;
            f.title = "Unrecognised UE4SS build";
            f.blame = "UE4SS";
            f.summary = "UE4SS.dll's hash is not in WCMG's list (" + (u.version_line.empty() ? std::string("version line not found in UE4SS.log") : u.version_line) + "). It may be a newer release; it may be a build from another game. The C++ mods in this folder were built against 3.0.1 g97b7e501.";
            f.evidence.push_back("UE4SS.dll sha256 " + u.dll_sha256 + ", " + format_bytes(u.dll_bytes));
            f.advice = "If native mods fail to load or crash on start, swap in the build linked from the CSS page.";
            add(f);
        }
        if (!u.other_proxy_dlls.empty()) {
            Finding f;
            f.id = "ue4ss.other-injectors"; f.severity = Severity::Medium; f.confidence = 50;
            f.title = "Other injected DLLs sit beside the game executable";
            f.blame = join(u.other_proxy_dlls, ", ");
            f.summary = "Besides the UE4SS loader, these DLLs are in Binaries/Win64 and load into the game by name: " + join(u.other_proxy_dlls, ", ") + ". ReShade, Special K, trainers and overlays use this trick. Each one hooks the renderer or the input path and is a crash candidate on its own, doubly so together with UE4SS.";
            f.advice = "Move them out one at a time and test.";
            add(f);
        }
        // Log health.
        if (!u.errors.empty() || !u.lua_errors.empty()) {
            std::set<std::string> culprits;
            for (const auto& l : u.lua_errors) {
                // "[Lua] [ModName] ..." or a script path ".../Mods/<Name>/Scripts/..."
                auto t = l.text;
                auto p = t.find("[Lua] [");
                if (p != std::string::npos) { auto e = t.find(']', p + 7); if (e != std::string::npos) culprits.insert(t.substr(p + 7, e - p - 7)); }
                auto m = t.find("Mods\\"); if (m == std::string::npos) m = t.find("Mods/");
                if (m != std::string::npos) { auto e = t.find_first_of("/\\", m + 5); if (e != std::string::npos) culprits.insert(t.substr(m + 5, e - m - 5)); }
            }
            Finding f;
            f.id = "ue4ss.log-errors"; f.severity = u.lua_errors.empty() ? Severity::Low : Severity::Medium; f.confidence = u.lua_errors.empty() ? 35 : 55;
            f.title = u.lua_errors.empty() ? "UE4SS.log reports errors" : "Lua errors in UE4SS.log";
            f.blame = culprits.empty() ? "UE4SS mods" : join(std::vector<std::string>(culprits.begin(), culprits.end()), ", ");
            f.summary = std::to_string(u.errors.size()) + " error line(s) and " + std::to_string(u.lua_errors.size()) + " Lua error line(s) in the current UE4SS.log" + (culprits.empty() ? "." : ", naming: " + f.blame + ".") + " A Lua error is usually caught and only kills that script, but errors inside a hook callback run on the game thread and can take the game with them.";
            std::string d = "Last error lines:\n";
            size_t n = 0;
            for (const auto& l : u.lua_errors) { if (n++ == 8) break; d += "  " + l.time + " " + l.text + "\n"; }
            for (const auto& l : u.errors) { if (n++ == 14) break; d += "  " + l.time + " " + l.text + "\n"; }
            if (!u.log_from_this_boot) d += "\nNote: UE4SS.log is rewritten every launch. This log is from a later launch than the newest crash report, so it describes a different session.";
            f.details = d;
            f.advice = "Disable the named mod(s) and check the log stays clean. Lua tracebacks name the script and line, which the mod's author needs.";
            add(f);
        }
        if (u.installed && !u.log_reached_game_thread && !u.tail.empty()) {
            Finding f;
            f.id = "ue4ss.stuck-at-start"; f.severity = Severity::High; f.confidence = 60;
            f.title = "UE4SS did not get past its startup scan";
            f.blame = "UE4SS";
            f.summary = "The log ends before UE4SS reports the engine as ready (no FName verification or hash-table self test). When the signature scan fails on a new game build, UE4SS stops here and the game either continues without mods or crashes during the scan.";
            std::string t;
            size_t start = u.tail.size() > 10 ? u.tail.size() - 10 : 0;
            for (size_t i = start; i < u.tail.size(); ++i) t += "  " + u.tail[i].time + " " + u.tail[i].text + "\n";
            f.details = "Last lines of UE4SS.log:\n" + t;
            f.advice = "Confirm the UE4SS build matches the game build; the MS2 NO AOB build is tied to a specific game version.";
            add(f);
        }
        // Per mod observations.
        for (const auto& m : u.mods) {
            if (!m.effectively_enabled || m.builtin) continue;
            std::vector<std::string> notes = m.risk_notes;
            Severity sev = Severity::Info; int conf = 25;
            if (m.native_loads) { sev = Severity::Low; conf = 35; }
            if (m.raw_memory_sites) { sev = Severity::Low; conf = 35; }
            for (const auto& h : m.hooked_functions) if (icontains(h, "ReceiveTick") || icontains(h, "Tick")) { sev = std::max(sev, Severity::Low); }
            auto k = known_mod(ev.knowledge, m.name);
            if (k) { sev = std::max(sev, k->severity); conf = std::max(conf, 45); }
            // Our own mods have their own section; a profile entry would only repeat it.
            if (m.ours && !k) continue;
            if (notes.empty() && !k && !m.has_dll) continue;
            Finding f;
            f.id = "mod.profile"; f.severity = sev; f.confidence = conf;
            f.title = "Mod " + m.name + ": " + (m.has_dll && m.has_scripts ? "Lua + native DLL" : m.has_dll ? "native C++ mod" : "Lua mod");
            f.blame = m.name;
            std::ostringstream s;
            s << (m.has_dll ? std::to_string(m.dlls.size()) + " DLL(s), " : "") << m.lua_files.size() << " Lua file(s), " << m.hooks << " hook site(s), " << m.game_thread_calls << " game-thread call(s), " << m.raw_memory_sites << " raw memory site(s)";
            if (m.native_loads) s << ", loads native code from Lua";
            s << ". Newest file " << format_time(m.newest_file) << ".";
            f.summary = s.str();
            std::string d;
            if (k) d += k->display + ": " + k->note + "\n\n";
            if (!m.hooked_functions.empty()) d += "Hooks: " + join(m.hooked_functions, ", ", 12) + "\n";
            if (!notes.empty()) d += "Notes:\n  " + join(notes, "\n  ", 12) + "\n";
            if (m.has_dll) {
                d += "DLLs:\n";
                for (size_t i = 0; i < m.dlls.size() && i < 6; ++i) d += "  " + path_utf8(m.dlls[i].filename()) + " " + format_bytes(i < m.dll_bytes.size() ? m.dll_bytes[i] : 0) + " sha256 " + (i < m.dll_sha256.size() ? m.dll_sha256[i].substr(0, 16) : "") + "...\n";
                d += "A native mod reads engine memory directly; it is tied to the UE4SS build and the game build it was compiled against.\n";
            }
            f.details = d;
            f.advice = "Nothing to do unless it appears in a crash stack or the log; this entry exists so the bundle shows what the mod does.";
            add(f);
        }
    }

    // ---------- CSS / CSSX / CCS ----------
    void ours() {
        for (const auto& o : ev.ours) {
            if (!o.installed) continue;
            if (o.loader_mismatch) {
                Finding f;
                f.id = "ours.loader-mismatch"; f.severity = Severity::High; f.confidence = 70;
                f.title = o.name + ": loader DLL does not match its contract";
                f.blame = o.name;
                f.summary = "loader-contract.json records a different hash for dlls/main.dll than the file on disk. A partial update (new core, old loader, or the other way round) leaves the loader and core speaking different ABIs, which fails at activation or crashes on the first call.";
                f.evidence.push_back("recorded " + o.loader_sha_recorded.substr(0, 16) + "..., on disk " + o.loader_sha_actual.substr(0, 16) + "...");
                f.advice = "Reinstall the " + o.name + " release ZIP over the folder with the game closed (keep state/).";
                add(f);
            }
            if (!o.active_core.empty() && !o.active_core_present) {
                Finding f;
                f.id = "ours.core-missing"; f.severity = Severity::High; f.confidence = 75;
                f.title = o.name + ": core.json names a core that is not installed";
                f.blame = o.name;
                f.summary = "core.json points at " + o.active_core + " but that file is not in the cores folder (installed: " + (o.cores.empty() ? "none" : join(o.cores, ", ", 6)) + "). The loader has nothing to activate, so the mod does nothing, and an older loader logs an error every launch.";
                f.advice = "Reinstall the release ZIP, or edit core.json to one of the cores present.";
                add(f);
            }
            if (!o.errors.empty()) {
                Finding f;
                f.id = "ours.log-errors"; f.severity = Severity::Medium; f.confidence = 50;
                f.title = o.name + " logged " + std::to_string(o.errors.size()) + " error line(s)";
                f.blame = o.name;
                f.summary = "The mod's own log has error lines. They do not prove a crash by themselves, but they say what the mod was struggling with before it.";
                std::string d = "Last error lines:\n";
                size_t start = o.errors.size() > 10 ? o.errors.size() - 10 : 0;
                for (size_t i = start; i < o.errors.size(); ++i) d += "  " + o.errors[i] + "\n";
                d += "\nLast lines of the log:\n";
                for (const auto& l : o.tail) d += "  " + l + "\n";
                f.details = d;
                f.advice = "Include the ZIP bundle when reporting; the author reads these lines first.";
                add(f);
            }
            if (o.corrupt_state_files > 0) {
                Finding f;
                f.id = "ours.corrupt-state"; f.severity = Severity::Low; f.confidence = 40;
                f.title = o.name + ": " + std::to_string(o.corrupt_state_files) + " corrupt state file(s) set aside";
                f.blame = o.name;
                f.summary = "The mod found its state.json unreadable on " + std::to_string(o.corrupt_state_files) + " occasion(s) and renamed it. That happens when the game dies while the state is being written, so each one marks an earlier hard crash or kill, and once in a while the file was half-written by another tool.";
                f.advice = "Harmless by itself; the mod starts with defaults. Delete the .corrupt-* files when you no longer need them.";
                add(f);
            }
            if (o.dev_channel_enabled) {
                Finding f;
                f.id = "ours.dev-channel"; f.severity = Severity::Info; f.confidence = 30;
                f.title = o.name + ": developer channel is enabled";
                f.blame = o.name;
                f.summary = "dev/enabled.txt exists, so the mod accepts live requests from the file channel and may run development cores. Players do not need this.";
                f.advice = "Delete dev/enabled.txt unless you are testing a build.";
                add(f);
            }
        }
    }

    // ---------- containers ----------
    void paks() {
        const auto& p = ev.paks;
        if (!p.error.empty()) {
            Finding f; f.id = "pak.scan-error"; f.severity = Severity::Info; f.confidence = 20;
            f.title = "Container scan incomplete"; f.blame = "WCMG"; f.summary = p.error; add(f);
        }
        for (const auto& c : p.containers) {
            if (!c.is_mod) continue;
            if (c.complete && !c.problem.empty()) {
                Finding f;
                f.id = "pak.note"; f.severity = Severity::Info; f.confidence = 20;
                f.title = "Container " + c.stem + ": " + c.problem;
                f.blame = c.stem;
                f.summary = "Not a failure. Listed so the bundle shows what kind of container this is.";
                f.advice = "Nothing to do.";
                add(f);
            }
            if (!c.complete) {
                Finding f;
                f.id = "pak.broken"; f.severity = Severity::High; f.confidence = 70;
                f.title = "Container " + c.stem + " is incomplete or unreadable";
                f.blame = c.stem;
                f.summary = (c.problem.empty() ? std::string("The container is missing one of its files.") : c.problem) + " Unreal mounts every container at startup; one it cannot read is either skipped with a log line or, with a utoc whose ucas is missing, a crash during mount.";
                f.evidence.push_back("pak " + (c.pak.empty() ? std::string("missing") : format_bytes(c.pak_bytes)) + ", utoc " + (c.utoc.empty() ? std::string("missing") : format_bytes(c.utoc_bytes)) + ", ucas " + (c.ucas.empty() ? std::string("missing") : format_bytes(c.ucas_bytes)));
                f.advice = "Delete the folder and extract the mod's archive again. All three files (.pak, .utoc, .ucas) belong together.";
                add(f);
            }
            bool shared = false;
            std::vector<std::string> shared_hits;
            for (const auto& h : c.override_hints) {
                if (icontains(h, "_Shared") || icontains(h, "Core/Characters") || icontains(h, "Humans/Player") || icontains(h, "/Engine") || h.starts_with("Engine")) { shared = true; shared_hits.push_back(h); }
            }
            if (shared) {
                Finding f;
                f.id = "pak.override-shared"; f.severity = Severity::Medium; f.confidence = 55;
                f.title = c.stem + " replaces shared game assets";
                f.blame = c.stem;
                f.summary = "This container overrides " + std::to_string(c.overrides_game) + " of the game's own packages, including shared ones (" + join(shared_hits, ", ", 5) + "). Shared skeletons, animation blueprints and player base classes are used by every shell and by other mods; a version from an older build, or two mods replacing the same one, is a classic source of crashes that look like the game's fault.";
                if (!c.override_names.empty()) f.evidence.push_back("overrides: " + join(c.override_names, ", ", 8));
                f.advice = "Test without this container. If another mod also touches the same folder (see the collision finding), keep only one.";
                add(f);
            } else if (c.overrides_game > 0 && !c.css_package) {
                Finding f;
                f.id = "pak.override"; f.severity = Severity::Low; f.confidence = 35;
                f.title = c.stem + " replaces " + std::to_string(c.overrides_game) + " game package(s)";
                f.blame = c.stem;
                f.summary = "A replacement mod: it ships the game's own asset paths so the engine draws its copy. Fine when built for this game build; after a patch, a replaced mesh or material can reference formats or shader hashes the game no longer has.";
                if (!c.override_hints.empty()) f.evidence.push_back("touches: " + join(c.override_hints, ", ", 8));
                f.advice = "If crashes started after a game update, this is one of the containers to test without.";
                add(f);
            }
        }
        if (!p.mod_collisions.empty()) {
            std::map<std::string, std::set<std::string>> by_pair;
            for (const auto& col : p.mod_collisions) by_pair[join(col.containers, " + ")].insert(col.name.empty() ? hex(col.package_id) : col.name);
            Finding f;
            f.id = "pak.collision"; f.severity = Severity::Medium; f.confidence = 55;
            f.title = std::to_string(p.mod_collisions.size()) + " package(s) are shipped by more than one mod container";
            f.blame = "conflicting containers";
            f.summary = "Two or more mod containers carry the same package. Only one copy wins (the engine picks by mount order, which players cannot see) and the loser's other assets may reference the winner's incompatible version. Visual glitches at best, a crash when the asset is a skeleton, animation or Blueprint.";
            std::string d;
            for (const auto& [pair, names] : by_pair) {
                d += pair + ":\n";
                size_t n = 0;
                for (const auto& nm : names) { if (n++ == 6) { d += "    ...\n"; break; } d += "    " + nm + "\n"; }
            }
            f.details = d;
            f.advice = "Keep one of each pair. Two outfits that both ship a shared body or hair asset need the same version of it.";
            add(f);
        }
        if (!p.stray_files.empty()) {
            Finding f;
            f.id = "pak.stray"; f.severity = Severity::Low; f.confidence = 30;
            f.title = "Files that are not containers sit in the mods folder";
            f.blame = "install mistake";
            f.summary = "Found in ~mods or LogicMods: " + join(p.stray_files, ", ", 8) + ". The engine ignores them, so a mod left as a .zip there is simply not installed, and a player then reports it as broken.";
            f.advice = "Extract archives so that each mod's .pak/.utoc/.ucas trio sits in its own folder, and delete the archives.";
            add(f);
        }
        if (!p.mods_outside_mods_folder.empty()) {
            Finding f;
            f.id = "pak.outside"; f.severity = Severity::Low; f.confidence = 30;
            f.title = "Patch containers placed outside ~mods";
            f.blame = "install mistake";
            f.summary = join(p.mods_outside_mods_folder, ", ", 8) + " are _P containers directly under Content/Paks. They load, but tools and mod managers will not see them, and a game update can delete or ignore them.";
            f.advice = "Move each into Content/Paks/~mods/<Name>/.";
            add(f);
        }
        // CSS packages without the shared container they may need.
        bool css = false, shared = false;
        for (const auto& c : p.containers) { if (c.css_package) css = true; if (icontains(c.stem, "CSS_SharedAssets")) shared = true; }
        if (css && !shared) {
            Finding f;
            f.id = "pak.css-shared-missing"; f.severity = Severity::Low; f.confidence = 35;
            f.title = "CSS outfit packages installed without CSS_SharedAssets_P";
            f.blame = "CSS packages";
            f.summary = "Some CSS outfit packages reference assets from the shared container. Without it the affected outfit shows missing parts or default materials, and a reference that fails to resolve during a wear can crash.";
            f.advice = "Install CSS_SharedAssets_P from the CSS page if any outfit looks incomplete.";
            add(f);
        }
    }

    // ---------- manifest / game files ----------
    void manifest() {
        const auto& m = ev.manifest;
        if (!m.present) return;
        if (m.missing_total > 0) {
            Finding f;
            f.id = "manifest.missing"; f.severity = Severity::High; f.confidence = 65;
            f.title = std::to_string(m.missing_total) + " shipped game file(s) are missing";
            f.blame = "game files";
            f.summary = "Files listed in the game's own build manifest are not on disk. A pak or a shipped DLL missing means content fails to mount or the executable fails to load; both show as crashes at start or when entering an area.";
            f.evidence.push_back(join(m.missing, ", ", 10));
            f.advice = "Steam: right click the game, Properties, Installed Files, Verify integrity of game files. Mods survive that; replaced game files do not.";
            add(f);
        }
        if (!m.replaced_binaries.empty()) {
            Finding f;
            f.id = "manifest.replaced"; f.severity = Severity::High; f.confidence = 60;
            f.title = "Shipped binaries were replaced";
            f.blame = "modified game files";
            f.summary = "These files differ from what Steam installed: " + join(m.replaced_binaries, ", ", 8) + ". Patched executables and swapped engine DLLs (DLSS, Streamline, FSR replacements) are a crash source on their own and they also move the addresses UE4SS scans for.";
            f.advice = "Verify game files in Steam, then re-apply only the swaps you know you need.";
            add(f);
        }
        std::vector<std::string> injected, loose_paks, other;
        for (const auto& fpath : m.foreign) {
            auto l = lower(fpath);
            if (l.ends_with(".dll") && (l.find("binaries/win64") != std::string::npos)) injected.push_back(fpath);
            else if (l.ends_with(".pak") || l.ends_with(".utoc") || l.ends_with(".ucas")) loose_paks.push_back(fpath);
            else other.push_back(fpath);
        }
        // The UE4SS loader is expected.
        std::erase_if(injected, [&](const std::string& s) { return !ev.ue4ss.proxy_dll.empty() && icontains(s, ev.ue4ss.proxy_dll); });
        if (!injected.empty()) {
            Finding f;
            f.id = "manifest.injected-dlls"; f.severity = Severity::Medium; f.confidence = 50;
            f.title = std::to_string(injected.size()) + " DLL(s) beside the executable that the game did not ship";
            f.blame = "injected DLLs";
            f.summary = join(injected, ", ", 8) + ". Anything with a system DLL's name in that folder loads into the game before the engine starts. Overlays, ReShade, trainers and DLSS swappers live here.";
            f.advice = "Move them out and test; put back one at a time.";
            add(f);
        }
        if (!loose_paks.empty()) {
            Finding f;
            f.id = "manifest.loose-paks"; f.severity = Severity::Low; f.confidence = 35;
            f.title = "Container files outside the mods folder";
            f.blame = "install mistake";
            f.summary = join(loose_paks, ", ", 8) + " are not part of the game and not under ~mods. If their names lack the _P suffix the engine may mount them below the game's own content, so they do nothing, or above it with no way to tell.";
            f.advice = "Move mod containers into Content/Paks/~mods.";
            add(f);
        }
        if (m.foreign_total > 0 && injected.empty() && loose_paks.empty() && !other.empty()) {
            Finding f;
            f.id = "manifest.foreign"; f.severity = Severity::Info; f.confidence = 20;
            f.title = std::to_string(m.foreign_total) + " extra file(s) in the game folders";
            f.blame = "none";
            f.summary = "Files not in the shipped manifest and not known mod locations: " + join(other, ", ", 8) + ". Caches, backups and tool leftovers are the usual content; listed so the bundle shows them.";
            f.advice = "Nothing to do unless one of them is a renamed DLL or pak.";
            add(f);
        }
    }

    // ---------- system ----------
    void system() {
        const auto& s = ev.system;
        if (s.dlss_frame_gen_forced) {
            Finding f;
            f.id = "sys.dlssg-forced"; f.severity = Severity::Medium; f.confidence = 55;
            f.title = "Engine.ini forces DLSS Frame Generation on";
            f.blame = "Engine.ini";
            f.summary = "Saved/Config/Windows/Engine.ini sets r.Streamline.DLSSG.Enable=1 (and the Load flag). The game's menu normally gates frame generation behind support checks; forcing it skips those. Combined with the GPU crash history this is a strong lead.";
            f.advice = "Delete the [ConsoleVariables] Streamline lines from Engine.ini and use the in-game toggle instead.";
            add(f);
        }
        if (s.fsr_frame_interp) {
            Finding f;
            f.id = "sys.fsr-fi"; f.severity = Severity::Low; f.confidence = 40;
            f.title = "FSR frame interpolation forced in Engine.ini";
            f.blame = "Engine.ini";
            f.summary = "r.FidelityFX.FI.Enabled=1 is set by hand. Same caveat as DLSS frame generation: it bypasses the game's checks.";
            f.advice = "Remove the line and use the in-game setting.";
            add(f);
        }
        if (!s.overlays_seen.empty()) {
            Finding f;
            f.id = "sys.overlays"; f.severity = Severity::Low; f.confidence = 40;
            f.title = "Overlay or capture software was inside the game process";
            f.blame = join(s.overlays_seen, ", ");
            f.summary = "The crash minidumps list these modules loaded in the game: " + join(s.overlays_seen, ", ", 8) + ". Overlays hook Present() and the input path; with frame generation or a second injector (UE4SS) they are a known crash multiplier.";
            f.advice = "Close them for a test run (Discord overlay, GeForce overlay, RTSS, OBS game capture).";
            add(f);
        }
        if (s.game_ran_under_wine || ev.paths.running_under_wine) {
            Finding f;
            f.id = "sys.wine"; f.severity = Severity::Info; f.confidence = 30;
            f.title = "The game runs under Wine or Proton";
            f.blame = "platform";
            f.summary = "Crash contexts say PlatformIsRunningWine. D3D12 goes through vkd3d-proton and the Windows crash reporter runs inside the prefix. GPU crashes here are often the Vulkan driver or the Proton version, and UE4SS's GUI console needs the dx11 setting.";
            f.advice = "Try a different Proton version (Experimental or GE) before changing mods when the crash is GPU-side.";
            add(f);
        }
        if (s.total_ram && s.total_ram < 15ull * 1024 * 1024 * 1024) {
            Finding f;
            f.id = "sys.low-ram"; f.severity = Severity::Low; f.confidence = 35;
            f.title = "Less than 16 GB of RAM";
            f.blame = "memory";
            f.summary = format_bytes(s.total_ram) + " physical memory. The game plus outfit packages with 4K textures plus overlays can exceed that, which ends in an out-of-memory crash or a page-file stutter the player reads as a freeze.";
            f.advice = "Raise the page file and keep texture quality a notch below Ultra.";
            add(f);
        }
        if (!ev.scan_errors.empty()) {
            Finding f;
            f.id = "wcmg.scan-errors"; f.severity = Severity::Info; f.confidence = 10;
            f.title = "Parts of the scan failed";
            f.blame = "WCMG";
            f.summary = join(ev.scan_errors, "; ", 6);
            f.advice = "The rest of the report still stands; report this to the WCMG author if it repeats.";
            add(f);
        }
    }
};

int rank(Severity s) { return int(s); }
}

std::vector<Finding> run_rules(const Evidence& ev) {
    Rules r{ev, {}};
    r.crashes();
    r.ue4ss();
    r.ours();
    r.paks();
    r.manifest();
    r.system();
    std::stable_sort(r.out.begin(), r.out.end(), [](const Finding& a, const Finding& b) {
        if (rank(a.severity) != rank(b.severity)) return rank(a.severity) > rank(b.severity);
        return a.confidence > b.confidence;
    });
    return r.out;
}

std::string write_verdict(const Evidence& ev, const std::vector<Finding>& findings) {
    std::ostringstream v;
    const Finding* top = findings.empty() ? nullptr : &findings.front();
    if (!top || top->severity <= Severity::Low) {
        v << "No single cause stands out. ";
        if (ev.crashes.empty()) v << "There are no crash reports to read, so this is a health check of the install rather than a diagnosis. ";
        else v << "The newest crash (" << ev.crashes.front().crash_type << ", " << format_time(ev.crashes.front().when) << ") did not point at a mod, a container or a file with any confidence. ";
        v << "Start by running without mods once; if it still crashes, verify game files in Steam and update the GPU driver.";
        return v.str();
    }
    v << "Most probable cause: " << top->blame << " (" << severity_name(top->severity) << ", " << top->confidence << "% confidence). " << top->summary;
    // Supporting findings of the same blame raise the story; different blames are alternatives.
    std::vector<const Finding*> alt;
    for (size_t i = 1; i < findings.size(); ++i) if (findings[i].severity >= Severity::Medium && findings[i].blame != top->blame) alt.push_back(&findings[i]);
    if (!alt.empty()) {
        v << "\n\nAlso worth ruling out: ";
        size_t n = 0;
        for (auto f : alt) { if (n == 3) break; v << (n ? "; " : "") << f->blame << " (" << f->title << ")"; ++n; }
        v << ".";
    }
    v << "\n\nNext step: " << top->advice;
    return v.str();
}
}

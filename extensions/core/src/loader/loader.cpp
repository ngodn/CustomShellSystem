// CSSX permanent loader: the UE4SS C++ mod at ue4ss/Mods/CSSX/dlls/main.dll.
// Owns the one engine tick callback, the frame-interval ring, the hook
// service and core selection. Everything else lives in the replaceable core.
#include "core_abi.h"
#include "hook_service.hpp"
#include "cssx_version.hpp"
#include "common.hpp"
#include "frame_stats.hpp"
#include <windows.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <optional>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <Input/KeyDef.hpp>

namespace {
using cssx::Json;
std::filesystem::path g_log_path;
std::mutex g_log_gate;
void log_line(const char* message) {
    try {
        std::lock_guard lock(g_log_gate);
        std::ofstream out(g_log_path, std::ios::app);
        out << cssx::utc_timestamp() << ' ' << message << '\n';
    } catch (...) {}
}
constexpr uint32_t ring_capacity = 8192;

class Loader final : public RC::CppUserModBase {
    std::shared_ptr<std::recursive_mutex> gate_ = std::make_shared<std::recursive_mutex>();
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    CssxHookService hooks_{gate_};
    std::filesystem::path root_, mods_root_;
    std::wstring root_string_, mods_string_;
    // Frame ring: written only from the tick callback, read by the core.
    int64_t intervals_[ring_capacity]{};
    CssxFrameRing ring_{ring_capacity, 0, 0, intervals_, 0};
    int64_t last_tick_qpc_ = 0;
    CssxLoaderHost host_{};
    HMODULE module_{};
    const CssxCoreApi* api_{};
    void* core_{};
    RC::Unreal::Hook::GlobalCallbackId hook_{};
    std::string loaded_, observed_;
    std::atomic<bool> selector_dirty_{true};
    std::atomic<uint32_t> hotkey_presses_{0};
    static uint32_t take_hotkey(void* context) { return static_cast<Loader*>(context)->hotkey_presses_.exchange(0); }
    // Map the first configured open key to a UE4SS key id. Only plain keys
    // that never collide with gameplay are accepted here; anything else is
    // left to the core's in-game polling.
    static std::optional<RC::Input::Key> hotkey_from_settings(const std::filesystem::path& root) {
        try {
            auto keys = cssx::read_json(root / "settings.json").value("open_keyboard", Json::array());
            if (!keys.is_array() || keys.size() != 1) return std::nullopt;
            const auto name = keys[0].get<std::string>();
            using K = RC::Input::Key;
            static const std::pair<const char*, K> table[] = {{"F1",K::F1},{"F2",K::F2},{"F3",K::F3},{"F4",K::F4},{"F5",K::F5},{"F6",K::F6},{"F7",K::F7},{"F8",K::F8},{"F9",K::F9},{"F11",K::F11},
                {"Insert",K::INS},{"Delete",K::DEL},{"Home",K::HOME},{"End",K::END},{"PageUp",K::PAGE_UP},{"PageDown",K::PAGE_DOWN}};
            for (const auto& [text, key] : table) if (name == text) return key;
        } catch (...) {}
        return std::nullopt;
    }
    HANDLE watch_ = INVALID_HANDLE_VALUE;
    bool stopped_ = false, stop_pending_ = false;

    std::string read_selector() {
        auto value = cssx::read_json(root_ / "core.json").at("file").get<std::string>();
        if (!value.starts_with("cssx_core") || !value.ends_with(".dll") || !cssx::valid_id(value)) throw std::runtime_error("Invalid core file name in core.json");
        return value;
    }
    // Game thread. Construct the candidate passively, then stop the old core.
    void switch_core(const std::string& filename) {
        if (filename == loaded_) return;
        auto path = root_ / "core" / filename;
        auto candidate = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!candidate) { log_line(("Core load failed (" + std::to_string(GetLastError()) + "): " + filename).c_str()); return; }
        auto entry = reinterpret_cast<CssxGetCoreApi>(GetProcAddress(candidate, "cssx_core_api"));
        auto* api = entry ? entry() : nullptr;
        if (!api || api->abi != CSSX_CORE_ABI || api->size < sizeof(CssxCoreApi) || !api->create || !api->tick || !api->stop || !api->destroy) {
            FreeLibrary(candidate); log_line(("Core ABI rejected: " + filename).c_str()); return;
        }
        auto* instance = api->create(&host_);
        if (!instance) { FreeLibrary(candidate); log_line(("Core initialization failed: " + filename).c_str()); return; }
        if (api_ && !api_->stop(core_)) {
            if (api->stop(instance)) { api->destroy(instance); FreeLibrary(candidate); }
            log_line("Core switch cancelled: the running core could not restore its owned changes yet; retrying");
            stop_pending_ = true; return;
        }
        if (api_) api_->destroy(core_);
        if (module_) FreeLibrary(module_);
        module_ = candidate; api_ = api; core_ = instance; loaded_ = filename; stop_pending_ = false;
        log_line(("Core activated: " + loaded_ + " (" + std::string(api->version ? api->version : "?") + ")").c_str());
        cssx::atomic_json(root_ / "runtime/loader.json", {{"abi", CSSX_CORE_ABI}, {"core", loaded_}, {"core_version", api->version ? api->version : ""},
                          {"loader_version", CSSX_VERSION}, {"pid", GetCurrentProcessId()}, {"tick_ms", GetTickCount64()}}, false, false, false);
    }
public:
    Loader() {
        wchar_t path[32768]{};
        HMODULE self{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&log_line), &self);
        auto length = GetModuleFileNameW(self, path, 32768);
        if (!length || length >= 32768) throw std::runtime_error("Cannot resolve the CSSX installation");
        root_ = std::filesystem::path(path).parent_path().parent_path();   // .../Mods/CSSX/dlls/main.dll -> Mods/CSSX
        mods_root_ = root_.parent_path();
        root_string_ = root_.wstring(); mods_string_ = mods_root_.wstring();
        g_log_path = root_ / "CSSX.log";
        LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency); ring_.frequency = frequency.QuadPart;
        host_ = {CSSX_CORE_ABI, sizeof(CssxLoaderHost), root_string_.c_str(), mods_string_.c_str(), CSSX_VERSION, log_line, hooks_.api(), &ring_, this, take_hotkey};
        if (auto key = hotkey_from_settings(root_)) {
            register_keydown_event(*key, [this] { hotkey_presses_.fetch_add(1); });
            log_line("Keyboard hotkey registered with UE4SS input");
        }
        ModName = STR("CSSX");
        ModVersion = CSSX_VERSION_WIDE;
        ModDescription = STR("Custom Shell System Extensions: standalone extension platform for Mortal Shell II");
        ModAuthors = STR("_eins0fx");
        // Directory change notification instead of polling core.json. core.json
        // lives in the mod root, so that is the watched directory (non-recursive:
        // runtime/ and logs/ writes do not wake it).
        std::error_code ec; std::filesystem::create_directories(root_ / "core", ec);
        watch_ = FindFirstChangeNotificationW(root_.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
        log_line(("CSSX loader " CSSX_VERSION " created at " + cssx::path_utf8(root_) + "; engine untouched until Unreal initialization").c_str());
    }
    void on_unreal_init() override {
        hook_ = RC::Unreal::Hook::RegisterEngineTickPostCallback(
            [this, gate = gate_, alive = alive_](auto&, RC::Unreal::UEngine* engine, float delta, bool) {
                LARGE_INTEGER now; QueryPerformanceCounter(&now);
                if (last_tick_qpc_) { intervals_[ring_.head] = now.QuadPart - last_tick_qpc_; ring_.head = (ring_.head + 1) % ring_capacity; ring_.total = ring_.total + 1; }
                last_tick_qpc_ = now.QuadPart;
                std::lock_guard lock(*gate);
                if (!*alive || stopped_) return;
                hooks_.game_thread();
                try {
                    if (selector_dirty_.exchange(false) || stop_pending_) {
                        try { switch_core(read_selector()); }
                        catch (const std::exception& error) { if (observed_ != error.what()) { observed_ = error.what(); log_line(error.what()); } }
                    }
                    if (api_) api_->tick(core_, engine, delta);
                } catch (const std::exception& error) { log_line(error.what()); }
            }, {false, false, STR("CSSX"), STR("CoreDispatch")});
        log_line("CSSX game-thread dispatcher registered");
    }
    // UE4SS update thread: a zero-timeout wait on the change notification, and
    // in developer mode a frame-statistics dump every ten seconds so a
    // loader-only configuration (no core) can be measured the same way.
    uint64_t next_dump_ = 0; bool dev_checked_ = false, dev_ = false;
    void on_update() override {
        if (watch_ != INVALID_HANDLE_VALUE && WaitForSingleObject(watch_, 0) == WAIT_OBJECT_0) { selector_dirty_ = true; FindNextChangeNotification(watch_); }
        const auto now = GetTickCount64();
        if (now < next_dump_) return;
        next_dump_ = now + 10000;
        if (!dev_checked_) { dev_checked_ = true; std::error_code ec; dev_ = std::filesystem::exists(root_ / "dev/enabled.txt", ec); }
        if (!dev_ || !ring_.total) return;
        try {
            auto all = cssx::newest_of(ring_.intervals, ring_.capacity, ring_.head, ring_.total, ring_.capacity);
            std::vector<int64_t> window; int64_t budget = 20 * ring_.frequency;
            for (auto it = all.rbegin(); it != all.rend() && budget > 0; ++it) { window.push_back(*it); budget -= *it; }
            cssx::atomic_json(root_ / "runtime/frames.json", {{"core", loaded_}, {"frames_total", uint64_t(ring_.total)}, {"window_s", 20},
                {"engine", cssx::summarize(window, ring_.frequency).json()}, {"tick_ms", now}}, false, false, false);
        } catch (...) {}
    }
    ~Loader() override {
        std::unique_lock lock(*gate_);
        *alive_ = false; stopped_ = true;
        hooks_.quiesce();
        lock.unlock();
        if (hook_) RC::Unreal::Hook::UnregisterCallback(hook_);
        lock.lock();
        if (watch_ != INVALID_HANDLE_VALUE) FindCloseChangeNotification(watch_);
        // Shutdown is not guaranteed to be on the game thread: never call engine functions here.
        if (api_) api_->destroy(core_);
        log_line("CSSX loader stopped");
    }
};
}
extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod() { return new Loader(); }
extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }

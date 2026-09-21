// CSSX permanent loader: the UE4SS C++ mod at ue4ss/Mods/CSSX/dlls/main.dll.
// Owns the one engine tick callback, the frame-interval ring, the hook
// service and core selection. Everything else lives in the replaceable core.
#include "core_abi.h"
#include "hook_service.hpp"
#include "cssx_version.hpp"
#include "common.hpp"
#include <windows.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>

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
                          {"loader_version", CSSX_VERSION}, {"pid", GetCurrentProcessId()}, {"tick_ms", GetTickCount64()}}, false);
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
        host_ = {CSSX_CORE_ABI, sizeof(CssxLoaderHost), root_string_.c_str(), mods_string_.c_str(), CSSX_VERSION, log_line, hooks_.api(), &ring_};
        ModName = STR("CSSX");
        ModVersion = CSSX_VERSION_WIDE;
        ModDescription = STR("Custom Shell System Extensions: standalone extension platform for Mortal Shell II");
        ModAuthors = STR("_eins0fx");
        // Directory change notification instead of polling core.json.
        std::error_code ec; std::filesystem::create_directories(root_ / "core", ec);
        watch_ = FindFirstChangeNotificationW((root_ / "core").c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
        log_line(("CSSX loader " CSSX_VERSION " created at " + cssx::path_utf8(root_) + "; engine untouched until Unreal initialization").c_str());
    }
    void on_unreal_init() override {
        hook_ = RC::Unreal::Hook::RegisterEngineTickPostCallback(
            [this, gate = gate_, alive = alive_](auto&, RC::Unreal::UEngine* engine, float delta, bool) {
                LARGE_INTEGER now; QueryPerformanceCounter(&now);
                if (last_tick_qpc_) { intervals_[ring_.head] = now.QuadPart - last_tick_qpc_; ring_.head = (ring_.head + 1) % ring_capacity; ++ring_.total; }
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
    // UE4SS update thread: a zero-timeout wait on the change notification.
    void on_update() override {
        if (watch_ == INVALID_HANDLE_VALUE) return;
        if (WaitForSingleObject(watch_, 0) == WAIT_OBJECT_0) { selector_dirty_ = true; FindNextChangeNotification(watch_); }
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

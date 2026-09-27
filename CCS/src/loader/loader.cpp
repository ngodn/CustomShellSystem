#include "ccs_abi.h"
#include "ccs_version.hpp"
#include "hook_service.hpp"
#include "writer.hpp"
#ifdef CCS_FRAME_PROFILE
#include "frame_profile.hpp"
#endif
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <memory>
#include <stdexcept>
#include <Input/KeyDef.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <nlohmann/json.hpp>

namespace {
std::mutex g_log_mutex;
std::weak_ptr<ccs::runtime::Writer> g_log_writer;
void log_line(const char* message) noexcept {
    try {
        std::shared_ptr<ccs::runtime::Writer> writer;
        {
            std::lock_guard lock(g_log_mutex);
            writer = g_log_writer.lock();
        }
        if (writer && message) writer->write(message);
    } catch (...) {}
}

class CcsLoader final : public RC::CppUserModBase {
    std::shared_ptr<std::recursive_mutex> gate_ = std::make_shared<std::recursive_mutex>();
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::shared_ptr<std::atomic<bool>> hotkey_ = std::make_shared<std::atomic<bool>>(false);
    RC::Unreal::Hook::GlobalCallbackId hook_{};
    std::filesystem::path root_;
    CcsLoaderContext host_{};
    HMODULE module_{};
    const CcsCoreApi* api_{};
    bool dispatch_failed_{};
    std::string core_name_;                 // the loaded core file, from core.json
    uint64_t selector_check_{}, selector_stamp_{};
    int switch_attempts_{};
    std::shared_ptr<ccs::runtime::Writer> log_writer_;
    std::unique_ptr<CcsHookService> hooks_;
#ifdef CCS_FRAME_PROFILE
    std::unique_ptr<ccs::runtime::FrameProfile> timing_;
#endif

    void load_core() {
        const auto selector_path = root_ / "core.json";
        if (std::filesystem::file_size(selector_path) > 4096) throw std::runtime_error("CCS selector exceeds bound");
        std::ifstream input(selector_path);
        const auto selector = nlohmann::json::parse(input);
        const auto name = selector.at("file").get<std::string>();
        if (name.size() > 128 || !name.starts_with("ccs_core") || !name.ends_with(".dll") ||
            name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") != std::string::npos)
            throw std::runtime_error("Invalid CCS core selector");
        const auto path = root_ / "core" / name;
        core_name_ = name;
        module_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module_) throw std::runtime_error("CCS core DLL could not load: " + std::to_string(GetLastError()));
        const auto entry = reinterpret_cast<const CcsCoreApi* (*)()>(GetProcAddress(module_, "ccs_get_core_api"));
        api_ = entry ? entry() : nullptr;
        if (!api_ || api_->abi_version != CCS_ABI_VERSION || api_->size < sizeof(CcsCoreApi) ||
            !api_->init || !api_->tick || !api_->on_hotkey || !api_->stop || !api_->shutdown || !api_->get_status_json)
            throw std::runtime_error("CCS core ABI rejected");
        if (api_->init(&host_) != 0) throw std::runtime_error("CCS core initialization failed");
        log_line(("CCS core loaded: " + name).c_str());
    }
    // core.json is the developer's live switch: when its selection changes while the game runs,
    // the current core is stopped on the game thread (it must be quiescent: hooks removed, menu
    // detached), shut down, unloaded, and the new file loaded in its place. Players never touch it.
    // The check is one file-time read per second; nothing is parsed until the stamp moves.
    void poll_core_selector(uint64_t now) {
        if (now < selector_check_) return;
        selector_check_ = now + 1000;
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW((root_ / "core.json").c_str(), GetFileExInfoStandard, &data)) return;
        const uint64_t stamp = (uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
        if (stamp == selector_stamp_) return;
        std::string wanted;
        try {
            std::ifstream input(root_ / "core.json");
            wanted = nlohmann::json::parse(input).at("file").get<std::string>();
        } catch (...) { selector_stamp_ = stamp; return; }
        if (wanted == core_name_) { selector_stamp_ = stamp; switch_attempts_ = 0; return; }
        if (api_ && api_->stop() == 0) {
            if (++switch_attempts_ <= 20) { selector_check_ = now + 250; return; }   // retry while the core drains
            log_line("CCS core switch abandoned: the running core would not stop");
            selector_stamp_ = stamp; switch_attempts_ = 0; return;
        }
        log_line(("CCS core switching to " + wanted).c_str());
        if (api_) { api_->shutdown(); api_ = nullptr; }
        if (module_) { FreeLibrary(module_); module_ = nullptr; }
        selector_stamp_ = stamp; switch_attempts_ = 0; dispatch_failed_ = false;
        try { load_core(); }
        catch (const std::exception& error) { api_ = nullptr; if (module_) { FreeLibrary(module_); module_ = nullptr; } log_line(error.what()); }
        catch (...) { api_ = nullptr; if (module_) { FreeLibrary(module_); module_ = nullptr; } log_line("CCS core load failed"); }
    }
public:
    CcsLoader() {
        ModName = STR("CCS");
        ModVersion = CCS_VERSION_WIDE;
        ModDescription = STR("Custom Combat System prototype for Mortal Shell II");
        ModAuthors = STR("eins0fx");
        wchar_t path[32768]{};
        HMODULE self{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&log_line), &self))
            throw std::runtime_error("Cannot resolve CCS loader module");
        const auto length = GetModuleFileNameW(self, path, 32768);
        if (!length || length >= 32768) throw std::runtime_error("Cannot resolve CCS installation");
        root_ = std::filesystem::path(path).parent_path().parent_path();
        log_writer_ = std::make_shared<ccs::runtime::Writer>(root_ / "CCS.log");
        {
            std::lock_guard lock(g_log_mutex);
            g_log_writer = log_writer_;
        }
        host_.abi_version = CCS_ABI_VERSION;
        host_.size = sizeof(CcsLoaderContext);
        host_.mod_root = root_.c_str();
        host_.log_info = log_line;
        host_.log_warn = log_line;
        host_.log_error = log_line;
#ifdef CCS_FRAME_PROFILE
        register_keydown_event(RC::Input::Key::F7, [pressed = hotkey_] { pressed->store(true); });
#endif
        log_line("CCS loader created; engine untouched until first Unreal tick");
    }
    void on_unreal_init() override {
        if (hook_) return;
        // Core construction reads metadata only. Keep startup disk access and JSON decoding outside the tick callback.
        try {
            hooks_ = std::make_unique<CcsHookService>(gate_);
            host_.hooks = hooks_->api();
#ifdef CCS_FRAME_PROFILE
            timing_ = std::make_unique<ccs::runtime::FrameProfile>(root_ / "runtime/timing", "loader");
#endif
            load_core();
        }
        catch (const std::exception& error) {
            api_ = nullptr;
            if (module_) { FreeLibrary(module_); module_ = nullptr; }
            log_line(error.what());
        }
        catch (...) {
            api_ = nullptr;
            if (module_) { FreeLibrary(module_); module_ = nullptr; }
            log_line("CCS core load failed");
        }
        hook_ = RC::Unreal::Hook::RegisterEngineTickPostCallback(
            [this, gate = gate_, alive = alive_](auto&, RC::Unreal::UEngine* engine, float delta, bool) {
#ifdef CCS_FRAME_PROFILE
                const auto entered = ccs::runtime::FrameProfile::now_ns();
#endif
                std::lock_guard lock(*gate);
#ifdef CCS_FRAME_PROFILE
                const auto locked = ccs::runtime::FrameProfile::now_ns();
#endif
                if (!*alive) return;
                try {
                    if (!hooks_->game_thread()) return;
                    poll_core_selector(GetTickCount64());
                    if (!api_ || dispatch_failed_) return;
                    const bool pressed = hotkey_->exchange(false);
#ifdef CCS_FRAME_PROFILE
                    if (pressed) timing_->arm(entered);
                    ccs::runtime::FrameProfile::Scope frame(*timing_, delta, entered);
                    timing_->record(ccs::runtime::FrameProfile::Phase::GateWait, locked - entered);
#endif
                    if (pressed) api_->on_hotkey(VK_F7);
                    CcsPlayerContext context{};
                    context.engine = engine;
                    api_->tick(&context, static_cast<double>(delta));
                } catch (const std::exception& error) {
                    dispatch_failed_ = true;
                    log_line(error.what());
                } catch (...) {
                    dispatch_failed_ = true;
                    log_line("CCS dispatch disabled after tick failure");
                }
            }, {false, false, STR("CCS"), STR("CoreDispatch")});
    }
    ~CcsLoader() override {
        {
            std::lock_guard lock(*gate_);
            *alive_ = false;
        }
        if (hook_) RC::Unreal::Hook::UnregisterCallback(hook_);
        std::lock_guard lock(*gate_);
        // This thread is not guaranteed to be the game thread. shutdown releases CPU resources only.
        const bool quiet = !hooks_ || hooks_->quiesce();
        if (quiet) {
            if (api_) api_->shutdown();
            if (module_) FreeLibrary(module_);
        } else {
            log_line("CCS core retained because a callback is still active or quiescence failed");
        }
    }
};
}
extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod() {
    try { return new CcsLoader(); } catch (...) { return nullptr; }
}
extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }

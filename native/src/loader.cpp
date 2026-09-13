#include "api.hpp"
#include "data.hpp"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>

namespace {
std::filesystem::path log_path;
void log_line(const char* message) noexcept {
    try {
        std::ofstream out(log_path, std::ios::app);
        out << GetTickCount64() << " " << message << '\n';
        out.flush();
    } catch (...) {}
}
class Loader final : public RC::CppUserModBase {
    std::recursive_mutex gate_;
    std::filesystem::path root_;
    std::wstring root_string_;
    CssHost host_{};
    HMODULE module_{};
    const CssCore* api_{};
    void* core_{};
    RC::Unreal::Hook::GlobalCallbackId hook_{};
    std::string pending_, loaded_, observed_;
    uint64_t next_poll_{};
    bool stopped_{};

    void reload() {
        auto filename = std::exchange(pending_, {});
        if (filename == loaded_) return;
        auto candidate = LoadLibraryW((root_ / "cores" / filename).c_str());
        if (!candidate) { log_line("Core load failed; current core retained"); return; }
        auto entry = reinterpret_cast<CssGetApi>(GetProcAddress(candidate, "css_get_api"));
        auto* api = entry ? entry() : nullptr;
        if (!api || api->abi != css_abi || !api->create || !api->tick || !api->render || !api->stop || !api->destroy) {
            FreeLibrary(candidate); log_line("Core ABI rejected; current core retained"); return;
        }
        if (api_ && !api_->stop(core_)) {
            FreeLibrary(candidate);
            log_line("Reload cancelled because old core could not restore its appearance"); return;
        }
        auto* instance = api->create(&host_);
        if (!instance) { FreeLibrary(candidate); log_line("Core initialization failed; current core retained"); return; }
        if (api_) api_->destroy(core_);
        if (module_) FreeLibrary(module_);
        module_ = candidate; api_ = api; core_ = instance; loaded_ = filename;
        log_line(("Core activated: " + loaded_).c_str());
        css::atomic_json(root_ / "runtime/loader.json", {{"abi", css_abi}, {"core", loaded_},
                         {"pid", GetCurrentProcessId()}, {"tick_ms", GetTickCount64()}}, false);
    }
public:
    Loader() {
        wchar_t path[32768]{};
        HMODULE self{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&log_line), &self);
        auto length = GetModuleFileNameW(self, path, 32768);
        if (!length || length >= 32768) throw std::runtime_error("Cannot resolve CSS installation");
        root_ = std::filesystem::path(path).parent_path().parent_path();
        root_string_ = root_.wstring();
        log_path = root_ / "CSS.log";
        host_ = {css_abi, root_string_.c_str(), log_line};
        ModName = STR("CSS - Custom Shell System");
        ModVersion = STR("0.1.0-dev");
        ModDescription = STR("Native shell appearance system with reloadable C++ core");
        ModAuthors = STR("eins0fx");
        register_tab(STR("Custom Shell System"), [](RC::CppUserModBase* mod) {
            auto* self = static_cast<Loader*>(mod);
            std::lock_guard lock(self->gate_);
            if (self->api_ && !self->stopped_) self->api_->render(self->core_);
        });
        log_line("CSS loader created; engine untouched until Unreal initialization");
    }
    void on_unreal_init() override {
        hook_ = RC::Unreal::Hook::RegisterEngineTickPostCallback(
            [this](auto&, RC::Unreal::UEngine* engine, float delta, bool) {
                std::lock_guard lock(gate_);
                if (stopped_) return;
                try {
                    if (!pending_.empty()) reload();
                    if (api_) api_->tick(core_, engine, delta);
                } catch (const std::exception& error) { log_line(error.what()); }
            }, {false, false, STR("CustomShellSystem"), STR("CoreDispatch")});
        log_line("CSS game-thread dispatcher registered");
    }
    void on_update() override {
        auto now = GetTickCount64();
        if (now < next_poll_) return;
        next_poll_ = now + 250;
        std::unique_lock lock(gate_, std::try_to_lock);
        if (!lock || stopped_) return;
        try {
            auto filename = css::read_json(root_ / "core.json").at("file").get<std::string>();
            if (filename == observed_) return;
            observed_ = filename;
            if (!filename.starts_with("css_core-") || !filename.ends_with(".dll") || !css::valid_id(filename))
                throw std::runtime_error("Invalid CSS core filename");
            pending_ = filename;
        } catch (const std::exception& error) {
            // Only report a missing startup core once, not on every poll.
            if (observed_ != "error") { observed_ = "error"; log_line(error.what()); }
        }
    }
    ~Loader() override {
        std::lock_guard lock(gate_);
        stopped_ = true;
        if (hook_) RC::Unreal::Hook::UnregisterCallback(hook_);
        // UE4SS shutdown is not guaranteed to be on the game thread. Never call
        // engine functions here. The OS releases modules on process exit.
        if (api_) api_->destroy(core_);
        log_line("CSS loader stopped");
    }
};
}
extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod() {
    return new Loader();
}
extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) {
    delete mod;
}

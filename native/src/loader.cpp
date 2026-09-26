#include "api.hpp"
#include "hook_host.hpp"
#include "css_version.hpp"
#include "data.hpp"
#include "file_writer.hpp"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <optional>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>

namespace {
std::filesystem::path log_path;
std::mutex log_mutex;
std::ofstream log_file;
// The log stays open for the session (one write per line, not an open/close), and a log that
// grew past 2 MB is rotated to CSS.log.1 when the game starts, so it can never grow unbounded.
void log_line(const char* message) noexcept {
    try {
        std::lock_guard lock(log_mutex);
        if (!log_file.is_open()) {
            std::error_code error;
            if (std::filesystem::file_size(log_path, error) > 2 * 1024 * 1024 && !error) {
                auto previous = log_path; previous += ".1";
                std::filesystem::rename(log_path, previous, error);
            }
            log_file.open(log_path, std::ios::app);
        }
        log_file << GetTickCount64() << " " << message << '\n';
        log_file.flush();
    } catch (...) {}
}
// The writer thread the core hands its JSON to (host ABI 2). Namespace scope so the plain
// function pointer in CssHost can reach it; it drains and joins when the loader stops.
std::optional<css::FileWriter> file_writer;
void write_file(const wchar_t* path, const char* data, size_t size, uint32_t flags) noexcept {
    try { if (file_writer && path && data) file_writer->post(std::filesystem::path(path), std::string(data, size), (flags & 1u) != 0); }
    catch (...) {}
}
class Loader final : public RC::CppUserModBase {
    std::shared_ptr<std::recursive_mutex> gate_=std::make_shared<std::recursive_mutex>();
    std::shared_ptr<bool> alive_=std::make_shared<bool>(true);
    CssHookService hook_service_{gate_};
    std::filesystem::path root_;
    std::wstring root_string_;
    CssHost host_{};      // host ABI 2, for a core that exports css_get_api2
    CssHost host_v1_{};   // host ABI 1, the first three fields, for an older core
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
        auto entry2 = reinterpret_cast<CssGetApi>(GetProcAddress(candidate, "css_get_api2"));
        auto* api = entry2 ? entry2() : entry ? entry() : nullptr;
        const CssHost* host = entry2 ? &host_ : &host_v1_;
        if (!api || api->abi != css_abi || !api->create || !api->tick || !api->render || !api->stop || !api->destroy) {
            FreeLibrary(candidate); log_line("Core ABI rejected; current core retained"); return;
        }
        // Core construction is passive. Validate the new instance before stopping
        // the working one, so a failed constructor cannot leave CSS stopped.
        auto* instance = api->create(host);
        if (!instance) { FreeLibrary(candidate); log_line("Core initialization failed; current core retained"); return; }
        if (api_ && !api_->stop(core_)) {
            if(api->stop(instance)) {api->destroy(instance);FreeLibrary(candidate);}
            log_line("Reload cancelled because the current core could not restore its owned changes"); return;
        }
        if (api_) api_->destroy(core_);
        if (module_) FreeLibrary(module_);
        module_ = candidate; api_ = api; core_ = instance; loaded_ = filename;
        log_line(("Core activated: " + loaded_).c_str());
        try {
            css::write_runtime_json(root_ / "runtime/loader.json", {{"abi", css_abi}, {"core", loaded_},
                                    {"pid", GetCurrentProcessId()}, {"tick_ms", GetTickCount64()}});
        } catch (const std::exception& error) { log_line(error.what()); }
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
        file_writer.emplace([](const std::string& message) { log_line(message.c_str()); });
        host_ = {css_host_abi, root_string_.c_str(), log_line, write_file};
        host_v1_ = {css_abi, root_string_.c_str(), log_line, nullptr};
        ModName = STR("CSS - Custom Shell System");
        ModVersion = CSS_VERSION_WIDE;
        ModDescription = STR("Native shell appearance system with reloadable C++ core");
        ModAuthors = STR("_eins0fx");
        register_tab(STR("Custom Shell System"), [](RC::CppUserModBase* mod) {
            auto* self = static_cast<Loader*>(mod);
            std::lock_guard lock(*self->gate_);
            if (self->api_ && !self->stopped_) self->api_->render(self->core_);
        });
        log_line("CSS loader created; engine untouched until Unreal initialization");
    }
    void on_unreal_init() override {
        hook_ = RC::Unreal::Hook::RegisterEngineTickPostCallback(
            [this,gate=gate_,alive=alive_](auto&, RC::Unreal::UEngine* engine, float delta, bool) {
                std::lock_guard lock(*gate);
                if(!*alive) return;
                if (stopped_) return;
                hook_service_.game_thread();
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
        // Read core.json before taking the gate. The game thread takes that gate every tick
        // and must never wait on this disk read.
        std::string filename, failure;
        try { filename = css::read_json(root_ / "core.json").at("file").get<std::string>(); }
        catch (const std::exception& error) { failure = error.what(); }
        std::unique_lock lock(*gate_, std::try_to_lock);
        if (!lock || stopped_) return;
        try {
            if (!failure.empty()) throw std::runtime_error(failure);
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
        std::unique_lock lock(*gate_);
        *alive_=false;
        stopped_ = true;
        hook_service_.quiesce();
        // Unregister may wait for a captured callback that is waiting on gate.
        // The shared liveness token makes that callback safe after this dies.
        lock.unlock();
        if (hook_) RC::Unreal::Hook::UnregisterCallback(hook_);
        lock.lock();
        // UE4SS shutdown is not guaranteed to be on the game thread. Never call
        // engine functions here. The OS releases modules on process exit.
        if (api_) api_->destroy(core_);
        file_writer.reset();   // writes what is still queued, then joins
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

#define CCS_BUILD_CORE
#include "core.hpp"
#include "common.hpp"
#include <windows.h>

namespace ccs {
static std::unique_ptr<Core> g_core;

Core::Core(const CcsLoaderContext* loader) {
    if (!loader || loader->abi_version != CCS_ABI_VERSION || loader->size < sizeof(CcsLoaderContext) || !loader->mod_root)
        throw std::runtime_error("CCS loader context is incompatible");
    root_dir_ = loader->mod_root;
    if (loader->hooks && loader->hooks->version == CCS_HOOK_ABI_VERSION && loader->hooks->size >= sizeof(CcsHookHost) &&
        loader->hooks->add_native_pre && loader->hooks->remove && loader->hooks->statistics && loader->hooks->on_game_thread) hooks_ = loader->hooks;
    writer_ = std::make_unique<runtime::Writer>(root_dir_ / "logs/ccs.jsonl");
#ifdef CCS_FRAME_PROFILE
    timing_ = std::make_unique<runtime::FrameProfile>(root_dir_ / "runtime/timing", "core");
#endif
#ifdef CCS_DISCOVERY_PROBE
    probe_ = std::make_unique<DiscoveryProbe>(root_dir_ / "logs/discovery.jsonl");
#elif defined(CCS_ATTACK_PROBE)
    attack_probe_ = std::make_unique<AttackProbe>(hooks_, root_dir_ / "logs/attack-calls.jsonl");
#else
    settings_ = std::make_unique<runtime::Settings>(root_dir_ / "settings.json");
    if (!settings_->load()) writer_->write(R"({"level":"warn","msg":"Settings rejected; using disabled defaults"})");
    storage_ = std::make_unique<runtime::Storage>(root_dir_ / "presets");
    movesets_ = std::make_unique<MovesetManager>();
    if (auto preset = storage_->load_preset(settings_->startup_preset())) movesets_->set_active_preset(std::move(*preset));
#ifdef CCS_EXPERIMENTAL_MENU
    std::array<wchar_t, 32768> executable{};
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) throw std::runtime_error("Game executable path is unavailable or truncated");
    content_monitor_ = std::make_unique<runtime::ContentMonitor>(std::filesystem::path(executable.data(), executable.data() + length));
    persistence_ = std::make_unique<runtime::Persistence>(root_dir_);
    Menu::Deps deps;
    deps.settings = settings_.get();
    deps.persistence = persistence_.get();
    deps.movesets = movesets_.get();
    deps.loaded_moves = &loaded_moves_;
    auto present = [&](const char* name) {
        const auto mod = root_dir_.parent_path() / name;
        return std::filesystem::exists(mod / "enabled.txt") && std::filesystem::exists(mod / "dlls/main.dll");
    };
    deps.css_present = present("CustomShellSystem");
    deps.cssx_present = present("CSSX");
    deps.log = [this](const std::string& message) {
        writer_->write(nlohmann::json{{"level", "info"}, {"msg", message}}.dump());
    };
    menu_ = std::make_unique<Menu>(std::move(deps));
#endif
#endif
    initialized_ = true;
    writer_->write(R"({"level":"info","msg":"CCS foundation initialized; combat routing is unavailable"})");
}
Core::~Core() { shutdown(); }

void Core::tick(const CcsPlayerContext* player, double delta) {
    if (!initialized_) return;
    if (error_reported_) {
        const auto now = runtime::now_ms();
        if (!fault_cleanup_complete_ && now >= fault_cleanup_after_) {
            fault_cleanup_after_ = now + 250;
            fault_cleanup_complete_ = stop();
        }
        return;
    }
#ifdef CCS_FRAME_PROFILE
    runtime::FrameProfile::Scope frame(*timing_, delta);
#endif
#ifdef CCS_DISCOVERY_PROBE
#ifdef CCS_FRAME_PROFILE
    timing_->measure(runtime::FrameProfile::Phase::Probe, [&] {
#endif
    probe_->tick(player ? player->engine : nullptr);
#ifdef CCS_FRAME_PROFILE
    });
#endif
    (void)delta;
#elif defined(CCS_ATTACK_PROBE)
#ifdef CCS_FRAME_PROFILE
    timing_->measure(runtime::FrameProfile::Phase::Probe, [&] {
#endif
    attack_probe_->tick(player ? player->engine : nullptr);
#ifdef CCS_FRAME_PROFILE
    });
#endif
    (void)delta;
#elif defined(CCS_EXPERIMENTAL_MENU)
    engine::PlayerContext context;
#ifdef CCS_FRAME_PROFILE
    timing_->measure(runtime::FrameProfile::Phase::Context, [&] {
#endif
    context = engine::player_context(player ? player->engine : nullptr);
#ifdef CCS_FRAME_PROFILE
    });
#endif
    if (menu_) {
#ifdef CCS_FRAME_PROFILE
        timing_->measure(runtime::FrameProfile::Phase::Menu, [&] {
#endif
        if (open_requested_) {
            open_requested_ = false;
            if (menu_->is_open()) menu_->close();
            else menu_->open(context);
        }
        menu_->tick(context, delta);
#ifdef CCS_FRAME_PROFILE
        });
        timing_->measure(runtime::FrameProfile::Phase::Discovery, [&] {
#endif
        const auto now = GetTickCount64();
        update_content(now, menu_->is_open());
        if (!content_monitor_->stopped() && content_observation_ && content_observation_->available &&
            !content_observation_->restart_required)
            loaded_moves_.tick(context, menu_->is_open(), now);
#ifdef CCS_FRAME_PROFILE
        });
#endif
        const auto observed = loaded_moves_.snapshot();
        const auto revision = observed ? observed->revision : 0;
        if (loaded_state_ != loaded_moves_.state() || loaded_revision_ != revision) {
            loaded_state_ = loaded_moves_.state(); loaded_revision_ = revision;
            menu_->invalidate();
        }
    }
#else
    (void)player;
    (void)delta;
#endif
}
#ifdef CCS_EXPERIMENTAL_MENU
void Core::update_content(uint64_t now, bool visible) {
    if (now >= content_poll_after_) {
        content_poll_after_ = now + 250;
        if (const auto result = content_monitor_->poll(); result &&
            (!content_observation_ || result->sequence != content_observation_->sequence)) {
            content_observation_ = result;
            if (!result->available || result->restart_required) loaded_moves_.reset();
        }
    }
    if (content_monitor_->stopped())
        loaded_moves_.suspend("content_unavailable", "Content inspection worker stopped");
    else if (!content_observation_)
        loaded_moves_.suspend("content_pending", "Waiting for installation inspection");
    else if (content_observation_->restart_required)
        loaded_moves_.suspend("content_changed", "Game files changed; restart before discovering moves");
    else if (!content_observation_->available)
        loaded_moves_.suspend("content_unavailable", content_observation_->error);
    if (visible && now >= content_request_after_ && !content_monitor_->stopped() &&
        (!content_observation_ || !content_observation_->restart_required))
        content_request_after_ = now + (content_monitor_->request() ? 60000 : 250);
}
#endif
void Core::on_hotkey(uint32_t key) {
    if (error_reported_) return;
#ifdef CCS_FRAME_PROFILE
    if (key == VK_F7) timing_->arm(runtime::FrameProfile::now_ns());
#endif
#ifdef CCS_DISCOVERY_PROBE
    if (key == VK_F7) probe_->toggle();
#elif defined(CCS_ATTACK_PROBE)
    if (key == VK_F7) attack_probe_->toggle();
#else
    if (key == VK_F7 && menu_) open_requested_ = true;
#endif
}
bool Core::stop() {
    if (hooks_ && !hooks_->on_game_thread(hooks_->context)) return false;
#ifdef CCS_EXPERIMENTAL_MENU
    if (content_monitor_) {
        content_monitor_->close();
        if (!content_monitor_->stopped()) return false;
    }
#endif
#ifdef CCS_DISCOVERY_PROBE
    probe_->cancel();
#endif
#ifdef CCS_ATTACK_PROBE
    if (!attack_probe_->stop()) return false;
#endif
    if (menu_) menu_->detach();
#ifdef CCS_EXPERIMENTAL_MENU
    loaded_moves_.reset();
#endif
    if (movesets_) movesets_->restore_vanilla({});
    if (hooks_) {
        CcsHookStats stats{}; stats.size = sizeof(stats);
        if (!hooks_->statistics(hooks_->context, &stats) || stats.slots || stats.running) return false;
    }
    return true;
}
void Core::report_error(const char* message) noexcept {
    if (error_reported_) return;
    error_reported_ = true;
    try {
        if (writer_) writer_->write(nlohmann::json{{"level", "error"}, {"msg", message}}.dump());
    } catch (...) {}
}
void Core::shutdown() {
    // Loader destruction can run off the game thread. Release CPU resources only.
    initialized_ = false;
    menu_.reset();
    if (persistence_ && settings_) {
        try {
            if (!persistence_->submit({runtime::FileOperation::SaveSettings, {}, settings_->to_json().dump()}))
                report_error("Final settings snapshot could not be queued");
        } catch (...) { report_error("Final settings snapshot failed"); }
    }
    persistence_.reset();
#ifdef CCS_EXPERIMENTAL_MENU
    content_monitor_.reset(); content_observation_.reset();
#endif
#ifdef CCS_DISCOVERY_PROBE
    probe_.reset();
#endif
#ifdef CCS_ATTACK_PROBE
    attack_probe_.reset();
#endif
    writer_.reset();
#ifdef CCS_FRAME_PROFILE
    timing_.reset();
#endif
}
const char* Core::get_status_json() {
    status_json_ = nlohmann::json{{"status", error_reported_ ? "faulted" : "prototype"}, {"combat_available", false},
        {"fault_cleanup_complete", fault_cleanup_complete_},
        {"enabled_requested", settings_ && settings_->enabled()}, {"catalog_moves", movesets_ ? movesets_->get_all_moves().size() : 0},
        {"menu_available", menu_ != nullptr}, {"menu_file_status", menu_ ? menu_->file_status() : ""},
        {"menu_input_error", menu_ ? menu_->input_error() : ""}}.dump();
    if (hooks_) {
        CcsHookStats stats{}; stats.size = sizeof(stats);
        if (hooks_->statistics(hooks_->context, &stats)) {
            auto status = nlohmann::json::parse(status_json_);
            status["hooks"] = {{"slots", stats.slots}, {"running", stats.running}, {"stopped", stats.stopped != 0},
                {"calls", stats.calls}, {"wrong_thread", stats.wrong_thread}, {"failures", stats.failures}};
            status_json_ = status.dump();
        }
    }
#ifdef CCS_FRAME_PROFILE
    auto timing_status = nlohmann::json::parse(status_json_);
    timing_status["timing"] = {{"active", timing_ && timing_->active()},
        {"completed", timing_ ? timing_->completed() : 0}, {"failures", timing_ ? timing_->failures() : 0}};
    status_json_ = timing_status.dump();
#endif
#ifdef CCS_DISCOVERY_PROBE
    auto status = nlohmann::json::parse(status_json_);
    status["discovery_state"] = probe_ ? probe_->state() : "uninitialized";
    status_json_ = status.dump();
#endif
#ifdef CCS_ATTACK_PROBE
    auto status = nlohmann::json::parse(status_json_);
    status["attack_probe"] = attack_probe_->status();
    status_json_ = status.dump();
#endif
#ifdef CCS_EXPERIMENTAL_MENU
    auto status = nlohmann::json::parse(status_json_);
    status["content_inspection"] = {{"state", content_monitor_->stopped() ? "stopped" :
        !content_observation_ ? "pending" : content_observation_->restart_required ? "restart_required" :
        content_observation_->available ? "observed" : "unavailable"},
        {"scope", "disk_metadata_change_detector"}, {"mounted_content_verified", false},
        {"sequence", content_observation_ ? content_observation_->sequence : 0},
        {"files", content_observation_ && content_observation_->manifest ? content_observation_->manifest->files.size() : 0},
        {"elapsed_us", content_observation_ ? content_observation_->elapsed_us : 0},
        {"background_priority", content_observation_ && content_observation_->background_priority},
        {"error", content_observation_ ? content_observation_->error : ""}};
    const auto observed = loaded_moves_.snapshot();
    status["loaded_moves"] = {{"state", loaded_moves_.state()}, {"error", loaded_moves_.error()},
        {"candidates", observed ? observed->candidates.size() : 0},
        {"revision", observed ? observed->revision : 0},
        {"observed_at_ms", observed ? observed->observed_at_ms : 0},
        {"maximum_step_us", loaded_moves_.maximum_step_us()}, {"combat_eligible", 0}};
    status_json_ = status.dump();
#endif
    return status_json_.c_str();
}
}

extern "C" {
static int ccs_core_init(const CcsLoaderContext* loader) noexcept {
    try {
        if (ccs::g_core) return -1;
        ccs::g_core = std::make_unique<ccs::Core>(loader);
        return 0;
    } catch (...) { return -1; }
}
static void ccs_core_tick(const CcsPlayerContext* player, double delta) noexcept {
    try { if (ccs::g_core) ccs::g_core->tick(player, delta); }
    catch (const std::exception& error) { if (ccs::g_core) ccs::g_core->report_error(error.what()); }
    catch (...) { if (ccs::g_core) ccs::g_core->report_error("Unknown CCS tick failure"); }
}
static void ccs_core_on_hotkey(uint32_t key) noexcept {
    try { if (ccs::g_core) ccs::g_core->on_hotkey(key); }
    catch (const std::exception& error) { if (ccs::g_core) ccs::g_core->report_error(error.what()); }
    catch (...) { if (ccs::g_core) ccs::g_core->report_error("Unknown CCS hotkey failure"); }
}
static int ccs_core_stop() noexcept {
    try { return !ccs::g_core || ccs::g_core->stop() ? 1 : 0; } catch (...) { return 0; }
}
static void ccs_core_shutdown() noexcept {
    try { ccs::g_core.reset(); } catch (...) {}
}
static const char* ccs_core_status() noexcept {
    try { return ccs::g_core ? ccs::g_core->get_status_json() : R"({"status":"uninitialized"})"; }
    catch (...) { return R"({"status":"error"})"; }
}
static const CcsCoreApi g_api = {
    CCS_ABI_VERSION, sizeof(CcsCoreApi), ccs_core_init, ccs_core_tick,
    ccs_core_on_hotkey, ccs_core_stop, ccs_core_shutdown, ccs_core_status
};
CCS_CORE_EXPORT const CcsCoreApi* ccs_get_core_api(void) { return &g_api; }
}

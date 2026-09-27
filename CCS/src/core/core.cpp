#define CCS_BUILD_CORE
#include "core.hpp"
#include "common.hpp"
#include <windows.h>

namespace ccs {

using RC::Unreal::UObject;

static std::unique_ptr<Core> g_core;

Core::Core(const CcsLoaderContext* loader) {
    if (loader && loader->mod_root) {
        root_dir_ = loader->mod_root;
    } else {
        root_dir_ = "ue4ss/Mods/CCS";
    }

    writer_ = std::make_unique<runtime::Writer>(root_dir_ / "logs/ccs.jsonl");
    writer_->write(R"({"level":"info","msg":"Custom Combat System Core initializing"})");

    settings_ = std::make_unique<runtime::Settings>(root_dir_ / "settings.json");
    settings_->load();

    storage_ = std::make_unique<runtime::Storage>(root_dir_ / "presets");

    movesets_ = std::make_unique<MovesetManager>();

    // Load startup preset if available
    auto default_preset = storage_->load_preset(settings_->startup_preset());
    if (default_preset) {
        movesets_->set_active_preset(*default_preset);
    }

    Menu::Deps menu_deps;
    menu_deps.settings = settings_.get();
    menu_deps.storage = storage_.get();
    menu_deps.movesets = movesets_.get();
    menu_deps.log = [this](const std::string& msg) {
        if (writer_) writer_->write(R"({"level":"info","msg":")" + msg + R"("})");
    };

    menu_ = std::make_unique<Menu>(menu_deps);
    initialized_ = true;
    writer_->write(R"({"level":"info","msg":"Custom Combat System Core initialized successfully"})");
}

Core::~Core() {
    shutdown();
}

void Core::tick(const CcsPlayerContext* player_ctx, double delta_seconds) {
    if (!initialized_) return;

    engine::PlayerContext ctx;
    if (player_ctx) {
        ctx.pc = static_cast<UObject*>(player_ctx->player_controller);
        ctx.pawn = static_cast<UObject*>(player_ctx->player_character);
        ctx.weapon = static_cast<UObject*>(player_ctx->weapon_actor);
        ctx.asc = static_cast<UObject*>(player_ctx->ability_system_comp);
    }

    // Apply movesets if mod is enabled
    if (settings_ && settings_->enabled()) {
        if (movesets_) movesets_->apply_to_player(ctx);
    } else {
        if (movesets_) movesets_->restore_vanilla(ctx);
    }

    if (menu_) {
        menu_->tick(ctx, delta_seconds);
    }
}

void Core::on_hotkey(uint32_t key_code) {
    if (!menu_) return;

    // F7 hotkey or custom chords
    if (key_code == VK_F7) {
        // Toggle menu or invalidate
        menu_->invalidate();
    }
}

void Core::shutdown() {
    if (!initialized_) return;
    if (menu_) {
        menu_->detach();
        menu_.reset();
    }
    if (writer_) {
        writer_->write(R"({"level":"info","msg":"Custom Combat System Core shutdown"})");
        writer_->flush();
    }
    initialized_ = false;
}

const char* Core::get_status_json() {
    status_json_ = "{\"status\":\"ok\",\"enabled\":" + std::string(settings_ && settings_->enabled() ? "true" : "false") + "}";
    return status_json_.c_str();
}

} // namespace ccs

// -------------------------------------------------------------
// Exported C ABI for Loader
// -------------------------------------------------------------
extern "C" {

static int ccs_core_init(const CcsLoaderContext* loader) {
    try {
        ccs::g_core = std::make_unique<ccs::Core>(loader);
        return 0;
    } catch (...) {
        return -1;
    }
}

static void ccs_core_tick(const CcsPlayerContext* player, double delta_seconds) {
    if (ccs::g_core) ccs::g_core->tick(player, delta_seconds);
}

static void ccs_core_on_hotkey(uint32_t key_code) {
    if (ccs::g_core) ccs::g_core->on_hotkey(key_code);
}

static void ccs_core_shutdown(void) {
    if (ccs::g_core) {
        ccs::g_core->shutdown();
        ccs::g_core.reset();
    }
}

static const char* ccs_core_status(void) {
    return ccs::g_core ? ccs::g_core->get_status_json() : "{\"status\":\"uninitialized\"}";
}

static const CcsCoreApi g_api = {
    CCS_ABI_VERSION,
    ccs_core_init,
    ccs_core_tick,
    ccs_core_on_hotkey,
    ccs_core_shutdown,
    ccs_core_status
};

CCS_CORE_EXPORT const CcsCoreApi* ccs_get_core_api(void) {
    return &g_api;
}

}

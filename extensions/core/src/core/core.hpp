#pragma once
// The replaceable CSSX runtime. One instance per loaded core DLL. Owns the
// extension runtime, the bridge, the HUD service, the menu, settings and the
// developer request channel. All engine work happens in tick() on the game
// thread, driven by the loader.
#include "core_abi.h"
#include "engine.hpp"
#include "bridge.hpp"
#include "hud.hpp"
#include "menu.hpp"
#include "devchannel.hpp"
#include "extensions.hpp"
#include "settings.hpp"
#include "storage.hpp"
#include "frame_stats.hpp"
#include <memory>

namespace cssx {
class Core {
public:
    explicit Core(const CssxLoaderHost& host);
    void tick(void* engine,float delta);
    bool stop();
    Json status();
private:
    const CssxLoaderHost host_;
    fs::path root_, mods_root_;
    Storage storage_;
    Settings settings_;
    std::unique_ptr<Bridge> bridge_;
    HudService hud_;
    std::unique_ptr<Runtime> runtime_;
    std::unique_ptr<Menu> menu_;
    std::unique_ptr<DevChannel> dev_;
    engine::PlayerContext player_{};
    void* engine_=nullptr;
    bool started_=false, stopped_=false, idle_=false, quiet_=false, css_present_=false;
    uint64_t start_tick_=0, status_after_=0, legacy_check_after_=0;
    double hotkey_accumulator_=0, seconds_=0;
    std::vector<bool> hotkey_down_;
    std::string notice_;
    // Legacy detection (see docs/migration.md)
    struct Legacy { bool activation_present=false; Json mapped=Json::array(); Json shared_ids=Json::array(); unsigned checks=0; } legacy_;
    void check_legacy();
    bool extensions_allowed() const { return !legacy_.activation_present && legacy_.mapped.empty(); }
    void start_runtime();
    // Frame accounting
    FrameRing<4096> phase_tick_, phase_menu_, phase_hud_, phase_ext_;
    Json frame_stats(double seconds) const;
    // Services for the runtime
    Json service(const Json& request);
    Json dev_request(const Json& request);
    void log(const std::string& level,const std::string& message,const Json& fields=Json::object());
    bool hotkey_pressed(const std::vector<std::string>& keys,size_t slot);
    void hotkey();
    bool game_menu_open() const;
    void publish_status();
};
}

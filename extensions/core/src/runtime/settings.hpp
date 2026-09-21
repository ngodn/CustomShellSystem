#pragma once
#include "common.hpp"
#include <vector>

namespace cssx {
// CSSX's own settings (Mods/CSSX/settings.json), schema 1. Unknown keys are
// preserved on save so a newer file survives an older core.
struct Settings {
    static constexpr int schema=1;
    // Unreal key names polled on the player controller. A chord fires when
    // every key is down and at least one was up on the previous sample.
    std::vector<std::string> open_keyboard{"F6"};
    std::vector<std::string> open_gamepad{"Gamepad_LeftThumbstick","Gamepad_RightThumbstick"};
    double ui_scale=1.0;          // 0.75 .. 1.5, multiplies the 1080-reference layout
    bool pause_while_open=true;   // ask the game's UI handler to pause
    bool hide_hud_while_open=true;
    bool show_extension_status=true;
    Json extra=Json::object();    // unknown keys, round-tripped
    Json json() const;
    static Settings parse(const Json&);
    // Load with recovery: missing -> defaults (written), invalid -> .bak, else defaults.
    static Settings load(const fs::path& file,std::string* note=nullptr);
    void save(const fs::path& file) const;
};
bool valid_key_name(const std::string&);
}

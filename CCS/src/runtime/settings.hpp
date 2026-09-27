#pragma once
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace ccs::runtime {

class Settings {
public:
    explicit Settings(std::filesystem::path file_path);

    bool load();
    bool save() const;

    bool enabled() const { return enabled_; }
    void set_enabled(bool val) { enabled_ = val; }

    const std::string& startup_preset() const { return startup_preset_; }
    void set_startup_preset(std::string preset) { startup_preset_ = std::move(preset); }

    bool preserve_weapon_mesh() const { return preserve_weapon_mesh_; }
    void set_preserve_weapon_mesh(bool val) { preserve_weapon_mesh_ = val; }

    bool show_hud_notification() const { return show_hud_notification_; }
    void set_show_hud_notification(bool val) { show_hud_notification_ = val; }

    double attack_speed_scale() const { return attack_speed_scale_; }
    void set_attack_speed_scale(double val) { attack_speed_scale_ = val; }

    double damage_scale() const { return damage_scale_; }
    void set_damage_scale(double val) { damage_scale_ = val; }

    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);

private:
    std::filesystem::path path_;
    bool enabled_{true};
    std::string startup_preset_{"default"};
    bool preserve_weapon_mesh_{true};
    bool show_hud_notification_{true};
    double attack_speed_scale_{1.0};
    double damage_scale_{1.0};
};

} // namespace ccs::runtime

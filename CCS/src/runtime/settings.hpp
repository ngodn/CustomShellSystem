#pragma once
#include <array>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "ccs_types.hpp"

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

    double ui_scale() const { return ui_scale_; }
    void set_ui_scale(double val) { ui_scale_ = val; }

    // Slot assignments (catalog move ids, empty for the weapon's own attack), saved with the settings
    // so the last customisation returns on the next launch without a named preset.
    const std::array<std::string, 13>& slots() const { return slots_; }
    void set_slots(std::array<std::string, 13> slots) { slots_ = std::move(slots); }
    // Per-slot tuning (speed, hit payload source, weapon shown), same order as slots().
    const std::array<SlotTuning, 13>& tuning() const { return tuning_; }
    void set_tuning(size_t slot, const SlotTuning& value) { if (slot < tuning_.size() && valid_tuning(value)) tuning_[slot] = value; }

    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);

private:
    std::filesystem::path path_;
    bool enabled_{false};
    std::string startup_preset_{"default"};
    bool preserve_weapon_mesh_{true};
    bool show_hud_notification_{true};
    double attack_speed_scale_{1.0};
    double damage_scale_{1.0};
    double ui_scale_{1.0};
    std::array<std::string, 13> slots_{};
    std::array<SlotTuning, 13> tuning_{};
};

} // namespace ccs::runtime

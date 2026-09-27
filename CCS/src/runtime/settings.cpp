#include "settings.hpp"
#include "common.hpp"
#include <fstream>

namespace ccs::runtime {

Settings::Settings(std::filesystem::path file_path) : path_(std::move(file_path)) {}

bool Settings::load() {
    if (!std::filesystem::exists(path_)) {
        save();
        return true;
    }
    std::string text = read_file_text(path_);
    if (text.empty()) return false;
    try {
        nlohmann::json j = nlohmann::json::parse(text);
        from_json(j);
        return true;
    } catch (...) {
        return false;
    }
}

bool Settings::save() const {
    try {
        nlohmann::json j = to_json();
        return write_file_atomic(path_, j.dump(2));
    } catch (...) {
        return false;
    }
}

nlohmann::json Settings::to_json() const {
    return {
        {"enabled", enabled_},
        {"startup_preset", startup_preset_},
        {"preserve_weapon_mesh", preserve_weapon_mesh_},
        {"show_hud_notification", show_hud_notification_},
        {"attack_speed_scale", attack_speed_scale_},
        {"damage_scale", damage_scale_}
    };
}

void Settings::from_json(const nlohmann::json& j) {
    if (j.contains("enabled") && j["enabled"].is_boolean()) enabled_ = j["enabled"].get<bool>();
    if (j.contains("startup_preset") && j["startup_preset"].is_string()) startup_preset_ = j["startup_preset"].get<std::string>();
    if (j.contains("preserve_weapon_mesh") && j["preserve_weapon_mesh"].is_boolean()) preserve_weapon_mesh_ = j["preserve_weapon_mesh"].get<bool>();
    if (j.contains("show_hud_notification") && j["show_hud_notification"].is_boolean()) show_hud_notification_ = j["show_hud_notification"].get<bool>();
    if (j.contains("attack_speed_scale") && j["attack_speed_scale"].is_number()) attack_speed_scale_ = j["attack_speed_scale"].get<double>();
    if (j.contains("damage_scale") && j["damage_scale"].is_number()) damage_scale_ = j["damage_scale"].get<double>();
}

} // namespace ccs::runtime

#include "settings.hpp"
#include "common.hpp"
#include <fstream>
#include <cmath>
#include <stdexcept>

namespace ccs::runtime {

Settings::Settings(std::filesystem::path file_path) : path_(std::move(file_path)) {}

bool Settings::load() {
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) {
        return !ec && save();
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
    if (!j.is_object()) throw std::runtime_error("Settings must be an object");
    auto candidate = *this;
    auto number = [&](const char* key, double fallback) {
        const double value = j.value(key, fallback);
        if (!std::isfinite(value) || value <= 0.0 || value > 10.0)
            throw std::runtime_error("Invalid combat scale");
        return value;
    };
    candidate.attack_speed_scale_ = number("attack_speed_scale", attack_speed_scale_);
    candidate.damage_scale_ = number("damage_scale", damage_scale_);
    if (j.contains("startup_preset") && (!j["startup_preset"].is_string() ||
        !valid_preset_name(j["startup_preset"].get<std::string>())))
        throw std::runtime_error("Invalid startup preset name");
    for (const auto* key : {"enabled", "preserve_weapon_mesh", "show_hud_notification"})
        if (j.contains(key) && !j[key].is_boolean()) throw std::runtime_error("Invalid settings toggle");
    candidate.enabled_ = j.value("enabled", enabled_);
    candidate.startup_preset_ = j.value("startup_preset", startup_preset_);
    candidate.preserve_weapon_mesh_ = j.value("preserve_weapon_mesh", preserve_weapon_mesh_);
    candidate.show_hud_notification_ = j.value("show_hud_notification", show_hud_notification_);
    *this = std::move(candidate);
}

} // namespace ccs::runtime

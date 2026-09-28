#include "settings.hpp"
#include "common.hpp"
#include <algorithm>
#include <cmath>
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
        {"charged_attacks_without_tarstone", charged_without_stone_},
        {"startup_preset", startup_preset_},
        {"preserve_weapon_mesh", preserve_weapon_mesh_},
        {"show_hud_notification", show_hud_notification_},
        {"attack_speed_scale", attack_speed_scale_},
        {"damage_scale", damage_scale_},
        {"ui_scale", ui_scale_},
        {"slots", slots_},
        {"slot_tuning", [&] {
            nlohmann::json list = nlohmann::json::array();
            for (const auto& t : tuning_) list.push_back({{"speed", t.speed}, {"feel", t.feel}, {"hit_damage", t.hit_damage}, {"weapon", t.weapon}, {"armor", t.armor}});
            return list;
        }()}
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
    {
        const double scale = j.value("ui_scale", ui_scale_);
        if (!std::isfinite(scale) || scale < 0.75 || scale > 1.5) throw std::runtime_error("Invalid menu scale");
        candidate.ui_scale_ = scale;
    }
    if (j.contains("slots")) {
        const auto& slots = j["slots"];
        if (!slots.is_array() || slots.size() > candidate.slots_.size()) throw std::runtime_error("Invalid slot list");
        for (size_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].is_string() || slots[i].get_ref<const std::string&>().size() > 256) throw std::runtime_error("Invalid slot move id");
            candidate.slots_[i] = slots[i].get<std::string>();
        }
    }
    if (j.contains("slot_tuning")) {
        const auto& list = j["slot_tuning"];
        if (!list.is_array() || list.size() > candidate.tuning_.size()) throw std::runtime_error("Invalid slot tuning list");
        for (size_t i = 0; i < list.size(); ++i) {
            const auto& t = list[i];
            if (!t.is_object()) throw std::runtime_error("Invalid slot tuning");
            SlotTuning value;
            value.speed = t.value("speed", 1.0);
            value.feel = t.value("feel", std::string("move"));
            value.hit_damage = t.value("hit_damage", std::string("move"));
            value.weapon = t.value("weapon", std::string("inventory"));
            value.armor = t.value("armor", std::string("full"));
            if (!std::isfinite(value.speed) || !valid_tuning(value)) throw std::runtime_error("Invalid slot tuning");
            candidate.tuning_[i] = value;
        }
    } else if (std::abs(candidate.attack_speed_scale_ - 1.0) > 1e-9) {
        // Older settings had one global speed; it becomes every slot's speed once.
        for (auto& t : candidate.tuning_) t.speed = std::clamp(candidate.attack_speed_scale_, 0.5, 2.0);
    }
    if (j.contains("startup_preset") && (!j["startup_preset"].is_string() ||
        !valid_preset_name(j["startup_preset"].get<std::string>())))
        throw std::runtime_error("Invalid startup preset name");
    for (const auto* key : {"enabled", "charged_attacks_without_tarstone", "preserve_weapon_mesh", "show_hud_notification"})
        if (j.contains(key) && !j[key].is_boolean()) throw std::runtime_error("Invalid settings toggle");
    candidate.enabled_ = j.value("enabled", enabled_);
    candidate.charged_without_stone_ = j.value("charged_attacks_without_tarstone", charged_without_stone_);
    candidate.startup_preset_ = j.value("startup_preset", startup_preset_);
    candidate.preserve_weapon_mesh_ = j.value("preserve_weapon_mesh", preserve_weapon_mesh_);
    candidate.show_hud_notification_ = j.value("show_hud_notification", show_hud_notification_);
    *this = std::move(candidate);
}

} // namespace ccs::runtime

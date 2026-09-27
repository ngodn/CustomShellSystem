#include "storage.hpp"
#include <cmath>
#include "common.hpp"
#include <algorithm>

namespace ccs::runtime {

Storage::Storage(std::filesystem::path presets_dir) : dir_(std::move(presets_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
}

std::vector<std::string> Storage::list_presets() const {
    std::vector<std::string> names;
    list_presets(names);
    return names;
}

bool Storage::list_presets(std::vector<std::string>& names) const {
    names.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(dir_, ec) || ec) return false;

    std::filesystem::directory_iterator it(dir_, ec), end;
    size_t inspected{};
    for (; !ec && it != end; it.increment(ec)) {
        if (++inspected > 4096) { names.clear(); return false; }
        if (it->is_regular_file(ec) && it->path().extension() == ".json") {
            auto name = it->path().stem().string();
            if (valid_preset_name(name)) {
                if (names.size() >= 1024) { names.clear(); return false; }
                names.push_back(std::move(name));
            }
        }
    }
    if (ec) { names.clear(); return false; }
    std::sort(names.begin(), names.end());
    return true;
}

std::optional<PresetData> Storage::load_preset(const std::string& name) const {
    if (!valid_preset_name(name)) return std::nullopt;
    std::filesystem::path path = dir_ / (name + ".json");
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;

    std::string text = read_file_text(path);
    if (text.empty()) return std::nullopt;

    try {
        nlohmann::json j = nlohmann::json::parse(text);
        return json_to_preset(j);
    } catch (...) {
        return std::nullopt;
    }
}

bool Storage::save_preset(const PresetData& preset) {
    return save_preset(preset, false) == FileWriteResult::Success;
}
FileWriteResult Storage::save_preset(const PresetData& preset, bool replace_existing) {
    if (!valid_preset_name(preset.name)) return FileWriteResult::Failed;
    std::filesystem::path path = dir_ / (preset.name + ".json");
    try {
        nlohmann::json j = preset_to_json(preset);
        return write_file_atomic(path, j.dump(2), replace_existing);
    } catch (...) {
        return FileWriteResult::Failed;
    }
}

bool Storage::delete_preset(const std::string& name) {
    if (!valid_preset_name(name)) return false;
    std::filesystem::path path = dir_ / (name + ".json");
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

nlohmann::json Storage::preset_to_json(const PresetData& preset) {
    nlohmann::json light = nlohmann::json::object();
    nlohmann::json ranged = nlohmann::json::object();
    nlohmann::json heavy = nlohmann::json::object();
    nlohmann::json sprint = nlohmann::json::object();

    for (const auto& slot : preset.slots) {
        nlohmann::json sj = {
            {"type", slot.origin == MoveOrigin::EnemyHumanoid ? "enemy" :
                     slot.origin == MoveOrigin::TarstoneFinisher ? "tarstone_finisher" :
                     slot.origin == MoveOrigin::HoldAttack ? "hold" : "player"},
            {"source", slot.source},
            {"move_id", slot.move_id},
            {"ability", slot.ability_path},
            {"montage", slot.montage_path},
            {"speed", slot.tuning.speed},
            {"feel", slot.tuning.feel},
            {"hit_damage", slot.tuning.hit_damage},
            {"weapon", slot.tuning.weapon}
        };
        if (!slot.tarstone_id.empty()) {
            sj["tarstone_id"] = slot.tarstone_id;
            sj["tarstone_name"] = slot.tarstone_name;
        }

        const char* s_str = slot_to_string(slot.slot);
        if (slot.slot <= SlotId::LC) light[s_str] = sj;
        else if (slot.slot <= SlotId::HC) heavy[s_str] = sj;
        else if (slot.slot == SlotId::R) ranged[s_str] = sj;
        else sprint[s_str] = sj;
    }

    return {
        {"schema_version", preset.schema_version},
        {"preset_name", preset.name},
        {"author", preset.author},
        {"description", preset.description},
        {"base_weapon", preset.base_weapon},
        {"light_chain", light},
        {"heavy_chain", heavy},
        {"ranged", ranged},
        {"sprint", sprint}
    };
}

std::optional<PresetData> Storage::json_to_preset(const nlohmann::json& j) {
    try {
        if (!j.is_object()) return std::nullopt;
        PresetData preset;
        if (j.contains("schema_version") && !j.at("schema_version").is_number_integer()) return std::nullopt;
        preset.schema_version = j.value("schema_version", 1);
        if (preset.schema_version != 1) return std::nullopt;
        preset.name = j.value("preset_name", preset.name);
        if (!valid_preset_name(preset.name)) return std::nullopt;
        preset.author = j.value("author", preset.author);
        preset.description = j.value("description", preset.description);
        preset.base_weapon = j.value("base_weapon", preset.base_weapon);
        if (preset.author.size() > 256 || preset.description.size() > 4096 || preset.base_weapon.size() > 256)
            return std::nullopt;
        auto parse_chain = [&](const char* key, bool light) {
            if (!j.contains(key)) return;
            const auto& chain = j.at(key);
            if (!chain.is_object()) throw std::runtime_error("Preset chain must be an object");
            for (auto it = chain.begin(); it != chain.end(); ++it) {
                auto slot = string_to_slot(it.key());
                if (!slot || ((*slot <= SlotId::LC) != light) || !it.value().is_object())
                    throw std::runtime_error("Invalid preset slot");
                const auto& val = it.value();
                auto text = [&](const char* name) {
                    auto value = val.value(name, std::string{});
                    if (value.size() > 4096 || value.find('\0') != std::string::npos)
                        throw std::runtime_error("Invalid preset text");
                    return value;
                };
                auto& binding = preset.slots[static_cast<size_t>(*slot)];
                binding.source = text("source");
                binding.ability_path = text("ability");
                binding.move_id = val.contains("move_id") ? text("move_id") : binding.ability_path;
                binding.montage_path = text("montage");
                binding.tarstone_id = text("tarstone_id");
                binding.tarstone_name = text("tarstone_name");
                binding.tuning.speed = val.value("speed", 1.0);
                binding.tuning.feel = val.value("feel", std::string("move"));
                binding.tuning.hit_damage = val.value("hit_damage", std::string("move"));
                binding.tuning.weapon = val.value("weapon", std::string("inventory"));
                if (!std::isfinite(binding.tuning.speed) || !valid_tuning(binding.tuning)) throw std::runtime_error("Invalid slot tuning");
                const auto type = val.value("type", std::string{"player"});
                if (type == "player") binding.origin = MoveOrigin::PlayerWeapon;
                else if (type == "enemy") binding.origin = MoveOrigin::EnemyHumanoid;
                else if (type == "tarstone_finisher") binding.origin = MoveOrigin::TarstoneFinisher;
                else if (type == "hold") binding.origin = MoveOrigin::HoldAttack;
                else throw std::runtime_error("Unknown move origin");
            }
        };
        parse_chain("light_chain", true);
        parse_chain("heavy_chain", false);
        if (j.contains("ranged")) parse_chain("ranged", false);
        if (j.contains("sprint")) parse_chain("sprint", false);
        return preset;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace ccs::runtime

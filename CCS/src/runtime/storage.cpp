#include "storage.hpp"
#include "common.hpp"
#include <algorithm>

namespace ccs::runtime {

Storage::Storage(std::filesystem::path presets_dir) : dir_(std::move(presets_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
}

std::vector<std::string> Storage::list_presets() const {
    std::vector<std::string> names;
    std::error_code ec;
    if (!std::filesystem::exists(dir_, ec)) return names;

    for (const auto& entry : std::filesystem::directory_iterator(dir_, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::optional<PresetData> Storage::load_preset(const std::string& name) const {
    std::filesystem::path path = dir_ / (name + ".json");
    if (!std::filesystem::exists(path)) return std::nullopt;

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
    if (preset.name.empty()) return false;
    std::filesystem::path path = dir_ / (preset.name + ".json");
    try {
        nlohmann::json j = preset_to_json(preset);
        return write_file_atomic(path, j.dump(2));
    } catch (...) {
        return false;
    }
}

bool Storage::delete_preset(const std::string& name) {
    std::filesystem::path path = dir_ / (name + ".json");
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

nlohmann::json Storage::preset_to_json(const PresetData& preset) {
    nlohmann::json light = nlohmann::json::object();
    nlohmann::json heavy = nlohmann::json::object();

    for (const auto& slot : preset.slots) {
        nlohmann::json sj = {
            {"source", slot.source},
            {"move_id", slot.move_id},
            {"ability", slot.ability_path},
            {"montage", slot.montage_path}
        };
        if (!slot.tarstone_id.empty()) {
            sj["tarstone_id"] = slot.tarstone_id;
            sj["tarstone_name"] = slot.tarstone_name;
        }

        const char* s_str = slot_to_string(slot.slot);
        if (slot.slot <= SlotId::LC) {
            light[s_str] = sj;
        } else {
            heavy[s_str] = sj;
        }
    }

    return {
        {"schema_version", preset.schema_version},
        {"preset_name", preset.name},
        {"author", preset.author},
        {"description", preset.description},
        {"base_weapon", preset.base_weapon},
        {"light_chain", light},
        {"heavy_chain", heavy}
    };
}

std::optional<PresetData> Storage::json_to_preset(const nlohmann::json& j) {
    PresetData preset;
    if (j.contains("schema_version") && j["schema_version"].is_number()) {
        preset.schema_version = j["schema_version"].get<int>();
    }
    if (j.contains("preset_name") && j["preset_name"].is_string()) {
        preset.name = j["preset_name"].get<std::string>();
    }
    if (j.contains("author") && j["author"].is_string()) {
        preset.author = j["author"].get<std::string>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        preset.description = j["description"].get<std::string>();
    }
    if (j.contains("base_weapon") && j["base_weapon"].is_string()) {
        preset.base_weapon = j["base_weapon"].get<std::string>();
    }

    auto parse_chain = [&](const nlohmann::json& chain) {
        if (!chain.is_object()) return;
        for (auto it = chain.begin(); it != chain.end(); ++it) {
            auto slot_opt = string_to_slot(it.key());
            if (!slot_opt) continue;
            SlotId slot_id = *slot_opt;
            const auto& val = it.value();
            if (!val.is_object()) continue;

            SlotBinding& binding = preset.slots[static_cast<size_t>(slot_id)];
            binding.slot = slot_id;
            if (val.contains("source") && val["source"].is_string()) binding.source = val["source"].get<std::string>();
            if (val.contains("move_id") && val["move_id"].is_string()) binding.move_id = val["move_id"].get<std::string>();
            if (val.contains("ability") && val["ability"].is_string()) binding.ability_path = val["ability"].get<std::string>();
            if (val.contains("montage") && val["montage"].is_string()) binding.montage_path = val["montage"].get<std::string>();
            if (val.contains("tarstone_id") && val["tarstone_id"].is_string()) binding.tarstone_id = val["tarstone_id"].get<std::string>();
            if (val.contains("tarstone_name") && val["tarstone_name"].is_string()) binding.tarstone_name = val["tarstone_name"].get<std::string>();
        }
    };

    if (j.contains("light_chain")) parse_chain(j["light_chain"]);
    if (j.contains("heavy_chain")) parse_chain(j["heavy_chain"]);

    return preset;
}

} // namespace ccs::runtime

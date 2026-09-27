#pragma once
#include "ccs_types.hpp"
#include <filesystem>
#include <vector>
#include <memory>

namespace ccs::runtime {

class Storage {
public:
    explicit Storage(std::filesystem::path presets_dir);

    std::vector<std::string> list_presets() const;
    std::optional<PresetData> load_preset(const std::string& name) const;
    bool save_preset(const PresetData& preset);
    bool delete_preset(const std::string& name);

    static nlohmann::json preset_to_json(const PresetData& preset);
    static std::optional<PresetData> json_to_preset(const nlohmann::json& j);

    const std::filesystem::path& presets_dir() const { return dir_; }

private:
    std::filesystem::path dir_;
};

} // namespace ccs::runtime

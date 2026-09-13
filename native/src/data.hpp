#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "colors.hpp"

namespace css {
using Json = nlohmann::json;
namespace fs = std::filesystem;
struct Variant {
    std::string id, name, mesh;
    std::map<int,std::string> materials;
};
struct Outfit {
    std::string id, name, author, description, category;
    std::vector<std::string> shells;
    std::vector<Variant> variants;
    bool same_skeleton = false;
    fs::path thumbnail;
    ColorOptions colors;
    fs::path resources;
};
struct Catalog {
    std::vector<Outfit> outfits;
    static Catalog load(const fs::path&,const fs::path& paks={},const fs::path& cache={});
    const Variant* find(const std::string& outfit, const std::string& variant) const;
    bool compatible(const std::string& outfit, const std::string& shell) const;
};
struct Selection { std::string outfit, variant; Customization colors; };
struct State {
    bool enabled = false;
    bool auto_apply = true;
    bool invert_orbit_x = false, invert_orbit_y = true;
    std::map<std::string, Selection> selections;
    std::map<std::string, Customization> remembered_colors;
    std::set<std::string> favorites;
    std::map<std::string, std::map<std::string, Selection>> presets;
    static State parse(const Json&);
    Json json() const;
};
State load_state(const fs::path&, bool* recovered = nullptr);
Json read_json(const fs::path&);
void atomic_json(const fs::path&, const Json&, bool backup = true);
bool valid_id(const std::string&);
bool valid_asset(const std::string&);
}

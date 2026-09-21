#pragma once
#include "common.hpp"
#include <set>
#include <vector>
#include <algorithm>
namespace cssx {
inline constexpr size_t max_extensions=128, max_sections=16, max_controls=512;
inline constexpr int manifest_schema=2, manifest_api=3;
bool valid_namespace(const std::string&);
struct Manifest {
    // schema: 1 (legacy CSS 0.3.x/0.4.x) or 2 (standalone CSSX 1.0). api: 1 or 3.
    int schema=2, api=3;
    std::string id, title, version, author, description, kind, layout;
    fs::path directory, entry, banner, menu;
    static Manifest parse(const Json&,const fs::path&);
    Json json() const;
};
struct Discovery { std::vector<Manifest> entries; Json errors=Json::array(); };
fs::path contained_file(const fs::path& root,const std::string& relative);
Discovery discover(const fs::path&);
void validate_model(const Json&);
Json bind_menu(const Json& definition,const Json& model);
}

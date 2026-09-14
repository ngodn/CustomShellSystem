#pragma once
#include "data.hpp"
namespace css::extensions {
inline constexpr size_t max_extensions=128, max_sections=16, max_controls=512;
struct Manifest {
    std::string id, title, version, author, description, kind, layout;
    fs::path directory, entry, banner;
    static Manifest parse(const Json&,const fs::path&);
    Json json() const;
};
struct Discovery { std::vector<Manifest> entries; Json errors=Json::array(); };
fs::path contained_file(const fs::path& root,const std::string& relative);
Discovery discover(const fs::path&);
void validate_model(const Json&);
struct LibraryPage {
    size_t count=0, page=0, selected=0;
    size_t pages() const { return std::max(size_t{1},(count+8)/9); }
    void normalize();
    void move(int x,int y);
    void slide(int direction);
};
}

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
    // Optional (CSSX 1.3.2): a pause-menu entry that opens the game's Photo Mode with this
    // extension's menu sections added to the panel as tabs, in the game's own widgets.
    struct PhotoTab { std::string section; fs::path icon; };
    std::string photo_entry;                 // label of the System page row, e.g. "Activate CINE Mode"
    std::vector<PhotoTab> photo_tabs;        // one tab per menu section, in order
    std::string photo_target, photo_min_version;
    static Manifest parse(const Json&,const fs::path&);
    Json json() const;
};
struct PhotoModeDefinition {
    struct Tab { const Manifest* provider; const Manifest::PhotoTab* spec; };
    const Manifest* owner;
    std::vector<Tab> tabs;
    std::vector<const Manifest*> contributors;
    size_t focus_tab(const std::string& provider,const std::string& section) const {
        const auto tab=std::ranges::find_if(tabs,[&](const auto& t){
            return t.provider->id==provider && t.spec->section==section;
        });
        return tab==tabs.end()?0:size_t(tab-tabs.begin());
    }
};
bool photo_version_matches(const std::string& version,const std::string& minimum);
struct Discovery { std::vector<Manifest> entries; Json errors=Json::array(); };
fs::path contained_file(const fs::path& root,const std::string& relative);
Discovery discover(const fs::path&);
void validate_model(const Json&);
Json bind_menu(const Json& definition,const Json& model);
}

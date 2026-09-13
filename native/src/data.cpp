#include "data.hpp"
#include "packages.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace css {
bool valid_id(const std::string& s) {
    return !s.empty() && s.size() <= 96 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    }) && s != "." && s != "..";
}
bool valid_asset(const std::string& s) {
    if (!s.starts_with("/Game/") || s.size() > 1024 || s.find("..") != s.npos) return false;
    auto dot = s.rfind('.');
    if (dot == s.npos || dot <= s.rfind('/') || dot + 1 == s.size()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '/' || c == '.';
    });
}
Json read_json(const fs::path& path) {
    if (fs::file_size(path) > 1024 * 1024) throw std::runtime_error("JSON exceeds 1 MiB: " + path.string());
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read: " + path.string());
    return Json::parse(in);
}
void atomic_json(const fs::path& path, const Json& data, bool backup) {
    fs::create_directories(path.parent_path());
    auto temp = path; temp += ".tmp";
    auto bytes = data.dump(2) + "\n";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) throw std::runtime_error("Failed writing CSS state");
    }
    if (read_json(temp) != data) throw std::runtime_error("State read-back mismatch");
    if (backup && fs::exists(path)) fs::copy_file(path, path.string() + ".bak", fs::copy_options::overwrite_existing);
#ifdef _WIN32
    auto file = CreateFileW(temp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot flush CSS state");
    bool flushed = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (!flushed || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot atomically replace CSS state");
#else
    fs::rename(temp, path);
#endif
}
State load_state(const fs::path& file, bool* recovered) {
    if(recovered) *recovered=false;
    auto backup=fs::path(file.string()+".bak");
    if(fs::exists(file)) {
        try {
            auto saved=read_json(file);
            auto state=State::parse(saved);
            if(state.json()!=saved) atomic_json(file,state.json());
            return state;
        } catch(const std::exception&) {
            // Keep the damaged primary, and never overwrite the good backup
            // with it during recovery. An invalid backup still fails visibly.
            auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
            fs::copy_file(file,file.string()+".corrupt-"+std::to_string(stamp));
            if(!fs::exists(backup)) throw;
        }
    }
    State state;
    if(fs::exists(backup)) {
        state=State::parse(read_json(backup));
        if(recovered) *recovered=true;
    }
    atomic_json(file,state.json(),false);
    return state;
}
Catalog Catalog::load(const fs::path& directory,const fs::path& paks,const fs::path& cache) {
    Catalog result;
    std::set<std::string> ids;
    std::vector<fs::path> files;
    if (fs::exists(directory)) {
        if (!fs::is_directory(directory)) throw std::runtime_error("CSS catalog path is not a directory");
        for (const auto& file : fs::directory_iterator(directory))
            if (file.is_regular_file() && file.path().filename().string().ends_with(".css.json")) files.push_back(file.path());
    }
    std::sort(files.begin(), files.end());
    auto documents=package_catalogs(paks,cache);
    for(const auto& path:files) documents.push_back({read_json(path),path.parent_path()});
    for (const auto& document : documents) {
        const auto& j = document.catalog;
        if (j.at("schema") != 1 || !j.at("outfits").is_array()) throw std::runtime_error("Unsupported catalog schema");
        for (const auto& item : j.at("outfits")) {
            Outfit outfit{item.at("id"), item.at("name"), item.value("author", ""),
                          item.value("description", ""), item.value("category", "Shell"), {}, {}, false, {}, {}, {}};
            if (!valid_id(outfit.id) || !ids.insert(outfit.id).second) throw std::runtime_error("Invalid or duplicate outfit id");
            if (outfit.name.empty() || outfit.name.size() > 256 || outfit.description.size() > 4096)
                throw std::runtime_error("Invalid outfit text");
            outfit.shells = item.at("shells").get<std::vector<std::string>>();
            const auto compatibility = item.value("compatibility", "listed_shells");
            if (compatibility != "listed_shells" && compatibility != "same_skeleton")
                throw std::runtime_error("Unsupported outfit compatibility policy");
            outfit.same_skeleton = compatibility == "same_skeleton";
            outfit.colors=ColorOptions::parse(item.value("colors",Json::object()));
            outfit.resources=document.artwork;
            auto thumbnail=item.value("thumbnail",std::string{});
            if(!thumbnail.empty()) {
                if(thumbnail!="thumbnail.png") throw std::runtime_error("Unsupported catalog thumbnail path");
                outfit.thumbnail=document.artwork/thumbnail;
            }
            if (outfit.shells.empty()) throw std::runtime_error("Outfit needs compatible shell tags");
            for (const auto& shell : outfit.shells) if (!valid_id(shell)) throw std::runtime_error("Invalid shell tag");
            std::set<std::string> variants;
            for (const auto& v : item.at("variants")) {
                Variant variant{v.at("id"), v.at("name"), v.at("mesh"), {}};
                if (!valid_id(variant.id) || !variants.insert(variant.id).second || !valid_asset(variant.mesh))
                    throw std::runtime_error("Invalid variant id or asset path");
                if(v.contains("materials")) {
                    const auto& materials=v.at("materials");
                    if(!materials.is_object() || materials.size()>128) throw std::runtime_error("Invalid variant materials");
                    for(const auto& [key,value]:materials.items()) {
                        if(key.empty() || key.size()>3 || (key.size()>1 && key[0]=='0') ||
                           !std::all_of(key.begin(),key.end(),[](char c){return c>='0' && c<='9';}))
                            throw std::runtime_error("Invalid material slot");
                        auto slot=std::stoi(key); auto path=value.get<std::string>();
                        if(slot>=128 || !valid_asset(path)) throw std::runtime_error("Invalid material override");
                        variant.materials.emplace(slot,std::move(path));
                    }
                }
                outfit.variants.push_back(std::move(variant));
            }
            if (outfit.variants.empty() || outfit.variants.size() > 256) throw std::runtime_error("Invalid variant count");
            result.outfits.push_back(std::move(outfit));
            if (result.outfits.size() > 4096) throw std::runtime_error("Catalog exceeds 4096 outfits");
        }
    }
    std::set<std::string> local_colors;
    // Optional local recipes make material authoring testable without remounting containers.
    if(fs::is_directory(directory)) for(const auto& file:fs::directory_iterator(directory)) if(file.is_regular_file() && file.path().filename().string().ends_with(".colors.json")) {
        auto j=read_json(file.path()); auto id=j.at("id").get<std::string>();
        if(!local_colors.insert(id).second) throw std::runtime_error("Duplicate local color recipes");
        auto found=std::find_if(result.outfits.begin(),result.outfits.end(),[&](const auto& o){return o.id==id;});
        if(found==result.outfits.end()) throw std::runtime_error("Local colors reference a missing outfit");
        found->colors=ColorOptions::parse(j.at("colors")); found->resources=file.path().parent_path();
    }
    return result;
}
const Variant* Catalog::find(const std::string& outfit, const std::string& variant) const {
    for (const auto& o : outfits) if (o.id == outfit)
        for (const auto& v : o.variants) if (v.id == variant) return &v;
    return nullptr;
}
bool Catalog::compatible(const std::string& outfit, const std::string& shell) const {
    for (const auto& o : outfits) if (o.id == outfit)
        return (o.same_skeleton && (shell.starts_with("CharacterId.Player.Shell.") || shell.starts_with("CharacterId.Player.Darkform."))) ||
               std::find(o.shells.begin(), o.shells.end(), shell) != o.shells.end();
    return false;
}
static std::map<std::string, Selection> parse_selections(const Json& values) {
    if (!values.is_object() || values.size() > 256) throw std::runtime_error("Invalid selections");
    std::map<std::string, Selection> result;
    for (const auto& [key, value] : values.items()) {
        Selection selection{value.at("outfit"), value.at("variant"), {}};
        selection.colors=Customization::parse(value.value("colors",Json::object()));
        if (!valid_id(key) || !valid_id(selection.outfit) || !valid_id(selection.variant))
            throw std::runtime_error("Invalid saved selection");
        result.emplace(key, std::move(selection));
    }
    return result;
}
State State::parse(const Json& j) {
    if (j.at("schema") != 1) throw std::runtime_error("Unsupported CSS state schema");
    State result;
    result.enabled = j.at("enabled").get<bool>();
    result.auto_apply = j.value("auto_apply", true);
    result.invert_orbit_x = j.value("invert_orbit_x", false);
    result.invert_orbit_y = j.value("invert_orbit_y", true);
    result.selections = parse_selections(j.at("selections"));
    if(j.contains("remembered_colors")) {
        if(!j.at("remembered_colors").is_object() || j.at("remembered_colors").size()>4096) throw std::runtime_error("Invalid saved outfit colors");
        for(const auto& [id,value]:j.at("remembered_colors").items()) {
            if(!valid_id(id)) throw std::runtime_error("Invalid saved outfit color id");
            result.remembered_colors[id]=Customization::parse(value);
        }
    }
    result.favorites = j.value("favorites", std::set<std::string>{});
    for (const auto& id : result.favorites) if (!valid_id(id)) throw std::runtime_error("Invalid favorite");
    if (j.contains("presets")) for (const auto& [key, values] : j.at("presets").items()) {
        if (!valid_id(key) || result.presets.size() >= 64) throw std::runtime_error("Invalid preset");
        result.presets.emplace(key, parse_selections(values));
    }
    return result;
}
static Json selections_json(const std::map<std::string, Selection>& values) {
    Json result = Json::object();
    for (const auto& [shell, selected] : values) result[shell] = {{"outfit", selected.outfit}, {"variant", selected.variant}, {"colors",selected.colors.json()}};
    return result;
}
Json State::json() const {
    Json presets_json = Json::object();
    Json remembered = Json::object();
    for(const auto& [id,colors]:remembered_colors) remembered[id]=colors.json();
    for (const auto& [name, values] : presets) presets_json[name] = selections_json(values);
    return {{"schema", 1}, {"enabled", enabled}, {"auto_apply", auto_apply},
            {"invert_orbit_x", invert_orbit_x}, {"invert_orbit_y", invert_orbit_y},
            {"selections", selections_json(selections)}, {"favorites", favorites}, {"presets", presets_json}, {"remembered_colors",remembered}};
}
}

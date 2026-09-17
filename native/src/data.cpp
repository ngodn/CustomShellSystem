#include <cmath>
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
// 1.0 renamed the manifest and state block from "colors" to "customize": it carries
// switches, textures and live springs now, not only colour. Anything published under the
// old name keeps loading, because the old key is read whenever the new one is absent.
// Naming both is refused rather than guessed at.
namespace {
constexpr const char* ITEM_SLOT_NAMES[]={
    "body","head","hair","face","ears","neck","chest","back","hands","waist","legs","feet",
    "jewelry","genitalia","fabric_outer","fabric_inner",
    "trinket1","trinket2","trinket3","trinket4"
};
}
const char* item_slot_name(ItemSlot slot) {
    const auto index=static_cast<size_t>(slot);
    return index<std::size(ITEM_SLOT_NAMES)?ITEM_SLOT_NAMES[index]:"body";
}
bool item_slot_from_name(const std::string& name,ItemSlot& out) {
    for(size_t i=0;i<std::size(ITEM_SLOT_NAMES);++i)
        if(name==ITEM_SLOT_NAMES[i]) { out=static_cast<ItemSlot>(i); return true; }
    return false;
}
static std::map<int,std::string> parse_materials(const Json& j) {
    std::map<int,std::string> result;
    if(!j.is_object() || j.size()>128) throw std::runtime_error("Invalid material overrides");
    for(const auto& [key,value]:j.items()) {
        if(key.empty() || key.size()>3 || (key.size()>1 && key[0]=='0') ||
           !std::all_of(key.begin(),key.end(),[](char c){return c>='0' && c<='9';}))
            throw std::runtime_error("Invalid material slot");
        auto slot=std::stoi(key); auto path=value.get<std::string>();
        if(slot>=128 || !valid_asset(path)) throw std::runtime_error("Invalid material override");
        result.emplace(slot,std::move(path));
    }
    return result;
}
static Json customize_block(const Json& j) {
    if(j.contains("customize")) {
        if(j.contains("colors")) throw std::runtime_error("Name the controls once: customize, or the older colors, not both");
        return j.at("customize");
    }
    return j.value("colors",Json::object());
}
static bool has_customize(const Json& j) { return j.contains("customize") || j.contains("colors"); }
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
    if (fs::file_size(path) > 1024 * 1024) throw std::runtime_error("JSON exceeds 1 MiB: " + path_utf8(path));
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read: " + path_utf8(path));
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
    if (backup && fs::exists(path)) {
        auto previous=path; previous+=".bak";
        fs::copy_file(path,previous,fs::copy_options::overwrite_existing);
    }
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
    auto backup=file; backup+=".bak";
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
            auto archived=file; archived+=".corrupt-"+std::to_string(stamp);
            fs::copy_file(file,archived);
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
            if (file.is_regular_file() && path_utf8(file.path().filename()).ends_with(".css.json")) files.push_back(file.path());
    }
    std::sort(files.begin(), files.end());
    auto documents=package_catalogs(paks,cache,&result.diagnostics);
    for(const auto& path:files) documents.push_back({read_json(path),path.parent_path(),{}});
    for (const auto& document : documents) {
        const auto previous_size=result.outfits.size();
        const auto previous_ids=ids;
        auto package_status=[&](const char* status,const std::string& reason={}) {
            auto source=document.source.generic_u8string();
            for(auto& file:result.diagnostics["files"]) if(file["path"]==std::string(source.begin(),source.end())) {
                file["status"]=status;
                if(!reason.empty()) file["reason"]=reason;
            }
        };
        try {
        const auto& j = document.catalog;
        if (j.at("schema") != 1 || !j.at("outfits").is_array()) throw std::runtime_error("Unsupported catalog schema");
        for (const auto& item : j.at("outfits")) {
            Outfit outfit{item.at("id"), item.at("name"), item.value("author", ""),
                          item.value("description", ""), item.value("category", "Shell"), {}, {}, false, {}, {}, {}, {}};
            if (!valid_id(outfit.id) || !ids.insert(outfit.id).second) throw std::runtime_error("Invalid or duplicate outfit id");
            if (outfit.name.empty() || outfit.name.size() > 256 || outfit.description.size() > 4096)
                throw std::runtime_error("Invalid outfit text");
            outfit.shells = item.at("shells").get<std::vector<std::string>>();
            const auto compatibility = item.value("compatibility", "listed_shells");
            if (compatibility != "listed_shells" && compatibility != "same_skeleton")
                throw std::runtime_error("Unsupported outfit compatibility policy");
            outfit.same_skeleton = compatibility == "same_skeleton";
            outfit.controls=ControlSet::parse(customize_block(item));
            outfit.resources=document.artwork;
            auto thumbnail=item.value("thumbnail",std::string{});
            if(!thumbnail.empty()) {
                if(thumbnail!="thumbnail.png") throw std::runtime_error("Unsupported catalog thumbnail path");
                outfit.thumbnail=document.artwork/thumbnail;
            }
            if (outfit.shells.empty()) throw std::runtime_error("Outfit needs compatible shell tags");
            for (const auto& shell : outfit.shells) if (!valid_id(shell)) throw std::runtime_error("Invalid shell tag");
            if(item.contains("templates")) {
                const auto& t = item.at("templates");
                if(!t.is_object()) throw std::runtime_error("Invalid outfit templates");
                auto parse_grp = [&](const char* key, TemplateKind kind) {
                    if(t.contains(key)) {
                        const auto& list = t.at(key);
                        if(!list.is_array() || list.size() > 64) throw std::runtime_error("Invalid template list");
                        for(const auto& entry : list) {
                            if(!entry.is_object()) throw std::runtime_error("Invalid template entry");
                            Template tmpl;
                            tmpl.id = entry.at("id").get<std::string>();
                            tmpl.name = entry.at("name").get<std::string>();
                            if(!valid_id(tmpl.id) || tmpl.name.empty() || tmpl.name.size() > 96)
                                throw std::runtime_error("Invalid template id or name");
                            tmpl.kind = kind;
                            tmpl.data = entry;
                            outfit.templates.push_back(std::move(tmpl));
                        }
                    }
                };
                parse_grp("combinations", TemplateKind::Combination);
                parse_grp("palettes", TemplateKind::Palette);
                parse_grp("archetypes", TemplateKind::Archetype);
                parse_grp("physics", TemplateKind::Physics);
                parse_grp("hair", TemplateKind::Hair);
                parse_grp("jewelry", TemplateKind::Jewelry);
                parse_grp("glow", TemplateKind::Glow);
                parse_grp("accessories", TemplateKind::Accessory);
                parse_grp("fabrics", TemplateKind::Fabric);
                parse_grp("anatomy", TemplateKind::Anatomy);
            }
            std::set<std::string> variants;
            for (const auto& v : item.at("variants")) {
                Variant variant{v.at("id"), v.at("name"), v.value("mesh",std::string{}), {}, {}, {}, {}};
                if(has_customize(v)) variant.controls=ControlSet::parse(customize_block(v));
                if (!valid_id(variant.id) || !variants.insert(variant.id).second ||
                    (!v.contains("items") && !valid_asset(variant.mesh)))
                    throw std::runtime_error("Invalid variant id or asset path");
                if(v.contains("materials")) variant.materials=parse_materials(v.at("materials"));
                // 1.0: a variant is a list of items. `mesh` and `materials` are the older
                // spelling of a single body item and stay the common case, so a package
                // published before this reads as a one-item package and nothing changes.
                if(v.contains("items")) {
                    if(v.contains("mesh") || v.contains("materials"))
                        throw std::runtime_error("A variant names its body once: items, or mesh and materials, not both");
                    const auto& list=v.at("items");
                    if(!list.is_array() || list.empty() || list.size()>16) throw std::runtime_error("A variant needs between one and sixteen items");
                    std::set<std::string> ids; std::set<int> slots; bool body=false;
                    for(const auto& j:list) {
                        Item entry;
                        entry.id=j.at("id"); entry.name=j.at("name"); entry.mesh=j.at("mesh");
                        if(!valid_id(entry.id) || !ids.insert(entry.id).second ||
                           entry.name.empty() || entry.name.size()>96 || !valid_asset(entry.mesh))
                            throw std::runtime_error("Invalid item id, name or mesh");
                        const auto slot=j.value("slot",std::string("body"));
                        if(!item_slot_from_name(slot,entry.slot)) throw std::runtime_error("Unsupported item slot: "+slot);
                        // One item per slot. The four trinket slots exist so a package with
                        // more jewellery than the body has places for it still has somewhere
                        // to put it, rather than stacking two things on one socket by accident.
                        if(!slots.insert(static_cast<int>(entry.slot)).second)
                            throw std::runtime_error("Two items claim the same slot: "+slot);
                        if(entry.slot==ItemSlot::Body) body=true;
                        entry.order=j.value("order",0);
                        if(entry.order<0 || entry.order>999) throw std::runtime_error("Item order outside range");
                        if(j.contains("materials")) entry.materials=parse_materials(j.at("materials"));
                        if(j.contains("hides")) {
                            const auto& hides=j.at("hides");
                            if(!hides.is_object()) throw std::runtime_error("Invalid item hides block");
                            if(hides.contains("sections")) {
                                entry.hides_sections=hides.at("sections").get<std::vector<int>>();
                                if(entry.hides_sections.size()>128) throw std::runtime_error("Invalid hidden section count");
                                for(int index:entry.hides_sections)
                                    if(index<0 || index>=128) throw std::runtime_error("Hidden section outside range");
                            }
                        }
                        variant.items.push_back(std::move(entry));
                    }
                    if(!body) throw std::runtime_error("A variant needs one item in the body slot");
                    // Everything downstream still asks a variant for its mesh, so the body
                    // item is mirrored there rather than teaching every caller about items.
                    for(const auto& entry:variant.items) if(entry.slot==ItemSlot::Body) {
                        variant.mesh=entry.mesh; variant.materials=entry.materials;
                    }
                } else {
                    Item body{variant.id,variant.name,variant.mesh,ItemSlot::Body,0,variant.materials,{}};
                    variant.items.push_back(std::move(body));
                }
                if(v.contains("attachments")) {
                    const auto& attachments=v.at("attachments");
                    if(!attachments.is_object() || attachments.size()>32) throw std::runtime_error("Invalid variant attachments");
                    for(const auto& [socket,value]:attachments.items()) {
                        if(socket.empty() || socket.size()>96 || !value.is_object()) throw std::runtime_error("Invalid attachment socket");
                        AttachmentOffset offset;
                        auto vec=[&](const char* key,std::array<double,3>& out,double limit) {
                            if(!value.contains(key)) return;
                            const auto& list=value.at(key);
                            if(!list.is_array() || list.size()!=3) throw std::runtime_error("Invalid attachment offset");
                            for(size_t i=0;i<3;++i) {
                                if(!list[i].is_number()) throw std::runtime_error("Invalid attachment offset");
                                out[i]=list[i].get<double>();
                                if(!std::isfinite(out[i]) || std::abs(out[i])>limit) throw std::runtime_error("Attachment offset out of range");
                            }
                        };
                        vec("location",offset.location,50.); vec("rotation",offset.rotation,180.);
                        // The live correction is optional: without a clearance there is
                        // nothing to hold, the fixed offset still applies, and a block
                        // written for an older CSS is simply ignored rather than
                        // rejecting the whole package.
                        if(value.contains("collision") && value.at("collision").is_object() &&
                           value.at("collision").value("clearance",0.)>0) {
                            const auto& live=value.at("collision");
                            auto& collision=offset.collision;
                            collision.anchor=live.value("anchor",std::string{});
                            if(collision.anchor.size()>96) throw std::runtime_error("Invalid collision anchor bone");
                            collision.clearance=live.at("clearance").get<double>();
                            if(!std::isfinite(collision.clearance) || collision.clearance>50.)
                                throw std::runtime_error("Collision clearance out of range");
                            collision.max_push=live.value("max_push",0.);
                            if(!std::isfinite(collision.max_push) || collision.max_push<=0 || collision.max_push>50.)
                                throw std::runtime_error("Collision push limit out of range");
                            const auto& direction=live.value("direction",Json::array({0,0,0}));
                            if(!direction.is_array() || direction.size()!=3) throw std::runtime_error("Invalid collision direction");
                            double length=0;
                            for(size_t i=0;i<3;++i) {
                                if(!direction[i].is_number()) throw std::runtime_error("Invalid collision direction");
                                collision.direction[i]=direction[i].get<double>();
                                if(!std::isfinite(collision.direction[i])) throw std::runtime_error("Invalid collision direction");
                                length+=collision.direction[i]*collision.direction[i];
                            }
                            if(!collision.anchor.empty() && std::abs(std::sqrt(length)-1.)>1e-3)
                                throw std::runtime_error("Collision direction is not a unit vector");
                        }
                        variant.attachments.emplace(socket,offset);
                    }
                }
                outfit.variants.push_back(std::move(variant));
            }
            if (outfit.variants.empty() || outfit.variants.size() > 256) throw std::runtime_error("Invalid variant count");
            result.outfits.push_back(std::move(outfit));
            if (result.outfits.size() > 4096) throw std::runtime_error("Catalog exceeds 4096 outfits");
        }
        if(!document.source.empty()) package_status("loaded");
        } catch(const std::exception& error) {
            // Loose authoring catalogs remain strict. Installed packages fail
            // independently and retain a filename and reason in diagnostics.
            if(document.source.empty()) throw;
            result.outfits.resize(previous_size); ids=previous_ids;
            package_status("rejected",error.what());
        }
    }
    std::set<std::string> local_recipes;
    // Optional local recipes make material authoring testable without remounting containers.
    if(fs::is_directory(directory)) for(const auto& file:fs::directory_iterator(directory)) if(file.is_regular_file() && (path_utf8(file.path().filename()).ends_with(".customize.json") ||
     path_utf8(file.path().filename()).ends_with(".colors.json"))) {
        auto j=read_json(file.path()); auto id=j.at("id").get<std::string>();
        if(!local_recipes.insert(id).second) throw std::runtime_error("Duplicate local control recipes");
        auto found=std::find_if(result.outfits.begin(),result.outfits.end(),[&](const auto& o){return o.id==id;});
        if(found==result.outfits.end()) throw std::runtime_error("Local controls reference a missing outfit");
        found->controls=ControlSet::parse(customize_block(j)); found->resources=file.path().parent_path();
    }
    for(const auto* status:{"loaded","rejected","ignored"}) {
        size_t count=0;
        for(const auto& file:result.diagnostics["files"]) if(file["status"]==status) ++count;
        result.diagnostics[status]=count;
    }
    return result;
}
std::string Catalog::empty_message() const {
    if(diagnostics.value("rejected",size_t{})>0)
        return "CSS could not load the outfit packages. See CSS.log for filenames and reasons.";
    if(diagnostics.contains("errors") && !diagnostics["errors"].empty())
        return "CSS could not read a package folder. See CSS.log for the path and reason.";
    if(diagnostics.value("pak_files",size_t{})>0)
        return "No CSS outfit metadata found. Install the CSS versions of outfit packages, then restart the game.";
    return "No CSS outfit packages found. Install an outfit package, then restart the game.";
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
        selection.custom=Customization::parse(customize_block(value));
        if (!valid_id(key) || !valid_id(selection.outfit) || !valid_id(selection.variant))
            throw std::runtime_error("Invalid saved selection");
        result.emplace(key, std::move(selection));
    }
    return result;
}
bool valid_walk_animation(const std::string& value) { return value=="normal" || value=="feminine"; }
static Preset parse_preset(const Json& j) {
    Preset result;
    // 0.4 templates are {"selections":{...},"walk_animation":...}; older ones are the bare selection map.
    if(j.is_object() && j.contains("selections")) {
        result.selections=parse_selections(j.at("selections"));
        result.walk_animation=j.value("walk_animation","normal");
        if(!valid_walk_animation(result.walk_animation))
            throw std::runtime_error("Invalid template animation");
        // Jog and sprint are pinned to normal: the 0.3.3 preview shipped a
        // "run_animation" and then a jog/sprint pair, and neither had its stride
        // matched. Whatever a template holds, it loads as normal.
    } else result.selections=parse_selections(j);
    return result;
}
State State::parse(const Json& j) {
    if (j.at("schema") != 1) throw std::runtime_error("Unsupported CSS state schema");
    State result;
    result.enabled = j.at("enabled").get<bool>();
    result.auto_apply = j.value("auto_apply", true);
    result.invert_orbit_x = j.value("invert_orbit_x", false);
    result.invert_orbit_y = j.value("invert_orbit_y", true);
    result.walk_animation = j.value("walk_animation", "normal");
    if(!valid_walk_animation(result.walk_animation))
        throw std::runtime_error("Invalid walk animation setting");
    // Jog and sprint stay normal, so a state written by the 0.3.3 preview comes
    // back with the game's own run rather than the unmatched borrowed one.
    result.selections = parse_selections(j.at("selections"));
    // Same rename, same compatibility: a state file written by 0.4 still loads.
    const auto remembered_key=j.contains("remembered_custom")?"remembered_custom":"remembered_colors";
    if(j.contains(remembered_key)) {
        if(!j.at(remembered_key).is_object() || j.at(remembered_key).size()>4096) throw std::runtime_error("Invalid saved outfit settings");
        for(const auto& [id,value]:j.at(remembered_key).items()) {
            if(!valid_id(id)) throw std::runtime_error("Invalid saved outfit id");
            result.remembered_custom[id]=Customization::parse(value);
        }
    }
    result.favorites = j.value("favorites", std::set<std::string>{});
    for (const auto& id : result.favorites) if (!valid_id(id)) throw std::runtime_error("Invalid favorite");
    const auto presets_key = j.contains("profiles") ? "profiles" : "presets";
    if (j.contains(presets_key)) for (const auto& [key, values] : j.at(presets_key).items()) {
        if (!valid_id(key) || result.presets.size() >= 64) throw std::runtime_error("Invalid profile");
        result.presets.emplace(key, parse_preset(values));
    }
    return result;
}
static Json selections_json(const std::map<std::string, Selection>& values) {
    Json result = Json::object();
    for (const auto& [shell, selected] : values) result[shell] = {{"outfit", selected.outfit}, {"variant", selected.variant}, {"customize",selected.custom.json()}};
    return result;
}
Json State::json() const {
    Json presets_json = Json::object();
    Json remembered = Json::object();
    for(const auto& [id,custom]:remembered_custom) remembered[id]=custom.json();
    for (const auto& [name, preset] : presets) presets_json[name] = {{"selections", selections_json(preset.selections)}, {"walk_animation", preset.walk_animation}};
    return {{"schema", 1}, {"enabled", enabled}, {"auto_apply", auto_apply},
            {"invert_orbit_x", invert_orbit_x}, {"invert_orbit_y", invert_orbit_y}, {"walk_animation", walk_animation},
            {"selections", selections_json(selections)}, {"favorites", favorites}, {"presets", presets_json}, {"remembered_custom",remembered}};
}
}

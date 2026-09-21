#include "animations.hpp"
#include "data.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace css {
namespace {
constexpr const char* SLOT_NAMES[]={"idle","walk","jog","sprint","beacon"};
void fields(const Json& value, const std::set<std::string>& allowed) {
    if(!value.is_object()) throw std::runtime_error("Animation entry must be an object");
    for(const auto& [key,_]:value.items()) if(!allowed.contains(key))
        throw std::runtime_error("Unknown animation field: "+key);
}
std::string asset(const Json& value,const char* key) {
    auto path=value.at(key).get<std::string>();
    if(!valid_asset(path)) throw std::runtime_error(std::string("Invalid animation asset: ")+key);
    return path;
}
bool weapon_tag(const std::string& tag) {
    if(!valid_id(tag) || !tag.starts_with("Weapon.") || tag.size()<=7 || tag.back()=='.' || tag.find("..")!=tag.npos)
        return false;
    return true;
}
}
const char* animation_slot_name(AnimationSlot slot) {
    const auto index=static_cast<size_t>(slot);
    if(index>=std::size(SLOT_NAMES)) throw std::runtime_error("Invalid animation slot");
    return SLOT_NAMES[index];
}
std::optional<AnimationSlot> animation_slot_from_name(std::string_view name) {
    for(size_t i=0;i<std::size(SLOT_NAMES);++i) if(name==SLOT_NAMES[i]) return static_cast<AnimationSlot>(i);
    return {};
}
AnimationSet AnimationSet::parse(const Json& value) {
    if(!value.is_object()) throw std::runtime_error("Animations must be an object");
    AnimationSet result;
    for(const auto& [key,list]:value.items()) {
        const auto slot=animation_slot_from_name(key);
        if(!slot || !list.is_array() || list.size()>64) throw std::runtime_error("Invalid animation slot or option list: "+key);
        auto& options=result.slots[*slot];
        std::set<std::string> ids;
        for(const auto& entry:list) {
            if(*slot==AnimationSlot::Idle) fields(entry,{"id","name","clip","by_weapon","hide_weapons"});
            else if(*slot==AnimationSlot::Beacon) fields(entry,{"id","name","depart","arrive"});
            else fields(entry,{"id","name","blend_space"});
            AnimationOption option;
            option.id=entry.at("id").get<std::string>(); option.name=entry.at("name").get<std::string>();
            if(!valid_id(option.id) || option.id=="original" || option.id==feminine_animation_id || !ids.insert(option.id).second ||
                option.name.empty() || option.name.size()>96)
                throw std::runtime_error("Invalid or duplicate animation option");
            if(*slot==AnimationSlot::Idle) {
                if(entry.contains("clip")) option.clip=asset(entry,"clip");
                option.hide_weapons=entry.value("hide_weapons",false);
                if(entry.contains("by_weapon")) {
                    const auto& weapons=entry.at("by_weapon");
                    if(!weapons.is_object() || weapons.empty() || weapons.size()>64)
                        throw std::runtime_error("Invalid idle weapon overrides");
                    for(const auto& [tag,path]:weapons.items()) {
                        if(!weapon_tag(tag) || !path.is_string() || !valid_asset(path.get<std::string>()))
                            throw std::runtime_error("Invalid idle weapon tag or asset");
                        option.by_weapon.emplace(tag,path.get<std::string>());
                    }
                }
                if(option.clip.empty() && option.by_weapon.empty()) throw std::runtime_error("Idle needs a clip or weapon overrides");
                if(option.hide_weapons && (option.clip.empty() || !option.by_weapon.empty()))
                    throw std::runtime_error("Hidden-weapon idle needs one common clip");
            } else if(*slot==AnimationSlot::Beacon) {
                option.depart=asset(entry,"depart"); option.arrive=asset(entry,"arrive");
            } else option.blend_space=asset(entry,"blend_space");
            options.push_back(std::move(option));
        }
    }
    return result;
}
ResolvedAnimation resolve_animation(const std::vector<AnimationOption>& options,AnimationSlot slot,
    const std::string& choice,const std::string& weapon) {
    if(choice.empty() || choice=="original") return {};
    const auto found=std::find_if(options.begin(),options.end(),[&](const auto& o){return o.id==choice;});
    if(found==options.end()) return {};
    std::string path;
    if(slot==AnimationSlot::Idle) {
        const auto specific=found->by_weapon.find(weapon);
        path=specific==found->by_weapon.end()?found->clip:specific->second;
    } else if(slot==AnimationSlot::Beacon) path=found->depart;
    else path=found->blend_space;
    if(path.empty()) return {};
    return {&*found,std::move(path),slot==AnimationSlot::Idle && found->hide_weapons};
}
AnimationMenu animation_menu(const std::vector<AnimationOption>& options,AnimationSlot slot,
    const std::string* saved,bool legacy_feminine) {
    AnimationMenu menu;
    menu.items.push_back({"original","Default"});
    if(slot==AnimationSlot::Walk || slot==AnimationSlot::Idle) menu.items.push_back({feminine_animation_id,"Feminine (CSS)"});
    for(const auto& option:options) menu.items.push_back({option.id,option.name});
    const std::string choice=saved?*saved:(slot==AnimationSlot::Walk || slot==AnimationSlot::Idle) && legacy_feminine?feminine_animation_id:"original";
    for(size_t i=0;i<menu.items.size();++i) if(menu.items[i].id==choice) {menu.selected=i;return menu;}
    menu.selected=menu.items.size();
    menu.items.push_back({choice,"Unavailable (using Default)",false});
    return menu;
}
std::string AnimationMenu::step(int direction) const {
    if(items.empty() || selected>=items.size() || (direction!=1 && direction!=-1))
        throw std::runtime_error("Invalid animation menu navigation");
    auto index=selected;
    for(size_t n=0;n<items.size();++n) {
        index=direction>0?(index+1)%items.size():(index+items.size()-1)%items.size();
        if(items[index].available) return items[index].id;
    }
    throw std::runtime_error("Animation menu has no available choice");
}
AnimationChoices AnimationChoices::parse(const Json& value) {
    if(!value.is_object() || value.size()>4096) throw std::runtime_error("Invalid saved animation outfits");
    AnimationChoices result;
    size_t variant_count=0;
    for(const auto& [outfit,variants]:value.items()) {
        if(!valid_id(outfit) || !variants.is_object() || variants.size()>256)
            throw std::runtime_error("Invalid saved animation variants");
        auto& saved=result.outfits[outfit];
        for(const auto& [variant,choices]:variants.items()) {
            if(!valid_id(variant) || !choices.is_object() || ++variant_count>4096)
                throw std::runtime_error("Invalid saved animation choices");
            auto& slots=saved[variant];
            for(const auto& [key,id]:choices.items()) {
                const auto slot=animation_slot_from_name(key);
                if(!slot || !id.is_string() || !valid_id(id.get<std::string>()))
                    throw std::runtime_error("Invalid saved animation choice");
                slots.emplace(*slot,id.get<std::string>());
            }
        }
    }
    return result;
}
Json AnimationChoices::json() const {
    Json result=Json::object();
    for(const auto& [outfit,variants]:outfits) {
        auto& saved=result[outfit]=Json::object();
        for(const auto& [variant,choices]:variants) {
            auto& slots=saved[variant]=Json::object();
            for(const auto& [slot,id]:choices) slots[animation_slot_name(slot)]=id;
        }
    }
    return result;
}
const std::string* AnimationChoices::find(const std::string& outfit,const std::string& variant,AnimationSlot slot) const {
    const auto o=outfits.find(outfit); if(o==outfits.end()) return nullptr;
    const auto v=o->second.find(variant); if(v==o->second.end()) return nullptr;
    const auto s=v->second.find(slot); return s==v->second.end()?nullptr:&s->second;
}
std::string AnimationChoices::get(const std::string& outfit,const std::string& variant,AnimationSlot slot) const {
    const auto* choice=find(outfit,variant,slot);
    return choice?*choice:"original";
}
}

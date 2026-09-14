#include "cheat_menu.hpp"

namespace cheat {
void Menu::shell_points(bool enabled) {
    constexpr const char* property="StartingMaxShellPoints";
    auto update=[&](const MapSaved& saved,const Json& expected,const Json& value) {
        return host_.request({{"op","map.update"},{"target",saved.owner},{"property",property},
                              {"key",saved.key},{"expected",expected},{"value",value}});
    };
    if(!enabled) {
        for(auto it=points_saved_.begin();it!=points_saved_.end();) {
            const auto& saved=it->second;Json entries;
            try {entries=host_.get(saved.owner,property).at("$map");}
            catch(const std::exception& error) {
                if(std::string(error.what()).find("expired")!=std::string::npos) {it=points_saved_.erase(it);continue;}
                throw;
            }
            bool found=false;
            for(const auto& entry:entries) if(entry.at("key")==saved.key) {
                found=true;
                if(entry.at("value")==saved.expected) update(saved,saved.expected,saved.before);
                else host_.log("A newer shell-point limit was kept during cleanup.","warning",{{"key",saved.key}});
                break;
            }
            // An entry removed by the game belongs to that newer state. Never
            // recreate it just to restore an earlier value.
            if(!found) host_.log("A removed shell-point entry was left absent.","warning",{{"key",saved.key}});
            it=points_saved_.erase(it);
        }
        return;
    }
    const auto player=require_player();Json component;
    try {component=host_.get(player.at("controller"),"Progression Component");}
    catch(const std::exception& error) {
        if(std::string(error.what()).find("property is missing")==std::string::npos) throw;
        component=host_.get(player.at("controller"),"ProgressionComponent");
    }
    if(!component.is_object() || !component.contains("$object")) throw std::runtime_error("Player progression component is unavailable.");
    const auto entries=host_.get(component,property).at("$map");
    if(!entries.is_array() || entries.empty() || entries.size()>128) throw std::runtime_error("Shell-point limits are unavailable or exceed 128 shells.");
    std::set<std::string> tags;
    for(const auto& entry:entries) {
        const auto tag=entry.at("key").at("TagName").get<std::string>();
        const auto& value=entry.at("value");
        if(!tag.starts_with("CharacterId.Player.Shell.") || !tags.insert(tag).second || !value.is_number_integer() || value.get<int>()<0 || value.get<int>()>10000)
            throw std::runtime_error("Unsupported shell-point entry; no limits were changed.");
    }
    const auto owner=std::to_string(component.at("$object").get<uint64_t>());
    for(const auto& entry:entries) {
        const auto current=entry.at("value");
        const auto id=owner+":"+entry.at("key").at("TagName").get<std::string>();
        auto it=points_saved_.find(id);
        if(it!=points_saved_.end()) {
            // Reapply only if the game returned to our original baseline.
            // Other changed values belong to another writer until disabled.
            if(current==it->second.before) update(it->second,current,it->second.expected);
            continue;
        }
        if(current.get<int>()>=100) continue;
        MapSaved saved{component,entry.at("key"),current,100};
        // Retain ownership before the request so failed cleanup can retry.
        points_saved_.emplace(id,saved);
        update(saved,current,100);
    }
}
}

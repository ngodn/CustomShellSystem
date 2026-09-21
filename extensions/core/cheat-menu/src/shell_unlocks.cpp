#include "cheat_menu.hpp"

namespace cheat {
namespace {
void signature(const cssx::Client& host,const Json& target,const char* function,
               std::initializer_list<std::pair<const char*,int>> fields) {
    const auto args=host.request({{"op","describe"},{"target",target},{"function",function}});
    if(!args.is_object() || args.size()!=fields.size()) throw std::runtime_error(std::string("Unsupported game signature: ")+function);
    for(const auto& [name,size]:fields)
        if(!args.contains(name) || args.at(name).at("size")!=size)
            throw std::runtime_error(std::string("Unsupported game parameter: ")+function+"."+name);
}
uint64_t id(const Json& object) {
    if(!object.is_object() || !object.contains("$object") || (!object.at("$object").is_number_unsigned() && !object.at("$object").is_number_integer()))
        throw std::runtime_error("A required shell-unlock object is unavailable.");
    const auto value=object.at("$object").get<int64_t>();
    if(value<=0) throw std::runtime_error("A required shell-unlock object is unavailable.");
    return static_cast<uint64_t>(value);
}
std::string tag_name(const Json& tag,bool shell_only=true) {
    if(!tag.is_object() || tag.size()!=1 || !tag.contains("TagName") || !tag.at("TagName").is_string())
        throw std::runtime_error("Unsupported shell tag data.");
    const auto name=tag.at("TagName").get<std::string>();
    const std::string prefix="CharacterId.Player.Shell.";
    if(name.empty() || name.size()>256 || (shell_only && (!name.starts_with(prefix) || name.size()==prefix.size())))
        throw std::runtime_error("The shell catalog contains an invalid tag.");
    return name;
}
std::map<std::string,bool> equipment_state(const cssx::Client& host,const Json& save) {
    const auto data=host.get(save,"EquipmentUnlockState");
    const auto& entries=data.at("$map");
    if(!entries.is_array() || entries.size()>4096) throw std::runtime_error("Equipment unlock data exceeds bounds.");
    std::map<std::string,bool> result;
    for(const auto& entry:entries) {
        const auto name=tag_name(entry.at("key"),false);
        if(!entry.at("value").is_boolean() || !result.emplace(name,entry.at("value").get<bool>()).second)
            throw std::runtime_error("Unsupported equipment unlock data.");
    }
    return result;
}
}
void Menu::unlock_shells(const Json& player) {
    const auto pawn=player.at("pawn"),pc=player.at("controller");
    const auto save=host_.get(pc,"PlayerSaveGameObject");id(save);
    const auto library=host_.request({{"op","class_default"},{"class","BPFL_Player_C"}});
    const auto equipment=host_.request({{"op","class_default"},{"class","BPFL_Player_Equipment_C"}});
    const auto statics=host_.request({{"op","class_default"},{"class","GameplayStatics"}});
    signature(host_,pc,"S_UnlockAllShells",{});
    signature(host_,library,"GetShellsIDsTagContainer",{{"ReturnValue",32},{"__WorldContext",8}});
    signature(host_,equipment,"UnlockShell",{{"ShellId",8},{"Save",1},{"__WorldContext",8}});
    signature(host_,save,"AddUnlockedEquipment",{{"ID",8},{"Unlocked",1}});
    signature(host_,statics,"GetAllActorsOfClass",{{"WorldContextObject",8},{"ActorClass",8},{"OutActors",16}});
    // Complete discovery before changing progression. Only enumerate the
    // player's world; global object scans include other worlds and defaults.
    const auto catalog=host_.call(library,"GetShellsIDsTagContainer",Json::array({pc}));
    const auto& entries=catalog.at("GameplayTags");
    if(!entries.is_array() || entries.empty() || entries.size()>128) throw std::runtime_error("The game's shell catalog is unavailable or exceeds bounds.");
    std::map<std::string,Json> tags;
    for(const auto& tag:entries) tags.emplace(tag_name(tag),tag);
    auto actors=[&](const char* path) {
        const auto type=host_.request({{"op","load"},{"path",path}});id(type);
        const auto found=host_.call(statics,"GetAllActorsOfClass",{{"WorldContextObject",pawn},{"ActorClass",type}});
        const auto& list=found.at("OutActors");
        if(!list.is_array() || list.size()>128) throw std::runtime_error("Streamed shell actors exceed the batch limit.");
        Json result=Json::array();std::set<uint64_t> seen;
        for(const auto& actor:list) if(seen.insert(id(actor)).second) result.push_back(actor);
        return result;
    };
    const auto pickups=actors("/Game/Sparta/Core/Interaction/Equipment/BP_Interactable_Shell_Locked.BP_Interactable_Shell_Locked_C");
    const auto summons=actors("/Game/Sparta/Characters/SpawnSystem/LevelScripts/BP_InteractibleShellSummon.BP_InteractibleShellSummon_C");
    std::vector<std::pair<Json,Json>> locked;
    for(const auto& actor:pickups) {
        const auto unlocked=host_.get(actor,"ShellUnlocked");
        if(!unlocked.is_boolean()) throw std::runtime_error("Unsupported streamed shell unlock flag.");
        const auto tag=host_.get(actor,"ShellId");tags.emplace(tag_name(tag),tag);
        if(!unlocked.get<bool>()) locked.emplace_back(actor,tag);
    }
    if(tags.size()>128) throw std::runtime_error("Combined shell catalog exceeds the batch limit.");
    for(const auto& summon:summons) signature(host_,summon,"CheckForShellUnlock",{});
    equipment_state(host_,save); // Validate the verification interface before any write.
    auto still_current=[&] {
        const auto now=require_player();
        if(id(now.at("pawn"))!=id(pawn) || id(now.at("controller"))!=id(pc) || id(host_.get(pc,"PlayerSaveGameObject"))!=id(save))
            throw std::runtime_error("The player or save changed during shell unlock.");
    };
    still_current();
    unsigned completed=0;
    try {
        // The controller owns its debug-shell range. Do not replay the old
        // Lua script's hard-coded 0..14 indices on an unknown game build.
        host_.call(pc,"S_UnlockAllShells");++completed;
        for(const auto& [name,tag]:tags) {
            still_current();host_.call(equipment,"UnlockShell",Json::array({tag,true,pc}));++completed;
            still_current();host_.request({{"op","call"},{"target",save},{"function","AddUnlockedEquipment"},
                {"args",{{"ID",tag},{"Unlocked",true}}},{"outputs",Json::array()}});++completed;
        }
        still_current();const auto actual=equipment_state(host_,save);
        for(const auto& [name,tag]:tags) if(!actual.contains(name) || !actual.at(name))
            throw std::runtime_error("The game did not record the shell unlock: "+name);
        for(const auto& [actor,tag]:locked) {
            still_current();
            if(host_.get(actor,"ShellId")!=tag) throw std::runtime_error("A streamed shell pickup changed during unlock.");
            host_.set(actor,"ShellUnlocked",true);++completed;
            if(host_.get(actor,"ShellUnlocked")!=true) throw std::runtime_error("The streamed shell pickup did not retain its unlock.");
        }
        for(const auto& summon:summons) {still_current();host_.call(summon,"CheckForShellUnlock");++completed;}
    } catch(const std::exception& error) {
        throw std::runtime_error("Shell unlock stopped after "+std::to_string(completed)+" completed steps. Changes may already be saved, including the failed step. Check your shells before retrying. "+error.what());
    }
    report(std::to_string(tags.size())+" shell unlocks verified; "+std::to_string(locked.size())+" streamed pickups updated and "+std::to_string(summons.size())+" summons refreshed. The game can save these changes.");
}
}

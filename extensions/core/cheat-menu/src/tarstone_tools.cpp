#include "cheat_menu.hpp"

namespace cheat {
namespace {
constexpr const char* categories[]{"Melee","Sidearm","Support"};
std::string table_path(const std::string& category) {
    return "/Game/Sparta/Core/Tarstones/"+category+"/DT_Tarstones_"+category+".DT_Tarstones_"+category;
}
Json soft_field(const Json& table,const std::string& row) {
    return {{"$table_field",{{"table",table},{"row",row},{"field","ItemClass"}}}};
}
void require_signature(const cssx::Client& host,const Json& target,const char* function,
                       std::initializer_list<std::pair<const char*,int>> fields) {
    const auto args=host.request({{"op","describe"},{"target",target},{"function",function}});
    if(!args.is_object() || args.size()!=fields.size()) throw std::runtime_error(std::string("Unsupported game signature: ")+function);
    for(const auto& [name,size]:fields)
        if(!args.contains(name) || args.at(name).at("size")!=size)
            throw std::runtime_error(std::string("Unsupported game parameter: ")+function+"."+name);
}
bool owns(const cssx::Client& host,const Json& source,const Json& item) {
    const auto map=host.get(source,"TarstoneLevels");
    const auto& entries=map.at("$map");
    if(!entries.is_array() || entries.size()>4096) throw std::runtime_error("Tarstone ownership data is unavailable.");
    bool found=false;
    for(const auto& entry:entries) {
        const auto& key=entry.at("key");const auto& value=entry.at("value");
        if(!key.is_object() || !key.contains("$object") || !value.at("Level").is_number_integer())
            throw std::runtime_error("Tarstone ownership data has an unsupported shape.");
        if(key.at("$object")==item.at("$object")) found=true;
    }
    return found;
}
}
void Menu::refresh_tarstones() {
    const auto player=require_player();const auto pawn=player.at("pawn");
    const auto library=host_.request({{"op","class_default"},{"class","BPFL_Tarstones_C"}});
    require_signature(host_,library,"BuildTarstoneName",{{"SoftTarstone",40},{"IncludeLevel",1},{"__WorldContext",8},{"ReturnValue",16}});
    Json options=Json::array();
    for(const auto* category:categories) {
        const auto table=host_.find(table_path(category));
        const auto rows=host_.request({{"op","table.rows"},{"target",table}});
        if(!rows.is_array() || rows.size()>512 || options.size()+rows.size()>512)
            throw std::runtime_error("Tarstone catalog exceeds 512 entries.");
        for(const auto& row:rows) {
            const auto id=row.get<std::string>();
            const auto label=host_.call(library,"BuildTarstoneName",Json::array({soft_field(table,id),false,pawn}));
            if(!label.is_string() || label.get_ref<const std::string&>().empty()) throw std::runtime_error("Tarstone name is unavailable.");
            options.push_back({{"id",std::string(category)+"/"+id},{"label",std::string(category)+": "+label.get<std::string>()}});
        }
    }
    if(options.empty()) throw std::runtime_error("The game returned an empty Tarstone catalog.");
    tarstones_=std::move(options);
    bool selected=false;for(const auto& option:tarstones_) if(option["id"]==values_["tarstone"]) selected=true;
    if(!selected) values_["tarstone"]=tarstones_[0]["id"];
    applied_["tarstone"]=values_["tarstone"];
    report(std::to_string(tarstones_.size())+" Tarstones available. Search by the name shown in the game.");
}
void Menu::tarstone_action(const std::string& id,const Json& player) {
    const auto pawn=player.at("pawn"),pc=player.at("controller");
    if(id!="add_tarstone") {
        std::string function;
        if(id=="give_tarstones_melee") function="S_AddAllTarstonesMelee";
        else if(id=="give_tarstones_sidearm") function="S_AddAllTarstonesSidearm";
        else if(id=="give_tarstones_support") function="S_AddAllTarstonesSupport";
        else throw std::runtime_error("Unknown Tarstone action.");
        require_signature(host_,pc,function.c_str(),{});
        host_.call(pc,function);report("Tarstone category grant requested. The game can save the change.");return;
    }
    const auto selected=values_.at("tarstone").get<std::string>();
    bool valid=false;for(const auto& option:tarstones_) if(option["id"]==selected) valid=true;
    const auto slash=selected.find('/');
    if(!valid || slash==std::string::npos) throw std::runtime_error("Refresh the catalog and choose a Tarstone.");
    const auto category=selected.substr(0,slash),row=selected.substr(slash+1);
    const auto table=host_.find(table_path(category)),soft=soft_field(table,row);
    const auto utility=host_.find("/Game/Sparta/Core/Utility/BPFL_Utility.Default__BPFL_Utility_C");
    const auto item=host_.call(utility,"ResolveSoftItemDefinition",Json::array({soft,pawn}));
    if(!item.is_object() || !item.contains("$object")) throw std::runtime_error("The selected Tarstone class is unavailable.");
    const auto component=host_.get(pawn,"TarstoneComponent");
    const auto runtime=host_.call(component,"GetRuntimeData");
    // Check both copies before granting. Newly acquired stones may not be in
    // the runtime save copy yet. Never reset an owned stone to level zero.
    const bool component_owns=owns(host_,component,item),runtime_owns=owns(host_,runtime,item);
    if(component_owns || runtime_owns) {report("That Tarstone is already owned. Its level and experience were kept.");return;}
    const auto library=host_.request({{"op","class_default"},{"class","BPFL_Player_C"}});
    const auto save=host_.request({{"op","class_default"},{"class","BPFL_SaveLoad_C"}});
    // Resolve every required interface before the first inventory mutation.
    require_signature(host_,library,"AddTarstoneToItemManager",{{"Tarstone",8},{"Silent",1},{"__WorldContext",8}});
    require_signature(host_,component,"AddTarstoneToInventory",{{"Tarstone",8},{"Level",4},{"exp",4}});
    require_signature(host_,save,"UpdateSoftItemStatus",{{"SoftItem",40},{"__WorldContext",8}});
    require_signature(host_,component,"CacheAllTarstones",{{"Completed",1}});
    try {
        host_.call(library,"AddTarstoneToItemManager",Json::array({item,false,pawn}));
        // The manager may already have registered it. Do not duplicate that
        // write or overwrite level data supplied by the game's receive event.
        if(!owns(host_,component,item)) host_.call(component,"AddTarstoneToInventory",Json::array({item,0,0}));
        host_.request({{"op","call"},{"target",save},{"function","UpdateSoftItemStatus"},
                       {"args",{{"SoftItem",soft},{"__WorldContext",pawn}}},{"outputs",Json::array()}});
        host_.call(component,"CacheAllTarstones");
        if(!owns(host_,component,item) && !owns(host_,runtime,item)) throw std::runtime_error("The game did not report ownership after the grant.");
    } catch(const std::exception& error) {
        throw std::runtime_error(std::string("Tarstone grant stopped; part of it may already be saved. Check Inventory before retrying. ")+error.what());
    }
    report("Tarstone added and ownership verified. The game can save this change.");
}
void Menu::set_tarstone_levels(const Json& player) {
    const auto pawn=player.at("pawn"),component=host_.get(pawn,"TarstoneComponent");
    const auto runtime=host_.call(component,"GetRuntimeData");
    const auto scope=values_.at("tarstone_scope").get<std::string>();
    const int level=values_.at("tarstone_level").get<int>()-1;
    Json selected;
    if(scope=="selected") {
        const auto id=values_.at("tarstone").get<std::string>();const auto slash=id.find('/');
        if(slash==std::string::npos) throw std::runtime_error("Choose a Tarstone first.");
        bool valid=false;for(const auto& option:tarstones_) if(option["id"]==id) valid=true;
        if(!valid) throw std::runtime_error("Refresh the Tarstone catalog first.");
        const auto table=host_.find(table_path(id.substr(0,slash)));
        const auto utility=host_.find("/Game/Sparta/Core/Utility/BPFL_Utility.Default__BPFL_Utility_C");
        selected=host_.call(utility,"ResolveSoftItemDefinition",Json::array({soft_field(table,id.substr(slash+1)),pawn}));
        if(!selected.is_object() || !selected.contains("$object")) throw std::runtime_error("The selected Tarstone is unavailable.");
    }
    struct Edit {Json owner,key,before,after;};
    std::vector<Edit> edits;
    std::map<uint64_t,Json> payloads;
    // Prefer component data for equipped instances while updating both save
    // copies. Each entry retains its own non-level fields.
    for(const auto& source:{runtime,component}) {
        const auto data=host_.get(source,"TarstoneLevels");const auto& entries=data.at("$map");
        if(!entries.is_array() || entries.size()>4096) throw std::runtime_error("Tarstone level map exceeds bounds.");
        for(const auto& entry:entries) {
            const auto& key=entry.at("key");const auto object=key.at("$object").get<uint64_t>();
            if(scope=="selected" && object!=selected.at("$object").get<uint64_t>()) continue;
            if(scope!="selected" && scope!="all" && key.value("name",std::string{}).find("/Tarstones/"+scope+"/")==std::string::npos) continue;
            const auto before=entry.at("value");
            if(!before.is_object() || before.size()!=4) throw std::runtime_error("Unsupported Tarstone level data.");
            for(const auto* field:{"Level","exp","Stacks","Durability"}) if(!before.contains(field) || !before[field].is_number_integer()) throw std::runtime_error("Unsupported Tarstone level field.");
            if(before["Level"].get<int>()<0 || before["Level"].get<int>()>2) throw std::runtime_error("Tarstone uses an unsupported level range.");
            auto after=before;after["Level"]=level;payloads[object]=after;
            if(after!=before) edits.push_back({source,key,before,after});
        }
    }
    if(payloads.empty()) throw std::runtime_error("No owned Tarstones match this selection. Add a Tarstone before editing its level.");
    if(payloads.size()>512 || edits.size()>1024) throw std::runtime_error("Tarstone edit exceeds the batch limit.");
    if(edits.empty()) {report("Matching Tarstones are already at that level. No changes made.");return;}
    std::vector<std::pair<Json,Json>> equipped;
    std::set<uint64_t> instances;
    for(const auto* name:{"EquippedTarstoneItemInstances","EquippedSupportTarstoneItemInstances"}) {
        const auto data=host_.get(component,name);const auto& entries=data.at("$map");
        if(!entries.is_array() || entries.size()>128) throw std::runtime_error("Equipped Tarstone map exceeds bounds.");
        for(const auto& entry:entries) {
            const auto key=entry.at("key").at("$object").get<uint64_t>();
            if(!payloads.contains(key) || entry.at("value").is_null()) continue;
            const auto instance=entry.at("value");
            if(instances.insert(instance.at("$object").get<uint64_t>()).second) equipped.emplace_back(instance,payloads.at(key));
        }
    }
    if(!equipped.empty()) require_signature(host_,component,"SetTarstoneLevel",{{"Tarstone",8},{"LevelData",16}});
    auto update=[&](const Edit& edit,bool undo) {
        return host_.request({{"op","map.update"},{"target",edit.owner},{"property","TarstoneLevels"},
                              {"key",edit.key},{"expected",undo?edit.after:edit.before},{"value",undo?edit.before:edit.after}});
    };
    size_t completed=0;
    try {for(const auto& edit:edits) {update(edit,false);++completed;}}
    catch(const std::exception& error) {
        const std::string original=error.what();bool restored=true;
        while(completed) {try {update(edits[--completed],true);} catch(...) {restored=false;}}
        throw std::runtime_error("Tarstone edit stopped: "+original+(restored?". Earlier writes were restored.":". Some values changed again and could not be restored; inspect Inventory."));
    }
    try {
        for(const auto& [instance,payload]:equipped) host_.call(component,"SetTarstoneLevel",Json::array({instance,payload}));
        for(const auto& source:{runtime,component}) {
            const auto data=host_.get(source,"TarstoneLevels");
            std::map<uint64_t,Json> actual;
            for(const auto& entry:data.at("$map")) actual.emplace(entry.at("key").at("$object").get<uint64_t>(),entry.at("value"));
            for(const auto& edit:edits) if(edit.owner==source) {
                const auto key=edit.key.at("$object").get<uint64_t>();
                if(!actual.contains(key) || actual.at(key)!=edit.after) throw std::runtime_error("Saved Tarstone data changed during equipped refresh.");
            }
        }
    } catch(const std::exception& error) {
        // An equipped-item call can have game-owned side effects. Do not issue
        // it twice or pretend a save-data rollback undoes those effects.
        throw std::runtime_error(std::string("Saved levels changed, but an equipped Tarstone refresh failed. Check Inventory before retrying. ")+error.what());
    }
    report(std::to_string(payloads.size())+" owned Tarstones set to level "+std::to_string(level+1)+". Experience, durability and stacks kept.");
}
}

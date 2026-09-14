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
}

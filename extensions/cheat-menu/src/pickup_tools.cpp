#include "cheat_menu.hpp"
#include <cctype>
namespace cheat {
namespace {
constexpr const char* table_path="/Game/Sparta/Items/Pickups/Core/DT_PickUpItems.DT_PickUpItems";
std::string pickup_label(const std::string& id) {
    std::string label;
    for(size_t i=0;i<id.size();++i) {
        const auto c=static_cast<unsigned char>(id[i]);
        if(c=='_') {label+=' ';continue;}
        if(i && std::isupper(c) && std::islower(static_cast<unsigned char>(id[i-1]))) label+=' ';
        label+=char(c);
    }
    return label;
}
}
void Menu::refresh_pickups() {
    require_player();
    const auto table=host_.find(table_path);
    const auto rows=host_.request({{"op","table.rows"},{"target",table}});
    if(!rows.is_array() || rows.empty() || rows.size()>512) throw std::runtime_error("Pickup catalog is unavailable or exceeds 512 entries.");
    Json options=Json::array();for(const auto& row:rows) {
        const auto id=row.get<std::string>();
        options.push_back({{"id",id},{"label",pickup_label(id)}});
    }
    pickup_table_=table;pickups_=std::move(options);
    bool selected=false;for(const auto& option:pickups_) if(option["id"]==values_["pickup"]) selected=true;
    if(!selected) values_["pickup"]=pickups_[0]["id"];
    applied_["pickup"]=values_["pickup"];
    report(std::to_string(pickups_.size())+" pickups available. Select an item, then Add or Remove.");
}
Json Menu::pickup_class(const Json& pawn) {
    const auto row=values_.at("pickup").get<std::string>();
    if(row=="none") throw std::runtime_error("Refresh the pickup list and choose an item.");
    // Resolve the current table field on each use. No cached soft-pointer
    // layout, raw UClass coercion, or stale row buffer survives a transition.
    const auto table=host_.find(table_path);
    const auto soft=Json{{"$table_field",{{"table",table},{"row",row},{"field","ItemClass"}}}};
    const auto utility=host_.find("/Game/Sparta/Core/Utility/BPFL_Utility.Default__BPFL_Utility_C");
    const auto item=host_.call(utility,"ResolveSoftItemDefinition",Json::array({soft,pawn}));
    if(!item.is_object() || !item.contains("$object")) throw std::runtime_error("The selected pickup class is unavailable.");
    return item;
}
}

#include "cheat_menu.hpp"
#include <cctype>

namespace cheat {
namespace {
bool handle(const Json& value) {return value.is_object() && value.contains("$object");}
std::string class_name(const Json& value) {
    auto name=value.value("class",std::string{});const auto dot=name.rfind('.');return dot==std::string::npos?name:name.substr(dot+1);
}
}
Json Menu::owned_abilities(const Json& pawn) {
    const auto asc=host_.get(pawn,"AbilitySystemComponent");
    const auto items=host_.get(asc,"ActivatableAbilities").at("Items");
    if(!items.is_array() || items.size()>512) throw std::runtime_error("Player ability list exceeds supported bounds.");
    Json result=Json::array();std::set<uint64_t> seen;
    for(const auto& item:items) for(const auto* field:{"NonReplicatedInstances","ReplicatedInstances"}) {
        const auto instances=item.value(field,Json::array());
        if(!instances.is_array() || instances.size()>32) throw std::runtime_error("Player ability instance list exceeds supported bounds.");
        for(const auto& instance:instances) if(handle(instance) && seen.insert(instance.at("$object").get<uint64_t>()).second) {
            if(result.size()>=512) throw std::runtime_error("Too many player ability instances.");
            result.push_back(instance);
        }
    }
    return result;
}
void Menu::combat_hook(const std::string& feature,const Json& target,const Json& spec) {
    const auto key=feature+":"+std::to_string(target.at("$object").get<uint64_t>())+":"+spec.at("function").get<std::string>();
    if(combat_hooks_.contains(key)) return;
    auto request=spec;request["op"]="hooks.add";request["target"]=target;
    const auto player=require_player();request["pawn"]=player.at("pawn");request["controller"]=player.at("controller");
    // Allocate the ownership record before registration. Cleanup can find a
    // successful hook even if a later registration or setting fails.
    auto it=combat_hooks_.emplace(key,OwnedHook{0,feature,target}).first;
    try {it->second.id=host_.request(request).get<uint64_t>();}
    catch(...) {combat_hooks_.erase(it);throw;}
}
void Menu::combat_clear(const std::string& feature) {
    if(feature.empty()) armed_.clear();
    for(auto it=combat_hooks_.begin();it!=combat_hooks_.end();) {
        if(!feature.empty() && it->second.feature!=feature) {++it;continue;}
        try {
            host_.request({{"op","hooks.remove"},{"id",it->second.id}});
        } catch(const std::exception& error) {
            const std::string err=error.what();
            if(err.find("pending")!=std::string::npos || err.find("busy")!=std::string::npos || err.find("executing")!=std::string::npos) throw;
        }
        it=combat_hooks_.erase(it);
    }
    if(feature.empty() || feature=="no_cooldown") {
        for(const auto* field:cooldown_fields) restore(field);
        cooldown_seen_.clear();
    }
}
void Menu::combat_sync() {
    bool active=false;
    for(const auto* id:combat_ids) {
        if(applied_.at(id)==true) active=true;
        else { combat_clear(id); if(armed_.erase(id)) host_.request({{"op","invalidate"}}); }
    }
    if(!active) return;
    auto status=host_.request({{"op","hooks.status"}});
    if(!status.value("available",false)) throw std::runtime_error("Install the complete updated CSS package and restart to use combat cheats.");
    for(const auto& rule:status.value("rules",Json::array())) if(rule.value("failed",false))
        throw std::runtime_error("A combat hook failed its runtime checks. Turn off all cheats before retrying.");
    const auto player=require_player();
    // Seal checks are cheap and must run every sync: a seal change disables
    // the matching cheat at once, before the throttled ability scan below.
    struct SealCheat {const char* id;const char* seal;const char* ability;};
    constexpr SealCheat seal_cheats[]={{"perfect_parry","ID_Seal_Infinite_C","GA_Parry_Handler_C"},
        {"perfect_block","ID_Seal_Default_C","GA_ActiveBlock_C"},{"perfect_harden","ID_Seal_Stone_C","GA_Harden_Original_C"}};
    // In-game names (ST_Core_Seals): Untarnished = guard, Infinite = parry, Vatra's = harden.
    auto seal_label=[](const std::string& seal){
        if(seal=="ID_Seal_Default_C") return std::string("Untarnished Seal (guard)");
        if(seal=="ID_Seal_Infinite_C") return std::string("Infinite Seal (parry)");
        if(seal=="ID_Seal_Stone_C") return std::string("Vatra's Seal (harden)");
        auto s=seal; if(s.starts_with("ID_Seal_")) s=s.substr(8); if(s.ends_with("_C")) s.resize(s.size()-2); return s+" Seal"; };
    std::set<std::string> waiting;
    for(const auto& cheat:seal_cheats) {
        if(applied_[cheat.id]!=true) { armed_.erase(cheat.id); continue; }
        const auto item=host_.get(player.at("controller"),"ActiveSealItemHandle");
        const auto definition=handle(item)?host_.get(item,"ItemDef"):item.value("ItemDef",Json());
        const auto name=definition.value("name",std::string{});
        const auto dot=name.rfind('.');
        if(dot==std::string::npos || name.substr(dot+1)!=cheat.seal) {
            // Not an error: the cheat waits, hook-free, until that seal is on.
            combat_clear(cheat.id); waiting.insert(cheat.id);
            const auto note=pretty(cheat.id)+" waits for the "+seal_label(cheat.seal)+".";
            if(armed_[cheat.id]!=note) { armed_[cheat.id]=note; host_.request({{"op","invalidate"}}); }
        } else if(armed_.erase(cheat.id)) host_.request({{"op","invalidate"}});
    }
    // Decoding every ability instance costs several milliseconds. Between full
    // syncs (every 10 s) only re-decode when the list length changed.
    {
        size_t count=abilities_count_;
        try { const auto asc=host_.get(player.at("pawn"),"AbilitySystemComponent"); const auto shallow=host_.request({{"op","get"},{"target",asc},{"property","ActivatableAbilities"},{"count",true}}); count=shallow.value("count",count); } catch(...) {}
        // Re-decode only when the ability list changed or work was left over
        // from the last pass; the old forced full pass every 10 s cost
        // 20-40 ms per pass with 100+ abilities.
        if(count==abilities_count_ && !combat_backlog_ && !combat_hooks_.empty()) return;
        abilities_count_=count; combat_backlog_=false;
    }
    const auto abilities=owned_abilities(player.at("pawn"));
    std::set<uint64_t> live;for(const auto& ability:abilities) live.insert(ability.at("$object").get<uint64_t>());
    // Remove rules for abilities the player no longer owns, even if another
    // engine object still keeps the old instance alive.
    bool lost_cooldown=false;
    for(auto it=combat_hooks_.begin();it!=combat_hooks_.end();) {
        if(live.contains(it->second.target.at("$object").get<uint64_t>())) {++it;continue;}
        if(it->second.feature=="no_cooldown") lost_cooldown=true;
        try {
            host_.request({{"op","hooks.remove"},{"id",it->second.id}});
        } catch(const std::exception& error) {
            const std::string err=error.what();
            if(err.find("pending")!=std::string::npos || err.find("busy")!=std::string::npos || err.find("executing")!=std::string::npos) throw;
        }
        it=combat_hooks_.erase(it);
    }
    if(lost_cooldown) {for(const auto* field:cooldown_fields) restore(field);cooldown_seen_.clear();}
    if(applied_["no_cooldown"]==true) {
        unsigned installed=0;
        for(const auto& ability:abilities) {
            const auto id=ability.at("$object").get<uint64_t>();
            if(cooldown_seen_.contains(id)) continue;
            // A few new abilities per sync (one second apart) keeps each frame
            // short; the rest follow on the next syncs.
            if(installed>=4) { combat_backlog_=true; break; }
            ++installed;
            std::vector<std::pair<std::string,Json>> fields;
            for(const auto* field:cooldown_fields) {
                Json value;
                try {value=host_.get(ability,field);}
                catch(const std::exception& error) {
                    // Abilities differ in which cooldown fields they carry; a
                    // missing field is the normal case, not a failure.
                    std::string text=error.what(); for(auto& c:text) c=char(std::tolower((unsigned char)c));
                    if(text.find("property is missing")!=std::string::npos || text.find("is missing")!=std::string::npos) continue;
                    throw;
                }
                if(!value.is_number() || !std::isfinite(value.get<double>()))
                    throw std::runtime_error("Unsupported cooldown field; turn off combat cheats to restore prior edits.");
                fields.emplace_back(field,value.is_number_integer()?Json(0):Json(0.));
            }
            if(fields.empty()) {cooldown_seen_.insert(id);continue;}
            for(const auto* kind:{"Local","Global"}) combat_hook("no_cooldown",ability,
                {{"function",std::string("Apply")+kind+"Cooldown"},{"mode","after"},{"after",std::string("Clear")+kind+"Cooldown"}});
            for(const auto& [field,value]:fields) override_value(ability,field,value);
            host_.call(ability,"ClearLocalCooldown",Json::array({player.at("pawn")}));
            host_.call(ability,"ClearGlobalCooldown",Json::array({player.at("pawn")}));
            cooldown_seen_.insert(id);
        }
    }
    for(const auto& cheat:seal_cheats) if(applied_[cheat.id]==true && !waiting.contains(cheat.id)) {
        bool found=false;
        for(const auto& ability:abilities) if(class_name(ability)==cheat.ability) {
            found=true;
            auto add=[&](const char* function,bool value) {combat_hook(cheat.id,ability,{{"function",function},{"mode","bool"},{"value",value},{"seal",cheat.seal}});};
            if(std::string(cheat.id)=="perfect_parry") {add("IsInParryWindow",true);add("IsUnparryableAttack",false);add("IsParryingAICharacter",true);}
            else add(std::string(cheat.id)=="perfect_block"?"CanPerfectBlock":"IsInPerfectStoneForm",true);
        }
        if(!found) { const auto note=pretty(cheat.id)+" waits for the seal's ability to load."; if(armed_[cheat.id]!=note) { armed_[cheat.id]=note; host_.request({{"op","invalidate"}}); } }
        else if(armed_.erase(cheat.id)) host_.request({{"op","invalidate"}});
    }
}
}

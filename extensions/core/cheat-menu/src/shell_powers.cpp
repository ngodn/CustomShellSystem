#include "cheat_menu.hpp"

namespace cheat {
namespace {
uint64_t object_id(const Json& value) {return value.is_object()?value.value("$object",uint64_t{}):0;}
int64_t effect_id(const Json& value) {return value.is_object()?value.at("Handle").get<int64_t>():-1;}
bool effect_active(const cssx::Client& host,const Json& effect) {
    if(effect_id(effect)<=0) return false;
    const auto library=host.find("/Script/GameplayAbilities.Default__AbilitySystemBlueprintLibrary");
    return object_id(host.call(library,"GetGameplayEffectFromActiveEffectHandle",Json::array({effect})))!=0;
}
void signature(const cssx::Client& host,const Json& target,const char* function,
               std::initializer_list<std::pair<const char*,int>> fields) {
    const auto args=host.request({{"op","describe"},{"target",target},{"function",function}});
    if(!args.is_object() || args.size()!=fields.size()) throw std::runtime_error(std::string("Unsupported power signature: ")+function);
    for(const auto& [name,size]:fields) if(!args.contains(name) || args.at(name).at("size")!=size)
        throw std::runtime_error(std::string("Unsupported power parameter: ")+function+"."+name);
}
const char* ability_class(const std::string& id) {
    if(id=="smert_stance") return "GA_Smert_FightStanceHandler_C";
    if(id=="genessa_clones") return "GA_AstralClones_Action_C";
    return "GA_Lazlo_Detonation_C";
}
}
Json Menu::power_ability(const Json& player,const char* name) {
    const auto expected=std::string(name)=="GA_AstralClones_Action_C"?"Genessa":std::string(name)=="GA_Smert_FightStanceHandler_C"?"Smert":"Lazlo";
    if(!shell_matches(host_.call(player.at("pawn"),"GetCharacterID"),expected))
        throw std::runtime_error(std::string("Equip ")+expected+" as the gameplay shell before enabling this power.");
    Json result;
    for(const auto& ability:owned_abilities(player.at("pawn"))) {
        if(!ability.value("class",std::string{}).ends_with(std::string(".")+name)) continue;
        if(!result.is_null()) throw std::runtime_error("More than one player ability matches this shell power.");
        signature(host_,ability,"GetAvatarActorFromActorInfo",{{"ReturnValue",8}});
        if(object_id(host_.call(ability,"GetAvatarActorFromActorInfo"))!=object_id(player.at("pawn")))
            throw std::runtime_error("Shell power belongs to another character.");
        result=ability;
    }
    if(result.is_null()) {
        if(std::string(name)=="GA_Lazlo_Detonation_C")
            throw std::runtime_error("Lazlo's Temperament upgrade is not active on this shell. Unlock/equip that upgrade before enabling repeating shockwaves.");
        throw std::runtime_error("Equip the matching gameplay shell before enabling this power. CSS outfits do not change shell abilities.");
    }
    return result;
}
bool Menu::gameplay_ready(const Json& player) {
    try {
        if(!player.is_object() || !player.contains("pawn") || !player.contains("controller")) return false;
        if(!object_id(player.at("pawn")) || !object_id(player.at("controller"))) return false;
        if(host_.request({{"op","input.focus"}})!=true) return false;
        const auto menu=host_.request({{"op","menu.status"}});
        if(!menu.contains("menu_open") || menu.at("menu_open")!=false) return false;
        const auto pc=player.at("controller");
        for(const auto* function:{"IsInGameMenu","IsMoveInputIgnored","IsLookInputIgnored"})
            if(host_.call(pc,function)!=false) return false;
        const auto statics=host_.request({{"op","class_default"},{"class","GameplayStatics"}});
        if(host_.call(statics,"IsGamePaused",{{"WorldContextObject",player.at("pawn")}})!=false) return false;
        const auto health=host_.get(player.at("pawn"),"HealthComponent");
        if(!health.is_object() || !object_id(health)) return false;
        try {
            const auto dying=host_.call(health,"IsDeadOrDying");
            if(dying.is_boolean() && dying.get<bool>()) return false;
        } catch(...) {}
        try {
            const auto dead=host_.call(player.at("pawn"),"IsDead");
            if(dead.is_boolean() && dead.get<bool>()) return false;
        } catch(...) {}
        const auto value=host_.call(health,"GetHealth");
        if(!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>()<=0) return false;
        try {
            const auto shell_hp=host_.call(health,"GetShellHealth");
            const auto shell_max=host_.call(health,"GetMaxShellHealth");
            if(shell_max.is_number() && shell_max.get<double>()>0) {
                if(shell_hp.is_number() && shell_hp.get<double>()<=0) return false;
            }
        } catch(...) {}
        return true;
    } catch(...) {
        return false;
    }
}
void Menu::power_sync() {
    for(const auto* feature:power_ids) {
        if(applied_.at(feature)!=true) {power_clear(feature);continue;}
        if(powers_.contains(feature)) continue;
        const auto player=require_player();const auto ability=power_ability(player,ability_class(feature));
        Power power;power.pawn=player.at("pawn");power.controller=player.at("controller");power.ability=ability;
        const std::string name=feature;
        if(name=="genessa_clones") {
            for(const auto* function:{"SpawnPrimaryClone","SpawnSecondaryClone","RemovePrimaryClone","RemoveSecondaryClone"}) signature(host_,ability,function,{});
            signature(host_,ability,"HasSecondaryClone",{{"ReturnValue",1}});
            if(!host_.request({{"op","hooks.status"}}).value("available",false))
                throw std::runtime_error("Persistent clones need the updated CSS loader. Install the complete CSS package and restart.");
            const auto count=host_.get(ability,"SpawnCount");
            if(!count.is_number_integer() || count.get<int>()<0 || count.get<int>()>9999) throw std::runtime_error("Unsupported clone count.");
            if(!host_.get(ability,"CurrentPrimaryClone").is_null() || !host_.get(ability,"CurrentSecondaryClone").is_null())
                throw std::runtime_error("Existing Genessa clones are active. Let them finish before enabling persistent clones.");
        } else if(name=="smert_stance") {
            signature(host_,ability,"EnableFightStance",{});
            signature(host_,ability,"RemovePermanentFightStance",{{"Immediate",1}});
            const auto effect=host_.get(ability,"GE_FightStanceActive");
            if(effect_active(host_,effect)) throw std::runtime_error("Smert's stance is already active. Leave it before enabling this cheat.");
        } else signature(host_,ability,"TriggerLastShockwave",{});
        powers_.emplace(feature,std::move(power));
        if(name=="genessa_clones") {
            combat_hook(feature,ability,{{"function","HasSecondaryClone"},{"mode","bool"},{"value",true}});
            override_value(ability,"SpawnCount",9999);
        }
    }
}
void Menu::power_clear(const std::string& feature) {
    for(auto it=powers_.begin();it!=powers_.end();) {
        if(!feature.empty() && it->first!=feature) {++it;continue;}
        auto& power=it->second;
        if(host_.request({{"op","valid"},{"target",power.ability}})==true) {
            auto finish_pending=[&](bool& pending,Json& owned,const char* field,bool effect=false) {
                if(!pending) return;
                const auto current=host_.get(power.ability,field);
                if(effect?effect_active(host_,current):object_id(current)!=0) {owned=current;pending=false;}
            };
            finish_pending(power.primary_pending,power.primary,"CurrentPrimaryClone");
            finish_pending(power.secondary_pending,power.secondary,"CurrentSecondaryClone");
            finish_pending(power.stance_pending,power.effect,"GE_FightStanceActive",true);
            if(power.primary_pending || power.secondary_pending || power.stance_pending)
                throw std::runtime_error("The game is still creating a shell power. Resume gameplay briefly, then retry Turn off all cheats. Cleanup retains the pending request.");
            if(it->first=="genessa_clones" && power.attempted) {
                for(auto side:{std::pair{"CurrentPrimaryClone","RemovePrimaryClone"},std::pair{"CurrentSecondaryClone","RemoveSecondaryClone"}}) {
                    auto& owned=std::string(side.first)=="CurrentPrimaryClone"?power.primary:power.secondary;
                    if(!object_id(owned)) continue;
                    const auto current=host_.get(power.ability,side.first);
                    if(object_id(current)==object_id(owned)) {
                        host_.call(power.ability,side.second);
                        if(object_id(host_.get(power.ability,side.first))==object_id(owned))
                            throw std::runtime_error("Clone removal has not completed. Turn off all cheats to retry cleanup.");
                    }
                    // Another clone can replace ours after a legitimate game
                    // cast. Do not remove that newer actor through this slot.
                    owned=nullptr;
                }
            } else if(it->first=="smert_stance" && effect_id(power.effect)>0) {
                const auto current=host_.get(power.ability,"GE_FightStanceActive");
                if(effect_id(current)==effect_id(power.effect) && effect_active(host_,power.effect)) {
                    host_.call(power.ability,"RemovePermanentFightStance",Json::array({true}));
                    if(effect_active(host_,power.effect))
                        throw std::runtime_error("Stance removal has not completed. Turn off all cheats to retry cleanup.");
                }
            }
        }
        if(it->first=="genessa_clones") {combat_clear("genessa_clones");restore("SpawnCount");}
        it=powers_.erase(it);
    }
}
void Menu::power_tick(double delta) {
    if(powers_.empty()) return;
    const auto player=require_player();
    // Shell changes can retain the pawn. Validate both avatar and ability
    // membership each update, not just the pawn pointer.
    const auto abilities=owned_abilities(player.at("pawn"));std::set<uint64_t> live;
    for(const auto& ability:abilities) live.insert(object_id(ability));
    for(auto it=powers_.begin();it!=powers_.end();) {
        const auto feature=it->first;auto& power=it->second;++it;
        try {
            if(object_id(player.at("pawn"))!=object_id(power.pawn) || object_id(player.at("controller"))!=object_id(power.controller) || !live.contains(object_id(power.ability)))
                throw std::runtime_error("Shell power stopped because the player or ability changed.");
            if(object_id(host_.call(power.ability,"GetAvatarActorFromActorInfo"))!=object_id(power.pawn))
                throw std::runtime_error("Shell power avatar changed.");
            const auto expected=feature=="genessa_clones"?"Genessa":feature=="smert_stance"?"Smert":"Lazlo";
            if(!shell_matches(host_.call(player.at("pawn"),"GetCharacterID"),expected)) throw std::runtime_error("Gameplay shell changed. Its previous power was stopped.");
            if(!gameplay_ready(player)) {power.elapsed=0;continue;}
            power.elapsed+=delta;
            if(feature=="lazlo_detonation") {
                if(power.elapsed<applied_.at("shockwave_interval").get<double>()) continue;
                power.elapsed=0;host_.call(power.ability,"TriggerLastShockwave");continue;
            }
            if(power.attempted) {
                if(power.primary_pending) {power.primary=host_.get(power.ability,"CurrentPrimaryClone");power.primary_pending=!object_id(power.primary);}
                if(power.secondary_pending) {power.secondary=host_.get(power.ability,"CurrentSecondaryClone");power.secondary_pending=!object_id(power.secondary);}
                if(power.stance_pending) {power.effect=host_.get(power.ability,"GE_FightStanceActive");power.stance_pending=!effect_active(host_,power.effect);}
                if(power.primary_pending || power.secondary_pending || power.stance_pending) {
                    if(power.elapsed>5) throw std::runtime_error("Shell power creation has not completed. No repeat request was sent.");
                    continue;
                }
                if(!power.started) {power.started=true;report(feature=="genessa_clones"?"Genessa's two persistent clones are active.":"Smert's fight stance is active.");}
                continue;
            }
            if(power.elapsed<.3) continue;
            if(feature=="genessa_clones") {
                if(!host_.get(power.ability,"CurrentPrimaryClone").is_null() || !host_.get(power.ability,"CurrentSecondaryClone").is_null())
                    throw std::runtime_error("A game clone appeared before activation. Persistent clones were cancelled.");
                if(host_.get(power.ability,"SpawnCount")!=9999) throw std::runtime_error("Another writer changed the clone count before activation.");
                if(host_.call(power.ability,"HasSecondaryClone")!=true)
                    throw std::runtime_error("The paired-clone override is unavailable. No clones were requested.");
                power.attempted=true;
                // SpawnPrimaryClone schedules SpawnSecondaryClone for the
                // next game tick. Track both results, but send one request.
                // HasSecondaryClone is overridden on this owned instance so
                // the pair does not require a permanent upgrade/save edit.
                power.secondary_pending=true;
                auto spawn=[&](const char* function,const char* field,Json& owned,bool& pending) {
                    pending=true;
                    try {host_.call(power.ability,function);}
                    catch(...) {owned=host_.get(power.ability,field);pending=!object_id(owned);throw;}
                    owned=host_.get(power.ability,field);pending=!object_id(owned);
                };
                spawn("SpawnPrimaryClone","CurrentPrimaryClone",power.primary,power.primary_pending);
                power.secondary=host_.get(power.ability,"CurrentSecondaryClone");
                power.secondary_pending=!object_id(power.secondary);
            } else {
                if(effect_active(host_,host_.get(power.ability,"GE_FightStanceActive"))) throw std::runtime_error("The game activated Smert's stance before the cheat. It was left unchanged.");
                power.attempted=true;power.stance_pending=true;
                try {host_.call(power.ability,"EnableFightStance");}
                catch(...) {power.effect=host_.get(power.ability,"GE_FightStanceActive");power.stance_pending=!effect_active(host_,power.effect);throw;}
                power.effect=host_.get(power.ability,"GE_FightStanceActive");power.stance_pending=!effect_active(host_,power.effect);
            }
            power.elapsed=0;
            power.started=!power.primary_pending && !power.secondary_pending && !power.stance_pending;
            report(power.started?(feature=="genessa_clones"?"Genessa's two persistent clones are active.":"Smert's fight stance is active."):"Waiting for the game to finish creating the shell power.");
        } catch(const std::exception& error) {
            applied_[feature]=values_[feature]=false;
            const std::string reason=error.what();
            try {power_clear(feature);}
            catch(const std::exception& cleanup) {cleanup_required_=true;action_error_=reason+" Cleanup needs retry: "+cleanup.what();report(action_error_);return;}
            action_error_=reason;report(reason);
        }
    }
}
}

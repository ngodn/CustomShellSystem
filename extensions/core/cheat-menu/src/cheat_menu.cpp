#include "cheat_menu.hpp"
#include <cctype>

namespace cheat {
namespace {
Json movement_speeds(const Json& value) {
    Json result=Json::object();
    for(const auto* field:{"WalkSpeed","JogSpeed","SprintSpeed"}) result[field]=value.at(field);
    return result;
}
struct Action {const char* id;const char* function;};
constexpr Action grants[]={{"gold","S_AddGold"},{"gloom","S_AddGloom"},{"glimpses","S_AddGlimpses"},
    {"shell_points","S_AddShellPoints"},{"tarcores","S_AddTarcores"},{"ventrium","S_AddVentrium"},
    {"laterite","S_AddLaterite"},{"dorsalite","S_AddDorsalite"},{"thoracium","S_AddThoracium"},{"ovums","S_AddOvums"}};
constexpr Action unlocks[]={{"clothing","S_UnlockAllClothing"},{"gates","S_UnlockAllGates"},
    {"landing","S_UnlockAllLandingAreas"},{"masks","S_UnlockAllMasks"},{"seals","S_UnlockAllSeals"},
    {"sidearms","S_UnlockAllSidearms"},{"weapons","S_UnlockAllWeapons"},{"shell_shades","S_UnlockShellShades"},
    {"red_harbinger","S_UnlockRedHarbinger"},{"cosmic_harbinger","S_UnlockCosmicHarbinger"},
    {"dark_shades","S_UnlockDarkFormShades"},{"map","S_UnlockMap"},{"fast_travel","S_UnlockFastTravel"},
    {"reveal_map","S_MapReveal_All"}};
std::string normalized(const std::string& text) {
    std::string result;for(unsigned char c:text) if(std::isalnum(c)) result+=char(std::tolower(c));return result;
}
uint64_t identity(const Json& handle) {return handle.is_object()?handle.value("$object",uint64_t{}):0;}
}
bool Menu::shell_matches(const Json& tag,const std::string& target) const {
    const auto value=tag.is_object()?tag.value("TagName",std::string{}):tag.is_string()?tag.get<std::string>():std::string{};
    const auto text=normalized(value),needle=normalized(target);
    if(!needle.empty() && text.find(needle)!=std::string::npos) return true;
    const auto token=shell_tokens_.find(target);
    return token!=shell_tokens_.end() && !token->second.empty() && text.find(token->second)!=std::string::npos;
}
Menu::Menu(const CssxHost* host):host_(host),recovery_(host) {
    settings_=host_.request({{"op","state.load"}});
    if(!settings_.is_object()) settings_=Json::object();
    auto number=[&](const char* id,double fallback,double low,double high) {
        double value=fallback;if(settings_.contains(id) && settings_[id].is_number()) value=settings_[id].get<double>();
        if(!std::isfinite(value)) return fallback;
        const double step=std::string(id)=="heal_interval" || std::string(id)=="move_multiplier"?.25:std::string(id)=="shockwave_interval"?.5:1.;
        return std::clamp(low+std::round((std::clamp(value,low,high)-low)/step)*step,low,high);
    };
    values_={{"god",false},{"auto_heal",false},{"infinite_resolve",false},{"move_fast",false},{"max_shell_points",false},
        {"heal_amount",number("heal_amount",100,1,9999)},{"resolve_amount",number("resolve_amount",100,1,9999)},
        {"heal_percent",number("heal_percent",100,1,100)},{"heal_interval",number("heal_interval",1,.25,5)},
        {"move_multiplier",number("move_multiplier",2,1,5)},{"grant_amount",int(number("grant_amount",100,1,100000))},
        {"damage_percent",int(number("damage_percent",50,1,99))},{"harbinger_level",int(number("harbinger_level",1,1,1000))},
        {"shell","none"},{"pickup","none"},{"tarstone","none"},{"tarstone_scope","selected"},
        {"tarstone_level",int(number("tarstone_level",1,1,3))},{"pickup_amount",int(number("pickup_amount",1,1,9999))}};
    for(const auto* id:combat_ids) values_[id]=false;
    for(const auto* id:power_ids) values_[id]=false;
    values_["shockwave_interval"]=number("shockwave_interval",3,.5,5);
    values_["bindings"]=settings_.value("bindings",Json::object());
    if(!values_["bindings"].is_object() || values_["bindings"].size()>64) values_["bindings"]=Json::object();
    const auto keys=binding_keys();
    for(auto it=values_["bindings"].begin();it!=values_["bindings"].end();) {
        if(!it.value().is_string() || !keys.contains(it.value().get<std::string>())) it=values_["bindings"].erase(it);else ++it;
    }
    applied_=values_;
    binding_reset();
    status_="Cheats start off. Edit settings, then Apply settings.";
}
void Menu::report(const std::string& text) {
    if(status_==text) return;status_=text;host_.log(text);host_.request({{"op","invalidate"}});
}
Json Menu::require_player() {
    auto player=host_.player();
    if(!identity(player.value("pawn",Json())) || !identity(player.value("controller",Json()))) throw std::runtime_error("Enter the game world before using this action.");
    return player;
}
bool Menu::has_changes() const {return values_!=applied_;}
void Menu::persist(const Json& values) {
    Json next=Json::object();
    for(auto it=values.begin();it!=values.end();++it) if(it.value().is_number()) next[it.key()]=it.value();
    next["bindings"]=values.at("bindings");
    host_.request({{"op","state.save"},{"value",next}});settings_=std::move(next);
}
void Menu::apply_settings() {
    if(cleanup_required_) throw std::runtime_error("Turn off all cheats to finish cleanup before applying more settings.");
    if(!has_changes()) return;
    binding_validate();
    const auto before=applied_;
    applied_=values_;
    try {
        if(applied_["god"]!=before["god"]) god(applied_["god"].get<bool>());
        if(applied_["move_fast"]!=before["move_fast"] ||
           (applied_["move_fast"]==true && applied_["move_multiplier"]!=before["move_multiplier"]))
            movement(applied_["move_fast"].get<bool>());
        if(applied_["max_shell_points"]!=before["max_shell_points"]) shell_points(applied_["max_shell_points"].get<bool>());
        if(applied_["auto_heal"]==true || applied_["infinite_resolve"]==true) require_player();
        combat_sync();
        power_sync();
        persist(applied_);
    } catch(const std::exception& e) {
        const std::string reason=e.what();applied_=before;
        try {god(before["god"].get<bool>());movement(before["move_fast"].get<bool>());shell_points(before["max_shell_points"].get<bool>());combat_sync();power_sync();}
        catch(const std::exception& cleanup) {
            cleanup_required_=true;
            throw std::runtime_error("Apply failed: "+reason+". Cleanup needs retry: "+cleanup.what());
        }
        throw std::runtime_error("Settings were not applied: "+reason);
    }
    bindings_checked_=true;binding_reset();heal_time_=resolve_time_=power_time_=0;reapply_pending_=false;report("Settings applied. Preferences and shortcuts saved; cheats remain session-only.");
}
void Menu::disable_all() {
    reapply_pending_=false;
    // Stop periodic work first. Failed restores retain their ownership records.
    for(const auto* id:toggle_ids) applied_[id]=false;
    cleanup_required_=true;
    power_clear();combat_clear();god(false);movement(false);shell_points(false);
    for(const auto* id:toggle_ids) values_[id]=false;
    cleanup_required_=false;report("All cheats off. Other pending edits were kept.");
    binding_reset();
}
void Menu::refresh_shells() {
    auto settings=host_.find("/Script/Sparta.Default__SpartaGameSettings");
    auto names=host_.call(settings,"GetShellNames");
    if(!names.is_array() || names.size()>128) throw std::runtime_error("Game shell list is not available.");
    Json choices=Json::array();
    for(const auto& name:names) if(name.is_string()) {
        const auto text=name.get<std::string>();const auto key=normalized(text);
        if(!text.empty() && key.find("loadfromsave")==std::string::npos) choices.push_back({{"id",text},{"label",text}});
    }
    if(choices.empty()) throw std::runtime_error("The game returned no selectable shells.");
    std::map<std::string,std::string> tokens;
    for(const auto& choice:choices) {
        const auto name=choice.at("id").get<std::string>();
        const auto definition=host_.call(settings,"GetShellItemDefinition",Json::array({name}));
        const auto path=definition.is_object()?definition.value("name",std::string{}):std::string{};
        const auto prefix=path.rfind(".ID_Shell_");
        if(prefix!=std::string::npos) {
            auto token=path.substr(prefix+10);if(token.ends_with("_C")) token.resize(token.size()-2);
            tokens[name]=normalized(token);
        }
    }
    shell_tokens_=std::move(tokens);
    if(choices!=shells_) {
        bindings_checked_=false;
        shells_=std::move(choices);bool found=false;for(const auto& option:shells_) if(option["id"]==values_["shell"]) found=true;
        if(!found) values_["shell"]=shells_[0]["id"];
        applied_["shell"]=values_["shell"];
        host_.request({{"op","invalidate"}});
    }
}
Json Menu::model() {
    Json enabled=Json::object();const bool live=current_.is_object() && identity(current_.value("pawn",Json()));
    for(const auto& action:grants) enabled[std::string("grant_")+action.id]=live && !pending_;
    for(const auto& action:unlocks) enabled[std::string("unlock_")+action.id]=live && !pending_;
    for(const auto* action:{"heal","resolve","revive","damage","set_harbinger","switch_shell","god","auto_heal","infinite_resolve","move_fast"}) enabled[action]=live && !pending_ && !has_changes() && !cleanup_required_;
    enabled["switch_shell"]=live && !pending_ && !has_changes() && !cleanup_required_ && values_["shell"]!="none";
    enabled["unlock_shells"]=live && !pending_ && !has_changes() && !cleanup_required_;
    for(const auto* id:toggle_ids) enabled[id]=live && !pending_ && !cleanup_required_;
    for(const auto* id:power_ids) {
        const auto shell=current_.is_object()?current_.value("shell",std::string{}):std::string{};
        const auto expected=std::string(id)=="smert_stance"?"Smert":std::string(id)=="genessa_clones"?"Genessa":"Lazlo";
        enabled[id]=live && !pending_ && !cleanup_required_ && (values_[id]==true || shell_matches(shell,expected));
    }
    enabled["refresh_pickups"]=live && !pending_;
    enabled["refresh_tarstones"]=live && !pending_;
    for(const auto* action:{"add_tarstone","set_tarstone_level","give_tarstones_melee","give_tarstones_sidearm","give_tarstones_support"})
        enabled[action]=live && !pending_ && !has_changes() && !cleanup_required_ && (std::string(action)!="add_tarstone" || values_["tarstone"]!="none");
    if(values_["tarstone_scope"]=="selected" && values_["tarstone"]=="none") enabled["set_tarstone_level"]=false;
    for(const auto* action:{"add_pickup","remove_pickup","give_all_pickups"}) enabled[action]=live && !pending_ && !has_changes() && !cleanup_required_ && (std::string(action)=="give_all_pickups" || values_["pickup"]!="none");
    enabled["repair_intro"]=live && !pending_ && !recovery_.running();
    enabled["cancel_recovery"]=recovery_.running();
    enabled["apply_settings"]=has_changes() && !pending_ && !cleanup_required_;
    enabled["discard_changes"]=has_changes() && !pending_;
    enabled["disable_all"]=!pending_ && (cleanup_required_ || !powers_.empty() || !combat_hooks_.empty() || !points_saved_.empty() || applied_["max_shell_points"]==true || !saved_.empty() || applied_["auto_heal"]==true || applied_["infinite_resolve"]==true || applied_["god"]==true || applied_["move_fast"]==true);
    std::string summary=cleanup_required_?"Cleanup needs retry. Use Turn off all cheats.":has_changes()?"Pending edits. Apply settings or Discard changes.":"Settings are applied.";
    unsigned active=0;for(const auto* id:toggle_ids) if(applied_[id]==true) ++active;
    summary+=" Active cheats: "+std::to_string(active)+".";
    Json confirmations=Json::object();
    for(const auto& action:grants) confirmations[std::string("grant_")+action.id]="Add "+std::to_string(int(values_["grant_amount"].get<double>()))+" "+action.id+"? The game can save this change.";
    confirmations["set_harbinger"]="Set Harbinger level to "+std::to_string(int(values_["harbinger_level"].get<double>()))+"? Progression and achievements can change.";
    confirmations["switch_shell"]="Switch gameplay shell to "+values_["shell"].get<std::string>()+"? Inventory will close and your shell abilities will change.";
    for(const auto* id:{"add_pickup","remove_pickup"}) confirmations[id]=std::string(id==std::string("add_pickup")?"Add ":"Remove ")+std::to_string(values_["pickup_amount"].get<int>())+" x "+values_["pickup"].get<std::string>()+"? The game can save this change.";
    for(const auto& option:tarstones_) if(option["id"]==values_["tarstone"])
        confirmations["add_tarstone"]="Add "+option["label"].get<std::string>()+"? An owned Tarstone will keep its current level and experience.";
    confirmations["set_tarstone_level"]="Set "+values_["tarstone_scope"].get<std::string>()+" owned Tarstones to level "+std::to_string(values_["tarstone_level"].get<int>())+"? Experience, durability and stacks will be kept. The game can save this change.";
    if(binding_consent()) confirmations["apply_settings"]="Save these shortcuts? Assigned gameplay-shell and health-reduction shortcuts run when pressed in gameplay, without another confirmation. They do not bypass active-cheat or game-state checks.";
    auto display_values=values_;display_values["binding_action"]=binding_action_;
    display_values["binding_key"]=values_["bindings"].value(binding_action_,std::string("none"));
    return {{"values",display_values},{"options",{{"shell",shells_},{"pickup",pickups_},{"tarstone",tarstones_},{"binding_action",binding_actions()},{"binding_key",binding_key_options()}}},{"enabled",enabled},{"status",summary+" "+status_},{"error",action_error_},{"confirmations",confirmations}};
}
void Menu::override_value(const Json& object,const std::string& property,const Json& value) {
    const auto key=std::to_string(identity(object))+":"+property;
    auto it=saved_.find(key);
    if(it==saved_.end()) {
        auto before=host_.get(object,property);
        if(property=="Movement") before=movement_speeds(before);
        it=saved_.emplace(key,Saved{object,std::move(before),Json(),property}).first;
    }
    auto expected=host_.set(object,property,value);
    it->second.expected=property=="Movement"?movement_speeds(expected):std::move(expected);
}
void Menu::restore(const std::string& property) {
    for(auto it=saved_.begin();it!=saved_.end();) {
        auto& saved=it->second;if(saved.property!=property){++it;continue;}
        Json current;
        try {current=host_.get(saved.object,saved.property);} catch(const std::exception& e) {
            const std::string err=e.what();
            if(err.find("expired")!=std::string::npos || err.find("null")!=std::string::npos ||
               err.find("target")!=std::string::npos || err.find("invalid")!=std::string::npos) {
                it=saved_.erase(it);continue;
            }
            throw;
        }
        try {
            if(property=="Movement" && saved.expected.is_object()) {
                Json patch=Json::object();
                for(auto field=saved.before.begin();field!=saved.before.end();++field)
                    if(current.at(field.key())==saved.expected.at(field.key())) patch[field.key()]=field.value();
                if(!patch.empty()) host_.set(saved.object,saved.property,patch);
            }
            else if(current==saved.expected) host_.set(saved.object,saved.property,saved.before);
            else host_.log("Another change replaced an owned value; leaving that newer value intact.","warning",{{"property",property}});
        } catch(const std::exception& e) {
            const std::string err=e.what();
            if(err.find("expired")!=std::string::npos || err.find("null")!=std::string::npos ||
               err.find("target")!=std::string::npos || err.find("invalid")!=std::string::npos) {
                it=saved_.erase(it);continue;
            }
            throw;
        }
        it=saved_.erase(it);
    }
}
void Menu::god(bool enabled) {
    if(!enabled) {restore("bCanBeDamaged");applied_["god"]=false;return;}
    auto player=require_player();auto pawn=player["pawn"];
    // The original God command ultimately changes this same actor flag. Own and
    // restore the exact original flag, including existing story invulnerability.
    if(host_.get(pawn,"bCanBeDamaged")!=false) override_value(pawn,"bCanBeDamaged",false);applied_["god"]=true;
}
void Menu::movement(bool enabled) {
    if(!enabled) {
        bool had=false;for(const auto& entry:saved_) if(entry.second.property=="Movement") had=true;
        restore("Movement");
        if(had) {
            auto player=host_.player();
            if(identity(player.value("pawn",Json())) && gameplay_ready(player)) {
                try { host_.call(player["pawn"],"InitialiseCharacterData"); } catch(...) {}
            }
        }
        applied_["move_fast"]=false;return;
    }
    auto player=require_player();auto pawn=player["pawn"];
    auto data=host_.get(pawn,"CharacterData");
    // Patch only these scalars. The surrounding structure contains maps and
    // curve state that neither the multiplier nor its cleanup owns.
    auto movement=movement_speeds(host_.get(data,"Movement"));const auto key=std::to_string(identity(data))+":Movement";
    const auto original=saved_.contains(key)?saved_.at(key).before:movement;
    for(const auto* field:{"WalkSpeed","JogSpeed","SprintSpeed"}) {
        const double value=original.at(field).get<double>();
        if(!std::isfinite(value) || value<=0) throw std::runtime_error("Movement data is not ready.");
        movement[field]=value*applied_.at("move_multiplier").get<double>();
    }
    override_value(data,"Movement",movement);host_.call(pawn,"InitialiseCharacterData");applied_["move_fast"]=true;
}
void Menu::event(const Json& event) {
    action_error_.clear();
    try {apply_event(event);} catch(const std::exception& error) {action_error_=error.what();report(error.what());throw;}
}
void Menu::apply_event(const Json& event) {
    const auto id=event.at("id").get<std::string>();
    if(id=="cancel_recovery") {recovery_.cancel();report("Intro-lock check cancelled.");return;}
    if(pending_) throw std::runtime_error("Wait for the current shell switch to finish.");
    if(id=="binding_action") {
        for(const auto& option:binding_actions()) if(option.at("id")==event.at("value")) {binding_action_=event.at("value").get<std::string>();return;}
        throw std::runtime_error("Unknown shortcut action.");
    }
    if(id=="binding_key") {
        const auto key=event.at("value").get<std::string>();if(!binding_keys().contains(key)) throw std::runtime_error("Unsupported shortcut key.");
        if(key=="none") values_["bindings"].erase(binding_action_);else values_["bindings"][binding_action_]=key;
        return;
    }
    if(id=="clear_bindings") {values_["bindings"]=Json::object();return;}
    if(id=="refresh_pickups") {refresh_pickups();return;}
    if(id=="refresh_tarstones") {refresh_tarstones();return;}
    if(id=="tarstone") {
        for(const auto& option:tarstones_) if(option["id"]==event.at("value")) {values_[id]=applied_[id]=event["value"];return;}
        throw std::runtime_error("Unknown Tarstone selection.");
    }
    if(id=="tarstone_scope") {
        for(const auto* scope:{"selected","Melee","Sidearm","Support","all"}) if(event.at("value")==scope) {values_[id]=applied_[id]=scope;return;}
        throw std::runtime_error("Unknown Tarstone level scope.");
    }
    if(id=="pickup") {
        for(const auto& option:pickups_) if(option["id"]==event.at("value")) {values_[id]=applied_[id]=event["value"];return;}
        throw std::runtime_error("Unknown pickup selection.");
    }
    if(id=="repair_intro") {if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the intro-lock check first.");recovery_.start();report(recovery_.message());return;}
    if(id=="disable_all") {disable_all();return;}
    if(id=="discard_changes") {values_=applied_;report("Pending settings discarded.");return;}
    if(id=="apply_settings") {if(binding_consent() && !event.value("confirmed",false)) throw std::runtime_error("Confirm the gameplay shortcuts before applying settings.");apply_settings();return;}
    if(cleanup_required_) throw std::runtime_error("Finish cleanup with Turn off all cheats first.");
    if(values_.contains(id) && values_[id].is_boolean()) {
        values_[id]=event.at("value").get<bool>();return;
    }
    if(values_.contains(id) && values_[id].is_number()) {
        if(!event.contains("value") || !event["value"].is_number()) throw std::runtime_error("A number is required.");
        const auto value=event["value"].get<double>();
        const std::map<std::string,std::pair<double,double>> bounds={{"heal_amount",{1,9999}},{"resolve_amount",{1,9999}},
            {"heal_percent",{1,100}},{"heal_interval",{.25,5}},{"move_multiplier",{1,5}},
            {"grant_amount",{1,100000}},{"pickup_amount",{1,9999}},{"tarstone_level",{1,3}},{"damage_percent",{1,99}},{"harbinger_level",{1,1000}},{"shockwave_interval",{.5,5}}};
        const auto range=bounds.at(id);
        if(!std::isfinite(value) || value<range.first || value>range.second) throw std::runtime_error("Value is outside the supported range.");
        const bool whole=id!="heal_interval" && id!="move_multiplier" && id!="shockwave_interval";
        if(whole && std::floor(value)!=value) throw std::runtime_error("Use a whole number for this setting.");
        if(!whole && std::abs(value*4-std::round(value*4))>1e-8) throw std::runtime_error("Use quarter-step values for this setting.");
        if(id=="shockwave_interval" && std::abs(value*2-std::round(value*2))>1e-8) throw std::runtime_error("Use half-second values for the shockwave interval.");
        values_[id]=whole?Json(int(value)):Json(value);return;
    }
    if(id=="shell") {
        for(const auto& option:shells_) if(option["id"]==event.at("value")){values_[id]=event["value"];applied_[id]=values_[id];return;}
        throw std::runtime_error("Unknown shell selection.");
    }
    auto player=require_player();auto pc=player["controller"];auto pawn=player["pawn"];
    if(has_changes()) throw std::runtime_error("Apply settings or Discard changes before running an action.");
    if(id=="unlock_shells") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the shell unlock first.");
        unlock_shells(player);return;
    }
    if(id=="set_tarstone_level") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the Tarstone level change first.");
        set_tarstone_levels(player);return;
    }
    if(id=="add_tarstone" || id=="give_tarstones_melee" || id=="give_tarstones_sidearm" || id=="give_tarstones_support") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the Tarstone grant first.");
        tarstone_action(id,player);return;
    }
    if(id=="add_pickup" || id=="remove_pickup" || id=="give_all_pickups") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the inventory change first.");
        if(id=="give_all_pickups") host_.call(pc,"S_AddAllItems");
        else {
            const auto count=values_["pickup_amount"].get<int>();const auto row=values_.at("pickup").get<std::string>();
            if(row=="none") throw std::runtime_error("Choose a pickup first.");
            if(id=="add_pickup") host_.call(pc,"S_AddItemQuantity",Json::array({row,count}));
            else {
                const auto item=pickup_class(pawn);
                const auto library=host_.request({{"op","class_default"},{"class","BPFL_Player_C"}});
                host_.call(library,"RemoveItemStacksSilent",Json::array({item,count,true,pawn}));
            }
        }
        report("Inventory change requested. The game can save this change.");return;
    }
    if(id=="heal" || id=="resolve") {host_.call(pc,id=="heal"?"S_Heal":"S_GainResolve",Json::array({values_[id+"_amount"]}));report(id=="heal"?"Health restored.":"Resolve added.");return;}
    if(id=="revive") {host_.call(pc,"S_ReviveShell");report("Shell revival requested.");return;}
    if(id=="damage") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the health reduction first.");
        auto health=host_.get(pawn,"HealthComponent");
        double current=host_.call(health,"GetShellHealth"),maximum=host_.call(health,"GetMaxShellHealth");
        if(current<=.5 || maximum<=0) {current=host_.call(health,"GetHealth");maximum=host_.call(health,"GetMaxHealth");}
        if(!std::isfinite(current) || !std::isfinite(maximum) || maximum<=0) throw std::runtime_error("Health is not ready.");
        const auto target=std::max(1.,maximum*values_["damage_percent"].get<double>()/100.);
        if(current>target) host_.call(pc,"S_DealDamage",Json::array({current-target}));
        report("Health target applied without depleting the active health pool.");return;
    }
    if(id=="set_harbinger") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the Harbinger level change first.");
        host_.call(pc,"S_SetDarkBroLevel",Json::array({int(values_["harbinger_level"].get<double>())-1}));report("Harbinger level change requested.");return;
    }
    for(const auto& action:grants) if(id==std::string("grant_")+action.id) {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the resource grant first.");
        host_.call(pc,action.function,Json::array({int(values_["grant_amount"].get<double>())}));report("Resource added.");return;
    }
    for(const auto& action:unlocks) if(id==std::string("unlock_")+action.id) {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the progression change first.");
        host_.call(pc,action.function);report("Unlock requested. Changes can be saved by the game.");return;
    }
    if(id=="switch_shell") {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the gameplay shell change first.");
        for(const auto* toggle:toggle_ids) if(applied_[toggle]==true) throw std::runtime_error("Turn off active cheats before switching gameplay shells.");
        const auto target=values_.at("shell").get<std::string>();if(target=="none") throw std::runtime_error("Choose a shell first.");
        if(shell_matches(host_.call(pawn,"GetCharacterID"),target)) {report("That gameplay shell is already active.");return;}
        pending_=PendingShell{target,pc};
        try {host_.request({{"op","menu.close"}});} catch(...) {pending_.reset();throw;}
        report("Closing Inventory before switching the gameplay shell.");return;
    }
    throw std::runtime_error("Unknown Cheat Menu action: "+id);
}
void Menu::shell_tick(double delta) {
    if(!pending_) return;pending_->elapsed+=delta;
    if(pending_->elapsed<(pending_->issued?.3:.05)) return;pending_->elapsed=0;
    auto player=require_player();auto& pending=*pending_;
    if(identity(player["controller"])!=identity(pending.controller)) {pending_.reset();report("Shell switch cancelled: the player controller changed.");return;}
    if(!pending.issued) {
        auto menu=host_.request({{"op","menu.status"}});
        if(!menu.contains("menu_open") || !menu["menu_open"].is_boolean()) throw std::runtime_error("Cannot verify that Inventory closed.");
        if(menu["menu_open"]==true) {
            if(++pending.close_checks>=20){pending_.reset();report("Shell switch cancelled: Inventory did not close.");}return;
        }
        pending.issued=true;
        host_.call(player["controller"],"S_SwitchToShell",Json::array({pending.target}));return;
    }
    if(shell_matches(host_.call(player["pawn"],"GetCharacterID"),pending.target)) {
        const auto name=pending.target;pending_.reset();report("Gameplay shell confirmed: "+name);return;
    }
    if(++pending.checks>=10) {pending_.reset();report("Shell switch was not confirmed. No repeat request was sent.");}
}
void Menu::tick(double seconds) {
    if(stopped_) return;
    binding_tick(seconds);
    const auto recovery_message=recovery_.message();recovery_.tick(seconds);
    if(recovery_.message()!=recovery_message) report(recovery_.message());
    try {shell_tick(seconds);} catch(const std::exception& e){pending_.reset();report(std::string("Shell switch stopped: ")+e.what());}
    refresh_+=seconds;heal_time_+=seconds;resolve_time_+=seconds;catalog_time_+=seconds;points_time_+=seconds;combat_time_+=seconds;power_time_+=seconds;
    if(refresh_<.25) return;refresh_=0;
    auto player=host_.player();
    if(!player.is_object() || !identity(player.value("pawn",Json()))) {if(!current_.is_null()){current_=nullptr;host_.request({{"op","invalidate"}});}return;}
    const bool changed=!current_.is_object() || identity(current_.value("pawn",Json()))!=identity(player["pawn"]) || owner_controller_!=identity(player["controller"]);
    const bool new_controller=owner_controller_ && owner_controller_!=identity(player["controller"]);
    try {
        if(changed) {
            // Restore shared CharacterData before acquiring the next pawn's
            // baseline. Never compound a multiplier across a restart.
            power_clear();for(const auto* id:power_ids) applied_[id]=values_[id]=false;
            combat_clear();restore("Movement");restore("bCanBeDamaged");
            if(new_controller) disable_all();
            current_=player;owner_controller_=identity(player["controller"]);catalog_ready_=false;catalog_time_=5;
            reapply_pending_=true;
            host_.request({{"op","invalidate"}});
        } else current_=player;
        if(!catalog_ready_ && catalog_time_>=5) {
            catalog_time_=0;
            try {refresh_shells();catalog_ready_=true;}
            catch(const std::exception& e) {report(std::string("Waiting for the shell catalog: ")+e.what());}
        }
        if(pending_ || cleanup_required_) return;
        const bool live=gameplay_ready(player);
        const auto power_delta=std::min(power_time_,.5);power_time_=0;power_tick(power_delta);
        if((reapply_pending_ || changed || combat_time_>=1.) && live) {combat_time_=0;combat_sync();}
        if(applied_["max_shell_points"]==true && (reapply_pending_ || changed || points_time_>=1.) && live) {points_time_=0;shell_points(true);}
        if(applied_["god"]==true && live) god(true);
        if(applied_["move_fast"]==true && (changed || reapply_pending_) && live) movement(true);
        if(live && applied_["auto_heal"]==true && heal_time_>=applied_["heal_interval"].get<double>()) {
            heal_time_=0;
            try {
                auto health=host_.get(player["pawn"],"HealthComponent");
                double current=host_.call(health,"GetHealth"),maximum=host_.call(health,"GetMaxHealth");
                double shell=host_.call(health,"GetShellHealth"),shell_max=host_.call(health,"GetMaxShellHealth");
                if(current>0 && maximum>0 && (current<maximum-.5 || (shell_max>0 && shell>0 && shell<shell_max-.5))) {
                    host_.call(player["controller"],"S_Heal",Json::array({std::max(1.,std::max(maximum,shell_max)*applied_["heal_percent"].get<double>()/100.)}));
                }
            } catch(...) {}
        }
        if(live && applied_["infinite_resolve"]==true && resolve_time_>=1.) {
            resolve_time_=0;
            try {
                auto health=host_.get(player["pawn"],"HealthComponent");auto attributes=host_.get(health,"HealthSet");
                auto resolve=host_.get(attributes,"Resolve"),maximum=host_.get(attributes,"MaxResolve");
                const double current=resolve.at("CurrentValue"),limit=maximum.at("CurrentValue");
                if(current<limit-.5 && limit>0) host_.call(player["controller"],"S_GainResolve",Json::array({limit-current}));
            } catch(...) {}
        }
        if(live) reapply_pending_=false;
        last_error_.clear();
    } catch(const std::exception& e) {
        if(last_error_!=e.what()) {last_error_=e.what();report("Waiting for game state: "+last_error_);}
    }
}
bool Menu::stop() {
    pending_.reset();recovery_.cancel();reapply_pending_=false;
    try {
        power_clear();combat_clear();shell_points(false);restore("bCanBeDamaged");restore("Movement");
        if(applied_["move_fast"]==true) {
            auto player=host_.player();
            if(identity(player.value("pawn",Json())) && gameplay_ready(player)) {
                try { host_.call(player["pawn"],"InitialiseCharacterData"); } catch(...) {}
            }
        }
        stopped_=true;return true;
    } catch(const std::exception& e) {host_.log(std::string("Cleanup failed: ")+e.what(),"error");return false;}
}
}

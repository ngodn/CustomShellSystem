#include "cheat_menu.hpp"
#include <cctype>

namespace cheat {
namespace {
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
bool shell_matches(const Json& tag,const std::string& target) {
    const auto value=tag.is_object()?tag.value("TagName",std::string{}):tag.is_string()?tag.get<std::string>():std::string{};
    const auto needle=normalized(target);return !needle.empty() && normalized(value).find(needle)!=std::string::npos;
}
uint64_t identity(const Json& handle) {return handle.is_object()?handle.value("$object",uint64_t{}):0;}
}
Menu::Menu(const CssxHost* host):host_(host) {
    settings_=host_.request({{"op","state.load"}});
    if(!settings_.is_object()) settings_=Json::object();
    auto number=[&](const char* id,double fallback,double low,double high) {
        double value=fallback;if(settings_.contains(id) && settings_[id].is_number()) value=settings_[id].get<double>();
        return std::isfinite(value)?std::clamp(value,low,high):fallback;
    };
    values_={{"god",false},{"auto_heal",false},{"infinite_resolve",false},{"move_fast",false},
        {"heal_amount",number("heal_amount",100,1,9999)},{"resolve_amount",number("resolve_amount",100,1,9999)},
        {"heal_percent",number("heal_percent",100,1,100)},{"heal_interval",number("heal_interval",1,.25,5)},
        {"move_multiplier",number("move_multiplier",2,1,5)},{"grant_amount",int(number("grant_amount",100,1,100000))},
        {"damage_percent",int(number("damage_percent",50,1,99))},{"harbinger_level",int(number("harbinger_level",1,1,1000))},
        {"shell","none"}};
    status_="Cheats start disabled. Select a feature to enable it.";
}
void Menu::report(const std::string& text) {
    if(status_==text) return;status_=text;host_.log(text);host_.request({{"op","invalidate"}});
}
Json Menu::require_player() {
    auto player=host_.player();
    if(!identity(player.value("pawn",Json())) || !identity(player.value("controller",Json()))) throw std::runtime_error("Enter the game world before using this action.");
    return player;
}
void Menu::persist() {
    for(auto it=values_.begin();it!=values_.end();++it) if(it.value().is_number()) settings_[it.key()]=it.value();
    host_.request({{"op","state.save"},{"value",settings_}});
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
    if(choices!=shells_) {
        shells_=std::move(choices);bool found=false;for(const auto& option:shells_) if(option["id"]==values_["shell"]) found=true;
        if(!found) values_["shell"]=shells_[0]["id"];
        host_.request({{"op","invalidate"}});
    }
}
Json Menu::model() {
    Json enabled=Json::object();const bool live=current_.is_object() && identity(current_.value("pawn",Json()));
    for(const auto& action:grants) enabled[std::string("grant_")+action.id]=live && !pending_;
    for(const auto& action:unlocks) enabled[std::string("unlock_")+action.id]=live && !pending_;
    for(const auto* action:{"heal","resolve","revive","damage","set_harbinger","switch_shell","god","auto_heal","infinite_resolve","move_fast"}) enabled[action]=live && !pending_;
    enabled["switch_shell"]=live && !pending_ && values_["shell"]!="none";
    return {{"values",values_},{"options",{{"shell",shells_}}},{"enabled",enabled},{"status",status_}};
}
void Menu::override_value(const Json& object,const std::string& property,const Json& value) {
    const auto key=std::to_string(identity(object))+":"+property;
    auto it=saved_.find(key);
    if(it==saved_.end()) it=saved_.emplace(key,Saved{object,host_.get(object,property),Json(),property}).first;
    it->second.expected=host_.set(object,property,value);
}
void Menu::restore(const std::string& property) {
    for(auto it=saved_.begin();it!=saved_.end();) {
        auto& saved=it->second;if(saved.property!=property){++it;continue;}
        Json current;
        try {current=host_.get(saved.object,saved.property);} catch(const std::exception& e) {
            if(std::string(e.what()).find("expired")!=std::string::npos) {it=saved_.erase(it);continue;}
            throw;
        }
        if(current==saved.expected) host_.set(saved.object,saved.property,saved.before);
        else host_.log("Another change replaced an owned value; leaving that newer value intact.","warning",{{"property",property}});
        it=saved_.erase(it);
    }
}
void Menu::god(bool enabled) {
    if(!enabled) {restore("bCanBeDamaged");values_["god"]=false;return;}
    auto player=require_player();auto pawn=player["pawn"];
    // The original God command ultimately changes this same actor flag. Own and
    // restore the exact original flag, including existing story invulnerability.
    override_value(pawn,"bCanBeDamaged",false);values_["god"]=true;
}
void Menu::movement(bool enabled) {
    auto player=require_player();auto pawn=player["pawn"];
    if(!enabled) {restore("Movement");host_.call(pawn,"InitialiseCharacterData");values_["move_fast"]=false;return;}
    auto data=host_.get(pawn,"CharacterData");
    auto movement=host_.get(data,"Movement");const auto key=std::to_string(identity(data))+":Movement";
    const auto original=saved_.contains(key)?saved_.at(key).before:movement;
    for(const auto* field:{"WalkSpeed","JogSpeed","SprintSpeed"}) {
        const double value=original.at(field).get<double>();
        if(!std::isfinite(value) || value<=0) throw std::runtime_error("Movement data is not ready.");
        movement[field]=value*values_.at("move_multiplier").get<double>();
    }
    override_value(data,"Movement",movement);host_.call(pawn,"InitialiseCharacterData");values_["move_fast"]=true;
}
void Menu::event(const Json& event) {
    try {apply_event(event);} catch(const std::exception& error) {report(error.what());throw;}
}
void Menu::apply_event(const Json& event) {
    const auto id=event.at("id").get<std::string>();
    if(pending_) throw std::runtime_error("Wait for the current shell switch to finish.");
    if(values_.contains(id) && values_[id].is_number()) {
        if(!event.contains("value") || !event["value"].is_number()) throw std::runtime_error("A number is required.");
        const auto value=event["value"].get<double>();
        const std::map<std::string,std::pair<double,double>> bounds={{"heal_amount",{1,9999}},{"resolve_amount",{1,9999}},
            {"heal_percent",{1,100}},{"heal_interval",{.25,5}},{"move_multiplier",{1,5}},
            {"grant_amount",{1,100000}},{"damage_percent",{1,99}},{"harbinger_level",{1,1000}}};
        const auto range=bounds.at(id);
        if(!std::isfinite(value) || value<range.first || value>range.second) throw std::runtime_error("Value is outside the supported range.");
        values_[id]=value;if(id=="move_multiplier" && values_["move_fast"]==true) movement(true);persist();return;
    }
    if(id=="shell") {
        for(const auto& option:shells_) if(option["id"]==event.at("value")){values_[id]=event["value"];return;}
        throw std::runtime_error("Unknown shell selection.");
    }
    auto player=require_player();auto pc=player["controller"];auto pawn=player["pawn"];
    if(id=="god") {god(event.at("value").get<bool>());report(values_[id]==true?"God enabled.":"God disabled.");return;}
    if(id=="move_fast") {movement(event.at("value").get<bool>());report(values_[id]==true?"Movement multiplier enabled.":"Movement restored.");return;}
    if(id=="auto_heal" || id=="infinite_resolve") {values_[id]=event.at("value").get<bool>();heal_time_=resolve_time_=0;report(id=="auto_heal"?"Auto Heal updated.":"Infinite Resolve updated.");return;}
    if(id=="heal" || id=="resolve") {host_.call(pc,id=="heal"?"S_Heal":"S_GainResolve",Json::array({values_[id+"_amount"]}));report(id=="heal"?"Health restored.":"Resolve added.");return;}
    if(id=="revive") {host_.call(pc,"S_ReviveShell");report("Shell revival requested.");return;}
    if(id=="damage") {
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
        host_.call(pc,action.function,Json::array({int(values_["grant_amount"].get<double>())}));report("Resource added.");return;
    }
    for(const auto& action:unlocks) if(id==std::string("unlock_")+action.id) {
        if(!event.value("confirmed",false)) throw std::runtime_error("Confirm the progression change first.");
        host_.call(pc,action.function);report("Unlock requested. Changes can be saved by the game.");return;
    }
    if(id=="switch_shell") {
        for(const auto* toggle:{"god","auto_heal","infinite_resolve","move_fast"}) if(values_[toggle]==true) throw std::runtime_error("Turn off active cheats before switching gameplay shells.");
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
    try {shell_tick(seconds);} catch(const std::exception& e){pending_.reset();report(std::string("Shell switch stopped: ")+e.what());}
    refresh_+=seconds;heal_time_+=seconds;resolve_time_+=seconds;
    if(refresh_<.25) return;refresh_=0;
    auto player=host_.player();
    if(!player.is_object() || !identity(player.value("pawn",Json()))) {if(!current_.is_null()){current_=nullptr;host_.request({{"op","invalidate"}});}return;}
    const bool changed=!current_.is_object() || identity(current_.value("pawn",Json()))!=identity(player["pawn"]);
    current_=player;
    try {
        if(changed) {refresh_shells();host_.request({{"op","invalidate"}});}
        if(pending_) return;
        if(values_["god"]==true) god(true);
        if(values_["move_fast"]==true && changed) movement(true);
        if(values_["auto_heal"]==true && heal_time_>=values_["heal_interval"].get<double>()) {
            heal_time_=0;auto health=host_.get(player["pawn"],"HealthComponent");
            double current=host_.call(health,"GetHealth"),maximum=host_.call(health,"GetMaxHealth");
            double shell=host_.call(health,"GetShellHealth"),shell_max=host_.call(health,"GetMaxShellHealth");
            if(current<maximum-.5 || (shell_max>0 && shell<shell_max-.5)) host_.call(player["controller"],"S_Heal",Json::array({std::max(1.,std::max(maximum,shell_max)*values_["heal_percent"].get<double>()/100.)}));
        }
        if(values_["infinite_resolve"]==true && resolve_time_>=1.) {
            resolve_time_=0;auto health=host_.get(player["pawn"],"HealthComponent");auto attributes=host_.get(health,"HealthSet");
            auto resolve=host_.get(attributes,"Resolve"),maximum=host_.get(attributes,"MaxResolve");
            const double current=resolve.at("CurrentValue"),limit=maximum.at("CurrentValue");
            if(current<limit-.5) host_.call(player["controller"],"S_GainResolve",Json::array({limit-current}));
        }
        last_error_.clear();
    } catch(const std::exception& e) {
        if(last_error_!=e.what()) {last_error_=e.what();report("Waiting for game state: "+last_error_);}
    }
}
bool Menu::stop() {
    pending_.reset();
    try {
        restore("bCanBeDamaged");restore("Movement");
        if(values_["move_fast"]==true) {auto player=host_.player();if(identity(player.value("pawn",Json()))) host_.call(player["pawn"],"InitialiseCharacterData");}
        stopped_=true;return true;
    } catch(const std::exception& e) {host_.log(std::string("Cleanup failed: ")+e.what(),"error");return false;}
}
}

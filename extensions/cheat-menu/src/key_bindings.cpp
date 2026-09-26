#include "cheat_menu.hpp"

namespace cheat {
Json Menu::binding_actions() const {
    Json result=Json::array();
    for(const auto& [id,label]:std::initializer_list<std::pair<const char*,const char*>>{
        {"god","God mode"},{"auto_heal","Auto Heal"},{"infinite_resolve","Infinite Resolve"},
        {"move_fast","Movement multiplier"},{"max_shell_points","Max Shell Points 100"},
        {"no_cooldown","No ability cooldown"},{"perfect_parry","Perfect parry"},
        {"perfect_block","Perfect block"},{"perfect_harden","Perfect harden"},
        {"genessa_clones","Genessa persistent clones"},{"smert_stance","Smert fight stance"},
        {"lazlo_detonation","Lazlo repeating shockwave"},{"heal","Restore health"},
        {"resolve","Restore Resolve"},{"revive","Revive shell"},{"damage","Reduce health to target"},
        {"disable_all","Turn off all cheats"}}) result.push_back({{"id",id},{"label",label}});
    for(const auto& shell:shells_) if(shell.at("id")!="none")
        result.push_back({{"id","switch:"+shell.at("id").get<std::string>()},{"label","Switch shell: "+shell.at("label").get<std::string>()}});
    return result;
}
const Json& Menu::binding_keys() const {
    static const Json keys=[] {
    Json result={{"none",Json::array()}};
    for(int i=1;i<=24;++i) {
        if(i==12) continue; // Leave Steam's screenshot default alone.
        const auto key="F"+std::to_string(i);result[key]=Json::array({key});
        result["LeftCtrl+"+key]=Json::array({"LeftControl",key});
    }
    for(const auto& [name,key]:std::initializer_list<std::pair<const char*,const char*>>{
        {"R3+D-pad Up","Gamepad_DPad_Up"},{"R3+D-pad Down","Gamepad_DPad_Down"},
        {"R3+D-pad Left","Gamepad_DPad_Left"},{"R3+D-pad Right","Gamepad_DPad_Right"}})
        result[name]=Json::array({"Gamepad_RightThumbstick",key});
    return result;
    }();return keys;
}
Json Menu::binding_key_options() const {
    Json options=Json::array({{{"id","none"},{"label","None"}}});
    for(const auto& [id,keys]:binding_keys().items()) if(id!="none") options.push_back({{"id",id},{"label",id}});
    return options;
}
void Menu::binding_validate() const {
    const auto& bindings=values_.at("bindings");const auto& keys=binding_keys();
    std::set<std::string> actions;for(const auto& action:binding_actions()) actions.insert(action.at("id").get<std::string>());
    if(!bindings.is_object() || bindings.size()>64) throw std::runtime_error("Too many shortcuts.");
    std::set<std::string> used;
    for(const auto& [action,key]:bindings.items()) {
        if(!actions.contains(action) || !key.is_string() || key=="none" || !keys.contains(key.get<std::string>()))
            throw std::runtime_error("A saved shortcut is no longer supported. Clear shortcuts, then assign it again.");
        const auto name=key.get<std::string>();
        if(!used.insert(name).second) throw std::runtime_error("Two actions use "+name+". Assign a different shortcut before applying.");
        // A bare F-key would also fire as part of its Ctrl chord.
        const auto other=name.starts_with("LeftCtrl+")?name.substr(9):"LeftCtrl+"+name;
        if(used.contains(other)) throw std::runtime_error("A shortcut overlaps its Ctrl combination. Use separate keys.");
    }
}
bool Menu::binding_consent() const {
    if(!values_.contains("bindings") || !applied_.contains("bindings")) return false;
    for(const auto& [action,key]:values_.at("bindings").items())
        if((action=="damage" || action.starts_with("switch:")) && (!applied_.at("bindings").contains(action) || applied_.at("bindings").at(action)!=key)) return true;
    return false;
}
void Menu::binding_reset() {
    binding_down_.clear();binding_time_=0;
    for(const auto& [action,key]:applied_.at("bindings").items()) binding_down_[action]=true;
}
void Menu::binding_fire(const std::string& action) {
    if(action.starts_with("switch:")) {
        const auto shell=action.substr(7);
        apply_event({{"id","shell"},{"value",shell}});
        // Binding this action required an explicit confirmation when saved.
        apply_event({{"id","switch_shell"},{"confirmed",true}});return;
    }
    for(const auto* toggle:toggle_ids) if(action==toggle) {
        values_[toggle]=!applied_.at(toggle).get<bool>();
        try {apply_settings();}
        catch(...) {values_=applied_;throw;}
        return;
    }
    if(action=="damage") {apply_event({{"id",action},{"confirmed",true}});return;}
    if(action=="heal" || action=="resolve" || action=="revive" || action=="disable_all") {apply_event({{"id",action}});return;}
    throw std::runtime_error("Unknown shortcut action.");
}
void Menu::binding_tick(double seconds) {
    if(applied_.at("bindings").empty()) return;
    binding_time_+=seconds;if(binding_time_<1./60.) return;binding_time_=0;
    try {
        if(!catalog_ready_ || has_changes() || pending_ || cleanup_required_) {binding_reset();return;}
        const auto player=host_.player();
        if(!current_.is_object() || player.value("pawn",Json())!=current_.value("pawn",Json()) || player.value("controller",Json())!=current_.value("controller",Json()) || !gameplay_ready(player)) {binding_reset();return;}
        if(!bindings_checked_) {binding_validate();bindings_checked_=true;}
        const auto& definitions=binding_keys();std::set<std::string> keys;
        for(const auto& [action,key]:applied_.at("bindings").items()) for(const auto& part:definitions.at(key.get<std::string>())) keys.insert(part.get<std::string>());
        const auto pressed=host_.request({{"op","input.keys"},{"target",player.at("controller")},{"keys",keys}});
        std::string fire;
        for(const auto& [action,key]:applied_.at("bindings").items()) {
            bool down=true;for(const auto& part:definitions.at(key.get<std::string>())) {
                if(!pressed.contains(part.get<std::string>()) || !pressed.at(part.get<std::string>()).is_boolean()) throw std::runtime_error("Shortcut input is unavailable.");
                down=down && pressed.at(part.get<std::string>()).get<bool>();
            }
            if(down && !binding_down_[action] && fire.empty()) fire=action;
            binding_down_[action]=down;
        }
        if(!fire.empty()) binding_fire(fire);
    } catch(const std::exception& error) {
        binding_reset();action_error_=error.what();report("Shortcut stopped: "+action_error_);
    }
}
}

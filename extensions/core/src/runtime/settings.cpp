#include "settings.hpp"
#include <algorithm>
#include <cctype>

namespace cssx {
bool valid_key_name(const std::string& name) {
    return !name.empty() && name.size()<=64 && std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c) || c=='_';});
}
namespace {
std::vector<std::string> keys(const Json& j,const char* key,const std::vector<std::string>& fallback) {
    if(!j.contains(key)) return fallback;
    const auto& value=j.at(key);
    if(!value.is_array() || value.empty() || value.size()>4) throw std::runtime_error(std::string("settings.")+key+" must list 1 to 4 keys");
    std::vector<std::string> result;
    for(const auto& item:value) {
        const auto name=item.get<std::string>();
        if(!valid_key_name(name) || std::find(result.begin(),result.end(),name)!=result.end()) throw std::runtime_error(std::string("settings.")+key+" has an invalid or duplicate key name");
        result.push_back(name);
    }
    return result;
}
}
Json Settings::json() const {
    Json j=extra;
    j["schema"]=schema;
    j["open_keyboard"]=open_keyboard;
    j["open_gamepad"]=open_gamepad;
    j["ui_scale"]=ui_scale;
    j["pause_while_open"]=pause_while_open;
    j["hide_hud_while_open"]=hide_hud_while_open;
    j["show_extension_status"]=show_extension_status;
    return j;
}
Settings Settings::parse(const Json& j) {
    if(!j.is_object()) throw std::runtime_error("settings.json must be an object");
    if(j.value("schema",0)!=schema) throw std::runtime_error("settings.json schema is not 1");
    Settings s;
    s.open_keyboard=keys(j,"open_keyboard",s.open_keyboard);
    s.open_gamepad=keys(j,"open_gamepad",s.open_gamepad);
    if(j.contains("ui_scale")) {
        const double scale=j.at("ui_scale").get<double>();
        if(!(scale>=0.75 && scale<=1.5)) throw std::runtime_error("settings.ui_scale must be between 0.75 and 1.5");
        s.ui_scale=scale;
    }
    if(j.contains("pause_while_open")) s.pause_while_open=j.at("pause_while_open").get<bool>();
    if(j.contains("hide_hud_while_open")) s.hide_hud_while_open=j.at("hide_hud_while_open").get<bool>();
    if(j.contains("show_extension_status")) s.show_extension_status=j.at("show_extension_status").get<bool>();
    for(auto it=j.begin();it!=j.end();++it) {
        static const char* known[]={"schema","open_keyboard","open_gamepad","ui_scale","pause_while_open","hide_hud_while_open","show_extension_status"};
        if(std::find(std::begin(known),std::end(known),it.key())==std::end(known)) s.extra[it.key()]=it.value();
    }
    return s;
}
Settings Settings::load(const fs::path& file,std::string* note) {
    std::error_code ec;
    if(!fs::exists(file,ec)) { Settings s; s.save(file); if(note) *note="Created default settings"; return s; }
    try { return parse(read_json(file)); }
    catch(const std::exception& first) {
        auto backup=file; backup+=".bak";
        if(fs::exists(backup,ec)) {
            try { auto s=parse(read_json(backup)); if(note) *note=std::string("Recovered settings from backup: ")+first.what(); return s; }
            catch(...) {}
        }
        if(note) *note=std::string("Settings invalid, using defaults: ")+first.what();
        return Settings{};
    }
}
void Settings::save(const fs::path& file) const { atomic_json(file,json()); }
}

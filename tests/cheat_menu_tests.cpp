#include "cheat_menu.hpp"
#include "extension_data.hpp"
#include <iostream>

using cheat::Json;
namespace {
Json object(uint64_t id){return {{"$object",id}};}
struct Host {
    Json state=Json::object();
    Json pawn=object(1),controller=object(2);
    Json movement={{"WalkSpeed",100.},{"JogSpeed",250.},{"SprintSpeed",400.}};
    bool damageable=true,open=false,confirm_switch=true,fail_restore=false;
    std::string shell="Genessa";
    std::vector<Json> calls;
    CssxHost api{CSSX_ABI,sizeof(CssxHost),this,request};
    static int request(void* context,const char* value,CssxSink sink,void* output) {
        try {auto response=static_cast<Host*>(context)->handle(Json::parse(value)).dump();sink(output,response.data(),response.size());return 1;}
        catch(const std::exception& e){auto response=Json{{"error",e.what()}}.dump();sink(output,response.data(),response.size());return 0;}
    }
    Json handle(const Json& j) {
        const auto op=j.at("op").get<std::string>();
        if(op=="state.load") return state;
        if(op=="state.save") {state=j.at("value");return true;}
        if(op=="log" || op=="invalidate") return nullptr;
        if(op=="player") return {{"pawn",pawn},{"controller",controller}};
        if(op=="menu.close") {open=false;return true;}
        if(op=="menu.status") return {{"menu_open",open}};
        if(op=="find") return object(3);
        if(op=="get") {
            const auto p=j.at("property");
            if(p=="bCanBeDamaged") return damageable;
            if(p=="CharacterData") return object(4);
            if(p=="Movement") return movement;
            if(p=="HealthComponent") return object(5);
            if(p=="HealthSet") return object(6);
            if(p=="Resolve") return {{"CurrentValue",0}};
            if(p=="MaxResolve") return {{"CurrentValue",100}};
        }
        if(op=="set") {
            if(j.at("property")=="bCanBeDamaged") {if(fail_restore && j.at("value")==true) throw std::runtime_error("restore failed");damageable=j.at("value").get<bool>();return damageable;}
            if(j.at("property")=="Movement") {movement=j.at("value");return movement;}
        }
        if(op=="call") {
            calls.push_back(j);const auto function=j.at("function");
            if(function=="GetShellNames") return {{"ReturnValue",Json::array({"Genessa","Proxima","ID_Shell_LoadFromSave"})}};
            if(function=="GetCharacterID") return {{"ReturnValue",{{"TagName","Shell."+shell}}}};
            if(function=="GetShellHealth" || function=="GetHealth") return {{"ReturnValue",80.}};
            if(function=="GetMaxShellHealth" || function=="GetMaxHealth") return {{"ReturnValue",100.}};
            if(function=="S_SwitchToShell") {if(confirm_switch)shell=j.at("args")[0].get<std::string>();return Json::object();}
            if(function=="InitialiseCharacterData" || function.get<std::string>().starts_with("S_")) return Json::object();
        }
        throw std::runtime_error("Unexpected test request: "+j.dump());
    }
    unsigned count(const std::string& function)const {unsigned n=0;for(const auto& call:calls) if(call.at("function")==function) ++n;return n;}
};
void check(bool value,const char* text){if(!value) throw std::runtime_error(text);}
template<class F> void rejects(F fn){try{fn();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const auto definition=css::read_json(css::extensions::utf8_path(argv[1]));
    Host host;cheat::Menu menu(&host.api);
    check(host.calls.empty(),"Startup mutated gameplay");
    check(menu.model()["values"]["god"]==false,"God was enabled at startup");
    menu.tick(.25);
    css::extensions::validate_model(css::extensions::bind_menu(definition,menu.model()));
    check(menu.model()["options"]["shell"].size()==2,"Load-from-save entry was not filtered");
    rejects([&]{menu.event({{"id","unlock_weapons"}});});
    check(host.count("S_UnlockAllWeapons")==0,"Unconfirmed unlock ran");
    menu.event({{"id","unlock_weapons"},{"confirmed",true}});
    check(host.count("S_UnlockAllWeapons")==1,"Confirmed unlock missing");
    menu.event({{"id","god"},{"value",true}});check(!host.damageable,"God did not apply");
    menu.event({{"id","shell"},{"value","Proxima"}});
    rejects([&]{menu.event({{"id","switch_shell"}});});
    check(host.count("S_SwitchToShell")==0,"Unsafe shell switch dispatched");
    menu.event({{"id","god"},{"value",false}});check(host.damageable,"God did not restore");
    menu.event({{"id","move_fast"},{"value",true}});check(host.movement["WalkSpeed"]==200.,"Speed did not apply");
    menu.event({{"id","move_multiplier"},{"value",3}});check(host.movement["WalkSpeed"]==300.,"Speed compounded instead of using the original");
    menu.event({{"id","move_fast"},{"value",false}});check(host.movement["WalkSpeed"]==100.,"Speed did not restore");
    menu.event({{"id","switch_shell"}});
    rejects([&]{menu.event({{"id","switch_shell"}});});
    menu.tick(.1);menu.tick(.3);
    check(host.count("S_SwitchToShell")==1,"Duplicate shell switch request");
    check(host.shell=="Proxima","Shell did not change");
    menu.event({{"id","shell"},{"value","Genessa"}});menu.event({{"id","switch_shell"}});
    host.controller=object(22);menu.tick(.1);
    check(host.count("S_SwitchToShell")==1,"Changed controller received pending switch");
    menu.event({{"id","switch_shell"}});host.confirm_switch=false;
    for(int i=0;i<50;++i) menu.tick(.3);
    check(host.count("S_SwitchToShell")==2,"Read-back timeout retried shell RPC");
    menu.event({{"id","auto_heal"},{"value",true}});menu.event({{"id","infinite_resolve"},{"value",true}});
    for(int i=0;i<15;++i) menu.tick(.1);
    check(host.count("S_Heal")>0,"Auto Heal did not run");check(host.count("S_GainResolve")>0,"Infinite Resolve did not run");
    rejects([&]{menu.event({{"id","heal_amount"},{"value",0}});});
    menu.event({{"id","god"},{"value",true}});host.fail_restore=true;
    check(!menu.stop(),"Failed cleanup allowed unloading");host.fail_restore=false;
    check(menu.stop() && host.damageable,"Cleanup retry did not restore state");
    cheat::Menu fresh(&host.api);check(fresh.model()["values"]["god"]==false,"Saved settings enabled God on boot");
    check(fresh.model()["values"]["move_multiplier"]==3.,"Numeric settings did not persist");
    check(fresh.stop(),"Fresh instance failed shutdown");
    std::cout<<"Passive boot, shared menu validation, confirmations, exact restore, timed shell coordination and cleanup retry passed\n";
}

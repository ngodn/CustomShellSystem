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
    bool damageable=true,open=false,confirm_switch=true,fail_restore=false,fail_save=false;
    std::string shell="Genessa";
    bool check_soft=false;
    bool stone_owned=false,stone_manager_registers=false,bad_stone_signature=false,fail_stone_cache=false;
    Json stone_component={{"Level",2},{"exp",85},{"Stacks",1},{"Durability",12}},stone_runtime=stone_component;
    int map_writes=0,fail_map_write=0;
    bool bad_level_signature=false,fail_equipped_refresh=false;
    bool intro_lock=false,intro_done=true,map_unlocked=true,montage=false,retain_lock=false;
    int intro_instances=1,tag_count=1;
    std::vector<Json> calls;
    CssxHost api{CSSX_ABI,sizeof(CssxHost),this,request};
    static int request(void* context,const char* value,CssxSink sink,void* output) {
        try {auto response=static_cast<Host*>(context)->handle(Json::parse(value)).dump();sink(output,response.data(),response.size());return 1;}
        catch(const std::exception& e){auto response=Json{{"error",e.what()}}.dump();sink(output,response.data(),response.size());return 0;}
    }
    Json handle(const Json& j) {
        const auto op=j.at("op").get<std::string>();
        if(op=="state.load") return state;
        if(op=="state.save") {if(fail_save) throw std::runtime_error("Disk write failed");state=j.at("value");return true;}
        if(op=="log" || op=="invalidate") return nullptr;
        if(op=="player") return {{"pawn",pawn},{"controller",controller}};
        if(op=="menu.close") {open=false;return true;}
        if(op=="menu.status") return {{"menu_open",open}};
        if(op=="find" || op=="class_default") return object(3);
        if(op=="table.rows") return Json::array({"Trollweed","Moonshine"});
        if(op=="describe") {
            const auto fn=j.at("function").get<std::string>();
            std::map<std::string,int> fields;
            if(fn=="BuildTarstoneName") fields={{"SoftTarstone",40},{"IncludeLevel",1},{"__WorldContext",8},{"ReturnValue",16}};
            else if(fn=="AddTarstoneToItemManager") fields={{"Tarstone",8},{"Silent",1},{"__WorldContext",8}};
            else if(fn=="AddTarstoneToInventory") fields={{"Tarstone",8},{"Level",4},{"exp",4}};
            else if(fn=="UpdateSoftItemStatus") fields={{"SoftItem",bad_stone_signature?8:40},{"__WorldContext",8}};
            else if(fn=="CacheAllTarstones") fields={{"Completed",1}};
            else if(fn=="SetTarstoneLevel") fields={{"Tarstone",8},{"LevelData",bad_level_signature?8:16}};
            else if(!fn.starts_with("S_AddAllTarstones")) throw std::runtime_error("Unexpected describe: "+fn);
            Json args=Json::object();for(const auto& [name,size]:fields) args[name]={{"size",size},{"return",name=="ReturnValue"},{"out",false}};
            return args;
        }
        if(op=="get") {
            const auto p=j.at("property");
            if(p=="bCanBeDamaged") return damageable;
            if(p=="CharacterData") return object(4);
            if(p=="Movement") return movement;
            if(p=="AbilitySystemComponent") return object(7);
            if(p=="Mesh") return object(8);
            if(p=="WeaponPutInHandBlock") return {{"Handle",42}};
            if(p=="ActivatableAbilities") {
                Json ability=object(9);ability["class"]="BlueprintGeneratedClass /Game/Test.GA_Player_Prologue_EggStrandingCustom_C";
                Json instances=Json::array();for(int i=0;i<intro_instances;++i) instances.push_back(ability);
                return {{"Items",Json::array({{{"Ability",ability},{"ActiveCount",1},{"NonReplicatedInstances",instances},{"ReplicatedInstances",Json::array()}}})}};
            }
            if(p=="HealthComponent") return object(5);
            if(p=="HealthSet") return object(6);
            if(p=="Resolve") return {{"CurrentValue",0}};
            if(p=="MaxResolve") return {{"CurrentValue",100}};
            if(p=="TarstoneComponent") return object(20);
            if(p=="TarstoneLevels") return {{"$map",stone_owned?Json::array({{{"key",object(90)},{"value",j.at("target")==object(20)?stone_component:stone_runtime}}}):Json::array()}};
            if(p=="EquippedTarstoneItemInstances") return {{"$map",Json::array()}};
            if(p=="EquippedSupportTarstoneItemInstances") return {{"$map",Json::array({{{"key",object(90)},{"value",object(30)}}})}};
        }
        if(op=="map.update") {
            ++map_writes;if(fail_map_write==map_writes) throw std::runtime_error("Concurrent level edit");
            auto& value=j.at("target")==object(20)?stone_component:stone_runtime;
            if(j.at("key")!=object(90) || j.at("expected")!=value) throw std::runtime_error("Stale map edit");
            value=j.at("value");return value;
        }
        if(op=="set") {
            if(j.at("property")=="bCanBeDamaged") {if(fail_restore && j.at("value")==true) throw std::runtime_error("restore failed");damageable=j.at("value").get<bool>();return damageable;}
            if(j.at("property")=="Movement") {movement=j.at("value");return movement;}
        }
        if(op=="call") {
            calls.push_back(j);const auto function=j.at("function");
            if(function=="GetGameplayTagCount") return {{"ReturnValue",intro_lock?tag_count:0}};
            if(function=="HasPlayedGetUp") return {{"ReturnValue",intro_done}};
            if(function=="IsMapUnlocked") return {{"ReturnValue",map_unlocked}};
            if(function=="GetGameplayEffectFromActiveEffectHandle") {auto effect=object(10);effect["class"]="BlueprintGeneratedClass /Game/Test.GE_State_Block_Weapon_PutInHand_Primary_C";return {{"ReturnValue",effect}};}
            if(function=="GetAnimInstance") return {{"ReturnValue",object(11)}};
            if(function=="GetCurrentActiveMontage") return {{"ReturnValue",montage?object(12):Json()}};
            if(function=="ResetPlayerState") {if(!retain_lock) intro_lock=false;return Json::object();}
            if(function=="ResolveSoftItemDefinition") {check_soft=j.at("args")[0].contains("$table_field");return {{"ReturnValue",object(90)}};}
            if(function=="RemoveItemStacksSilent") return Json::object();
            if(function=="BuildTarstoneName") return {{"ReturnValue","Localized stone"}};
            if(function=="GetRuntimeData") return {{"ReturnValue",object(21)}};
            if(function=="AddTarstoneToItemManager") {if(stone_manager_registers) stone_owned=true;return Json::object();}
            if(function=="AddTarstoneToInventory") {stone_owned=true;return Json::object();}
            if(function=="UpdateSoftItemStatus") return Json::object();
            if(function=="CacheAllTarstones") {if(fail_stone_cache) throw std::runtime_error("Cache unavailable");return {{"Completed",true}};}
            if(function=="SetTarstoneLevel") {if(fail_equipped_refresh) throw std::runtime_error("Equipped refresh unavailable");return Json::object();}
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
    menu.event({{"id","god"},{"value",true}});
    check(host.damageable,"Editing God applied before confirmation");menu.tick(.25);
    check(host.damageable,"Tick used the draft instead of applied settings");
    menu.event({{"id","discard_changes"}});check(menu.model()["values"]["god"]==false,"Discard lost applied state");
    menu.event({{"id","god"},{"value",true}});host.fail_save=true;
    rejects([&]{menu.event({{"id","apply_settings"}});});
    check(host.damageable,"Save failure left newly enabled God active");host.fail_save=false;
    menu.event({{"id","apply_settings"}});check(!host.damageable,"God did not apply");
    menu.event({{"id","shell"},{"value","Proxima"}});
    rejects([&]{menu.event({{"id","switch_shell"},{"confirmed",true}});});
    check(host.count("S_SwitchToShell")==0,"Unsafe shell switch dispatched");
    menu.event({{"id","god"},{"value",false}});menu.event({{"id","apply_settings"}});check(host.damageable,"God did not restore");
    menu.event({{"id","move_fast"},{"value",true}});menu.event({{"id","apply_settings"}});check(host.movement["WalkSpeed"]==200.,"Speed did not apply");
    menu.event({{"id","move_multiplier"},{"value",3}});check(host.movement["WalkSpeed"]==200.,"Editing speed applied the draft");menu.event({{"id","apply_settings"}});check(host.movement["WalkSpeed"]==300.,"Speed compounded instead of using the original");
    menu.event({{"id","move_fast"},{"value",false}});menu.event({{"id","apply_settings"}});check(host.movement["WalkSpeed"]==100.,"Speed did not restore");
    menu.event({{"id","switch_shell"},{"confirmed",true}});
    rejects([&]{menu.event({{"id","switch_shell"},{"confirmed",true}});});
    menu.tick(.1);menu.tick(.3);
    check(host.count("S_SwitchToShell")==1,"Duplicate shell switch request");
    check(host.shell=="Proxima","Shell did not change");
    menu.event({{"id","shell"},{"value","Genessa"}});menu.event({{"id","switch_shell"},{"confirmed",true}});
    host.controller=object(22);menu.tick(.1);
    check(host.count("S_SwitchToShell")==1,"Changed controller received pending switch");
    menu.event({{"id","switch_shell"},{"confirmed",true}});host.confirm_switch=false;
    for(int i=0;i<50;++i) menu.tick(.3);
    check(host.count("S_SwitchToShell")==2,"Read-back timeout retried shell RPC");
    menu.event({{"id","auto_heal"},{"value",true}});menu.event({{"id","infinite_resolve"},{"value",true}});menu.event({{"id","apply_settings"}});
    for(int i=0;i<15;++i) menu.tick(.1);
    check(host.count("S_Heal")>0,"Auto Heal did not run");check(host.count("S_GainResolve")>0,"Infinite Resolve did not run");
    rejects([&]{menu.event({{"id","heal_amount"},{"value",0}});});
    menu.event({{"id","god"},{"value",true}});menu.event({{"id","apply_settings"}});host.fail_restore=true;
    check(!menu.stop(),"Failed cleanup allowed unloading");host.fail_restore=false;
    check(menu.stop() && host.damageable,"Cleanup retry did not restore state");
    rejects([&]{menu.event({{"id","heal_amount"},{"value",1.5}});});
    rejects([&]{menu.event({{"id","move_multiplier"},{"value",1.1}});});
    cheat::Menu fresh(&host.api);check(fresh.model()["values"]["god"]==false,"Saved settings enabled God on boot");
    check(fresh.model()["values"]["move_multiplier"]==3.,"Numeric settings did not persist");
    check(fresh.stop(),"Fresh instance failed shutdown");
    Host items;cheat::Menu item_menu(&items.api);item_menu.tick(.25);
    item_menu.event({{"id","refresh_pickups"}});check(item_menu.model()["options"]["pickup"].size()==2,"Pickup catalog not exposed");
    rejects([&]{item_menu.event({{"id","add_pickup"}});});check(items.count("S_AddItemQuantity")==0,"Unconfirmed pickup grant ran");
    item_menu.event({{"id","pickup_amount"},{"value",2}});
    rejects([&]{item_menu.event({{"id","add_pickup"},{"confirmed",true}});});
    item_menu.event({{"id","apply_settings"}});item_menu.event({{"id","add_pickup"},{"confirmed",true}});
    check(items.count("S_AddItemQuantity")==1,"Confirmed pickup grant missing");
    item_menu.event({{"id","remove_pickup"},{"confirmed",true}});check(items.check_soft && items.count("RemoveItemStacksSilent")==1,"Pickup removal bypassed typed definition");
    rejects([&]{item_menu.event({{"id","give_all_pickups"}});});item_menu.event({{"id","give_all_pickups"},{"confirmed",true}});
    check(items.count("S_AddAllItems")==1,"Give-all confirmation failed");check(item_menu.stop(),"Item menu failed cleanup");
    Host stones;cheat::Menu stone_menu(&stones.api);stone_menu.tick(.25);
    stone_menu.event({{"id","refresh_tarstones"}});
    check(stone_menu.model()["options"]["tarstone"].size()==6,"Tarstone categories did not merge");
    check(stone_menu.model()["options"]["tarstone"][0]["label"]=="Melee: Localized stone","Tarstone names were not localized");
    css::extensions::validate_model(css::extensions::bind_menu(definition,stone_menu.model()));
    rejects([&]{stone_menu.event({{"id","add_tarstone"}});});
    check(stones.count("AddTarstoneToItemManager")==0,"Unconfirmed Tarstone grant ran");
    stones.bad_stone_signature=true;
    rejects([&]{stone_menu.event({{"id","add_tarstone"},{"confirmed",true}});});
    check(stones.count("AddTarstoneToItemManager")==0,"Incompatible save interface allowed partial grant");
    stones.bad_stone_signature=false;stones.stone_owned=true;
    stone_menu.event({{"id","add_tarstone"},{"confirmed",true}});
    check(stones.count("AddTarstoneToInventory")==0 && stones.count("AddTarstoneToItemManager")==0,"Owned stone was granted again");
    stones.stone_owned=false;
    stone_menu.event({{"id","add_tarstone"},{"confirmed",true}});
    check(stones.count("AddTarstoneToInventory")==1 && stones.count("UpdateSoftItemStatus")==1,"Single Tarstone grant incomplete");
    for(const auto& call:stones.calls) if(call["function"]=="UpdateSoftItemStatus")
        check(call["args"]["SoftItem"].contains("$table_field") && call["outputs"].empty(),"Tarstone soft output was reinterpreted");
    stones.stone_owned=false;stones.stone_manager_registers=true;stones.fail_stone_cache=true;
    rejects([&]{stone_menu.event({{"id","add_tarstone"},{"confirmed",true}});});
    const auto grants_before=stones.count("AddTarstoneToItemManager");
    for(int i=0;i<20;++i) stone_menu.tick(.25);
    check(stones.count("AddTarstoneToItemManager")==grants_before,"Partial grant automatically retried");
    check(stones.count("AddTarstoneToInventory")==1,"Manager-owned stone was overwritten");
    for(const auto* category:{"melee","sidearm","support"}) {
        const auto id=std::string("give_tarstones_")+category;
        rejects([&]{stone_menu.event({{"id",id}});});
        stone_menu.event({{"id",id},{"confirmed",true}});
    }
    check(stones.count("S_AddAllTarstonesMelee")==1 && stones.count("S_AddAllTarstonesSidearm")==1 && stones.count("S_AddAllTarstonesSupport")==1,"Category grant failed");
    check(stone_menu.stop(),"Tarstone menu failed shutdown");
    Host levels;levels.stone_owned=true;cheat::Menu level_menu(&levels.api);level_menu.tick(.25);
    level_menu.event({{"id","tarstone_scope"},{"value","all"}});
    rejects([&]{level_menu.event({{"id","set_tarstone_level"}});});
    level_menu.event({{"id","tarstone_level"},{"value",2}});
    rejects([&]{level_menu.event({{"id","set_tarstone_level"},{"confirmed",true}});});
    check(levels.map_writes==0,"Draft or unconfirmed level edit changed the save maps");
    level_menu.event({{"id","apply_settings"}});levels.bad_level_signature=true;
    rejects([&]{level_menu.event({{"id","set_tarstone_level"},{"confirmed",true}});});
    check(levels.map_writes==0,"Incompatible equipped interface allowed a partial level edit");
    levels.bad_level_signature=false;
    level_menu.event({{"id","set_tarstone_level"},{"confirmed",true}});
    auto expected=Json{{"Level",1},{"exp",85},{"Stacks",1},{"Durability",12}};
    check(levels.stone_component==expected && levels.stone_runtime==expected,"Level editing lost other fields or skipped a map");
    check(levels.count("SetTarstoneLevel")==1,"Equipped Tarstone was not refreshed");
    for(const auto& call:levels.calls) if(call["function"]=="SetTarstoneLevel") check(call["args"][1]==expected,"Equipped payload lost saved data");
    level_menu.event({{"id","tarstone_level"},{"value",1}});level_menu.event({{"id","apply_settings"}});
    levels.map_writes=0;levels.fail_map_write=2;
    rejects([&]{level_menu.event({{"id","set_tarstone_level"},{"confirmed",true}});});
    check(levels.stone_component==expected && levels.stone_runtime==expected,"Failed level batch did not restore earlier writes");
    levels.fail_map_write=0;levels.fail_equipped_refresh=true;
    rejects([&]{level_menu.event({{"id","set_tarstone_level"},{"confirmed",true}});});
    const auto refreshes=levels.count("SetTarstoneLevel");
    for(int i=0;i<20;++i) level_menu.tick(.25);
    check(levels.count("SetTarstoneLevel")==refreshes,"Equipped refresh automatically retried");
    check(level_menu.stop(),"Level menu failed shutdown");
    Host restart;cheat::Menu restarting(&restart.api);restarting.tick(.25);
    restarting.event({{"id","move_fast"},{"value",true}});restarting.event({{"id","apply_settings"}});
    restart.pawn=object(71);restarting.tick(.25);
    check(restart.movement["WalkSpeed"]==200.,"Pawn restart compounded the speed multiplier");
    restarting.event({{"id","disable_all"}});check(restart.movement["WalkSpeed"]==100.,"Pawn restart lost the original movement baseline");
    restarting.event({{"id","god"},{"value",true}});restarting.event({{"id","apply_settings"}});
    restart.pawn=nullptr;restarting.tick(.25);restart.pawn=object(72);restart.controller=object(73);restarting.tick(.25);
    check(restart.damageable && restarting.model()["values"]["god"]==false,"Cheats carried across controller/save transition");
    check(restarting.stop(),"Restart cleanup failed");
    Host intro;cheat::PrologueRecovery recovery(&intro.api);
    recovery.start();recovery.tick(1);check(intro.count("ResetPlayerState")==0 && !recovery.running(),"Normal state triggered recovery");
    intro.intro_lock=true;recovery.start();for(int i=0;i<8;++i) recovery.tick(1);
    check(intro.count("ResetPlayerState")==0,"Intro recovery skipped its observation period");
    recovery.tick(1);check(intro.count("ResetPlayerState")==1 && !intro.intro_lock,"Verified intro lock was not cleared");
    intro.intro_lock=true;recovery.start();for(int i=0;i<10;++i) recovery.tick(1);
    check(intro.count("ResetPlayerState")==1,"Cleanup repeated on the same ability");
    for(int scenario=0;scenario<5;++scenario) {
        Host unsafe;unsafe.intro_lock=true;
        if(scenario==0) unsafe.intro_done=false;
        if(scenario==1) unsafe.map_unlocked=false;
        if(scenario==2) unsafe.montage=true;
        if(scenario==3) unsafe.intro_instances=2;
        if(scenario==4) unsafe.tag_count=2;
        cheat::PrologueRecovery guard(&unsafe.api);guard.start();for(int i=0;i<12;++i) guard.tick(1);
        check(unsafe.count("ResetPlayerState")==0,"Unverified lock triggered cleanup");
    }
    Host moved;moved.intro_lock=true;cheat::PrologueRecovery transition(&moved.api);transition.start();transition.tick(1);moved.controller=object(99);transition.tick(1);
    check(!transition.running() && moved.count("ResetPlayerState")==0,"Recovery survived a controller transition");
    std::cout<<"Passive boot, shared menu validation, confirmations, exact restore, timed shell coordination and cleanup retry passed\n";
}

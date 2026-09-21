#include "cheat_menu.hpp"
#include "manifest.hpp"
#include <iostream>
#include <source_location>

using cheat::Json;
namespace {
Json object(uint64_t id){return {{"$object",id}};}
struct Host {
    Json state=Json::object();
    Json pawn=object(1),controller=object(2);
    Json movement={{"WalkSpeed",100.},{"JogSpeed",250.},{"SprintSpeed",400.},{"RotationSpeed",580.},{"RotationOverrideTable",{{"$map",Json::array()}}}};
    bool damageable=true,open=false,confirm_switch=true,fail_restore=false,fail_save=false;
    std::string shell="Genessa";
    bool check_soft=false;
    std::string power_fixture;
    bool focused=true,paused=false,input_blocked=false,power_missing=false,power_foreign=false,fail_clone_remove=false,fail_second_clone=false,delayed_clones=false,stance_active=false;
    double health=80.;bool dead=false;
    int spawn_count=1,stance_handle=-1;
    Json primary_clone,secondary_clone;
    Json pressed_keys=Json::object();unsigned input_polls=0;
    bool shell_unlock_fixture=false,bad_unlock_signature=false,fail_unlock_save=false,ignore_unlock_save=false,change_unlock_player=false;
    bool pickup_unlocked=false;
    unsigned pickup_writes=0,save_unlocks=0,actor_count=1;
    Json unlock_tags=Json::array({{{"TagName","CharacterId.Player.Shell.Genessa"}},{{"TagName","CharacterId.Player.Shell.Tiel"}},{{"TagName","CharacterId.Player.Shell.Tiel"}}});
    Json unlocked_equipment=Json::array();
    bool stone_owned=false,stone_manager_registers=false,bad_stone_signature=false,fail_stone_cache=false;
    Json stone_component={{"Level",2},{"exp",85},{"Stacks",1},{"Durability",12}},stone_runtime=stone_component;
    int map_writes=0,fail_map_write=0;
    bool bad_level_signature=false,fail_equipped_refresh=false;
    Json point_limits=Json::array({{{"key",{{"TagName","CharacterId.Player.Shell.Genessa"}}},{"value",44}},{{"key",{{"TagName","CharacterId.Player.Shell.Tiel"}}},{"value",40}},{{"key",{{"TagName","CharacterId.Player.Shell.Other"}}},{"value",150}}});
    bool fail_point_restore=false;
    bool combat_fixture=false,hook_available=true,fail_hook_remove=false;
    unsigned next_hook=1,hook_adds=0,fail_hook_add=0;
    std::map<unsigned,Json> hooks;
    Json cooldown={{"CooldownDuration",-1.},{"GlobalCooldownDuration",1.},{"Cooldown",1},{"GlobalCooldown",2}};
    std::string seal="ID_Seal_Infinite_C";
    bool intro_lock=false,intro_done=true,map_unlocked=true,montage=false,retain_lock=false;
    int intro_instances=1,tag_count=1,selector_count=-1,effect_handle=42;
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
        if(op=="player") return {{"pawn",pawn},{"controller",controller},{"shell","CharacterId.Player.Shell."+shell}};
        if(op=="input.focus") return focused;
        if(op=="input.keys") {++input_polls;Json result=Json::object();for(const auto& key:j.at("keys")) result[key.get<std::string>()]=pressed_keys.value(key.get<std::string>(),false);return result;}
        if(op=="valid") return true;
        if(op=="hooks.status") return {{"available",hook_available},{"rules",Json::array()}};
        if(op=="hooks.add") {
            ++hook_adds;if(fail_hook_add==hook_adds) throw std::runtime_error("Hook installation failed");
            hooks[next_hook]=j;return next_hook++;
        }
        if(op=="hooks.remove") {
            if(fail_hook_remove) throw std::runtime_error("Hook removal pending");
            hooks.erase(j.at("id").get<unsigned>());return true;
        }
        if(op=="menu.close") {open=false;return true;}
        if(op=="menu.status") return {{"menu_open",open}};
        if(op=="load" && shell_unlock_fixture) return object(j.at("path").get<std::string>().find("Shell_Locked")!=std::string::npos?60:61);
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
            else if(!power_fixture.empty() && fn=="GetAvatarActorFromActorInfo") fields={{"ReturnValue",8}};
            else if(!power_fixture.empty() && fn=="HasSecondaryClone") fields={{"ReturnValue",1}};
            else if(!power_fixture.empty() && fn=="RemovePermanentFightStance") fields={{"Immediate",1}};
            else if(!power_fixture.empty() && (fn=="SpawnPrimaryClone" || fn=="SpawnSecondaryClone" || fn=="RemovePrimaryClone" || fn=="RemoveSecondaryClone" || fn=="EnableFightStance" || fn=="TriggerLastShockwave")) {}
            else if(shell_unlock_fixture && (fn=="S_UnlockAllShells" || fn=="CheckForShellUnlock")) {if(bad_unlock_signature && fn=="CheckForShellUnlock") fields={{"NewRequiredArg",8}};}
            else if(shell_unlock_fixture && fn=="GetShellsIDsTagContainer") fields={{"ReturnValue",32},{"__WorldContext",8}};
            else if(shell_unlock_fixture && fn=="UnlockShell") fields={{"ShellId",8},{"Save",1},{"__WorldContext",8}};
            else if(shell_unlock_fixture && fn=="AddUnlockedEquipment") fields={{"ID",8},{"Unlocked",1}};
            else if(shell_unlock_fixture && fn=="GetAllActorsOfClass") fields={{"WorldContextObject",8},{"ActorClass",8},{"OutActors",16}};
            else if(!fn.starts_with("S_AddAllTarstones")) throw std::runtime_error("Unexpected describe: "+fn);
            Json args=Json::object();for(const auto& [name,size]:fields) args[name]={{"size",size},{"return",name=="ReturnValue"},{"out",false}};
            return args;
        }
        if(op=="get") {
            const auto p=j.at("property");
            if(shell_unlock_fixture && p=="PlayerSaveGameObject") return object(62);
            if(shell_unlock_fixture && p=="EquipmentUnlockState") return {{"$map",unlocked_equipment}};
            if(shell_unlock_fixture && p=="ShellUnlocked") return pickup_unlocked;
            if(shell_unlock_fixture && p=="ShellId") return {{"TagName","CharacterId.Player.Shell.Smert"}};
            if(p=="bCanBeDamaged") return damageable;
            if(p=="CharacterData") return object(4);
            if(p=="Movement") return movement;
            if(p=="AbilitySystemComponent") return object(7);
            if(p=="Mesh") return object(8);
            if(p=="WeaponPutInHandBlock") return {{"Handle",effect_handle}};
            if(p=="ActivatableAbilities") {
                if(!power_fixture.empty()) {
                    Json ability=object(70);ability["class"]="BlueprintGeneratedClass /Game/Test."+power_fixture;
                    auto instances=power_missing?Json::array():Json::array({ability});
                    return {{"Items",Json::array({{{"Ability",object(999)},{"NonReplicatedInstances",instances},{"ReplicatedInstances",instances}}})}};
                }
                if(combat_fixture) {
                    Json ability=object(80);ability["class"]="BlueprintGeneratedClass /Game/Test.GA_Parry_Handler_C";
                    return {{"Items",Json::array({{{"Ability",object(999)},{"NonReplicatedInstances",Json::array({ability})},{"ReplicatedInstances",Json::array({ability})}}})}};
                }
                Json ability=object(9);ability["class"]="BlueprintGeneratedClass /Game/Test.GA_Player_Prologue_EggStrandingCustom_C";
                Json instances=Json::array();for(int i=0;i<intro_instances;++i) instances.push_back(ability);
                return {{"Items",Json::array({{{"Ability",ability},{"ActiveCount",1},{"NonReplicatedInstances",instances},{"ReplicatedInstances",Json::array()}}})}};
            }
            if(p=="HealthComponent") return object(5);
            if(p=="SpawnCount") return spawn_count;
            if(p=="CurrentPrimaryClone") return primary_clone;
            if(p=="CurrentSecondaryClone") return secondary_clone;
            if(p=="GE_FightStanceActive") return {{"Handle",stance_handle},{"bPassedFiltersAndWasExecuted",stance_active}};
            if(p=="ActiveSealItemHandle") return object(81);
            if(p=="ItemDef") return {{"$object",82},{"name","BlueprintGeneratedClass /Game/Seals."+seal}};
            if(combat_fixture && j.at("target").at("$object")==80 && cooldown.contains(p.get<std::string>())) return cooldown.at(p.get<std::string>());
            if(combat_fixture && (p=="StoneFormCooldown" || p=="PerfectStoneFormCooldown")) throw std::runtime_error("CSSX property is missing");
            if(p=="HealthSet") return object(6);
            if(p=="Resolve") return {{"CurrentValue",0}};
            if(p=="MaxResolve") return {{"CurrentValue",100}};
            if(p=="Progression Component") return object(44);
            if(p=="StartingMaxShellPoints") return {{"$map",point_limits}};
            if(p=="TarstoneComponent") return object(20);
            if(p=="TarstoneLevels") return {{"$map",stone_owned?Json::array({{{"key",object(90)},{"value",j.at("target")==object(20)?stone_component:stone_runtime}}}):Json::array()}};
            if(p=="EquippedTarstoneItemInstances") return {{"$map",Json::array()}};
            if(p=="EquippedSupportTarstoneItemInstances") return {{"$map",Json::array({{{"key",object(90)},{"value",object(30)}}})}};
        }
        if(op=="map.update") {
            if(j.at("property")=="StartingMaxShellPoints") {
                if(fail_point_restore && j.at("value")!=100) throw std::runtime_error("Point restore failed");
                for(auto& entry:point_limits) if(entry.at("key")==j.at("key")) {
                    if(entry.at("value")!=j.at("expected")) throw std::runtime_error("Stale shell points");
                    entry["value"]=j.at("value");return entry["value"];
                }
                throw std::runtime_error("Point key disappeared");
            }
            ++map_writes;if(fail_map_write==map_writes) throw std::runtime_error("Concurrent level edit");
            auto& value=j.at("target")==object(20)?stone_component:stone_runtime;
            if(j.at("key")!=object(90) || j.at("expected")!=value) throw std::runtime_error("Stale map edit");
            value=j.at("value");return value;
        }
        if(op=="set") {
            if(j.at("property")=="SpawnCount") {spawn_count=j.at("value").get<int>();return spawn_count;}
            if(shell_unlock_fixture && j.at("property")=="ShellUnlocked") {++pickup_writes;pickup_unlocked=j.at("value").get<bool>();return pickup_unlocked;}
            if(combat_fixture && cooldown.contains(j.at("property").get<std::string>())) {cooldown[j.at("property").get<std::string>()]=j.at("value");return j.at("value");}
            if(j.at("property")=="bCanBeDamaged") {if(fail_restore && j.at("value")==true) throw std::runtime_error("restore failed");damageable=j.at("value").get<bool>();return damageable;}
            if(j.at("property")=="Movement") {
                const auto& patch=j.at("value");
                if(patch.contains("RotationOverrideTable")) throw std::runtime_error("CSSX cannot write this reflected property type");
                movement.update(patch);return movement;
            }
        }
        if(op=="call") {
            calls.push_back(j);const auto function=j.at("function");
            if(function=="GetShellItemDefinition") {
                auto name=j.at("args")[0].get<std::string>();if(name=="Lazlo") name="Necrophage";
                return {{"ReturnValue",{{"$object",99},{"name","BlueprintGeneratedClass /Game/Test.ID_Shell_"+name+"_C"}}}};
            }
            if(function=="GetAvatarActorFromActorInfo") return {{"ReturnValue",power_foreign?object(777):pawn}};
            if(function=="IsInGameMenu") return {{"ReturnValue",open}};
            if(function=="IsMoveInputIgnored" || function=="IsLookInputIgnored") return {{"ReturnValue",input_blocked}};
            if(function=="IsGamePaused") return {{"ReturnValue",paused}};
            if(function=="HasSecondaryClone") {
                bool paired=false;for(const auto& [id,hook]:hooks) if(hook.at("function")=="HasSecondaryClone" && hook.at("value")==true) paired=true;
                return {{"ReturnValue",paired}};
            }
            if(function=="SpawnPrimaryClone") {
                bool paired=false;for(const auto& [id,hook]:hooks) if(hook.at("function")=="HasSecondaryClone" && hook.at("value")==true) paired=true;
                if(!delayed_clones) {primary_clone=object(71);if(paired && !fail_second_clone) secondary_clone=object(72);}
                return Json::object();
            }
            if(function=="SpawnSecondaryClone") {if(fail_second_clone) throw std::runtime_error("Second clone failed");if(!delayed_clones) secondary_clone=object(72);return Json::object();}
            if(function=="RemovePrimaryClone") {if(fail_clone_remove) throw std::runtime_error("Clone cleanup failed");primary_clone=nullptr;return Json::object();}
            if(function=="RemoveSecondaryClone") {secondary_clone=nullptr;return Json::object();}
            if(function=="EnableFightStance") {stance_handle=88;stance_active=true;return Json::object();}
            if(function=="RemovePermanentFightStance") {stance_active=false;return Json::object();}
            if(function=="TriggerLastShockwave") return Json::object();
            if(shell_unlock_fixture && function=="GetShellsIDsTagContainer") return {{"ReturnValue",{{"GameplayTags",unlock_tags}}}};
            if(shell_unlock_fixture && function=="GetAllActorsOfClass") {
                check_actor_world(j);Json actors=Json::array();
                for(unsigned i=0;i<actor_count;++i) actors.push_back(object(j.at("args").at("ActorClass")==object(60)?63:64));
                return {{"OutActors",actors}};
            }
            if(shell_unlock_fixture && function=="UnlockShell") return Json::object();
            if(shell_unlock_fixture && function=="AddUnlockedEquipment") {
                ++save_unlocks;if(fail_unlock_save) throw std::runtime_error("Save unlock failed");
                if(!ignore_unlock_save) unlocked_equipment.push_back({{"key",j.at("args").at("ID")},{"value",true}});
                if(change_unlock_player) pawn=object(99);
                return Json::object();
            }
            if(shell_unlock_fixture && function=="CheckForShellUnlock") return Json::object();
            if(function=="GetGameplayTagCount") {
                const bool selector=j.at("args")[0].at("TagName")=="State.Block.Ability.Attack.Selector";
                return {{"ReturnValue",intro_lock?(selector && selector_count>=0?selector_count:tag_count):0}};
            }
            if(function=="ClearLocalCooldown" || function=="ClearGlobalCooldown") return Json::object();
            if(function=="HasPlayedGetUp") return {{"ReturnValue",intro_done}};
            if(function=="IsMapUnlocked") return {{"ReturnValue",map_unlocked}};
            if(function=="GetGameplayEffectFromActiveEffectHandle") {if(!power_fixture.empty()) return {{"ReturnValue",stance_active?object(73):Json()}};auto effect=object(10);effect["class"]="BlueprintGeneratedClass /Game/Test.GE_State_Block_Weapon_PutInHand_Primary_C";return {{"ReturnValue",effect}};}
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
            if(function=="GetShellNames") return {{"ReturnValue",power_fixture.empty()?Json::array({"Genessa","Proxima","ID_Shell_LoadFromSave"}):Json::array({"Genessa","Smert","Lazlo"})}};
            if(function=="GetCharacterID") return {{"ReturnValue",{{"TagName","Shell."+shell}}}};
            if(function=="IsDeadOrDying" || function=="IsDead") return {{"ReturnValue",dead}};
            if(function=="GetShellHealth" || function=="GetHealth") return {{"ReturnValue",health}};
            if(function=="GetMaxShellHealth" || function=="GetMaxHealth") return {{"ReturnValue",100.}};
            if(function=="S_SwitchToShell") {if(confirm_switch)shell=j.at("args")[0].get<std::string>();return Json::object();}
            if(function=="InitialiseCharacterData" || function.get<std::string>().starts_with("S_")) return Json::object();
        }
        throw std::runtime_error("Unexpected test request: "+j.dump());
    }
    unsigned count(const std::string& function)const {unsigned n=0;for(const auto& call:calls) if(call.at("function")==function) ++n;return n;}
    void check_actor_world(const Json& j) {if(j.at("args").at("WorldContextObject")!=pawn) throw std::runtime_error("Actor lookup did not use the current player world");}
};
void check(bool value,const char* text){if(!value) throw std::runtime_error(text);}
template<class F> void rejects(F fn,const std::source_location where=std::source_location::current()){try{fn();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection at line "+std::to_string(where.line()));}
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const auto definition=cssx::read_json(cssx::utf8_path(argv[1]));
    {
        Host h;cheat::Menu m(&h.api);m.tick(.25);m.tick(.25);check(h.input_polls==0,"Unbound shortcuts polled keys");
        m.event({{"id","binding_key"},{"value","F5"}});h.pressed_keys["F5"]=true;m.tick(.25);
        check(h.damageable && h.input_polls==0,"Shortcut draft fired");
        m.event({{"id","apply_settings"}});m.tick(.25);check(h.damageable,"Held key fired immediately after applying");
        h.pressed_keys["F5"]=false;m.tick(.025);h.pressed_keys["F5"]=true;m.tick(.025);
        check(!h.damageable,"Short press between gameplay updates was missed");m.tick(.25);check(!h.damageable,"Held shortcut repeated");
        h.open=true;h.pressed_keys["F5"]=false;m.tick(.025);h.pressed_keys["F5"]=true;m.tick(.025);h.open=false;m.tick(.025);
        check(!h.damageable,"Menu key leaked into gameplay");
        h.pressed_keys["F5"]=false;m.tick(.025);h.pressed_keys["F5"]=true;m.tick(.025);check(h.damageable,"Fresh key press did not toggle off");
        check(h.state["bindings"]["god"]=="F5" && !h.state.contains("god"),"Shortcut preference or passive boot contract broken");
        Host restart;restart.state=h.state;restart.pressed_keys["F5"]=true;cheat::Menu resumed(&restart.api);resumed.tick(.25);resumed.tick(.25);
        check(restart.damageable,"Restored shortcut fired while held at startup");
        restart.pressed_keys["F5"]=false;resumed.tick(.025);restart.focused=false;restart.pressed_keys["F5"]=true;resumed.tick(.025);restart.focused=true;resumed.tick(.025);
        check(restart.damageable,"Focus regain fired held shortcut");
        cssx::validate_model(cssx::bind_menu(definition,m.model()));
    }
    {
        Host h;cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","binding_key"},{"value","F5"}});m.event({{"id","binding_action"},{"value","heal"}});m.event({{"id","binding_key"},{"value","F5"}});
        rejects([&]{m.event({{"id","apply_settings"}});});check(h.state.empty(),"Conflicting shortcuts were saved");
        m.event({{"id","binding_key"},{"value","LeftCtrl+F5"}});rejects([&]{m.event({{"id","apply_settings"}});});
        m.event({{"id","binding_key"},{"value","R3+D-pad Up"}});m.event({{"id","apply_settings"}});m.tick(.025);
        h.pressed_keys["Gamepad_RightThumbstick"]=true;m.tick(.025);check(h.count("S_Heal")==0,"Partial controller chord fired");
        h.pressed_keys["Gamepad_DPad_Up"]=true;m.tick(.025);m.tick(.025);check(h.count("S_Heal")==1,"Controller chord did not fire once");
        m.event({{"id","clear_bindings"}});m.event({{"id","discard_changes"}});check(m.model()["values"]["bindings"].size()==2,"Discard did not restore shortcut draft");
        m.event({{"id","clear_bindings"}});m.event({{"id","apply_settings"}});const auto polls=h.input_polls;m.tick(.25);check(h.input_polls==polls && h.state["bindings"].empty(),"Cleared shortcuts still polled");
    }
    {
        Host h;cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","binding_action"},{"value","switch:Proxima"}});m.event({{"id","binding_key"},{"value","F6"}});
        rejects([&]{m.event({{"id","apply_settings"}});});check(h.state.empty(),"Gameplay shortcut saved without confirmation");
        m.event({{"id","apply_settings"},{"confirmed",true}});m.tick(.025);h.pressed_keys["F6"]=true;m.tick(.1);m.tick(.5);
        check(h.count("S_SwitchToShell")==1 && h.shell=="Proxima","Confirmed shell shortcut failed");
    }
    {
        Host h;h.power_fixture="GA_AstralClones_Action_C";cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","genessa_clones"},{"value",true}});m.tick(1);
        check(h.spawn_count==1 && h.count("SpawnPrimaryClone")==0,"Clone draft changed gameplay");
        m.event({{"id","apply_settings"}});check(h.spawn_count==9999,"Clone spawn count not applied");
        h.open=true;m.tick(1);check(h.count("SpawnPrimaryClone")==0,"Clones spawned in a menu");
        h.open=false;h.focused=false;m.tick(1);check(h.count("SpawnPrimaryClone")==0,"Clones spawned without focus");
        h.focused=true;m.tick(.25);check(h.count("SpawnPrimaryClone")==0,"Clone activation ignored settle delay");
        m.tick(.25);check(h.count("SpawnPrimaryClone")==1 && h.count("SpawnSecondaryClone")==0 && h.hooks.size()==1,"Clone pair did not use the game-owned follow-up");
        m.tick(3);check(h.count("SpawnPrimaryClone")==1,"Clone pair spawned repeatedly");
        h.primary_clone=object(777);m.event({{"id","disable_all"}});
        check(h.primary_clone==object(777) && h.secondary_clone.is_null() && h.spawn_count==1,"Clone cleanup removed a newer actor or lost original count");
        check(h.count("RemovePrimaryClone")==0 && h.count("RemoveSecondaryClone")==1,"Clone cleanup ignored actor ownership");
    }
    for(int failure=0;failure<6;++failure) {
        Host h;h.power_fixture="GA_AstralClones_Action_C";cheat::Menu m(&h.api);m.tick(.25);
        if(failure==0) h.primary_clone=object(777);
        if(failure==1) h.power_foreign=true;
        if(failure==2) h.fail_save=true;
        if(failure==3) h.shell="Tiel";
        if(failure==4) h.hook_available=false;
        if(failure==5) h.fail_hook_add=1;
        m.event({{"id","genessa_clones"},{"value",true}});
        rejects([&]{m.event({{"id","apply_settings"}});});
        check(h.spawn_count==1 && h.count("SpawnPrimaryClone")==0 && h.hooks.empty(),"Rejected clone setup left a mutation");
    }
    {
        Host h;h.power_fixture="GA_AstralClones_Action_C";cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","genessa_clones"},{"value",true}});m.event({{"id","apply_settings"}});
        h.fail_second_clone=true;for(int i=0;i<14;++i) m.tick(.5);
        check(h.count("SpawnPrimaryClone")==1 && h.count("SpawnSecondaryClone")==0,"Failed clone activation retried");
        check(m.model()["values"]["genessa_clones"]==false && !m.stop(),"Unresolved failed spawn was forgotten on unload");
        h.secondary_clone=object(72);m.event({{"id","disable_all"}});
        check(h.primary_clone.is_null() && h.secondary_clone.is_null() && h.spawn_count==1 && h.hooks.empty(),"Late clone completion was not cleaned up");
    }
    {
        Host h;h.power_fixture="GA_AstralClones_Action_C";h.delayed_clones=true;cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","genessa_clones"},{"value",true}});m.event({{"id","apply_settings"}});m.tick(.5);m.tick(.5);
        check(h.count("SpawnPrimaryClone")==1 && h.count("SpawnSecondaryClone")==0,"Delayed pair bypassed the game-owned follow-up");
        rejects([&]{m.event({{"id","disable_all"}});});check(!m.stop(),"Unload forgot a pending clone request");
        h.primary_clone=object(71);h.secondary_clone=object(72);m.event({{"id","disable_all"}});
        check(h.primary_clone.is_null() && h.secondary_clone.is_null() && h.spawn_count==1 && m.stop(),"Delayed clone pair cleanup failed");
    }
    {
        Host h;h.power_fixture="GA_AstralClones_Action_C";cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","genessa_clones"},{"value",true}});m.event({{"id","apply_settings"}});m.tick(.5);
        h.fail_clone_remove=true;check(!m.stop(),"Unload accepted failed clone cleanup");
        h.fail_clone_remove=false;check(m.stop(),"Clone cleanup retry failed");
        check(h.primary_clone.is_null() && h.secondary_clone.is_null() && h.spawn_count==1,"Unload left clone state");
    }
    {
        Host h;h.power_fixture="GA_AstralClones_Action_C";cheat::Menu m(&h.api);m.tick(.25);
        m.event({{"id","genessa_clones"},{"value",true}});m.event({{"id","apply_settings"}});
        h.shell="Tiel";m.tick(.5);
        check(h.count("SpawnPrimaryClone")==0 && h.spawn_count==1,"Same-pawn shell switch kept delayed clone activation");
        check(m.model()["values"]["genessa_clones"]==false,"Lost shell power remained enabled");
    }
    {
        Host h;h.power_fixture="GA_Smert_FightStanceHandler_C";h.shell="Smert";cheat::Menu m(&h.api);m.tick(.25);
        h.stance_active=true;h.stance_handle=77;m.event({{"id","smert_stance"},{"value",true}});
        rejects([&]{m.event({{"id","apply_settings"}});});
        check(h.count("RemovePermanentFightStance")==0,"Pre-existing Smert stance was removed");
        h.stance_active=false;m.event({{"id","apply_settings"}});m.tick(.5);
        check(h.stance_active && h.count("EnableFightStance")==1,"Smert stance did not activate");
        h.stance_handle=99;m.event({{"id","disable_all"}});
        check(h.stance_active && h.count("RemovePermanentFightStance")==0,"Newer Smert effect was removed");
        h.stance_active=false;m.event({{"id","smert_stance"},{"value",true}});m.event({{"id","apply_settings"}});m.tick(.5);
        m.event({{"id","disable_all"}});check(!h.stance_active && h.count("RemovePermanentFightStance")==1,"Owned Smert effect was not removed");
    }
    {
        Host h;h.power_fixture="GA_Lazlo_Detonation_C";h.shell="Necrophage.Default";cheat::Menu m(&h.api);m.tick(.25);
        check(m.model()["enabled"]["lazlo_detonation"]==true,"Lazlo internal shell name was not recognized");
        m.event({{"id","shockwave_interval"},{"value",.5}});m.event({{"id","lazlo_detonation"},{"value",true}});m.event({{"id","apply_settings"}});
        h.paused=true;m.tick(5);check(h.count("TriggerLastShockwave")==0,"Shockwave fired while paused");
        h.paused=false;h.input_blocked=true;m.tick(5);check(h.count("TriggerLastShockwave")==0,"Shockwave bypassed input restriction");
        h.input_blocked=false;m.tick(.25);check(h.count("TriggerLastShockwave")==0,"Shockwave replayed missed shots after pause");
        m.tick(.25);check(h.count("TriggerLastShockwave")==1,"Shockwave interval did not fire");
        m.tick(10);check(h.count("TriggerLastShockwave")==2,"Long frame burst fired shockwaves");
        h.power_missing=true;m.tick(.5);check(h.count("TriggerLastShockwave")==2 && m.model()["values"]["lazlo_detonation"]==false,"Lost Lazlo ability kept firing");
        check(h.state["shockwave_interval"]==.5 && !h.state.contains("lazlo_detonation"),"Power enabled state persisted across launches");
        cssx::validate_model(cssx::bind_menu(definition,m.model()));
    }
    {
        Host h;h.shell_unlock_fixture=true;h.actor_count=2;cheat::Menu m(&h.api);m.tick(.25);
        rejects([&]{m.event({{"id","unlock_shells"}});});
        check(h.count("S_UnlockAllShells")==0,"Shells unlocked without confirmation");
        m.event({{"id","god"},{"value",true}});
        check(m.model()["enabled"]["unlock_shells"]==false,"Shell unlock enabled with pending settings");
        rejects([&]{m.event({{"id","unlock_shells"},{"confirmed",true}});});
        check(h.count("S_UnlockAllShells")==0,"Shells unlocked with pending settings");
        m.event({{"id","discard_changes"}});
        m.event({{"id","unlock_shells"},{"confirmed",true}});
        check(h.count("S_UnlockAllShells")==1 && h.count("S_UnlockShell")==0,"Shell unlock used guessed debug indices");
        check(h.save_unlocks==3 && h.count("UnlockShell")==3,"Tags were duplicated or streamed pickup tag was omitted");
        check(h.pickup_unlocked && h.pickup_writes==1 && h.count("CheckForShellUnlock")==1,"Streamed shell refresh missing or duplicated");
        cssx::validate_model(cssx::bind_menu(definition,m.model()));
    }
    for(int failure=0;failure<4;++failure) {
        Host h;h.shell_unlock_fixture=true;cheat::Menu m(&h.api);m.tick(.25);
        if(failure==0) h.bad_unlock_signature=true;
        if(failure==1) h.unlock_tags=Json::array();
        if(failure==2) h.unlock_tags[0]={{"TagName","Weapon.Invalid"}};
        if(failure==3) h.actor_count=129;
        rejects([&]{m.event({{"id","unlock_shells"},{"confirmed",true}});});
        check(h.count("S_UnlockAllShells")==0 && h.save_unlocks==0 && h.pickup_writes==0,"Failed shell preflight mutated progression");
    }
    for(int failure=0;failure<3;++failure) {
        Host h;h.shell_unlock_fixture=true;cheat::Menu m(&h.api);m.tick(.25);
        h.fail_unlock_save=failure==0;h.ignore_unlock_save=failure==1;h.change_unlock_player=failure==2;
        rejects([&]{m.event({{"id","unlock_shells"},{"confirmed",true}});});
        check(m.model()["error"].get<std::string>().find("Changes may already be saved")!=std::string::npos,"Partial shell unlock was not reported");
        check(h.pickup_writes==0 && h.count("CheckForShellUnlock")==0,"Failed ownership check still collected a shell pickup");
        const auto requests=h.count("S_UnlockAllShells");const auto writes=h.save_unlocks;
        m.tick(2);m.tick(2);
        check(h.count("S_UnlockAllShells")==requests && h.save_unlocks==writes,"Partial shell unlock retried automatically");
    }
    Host host;cheat::Menu menu(&host.api);
    check(host.calls.empty(),"Startup mutated gameplay");
    check(menu.model()["values"]["god"]==false,"God was enabled at startup");
    menu.tick(.25);
    cssx::validate_model(cssx::bind_menu(definition,menu.model()));
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
    host.movement["RotationSpeed"]=700.;host.movement["SprintSpeed"]=1700.;
    menu.event({{"id","move_fast"},{"value",false}});menu.event({{"id","apply_settings"}});check(host.movement["WalkSpeed"]==100.,"Speed did not restore");
    check(host.movement["RotationSpeed"]==700. && host.movement["SprintSpeed"]==1700.,"Cleanup replaced newer movement values");
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
    const auto heals_before=host.count("S_Heal");
    host.health=0.;
    for(int i=0;i<15;++i) menu.tick(.1);
    check(host.count("S_Heal")==heals_before,"Auto Heal ran while player had 0 health");
    host.health=80.;host.dead=true;
    for(int i=0;i<15;++i) menu.tick(.1);
    check(host.count("S_Heal")==heals_before,"Auto Heal ran while player was dead");
    host.dead=false;host.focused=false;
    for(int i=0;i<15;++i) menu.tick(.1);
    check(host.count("S_Heal")==heals_before,"Auto Heal ran while gameplay was not focused/ready");
    host.focused=true;
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
    cssx::validate_model(cssx::bind_menu(definition,stone_menu.model()));
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
    Host points;cheat::Menu point_menu(&points.api);point_menu.tick(.25);
    const auto original_points=points.point_limits;
    point_menu.event({{"id","max_shell_points"},{"value",true}});point_menu.tick(1);
    check(points.point_limits==original_points,"Shell-point draft affected gameplay");
    point_menu.event({{"id","apply_settings"}});
    check(points.point_limits[0]["value"]==100 && points.point_limits[1]["value"]==100,"Shell-point limits did not apply");
    check(points.point_limits[2]["value"]==150,"Higher limit from another owner was lowered");
    points.point_limits[0]["value"]=75;point_menu.tick(1);
    check(points.point_limits[0]["value"]==75,"Newer owner's shell-point limit was overwritten");
    point_menu.event({{"id","disable_all"}});
    check(points.point_limits[0]["value"]==75 && points.point_limits[1]["value"]==40,"Shell-point cleanup lost ownership");
    points.point_limits=original_points;points.fail_save=true;
    point_menu.event({{"id","max_shell_points"},{"value",true}});
    rejects([&]{point_menu.event({{"id","apply_settings"}});});
    check(points.point_limits==original_points,"Failed settings save left point limits enabled");
    points.fail_save=false;point_menu.event({{"id","apply_settings"}});points.fail_point_restore=true;
    check(!point_menu.stop(),"Failed point cleanup allowed unloading");
    points.fail_point_restore=false;check(point_menu.stop() && points.point_limits==original_points,"Point cleanup retry lost original values");
    Host restart;cheat::Menu restarting(&restart.api);restarting.tick(.25);
    restarting.event({{"id","move_fast"},{"value",true}});restarting.event({{"id","apply_settings"}});
    restart.pawn=object(71);restarting.tick(.25);
    check(restart.movement["WalkSpeed"]==200.,"Pawn restart compounded the speed multiplier");
    restarting.event({{"id","disable_all"}});check(restart.movement["WalkSpeed"]==100.,"Pawn restart lost the original movement baseline");
    restarting.event({{"id","god"},{"value",true}});restarting.event({{"id","apply_settings"}});
    restart.pawn=nullptr;restarting.tick(.25);restart.pawn=object(72);restart.controller=object(73);restarting.tick(.25);
    check(restart.damageable && restarting.model()["values"]["god"]==false,"Cheats carried across controller/save transition");
    check(restarting.stop(),"Restart cleanup failed");
    Host combat;combat.combat_fixture=true;cheat::Menu combat_menu(&combat.api);combat_menu.tick(.25);
    const auto original_cooldown=combat.cooldown;
    combat_menu.event({{"id","no_cooldown"},{"value",true}});
    check(combat.hooks.empty() && combat.cooldown==original_cooldown,"Passive cooldown draft edited abilities");
    combat_menu.event({{"id","apply_settings"}});
    check(combat.hooks.size()==2 && combat.cooldown["CooldownDuration"]==0.,"Cooldown did not hook owned instances");
    check(combat.count("ClearLocalCooldown")==1,"Duplicate replicated instance was processed twice");
    for(const auto& [id,hook]:combat.hooks) check(hook.at("target").at("$object")==80 && hook.at("pawn")==combat.pawn,"Hook escaped player instance ownership");
    combat_menu.tick(1.1);check(combat.hook_adds==2,"Unchanged ability list reinstalled hooks");
    combat.cooldown["GlobalCooldownDuration"]=7.;
    combat_menu.event({{"id","disable_all"}});
    check(combat.hooks.empty() && combat.cooldown["CooldownDuration"]==-1. && combat.cooldown["GlobalCooldownDuration"]==7.,"Cooldown cleanup overwrote another writer");
    combat_menu.event({{"id","perfect_parry"},{"value",true}});combat_menu.event({{"id","apply_settings"}});
    check(combat.hooks.size()==3,"Parry hook group incomplete");
    combat.seal="ID_Seal_Stone_C";combat_menu.tick(1.1);
    check(combat.hooks.empty() && combat_menu.model()["values"]["perfect_parry"]==false,"Seal change kept parry enabled");
    check(combat_menu.stop(),"Combat cleanup failed");
    for(int scenario=0;scenario<3;++scenario) {
        Host broken;broken.combat_fixture=true;cheat::Menu candidate(&broken.api);candidate.tick(.25);
        if(scenario==0) broken.hook_available=false;
        if(scenario==1) broken.fail_hook_add=2;
        if(scenario==2) broken.fail_save=true;
        candidate.event({{"id","no_cooldown"},{"value",true}});rejects([&]{candidate.event({{"id","apply_settings"}});});
        check(!candidate.model()["error"].get<std::string>().empty(),"Failed combat apply reported success");
        check(broken.hooks.empty() && broken.cooldown==original_cooldown,"Failed combat apply left edits or hooks behind");
        check(candidate.stop(),"Failed apply prevented cleanup");
    }
    Host retry_combat;retry_combat.combat_fixture=true;cheat::Menu retry_menu(&retry_combat.api);retry_menu.tick(.25);
    retry_menu.event({{"id","no_cooldown"},{"value",true}});retry_menu.event({{"id","apply_settings"}});
    retry_combat.fail_hook_remove=true;check(!retry_menu.stop(),"Busy hook allowed unloading");
    retry_combat.fail_hook_remove=false;check(retry_menu.stop() && retry_combat.hooks.empty() && retry_combat.cooldown==original_cooldown,"Hook cleanup retry lost ownership");
    Host intro;cheat::PrologueRecovery recovery(&intro.api);
    recovery.start();recovery.tick(1);check(intro.count("ResetPlayerState")==0 && !recovery.running(),"Normal state triggered recovery");
    intro.intro_lock=true;recovery.start();for(int i=0;i<8;++i) recovery.tick(1);
    check(intro.count("ResetPlayerState")==0,"Intro recovery skipped its observation period");
    recovery.tick(1);check(intro.count("ResetPlayerState")==1 && !intro.intro_lock,"Verified intro lock was not cleared");
    intro.intro_lock=true;recovery.start();for(int i=0;i<10;++i) recovery.tick(1);
    check(intro.count("ResetPlayerState")==1,"Cleanup repeated on the same ability");
    Host drawing;drawing.intro_lock=true;drawing.selector_count=0;
    cheat::PrologueRecovery drawing_recovery(&drawing.api);drawing_recovery.start();
    for(int i=0;i<8;++i) drawing_recovery.tick(1);
    check(drawing.count("ResetPlayerState")==0,"Draw-only lock skipped its observation period");
    drawing_recovery.tick(1);
    check(drawing.count("ResetPlayerState")==1 && !drawing.intro_lock,"Completed intro kept weapon stowed when attack selection was already unlocked");
    Host recurring;recurring.intro_lock=true;recurring.selector_count=0;
    cheat::PrologueRecovery monitor(&recurring.api);monitor.watch();
    for(int i=0;i<9;++i) monitor.tick(1);
    check(recurring.count("ResetPlayerState")==1 && monitor.running(),"Automatic recovery stopped watching after cleanup");
    recurring.intro_lock=true;
    for(int i=0;i<12;++i) monitor.tick(1);
    check(recurring.count("ResetPlayerState")==1,"Automatic recovery retried the same effect");
    recurring.effect_handle=43;
    for(int i=0;i<8;++i) monitor.tick(1);
    check(recurring.count("ResetPlayerState")==1,"Recurring lock skipped its observation period");
    monitor.tick(1);
    check(recurring.count("ResetPlayerState")==2 && !recurring.intro_lock,"Automatic recovery missed a newly applied effect on the same ability");
    recurring.intro_lock=true;recurring.effect_handle=44;recurring.montage=true;
    for(int i=0;i<12;++i) monitor.tick(1);
    check(recurring.count("ResetPlayerState")==2,"Automatic recovery interrupted animation");
    recurring.montage=false;for(int i=0;i<9;++i) monitor.tick(1);
    check(recurring.count("ResetPlayerState")==3,"Automatic recovery failed to resume after animation");
    for(int scenario=0;scenario<6;++scenario) {
        Host unsafe;unsafe.intro_lock=true;
        if(scenario==0) unsafe.intro_done=false;
        if(scenario==1) unsafe.map_unlocked=false;
        if(scenario==2) unsafe.montage=true;
        if(scenario==3) unsafe.intro_instances=2;
        if(scenario==4) unsafe.tag_count=2;
        if(scenario==5) unsafe.selector_count=2;
        cheat::PrologueRecovery guard(&unsafe.api);guard.start();for(int i=0;i<12;++i) guard.tick(1);
        check(unsafe.count("ResetPlayerState")==0,"Unverified lock triggered cleanup");
    }
    Host moved;moved.intro_lock=true;cheat::PrologueRecovery transition(&moved.api);transition.start();transition.tick(1);moved.controller=object(99);transition.tick(1);
    check(!transition.running() && moved.count("ResetPlayerState")==0,"Recovery survived a controller transition");
    std::cout<<"Passive boot, shared menu validation, confirmations, exact restore, timed shell coordination and cleanup retry passed\n";
}

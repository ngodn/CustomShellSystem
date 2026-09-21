#include "data.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace css;
static unsigned checks;
static void expect(bool value,const char* message) { ++checks; if(!value) throw std::runtime_error(message); }
static void rejects(const std::function<void()>& action) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    expect(rejected,"Invalid animation data accepted");
}
int main() {
    const auto dir=fs::temp_directory_path()/"css-animation-tests";
    fs::create_directories(dir);
    try {
        const Json definitions=Json::parse(R"({
          "idle": [
            {"id":"relaxed","name":"Relaxed","clip":"/Game/CSS/Eve/AN_Idle.AN_Idle","hide_weapons":true},
            {"id":"armed","name":"Armed","clip":"/Game/CSS/Eve/AN_Ready.AN_Ready",
             "by_weapon":{"Weapon.MartyrsBlade":"/Game/CSS/Eve/AN_Blade.AN_Blade"}},
            {"id":"blade","name":"Blade","by_weapon":{"Weapon.MartyrsBlade":"/Game/CSS/Eve/AN_Blade.AN_Blade"}}
          ],
          "walk":[{"id":"eve","name":"Eve","blend_space":"/Game/CSS/Eve/BS_Walk.BS_Walk"}],
          "jog":[{"id":"eve","name":"Eve","blend_space":"/Game/CSS/Eve/BS_Jog.BS_Jog"}],
          "sprint":[{"id":"eve","name":"Eve","blend_space":"/Game/CSS/Eve/BS_Sprint.BS_Sprint"}],
          "beacon":[{"id":"graceful","name":"Graceful","depart":"/Game/CSS/Eve/AN_Kneel.AN_Kneel",
                     "arrive":"/Game/CSS/Eve/AN_Rise.AN_Rise"}]
        })");
        const auto set=AnimationSet::parse(definitions);
        expect(set.slots.size()==5,"Animation slots lost");
        const auto& idle=set.slots.at(AnimationSlot::Idle);
        auto pose=resolve_animation(idle,AnimationSlot::Idle,"relaxed","Weapon.MartyrsBlade");
        expect(pose.option && pose.hide_weapons && pose.asset==idle[0].clip,"Common unarmed idle not resolved");
        pose=resolve_animation(idle,AnimationSlot::Idle,"armed","Weapon.MartyrsBlade");
        expect(pose.option && !pose.hide_weapons && pose.asset==idle[1].by_weapon.at("Weapon.MartyrsBlade"),"Exact weapon override lost");
        pose=resolve_animation(idle,AnimationSlot::Idle,"armed","Weapon.NewWeapon");
        expect(pose.option && pose.asset==idle[1].clip,"Unlisted weapon did not use declared common idle");
        for(const auto* weapon:{"","Weapon.NewWeapon","Weapon.MartyrsBlade.Extra"})
            expect(!resolve_animation(idle,AnimationSlot::Idle,"blade",weapon).option,"Weapon-only idle leaked to another weapon");
        for(const auto* choice:{"","original","removed-option"}) {
            const auto fallback=resolve_animation(idle,AnimationSlot::Idle,choice,"Weapon.MartyrsBlade");
            expect(!fallback.option && fallback.asset.empty() && !fallback.hide_weapons,"Default or missing choice changed weapon visibility");
        }
        for(const auto slot:{AnimationSlot::Walk,AnimationSlot::Jog,AnimationSlot::Sprint}) {
            const auto& options=set.slots.at(slot);
            auto selected=resolve_animation(options,slot,"eve");
            expect(selected.option && selected.asset==options[0].blend_space && !selected.hide_weapons,"Gait did not resolve independently");
        }
        const auto beacon=resolve_animation(set.slots.at(AnimationSlot::Beacon),AnimationSlot::Beacon,"graceful");
        expect(beacon.option && !beacon.option->arrive.empty() && !beacon.hide_weapons,"Beacon pair incomplete");

        Json outfit=Json::parse(R"({
          "id":"eve","name":"Eve","shells":["CharacterId.Player.Shell.Genessa"],
          "variants":[
            {"id":"pearl","name":"Pearl","mesh":"/Game/CSS/Eve/SK_Pearl.SK_Pearl"},
            {"id":"other","name":"Other","mesh":"/Game/CSS/Eve/SK_Other.SK_Other",
             "animations":{"idle":[],"jog":[{"id":"slow","name":"Slow","blend_space":"/Game/CSS/Eve/BS_Slow.BS_Slow"}]}}
          ]
        })");
        outfit["animations"]=definitions;
        const auto file=dir/"eve.css.json";
        auto load=[&](const Json& value) { atomic_json(file,{{"schema",1},{"outfits",{value}}},false);return Catalog::load(dir); };
        const auto catalog=load(outfit);
        expect(catalog.animation_options("eve","pearl",AnimationSlot::Idle).size()==3,"Outfit animations not inherited");
        expect(catalog.animation_options("eve","other",AnimationSlot::Idle).empty(),"Empty variant slot did not disable inheritance");
        expect(catalog.animation_options("eve","other",AnimationSlot::Walk)[0].id=="eve","Unmentioned variant slot did not inherit");
        expect(catalog.animation_options("eve","other",AnimationSlot::Jog)[0].id=="slow","Variant option did not replace outfit slot");
        expect(catalog.animation_options("eve","missing",AnimationSlot::Idle).empty(),"Missing variant borrowed outfit animation");
        expect(catalog.animation_options("other","pearl",AnimationSlot::Idle).empty(),"Animation leaked across outfits");
        auto legacy=outfit;legacy.erase("animations");legacy["variants"][1].erase("animations");
        expect(load(legacy).animation_options("eve","pearl",AnimationSlot::Walk).empty(),"Legacy catalog acquired animation overrides");

        for(const Json& bad:Json::array({nullptr,true,"idle",Json::array(),{{"idlle",Json::array()}},{{"walk",Json::object()}}}))
            rejects([&]{AnimationSet::parse(bad);});
        const std::vector<std::function<void(Json&)>> mutations={
            [](Json& d){d["idle"][0]["id"]="original";},
            [](Json& d){d["idle"].push_back(d["idle"][0]);},
            [](Json& d){d["idle"][0]["hide_weapons"]="true";},
            [](Json& d){d["idle"][1]["hide_weapons"]=true;},
            [](Json& d){d["idle"][0].erase("clip");},
            [](Json& d){d["idle"][2]["by_weapon"]=Json::object();},
            [](Json& d){d["idle"][2]["by_weapon"]={{"Weapon..Blade","/Game/CSS/A.A"}};},
            [](Json& d){d["idle"][2]["by_weapon"]={{"Ability.Attack","/Game/CSS/A.A"}};},
            [](Json& d){d["idle"][2]["by_weapon"]={{"Weapon.MartyrsBlade",false}};},
            [](Json& d){d["walk"][0]["blend_space"]="/Game/CSS/../A.A";},
            [](Json& d){d["walk"][0]["clip"]="/Game/CSS/A.A";},
            [](Json& d){d["sprint"][0]["hide_weapons"]=true;},
            [](Json& d){d["beacon"][0].erase("arrive");},
            [](Json& d){d["beacon"][0]["notify_times"]=Json::array({1,2,3});},
            [](Json& d){d["beacon"][0]["name"]="";},
            [](Json& d){d["jog"]=Json::array();for(int i=0;i<65;++i)d["jog"].push_back({{"id","j"+std::to_string(i)},{"name","Jog"},{"blend_space","/Game/CSS/A.A"}});}
        };
        for(const auto& mutate:mutations) {
            auto d=definitions;mutate(d);auto malformed=outfit;malformed["animations"]=d;
            rejects([&]{load(malformed);});
            malformed=outfit;malformed["variants"][0]["animations"]=d;
            rejects([&]{load(malformed);});
        }

        State state;
        state.walk_animation="feminine";
        state.animation_choices=AnimationChoices::parse({{"eve",{{"pearl",{{"idle","relaxed"},{"walk","original"},{"jog","eve"},{"sprint","eve"},{"beacon","graceful"}}}}},
                                                       {"missing.mod",{{"missing.variant",{{"idle","missing.option"}}}}}});
        state.presets["all"]={state.selections,state.walk_animation,state.animation_choices};
        const auto restored=State::parse(state.json());
        expect(restored.json()==state.json(),"Animation state/profile round trip lost choices");
        expect(restored.presets.at("all").animation_choices==state.animation_choices,"Profile lost animation snapshot");
        expect(restored.animation_choices.get("missing.mod","missing.variant",AnimationSlot::Idle)=="missing.option","Uninstalled mod choice discarded");
        expect(restored.animation_choices.get("eve","pearl",AnimationSlot::Walk)=="original","Explicit Default choice lost");
        expect(restored.animation_choices.find("eve","pearl",AnimationSlot::Walk)!=nullptr &&
               restored.animation_choices.find("eve","other",AnimationSlot::Walk)==nullptr,
               "Explicit Default cannot be distinguished from legacy unset behavior");
        expect(restored.animation_choices.get("eve","other",AnimationSlot::Jog)=="original","Variant borrowed another variant choice");
        expect(restored.walk_animation=="feminine","Legacy feminine walk setting changed");
        auto old=state.json();old.erase("animation_choices");old["presets"]["all"].erase("animation_choices");
        const auto migrated=State::parse(old);
        expect(migrated.animation_choices.outfits.empty() && migrated.presets.at("all").animation_choices.outfits.empty(),"Legacy save/profile enabled new animations");
        auto old_profile=old;old_profile["presets"]["all"]=Json::object();
        expect(State::parse(old_profile).presets.at("all").animation_choices.outfits.empty(),"Bare legacy profile migration failed");
        for(const Json& invalid:Json::array({nullptr,{{"../bad",Json::object()}},{{"eve",{{"../bad",Json::object()}}}},
            {{"eve",{{"pearl",{{"run","eve"}}}}}},{{"eve",{{"pearl",{{"idle",false}}}}}},{{"eve",{{"pearl",{{"idle","../bad"}}}}}}})) {
            auto bad=state.json();bad["animation_choices"]=invalid;
            rejects([&]{State::parse(bad);});
            bad=state.json();bad["presets"]["all"]["animation_choices"]=invalid;
            rejects([&]{State::parse(bad);});
        }
        fs::remove_all(dir);
        std::cout<<checks<<" animation checks passed\n";
    } catch(const std::exception& e) {
        fs::remove_all(dir);std::cerr<<e.what()<<'\n';return 1;
    }
}

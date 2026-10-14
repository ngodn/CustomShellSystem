#include "data.hpp"
#include "profile_input.hpp"
#include <iostream>
#include <stdexcept>

static void check(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
static void rejected(css::State& state,const std::string& name,const std::string& reason) {
    const auto before=state.json();
    std::string error;
    try { css::save_profile_snapshot(state,name); }
    catch(const std::runtime_error& e) { error=e.what(); }
    check(error==reason,"Save did not explain the specific name/capacity error");
    check(state.json()==before,"Rejected profile save changed state");
}
int main(int argc,char** argv) {
    {
        const std::string shell="CharacterId.Player.Shell.Genessa";
        css::State state;
        state.darkform_mirror_cleaned=true;
        state.selections[shell]={"test.profile","regular",{}};
        state.misc_rules["seal"]={"hidden"};
        css::save_profile_snapshot(state,"installed");
        state.misc_rules["seal"]={"in_use"};
        state.walk_animation="feminine";
        state.doubles={false,false};
        const auto before=state.json();
        auto preset=state.presets.at("installed");
        css::Catalog catalog;
        auto reject=[&](const std::string& reason) {
            std::string error;
            try {
                css::restore_profile_snapshot(state,preset,catalog,shell);
            } catch(const std::runtime_error& e) { error=e.what(); }
            check(error.find(reason)!=std::string::npos,"Unavailable profile was not rejected before settings changed");
            check(state.json()==before,"Unavailable profile changed current settings or saved choices");
        };
        reject("Profile outfit is unavailable: test.profile");
        css::Outfit outfit;
        outfit.id="test.profile";
        outfit.shells={shell};
        css::Variant variant;
        variant.id="regular";
        outfit.variants.push_back(variant);
        catalog.outfits.push_back(outfit);
        preset.selections[shell].variant="removed";
        reject("Profile variant is unavailable: test.profile/removed");
        preset.selections[shell].variant="regular";
        catalog.outfits[0].shells={"CharacterId.Player.Shell.Tiel"};
        reject("Profile outfit does not support the current shell");
        catalog.outfits[0].shells={shell};
        const auto selected=css::profile_selection(preset,catalog,state,shell);
        check(selected.choice && selected.choice->outfit=="test.profile" && selected.choice->variant=="regular",
              "Reinstalled profile did not resolve without resaving");
        check(state.json()==before,"Preflight changed saved state");
        check(css::restore_profile_settings(state,preset) && state.misc_rules.at("seal").mode=="hidden",
              "Available profile no longer restores its settings");
        check(!css::profile_selection(preset,catalog,state,"CharacterId.Player.Shell.Eredrim").choice,
              "A profile without a shell selection invented an outfit");
        preset.selections["CharacterId.Player.Shell.Eredrim"]={"absent.other","missing",{}};
        check(css::profile_selection(preset,catalog,state,shell).choice.has_value(),
              "An unavailable selection for another shell blocked the current shell");
    }
    {
        const std::string living="CharacterId.Player.Shell.Genessa";
        const std::string stray="CharacterId.Player.Darkform.CorruptedGenessa";
        css::Catalog catalog;
        css::Outfit outfit;
        outfit.id="test.mirror";
        outfit.same_skeleton=true;
        for(const auto* id:{"saved","separate","current"}) {
            css::Variant variant; variant.id=id; outfit.variants.push_back(variant);
        }
        catalog.outfits.push_back(outfit);
        css::State state;
        state.darkform_mirror_cleaned=true;
        state.last_living_shell=living;
        state.selections[living]={outfit.id,"current",{}};
        state.selections[stray]={outfit.id,"separate",{}};
        css::Preset profile;
        profile.selections=state.selections;
        profile.selections[living].variant="saved";
        const auto before=state.json();
        const auto selected=css::profile_selection(profile,catalog,state,stray);
        check(selected.choice && selected.choice->variant=="saved" && selected.slot==living,
              "Mirrored profile loaded the separate Darkform choice instead of the saved living choice");
        check(state.json()==before,"Resolving a mirrored profile changed saved state");
        state.harbinger_mirror=false;
        const auto separate=css::profile_selection(profile,catalog,state,stray);
        check(separate.choice && separate.choice->variant=="separate" && separate.slot==stray,
              "Profile ignored disabled mirroring");
        state.harbinger_mirror=true;
        state.last_living_shell.clear();
        check(css::profile_selection(profile,catalog,state,stray).slot==living,
              "Profile did not infer the living source after a severed restart");
        profile.selections.erase(stray);
        check(css::profile_selection(profile,catalog,state,stray).choice.has_value(),
              "Profile without a separate Darkform slot lost its mirrored outfit");
        profile.selections.clear();
        const auto native=css::profile_selection(profile,catalog,state,stray);
        check(!native.choice,"Default profile invented a mirrored outfit");
        css::restore_profile_snapshot(state,profile,catalog,stray);
        check(state.selections.empty(),"Default profile left an old mirrored or separate choice to reapply");
        profile.selections[living]={outfit.id,"saved",{}};
        profile.selections[stray]={outfit.id,"separate",{}};
        profile.selections["CharacterId.Player.Shell.Tiel"]={"absent.other","old",{}};
        profile.misc_rules["sidearm"]={"hidden"};
        profile.has_misc_rules=true;
        const auto loaded=css::restore_profile_snapshot(state,profile,catalog,stray);
        check(loaded.slot==living && loaded.choice && loaded.choice->variant=="saved",
              "Restoring the snapshot lost the mirrored choice");
        check(state.selections.at(stray).variant=="separate" &&
              state.selections.at("CharacterId.Player.Shell.Tiel").outfit=="absent.other",
              "Restoring a profile overwrote its independent or unavailable other-shell choice");
        check(state.misc_rules.at("sidearm").mode=="hidden","Snapshot lost global settings");
        const auto restored=state.json();
        css::restore_profile_snapshot(state,profile,catalog,stray);
        check(state.json()==restored,"Repeated mirrored profile restore changed state");
        const auto restarted=css::State::parse(restored);
        check(css::appearance_selection_key(restarted,catalog,stray)==living &&
              restarted.selections.at(stray).variant=="separate",
              "Restart lost the mirror source or independent form choice");
    }
    {
        css::State defaults;
        defaults.darkform_mirror_cleaned=true;
        css::save_profile_snapshot(defaults,"all-default");
        auto loaded=css::State::parse(defaults.json());
        loaded.misc_rules["sidearm"]={"in_use"};
        loaded.misc_rules["seal"]={"hidden"};
        check(css::restore_profile_settings(loaded,loaded.presets.at("all-default")),
              "Loading an explicit Default visibility profile did not report a change");
        check(loaded.misc_rules.empty(),"Default visibility profile retained later overrides");
        check(!css::restore_profile_settings(loaded,loaded.presets.at("all-default")),
              "Loading the same profile again dirtied state");
        for(const css::Json invalid:{css::Json{},css::Json::array(),css::Json("default")}) {
            auto malformed=defaults.json();
            malformed["presets"]["all-default"]["misc_rules"]=invalid;
            auto recovered=css::State::parse(malformed);
            recovered.misc_rules["seal"]={"hidden"};
            css::restore_profile_settings(recovered,recovered.presets.at("all-default"));
            check(recovered.misc_rules.at("seal").mode=="hidden",
                  "Malformed profile visibility data cleared valid current settings");
        }
        for(const bool bare:{false,true}) {
            auto legacy=defaults.json();
            auto& profile=legacy["presets"]["all-default"];
            if(bare) profile=profile.at("selections").get<css::Json>();
            else profile.erase("misc_rules");
            auto old=css::State::parse(legacy);
            check(!old.presets.at("all-default").has_misc_rules,"Legacy profile invented visibility settings");
            check(!old.json()["presets"]["all-default"].contains("misc_rules"),
                  "Saving a legacy profile invented a Default visibility reset");
            old=css::State::parse(old.json());
            old.misc_rules["seal"]={"hidden"};
            old.doubles={false,false};
            const auto before=old.misc_rules;
            css::restore_profile_settings(old,old.presets.at("all-default"));
            check(old.misc_rules==before,"Legacy profile reset visibility it never stored");
            if(bare) check(!old.doubles.faithful_css && !old.doubles.stray_css,
                           "Bare legacy profile reset doubles it never stored");
        }
        defaults.misc_rules["seal"]={"hidden"};
        defaults.walk_animation="feminine";
        defaults.animation_choices.outfits["eins0fx.commanderwhite"]["original"][css::AnimationSlot::Walk]="eve";
        defaults.doubles={false,true};
        css::save_profile_snapshot(defaults,"custom");
        loaded=css::State::parse(defaults.json());
        loaded.misc_rules.clear();
        loaded.walk_animation="normal";
        loaded.animation_choices={};
        loaded.doubles={true,false};
        const auto saved=loaded.presets.at("custom");
        check(css::restore_profile_settings(loaded,saved),"Custom profile did not restore settings");
        check(loaded.misc_rules==defaults.misc_rules && loaded.walk_animation==defaults.walk_animation &&
              loaded.animation_choices==defaults.animation_choices && loaded.doubles==defaults.doubles,
              "Custom profile lost a saved visibility, animation or doubles setting");
        check(loaded.json()["presets"]==defaults.json()["presets"],"Loading a profile rewrote stored profiles");
    }
    check(css::ProfileSaveShortcut::key=="Enter","Save prompt uses a text-inserting key");
    css::ProfileSaveShortcut shortcut;
    check(!shortcut.update(true,false),"Entering the profile tab submitted a save");
    check(shortcut.update(true,true),"Enter did not submit while editing the name");
    check(!shortcut.update(true,true),"Held Enter submitted repeatedly");
    check(!shortcut.update(false,true),"Background or other page submitted a profile");
    check(!shortcut.update(true,true),"Held Enter saved on returning to the page");
    shortcut.update(true,false);
    check(shortcut.update(true,true),"Second Enter press was ignored");
    css::State state;
    rejected(state,"","Enter a profile name, for example profile.1.");
    for(const auto name:{"Eve Black Pearl"," profile.1","profile.1 ","../profile",".","..","Eve/1","Eve:1"})
        rejected(state,name,"Use A-Z, 0-9, periods, underscores or hyphens; no spaces.");
    rejected(state,std::string(97,'a'),"Profile names must be 96 characters or fewer.");
    state.walk_animation="feminine";
    state.misc_rules["seal"]={"hidden"};
    css::save_profile_snapshot(state,"profile.1");
    check(state.presets.size()==1,"Suggested profile name failed to create a slot");
    css::save_profile_snapshot(state,"Eve_Black-Pearl.2");
    check(state.presets.size()==2,"A second saved profile failed");
    auto restored=css::State::parse(state.json());
    check(restored.presets.size()==2 && restored.presets.at("profile.1").walk_animation=="feminine" &&
          restored.presets.at("profile.1").misc_rules.at("seal").mode=="hidden",
          "Profile snapshot did not survive serialization");
    for(int i=3;i<=64;++i) css::save_profile_snapshot(state,"profile."+std::to_string(i));
    rejected(state,"profile.65","All 64 profiles are used. Replace or delete a saved profile.");
    state.walk_animation="normal";
    css::save_profile_snapshot(state,"profile.1");
    check(state.presets.size()==64 && state.presets.at("profile.1").walk_animation=="normal",
          "Replacing a profile at capacity failed");
    state.presets.erase("profile.1");
    css::save_profile_snapshot(state,std::string(96,'a'));
    check(css::State::parse(state.json()).presets.size()==64,"Maximum-length valid name failed");
    size_t snapshots=0,profiles=0;
    for(int i=1;i<argc;++i) {
        const auto input=css::read_json(argv[i]);
        const auto base=css::State::parse(input);
        const auto canonical=base.json();
        check(css::State::parse(canonical).json()==canonical,"Archived snapshot failed a second round trip");
        for(const auto* key:{"selections","animation_choices","presets","remembered_custom","favorites","misc_rules","doubles"})
            if(input.contains(key)) check(canonical.at(key)==input.at(key),"Archived saved settings changed on load");
        for(const auto& [name,profile]:base.presets) {
            auto working=base;
            css::restore_profile_settings(working,profile);
            check(working.walk_animation==profile.walk_animation && working.animation_choices==profile.animation_choices,
                  "Archived profile animation choices changed on restore");
            if(profile.has_misc_rules || !profile.misc_rules.empty())
                check(working.misc_rules==profile.misc_rules,"Archived profile visibility changed on restore");
            else check(working.misc_rules==base.misc_rules,"Archived legacy profile reset visibility");
            check(working.doubles==profile.doubles.value_or(base.doubles),"Archived profile doubles changed on restore");
            check(working.json()["selections"]==canonical["selections"] &&
                  working.json()["remembered_custom"]==canonical["remembered_custom"] &&
                  working.json()["presets"]==canonical["presets"],
                  "Restoring global profile settings changed appearances or stored profiles");
            check(!css::restore_profile_settings(working,profile),"Repeated archived profile restore dirtied state");
            ++profiles;
        }
        ++snapshots;
    }
    std::cout<<"Profile validation, snapshots, round trips and capacity checks passed; "
             <<snapshots<<" archived snapshots and "<<profiles<<" stored profiles checked\n";
}

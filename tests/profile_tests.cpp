#include "data.hpp"
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
int main() {
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
    std::cout<<"Profile validation, snapshots, round trips and capacity checks passed\n";
}

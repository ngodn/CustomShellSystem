#include "hand_support_catalog.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using namespace css::hand_support;
using Json=nlohmann::json;
static void check(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
static void reject(const Json& value) {
    try {Catalog::parse(value);} catch(const std::exception&) {return;}
    throw std::runtime_error("Invalid calibration catalog was accepted");
}
int main(int argc,char** argv) {
    check(argc==2,"Supply the shipped calibration catalog");
    const auto catalog=Catalog::load(argv[1]);
    std::ifstream stream(argv[1]);const auto document=Json::parse(stream);
    check(catalog.bindings().size()==9,"Shipped binding count changed");
    // Eve standalones (1.0.0) carry their own copies of the Black Pearl and Peony bindings.
    check(catalog.match("/Game/CSS/CSS_EveBlackPearl_eins0fx_P/SK_BlackPearl2.SK_BlackPearl2",
                        "/Game/CSS/CSS_EveBlackPearl_eins0fx_P/ABP_Secondary.ABP_Secondary_C")!=nullptr,"Standalone Black Pearl binding missing");
    check(catalog.match("/Game/CSS/CSS_EvePeony_eins0fx_P/PeonyV2/Meshes/SK_PeonyV2.SK_PeonyV2",
                        "/Game/CSS/CSS_EvePeony_eins0fx_P/ABP_Secondary.ABP_Secondary_C")!=nullptr,"Standalone Peony binding missing");
    std::set<std::string> variants;
    for(const auto& binding:catalog.bindings()) {
        check(catalog.match(binding.mesh,binding.animation)==&binding,"Paired binding rejected");
        check(!catalog.match(binding.mesh+"_Copy",binding.animation),"Unlisted mesh accepted");
        check(!catalog.match(binding.mesh,binding.animation+"_Copy"),"Unlisted animation accepted");
        variants.insert(binding.grip_profile);
        for(const auto& row:document.at("weapons")) {
            const auto weapon=row.at("id").get<std::string>();
            check(catalog.weapon_class(row.at("class").get<std::string>())==weapon,"Weapon lookup failed");
            check(catalog.profile(binding.finger_profile,weapon),"Missing shipped finger target");
            check(catalog.grip(binding.grip_profile,weapon),"Missing shipped grip");
        }
    }
    check(variants.size()==5,"Variant-specific grips were conflated");
    check(!catalog.match("","") && catalog.weapon_class("unknown").empty(),"Unknown identity accepted");
    check(!catalog.grip("missing","scythe") && !catalog.profile("eve","missing"),"Unknown calibration accepted");
    check(catalog.grip("peony","scythe")->rotation[0]==-0.65528999619983574,"Measured grip value changed");
    check(catalog.profile("eve","scythe")->standing[0][0]==0.087865941234158071,"Measured finger value changed");

    auto custom=document;
    custom["bindings"][0]["mesh"]="/Game/Example/CustomMesh.CustomMesh";
    custom["bindings"][0]["candidate"]="/Game/Example/CustomRig.CustomRig_C";
    custom["weapons"][0]["id"]="custom_weapon";
    custom["weapons"][0]["class"]="/Game/Example/Weapon.Weapon_C";
    for(const auto key:{"grips","targets"}) for(auto& row:custom[key])
        if(row["weapon"]=="scythe") row["weapon"]="custom_weapon";
    custom["grips"][0]["translation"][0]=-8.;
    auto changed=Catalog::parse(custom);
    check(changed.match("/Game/Example/CustomMesh.CustomMesh",catalog.bindings()[0].animation),
          "New mesh still depends on a compiled allowlist");
    check(changed.weapon_class("/Game/Example/Weapon.Weapon_C")=="custom_weapon" &&
          changed.grip("peony","custom_weapon")->translation[0]==-8.,"Catalog edits did not reach runtime data");
    check(catalog.grip("peony","scythe")->translation[0]!=-8.,"Separate catalogs share mutable calibration");

    auto bad=document;bad["schema"]=2;reject(bad);
    bad=document;bad["rig_contract"]="unknown";reject(bad);
    bad=document;bad["bindings"].push_back(bad["bindings"][0]);reject(bad);
    bad=document;bad["weapons"].push_back(bad["weapons"][0]);reject(bad);
    bad=document;bad["grips"].push_back(bad["grips"][0]);reject(bad);
    bad=document;bad["targets"].push_back(bad["targets"][0]);reject(bad);
    bad=document;bad["bindings"][0]["grip_profile"]="missing";reject(bad);
    bad=document;bad["targets"][0]["weapon"]="missing";reject(bad);
    bad=document;bad["grips"].erase(bad["grips"].begin());reject(bad);
    bad=document;bad["targets"][0]["standing"][0]={0.,0.,0.,0.};reject(bad);
    bad=document;bad["targets"][0]["standing"][0]={1.,0.,0.};reject(bad);
    bad=document;bad["targets"][0]["standing"].erase(0);reject(bad);
    bad=document;bad["target_bones"][0]="pelvis";reject(bad);
    bad=document;bad["grips"][0]["translation"][0]=std::numeric_limits<double>::infinity();reject(bad);
    bad=document;bad["grips"][0]["translation"][0]=1000.;reject(bad);
    bad=document;bad["bindings"][0]["mesh"]="../../other";reject(bad);
    bad=document;bad["bindings"][0]["output_bones"]={"hand_r"};reject(bad);
    bad=document;bad["carry"]["blend_in_seconds"]=0.;reject(bad);
    bad=document;bad["carry"]["wrist_height"]={5.,-24.};reject(bad);
    const auto temporary=std::filesystem::temp_directory_path()/
        ("css-hand-catalog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto reject_file=[&] {
        try {Catalog::load(temporary);} catch(const std::exception&) {return;}
        throw std::runtime_error("Invalid calibration file was accepted");
    };
    reject_file();
    {std::ofstream file(temporary);file<<"{\"schema\":1,\"schema\":2}";}
    reject_file();
    {std::ofstream file(temporary);file<<std::string(32,'[')<<"0"<<std::string(32,']');}
    reject_file();
    {std::ofstream file(temporary);file<<std::string(1024*1024+1,' ');}
    reject_file();std::filesystem::remove(temporary);

    const auto& settings=catalog.carry();
    CarryInput input{"scythe",true,true,true,false,-12.,.12};CarryBlend carry;
    check(input.supported(settings),"Measured shoulder pose rejected");
    check(carry.update(input,.1,settings)==0.,"Weapon change reused carry weight");
    for(int i=0;i<15;++i)carry.update(input,.01,settings);
    check(std::abs(carry.value()-1.)<1e-9,"Carry entry blend changed");
    input.action=true;for(int i=0;i<6;++i)carry.update(input,.01,settings);
    check(carry.value()<1e-9,"Attack/parry retained carry target");
    input.action=false;carry.update(input,.1,settings);check(carry.value()>0.,"Carry did not return");
    input.weapon="hammer";check(carry.update(input,.01,settings)==0.,"Weapon swap reused old weight");
    carry.update(input,.1,settings);input.enabled=false;
    check(carry.update(input,0.,settings)==0.,"Default game did not disable immediately");
    input.enabled=true;input.held=false;check(!input.supported(settings),"Sheathed weapon accepted");
    input.held=true;input.grounded=false;check(!input.supported(settings),"Airborne pose accepted");
    input.grounded=true;input.wrist_height=-40.;check(!input.supported(settings),"Lower carry accepted");
    input.wrist_height=-12.;input.prop_local_z=std::numeric_limits<double>::quiet_NaN();
    check(!input.supported(settings),"Invalid prop pose accepted");
    check(moving_phase(.12,settings)==0. && moving_phase(3.3,settings)==1.,"Gait anchors changed");
    check(std::abs(moving_phase(1.525,settings)-.5)<1e-9,"Gait transition is discontinuous");
    std::cout<<"Catalog loading, edits, rejection and carry behavior passed\n";
}

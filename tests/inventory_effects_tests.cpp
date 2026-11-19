#include "inventory_effects.hpp"
#include <iostream>
#include <stdexcept>

using namespace css::inventory_effects;
int main() {
    unsigned checks=0;
    auto check=[&](bool ok,const char* message){if(!ok) throw std::runtime_error(message);++checks;};
    for(auto name:{"FlowFabric","FlowPanel","FlowStrap"}) {
        auto leaf=std::string("M_PeonyV2_")+name;
        for(auto prefix:{"/Game/CSS/eins0fx/EveSB/","/Game/CSS/CSS_EvePeony_eins0fx_P/"})
            check(driver(prefix+std::string("PeonyV2/Materials/")+leaf+"."+leaf)==Driver::peony_phase,"Peony flow");
    }
    check(driver("/Game/CSS/eins0fx/tHornyProxima/Preview/Liara04/Materials/M_LiaraAtlas.M_LiaraAtlas")==Driver::profane_seconds,"Profane metal");
    check(driver("/Game/CSS/eins0fx/tHornyProxima/Materials/Final/M_WaterHuman.M_WaterHuman")==Driver::none,"anchored droplets");
    check(driver("/Other/M_PeonyV2_FlowPanel.M_PeonyV2_FlowPanel")==Driver::none,"unrelated root");
    PausedClock clock{120.,78.5};
    check(clock.sample(Driver::peony_phase,.125f,2.f,121.125)==.375f,"periodic phase with authored speed");
    check(clock.sample(Driver::peony_phase,.125f,0.f,121.125)==.125f,"stationary phase");
    check(clock.sample(Driver::profane_seconds,0,1,120.25)==78.75f,"world phase plus dilated unpaused time");
    check(clock.sample(Driver::profane_seconds,3,0,121.25)==3,"authored fixed seconds");
    check(clock.sample(Driver::profane_seconds,1.5f,.5f,120.25)==40.875f,"blended time");
    check(cloth_tick_offset(0xb60,0xbf0,0xd00,0x28,0x30,0xa)==0xbc0,"5.6 tick layout");
    check(cloth_tick_offset(0xb60,0xbf0,0xb80,0x28,0x30,0xa)<0,"component bounds");
    check(cloth_tick_offset(0xb60,0xbf4,0xd00,0x28,0x30,0xa)<0,"tick alignment");
    check(cloth_tick_offset(0xb60,0xbf0,0xd00,0x28,0x38,0xa)<0,"different tick type");
    for(bool desired:{true,false}) {
        PreviewFlag flag;
        bool preview=!desired;
        int writes=0;
        auto set=[&](bool value){preview=value;++writes;};
        flag.sync(preview,true,desired,set);
        flag.sync(preview,true,desired,set);
        check(preview==desired && writes==1 && flag.owned,"one preview write across maintenance");
        flag.sync(preview,false,desired,set);
        check(preview!=desired && writes==2 && !flag.owned,"restore on exit or suspension");
        preview=desired;flag.sync(preview,true,desired,set);flag.sync(preview,false,desired,set);
        check(preview==desired && writes==2,"preserve already-enabled native flag");
        preview=!desired;flag.sync(preview,true,desired,set);
        preview=!desired;flag.sync(preview,false,desired,set);
        check(writes==3 && !flag.owned,"external reset before close");
    }
    ClothBlend blend;
    float weight=0;
    auto set_weight=[&](float value){weight=value;};
    blend.sync(weight,1.f,set_weight);blend.sync(weight,.5f,set_weight);
    check(weight==.5f && blend.original==0.f,"mirror changing world blend");
    blend.sync(weight,{},set_weight);
    check(weight==0.f && !blend.original,"restore preview blend");
    blend.sync(weight,.5f,set_weight);weight=.75f;blend.sync(weight,{},set_weight);
    check(weight==.75f && !blend.original,"respect external blend replacement");
    int restored=-1;
    auto set_material=[&](int id){restored=id;};
    restore_slot(7,7,true,42,19,set_material);
    check(restored==42,"restore current world override");
    restore_slot(7,7,true,0,19,set_material);
    check(restored==0,"restore implicit mesh default");
    restore_slot(7,7,false,42,19,set_material);
    check(restored==19,"source changed mesh");
    restore_slot(9,7,true,42,19,set_material);
    check(restored==19,"leave externally replaced slot alone");
    std::cout<<checks<<" preview effect checks passed\n";
}

#include "hand_support.hpp"
#include "hand_support_targets.hpp"
#include "hand_support_grips.hpp"
#include <limits>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

static void check(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
int main() {
    using namespace css::hand_support;
    const std::string stock="/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/SK_Sester_Genessa_V6.SK_Sester_Genessa_V6";
    for(const auto& binding:bindings) {
        check(match(binding.mesh,binding.animation)==&binding,"Verified outfit binding rejected");
        check(!match(stock,binding.animation),"Stock shell accepted through a custom animation class");
        check(!match(binding.mesh,"/Game/Sparta/Characters/Humans/Player/Animations/ABP_Player.ABP_Player_C"),"Main player animation class accepted");
        check(!match(std::string(binding.mesh)+"_Copy",binding.animation),"Unaudited copied mesh accepted");
        check(!match(binding.mesh,std::string(binding.animation)+"_Copy"),"Unaudited animation copy accepted");
        for(const auto& other:bindings)
            if(other.animation!=binding.animation) check(!match(binding.mesh,other.animation),"Unpaired animation accepted");
    }
    check(!match("",""),"Empty identity accepted");
    std::set<Variant> variants;
    for(const auto& binding:bindings) {
        variants.insert(binding.variant);
        check(binding.variant!=Variant::none,"Audited binding has no grip identity");
        for(const auto weapon:{Weapon::scythe,Weapon::hadern,Weapon::needle,Weapon::hammer,Weapon::battle_axe}) {
            const auto* g=grip(binding.variant,weapon);check(g,"Audited binding has no weapon grip");
            double norm=0.;for(const auto v:g->rotation)norm+=v*v;
            check(std::isfinite(norm) && std::abs(norm-1.)<1e-12,"Grip rotation is not normalized");
            for(const auto v:g->translation)check(std::isfinite(v) && std::abs(v)<25.,"Grip offset is outside the measured hand envelope");
        }
    }
    check(variants.size()==5,"Grip variants are missing or conflated");
    check(!grip(Variant::none,Weapon::scythe) && !grip(Variant::peony,Weapon::none),"Unknown grip identity accepted");
    check(std::set<std::string_view>(arm_bones.begin(),arm_bones.end())==
          std::set<std::string_view>{"upperarm_r","lowerarm_r","hand_r","prop_r"},"Arm output transfer changed scope");
    std::set<std::string_view> bones(right_bones.begin(),right_bones.end());
    check(bones.size()==19,"Finger set contains duplicate or missing entries");
    for(const auto bone:bones) {
        check(bone.ends_with("_r"),"Right-hand stage includes the other hand");
        check(bone.starts_with("thumb_") || bone.starts_with("index_") || bone.starts_with("middle_") ||
              bone.starts_with("ring_") || bone.starts_with("pinky_"),"Finger stage includes a wrist, prop or body bone");
    }
    CarryInput input{Weapon::scythe,true,true,true,false,-12.,.12};
    CarryBlend carry;
    check(input.supported(),"Measured shoulder pose rejected");
    check(carry.update(input,.1)==0.,"Weapon change started at full carry weight");
    for(int i=0;i<15;++i)carry.update(input,.01);
    check(std::abs(carry.value()-1.)<1e-9,"Carry did not settle after the entry blend");
    input.action=true;
    for(int i=0;i<6;++i)carry.update(input,.01);
    check(carry.value()<1e-9,"Attack or parry retained the carry target");
    input.action=false;carry.update(input,.1);check(carry.value()>0.,"Carry did not return after the action");
    input.weapon=Weapon::hammer;check(carry.update(input,.01)==0.,"Weapon swap reused the old carry weight");
    carry.update(input,.1);input.enabled=false;
    check(carry.update(input,0.)==0.,"Default game did not disable carry immediately");
    input.enabled=true;input.held=false;check(!input.supported(),"Sheathed weapon accepted");
    input.held=true;input.grounded=false;check(!input.supported(),"Airborne pose accepted");
    input.grounded=true;input.wrist_height=-40.;check(!input.supported(),"Lower carry pose accepted");
    input.wrist_height=-12.;input.prop_local_z=std::numeric_limits<double>::quiet_NaN();
    check(!input.supported(),"Invalid prop pose accepted");
    check(moving_phase(.12)==0. && moving_phase(3.3)==1.,"Measured gait anchors changed");
    check(moving_phase(1.525)>.499 && moving_phase(1.525)<.501,"Mid-transition phase is discontinuous");
    check(weapon_class("/Game/Sparta/Core/Weapons/Player/BlackNeedle/WP_BlackNeedle.WP_BlackNeedle_C")==Weapon::needle,"Measured weapon class rejected");
    check(weapon_class("/Game/Sparta/Core/Weapons/Player/BlackNeedle/WP_BlackNeedle.WP_BlackNeedle_C_Copy")==Weapon::none,"Unaudited weapon class accepted");
    for(const auto family:{Family::eve,Family::unholy,Family::commander})
        for(const auto weapon:{Weapon::scythe,Weapon::hadern,Weapon::needle,Weapon::hammer,Weapon::battle_axe}) {
            const auto* p=profile(family,weapon);check(p,"Missing measured carry profile");
            for(const auto& q:p->standing) {
                double norm=0.;for(const auto v:q) norm+=v*v;
                check(std::isfinite(norm) && std::abs(norm-1.)<1e-12,"Profile rotation is not a unit quaternion");
            }
        }
    check(!profile(Family::none,Weapon::scythe) && !profile(Family::eve,Weapon::none),"Unknown carry profile accepted");
    std::cout << "Hand support boundaries, interruption blends and profile rotations passed\n";
}

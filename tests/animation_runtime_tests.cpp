#include "animation_override.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace css;
using Object=std::pair<int,int>; // Object index and generation, like a weak engine handle.
using Lease=AnimationOverrideLease<Object>;
static unsigned checks;
static void expect(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
static bool same(const Lease::Value& a,const Lease::Value& b) {return a.object==b.object && a.enabled==b.enabled;}
int main() {
    try {
        const Lease::Value css_walk{{1,10},true},css_jog{{2,10},true},other{{3,20},true};
        for(const Lease::Value baseline:{Lease::Value{{0,0},false},Lease::Value{{4,30},false},Lease::Value{{4,30},true}}) {
            Lease lease;
            expect(!lease.restoration(baseline) && !lease.engaged(),"Unarmed lease restored state");
            lease.claim(baseline,css_walk);
            expect(lease.owns(css_walk),"Acquired override not owned");
            for(int i=0;i<100;++i) lease.claim(css_walk,css_walk);
            expect(same(*lease.restoration(css_walk),baseline),"Repeated update recaptured CSS as original");
            lease.claim(css_walk,css_jog);
            expect(same(*lease.restoration(css_jog),baseline),"Gait transition lost original pair");
            expect(!lease.restoration(other),"Release overwrote another mod's replacement");
            Lease::Value flag_only=css_jog;flag_only.enabled=false;
            expect(!lease.restoration(flag_only),"Release overwrote another writer's flag change");
            Lease::Value recycled=css_jog;++recycled.object.second;
            expect(!lease.restoration(recycled),"Recycled object address mistaken for ownership");
            lease.claim(other,css_walk);
            expect(same(*lease.restoration(css_walk),other),"Reacquisition lost latest external baseline");
            lease.reset();
            expect(!lease.engaged() && !lease.restoration(css_walk) && !lease.original(),"Level/reset retained lease");
            lease.claim(flag_only,css_walk);
            expect(same(*lease.restoration(css_walk),flag_only),"Disabled external pointer not preserved");
        }
        Lease old_instance,new_instance;
        const Lease::Value before{{5,1},true},after{{5,2},false};
        old_instance.claim(before,css_walk);new_instance.claim(after,css_jog);
        expect(same(*old_instance.restoration(css_walk),before) && same(*new_instance.restoration(css_jog),after),
               "Animation instance replacement mixed baselines");
        for(bool enabled:{false,true}) for(bool player:{false,true}) for(bool thread:{false,true}) {
            const float wanted=enabled && player && thread?85.f:184.f;
            expect(feminine_walk_speed(184.f,enabled,player,thread)==wanted,"Speed hook affected wrong owner/thread/state");
        }
        for(float speed:{0.f,85.f,150.f,200.f,540.f,800.f})
            expect(feminine_walk_speed(speed,true,true,true)==speed,"Non-walk speed changed");
        expect(std::isnan(feminine_walk_speed(std::numeric_limits<float>::quiet_NaN(),true,true,true)),"NaN became a movement speed");
        expect(std::isinf(feminine_walk_speed(std::numeric_limits<float>::infinity(),true,true,true)),"Infinity became a movement speed");
        expect(animation_gait(0,true,false,true)==AnimationGait::Idle,"Walk toggle replaced stationary idle");
        expect(animation_gait(85,true,false,true)==AnimationGait::Walk,"Walking flag lost");
        expect(animation_gait(30,false,false,true)==AnimationGait::Jog,"Jog acceleration mistaken for walk");
        expect(animation_gait(30,false,true,true)==AnimationGait::Sprint,"Sprint acceleration mistaken for jog");
        expect(animation_gait(540,false,false,true)==AnimationGait::Jog,"Jog not classified");
        expect(animation_gait(800,false,true,true)==AnimationGait::Sprint,"Sprint not classified");
        expect(animation_gait(85,true,true,true)==AnimationGait::None,"Conflicting gait flags accepted");
        expect(animation_gait(85,{},false,true)==AnimationGait::None && animation_gait(85,false,{},true)==AnimationGait::None,
               "Custom movement guessed a missing gait flag");
        expect(animation_gait(85,{}, {},false)==AnimationGait::Walk && animation_gait(540,{}, {},false)==AnimationGait::Jog &&
               animation_gait(800,{}, {},false)==AnimationGait::Sprint,"Legacy speed fallback changed");
        expect(animation_gait(-1,false,false,true)==AnimationGait::None &&
               animation_gait(std::numeric_limits<double>::quiet_NaN(),false,false,true)==AnimationGait::None,
               "Invalid velocity used for animation selection");
        std::cout<<checks<<" animation runtime checks passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

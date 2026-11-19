#include "animations.hpp"
#include "facial_performance.hpp"
#include "inventory_idle.hpp"
#include <iostream>
using namespace css;

int main() {
    int checks=0;
    auto expect=[&](bool valid,const char* reason) {
        if(!valid) throw std::runtime_error(reason);
        ++checks;
    };
    AnimationOption eve;
    eve.id="eve.default";eve.clip="/Game/CSS/Eve/Idle.Idle";eve.saved_aliases={"old.eve"};
    std::vector<AnimationOption> options{eve};
    AnimationChoices saved;
    saved.outfits["white"]["coat"][AnimationSlot::Idle]="old.eve";
    saved.outfits["white"]["coat"][AnimationSlot::Vista]="other.vista";
    auto select=[&](const std::string& variant,const std::string& override_choice,bool legacy) {
        return selected_idle_choice(options,saved.find("white",variant,AnimationSlot::Idle),override_choice,legacy);
    };
    expect(resolve_animation(options,AnimationSlot::Idle,select("coat","",false)).asset==eve.clip,"Lost bundled alias");
    expect(select("coat","missing",false)=="old.eve","Unavailable override displaced saved idle");
    expect(select("base","eve.default",false)=="eve.default","Available global override was missed");
    expect(select("base","",true)==feminine_animation_id,"Legacy Feminine was lost");
    saved.outfits["white"]["base"][AnimationSlot::Idle]="original";
    expect(select("base","",true)=="original","Explicit Default must win over legacy Feminine");

    InventoryIdleChoice choice{"white","coat",select("coat","",false),"",1};
    InventoryIdlePlayback playback;
    constexpr std::string_view human="/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    std::string_view source=authored_base_skeleton,target=authored_cw_hair_skeleton;
    bool layer=true,carrier=false,pending=false;
    int starts=0,errors=0;
    std::string pose="game",error;
    const auto stop=[&]{pose="game";};
    const auto sync=[&](uint64_t now,bool same_owner=true) {
        try {
            playback.sync(choice,same_owner,now,stop,[&] {
                ++starts;
                if(pending) return 0.;
                require_inventory_idle_rig(choice.idle==feminine_animation_id,layer,carrier,source,target);
                pose=choice.idle;
                return 2.;
            });
        } catch(const std::runtime_error& e) { ++errors;error=e.what(); }
    };
    sync(1000);
    expect(pose=="old.eve" && playback.playing(),"Worn variant idle did not start");
    sync(1250);
    expect(starts==1 && playback.position(1250)==.25,"Maintenance restarted the clock");
    expect(playback.position(990)==0.,"Bind after the frame timestamp wrapped the clock");
    choice.idle=feminine_animation_id;source=human;layer=false;
    sync(1300);
    expect(pose==feminine_animation_id && playback.position(1300)==0.,"Feminine change while open failed");
    choice.variant="base";choice.idle=select("base","",false);
    sync(1400);
    expect(!playback.playing() && pose=="game","Default retained the previous variant's pose");

    choice.outfit="eve";choice.variant="black.pearl";choice.idle="shared.iris";
    source=target=authored_base_skeleton;layer=true;
    sync(2000);
    expect(pose=="shared.iris","Outfit switch missed shared idle");
    FacialPerformance curve{"shared.iris",2.,{{"jaw",{{0.,0.f},{2.,1.f}}}}};
    FacialPerformancePose face(curve,{{"jaw",0.f}});
    expect(face.sample(playback.position(3500),1.).at("jaw")==.75f,"Face did not follow the body clock");
    expect(playback.position(4500)==.5 && face.sample(playback.position(4500),1.).at("jaw")==.25f,
           "Body and face did not wrap together");
    playback.reset(stop);
    expect(!playback.playing() && pose=="game","Leaving CSS retained an override");
    sync(5000);
    expect(pose=="shared.iris" && playback.position(5000)==0.,"Reopen failed to reacquire the same choice");
    sync(5500,false);
    expect(playback.position(5500)==0.,"Reconstructed animation instance retained an old clock");

    choice.variant="unsupported";layer=false;
    sync(6000);
    expect(errors==1 && error.find("idle layer")!=std::string::npos && !playback.playing() && pose=="game",
           "Missing rig did not release to the menu pose with a reason");
    sync(6100);
    expect(errors==1,"The same failed rig logged twice");
    carrier=true;source=target=human;
    sync(6200,false);
    expect(playback.playing(),"Native carrier owner could not recover a failed selection");
    target="/unknown";
    sync(6300,false);
    expect(errors==2 && error.find("skeleton")!=std::string::npos && pose=="game","Wrong skeleton acquired the pose");
    source=target=human;pending=true;
    sync(6400,false);
    expect(!playback.playing() && pose=="game","Pending load displaced the game pose");
    pending=false;
    sync(6500);
    expect(playback.playing() && starts>1,"Pending load did not retry");
    std::cout<<checks<<" inventory idle checks passed\n";
}

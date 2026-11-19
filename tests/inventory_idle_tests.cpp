#include "animation_override.hpp"
#include "animation_library.hpp"
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
    LibraryAnimation entry;
    entry.id="idle.nl2";
    entry.slot=AnimationSlot::Idle;
    entry.assets={{"eve",std::string(authored_base_skeleton),{}},{"nl2",std::string(human),"nl2-grid"}};
    AnimationLibrary library;
    library.entries={entry};
    const std::vector<AnimationLibrary> libraries{library};
    for(auto rig:{std::string_view(human),authored_base_skeleton,authored_cw_belt_skeleton,
                  authored_wing_skeleton,legacy_base_skeleton,seductress_idle_skeleton}) {
        const auto match=resolve_library_animation(libraries,AnimationSlot::Idle,entry.id,{},std::string(rig));
        expect(match.candidates.size()==1,"idle resolver selected an incompatible alternative");
        const auto& candidate=match.candidates.front();
        expect(candidate.asset==(rig==human?"nl2":"eve"),"idle resolver changed the reference pose");
        expect(idle_carrier_path(*match.entry,candidate)=="nl2-grid","missing idle grid on a layerless human look");
    }
    expect(resolve_library_animation(libraries,AnimationSlot::Idle,entry.id,{},"creature").candidates.empty(),
           "unsupported rig did not keep the game pose");
    expect(resolve_library_animation(libraries,AnimationSlot::Walk,entry.id,{},std::string(human)).candidates.empty(),
           "walking accepted an idle carrier");
    int game_node=0,carrier_node=1,slot_node=2;
    int* root=&game_node;
    auto set_root=[&](int* node){root=node;};
    acquire_idle_link(root,&game_node,&carrier_node,set_root);
    expect(root==&carrier_node,"native idle passed through hand IK");
    restore_idle_link(root,&carrier_node,&game_node,set_root);
    expect(root==&game_node,"walking did not reacquire the game output");
    acquire_idle_link(root,&game_node,&slot_node,set_root);
    root=&carrier_node;
    restore_idle_link(root,&slot_node,&game_node,set_root);
    expect(root==&carrier_node,"page exit replaced a newer output");
    bool rejected=false;
    try { acquire_idle_link(root,&game_node,&slot_node,set_root); } catch(const std::runtime_error&) { rejected=true; }
    expect(rejected && root==&carrier_node,"unrecognized root displaced the game pose");
    expect(world_idle_output(false,true,false,true,false)==IdleOutput::carrier,"settled third-party idle did not bypass IK");
    expect(world_idle_output(false,true,false,false,true)==IdleOutput::carrier,"settled Feminine missed its carrier");
    expect(world_idle_output(false,true,false,false,false)==IdleOutput::game,"authored post-process route changed");
    for(auto speed:{85.,540.,800.}) {
        auto gait=animation_gait(speed,speed==85.,speed==800.,true);
        expect(gait!=AnimationGait::Idle && world_idle_output(false,false,true,true,false)==IdleOutput::game,
               "movement retained direct idle output");
    }
    expect(world_idle_output(true,true,false,true,false)==IdleOutput::game,"Vista lost its game output");
    expect(compatible_idle_skeletons(authored_base_skeleton,seductress_idle_skeleton) &&
           !compatible_animation_skeletons(authored_base_skeleton,seductress_idle_skeleton),"idle alias changed gait matching");
    expect(!compatible_idle_skeletons(authored_base_skeleton,"/Other/Skeleton"),"unverified skeleton selected an idle");
    std::cout<<checks<<" inventory idle checks passed\n";
}

#include "idle_output.hpp"
#include "arm_stretch.hpp"
#include "data.hpp"
#include "inventory_idle.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>

using namespace css;
using Json=nlohmann::json;

int main(int argc,char** argv) {
    if(argc!=2) throw std::runtime_error("Expected the cooked ABP_Player graph fixture");
    std::ifstream input(argv[1]);
    const auto graph=Json::parse(input);
    const auto& nodes=graph.at("nodes");
    std::vector<const Json*> properties;
    for(const auto& node:nodes) properties.push_back(&node);
    const auto lookup=[&](std::wstring_view name) {
        return idle_graph_property<const Json>(properties,name,[](const Json* node) {
            const auto& text=node->at("name").get_ref<const std::string&>();
            return std::wstring(text.begin(),text.end());
        });
    };
    int checks=0;
    const auto check=[&](bool ok,const char* reason) {
        if(!ok) throw std::runtime_error(reason);
        ++checks;
    };
    check(graph.at("steam_build")==26640691 && graph.at("class")==
        "/Game/Sparta/Characters/Humans/Player/Animations/ABP_Player.ABP_Player_C","Wrong cooked player class");
    const auto* root=lookup(player_idle_root);
    const auto* source=lookup(player_idle_source);
    const auto* carrier=lookup(player_idle_carrier);
    const auto* slot=lookup(player_idle_slot);
    check(root && root->at("graph")=="AnimGraph" && root->at("type")=="AnimNode_Root","Picked a layer root");
    check(nodes.front().at("graph")=="LateUpdate" && nodes.front().at("link")!=root->at("link"),
          "Missing the shipped numbered root that collides in a comparison-index lookup");
    const auto* incoming=&nodes.at(root->at("link").get<size_t>());
    check(incoming==source && source->at("instance")=="AnimBlueprintGeneratedClass'ABPL_Humanoid_HeadTracking_C'",
          "Root did not connect to the shipped head-tracking node");
    check(carrier && carrier->at("type")=="AnimNode_BlendSpacePlayer","Missing world idle carrier");
    check(slot && slot->at("slot")=="DefaultSlot" && slot->at("type")=="AnimNode_Slot","Wrong Inventory slot");
    check(!lookup(L"AnimGraphNode_Root_99"),"Missing node matched a numbered sibling");

    for(auto* target:{carrier,slot}) {
        const Json* current=incoming;
        auto set=[&](const Json* value){current=value;};
        IdleDirectFallback fallback;
        int warnings=0,attempts=0;
        auto log=[&](const char*){++warnings;};
        auto bind=[&]{
            ++attempts;
            acquire_idle_link(current,source,target,set);
        };
        fallback.attempt(0,bind);
        check(current==target && !fallback.active(),"Direct link failed on the real graph topology");
        restore_idle_link(current,target,incoming,set);
        check(current==incoming,"Failed to restore the gameplay source");

        // An unknown connection keeps evaluating the already-started BlendSpace or montage.
        current=&nodes.front();
        for(int tick=0;tick<4;++tick) {
            fallback.attempt(tick*150,bind);
            fallback.report("previous idle path",log);
        }
        check(fallback.active() && current==&nodes.front(),"Fallback changed the gameplay connection");
        check(warnings==1 && attempts==2,"Maintenance repeatedly tried or logged the same rejected link");
        fallback.attempt(1000,bind);
        fallback.report("previous idle path",log);
        check(warnings==1,"Reselecting the same fallback repeated the warning");
        current=incoming;
        fallback.attempt(2000,bind);
        check(current==target && !fallback.active(),"Could not recover when the engine linked the original graph");
    }
    ArmStretchPair shipped{};
    const auto stretch_count=std::count_if(nodes.begin(),nodes.end(),[](const Json& row) {
        return row.value("bAllowStretching",false);
    });
    check(stretch_count==arm_ik_nodes.size(),"Unaccounted stretch-enabled node in the shipped graph");
    for(size_t i=0;i<arm_ik_nodes.size();++i) {
        const auto* arm=lookup(arm_ik_nodes[i]);
        check(arm && arm->at("type")=="AnimNode_TwoBoneIK" && arm->at("bone")==(i==0?"hand_l":"hand_r"),
              "Arm cap addresses the wrong node or limb");
        check(arm->at("exposed_destinations")==Json::array({Json::array({"Alpha"})}),
              "Animation update can overwrite a stretch setting");
        shipped[i]={arm->at("bAllowStretching").get<bool>(),arm->at("MaxStretchScale").get<double>()};
        check(shipped[i]==ArmStretchValues{true,1.2} && arm->at("StartStretchRatio")==1.0,"Shipped stretch inputs changed");
        check(locked_arm_pair[i]==ArmStretchValues{false,1.0},"Locked arm could still stretch");
    }
    ArmStretchLease arms;
    arms.lock(shipped);
    arms.lock(locked_arm_pair);
    check(arms.restoration(locked_arm_pair)==shipped,"Game mode lost a shipped arm setting");
    ArmStretchPair reinit={ArmStretchValues{true,1.35},ArmStretchValues{true,1.07}};
    arms.lock(reinit);
    check(arms.restoration(locked_arm_pair)==reinit,"Same-instance reinit retained stale scales");
    auto external=locked_arm_pair;
    external[1].maximum=1.14;
    check(arms.restoration(external)[1].maximum==1.14,"Release overwrote another writer");
    arms.reset();
    ArmStretchPair replacement={ArmStretchValues{false,1.05},ArmStretchValues{true,1.0}};
    arms.lock(replacement);
    check(arms.restoration(locked_arm_pair)==replacement,"Swapping instances leaked the previous arm settings");
    arms.reset();
    check(!arms.bound() && arms.restoration(shipped)==shipped,"Game mode claimed an untouched instance");
    State settings;
    check(settings.arm_stretch=="locked","New installs must default to Locked");
    auto saved=settings.json();
    saved.erase("arm_stretch");
    check(State::parse(saved).arm_stretch=="locked","Upgraded installs must default to Locked");
    saved["arm_stretch"]=42;
    check(State::parse(saved).arm_stretch=="locked","Invalid saved stretch mode did not fall back");
    settings.arm_stretch="game";
    check(State::parse(settings.json()).arm_stretch=="game","Game did not survive save and reload");

    InventoryIdlePlayback menu;
    InventoryIdleChoice choice;
    choice.idle="eve.default";
    IdleDirectFallback menu_fallback;
    const Json* current=&nodes.front();
    bool montage=false;
    int starts=0;
    auto place=[&](uint64_t now) {
        menu_fallback.attempt(now,[&]{
            acquire_idle_link(current,source,slot,[&](const Json* value){current=value;});
        });
    };
    auto update=[&](uint64_t now) {
        if(montage) place(now);
        menu.sync(choice,montage,now,[&]{montage=false;},[&]{
            ++starts;
            montage=true;
            place(now);
            return 4.;
        });
    };
    update(100);
    update(600);
    check(montage && menu_fallback.active() && menu.position(600)==0.5 && starts==1,
          "Rejected direct link stopped or restarted the Inventory montage");
    current=incoming;
    update(1100);
    check(montage && current==slot && menu.position(1100)==1. && starts==1,
          "Inventory lost the clip phase when direct output recovered");
    std::cout<<checks<<" cooked player graph, fallback and arm stretch checks passed\n";
}

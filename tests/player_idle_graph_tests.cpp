#include "idle_output.hpp"
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
    std::cout<<checks<<" cooked player graph and fallback checks passed\n";
}

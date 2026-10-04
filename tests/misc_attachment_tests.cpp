#include "misc_attachment.hpp"
#include <iostream>
#include <stdexcept>

static void check(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}

int main() {
    using namespace css;
    // Cooked BP_Attachable_Item_Heart attaches its DefaultSceneRoot here.
    constexpr auto heart="BP_Attachable_Item_Heart_C";
    constexpr auto socket="Socket_Prop_Stowed_InfiniteSeal_Right";
    check(misc_attachment_component("SceneComponent",heart),
          "Heart of Vatra was skipped before MISC could hide its child mesh");
    check(misc_known_scene_accessory(heart),"Heart of Vatra has no individual accessory control");
    check(misc_category(socket,heart)=="accessories","Quest heart was mistaken for the seal");
    check(misc_attachment_component("StaticMeshComponent",heart),"Heart mesh fallback was rejected");
    check(!misc_known_scene_accessory("WP_AlienHeart_C"),"Revered Heart was mistaken for the quest item");
    check(misc_attachment_component("SceneComponent","BP_Gragu_Helmet_C"),"Helmet root regressed");
    for(const auto owner:{"BP_Attachable_Item_C","BP_Attachable_Item_Heart_Other_C","BP_Unknown_C",""})
        check(!misc_attachment_component("SceneComponent",owner),"Unknown scene root entered MISC");
    for(const auto component:{"NiagaraComponent","AudioComponent","SceneComponentSubclass",""})
        check(!misc_attachment_component(component,heart),"Non-mesh effect entered MISC");
    check(misc_category(socket,"BP_InfiniteSeal_C")=="seal","Actual seal category regressed");
    check(misc_category("Socket_Prop_Stowed_NailShotgun","WP_NailShotgun_C")=="sidearm","Stowed gun regressed");
    check(misc_category("Socket_Prop_Stowed_Weapon","WP_Sword_C")=="stowed_weapons","Stowed melee regressed");
    check(misc_category("Socket_ShellItem","BP_Charm_C")=="accessories","Named accessory regressed");
    for(const auto body_socket:{"None","root","head","hand_r","spine_03","Socket_Prop_R"})
        check(misc_category(body_socket,"BP_Unknown_C").empty(),"Generic body socket entered MISC");
    std::cout<<"MISC attachment classification checks passed\n";
}

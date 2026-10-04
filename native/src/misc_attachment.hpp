#pragma once
#include <cctype>
#include <string>
#include <string_view>

namespace css {
// Lowercase helper.
inline std::string misc_lower(std::string s) {
    for(char& c:s) c=char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Only these accessory actors may attach through a non-rendering scene root.
inline bool misc_known_scene_accessory(std::string_view owner_class) {
    return owner_class=="BP_Gragu_Helmet_C" || owner_class=="BP_Attachable_Item_Heart_C";
}
inline bool misc_attachment_component(std::string_view component_class, std::string_view owner_class) {
    return component_class.find("Mesh")!=std::string_view::npos ||
        (component_class=="SceneComponent" && misc_known_scene_accessory(owner_class));
}

// A socket that belongs to the skeleton itself or to a held weapon (hand/prop), as opposed to a
// named attachment socket a shell hangs gear on. Items on these are never treated as accessories;
// a held weapon on Socket_Prop_R is classified separately as a drawn weapon.
inline bool misc_body_socket(const std::string& sl) {
    if(sl.empty()||sl=="none"||sl=="root") return true;
    for(const char* k:{"hand","prop","foot","ball_","spine","pelvis","head","neck","clavicle",
                       "upperarm","lowerarm","arm_","thigh","calf","hips","finger","thumb","index",
                       "middle","ring","pinky","wrist","elbow","knee","ankle","toe","chest","breast",
                       "butt","cheek","eye","jaw","tongue","ear","hair","tail"})
        if(sl.find(k)!=std::string::npos) return true;
    return false;
}
// Category of an item resting on a socket, or "" for anything MISC must not touch. Generic across
// shells: seals and stowed weapons are matched by socket, and anything else on a named (non-body,
// non-hand) socket is a shell ornament or tool. Body/hand sockets and audio/VFX never classify.
inline std::string misc_category(const std::string& socket, const std::string& owner_class) {
    // Heart of Vatra uses the InfiniteSeal socket but is a separate quest accessory.
    if(misc_known_scene_accessory(owner_class)) return "accessories";
    const std::string sl=misc_lower(socket), ol=misc_lower(owner_class);
    auto in=[](const std::string& h,const char* n){ return h.find(n)!=std::string::npos; };
    // Seal first: its socket also contains "prop" and "stowed".
    if(in(sl,"seal")||in(ol,"seal")) return "seal";
    if(in(sl,"stowed")) {   // an item holstered on the body
        if(in(ol,"nailshotgun")||in(ol,"ballistazooka")||in(ol,"crossbow")||in(ol,"machinegun")
           ||in(ol,"parasite")||in(ol,"ballista")||in(ol,"shotgun")
           ||in(sl,"nailshotgun")||in(sl,"ballistazooka")||in(sl,"crossbow")||in(sl,"machinegun")||in(sl,"parasite"))
            return "sidearm";
        if(in(sl,"shellitem")||in(ol,"pouch")||in(ol,"relic")||in(ol,"charm")||in(ol,"totem")||in(ol,"idol"))
            return "accessories";
        return "stowed_weapons";   // remaining stowed items are holstered melee weapons
    }
    // Any other item on a named, non-body socket is a shell ornament or usable shell tool
    // (Eredrim's Diapason, Tiel's dagger charm, a flower crown, a cape, a pouch...).
    if(!misc_body_socket(sl)) return "accessories";
    return "";
}

}

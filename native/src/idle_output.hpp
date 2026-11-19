#pragma once
#include "skeleton_compatibility.hpp"
#include <stdexcept>

namespace css {
// The original Seductress uses the pre-namespace version of the 379-bone Eve rig.
inline constexpr std::string_view seductress_idle_skeleton =
    "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
inline bool compatible_idle_skeletons(std::string_view source,std::string_view target) {
    if(target==seductress_idle_skeleton) target=authored_base_skeleton;
    if(source==seductress_idle_skeleton) source=authored_base_skeleton;
    return compatible_animation_skeletons(source,target);
}

template<class Node,class Set>
void acquire_idle_link(Node current,Node original,Node selected,Set set) {
    if(!original || !selected || current!=original || selected==original)
        throw std::runtime_error("Player idle output is unavailable or already owned");
    set(selected);
}
template<class Node,class Set>
void restore_idle_link(Node current,Node selected,Node original,Set set) {
    if(current==selected) set(original);
}

enum class IdleOutput { game, carrier, slot };
inline IdleOutput world_idle_output(bool vista,bool settled,bool moving_override,bool carrier,bool feminine) {
    return !vista && settled && !moving_override && (carrier || feminine) ? IdleOutput::carrier : IdleOutput::game;
}
}

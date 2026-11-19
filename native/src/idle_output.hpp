#pragma once
#include "skeleton_compatibility.hpp"
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>

namespace css {
// The original Seductress uses the pre-namespace version of the 379-bone Eve rig.
inline constexpr std::string_view seductress_idle_skeleton =
    "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
inline bool compatible_idle_skeletons(std::string_view source,std::string_view target) {
    if(target==seductress_idle_skeleton) target=authored_base_skeleton;
    if(source==seductress_idle_skeleton) source=authored_base_skeleton;
    return compatible_animation_skeletons(source,target);
}

inline constexpr std::wstring_view player_idle_root=L"AnimGraphNode_Root";
inline constexpr std::wstring_view player_idle_source=L"AnimGraphNode_LinkedAnimGraph_1";
inline constexpr std::wstring_view player_idle_carrier=L"AnimGraphNode_BlendSpacePlayer";
inline constexpr std::wstring_view player_idle_slot=L"AnimGraphNode_Slot_5";

// UE4SS GetPropertyByNameInChain compares only FName's comparison index.
// Cooked graph nodes with different numeric suffixes share that index.
template<class Property,class Range,class FullName>
Property* idle_graph_property(Range&& properties,std::wstring_view name,FullName full_name) {
    for(auto* property:properties)
        if(full_name(property)==name) return property;
    return nullptr;
}

class IdleDirectFallback {
    std::string reason_;
    std::set<std::string> reported_;
    uint64_t retry_after_=0;
public:
    bool active() const { return !reason_.empty(); }
    void reset() { reason_.clear(); retry_after_=0; }
    template<class Bind> void attempt(uint64_t now,Bind bind) {
        if(now<retry_after_) return;
        try {
            bind();
            reset();
        } catch(const std::runtime_error& error) {
            reason_=error.what();
            retry_after_=now+1000;
        }
    }
    template<class Log> void report(const char* route,Log log) {
        if(active() && reported_.insert(reason_).second) {
            const std::string message=std::string("CSS idle continuing on ")+route+": "+reason_;
            log(message.c_str());
        }
    }
};

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

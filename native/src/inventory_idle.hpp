#pragma once
#include "skeleton_compatibility.hpp"
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace css {
struct InventoryIdleChoice {
    std::string outfit, variant, idle, expression;
    uint64_t catalog_generation;
    bool operator==(const InventoryIdleChoice&) const = default;
};

// The feminine blend space samples this game-human sequence at zero speed.
// Its human tracks also drive the audited CSS base and its extensions.
inline bool inventory_idle_skeleton(bool feminine,std::string_view source,std::string_view target) {
    constexpr std::string_view human="/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    if(feminine) return source==human && (target==human || compatible_animation_skeletons(authored_base_skeleton,target));
    return compatible_animation_skeletons(source,target);
}

inline void require_inventory_idle_rig(bool feminine,bool idle_layer,bool native_carrier,
                                       std::string_view source,std::string_view target) {
    if(!feminine && !idle_layer && !native_carrier)
        throw std::runtime_error("Inventory mesh has no compatible idle layer");
    if(!inventory_idle_skeleton(feminine,source,target))
        throw std::runtime_error("Idle skeleton is incompatible with the inventory mesh");
}

class InventoryIdlePlayback {
    std::optional<InventoryIdleChoice> choice_;
    uint64_t started_=0;
    double duration_=0;
public:
    bool requested() const { return choice_.has_value(); }
    bool playing() const { return duration_>0; }
    template<class Stop> void reset(Stop stop) {
        if(choice_) stop();
        choice_.reset();
        duration_=0;
    }
    // Keep a rejected choice until the target or selection changes. Transient
    // loads return zero and retry on the next menu maintenance pass.
    template<class Stop,class Start> void sync(const InventoryIdleChoice& choice,bool same_owner,
                                              uint64_t now,Stop stop,Start start) {
        if(same_owner && choice_==choice) return;
        reset(stop);
        if(choice.idle.empty() || choice.idle=="original") return;
        choice_=choice;
        try {
            const auto duration=start();
            if(duration==0) { reset(stop); return; }
            if(!std::isfinite(duration) || duration<0) throw std::runtime_error("Invalid preview idle length");
            started_=now; duration_=duration;
        } catch(...) { stop(); throw; }
    }
    double position(uint64_t now) const {
        return playing() && now>started_?std::fmod(double(now-started_)/1000.,duration_):0.;
    }
};
}

#pragma once
#include "astral_namespace.hpp"
#include <cmath>
#include <algorithm>
#include <optional>

namespace css::inventory_effects {
enum class Driver { none, peony_phase, profane_seconds };

inline Driver driver(std::string_view root) {
    for(auto path:{
        "/Game/CSS/eins0fx/EveSB/PeonyV2/Materials/M_PeonyV2_FlowFabric.M_PeonyV2_FlowFabric",
        "/Game/CSS/eins0fx/EveSB/PeonyV2/Materials/M_PeonyV2_FlowPanel.M_PeonyV2_FlowPanel",
        "/Game/CSS/eins0fx/EveSB/PeonyV2/Materials/M_PeonyV2_FlowStrap.M_PeonyV2_FlowStrap"})
        if(astral_material_root_matches(path,root)) return Driver::peony_phase;
    for(auto path:{
        "/Game/CSS/eins0fx/tHornyProxima/Materials/Final/M_NeckFlow.M_NeckFlow",
        "/Game/CSS/eins0fx/tHornyProxima/Materials/Preview/Relief01/M_HeadFlow.M_HeadFlow",
        "/Game/CSS/eins0fx/tHornyProxima/Preview/Liara04/Materials/M_LiaraAtlas.M_LiaraAtlas",
        "/Game/CSS/eins0fx/tHornyProxima/Materials/Heels94/M_ThornMetalHeels.M_ThornMetalHeels"})
        if(astral_material_root_matches(path,root)) return Driver::profane_seconds;
    return Driver::none;
}

struct PausedClock {
    double unpaused_start;
    double world_seconds;

    float sample(Driver kind,float base,float rate,double unpaused_now) const {
        const double elapsed=std::max(0.,unpaused_now-unpaused_start);
        if(kind==Driver::peony_phase)
            return static_cast<float>(std::remainder(base+elapsed*rate,1.));
        return static_cast<float>(base+(world_seconds+elapsed)*rate);
    }
};

// Both mesh flags are bits. Remember only a change we made and give it back on release.
struct PreviewFlag {
    bool owned=false;
    template<class Set>
    void sync(bool current,bool active,bool desired,Set set) {
        if(active && current!=desired) {
            set(desired);owned=true;
        } else if(!active && owned) {
            if(current==desired) set(!desired);
            owned=false;
        }
    }
};

struct ClothBlend {
    std::optional<float> original;
    float applied=0;

    template<class Set>
    void sync(float current,std::optional<float> desired,Set set) {
        if(original && current!=applied) original.reset();
        if(desired && current!=*desired) {
            if(!original) original=current;
            set(*desired);applied=*desired;
        } else if(!desired && original) {
            set(*original);original.reset();
        }
    }
};

template<class Material,class Set>
void restore_slot(Material current,Material copy,bool same_mesh,Material source_override,Material saved,Set set) {
    if(copy && current==copy) set(same_mesh ? source_override : saved);
}

// The native cloth tick ends immediately before the reflected teleport threshold in UE 5.6.
inline int cloth_tick_offset(int begin,int end,int component_size,int base_size,int tick_size,int flag_offset) {
    if(begin<0 || end>component_size || end<begin || end-begin>512 ||
       base_size!=0x28 || tick_size!=0x30 || flag_offset!=0xa ||
       end-begin<tick_size || (end-tick_size)%8!=0) return -1;
    return end-tick_size;
}
}

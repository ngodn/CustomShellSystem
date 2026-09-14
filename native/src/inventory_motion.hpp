#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace css {
// Keep motion integration independent of Unreal's paused world clock.
struct InventoryMotion {
    std::array<double,4> velocity{};
    void reset() { velocity={}; }
    static std::array<double,2> stick(std::array<double,2> value) {
        const auto length=std::hypot(value[0],value[1]);
        if(!std::isfinite(length) || length<=.16) return {};
        const auto strength=std::pow(std::clamp((length-.16)/.84,0.,1.),1.5);
        return {value[0]/length*strength,value[1]/length*strength};
    }
    // Result: yaw, zoom, horizontal framing, vertical framing.
    std::array<double,4> step(std::array<double,2> right,std::array<double,2> left,double dt,bool invert_x) {
        if(!std::isfinite(dt) || dt<=0 || dt>.1) { reset(); return {}; }
        right=stick(right); left=stick(left);
        std::array<double,4> target{right[0]*80*(invert_x?-1:1),right[1]*.65,left[0]*45,left[1]*45};
        std::array<double,4> movement{};
        for(size_t i=0;i<target.size();++i) {
            const double time=target[i]==0?.025:.055;
            const auto decay=std::exp(-dt/time);
            // Integrate the velocity curve exactly, so 30/60/144 FPS agree.
            movement[i]=target[i]*dt+(velocity[i]-target[i])*time*(1-decay);
            velocity[i]=target[i]+(velocity[i]-target[i])*decay;
            if(target[i]==0 && std::abs(velocity[i])<1e-7) velocity[i]=0;
        }
        return movement;
    }
};
}

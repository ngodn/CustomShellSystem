#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <string_view>

namespace css::hand_support {
// Bone order is the cooked rig ABI, not an outfit calibration.
inline constexpr std::array<std::string_view, 19> right_bones = {
    "pinky_metacarpal_r", "pinky_01_r", "pinky_02_r", "pinky_03_r",
    "ring_metacarpal_r", "ring_01_r", "ring_02_r", "ring_03_r",
    "middle_metacarpal_r", "middle_01_r", "middle_02_r", "middle_03_r",
    "index_metacarpal_r", "index_01_r", "index_02_r", "index_03_r",
    "thumb_01_r", "thumb_02_r", "thumb_03_r",
};
inline constexpr std::array<std::string_view, 4> arm_bones = {
    "upperarm_r", "lowerarm_r", "hand_r", "prop_r",
};
struct CarrySettings {
    std::array<double,2> wrist_height{}, prop_local_z{}, moving_phase{};
    double blend_in_seconds=0.,blend_out_seconds=0.,max_delta_seconds=0.;
};
inline double moving_phase(double prop_local_z,const CarrySettings& settings) {
    if(!std::isfinite(prop_local_z)) return 0.;
    const auto& range=settings.moving_phase;
    return std::clamp((prop_local_z-range[0])/(range[1]-range[0]),0.,1.);
}
struct CarryInput {
    std::string_view weapon;
    bool enabled=false,held=false,grounded=false,action=false;
    double wrist_height=0.,prop_local_z=0.;
    bool supported(const CarrySettings& settings) const {
        return enabled && !weapon.empty() && held && grounded && !action &&
            std::isfinite(wrist_height) && wrist_height>settings.wrist_height[0] && wrist_height<settings.wrist_height[1] &&
            std::isfinite(prop_local_z) && prop_local_z>settings.prop_local_z[0] && prop_local_z<settings.prop_local_z[1];
    }
};
class CarryBlend {
    std::string_view weapon_;
    double value_=0.;
public:
    void reset() {weapon_={};value_=0.;}
    double value() const {return value_;}
    double update(const CarryInput& input,double seconds,const CarrySettings& settings) {
        if(!input.enabled || input.weapon!=weapon_) {value_=0.;weapon_=input.weapon;return value_;}
        if(!std::isfinite(seconds) || seconds<0.) {value_=0.;return value_;}
        const bool carry=input.supported(settings);
        const double step=std::min(seconds,settings.max_delta_seconds)/(carry?settings.blend_in_seconds:settings.blend_out_seconds);
        value_=std::clamp(value_+(carry?step:-step),0.,1.);
        return value_;
    }
};
using Quat=std::array<double,4>;
using Targets=std::array<Quat,19>;
}

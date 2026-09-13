#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace css {
struct Stick { double x{}, y{}; };
inline Stick stick(int16_t x,int16_t y,double deadzone) {
    double length=std::hypot(static_cast<double>(x),static_cast<double>(y));
    if(length<=deadzone) return {};
    double strength=std::clamp((length-deadzone)/(32767.0-deadzone),0.0,1.0);
    return {x/length*strength,y/length*strength};
}
inline double frame_delta(float seconds) { return std::clamp(static_cast<double>(seconds),0.0,.05); }
}

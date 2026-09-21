#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace css {
using LightVector = std::array<double,3>;

// Orbit a captured light as a rigid transform. Position and all three axes
// rotate together, preserving its aim, roll and distance from the character.
struct InventoryLightOrbit {
    LightVector pivot{}, position{}, forward{}, right{}, up{}, screen_right{}, screen_up{};
    double yaw=0, pitch=0;
    static double dot(const LightVector& a,const LightVector& b) {
        return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    }
    static bool finite(const LightVector& a) {
        return std::all_of(a.begin(),a.end(),[](double x){return std::isfinite(x);});
    }
    static LightVector cross(const LightVector& a,const LightVector& b) {
        return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
    }
    void validate() const {
        for(const auto& v:{pivot,position,forward,right,up,screen_right,screen_up})
            if(!finite(v)) throw std::runtime_error("Invalid preview light transform");
        for(const auto& v:{forward,right,up,screen_right,screen_up})
            if(std::abs(dot(v,v)-1)>1e-5) throw std::runtime_error("Invalid preview light axis");
        if(std::abs(dot(forward,right))>1e-5 || std::abs(dot(forward,up))>1e-5 ||
           std::abs(dot(right,up))>1e-5 || std::abs(dot(screen_right,screen_up))>1e-5 ||
           dot(cross(forward,right),up)<.99999)
            throw std::runtime_error("Invalid preview light basis");
    }
    void move(double horizontal,double vertical) {
        if(!std::isfinite(horizontal) || !std::isfinite(vertical))
            throw std::runtime_error("Invalid preview light movement");
        // Reduce increments first so even very large finite inputs cannot overflow.
        yaw=std::remainder(yaw+std::remainder(horizontal,360.),360.);
        pitch=std::clamp(pitch+std::clamp(vertical,-160.,160.),-80.,80.);
    }
    static LightVector rotate(LightVector value,const LightVector& axis,double degrees) {
        const double radians=degrees*3.14159265358979323846/180.;
        const double c=std::cos(radians),s=std::sin(radians),projection=dot(axis,value);
        const auto perpendicular=cross(axis,value);
        for(int i=0;i<3;++i) value[i]=value[i]*c+perpendicular[i]*s+axis[i]*projection*(1-c);
        return value;
    }
    LightVector direction(LightVector v) const {
        return rotate(rotate(v,screen_right,pitch),screen_up,yaw);
    }
    LightVector location() const {
        auto offset=position;
        for(int i=0;i<3;++i) offset[i]-=pivot[i];
        offset=direction(offset);
        for(int i=0;i<3;++i) offset[i]+=pivot[i];
        return offset;
    }
};
}

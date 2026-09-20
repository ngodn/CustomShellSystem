#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace css {
using BackdropVector = std::array<double,3>;
// Intersect the four camera rays with a plane, preserving its artwork aspect.
// Axes are unit world vectors; half extents include the original world scale.
inline double backdrop_coverage(const BackdropVector& camera, const BackdropVector& forward,
    const BackdropVector& right, const BackdropVector& up, double horizontal_fov, double aspect,
    const BackdropVector& center, const BackdropVector& axis_x, const BackdropVector& axis_y,
    double half_x, double half_y) {
    auto dot=[](const auto& a,const auto& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; };
    const BackdropVector normal{axis_x[1]*axis_y[2]-axis_x[2]*axis_y[1],
        axis_x[2]*axis_y[0]-axis_x[0]*axis_y[2],axis_x[0]*axis_y[1]-axis_x[1]*axis_y[0]};
    BackdropVector offset{};
    for(int i=0;i<3;++i) offset[i]=center[i]-camera[i];
    if(!std::isfinite(horizontal_fov) || horizontal_fov<=0 || horizontal_fov>=179 ||
       !std::isfinite(aspect) || aspect<=0 || !std::isfinite(half_x) || half_x<=0 ||
       !std::isfinite(half_y) || half_y<=0)
        throw std::runtime_error("Invalid preview backdrop dimensions");
    const double tangent=std::tan(horizontal_fov*3.141592653589793/360.);
    double factor=1.;
    for(int x:{-1,1}) for(int y:{-1,1}) {
        BackdropVector ray{},point{};
        for(int i=0;i<3;++i) ray[i]=forward[i]+x*tangent*right[i]+y*tangent/aspect*up[i];
        const double denominator=dot(ray,normal);
        if(!std::isfinite(denominator) || std::abs(denominator)<1e-6)
            throw std::runtime_error("Preview backdrop is parallel to the view");
        const double distance=dot(offset,normal)/denominator;
        if(!std::isfinite(distance) || distance<=0)
            throw std::runtime_error("Preview backdrop is behind the view");
        for(int i=0;i<3;++i) point[i]=distance*ray[i]-offset[i];
        const double required=std::max(std::abs(dot(point,axis_x))/half_x,
                                       std::abs(dot(point,axis_y))/half_y);
        if(!std::isfinite(required)) throw std::runtime_error("Invalid preview backdrop projection");
        factor=std::max(factor,required*1.03);
    }
    return factor;
}
}

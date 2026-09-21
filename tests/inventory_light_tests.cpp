#include "inventory_light.hpp"
#include "inventory_motion.hpp"
#include <iostream>
#include <limits>

using css::InventoryLightOrbit;
using css::LightVector;
void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
bool near(LightVector a,LightVector b,double tolerance=1e-9) {
    for(int i=0;i<3;++i) if(std::abs(a[i]-b[i])>tolerance) return false;
    return true;
}
InventoryLightOrbit baseline() {
    InventoryLightOrbit o;
    o.pivot={1000,-500,100};o.position={1100,-500,100};
    o.forward={-1,0,0};o.right={0,-1,0};o.up={0,0,1};
    o.screen_right={0,1,0};o.screen_up={0,0,1};o.validate();return o;
}
int main() {
    auto o=baseline();
    require(near(o.location(),o.position),"Zero orbit must preserve original position");
    o.move(90,0);
    require(near(o.location(),{1000,-400,100}),"Quarter orbit must move around character pivot");
    require(near(o.direction(o.forward),{0,-1,0}),"Quarter orbit must preserve light aim");
    o.move(270,0);
    require(near(o.location(),o.position),"Full orbit must return to original position");
    o.move(0,30);
    require(near(o.location(),{1000+50*std::sqrt(3.),-500,50}),"Elevation has wrong screen-axis convention");
    for(double yaw:{-179.,-90.,0.,90.,179.}) for(double pitch:{-79.,0.,79.}) {
        o=baseline();o.move(yaw,pitch);
        auto offset=o.location();for(int i=0;i<3;++i) offset[i]-=o.pivot[i];
        const auto f=o.direction(o.forward),r=o.direction(o.right),u=o.direction(o.up);
        require(std::abs(InventoryLightOrbit::dot(offset,offset)-10000)<1e-8,"Orbit changed light distance");
        require(std::abs(InventoryLightOrbit::dot(offset,f)+100)<1e-9,"Orbit changed aim relative to character");
        require(near(InventoryLightOrbit::cross(f,r),u),"Orbit changed orientation handedness");
        require(std::abs(InventoryLightOrbit::dot(r,u))<1e-9,"Orbit skewed light axes");
    }
    LightVector fps_reference{};
    for(int fps:{30,60,144}) {
        o=baseline();css::InventoryMotion motion;
        for(int i=0;i<fps*2;++i) {
            auto d=motion.step({.5,.2},{0,0},1./fps,false);o.move(d[0],d[1]*80/.65);
        }
        if(fps==30) fps_reference=o.location();
        else require(near(o.location(),fps_reference),"Light drift depends on frame rate");
    }
    o=baseline();o.move(0,1000);require(o.pitch==80,"Elevation must be bounded");
    o.move(0,-1000);require(o.pitch==-80,"Negative elevation must be bounded");
    o.move(std::numeric_limits<double>::max(),0);
    require(InventoryLightOrbit::finite(o.location()),"Large finite orbit overflowed");
    const auto before=o.location();bool rejected=false;
    try{o.move(1,std::numeric_limits<double>::quiet_NaN());}catch(const std::runtime_error&){rejected=true;}
    require(rejected && near(o.location(),before),"Invalid movement changed orbit");
    o=baseline();o.up={0,0,-1};rejected=false;
    try{o.validate();}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Reflected basis must reject mirrored axes");
    std::cout<<"Preview light: pivot, aim, rigid orbit, bounds, frame rates and invalid input passed\n";
}

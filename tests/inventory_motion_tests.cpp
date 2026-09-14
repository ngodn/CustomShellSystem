#include "inventory_motion.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using css::InventoryMotion;
int main() {
    assert((InventoryMotion::stick({.1,.1})==std::array<double,2>{}));
    const auto diagonal=InventoryMotion::stick({1,1});
    assert(std::abs(std::hypot(diagonal[0],diagonal[1])-1)<1e-12);
    double baseline=0;
    for(int fps:{30,60,144}) {
        InventoryMotion motion; double yaw=179;
        for(int i=0;i<fps*4;++i) {
            auto d=motion.step({1,0},{0,0},1./fps,false);
            assert(d[0]>=0 && d[0]<=80./fps+1e-9); yaw+=d[0];
        }
        assert(yaw>490); // Cross 180 and 360 without reversing.
        if(!baseline) baseline=yaw; else assert(std::abs(yaw-baseline)<1e-9);
        const auto before=yaw;
        for(int i=0;i<fps;++i) yaw+=motion.step({0,0},{0,0},1./fps,false)[0];
        assert(yaw-before<2.01 && yaw-before>1.99);
        assert(motion.step({0,0},{0,0},1./fps,false)[0]==0);
    }
    // Each physical stick axis affects only the requested operation.
    for(int axis=0;axis<4;++axis) {
        InventoryMotion motion;
        std::array<double,2> right{},left{};
        (axis<2?right:left)[axis%2]=1;
        auto d=motion.step(right,left,.016,false);
        for(int output=0;output<4;++output) assert(output==axis?d[output]>0:d[output]==0);
    }
    InventoryMotion normal,inverted;
    auto a=normal.step({.4,.6},{0,0},.016,false);
    auto b=inverted.step({.4,.6},{0,0},.016,true);
    assert(a[0]==-b[0] && a[1]==b[1]);
    normal.reset(); assert((normal.step({0,0},{0,0},.016,false)==std::array<double,4>{}));
    assert((normal.step({1,1},{1,1},1.,false)==std::array<double,4>{}));
    assert((InventoryMotion::stick({std::numeric_limits<double>::quiet_NaN(),0})==std::array<double,2>{}));
    std::cout<<"Inventory motion: dead zone, direction, frame rates, release and reset passed\n";
}

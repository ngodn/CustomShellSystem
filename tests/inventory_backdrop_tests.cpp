#include "inventory_backdrop.hpp"
#include <iostream>
#include <limits>
using namespace css;
static void require(bool value) { if(!value) throw std::runtime_error("Backdrop check failed"); }
int main() {
    const BackdropVector camera{0,0,0},forward{1,0,0},right{0,1,0},up{0,0,1};
    int checks=0;
    // Actual captured planes fail at max zoom before fitting: height is 55-56%.
    for(auto plane: {std::array<double,3>{98,67.89,19.09},{700,475.2,133.65}}) {
        const double depth=plane[0],hx=plane[1],hy=plane[2];
        const double original_height=depth*std::tan(63.361026763916016*3.141592653589793/360.)/(16./9);
        require(hy<original_height*.57);
        for(double aspect:{4./3,16./9,21./9,32./9}) for(double lens:{.55,1.,2.})
        for(double pan:{-90.,0.,90.}) for(double frame:{-70.,0.,70.}) {
            const double fov=2*std::atan(std::tan(37.497356*3.141592653589793/360.)/lens)*180/3.141592653589793;
            BackdropVector moved_camera{0,-pan,-frame},center{depth,-pan,-frame};
            const double factor=backdrop_coverage(moved_camera,forward,right,up,fov,aspect,center,right,up,hx,hy);
            const double width=depth*std::tan(fov*3.141592653589793/360.);
            require(hx*factor>=width*1.03-1e-8 && hy*factor>=width/aspect*1.03-1e-8);
            require(factor>=1 && std::isfinite(factor)); ++checks;
        }
        // Off-center and rotated planes use ray intersections, not an axis guess.
        auto f=backdrop_coverage(camera,forward,right,up,63.361,16./9,{depth,3,-2},right,up,hx,hy);
        require(f*hy>=original_height+1.99); ++checks;
    }
    for(double aspect:{0.,-1.,std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected=false;
        try { backdrop_coverage(camera,forward,right,up,60,aspect,{98,0,0},right,up,68,19); }
        catch(const std::runtime_error&) { rejected=true; }
        require(rejected); ++checks;
    }
    bool rejected=false;
    try { backdrop_coverage(camera,forward,right,up,60,1.8,{-98,0,0},right,up,68,19); }
    catch(const std::runtime_error&) { rejected=true; }
    require(rejected);
    std::cout<<checks+1<<" backdrop coverage checks passed\n";
}

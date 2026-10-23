#include "native_foot_ik.hpp"
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void expect(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
}

int main() try {
    using namespace css::native_foot_ik;
    // Rig lengths as read back from CTRL_Human_FullbodyIK (cm): thigh 45.93, calf 45.50, foot 12.
    const Lengths rig={45.93,45.50,12.,45.93,45.50,12.};
    const Lengths proxima={46.29,42.85,12.1,46.29,42.85,12.1};   // calf 2.65 cm shorter
    const Lengths close={45.95,45.47,12.02,45.90,45.52,11.98};   // within 0.5 cm everywhere
    const std::string_view css_skeleton="/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base.SKEL_Base";

    expect(needs_mesh_lengths(css_skeleton,rig,proxima),"A CSS body with shorter calves must take its own lengths");
    expect(std::abs(*largest_difference(rig,proxima)-2.65)<1e-9,"Largest difference must be the calf");
    expect(!needs_mesh_lengths(css_skeleton,rig,close),"A CSS body within half a centimetre must keep the rig as authored");
    expect(!needs_mesh_lengths(game_skeleton,rig,proxima),"A body on the game skeleton must never be touched");
    expect(!needs_mesh_lengths("",rig,proxima),"An unknown skeleton must never be touched");

    Lengths missing{}; missing[1]=42.85;
    Lengths rig_missing{}; rig_missing[0]=45.93;
    expect(!largest_difference(rig_missing,missing) && !needs_mesh_lengths(css_skeleton,rig_missing,missing),
           "Without a comparable segment the rig must be left alone");
    Lengths one_side=rig; one_side[4]=43.;
    expect(needs_mesh_lengths(css_skeleton,rig,one_side),"One mismatched segment is enough");
    Lengths nan_mesh=proxima; for(auto& v:nan_mesh) v=std::nan("");
    expect(!needs_mesh_lengths(css_skeleton,rig,nan_mesh),"Non-finite lengths must never trigger the correction");
    expect(!needs_mesh_lengths(css_skeleton,rig,Lengths{45.93+.5,45.50,12.,45.93,45.50,12.}),"Exactly the threshold stays as authored");

    std::cout<<checks<<" native foot IK checks passed\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"FAILED: "<<error.what()<<"\n";
    return 1;
}

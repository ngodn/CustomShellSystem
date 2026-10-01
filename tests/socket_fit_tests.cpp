#include "socket_fit.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using css::SocketRebase;
using css::SocketTransform;
static void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
static bool close(const std::array<double,3>& a,const std::array<double,3>& b,double tolerance=1e-3) {
    for(int i=0;i<3;++i) if(std::abs(a[i]-b[i])>tolerance) return false;
    return true;
}

int main() {
    require(css::stowed_fit_socket("Socket_CrossBow_Stowed") &&
            css::stowed_fit_socket("Socket_Prop_Stowed_InfiniteSeal_Right"),"Stowed gear must be eligible for fitting");
    require(!css::stowed_fit_socket("Socket_Prop_L_02") && !css::stowed_fit_socket("Socket_Prop_Tiel_Dagger") &&
            !css::stowed_fit_socket("prop_r"),"Fit tables must not move held or unclassified sockets");
    // The game's own readings (live probe, Genessa shell): it stows the Tarnished Seal with
    // CD_Genessa's adjustment, translation (5,-5,-3) and quaternion Y 0.0610485, and the
    // component shows that as relative pitch -7. The lute's X 0.0348995 shows as roll -4.
    const SocketTransform genessa_seal{{0,0.0610485395,0,0.9981347984},{5,-5,-3}};
    const SocketTransform thorn_seal{{0,0,0,1},{0,0,-3}};
    require(close(css::socket_rotator(css::socket_matrix(genessa_seal.rotation)),{-7,0,0}),"Quaternion pitch must match the game's rotator");
    require(close(css::socket_rotator(css::socket_matrix(std::array<double,4>{0.0348994967,0,0,0.9993908270})),{0,0,-4}),"Quaternion roll must match the game's rotator");

    // Rotator round trip, including a combined rotation.
    for(const std::array<double,3> r:{std::array<double,3>{10,20,30},{-35,170,-80},{0,0,0},{89,-45,12}})
        require(close(css::socket_rotator(css::socket_matrix(r)),r),"Rotator must survive the matrix round trip");

    // Sariel wearing Genessa's look: the seal the game hung at Sariel's adjustment moves to
    // exactly Genessa's.
    std::array<double,3> location=thorn_seal.translation,rotation{0,0,0};
    css::socket_rebase({thorn_seal,genessa_seal},location,rotation);
    require(close(location,{5,-5,-3}) && close(rotation,{-7,0,0}),"A rebased stow must land on the look's adjustment");

    // And back: Genessa wearing Sariel's look.
    css::socket_rebase({genessa_seal,thorn_seal},location,rotation);
    require(close(location,{0,0,-3}) && close(rotation,{0,0,0}),"Rebasing back must restore the worn shell's adjustment");

    // A base the game moved (stow interpolation) keeps its difference from `from`, expressed
    // in the new adjustment's frame. With translation-only adjustments that is a plain shift.
    const SocketTransform gragu_crossbow{{0,0,0,1},{0,-5,0}},genessa_crossbow{{0,0,0,1},{0,5,0}};
    location={1,-3,2}; rotation={0,15,0};
    css::socket_rebase({gragu_crossbow,genessa_crossbow},location,rotation);
    require(close(location,{1,7,2}) && close(rotation,{0,15,0}),"A moved base must carry its own offset across");

    // Identical tables are a no-op.
    location={3,4,5}; rotation={1,2,3};
    css::socket_rebase({genessa_seal,genessa_seal},location,rotation);
    require(close(location,{3,4,5}) && close(rotation,{1,2,3}),"Same shell must leave the stow alone");
    require(SocketTransform{}.identity() && !genessa_seal.identity(),"Identity check");

    // A game re-stow must discard CSS's old push, even after an earlier successful fit.
    css::StowedPose pose;
    pose.observe(gragu_crossbow.translation,{0,0,0});
    require(!pose.owned,"First acquisition must not carry a previous push");
    location=pose.location; rotation=pose.rotation;
    css::socket_rebase({gragu_crossbow,genessa_crossbow},location,rotation);
    pose.record(location,rotation);
    pose.observe(location,rotation);
    require(pose.owned && close(pose.location,gragu_crossbow.translation),"An unchanged correction must retain its game base");
    pose.observe(gragu_crossbow.translation,{0,0,0});
    require(!pose.owned,"A game re-stow must not reuse the previous collision push");
    location=pose.location; rotation=pose.rotation;
    css::socket_rebase({gragu_crossbow,genessa_crossbow},location,rotation);
    require(close(location,genessa_crossbow.translation),"Repeated stow must still fit the selected look");

    pose.record(location,rotation);
    require(pose.owns(location,rotation),"Default can restore an unchanged CSS correction");
    require(!pose.owns(location,{0,20,0}),"Default must preserve a new game rotation");
    pose.observe(location,{0,20,0});
    require(!pose.owned && close(pose.rotation,{0,20,0}),"A rotation-only re-stow must refresh the base");
    require(close(pose.location,gragu_crossbow.translation),"Rotation-only update must not bake in CSS's location correction");
    location=pose.location; rotation=pose.rotation;
    css::socket_rebase({gragu_crossbow,genessa_crossbow},location,rotation);
    require(close(location,genessa_crossbow.translation) && close(rotation,{0,20,0}),"Rotation-only update must not double the fit");
    pose.record(location,{0,180,0});
    require(pose.owns(location,{0,-180,0}),"Equivalent wrapped angles must not accumulate corrections");
    pose.owned=false;   // The game borrowed the item for a hand socket.
    require(!pose.owns(location,{0,180,0}),"Borrowed gear must not be restored by CSS");
    pose.observe({0,0,-3},{0,0,0});
    pose.record({5,-5,-3},{-7,0,0});
    pose.observe({6,-5,-3},{-7,0,0});
    require(!pose.owned && close(pose.rotation,{0,0,0}),"Location-only update must not bake in CSS's rotation correction");
    std::cout<<"socket fit tests passed\n";
}

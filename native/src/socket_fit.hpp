#pragma once
// Moves a stowed item from one shell's socket adjustment to another's.
//
// Every shell's character data carries a SocketAdjustmentData table (socket name to
// transform), and the game stows each weapon with that transform as its relative transform
// at the socket. The table is tuned to the shell's own body: Genessa pulls her back weapons
// in by 4 to 5 cm, Gragu and Lazlo push theirs out by 5 to 25 cm. Wearing one shell's body on
// another keeps the worn shell's table, so a slim look on a large shell carries its gear off
// the body and a large look on a slim shell sinks it in. Rebasing swaps the worn shell's
// adjustment for the look's: target = base * inverse(from) * to, which is exactly `to`
// whenever the game left the item at `from`.
//
// Unreal's conventions, kept here so this stays free of engine headers and testable on the
// host: a rotation matrix's rows are its X, Y and Z axes, vectors are rows (v * M), and
// A * B means "A, then B" (rotation M_A * M_B, translation t_A * M_B + t_B).
#include <array>
#include <cmath>
#include <string_view>

namespace css {

inline bool stowed_fit_socket(std::string_view name) {
    return name.find("_Stowed")!=std::string_view::npos;
}

struct SocketTransform {
    std::array<double,4> rotation{0,0,0,1};   // FQuat X, Y, Z, W
    std::array<double,3> translation{};        // cm
    bool operator==(const SocketTransform&) const = default;
    bool identity(double tolerance=1e-6) const {
        for(double v:translation) if(std::abs(v)>tolerance) return false;
        return std::abs(std::abs(rotation[3])-1)<=tolerance;
    }
};
struct SocketRebase {
    SocketTransform from, to;   // the worn shell's adjustment, the look's
    bool operator==(const SocketRebase&) const = default;
};

struct StowedPose {
    std::array<double,3> location{}, rotation{}, applied{}, applied_rotation{};
    bool owned=false;

    static bool matches(const std::array<double,3>& current,const std::array<double,3>& previous,bool angles=false) {
        for(int i=0;i<3;++i) {
            const double delta=current[i]-previous[i];
            if(!std::isfinite(delta) || std::abs(angles?std::remainder(delta,360.):delta)>.01) return false;
        }
        return true;
    }
    bool owns(const std::array<double,3>& current,const std::array<double,3>& angles) const {
        return owned && matches(current,applied) && matches(angles,applied_rotation,true);
    }
    void observe(const std::array<double,3>& current,const std::array<double,3>& angles) {
        const bool same_location=owned && matches(current,applied);
        const bool same_rotation=owned && matches(angles,applied_rotation,true);
        if(same_location && same_rotation) return;
        // A rotation-only write must not bake CSS's location correction into the game base.
        if(!same_location) location=current;
        if(!same_rotation) rotation=angles;
        owned=false;
    }
    void record(const std::array<double,3>& current,const std::array<double,3>& angles) {
        applied=current; applied_rotation=angles; owned=true;
    }
};

using SocketMatrix = std::array<std::array<double,3>,3>;

// FQuatRotationTranslationMatrix.
inline SocketMatrix socket_matrix(const std::array<double,4>& q) {
    const double x=q[0],y=q[1],z=q[2],w=q[3];
    const double x2=x+x,y2=y+y,z2=z+z;
    const double xx=x*x2,xy=x*y2,xz=x*z2,yy=y*y2,yz=y*z2,zz=z*z2,wx=w*x2,wy=w*y2,wz=w*z2;
    return {{{1-(yy+zz),xy+wz,xz-wy},{xy-wz,1-(xx+zz),yz+wx},{xz+wy,yz-wx,1-(xx+yy)}}};
}
// FRotationMatrix, from an FRotator in degrees (pitch, yaw, roll).
inline SocketMatrix socket_matrix(const std::array<double,3>& rotator) {
    constexpr double radians=3.14159265358979323846/180.;
    const double p=rotator[0]*radians,y=rotator[1]*radians,r=rotator[2]*radians;
    const double sp=std::sin(p),sy=std::sin(y),sr=std::sin(r),cp=std::cos(p),cy=std::cos(y),cr=std::cos(r);
    return {{{cp*cy,cp*sy,sp},{sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp},{-(cr*sp*cy+sr*sy),cy*sr-cr*sp*sy,cr*cp}}};
}
// FMatrix::Rotator.
inline std::array<double,3> socket_rotator(const SocketMatrix& m) {
    constexpr double degrees=180./3.14159265358979323846;
    const auto& x=m[0];
    std::array<double,3> rotator{std::atan2(x[2],std::sqrt(x[0]*x[0]+x[1]*x[1]))*degrees,std::atan2(x[1],x[0])*degrees,0};
    const auto y_axis=socket_matrix(rotator)[1];
    auto dot=[](const std::array<double,3>& a,const std::array<double,3>& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    rotator[2]=std::atan2(dot(m[2],y_axis),dot(m[1],y_axis))*degrees;
    return rotator;
}
inline SocketMatrix socket_multiply(const SocketMatrix& a,const SocketMatrix& b) {
    SocketMatrix r{};
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k) r[i][j]+=a[i][k]*b[k][j];
    return r;
}
inline SocketMatrix socket_transpose(const SocketMatrix& m) {
    SocketMatrix r{};
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) r[i][j]=m[j][i];
    return r;
}
inline std::array<double,3> socket_apply(const std::array<double,3>& v,const SocketMatrix& m) {
    return {v[0]*m[0][0]+v[1]*m[1][0]+v[2]*m[2][0],v[0]*m[0][1]+v[1]*m[1][1]+v[2]*m[2][1],v[0]*m[0][2]+v[1]*m[1][2]+v[2]*m[2][2]};
}
// base * inverse(from) * to, on a relative location and rotator.
inline void socket_rebase(const SocketRebase& rebase,std::array<double,3>& location,std::array<double,3>& rotation) {
    const auto from=socket_matrix(rebase.from.rotation),to=socket_matrix(rebase.to.rotation);
    const auto from_inverse=socket_transpose(from);
    std::array<double,3> local{};
    for(int i=0;i<3;++i) local[i]=location[i]-rebase.from.translation[i];
    local=socket_apply(socket_apply(local,from_inverse),to);
    for(int i=0;i<3;++i) location[i]=local[i]+rebase.to.translation[i];
    rotation=socket_rotator(socket_multiply(socket_multiply(socket_matrix(rotation),from_inverse),to));
}

}   // namespace css

#include "body_geometry.hpp"
#include "controls.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace css {
namespace {
using Json=nlohmann::json;
void require(bool valid,const char* message) {
    if(!valid) throw std::runtime_error(message);
}
void array(const Json& value,size_t size) {
    require(value.is_array() && value.size()==size,"Body geometry array length mismatch");
}
double number(const Json& value,double low,double high) {
    require(value.is_number(),"Body geometry requires numbers");
    const double result=value.get<double>();
    require(std::isfinite(result) && result>=low && result<=high,"Body geometry number outside bounds");
    return result;
}
BodyVector vector(const Json& value) {
    array(value,3);
    return {number(value[0],-10000,10000),number(value[1],-10000,10000),number(value[2],-10000,10000)};
}
bool identifier(const std::string& value) {
    return !value.empty() && value.size()<=128 && std::all_of(value.begin(),value.end(),[](unsigned char c){
        return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_';
    });
}
double determinant(const std::array<BodyVector,3>& a) {
    return a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-
           a[1][0]*(a[0][1]*a[2][2]-a[0][2]*a[2][1])+
           a[2][0]*(a[0][1]*a[1][2]-a[0][2]*a[1][1]);
}
}

BodyGeometryModel BodyGeometryModel::parse(const Json& data) {
    require(data.is_object() && data.size()==3 && data.at("schema").is_number_integer() && data.at("schema")==1,
            "Unsupported body geometry schema");
    array(data.at("morphs"),6);array(data.at("helpers"),7);
    BodyGeometryModel model;
    std::set<std::string> seen;
    for(size_t i=0;i<6;++i) {
        model.morphs[i]=data.at("morphs")[i].get<std::string>();
        require(identifier(model.morphs[i]) && seen.insert(model.morphs[i]).second,"Invalid body geometry morph name");
    }
    for(size_t i=0;i<7;++i) {
        const auto& source=data.at("helpers")[i];auto& target=model.helpers[i];
        require(source.is_object() && source.size()==10 && source.at("bone")==body_region_names[i],
                "Body geometry helper identity mismatch");
        target.contact_bone=source.at("contact_bone").get<std::string>();
        require(identifier(target.contact_bone),"Invalid body contact bone");
        target.offset=vector(source.at("offset_cm"));
        target.moment=number(source.at("moment_cm2"),1e-6,1e8);
        array(source.at("offset_deltas_cm"),6);array(source.at("moment_linear_cm2"),6);
        array(source.at("moment_quadratic_cm2"),6);
        for(size_t m=0;m<6;++m) {
            target.offset_deltas[m]=vector(source.at("offset_deltas_cm")[m]);
            target.moment_linear[m]=number(source.at("moment_linear_cm2")[m],-1e8,1e8);
            array(source.at("moment_quadratic_cm2")[m],6);
            for(size_t n=0;n<6;++n)
                target.moment_quadratic[m][n]=number(source.at("moment_quadratic_cm2")[m][n],-1e8,1e8);
        }
        for(size_t m=0;m<6;++m) for(size_t n=0;n<6;++n)
            require(std::abs(target.moment_quadratic[m][n]-target.moment_quadratic[n][m])<=1e-8,
                    "Body inertia matrix must be symmetric");
        array(source.at("contact_centers_cm"),64);array(source.at("contact_scales"),64);
        for(size_t corner=0;corner<64;++corner) {
            target.contact_centers[corner]=vector(source.at("contact_centers_cm")[corner]);
            target.contact_scales[corner]=number(source.at("contact_scales")[corner],1e-6,1000);
        }
        array(source.at("contact_axes_cm"),3);
        for(size_t axis=0;axis<3;++axis) target.contact_axes[axis]=vector(source.at("contact_axes_cm")[axis]);
        require(std::abs(determinant(target.contact_axes))>1e-8,"Singular body contact geometry");
    }
    // Catch invalid corner inertia at load time. Interior values are checked before use.
    for(unsigned mask=0;mask<64;++mask) {
        std::array<float,6> values{};
        for(unsigned i=0;i<6;++i) values[i]=float((mask>>i)&1);
        model.evaluate(values);
    }
    return model;
}

BodyGeometry BodyGeometryModel::evaluate(const std::array<float,6>& morphs) const {
    for(float value:morphs) require(std::isfinite(value) && value>=0 && value<=1,"Body morph outside [0,1]");
    std::array<double,64> weights{};
    for(unsigned mask=0;mask<64;++mask) {
        weights[mask]=1;
        for(unsigned m=0;m<6;++m) weights[mask]*=(mask&(1u<<m))?double(morphs[m]):1-double(morphs[m]);
    }
    BodyGeometry result;
    for(size_t i=0;i<7;++i) {
        const auto& source=helpers[i];
        auto& offset=result.offsets[i];offset=source.offset;
        double moment=source.moment,scale=0;
        for(size_t m=0;m<6;++m) {
            for(size_t axis=0;axis<3;++axis) offset[axis]+=double(morphs[m])*source.offset_deltas[m][axis];
            moment+=double(morphs[m])*source.moment_linear[m];
            for(size_t n=0;n<6;++n) moment+=double(morphs[m])*double(morphs[n])*source.moment_quadratic[m][n];
        }
        require(std::isfinite(moment) && moment>1e-6 && moment<=1e8,"Invalid evaluated body inertia");
        result.moments[i]=float(moment);
        for(size_t corner=0;corner<64;++corner) {
            scale+=weights[corner]*source.contact_scales[corner];
            for(size_t axis=0;axis<3;++axis)
                result.contact_centers[i][axis]+=weights[corner]*source.contact_centers[corner][axis];
        }
        for(size_t axis=0;axis<3;++axis) for(size_t component=0;component<3;++component)
            result.contact_axes[i][axis][component]=scale*source.contact_axes[axis][component];
        auto bounded=[](const BodyVector& v) {
            return std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x) && std::abs(x)<=1e7;});
        };
        require(bounded(offset) && bounded(result.contact_centers[i]) &&
                std::all_of(result.contact_axes[i].begin(),result.contact_axes[i].end(),bounded),
                "Invalid evaluated body geometry");
    }
    return result;
}
}

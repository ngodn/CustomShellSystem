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
    require(data.is_object() && data.contains("schema") && data.at("schema").is_number_integer(),"Unsupported body geometry schema");
    BodyGeometryModel model;
    model.schema=data.at("schema").get<int>();
    size_t axes=0;
    if(model.schema==1) {
        // The six-axis model cooked before the axis list was recorded: exactly as before.
        require(data.size()==3,"Unsupported body geometry schema");
        array(data.at("morphs"),6);
        axes=6;
    } else {
        require(model.schema==2 && data.size()==4,"Unsupported body geometry schema");
        require(data.at("morphs").is_array(),"Body geometry requires a morph list");
        axes=data.at("morphs").size();
        require(axes>=body_geometry_min_axes && axes<=body_geometry_max_axes,"Body geometry axis count outside 6..8");
        require(data.at("corners").is_number_unsigned() && data.at("corners").get<size_t>()==(size_t{1}<<axes),
                "Body geometry corner count must be 2^axes");
    }
    const size_t corners=size_t{1}<<axes;
    array(data.at("helpers"),7);
    std::set<std::string> seen;
    for(size_t i=0;i<axes;++i) {
        require(data.at("morphs")[i].is_string(),"Invalid body geometry morph name");
        model.morphs.push_back(data.at("morphs")[i].get<std::string>());
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
        array(source.at("offset_deltas_cm"),axes);array(source.at("moment_linear_cm2"),axes);
        array(source.at("moment_quadratic_cm2"),axes);
        target.offset_deltas.resize(axes);target.moment_linear.resize(axes);
        target.moment_quadratic.assign(axes,std::vector<double>(axes));
        for(size_t m=0;m<axes;++m) {
            target.offset_deltas[m]=vector(source.at("offset_deltas_cm")[m]);
            target.moment_linear[m]=number(source.at("moment_linear_cm2")[m],-1e8,1e8);
            array(source.at("moment_quadratic_cm2")[m],axes);
            for(size_t n=0;n<axes;++n)
                target.moment_quadratic[m][n]=number(source.at("moment_quadratic_cm2")[m][n],-1e8,1e8);
        }
        for(size_t m=0;m<axes;++m) for(size_t n=0;n<axes;++n)
            require(std::abs(target.moment_quadratic[m][n]-target.moment_quadratic[n][m])<=1e-8,
                    "Body inertia matrix must be symmetric");
        array(source.at("contact_centers_cm"),corners);array(source.at("contact_scales"),corners);
        target.contact_centers.resize(corners);target.contact_scales.resize(corners);
        for(size_t corner=0;corner<corners;++corner) {
            target.contact_centers[corner]=vector(source.at("contact_centers_cm")[corner]);
            target.contact_scales[corner]=number(source.at("contact_scales")[corner],1e-6,1000);
        }
        array(source.at("contact_axes_cm"),3);
        for(size_t axis=0;axis<3;++axis) target.contact_axes[axis]=vector(source.at("contact_axes_cm")[axis]);
        require(std::abs(determinant(target.contact_axes))>1e-8,"Singular body contact geometry");
    }
    // Catch invalid corner inertia at load time. Interior values are checked before use.
    for(size_t mask=0;mask<corners;++mask) {
        std::vector<float> values(axes);
        for(size_t i=0;i<axes;++i) values[i]=float((mask>>i)&1);
        model.evaluate(values);
    }
    return model;
}

BodyGeometry BodyGeometryModel::evaluate(const std::vector<float>& morphs) const {
    const size_t count=axes(),total=corners();
    require(morphs.size()==count,"Body morph count differs from the model");
    // Outfits may push a body slider past its authored morph (weight above 1) to reach a larger
    // shape. The model is linear/quadratic in the weights, so it extends past 1; the inertia and
    // bounds checks below still reject a result that stops being physical.
    for(float value:morphs) require(std::isfinite(value) && value>=0 && value<=max_body_morph_weight,"Body morph outside [0,3]");
    std::vector<double> weights(total);
    for(size_t mask=0;mask<total;++mask) {
        weights[mask]=1;
        for(size_t m=0;m<count;++m) weights[mask]*=(mask&(size_t{1}<<m))?double(morphs[m]):1-double(morphs[m]);
    }
    BodyGeometry result;
    result.shape=morphs;
    for(size_t i=0;i<7;++i) {
        const auto& source=helpers[i];
        auto& offset=result.offsets[i];offset=source.offset;
        double moment=source.moment,scale=0;
        for(size_t m=0;m<count;++m) {
            for(size_t axis=0;axis<3;++axis) offset[axis]+=double(morphs[m])*source.offset_deltas[m][axis];
            moment+=double(morphs[m])*source.moment_linear[m];
            for(size_t n=0;n<count;++n) moment+=double(morphs[m])*double(morphs[n])*source.moment_quadratic[m][n];
        }
        require(std::isfinite(moment) && moment>1e-6 && moment<=1e8,"Invalid evaluated body inertia");
        result.moments[i]=float(moment);
        for(size_t corner=0;corner<total;++corner) {
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

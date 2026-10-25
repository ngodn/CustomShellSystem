#include "body_geometry.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace css;
using Json=nlohmann::json;
unsigned checks;
void expect(bool valid,const char* message) { ++checks; if(!valid) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
    bool rejected=false;try { action(); } catch(const std::exception&) { rejected=true; }
    expect(rejected,"Invalid body geometry accepted");
}

// A synthetic model with `axes` morphs: distinct, exactly representable numbers so the
// independent reference below can compare to the reader without tolerance games.
Json synthetic(size_t axes,int schema) {
    const char* bones[]={"brust001","brust002","butt001","butt002","thigh_twist_02_l","thigh_twist_02_r","belly"};
    const size_t corners=size_t{1}<<axes;
    Json morphs=Json::array();
    for(size_t m=0;m<axes;++m) morphs.push_back("Axis"+std::to_string(m));
    Json helpers=Json::array();
    for(size_t i=0;i<7;++i) {
        Json deltas=Json::array(),linear=Json::array(),quadratic=Json::array(),centers=Json::array(),scales=Json::array();
        for(size_t m=0;m<axes;++m) {
            deltas.push_back({.25*double(m+1),-.5*double(i),.125});
            linear.push_back(2.0*double(m)-3);
            Json row=Json::array();
            for(size_t n=0;n<axes;++n) row.push_back(.5*double(m+n)+double(i));
            quadratic.push_back(row);
        }
        for(size_t c=0;c<corners;++c) {
            centers.push_back({double(c%7)-3,double(c%5)*.5,double(i)+double(c)/double(corners)});
            scales.push_back(1+double(c%9)*.125);
        }
        helpers.push_back({{"bone",bones[i]},{"offset_cm",{1.0*double(i),2,3}},{"offset_deltas_cm",deltas},
            {"moment_cm2",400+double(i)},{"moment_linear_cm2",linear},{"moment_quadratic_cm2",quadratic},
            {"contact_bone","spine_04"},{"contact_centers_cm",centers},{"contact_scales",scales},
            {"contact_axes_cm",{{2,0,0},{0,3,0},{0,.5,4}}}});
    }
    Json model={{"schema",schema},{"morphs",morphs},{"helpers",helpers}};
    if(schema==2) model["corners"]=corners;
    return model;
}

// Independent multilinear reference over the JSON, not over the parsed model.
void reference_check(const Json& source,const BodyGeometryModel& model,const std::vector<float>& values) {
    const auto actual=model.evaluate(values);
    expect(actual.shape==values,"Shape inputs lost morph coefficients");
    const size_t axes=values.size(),corners=size_t{1}<<axes;
    for(size_t i=0;i<7;++i) {
        const auto& h=source["helpers"][i];
        double scale=0;BodyVector center{};
        for(size_t c=0;c<corners;++c) {
            double w=1;
            for(size_t m=0;m<axes;++m) w*=(c>>m)&1?double(values[m]):1-double(values[m]);
            scale+=w*h["contact_scales"][c].get<double>();
            for(size_t k=0;k<3;++k) center[k]+=w*h["contact_centers_cm"][c][k].get<double>();
        }
        double moment=h["moment_cm2"].get<double>();
        for(size_t m=0;m<axes;++m) {
            moment+=double(values[m])*h["moment_linear_cm2"][m].get<double>();
            for(size_t n=0;n<axes;++n) moment+=double(values[m])*double(values[n])*h["moment_quadratic_cm2"][m][n].get<double>();
        }
        for(size_t k=0;k<3;++k) {
            double offset=h["offset_cm"][k].get<double>();
            for(size_t m=0;m<axes;++m) offset+=double(values[m])*h["offset_deltas_cm"][m][k].get<double>();
            expect(std::abs(actual.offsets[i][k]-offset)<1e-9,"Offset differs from the reference");
            expect(std::abs(actual.contact_centers[i][k]-center[k])<1e-9,"Contact center differs from the reference");
            for(size_t axis=0;axis<3;++axis)
                expect(std::abs(actual.contact_axes[i][axis][k]-scale*h["contact_axes_cm"][axis][k].get<double>())<1e-9,
                       "Contact axes differ from the reference");
        }
        expect(std::abs(double(actual.moments[i])-moment)<1e-3,"Inertia differs from the reference");
    }
}

void synthetic_suite() {
    for(size_t axes:{6,7,8}) {
        const auto source=synthetic(axes,2);
        const auto model=BodyGeometryModel::parse(source);
        expect(model.axes()==axes && model.corners()==(size_t{1}<<axes) && model.schema==2,"Axis count not read");
        expect(BodyGeometryModel::parse(source)==model,"Identical geometry rejected");
        for(size_t c=0;c<model.corners();c+=model.corners()/16) {
            std::vector<float> values(axes);
            for(size_t m=0;m<axes;++m) values[m]=float((c>>m)&1);
            reference_check(source,model,values);
            // At a corner the contact center is that corner's own value.
            const auto at=model.evaluate(values);
            for(size_t k=0;k<3;++k) expect(at.contact_centers[0][k]==source["helpers"][0]["contact_centers_cm"][c][k].get<double>(),
                                           "Corner does not reproduce its measured center");
        }
        std::vector<float> interior(axes);
        for(size_t m=0;m<axes;++m) interior[m]=float(m+1)/float(axes+2);
        reference_check(source,model,interior);
        // The highest axis must actually move the model (no silent six-axis truncation).
        std::vector<float> low(axes,0.f),high(axes,0.f);high[axes-1]=1;
        expect(model.evaluate(low)!=model.evaluate(high),"Last axis ignored");
        rejects([&]{model.evaluate(std::vector<float>(axes-1,0.f));});
        rejects([&]{model.evaluate(std::vector<float>(axes+1,0.f));});
        auto bad=source;bad["corners"]=(size_t{1}<<axes)-1;rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad.erase("corners");rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][3]["contact_scales"].erase(bad["helpers"][3]["contact_scales"].begin());rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][3]["contact_centers_cm"].push_back({0,0,0});rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][2]["moment_quadratic_cm2"][0].erase(bad["helpers"][2]["moment_quadratic_cm2"][0].begin());
        rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["morphs"][axes-1]=bad["morphs"][0];rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["schema"]=1;rejects([&]{BodyGeometryModel::parse(bad);});
        for(float invalid:{-1.f,max_body_morph_weight+.01f,std::numeric_limits<float>::quiet_NaN()}) {
            std::vector<float> values(axes,0.f);values[axes-1]=invalid;rejects([&]{model.evaluate(values);});
        }
        // A slider extended past its authored morph extrapolates the helper offsets linearly.
        {
            std::vector<float> zero(axes,0.f),one(axes,0.f),two(axes,0.f);one[0]=1;two[0]=2;
            const auto a=model.evaluate(zero),b=model.evaluate(one),c=model.evaluate(two);
            for(size_t i=0;i<7;++i) for(size_t axis=0;axis<3;++axis)
                expect(std::abs(c.offsets[i][axis]-(2*b.offsets[i][axis]-a.offsets[i][axis]))<1e-6,"Extended morph does not extrapolate offsets");
        }
    }
    // Outside 6..8 axes, in either schema.
    for(size_t axes:{5,9}) rejects([&]{BodyGeometryModel::parse(synthetic(axes,2));});
    // Schema 1 is the cooked six-axis model: no "corners", exactly six axes, read unchanged.
    const auto legacy=synthetic(6,1);
    const auto old=BodyGeometryModel::parse(legacy);
    expect(old.schema==1 && old.axes()==6 && old.corners()==64,"Six-axis schema 1 model not read");
    std::vector<float> half(6,.5f);
    reference_check(legacy,old,half);
    auto renamed=synthetic(6,2);
    expect(BodyGeometryModel::parse(renamed).evaluate(half)==old.evaluate(half),"Schema 2 six-axis differs from schema 1");
    auto bad=legacy;bad["corners"]=64;rejects([&]{BodyGeometryModel::parse(bad);});
    rejects([&]{BodyGeometryModel::parse(synthetic(8,1));});
    bad=legacy;bad["schema"]=3;rejects([&]{BodyGeometryModel::parse(bad);});
}

// Parity with the reference converters on a measured model and its fixtures (any axis count).
void parity(const char* model_path,const char* fixture_path,Json& summary) {
    std::ifstream model_file(model_path),fixture_file(fixture_path);
    const auto source=Json::parse(model_file),fixtures=Json::parse(fixture_file);
    const auto model=BodyGeometryModel::parse(source);
    expect(BodyGeometryModel::parse(source)==model,"Identical adapter geometry rejected");
    auto changed=model;
    changed.helpers[0].contact_bone+="_other";
    expect(changed!=model,"Another contact bone accepted by preview");
    changed=model;changed.morphs[0]+="_other";
    expect(changed!=model,"Another morph binding accepted by preview");
    changed=model;changed.helpers[0].contact_scales.back()+=0.1;
    expect(changed!=model,"Different corner contact geometry accepted by preview");
    changed=model;changed.helpers[6].moment_quadratic.back().back()+=0.1;
    expect(changed!=model,"Different rig inertia accepted by preview");
    expect(fixtures.at("cases").size()==model.corners()+64,"Expected all corners and interior cases");
    double max_vector=0,max_moment=0;
    for(const auto& fixture:fixtures.at("cases")) {
        const auto values=fixture.at("morphs").get<std::vector<float>>();
        const auto actual=model.evaluate(values);
        expect(actual.shape==values,"Collision shape inputs lost morph coefficients");
        const auto& expected=fixture.at("expected");
        for(size_t i=0;i<7;++i) {
            auto compare=[&](const BodyVector& vector,const Json& target) {
                for(size_t j=0;j<3;++j) {
                    const double residual=std::abs(vector[j]-target[j].get<double>());
                    max_vector=std::max(max_vector,residual);
                    expect(residual<1e-9,"Native morph geometry differs from reference converter");
                }
            };
            compare(actual.offsets[i],expected.at("Offsets")[i]);
            compare(actual.contact_centers[i],expected.at("ContactCenters")[i]);
            for(size_t axis=0;axis<3;++axis)
                compare(actual.contact_axes[i][axis],expected.at(std::string("ContactAxes")+"XYZ"[axis])[i]);
            const double residual=std::abs(double(actual.moments[i])-expected.at("Moments")[i].get<double>());
            max_moment=std::max(max_moment,residual);
            expect(residual<1e-4,"Native body inertia differs from reference converter");
        }
    }
    auto constant=model;
    for(auto& helper:constant.helpers) {
        for(auto& d:helper.offset_deltas) d={};
        for(auto& l:helper.moment_linear) l=0;
        for(auto& row:helper.moment_quadratic) for(auto& q:row) q=0;
        std::fill(helper.contact_centers.begin(),helper.contact_centers.end(),helper.contact_centers[0]);
        std::fill(helper.contact_scales.begin(),helper.contact_scales.end(),helper.contact_scales[0]);
    }
    std::vector<float> zero(model.axes(),0.f),last(model.axes(),0.f);last.back()=1;
    expect(constant.evaluate(zero)!=constant.evaluate(last),
           "Shape-only collision change would be skipped when body helpers stay unchanged");
    for(float invalid:{-1.f,1.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        auto values=zero;values[2]=invalid;rejects([&]{model.evaluate(values);});
    }
    for(const char* key:{"morphs","helpers"}) {
        auto bad=source;bad[key].erase(bad[key].begin());rejects([&]{BodyGeometryModel::parse(bad);});
    }
    for(const char* key:{"offset_cm","offset_deltas_cm","moment_linear_cm2","moment_quadratic_cm2",
                         "contact_centers_cm","contact_scales","contact_axes_cm"}) {
        auto bad=source;bad["helpers"][0][key].erase(bad["helpers"][0][key].begin());
        rejects([&]{BodyGeometryModel::parse(bad);});
    }
    auto bad=source;bad["schema"]=3;rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["morphs"][1]=bad["morphs"][0];rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["bone"]="head";rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["moment_cm2"]=-1;rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["moment_linear_cm2"][0]=-1e7;rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["moment_quadratic_cm2"][0][1]=100;rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["contact_axes_cm"][0]={0,0,0};rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["contact_scales"][32]=0;rejects([&]{BodyGeometryModel::parse(bad);});
    bad=source;bad["helpers"][0]["offset_cm"][0]=true;rejects([&]{BodyGeometryModel::parse(bad);});
    summary["parity"].push_back({{"model",model_path},{"schema",model.schema},{"axes",model.axes()},
        {"corners",model.corners()},{"fixtures",fixtures.at("cases").size()},
        {"maximum_vector_error_cm",max_vector},{"maximum_float_moment_error_cm2",max_moment}});
}

// No arguments: the synthetic 6/7/8-axis and schema suite (ctest). With pairs of
// model and fixture paths: also the measured parity checks for each pair.
int main(int argc,char** argv) {
    try {
        if(argc%2!=1) throw std::runtime_error("Expected model and fixture path pairs");
        synthetic_suite();
        Json summary={{"passed",true},{"parity",Json::array()}};
        for(int i=1;i<argc;i+=2) parity(argv[i],argv[i+1],summary);
        summary["checks"]=checks;
        std::cout<<summary.dump(2)<<'\n';
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

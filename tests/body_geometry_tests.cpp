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
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Expected model and independent fixture paths");
        std::ifstream model_file(argv[1]),fixture_file(argv[2]);
        const auto source=Json::parse(model_file),fixtures=Json::parse(fixture_file);
        const auto model=BodyGeometryModel::parse(source);
        expect(BodyGeometryModel::parse(source)==model,"Identical adapter geometry rejected");
        auto changed=model;
        changed.helpers[0].contact_bone+="_other";
        expect(changed!=model,"Another contact bone accepted by preview");
        changed=model;changed.morphs[0]+="_other";
        expect(changed!=model,"Another morph binding accepted by preview");
        changed=model;changed.helpers[0].contact_scales[63]+=0.1;
        expect(changed!=model,"Different corner contact geometry accepted by preview");
        changed=model;changed.helpers[6].moment_quadratic[5][5]+=0.1;
        expect(changed!=model,"Different rig inertia accepted by preview");
        expect(fixtures.at("cases").size()==128,"Expected all corners and interior cases");
        double max_vector=0,max_moment=0;
        for(const auto& fixture:fixtures.at("cases")) {
            const auto actual=model.evaluate(fixture.at("morphs").get<std::array<float,6>>());
            expect(actual.shape==fixture.at("morphs").get<std::array<float,6>>(),"Collision shape inputs lost morph coefficients");
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
            helper.offset_deltas={};helper.moment_linear={};helper.moment_quadratic={};
            helper.contact_centers.fill(helper.contact_centers[0]);
            helper.contact_scales.fill(helper.contact_scales[0]);
        }
        expect(constant.evaluate({})!=constant.evaluate({0,0,0,0,0,1}),
               "Shape-only collision change would be skipped when body helpers stay unchanged");
        for(float invalid:{-1.f,1.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
            std::array<float,6> values{};values[2]=invalid;rejects([&]{model.evaluate(values);});
        }
        for(const char* key:{"morphs","helpers"}) {
            auto bad=source;bad[key].erase(bad[key].begin());rejects([&]{BodyGeometryModel::parse(bad);});
        }
        for(const char* key:{"offset_cm","offset_deltas_cm","moment_linear_cm2","moment_quadratic_cm2",
                             "contact_centers_cm","contact_scales","contact_axes_cm"}) {
            auto bad=source;bad["helpers"][0][key].erase(bad["helpers"][0][key].begin());
            rejects([&]{BodyGeometryModel::parse(bad);});
        }
        auto bad=source;bad["schema"]=2;rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["morphs"][1]=bad["morphs"][0];rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["bone"]="head";rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["moment_cm2"]=-1;rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["moment_linear_cm2"][0]=-1e7;rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["moment_quadratic_cm2"][0][1]=100;rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["contact_axes_cm"][0]={0,0,0};rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["contact_scales"][32]=0;rejects([&]{BodyGeometryModel::parse(bad);});
        bad=source;bad["helpers"][0]["offset_cm"][0]=true;rejects([&]{BodyGeometryModel::parse(bad);});
        std::cout<<Json{{"passed",true},{"checks",checks},{"fixtures",128},
            {"maximum_vector_error_cm",max_vector},{"maximum_float_moment_error_cm2",max_moment}}.dump(2)<<'\n';
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

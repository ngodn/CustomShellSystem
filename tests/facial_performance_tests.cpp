#include "data.hpp"
#include "performance_clock.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

using namespace css;
namespace {
int checks=0;
void expect(bool value,const char* message) {
    ++checks;
    if(!value) throw std::runtime_error(message);
}
template<class F> void rejects(F action) {
    bool rejected=false;
    try {action();} catch(const std::exception&) {rejected=true;}
    expect(rejected,"Invalid facial performance was accepted");
}
bool near(float a,double b) {return std::abs(double(a)-b)<2.e-7;}
}
int main(int argc,char** argv) {
    try {
        const std::vector<std::string> morphs={"jaw_open","squint_left","unused"};
        const Json document={{"clip","css.eve.dance.comecloser"},{"duration",2.},
            {"curves",{
                {{"morph","jaw_open"},{"keys",{{0.,-.13211426},{.75,1.116883},{2.,.3}}}},
                {{"morph","squint_left"},{"keys",{{.4,.6}}}}
            }}};
        const auto performance=FacialPerformance::parse(document,morphs);
        expect(performance.key_count()==4 && performance.curves.size()==2,"Source key count changed");
        const std::array<float,2> baseline={.2f,-.1f};
        std::array<float,2> output{};
        performance.sample(0.,0.,baseline,output);
        expect(output==baseline,"Zero blend weight changed the selected expression");
        performance.sample(.75,1.,baseline,output);
        expect(near(output[0],1.116883) && near(output[1],.6),"Source weights above one were clamped");
        performance.sample(-1.,1.,baseline,output);
        expect(near(output[0],-.13211426) && near(output[1],.6),"Negative weight or constant extrapolation lost");
        performance.sample(.375,1.,baseline,output);
        expect(near(output[0],(-.13211426+1.116883)/2),"Linear source interval was not preserved");
        performance.sample(10.,1.,baseline,output);
        expect(near(output[0],.3) && near(output[1],.6),"Last key did not hold");
        performance.sample(.75,.5,baseline,output);
        expect(near(output[0],(.2+1.116883)/2),"Facial blend did not use the supplied baseline");
        const auto valid=output;
        for(double bad:{-1.,1.1,std::numeric_limits<double>::infinity()}) {
            rejects([&]{performance.sample(0.,bad,baseline,output);});
            expect(output==valid,"Invalid blend weight partially wrote its output");
        }
        rejects([&]{performance.sample(std::numeric_limits<double>::quiet_NaN(),1.,baseline,output);});
        rejects([&]{performance.sample(0.,1.,std::span(baseline).first(1),output);});
        rejects([&]{performance.sample(0.,1.,baseline,std::span(output).first(1));});
        expect(output==valid,"Invalid time or buffer partially wrote its output");
        for(float bad:{4.01f,std::numeric_limits<float>::quiet_NaN()}) {
            auto invalid_baseline=baseline;invalid_baseline[1]=bad;
            rejects([&]{performance.sample(0.,1.,invalid_baseline,output);});
            expect(output==valid,"Invalid baseline partially wrote its output");
        }
        PerformanceClock clock;
        clock.begin(AnimationPlayback::Once,performance.duration);
        clock.advance(.18);clock.seek(.75);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(near(output[0],1.116883),"Body seek and face sample disagree");
        clock.pause(true);clock.advance(20.);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(near(output[0],1.116883),"Paused body clock advanced facial animation");
        clock.pause(false);clock.set_rate(.5);clock.advance(.5);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(near(output[0],1.116883+(.3-1.116883)*.2),"Body speed change desynchronized the face");
        clock.stop();clock.advance(.09);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(near(output[0],(.2+1.116883+(.3-1.116883)*.2)/2),"Exit fade ignored the body weight");
        clock.advance(.1);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(output==baseline,"Completed body fade did not restore the exact baseline");
        clock.begin(AnimationPlayback::Loop,2.);clock.advance(2.75);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(near(output[0],1.116883),"Loop wrap desynchronized the face");
        clock.stop(true);
        performance.sample(clock.position(),clock.weight(),baseline,output);
        expect(output==baseline,"Immediate interruption retained a facial override");
        auto invalid=[&](auto edit) {auto value=document;edit(value);rejects([&]{FacialPerformance::parse(value,morphs);});};
        invalid([](auto& j){j["clip"]="original";});
        invalid([](auto& j){j["duration"]=true;});
        invalid([](auto& j){j["duration"]=0;});
        invalid([](auto& j){j["duration"]=86401.;});
        invalid([](auto& j){j["curves"]=Json::array();});
        invalid([](auto& j){j["curves"].push_back(j["curves"][0]);});
        invalid([](auto& j){j["curves"][0]["morph"]="Jaw_Open";});
        invalid([](auto& j){j["curves"][0]["morph"]="CSS_ChestSize";});
        invalid([](auto& j){j["curves"][0]["interpolation"]="cubic";});
        invalid([](auto& j){j["curves"][0]["keys"]=Json::array();});
        invalid([](auto& j){j["curves"][0]["keys"][1][0]=0.;});
        invalid([](auto& j){j["curves"][0]["keys"][0][0]=-.1;});
        invalid([](auto& j){j["curves"][0]["keys"][2][0]=2.01;});
        invalid([](auto& j){j["curves"][0]["keys"][0][1]=true;});
        invalid([](auto& j){j["curves"][0]["keys"][0][1]=4.01;});
        invalid([](auto& j){j["curves"][0]["keys"][0][1]=std::numeric_limits<double>::infinity();});
        invalid([](auto& j){j["curves"][0]["keys"][0].push_back(3);});
        invalid([](auto& j){j["loop"]=true;});
        invalid([](auto& j){
            auto& keys=j["curves"][0]["keys"];keys=Json::array();
            for(int i=0;i<16385;++i) keys.push_back({i/10000.,0.});
        });
        invalid([](auto& j){
            auto curve=j["curves"][0];j["curves"]=Json::array();
            for(int i=0;i<129;++i) j["curves"].push_back(curve);
        });
        if(argc==2) {
            const auto actual=read_json(argv[1]);
            const auto names=actual.at("morphs").get<std::vector<std::string>>();
            const auto clip=FacialPerformance::parse(actual.at("performance"),names);
            expect(clip.curves.size()==15 && clip.key_count()==987,"Come Closer source curves changed");
            std::vector<float> zeros(clip.curves.size()),values(clip.curves.size());
            for(const auto& row:actual.at("samples")) {
                clip.sample(row.at("seconds").get<double>(),1.,zeros,values);
                const auto expected=row.at("weights").get<std::vector<double>>();
                expect(expected.size()==values.size(),"Reference curve count differs");
                for(size_t i=0;i<values.size();++i)
                    expect(near(values[i],expected[i]),"Native facial sample differs from decoded source");
            }
        }
        std::cout<<checks<<" facial performance checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

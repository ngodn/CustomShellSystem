#include "data.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <chrono>

using namespace css;
namespace {
int checks=0;
void expect(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
    bool rejected=false;
    try {action();} catch(const std::exception&) {rejected=true;}
    expect(rejected,"Invalid facial catalog was accepted");
}
}
int main(int argc,char** argv) {
    const auto root=fs::temp_directory_path()/("css-facial-library-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const std::string source="/Game/CSS/Eve/SK_Body.SK_Body";
        const std::string target="/Game/CSS/Locomotion/Faces/SK_Body.SK_Body";
        const std::string skeleton="/Game/CSS/Shared/SKEL_Base.SKEL_Base";
        const std::string hash(64,'a');
        const Json document={
            {"id","css.face.eve"},{"morphs",{"Jaw_Left","lip_corner_left","eye_close"}},
            {"expressions",{
                {{"id","css.face.eve.neutral"},{"name","Neutral"},{"weights",{{"Jaw_Left",-.07407111}}}},
                {{"id","css.face.eve.wink"},{"name","Wink"},{"weights",{{"eye_close",1.0}}}}
            }},
            {"adapters",{{{"source_outfit","css.eve"},{"source_mesh",source},{"target_mesh",target},
                {"skeleton",skeleton},{"source_toc_sha256",hash},{"source_bulk_bytes",uint64_t{1234}}}}}
        };
        auto rig=FacialRig::parse(document);
        const auto neutral=rig.weights("css.face.eve.neutral");
        expect(neutral.size()==3 && std::abs(neutral.at("Jaw_Left")+.07407111f)<1e-7f,
               "Signed source weights were clamped or lost");
        const auto wink=rig.weights("css.face.eve.wink");
        expect(wink.at("eye_close")==1 && wink.at("Jaw_Left")==0 && wink.at("lip_corner_left")==0,
               "Changing a sparse expression retained the previous expression");
        expect(!wink.contains("CSS_ChestSize"),"Facial application acquired a body morph");
        expect(!rig.find("original") && !rig.find(""),"Source Neutral was conflated with Reset");
        rejects([&]{rig.weights("original");});
        std::vector<FacialRig> rigs{rig};
        auto resolve=[&](std::string outfit,std::string revision,uint64_t bulk,std::string mesh) {
            return resolve_facial_rig(rigs,outfit,revision,bulk,mesh);
        };
        expect(resolve("css.eve",hash,1234,source).adapter->target_mesh==target,"Verified outfit did not resolve");
        expect(!resolve("css.eve","",1234,source).rig,"Loose catalog acquired facial compatibility");
        expect(!resolve("css.eve",std::string(64,'b'),1234,source).rig,"Different outfit revision acquired old face");
        expect(!resolve("css.eve",hash,1235,source).rig,"Mismatched bulk data acquired old face");
        expect(!resolve("css.other",hash,1234,source).rig,"Different outfit acquired Eve face");
        expect(!resolve("css.eve",hash,1234,target).rig,"Facial adapter recursively replaced itself");
        rigs.clear();
        expect(!resolve("css.eve",hash,1234,source).rig,"Removed pack retained facial compatibility");
        rigs={rig,rig};
        rejects([&]{resolve("css.eve",hash,1234,source);});
        auto invalid=[&](auto edit) {auto changed=document; edit(changed); rejects([&]{FacialRig::parse(changed);});};
        invalid([](auto& j){j["morphs"].push_back("jaw_left");});
        invalid([](auto& j){j["morphs"]={"bad name"};});
        invalid([](auto& j){j["morphs"]=Json::array();});
        invalid([](auto& j){j["expressions"][0]["weights"]["jaw_left"]=1;});
        invalid([](auto& j){j["expressions"][0]["weights"]["body_size"]=1;});
        invalid([](auto& j){j["expressions"][0]["weights"]["Jaw_Left"]=true;});
        invalid([](auto& j){j["expressions"][0]["weights"]["Jaw_Left"]=std::numeric_limits<double>::infinity();});
        invalid([](auto& j){j["expressions"][0]["weights"]["Jaw_Left"]=-4.01;});
        invalid([](auto& j){j["expressions"].push_back(j["expressions"][0]);});
        invalid([](auto& j){j["expressions"][0]["id"]="original";});
        invalid([](auto& j){j["expressions"][0]["name"]="Wink\nOther";});
        invalid([](auto& j){j["adapters"][0]["source_bulk_bytes"]=-1;});
        invalid([](auto& j){j["adapters"][0]["source_bulk_bytes"]=0;});
        invalid([](auto& j){j["adapters"][0]["source_toc_sha256"]=std::string(64,'z');});
        invalid([](auto& j){j["adapters"][0]["target_mesh"]="/Game/Other.Other";});
        invalid([](auto& j){j["adapters"].push_back(j["adapters"][0]);});
        invalid([](auto& j){j["unknown"]=true;});
        Json library={{"id","css.library"},{"name","Library"},{"author","Test"},{"version","1"},
            {"entries",{{{"id","css.pose"},{"name","Pose"},{"category","photo"},{"playback","pose"},
                {"assets",{{{"asset","/Game/CSS/Locomotion/AN_Pose.AN_Pose"},{"skeleton",skeleton}}}}}}}};
        expect(AnimationLibrary::parse(library).faces.empty(),"Old libraries now require faces");
        library["faces"]={document};
        expect(AnimationLibrary::parse(library).faces[0].expressions.size()==2,"Shared catalog lost expressions");
        library["faces"].push_back(document);
        rejects([&]{AnimationLibrary::parse(library);});
        library["faces"][1]["id"]="css.face.other";
        rejects([&]{AnimationLibrary::parse(library);});
        library["faces"]={document};
        fs::create_directories(root);
        atomic_json(root/"first.css.json",{{"schema",1},{"animation_library",library}},false);
        expect(Catalog::load(root).animation_libraries[0].faces.size()==1,"Discovery lost facial rigs");
        auto other=library; other["id"]="css.other.library";other["entries"][0]["id"]="css.other.pose";
        atomic_json(root/"second.css.json",{{"schema",1},{"animation_library",other}},false);
        rejects([&]{Catalog::load(root);});
        other["faces"][0]["id"]="css.face.other";
        atomic_json(root/"second.css.json",{{"schema",1},{"animation_library",other}},false);
        rejects([&]{Catalog::load(root);});
        fs::remove(root/"second.css.json");
        expect(Catalog::load(root).animation_libraries.size()==1,"Removing conflicting library did not recover discovery");
        if(argc==2) {
            const auto actual=FacialRig::parse(read_json(argv[1]));
            expect(actual.morphs.size()==99 && actual.expressions.size()==21 && actual.adapters.size()==6,
                   "Audited Eve facial catalog dimensions differ");
            const auto source_neutral=actual.weights("css.eve.face.neutral");
            expect(std::count_if(source_neutral.begin(),source_neutral.end(),[](const auto& pair){return pair.second!=0;})==7,
                   "Source Neutral lost its seven authored weights");
        }
        fs::remove_all(root);
        std::cout<<checks<<" facial catalog and compatibility checks passed\n";
        return 0;
    } catch(const std::exception& error) {fs::remove_all(root);std::cerr<<error.what()<<'\n';return 1;}
}

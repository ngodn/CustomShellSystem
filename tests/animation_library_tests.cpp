#include "data.hpp"
#include <chrono>
#include <functional>
#include <iostream>

using namespace css;
namespace {
int checks=0;
void expect(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& action) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    expect(rejected,"Invalid animation library accepted");
}
}
int main(int argc,char** argv) {
    const auto root=fs::temp_directory_path()/(
        "css-animation-library-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const Json library=Json::parse(R"({
          "id":"css.locomotion","name":"CSS Locomotion","author":"eins0fx","version":"0.1.0",
          "entries":[
            {"id":"css.eve.idle","name":"Eve Default Idle","category":"idle","playback":"loop",
             "slot":"idle","hide_weapons":true,
             "assets":[{"asset":"/Game/CSS/Locomotion/Eve/AN_Idle.AN_Idle",
                        "skeleton":"/Game/CSS/Shared/SKEL_Base.SKEL_Base"}],
             "aliases":[{"slot":"idle","id":"eve","asset":"/Game/CSS/Eve/Anim/AN_I2_Idle.AN_I2_Idle"}]},
            {"id":"css.eve.dance","name":"Dance","category":"dance","playback":"once",
             "assets":[{"asset":"/Game/CSS/Locomotion/Eve/AN_Dance.AN_Dance",
                        "skeleton":"/Game/CSS/Shared/SKEL_Base.SKEL_Base"}]}
          ]
        })");
        const auto parsed=AnimationLibrary::parse(library);
        expect(parsed.entries.size()==2,"Shared performances lost");
        expect(parsed.entries[0].slot==AnimationSlot::Idle && parsed.entries[0].hide_weapons,"Idle contract lost");
        expect(!parsed.entries[1].slot && parsed.entries[1].playback==AnimationPlayback::Once,"Performance forced into idle");
        expect(parsed.entries[0].aliases[0].asset=="/Game/CSS/Eve/Anim/AN_I2_Idle.AN_I2_Idle","Legacy alias lacks exact asset");
        for(const auto& mutate:std::vector<std::function<void(Json&)>>{
            [](Json& j){j["id"]="original";},
            [](Json& j){j["entries"][0]["id"]="css.feminine";},
            [](Json& j){j["entries"].push_back(j["entries"][0]);},
            [](Json& j){j["entries"]=Json::array();},
            [](Json& j){j["entries"][0]["playback"]="once";},
            [](Json& j){j["entries"][0]["playback"]="pingpong";},
            [](Json& j){j["entries"][0]["slot"]="beacon";},
            [](Json& j){j["entries"][0]["slot"]="walk";},
            [](Json& j){j["entries"][0]["hide_weapons"]="yes";},
            [](Json& j){j["entries"][0]["assets"]=Json::array();},
            [](Json& j){j["entries"][0]["assets"][0]["asset"]="/Game/CSS/Eve/AN_Idle.AN_Idle";},
            [](Json& j){j["entries"][0]["assets"][0]["asset"]="/Game/CSS/Locomotion/../AN_Idle.AN_Idle";},
            [](Json& j){j["entries"][0]["assets"].push_back(j["entries"][0]["assets"][0]);},
            [](Json& j){j["entries"][0]["aliases"][0].erase("asset");},
            [](Json& j){j["entries"][0]["aliases"][0]["slot"]="walk";},
            [](Json& j){j["entries"][0]["aliases"].push_back(j["entries"][0]["aliases"][0]);},
            [](Json& j){j["entries"][0]["name"]="Invisible\nLabel";},
            [](Json& j){j["entries"][0]["unknown"]=true;},
            [](Json& j){j["extra"]=true;}
        }) { auto bad=library; mutate(bad); rejects([&]{AnimationLibrary::parse(bad);}); }
        auto many=library;
        many["entries"]=Json::array();
        for(int i=0;i<256;++i) {
            auto entry=library["entries"][1];entry["id"]="clip."+std::to_string(i);
            many["entries"].push_back(std::move(entry));
        }
        expect(AnimationLibrary::parse(many).entries.size()==256,"Shared limit accidentally uses outfit's 64-choice limit");
        auto extra=many["entries"][0];extra["id"]="clip.256";many["entries"].push_back(extra);
        rejects([&]{AnimationLibrary::parse(many);});
        fs::create_directories(root);
        const auto file=root/"library.css.json";
        atomic_json(file,{{"schema",1},{"animation_library",library}},false);
        const auto loaded=Catalog::load(root);
        expect(loaded.outfits.empty() && loaded.animation_libraries.size()==1,"Library created a wearable outfit");
        const Json outfit={{"id","existing"},{"name","Existing"},{"shells",{"CharacterId.Player.Shell.Genessa"}},
                           {"variants",{{{"id","default"},{"name","Default"},{"mesh","/Game/CSS/Old/SK_Body.SK_Body"}}}}};
        atomic_json(root/"outfit.css.json",{{"schema",1},{"outfits",{outfit}}},false);
        expect(Catalog::load(root).outfits.size()==1,"Library displaced an old outfit");
        atomic_json(root/"duplicate.css.json",{{"schema",1},{"animation_library",library}},false);
        rejects([&]{Catalog::load(root);});
        fs::remove(root/"duplicate.css.json");
        fs::remove(file);
        expect(Catalog::load(root).animation_libraries.empty() && Catalog::load(root).outfits.size()==1,
               "Removing library invalidated existing outfit");
        if(argc==2) {
            const auto packages=Catalog::load(root,argv[1],root/"cache");
            expect(packages.outfits.size()==1,"Packaged library displaced existing outfit");
            expect(packages.animation_libraries.size()==1 && packages.animation_libraries[0].id=="test.valid",
                   "Malformed or conflicting packaged libraries were accepted");
            expect(packages.diagnostics.at("rejected")==4,"Bad library packages were not reported independently");
            expect(packages.diagnostics.at("loaded")==1,"Valid packaged library did not load");
        }
        fs::remove_all(root);
        std::cout<<checks<<" animation library checks passed\n";
    } catch(const std::exception& error) {
        fs::remove_all(root);
        std::cerr<<error.what()<<'\n';return 1;
    }
}

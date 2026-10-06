#include "data.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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
        {
            const std::string human="/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
            auto native=library;
            auto& entry=native["entries"][0];
            const std::string carrier="/Game/CSS/Locomotion/NativeHuman/BS_Idle.BS_Idle";
            entry["assets"].push_back({{"asset","/Game/CSS/Locomotion/NativeHuman/AN_Idle.AN_Idle"},
                {"skeleton",human},{"idle_blend_space",carrier}});
            for(const auto* gait:{"walk","jog","sprint"}) {
                auto movement=native["entries"][0];
                movement["id"]=std::string("css.eve.")+gait;
                movement["slot"]=gait;movement["hide_weapons"]=false;movement.erase("aliases");
                movement["assets"]=Json::array({{{"asset",std::string("/Game/CSS/Locomotion/NativeHuman/BS_")+gait+".BS_"+gait},
                                                {"skeleton",human}}});
                native["entries"].push_back(movement);
            }
            Catalog native_catalog;
            native_catalog.animation_libraries={AnimationLibrary::parse(native)};
            auto selected=resolve_library_animation(native_catalog.animation_libraries,AnimationSlot::Idle,"css.eve.idle",{},human);
            expect(selected.candidates.size()==1 && selected.candidates[0].idle_blend_space==carrier,
                   "Native idle lost its explicit carrier");
            auto performance=resolve_library_performance(native_catalog.animation_libraries,"css.eve.idle",human);
            expect(performance.candidates.size()==1 && performance.candidates[0].idle_blend_space.empty(),
                   "Explicit performance borrowed a locomotion carrier");
            State state;
            const std::string shell="CharacterId.Player.Shell.Genessa";
            expect(set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Idle,"css.eve.idle",human),
                   "Native appearance could not select a shared idle");
            expect(!set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Idle,"css.eve.idle",human),
                   "Repeated native choice dirtied state");
            expect(state.animation_choices.outfits.empty() && state.animation_choices.native.size()==1,
                   "Native choice created a fake outfit");
            for(auto slot:{AnimationSlot::Walk,AnimationSlot::Jog,AnimationSlot::Sprint})
                expect(set_animation_choice(state,native_catalog,shell,"","",slot,
                    std::string("css.eve.")+animation_slot_name(slot),human),"Native movement choice rejected");
            expect(state.animation_choices.native.size()==4,"Native slot selections overwrote one another");
            save_profile_snapshot(state,"native");
            const auto restored=State::parse(state.json());
            expect(restored.animation_choices==state.animation_choices &&
                   restored.presets.at("native").animation_choices==state.animation_choices,
                   "Native choice was lost from state or profile");
            const auto before=state.json();
            rejects([&]{set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Idle,"css.eve.dance",human);});
            rejects([&]{set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Idle,"css.eve.idle","wrong");});
            rejects([&]{set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Beacon,"original",human);});
            rejects([&]{set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Jog,feminine_animation_id,human);});
            expect(state.json()==before,"Rejected native choice changed state");
            native_catalog.animation_libraries.clear();native_catalog.animation_option_cache.clear();
            const auto* saved=state.animation_choices.find("","",AnimationSlot::Idle);
            auto menu=animation_menu(native_catalog.animation_options("","",AnimationSlot::Idle),AnimationSlot::Idle,saved,false);
            expect(saved && !menu.items[menu.selected].available && *saved=="css.eve.idle",
                   "Removing the pack erased or hid the saved native choice");
            expect(set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Idle,"original",human),
                   "Missing native pack prevented selecting Default");
            set_animation_choice(state,native_catalog,shell,"","",AnimationSlot::Idle,feminine_animation_id,human);
            expect(use_feminine_animation(state,shell,AnimationSlot::Idle),"Native feminine idle was ignored");
            expect(set_legacy_walk_choice(state,native_catalog,shell,"normal") &&
                   !use_feminine_animation(state,shell,AnimationSlot::Idle),"Legacy Default left a native idle override");
            auto invalid=native;
            invalid["entries"][0]["assets"][1]["idle_blend_space"]="/Game/Elsewhere/BS_Idle.BS_Idle";
            rejects([&]{AnimationLibrary::parse(invalid);});
            invalid=native;invalid["entries"][0]["assets"][1]["skeleton"]="/Game/CSS/Shared/SKEL_Base.SKEL_Base";
            rejects([&]{AnimationLibrary::parse(invalid);});
            invalid=native;invalid["entries"][0]["slot"]="walk";invalid["entries"][0]["hide_weapons"]=false;
            rejects([&]{AnimationLibrary::parse(invalid);});
            rejects([&]{AnimationChoices::parse(Json::parse(R"({"$native":{"beacon":"original"}})"));});
            rejects([&]{AnimationChoices::parse(Json::parse(R"({"$native":{"idle":12}})"));});
        }
        const std::string base="/Game/CSS/Shared/SKEL_Base.SKEL_Base";
        const std::string wings="/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
        const auto bundled=AnimationSet::parse(Json::parse(R"({"idle":[
          {"id":"eve","name":"Same display name","clip":"/Game/CSS/Eve/Anim/AN_I2_Idle.AN_I2_Idle","hide_weapons":true}
        ]})")).slots.at(AnimationSlot::Idle);
        const std::vector<AnimationLibrary> libraries{parsed};
        const auto resolve=[&](const std::string& choice,const std::vector<AnimationOption>& options,const std::string& target) {
            return resolve_library_animation(libraries,AnimationSlot::Idle,choice,options,target);
        };
        auto match=resolve("eve",bundled,base);
        expect(match.entry && match.entry->id=="css.eve.idle","Audited old ID did not resolve to shared identity");
        expect(match.candidates.size()==2 && !match.candidates[0].bundled && match.candidates[1].bundled,
               "Shared preference or bundled fallback order lost");
        expect(match.candidates[1].asset==bundled[0].clip && match.candidates[1].declared_skeleton.empty(),
               "Bundled fallback invented a skeleton declaration");
        expect(resolve("css.eve.idle",bundled,base).candidates.size()==2,"Canonical choice lost old bundled fallback");
        expect(resolve("css.eve.idle",{},base).candidates.size()==1,"Shared-only installation needs an outfit alias");
        expect(resolve("eve",{},base).entry==nullptr,"Legacy ID matched without its exact bundled asset");
        auto specialized=bundled;specialized[0].clip="/Game/CSS/UnholyGenessa/AN_Wings.AN_Wings";
        expect(!resolve("eve",specialized,wings).entry,"Unaliased wing animation was replaced by name or ID");
        specialized=bundled;specialized[0].hide_weapons=false;
        expect(!resolve("eve",specialized,base).entry,"Alias silently changed weapon visibility");
        specialized=bundled;specialized[0].id="css.eve.idle";
        expect(!resolve("css.eve.idle",specialized,base).entry,"Shared identity took over an unaliased outfit choice");
        auto alternatives=libraries;
        alternatives[0].entries[0].assets.push_back({"/Game/CSS/Locomotion/Wings/AN_Idle.AN_Idle",wings});
        match=resolve_library_animation(alternatives,AnimationSlot::Idle,"eve",bundled,wings);
        expect(match.candidates.size()==3 && match.candidates[0].declared_skeleton==wings &&
               match.candidates[1].declared_skeleton==base && match.candidates[2].bundled,
               "Exact wing rig did not precede directed base compatibility and fallback");
        match=resolve("css.eve.idle",{},"/Game/Unknown/Skeleton.Skeleton");
        expect(match.entry && match.candidates.empty(),"Unsupported rig accepted shared asset");
        expect(resolve("eve",bundled,"/Game/Unknown/Skeleton.Skeleton").candidates.size()==1,
               "Unsupported shared rig discarded the outfit's fallback before runtime validation");
        for(const char* choice:{"original","css.feminine","","missing","css.eve.dance"})
            expect(!resolve(choice,bundled,base).entry,"Default or one-shot performance resolved as a shared idle");
        expect(!resolve_library_animation({},AnimationSlot::Idle,"eve",bundled,base).entry &&
               resolve_animation(bundled,AnimationSlot::Idle,"eve").asset==bundled[0].clip,
               "Removing shared library lost old bundled resolution");
        auto ambiguous=libraries;ambiguous.push_back(parsed);
        expect(!resolve_library_animation(ambiguous,AnimationSlot::Idle,"eve",bundled,base).entry,
               "Ambiguous direct resolver input chose a library by order");
        const auto merged=merge_library_animation_options(libraries,AnimationSlot::Idle,bundled);
        expect(merged.size()==1 && merged[0].id=="eve" && merged[0].library_identity=="css.eve.idle",
               "Shared pack duplicated an audited bundled idle or changed its persisted ID");
        expect(merged[0].clip==bundled[0].clip,"Menu merge destroyed bundled fallback metadata");
        const std::string canonical="css.eve.idle";
        const auto menu=animation_menu(merged,AnimationSlot::Idle,&canonical,false);
        expect(menu.items[menu.selected].id=="eve" && menu.items[menu.selected].available,
               "Canonical selection did not select its deduplicated menu row");
        expect(resolve_animation(merged,AnimationSlot::Idle,canonical).asset==bundled[0].clip,
               "Merged canonical alias lost its retained outfit option");
        const auto shared_only=merge_library_animation_options(libraries,AnimationSlot::Idle,{});
        expect(shared_only.size()==1 && shared_only[0].id==canonical,"Shared-only outfit has no idle choice");
        expect(merge_library_animation_options(libraries,AnimationSlot::Walk,{}).empty(),"Idle leaked into a movement slot");
        specialized=bundled;specialized[0].clip="/Game/CSS/UnholyGenessa/AN_Wings.AN_Wings";
        const auto specialized_menu=merge_library_animation_options(libraries,AnimationSlot::Idle,specialized);
        expect(specialized_menu.size()==2 && specialized_menu[0].clip==specialized[0].clip &&
               specialized_menu[0].library_identity.empty(),"Menu merge replaced a specialized wing clip");
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
        const auto combined=Catalog::load(root);
        expect(combined.outfits.size()==1,"Library displaced an old outfit");
        expect(combined.animation_options("existing","default",AnimationSlot::Idle).size()==1,
               "Catalog did not expose shared idle to a valid outfit");
        expect(combined.bundled_animation_options("existing","default",AnimationSlot::Idle).empty(),
               "Shared cache polluted bundled asset metadata");
        expect(combined.animation_options("existing","missing",AnimationSlot::Idle).empty(),
               "Missing outfit variant acquired shared options");
        const auto* cached=combined.animation_options("existing","default",AnimationSlot::Idle).data();
        expect(cached==combined.animation_options("existing","default",AnimationSlot::Idle).data(),
               "Unchanged merged catalog was rebuilt");
        auto aliased=combined;
        aliased.outfits[0].animations.slots[AnimationSlot::Idle]=bundled;
        aliased.animation_option_cache.clear();
        State saved;
        saved.selections["CharacterId.Player.Shell.Genessa"].outfit="existing";
        saved.selections["CharacterId.Player.Shell.Genessa"].variant="default";
        set_animation_choice(saved,aliased,"CharacterId.Player.Shell.Genessa","existing","default",AnimationSlot::Idle,canonical);
        expect(saved.animation_choices.get("existing","default",AnimationSlot::Idle)=="eve",
               "Selecting shared alias did not preserve the outfit's fallback ID");
        aliased.animation_libraries.clear();aliased.animation_option_cache.clear();
        expect(resolve_animation(aliased.animation_options("existing","default",AnimationSlot::Idle),AnimationSlot::Idle,
               saved.animation_choices.get("existing","default",AnimationSlot::Idle)).asset==bundled[0].clip,
               "Removing shared pack after saving lost the bundled idle");
        atomic_json(root/"duplicate.css.json",{{"schema",1},{"animation_library",library}},false);
        rejects([&]{Catalog::load(root);});
        fs::remove(root/"duplicate.css.json");
        fs::remove(file);
        expect(Catalog::load(root).animation_libraries.empty() && Catalog::load(root).outfits.size()==1,
               "Removing library invalidated existing outfit");
        if(argc==2) {
            const auto packages=Catalog::load(root,argv[1],root/"cache");
            expect(packages.outfits.size()==1,"Packaged library displaced existing outfit");
            expect(packages.outfit_revisions.empty(),"Loose outfit acquired verified package identity");
            expect(packages.animation_libraries.size()==1 && packages.animation_libraries[0].id=="test.valid",
                   "Malformed or conflicting packaged libraries were accepted");
            expect(packages.diagnostics.at("rejected")==4,"Bad library packages were not reported independently");
            expect(packages.diagnostics.at("loaded")==1,"Valid packaged library did not load");
        }
        if(argc==3 && (std::string_view(argv[1])=="--audited-pack" || std::string_view(argv[1])=="--audited-facial-pack" ||
                       std::string_view(argv[1])=="--audited-face-motion-pack" ||
                       std::string_view(argv[1])=="--audited-blender-face-pack" ||
                       std::string_view(argv[1])=="--audited-dance-pack" ||
                       std::string_view(argv[1])=="--audited-dance-isolated-pack" ||
                       std::string_view(argv[1])=="--audited-cw-face-pack" ||
                       std::string_view(argv[1])=="--audited-cw-face-only-pack" ||
                       std::string_view(argv[1])=="--audited-locomotion-pack" ||
                       std::string_view(argv[1])=="--audited-locomotion-only-pack" ||
                       std::string_view(argv[1])=="--audited-native-pack" ||
                       std::string_view(argv[1])=="--audited-native-only-pack" ||
                       std::string_view(argv[1])=="--audited-expanded-pack" ||
                       std::string_view(argv[1])=="--audited-expanded-isolated-pack" ||
                       std::string_view(argv[1])=="--audited-isolated-pack")) {
            const bool commander_face=std::string_view(argv[1])=="--audited-cw-face-pack" ||
                                      std::string_view(argv[1])=="--audited-cw-face-only-pack";
            const bool locomotion=commander_face || std::string_view(argv[1])=="--audited-locomotion-pack" ||
                                  std::string_view(argv[1])=="--audited-locomotion-only-pack";
            const bool native_only=std::string_view(argv[1])=="--audited-cw-face-only-pack" ||
                                   std::string_view(argv[1])=="--audited-native-only-pack" ||
                                   std::string_view(argv[1])=="--audited-locomotion-only-pack";
            const bool native_pack=(native_only && !locomotion) || std::string_view(argv[1])=="--audited-native-pack";
            const bool expanded=std::string_view(argv[1])=="--audited-expanded-pack" ||
                                std::string_view(argv[1])=="--audited-expanded-isolated-pack";
            const bool isolated=std::string_view(argv[1])=="--audited-isolated-pack" ||
                                std::string_view(argv[1])=="--audited-expanded-isolated-pack" ||
                                std::string_view(argv[1])=="--audited-dance-isolated-pack";
            const bool dances=locomotion || native_pack || std::string_view(argv[1])=="--audited-dance-pack" ||
                              std::string_view(argv[1])=="--audited-dance-isolated-pack";
            const auto packages=Catalog::load(root,argv[2],root/"audit-cache");
            expect(packages.animation_libraries.size()==1,"Audited pack did not load as one library");
            const auto& audited=packages.animation_libraries[0];
            expect(audited.entries.size()==(locomotion?62:dances?59:57) && audited.adapters.size()==(expanded?6:5),"Audited animation set changed");
            if(dances) {
                for(const auto* id:{"css.eve.dance.dumbdumb","css.eve.dance.irisout"}) {
                    const auto count=std::count_if(audited.entries.begin(),audited.entries.end(),[&](const auto& entry) {
                        return entry.id==id && entry.playback==AnimationPlayback::Once &&
                               !entry.slot && entry.hide_weapons && entry.assets.size()==1 &&
                               entry.assets.front().skeleton==base;
                    });
                    expect(count==1,"Corrected dance lost its identity or playback contract");
                }
            }
            if(locomotion) {
                const std::string human="/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
                size_t native_alternatives=0;
                for(const auto& entry:audited.entries) for(const auto& asset:entry.assets)
                    if(asset.skeleton==human) ++native_alternatives;
                expect(native_alternatives==4,"Native scope expanded beyond four locomotion choices");
                for(auto slot:{AnimationSlot::Idle,AnimationSlot::Walk,AnimationSlot::Jog,AnimationSlot::Sprint}) {
                    const auto id=slot==AnimationSlot::Idle?std::string("css.eve.idle.default"):
                                  std::string("css.eve.")+animation_slot_name(slot);
                    const auto match=resolve_library_animation(packages.animation_libraries,slot,id,{},human);
                    expect(match.entry && match.candidates.size()==1 && match.candidates[0].declared_skeleton==human,
                           "Native locomotion cannot resolve without an outfit manifest");
                    expect((slot==AnimationSlot::Idle)==!match.candidates[0].idle_blend_space.empty(),
                           "Native carrier assigned to the wrong slot");
                }
                expect(resolve_library_performance(packages.animation_libraries,"css.eve.dance.blackhole",human).candidates.empty(),
                       "Out-of-scope native dance remained in the candidate");
                expect(resolve_library_performance(packages.animation_libraries,"css.eve.dance.blackhole",base).candidates.size()==1,
                       "Limiting native scope removed the custom-rig dance");
            }
            if(native_pack) {
                const std::string human="/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
                const auto native=resolve_library_performance(packages.animation_libraries,"css.eve.dance.blackhole",human);
                expect(native.entry && native.candidates.size()==1 && native.candidates[0].declared_skeleton==human &&
                    native.candidates[0].asset=="/Game/CSS/Locomotion/NativeHuman/Studies/NH2/AN_BlackHole.AN_BlackHole",
                    "Native Black Hole did not resolve independently of an outfit");
                const auto custom=resolve_library_performance(packages.animation_libraries,"css.eve.dance.blackhole",base);
                expect(custom.entry && custom.candidates.size()==1 && custom.candidates[0].declared_skeleton==base,
                    "Native candidate displaced the custom-rig performance");
                expect(resolve_library_performance(packages.animation_libraries,"css.eve.dance.blackhole",
                    "/Game/Unknown/Body.Body").candidates.empty(),"Native candidate opened an unsupported rig");
            }
            expect(packages.outfit_revisions.size()==(native_only?0:isolated?1:expanded?4:3),"Source outfits lack verified revisions");
            size_t resolved_layers=0;
            for(const auto& adapter:audited.adapters) {
                const auto found=packages.outfit_revisions.find(adapter.source_outfit);
                if((isolated || native_only) && found==packages.outfit_revisions.end()) {
                    expect(!resolve_performance_adapter(packages.animation_libraries,adapter.source_outfit,
                        {},adapter.source_class,adapter.skeleton),"Absent outfit gained an unverified adapter");
                    continue;
                }
                expect(found!=packages.outfit_revisions.end(),"Adapter source outfit was not discovered");
                const auto* resolved=resolve_performance_adapter(packages.animation_libraries,adapter.source_outfit,
                    found->second,adapter.source_class,adapter.skeleton);
                expect(resolved && resolved->target_class==adapter.target_class,"Packaged adapter does not resolve");
                ++resolved_layers;
            }
            expect(native_only?resolved_layers==0:resolved_layers>0,"Unexpected outfit performance layer count");
            expect(packages.diagnostics.at("rejected")==0,"Audited package set contains rejected metadata");
            if(std::string_view(argv[1])!="--audited-pack") {
                expect(audited.faces.size()==(commander_face?2:1) && audited.faces.front().expressions.size()==21 &&
                       audited.faces.front().adapters.size()==6,"Combined pack lost facial metadata");
                if(commander_face) {
                    const auto& rig=audited.faces.back();
                    expect(rig.id=="css.commanderwhite.face" && rig.expressions.size()==9 &&
                           rig.morphs.size()==74 && rig.adapters.size()==1 && rig.performances.empty(),
                           "Commander White lost its verified static expression contract");
                }
                size_t matched=0;
                for(const auto& outfit:packages.outfits) for(const auto& variant:outfit.variants) {
                    const auto face=packages.facial_match(outfit.id,variant.id);
                    if(!face.rig) continue;
                    expect(face.adapter && face.adapter->source_mesh==variant.mesh,
                           "Facial adapter resolved to another variant's mesh");
                    ++matched;
                }
                size_t expected_faces=0;
                for(const auto& rig:audited.faces) for(const auto& adapter:rig.adapters)
                    if(packages.outfit_revisions.contains(adapter.source_outfit)) ++expected_faces;
                expect(matched==expected_faces,"Combined pack did not resolve exactly the installed outfit's faces");
                const bool blender_face=std::string_view(argv[1])=="--audited-blender-face-pack" || isolated || expanded || dances;
                if(std::string_view(argv[1])=="--audited-face-motion-pack" || blender_face) {
                    const auto& rig=audited.faces.front();
                    const auto* performance=rig.find_performance("css.eve.dance.comecloser");
                    expect(rig.morphs.size()==105 && rig.performances.size()==(blender_face?2:1),
                           "Facial performance catalog dimensions differ");
                    expect(performance && performance->curves.size()==15 && performance->key_count()==987,
                           "Packaged Come Closer lost its original facial curves");
                    if(blender_face) {
                        const auto* gesture=rig.find_performance("css.eve.performance.example200");
                        expect(gesture && gesture->curves.size()==1 && gesture->key_count()==275 &&
                               gesture->duration==.36,"Packaged Gesture 200 face differs from its source");
                        const std::array<float,1> baseline={0.f};std::array<float,1> values{};
                        for(int step=0;step<=1000;++step) {
                            const double t=step/1000.;
                            gesture->sample(.36*t,1.,baseline,values);
                            expect(std::abs(values[0]-(3*t*t-2*t*t*t))<1.01e-5,
                                   "Packaged Bezier conversion exceeds its error bound");
                        }
                    }
                }
            }
        }
        fs::remove_all(root);
        std::cout<<checks<<" animation library checks passed\n";
    } catch(const std::exception& error) {
        fs::remove_all(root);
        std::cerr<<error.what()<<'\n';return 1;
    }
}

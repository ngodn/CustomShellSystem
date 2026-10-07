#include "performance_session.hpp"
#include "performance_request.hpp"
#include <cmath>
#include <iostream>

using namespace css;
namespace {
int checks=0;
void expect(bool ok,const char* message) { ++checks;if(!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    expect(rejected,"Invalid performance request was accepted");
}
}
int main() {
    try {
        PerformanceSession session;
        rejects([&]{session.stop(0);});
        const auto first=session.begin("css.dance",AnimationPlayback::Once,10.);
        session.advance(2.);
        rejects([&]{session.begin("css.pose",AnimationPlayback::Pose,1.);});
        rejects([&]{session.seek(first+1,8.);});
        expect(session.clip()=="css.dance" && session.clock().position()==2.,"Rejected request changed active take");
        session.stop(first);session.advance(.2);
        const auto second=session.begin("css.pose",AnimationPlayback::Pose,1.);
        rejects([&]{session.stop(first);});
        rejects([&]{session.pause(first,true);});
        rejects([&]{session.rate(first,2.);});
        expect(second!=first && session.active(),"Stale take stopped a newer one");
        session.advance(1.);
        expect(session.clock().phase()==PerformancePhase::Holding,"Photo pose did not hold");
        session.interrupt();
        rejects([&]{session.seek(second,0.);});
        rejects([&]{session.begin("css.bad",AnimationPlayback::Once,0.);});
        expect(session.token()==second && !session.active(),"Failed start consumed a session");

        for(const auto authored:{AnimationPlayback::Once,AnimationPlayback::Pose,AnimationPlayback::Loop}) {
            session.begin("css.idle",authored,1.,1.,0.,PerformanceUse::Idle);
            session.advance(2.25);
            expect(session.active() && session.clock().position()==.25 && session.clock().weight()==1.,
                   "A selected idle stopped or held instead of repeating with its facial clock");
            session.interrupt();
            expect(!session.active() && session.clock().weight()==0.,"Idle interruption retained its pose weight");
        }
        session.begin("css.scene",AnimationPlayback::Once,1.);
        session.advance(2.);
        expect(!session.active(),"Automatic idle looping changed CINE's authored one-shot mode");

        FacialPerformance face{"css.idle",2.,{{"eye_close",{{0.,0.f},{2.,1.f}}}}};
        for(const int fps:{30,60,144}) {
            PerformanceSession body;
            body.begin(face.clip,AnimationPlayback::Once,face.duration,1.,0.,PerformanceUse::Idle);
            FacialPerformancePose facial_pose(face,{{"eye_close",.2f}});
            for(int frame=0;frame<fps*5/2;++frame) body.advance(1./fps);
            const auto& pose=facial_pose.sample(body.clock().position(),body.clock().weight());
            expect(std::abs(body.clock().position()-.5)<1.e-10 && std::abs(pose.at("eye_close")-.25f)<1.e-6,
                   "Idle facial playback drifted from the body loop at a different frame rate");
            body.advance(0.);
            expect(std::abs(body.clock().position()-.5)<1.e-10,"Paused game time advanced the idle face");
            body.interrupt();
            expect(std::abs(facial_pose.sample(body.clock().position(),body.clock().weight()).at("eye_close")-.2f)<1.e-6,
                   "Interrupted idle did not restore its selected expression baseline");
        }

        AutomaticIdlePlayback automatic;
        PerformanceContext idle_context{1,2,3,"css.outfit","black","/Game/CSS/Body.Body"};
        automatic.begin(idle_context,"css.idle",10);
        expect(automatic.owns(10) && !automatic.owns(11),"An automatic idle claimed another scene's token");
        expect(automatic.matches(idle_context,"css.idle"),"Automatic idle lost its binding");
        auto changed_context=idle_context; ++changed_context.player;
        expect(!automatic.matches(changed_context,"css.idle"),"Automatic idle survived pawn replacement");
        changed_context=idle_context; ++changed_context.appearance;
        expect(!automatic.matches(changed_context,"css.idle"),"Automatic idle survived appearance replacement");
        changed_context=idle_context; ++changed_context.catalog;
        expect(!automatic.matches(changed_context,"css.idle"),"Automatic idle survived catalog replacement");
        expect(!automatic.matches(idle_context,"css.other"),"Automatic idle ignored a new idle selection");
        automatic.fail(idle_context,"css.idle");
        expect(!automatic.active() && !automatic.retry_allowed(idle_context,"css.idle"),
               "Failed automatic idle would retry every update");
        automatic.clear();
        expect(!automatic.retry_allowed(idle_context,"css.idle"),"Session cleanup erased the failed-binding guard");
        expect(automatic.retry_allowed(changed_context,"css.idle") && automatic.retry_allowed(idle_context,"css.other"),
               "A failed facial binding blocked a changed catalog or selection");
        automatic.begin(changed_context,"css.idle",11);
        expect(automatic.owns(11) && !automatic.owns(10),"Previous idle still owned the new session");
        automatic.clear();
        expect(!automatic.active() && !automatic.owns(11),"CINE handoff retained automatic ownership");
        rejects([&]{automatic.begin(idle_context,"css.idle",0);});

        const std::string base="/Game/CSS/Shared/SKEL_Base.SKEL_Base";
        const std::string wings="/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
        AnimationLibrary library;
        LibraryAnimation dance;
        dance.id="css.dance";
        dance.assets={{"/Game/CSS/Locomotion/AN_Base.AN_Base",base},
                      {"/Game/CSS/Locomotion/AN_Wings.AN_Wings",wings}};
        library.entries={dance};
        std::vector<AnimationLibrary> libraries{library};
        auto match=resolve_library_performance(libraries,"css.dance",wings);
        expect(match.entry && match.candidates.size()==2,"Audited skeleton alternatives were lost");
        expect(match.candidates.front().declared_skeleton==wings,"Base rig took precedence over exact rig");
        expect(resolve_library_performance(libraries,"css.dance",base).candidates.size()==1,
               "Wing-specific clip leaked onto base skeleton");
        expect(resolve_library_performance(libraries,"css.dance","/Game/Unknown.Unknown").candidates.empty(),
               "Unaudited skeleton became playable");
        expect(!resolve_library_performance(libraries,"eve",base).entry,"Legacy ID selected an unrelated performance");
        expect(!resolve_library_performance(libraries,dance.assets[0].asset,base).entry,"Raw path bypassed catalog identity");
        libraries.push_back(library);
        rejects([&]{resolve_library_performance(libraries,"css.dance",base);});
        libraries.resize(1);
        libraries[0].entries[0].slot=AnimationSlot::Walk;
        rejects([&]{resolve_library_performance(libraries,"css.dance",base);});
        const AnimationContainerRevision revision{std::string(64,'a'),1234};
        const std::string source="/Game/CSS/Outfit/ABP_Body.ABP_Body_C";
        const std::string target="/Game/CSS/Locomotion/Adapters/ABP_Body.ABP_Body_C";
        nlohmann::json document={
            {"id","css.library"},{"name","Library"},{"author","Test"},{"version","1"},
            {"entries",{{{"id","css.dance"},{"name","Dance"},{"category","dance"},{"playback","once"},
                {"assets",{{{"asset",dance.assets[0].asset},{"skeleton",base}}}}}}},
            {"adapters",{{{"source_outfit","css.outfit"},{"source_class",source},{"target_class",target},
                {"skeleton",base},{"source_toc_sha256",revision.toc_sha256},{"source_bulk_bytes",uint64_t{1234}}}}}
        };
        libraries={AnimationLibrary::parse(document)};
        const auto resolve=[&](const AnimationContainerRevision& rev,const std::string& outfit,
                               const std::string& graph,const std::string& rig) {
            return resolve_performance_adapter(libraries,outfit,rev,graph,rig);
        };
        expect(resolve(revision,"css.outfit",source,base)->target_class==target,"Audited adapter was not resolved");
        expect(!resolve({},"css.outfit",source,base),"Loose catalog acquired package provenance");
        expect(!resolve({std::string(64,'b'),1234},"css.outfit",source,base),"A new outfit version used an old graph");
        expect(!resolve({revision.toc_sha256,1235},"css.outfit",source,base),"Mismatched bulk size became compatible");
        expect(!resolve(revision,"css.other",source,base),"Another outfit acquired the adapter");
        expect(!resolve(revision,"css.outfit",target,base),"Adapter recursively replaced itself");
        expect(!resolve(revision,"css.outfit",source,wings),"Different skeleton acquired adapter");
        const auto bound=[&](const AnimationContainerRevision& rev,const std::string& outfit,
                             const std::string& graph,const std::string& rig) {
            return resolve_performance_layer(libraries,outfit,rev,graph,rig);
        };
        expect(bound(revision,"css.outfit",target,base)->source_class==source,
               "Catalog-defined layer outside the original class list was rejected");
        expect(!bound({},"css.outfit",target,base),"Unproven package acquired an active layer");
        expect(!bound({std::string(64,'b'),1234},"css.outfit",target,base),"Stale layer revision was accepted");
        expect(!bound({revision.toc_sha256,1235},"css.outfit",target,base),"Different bulk acquired active layer");
        expect(!bound(revision,"css.other",target,base),"Another outfit acquired active layer");
        expect(!bound(revision,"css.outfit",source,base),"Original graph was mistaken for a performance layer");
        expect(!bound(revision,"css.outfit",target,wings),"Different rig acquired active layer");
        expect(!bound(revision,"css.outfit","/Game/CSS/Locomotion/Adapters/ABP_Eve.ABP_Eve_C",base),
               "An unlisted former hardcoded class was accepted");
        libraries.push_back(libraries[0]);
        rejects([&]{resolve(revision,"css.outfit",source,base);});
        rejects([&]{bound(revision,"css.outfit",target,base);});
        const auto original=document;
        document["adapters"][0]["source_toc_sha256"]="short";
        rejects([&]{AnimationLibrary::parse(document);});
        document=original;document["adapters"][0]["source_bulk_bytes"]=-1;
        rejects([&]{AnimationLibrary::parse(document);});
        document=original;document["adapters"][0]["target_class"]="/Game/Other.Other_C";
        rejects([&]{AnimationLibrary::parse(document);});
        document=original;document["adapters"].push_back(document["adapters"][0]);
        rejects([&]{AnimationLibrary::parse(document);});
        PendingPerformance pending;
        const PerformanceContext context{1,2,3,"outfit","variant","/Game/CSS/Body.Body"};
        using Pending=PendingPerformance::State;
        expect(pending.poll(context,0,false)==Pending::Empty,"Empty selection queued a take");
        pending.begin("css.dance",context,1000);
        expect(pending.poll(context,1100,false)==Pending::Waiting,"Menu close debounce was skipped");
        expect(pending.poll(context,1500,true)==Pending::Waiting,"Performance started inside menu");
        expect(pending.poll(context,1500,false)==Pending::Ready,"Closed menu did not release selection");
        expect(pending.poll(context,6000,true)==Pending::Expired,"Stuck menu retained a delayed take");
        for(int field=0;field<6;++field) {
            auto changed=context;
            if(field==0) ++changed.player;
            if(field==1) ++changed.appearance;
            if(field==2) ++changed.catalog;
            if(field==3) changed.outfit="another";
            if(field==4) changed.variant="another";
            if(field==5) changed.mesh="/Game/Other.Other";
            expect(pending.poll(changed,1500,false)==Pending::Changed,"Stale selection crossed a context change");
        }
        pending.clear();
        expect(!pending.active() && pending.poll(context,1600,false)==Pending::Empty,"Canceled selection started later");
        const PerformanceContext native{1,2,3,"","","/Game/Sparta/Body.Body"};
        pending.begin("css.dance",native,2000);
        expect(pending.poll(native,2300,false)==Pending::Ready,"Native mesh required a CSS outfit manifest");
        auto replaced=native;replaced.mesh="/Game/Other/Body.Body";
        expect(pending.poll(replaced,2300,false)==Pending::Changed,"External native mesh change kept a pending take");
        rejects([&]{pending.begin("css.dance",{1,2,3,"","",""},2000);});
        rejects([&]{pending.begin("css.dance",{1,2,3,"outfit","","/Game/Body.Body"},2000);});
        rejects([&]{pending.begin("css.dance",{1,2,3,"","variant","/Game/Body.Body"},2000);});
        std::cout<<checks<<" performance session/resolver checks passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

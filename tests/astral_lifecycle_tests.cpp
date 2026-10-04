#include "astral_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace css;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
size_t events(const AstralLifecycle::Changes& changes,AstralEventKind kind) {
    size_t count=0;
    for(const auto& event:changes.view()) if(event.kind==kind) ++count;
    return count;
}
int main() {
    const AstralContext context{{1,1},{2,1},{3,1}};
    AstralObservation faithful{{4,1},context.player,{5,1},{6,1},{7,1},AstralKind::faithful,true,true,false,false};
    AstralLifecycle registry;
    auto sample=[&](const AstralObservation& row) { return registry.observe(context,{&row,1}); };
    check(astral_shell("CharacterId.Player.Shell.Genessa"),"Faithful gameplay tag rejected");
    check(astral_shell("CharacterId.Player.Darkform.CorruptedGenessa"),"Live Stray gameplay tag rejected");
    for(const auto tag:{"", "CharacterId.Player.Shell.CorruptedGenessa", "CharacterId.Player.Harbinger",
                        "/Game/CSS/UnholyGenessa/SK_EveW3", "CharacterId.Player.Shell.Genessa.Other"})
        check(!astral_shell(tag),"Appearance or unrelated shell granted doubles eligibility");
    check(astral_kind("AstralCloneSecondary")==AstralKind::stray_secondary,"Secondary not supported");
    check(astral_kind("AstralCopyOther")==AstralKind::unknown,"Unknown summon accepted");
    auto changes=sample(faithful);
    check(changes.accepted && events(changes,AstralEventKind::discovered)==1 &&
        events(changes,AstralEventKind::activated)==1,"First enabled summon did not activate");
    check(registry.entries()[0].activation==1,"First activation generation wrong");
    check(sample(faithful).count==0,"Stable sample generates work repeatedly");

    // Observed Faithful reuse: uncached/enabled, cached/disabled, uncached/enabled.
    faithful.cached=true; faithful.enabled=false;
    check(events(sample(faithful),AstralEventKind::deactivated)==1,"Pooled double remained active");
    check(sample(faithful).count==0,"Pooled double repeatedly deactivated");
    faithful.cached=false; faithful.enabled=true;
    check(events(sample(faithful),AstralEventKind::activated)==1,"Reused double lost activation");
    check(registry.entries()[0].activation==2,"Reused double inherited old activation generation");

    auto stray=faithful;
    stray.actor={8,1}; stray.component={9,1}; stray.kind=AstralKind::stray_primary; stray.cached=true;
    changes=sample(stray);
    check(events(changes,AstralEventKind::removed)==1 && events(changes,AstralEventKind::activated)==1,
        "Live cached-and-enabled Stray primary was excluded");
    check(sample(stray).count==0,"Cache flag causes repeated Stray activation");
    stray.cached=false;
    check(sample(stray).count==0,"Cache flag alone restarted an enabled double");
    stray.hidden=true;
    check(events(sample(stray),AstralEventKind::deactivated)==1,"Hidden double remained active");
    stray.hidden=false;
    check(events(sample(stray),AstralEventKind::activated)==1,"Visible double did not reactivate");
    stray.native_mid={10,1};
    check(events(sample(stray),AstralEventKind::rebound)==1,"New native fade MID retained old material ownership");
    stray.mesh={11,1};
    check(events(sample(stray),AstralEventKind::rebound)==1,"New component retained old mesh ownership");
    stray.component={12,1};
    check(events(sample(stray),AstralEventKind::rebound)==1,"New Astral component retained old state");

    // Same object-array slot, different serial: a different actor, not pooled reuse.
    ++stray.actor.serial;
    changes=sample(stray);
    check(events(changes,AstralEventKind::removed)==1 && events(changes,AstralEventKind::discovered)==1 &&
        registry.entries()[0].activation==1,"Object slot reuse inherited previous actor state");
    stray.owner={20,1};
    check(events(sample(stray),AstralEventKind::removed)==1 && registry.entries().empty(),
        "Foreign owner's actor remained tracked");
    stray.owner=context.player;
    stray.initialized=false;
    check(events(sample(stray),AstralEventKind::activated)==0,"Uninitialized actor activated");
    stray.initialized=true; stray.native_mid={};
    check(events(sample(stray),AstralEventKind::activated)==0,"Missing native fade material activated");
    stray.native_mid={10,1};
    check(events(sample(stray),AstralEventKind::activated)==1,"Late initialization was missed");

    for(int part=0;part<3;++part) {
        sample(faithful);
        auto changed=context;
        if(part==0) ++changed.world.serial;
        if(part==1) ++changed.player.serial;
        if(part==2) ++changed.spawner.serial;
        auto next=faithful; next.owner=changed.player;
        changes=registry.observe(changed,{&next,1});
        check(events(changes,AstralEventKind::removed)==1 && registry.entries()[0].activation==1,
            "World/player/spawner change retained old lifecycle ownership");
        registry.clear();
    }
    sample(faithful);
    changes=registry.observe(context,{});
    check(events(changes,AstralEventKind::removed)==1 && registry.entries().empty(),"Absent actor not removed");
    sample(faithful);
    check(registry.clear().count==1 && registry.clear().count==0,"Explicit stop did not clean up exactly once");
    sample(faithful);
    check(!registry.observe({},{}).accepted && registry.entries().empty(),"Invalid context retained stale state");
    std::array duplicate{faithful,faithful};
    sample(faithful);
    check(!registry.observe(context,duplicate).accepted && registry.entries().empty(),"Duplicate snapshot partially committed");
    std::vector<AstralObservation> large(AstralLifecycle::capacity+1,faithful);
    sample(faithful);
    check(!registry.observe(context,large).accepted && registry.entries().empty(),"Oversized snapshot accepted");

    large.resize(AstralLifecycle::capacity);
    for(size_t i=0;i<large.size();++i) large[i].actor={int32_t(100+i),1};
    changes=registry.observe(context,large);
    check(changes.accepted && changes.count==AstralLifecycle::capacity*2,"Capacity-size snapshot failed");
    for(auto& row:large) ++row.actor.serial;
    changes=registry.observe(context,large);
    check(changes.count==AstralLifecycle::capacity*3 && registry.entries().size()==AstralLifecycle::capacity,
        "Full replacement lost removal or activation events");
    auto another=context; ++another.world.serial;
    changes=registry.observe(another,large);
    check(changes.count==AstralLifecycle::capacity*3,"Full world change lost events");
    std::cout<<"Astral lifecycle checks passed\n";
}

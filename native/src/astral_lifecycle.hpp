#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace css {
struct AstralIdentity {
    int32_t index=-1, serial=0;
    bool valid() const { return index>=0 && serial>0; }
    bool operator==(const AstralIdentity&) const = default;
};
enum class AstralKind { unknown, faithful, stray_primary, stray_secondary };
inline AstralKind astral_kind(std::string_view id) {
    if(id=="AstralCopy") return AstralKind::faithful;
    if(id=="AstralClonePrimary") return AstralKind::stray_primary;
    if(id=="AstralCloneSecondary") return AstralKind::stray_secondary;
    return AstralKind::unknown;
}
inline bool astral_shell(std::string_view shell) {
    return shell=="CharacterId.Player.Shell.Genessa" ||
           shell=="CharacterId.Player.Darkform.CorruptedGenessa";
}
struct AstralContext {
    AstralIdentity world, player, spawner;
    bool valid() const { return world.valid() && player.valid() && spawner.valid(); }
    bool operator==(const AstralContext&) const = default;
};
struct AstralObservation {
    AstralIdentity actor, owner, component, mesh, native_mid;
    AstralKind kind=AstralKind::unknown;
    bool initialized=false, enabled=false, hidden=false, cached=false;
    bool active() const {
        // Live Stray primaries can be cached while enabled and visibly fading.
        return initialized && enabled && !hidden && mesh.valid() && native_mid.valid();
    }
};
enum class AstralEventKind { discovered, activated, deactivated, rebound, removed };
struct AstralEvent { AstralIdentity actor; AstralEventKind kind; uint64_t activation; };
struct AstralEntry { AstralObservation observed; uint64_t activation=0; };

// Snapshots must be complete and taken on the game thread. Identities include
// the UE weak serial, so recycling an object-array slot cannot inherit state.
class AstralLifecycle {
public:
    static constexpr size_t capacity=32;
    struct Changes {
        bool accepted=true;
        std::array<AstralEvent,capacity*3> events{};
        size_t count=0;
        std::span<const AstralEvent> view() const { return {events.data(),count}; }
        void add(const AstralEntry& entry,AstralEventKind kind) {
            events[count++]={entry.observed.actor,kind,entry.activation};
        }
    };
private:
    AstralContext context_;
    std::array<AstralEntry,capacity> entries_{};
    size_t count_=0;
public:
    std::span<const AstralEntry> entries() const { return {entries_.data(),count_}; }
    bool deactivate(AstralIdentity actor) {
        // The fade pass can see pooling between two discovery snapshots.
        // Preserve that edge so immediate reuse starts a fresh activation.
        for(size_t i=0;i<count_;++i) if(entries_[i].observed.actor==actor) {
            const bool active=entries_[i].observed.active();
            entries_[i].observed.enabled=false;
            return active;
        }
        return false;
    }
    Changes clear() {
        Changes changes;
        for(const auto& entry:entries()) changes.add(entry,AstralEventKind::removed);
        entries_={}; count_=0; context_={};
        return changes;
    }
    Changes observe(AstralContext context,std::span<const AstralObservation> observed) {
        if(!context.valid() || observed.size()>capacity) {
            auto changes=clear(); changes.accepted=false; return changes;
        }
        for(size_t i=0;i<observed.size();++i) for(size_t j=0;j<i;++j)
            if(observed[i].actor.valid() && observed[i].actor==observed[j].actor) {
                auto changes=clear(); changes.accepted=false; return changes;
            }
        Changes changes;
        if(context_!=context) changes=clear();
        context_=context;
        std::array<AstralEntry,capacity> next{};
        size_t next_count=0;
        for(const auto& row:observed) {
            if(!row.actor.valid() || row.owner!=context.player || !row.component.valid() ||
               row.kind==AstralKind::unknown) continue;
            const AstralEntry* previous=nullptr;
            for(const auto& entry:entries()) if(entry.observed.actor==row.actor) { previous=&entry; break; }
            AstralEntry entry{row,previous?previous->activation:0};
            if(!previous) changes.add(entry,AstralEventKind::discovered);
            const bool was_active=previous && previous->observed.active();
            const bool rebound=previous && (previous->observed.component!=row.component ||
                previous->observed.mesh!=row.mesh || previous->observed.native_mid!=row.native_mid ||
                previous->observed.kind!=row.kind);
            if(row.active() && (!was_active || rebound)) {
                ++entry.activation;
                changes.add(entry,was_active?AstralEventKind::rebound:AstralEventKind::activated);
            } else if(was_active && !row.active()) changes.add(entry,AstralEventKind::deactivated);
            next[next_count++]=entry;
        }
        for(const auto& previous:entries()) {
            bool found=false;
            for(size_t i=0;i<next_count;++i) if(next[i].observed.actor==previous.observed.actor) { found=true; break; }
            if(!found) changes.add(previous,AstralEventKind::removed);
        }
        entries_=next; count_=next_count;
        return changes;
    }
};
}

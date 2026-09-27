#pragma once
// The combat engine: ten slots (L1 L2 L3 LF LC, H1 H2 H3 HF HC), each holding a move from the
// catalog or nothing (the weapon's own attack). Every player attack goes through the native
// montage task factory; a loader-owned pre-hook on it reads the owning ability, classifies its
// class name into a slot once (cached by name), and rewrites the montage and play rate in the
// rebuilt parameter frame before the native runs. No per-frame work: one callback per swing.
#include "engine.hpp"
#include "catalog.hpp"
#include "ccs_hook_api.h"
#include "ccs_types.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ccs {
class Combat {
public:
    struct Deps {
        const CcsHookHost* hooks{};
        const runtime::Catalog* catalog{};
        std::function<void(const std::string&)> log;
    };
    explicit Combat(Deps deps);
    Combat(const Combat&) = delete;
    Combat& operator=(const Combat&) = delete;
    // Configuration (game thread). Montages load lazily on later ticks, one per tick.
    void set_enabled(bool on);
    bool enabled() const { return enabled_; }
    void set_slot(SlotId slot, std::string move_id);   // empty id: the weapon's own attack
    // Per-slot tuning: speed multiplies the play rate; hit_damage "weapon" copies the slot's original
    // hit payload onto the replacement's hit-check notifies; weapon "move" shows the move's own weapon
    // mesh in hand while the montage plays. Applied on the next swing.
    void set_tuning(SlotId slot, const SlotTuning& tuning);
    const SlotTuning& tuning(SlotId slot) const { return slots_[size_t(slot)].tuning; }
    bool slot_weapon_available(SlotId slot) const { return slots_[size_t(slot)].show_mesh.alive(); }
    // Static mesh of a move source's weapon (player weapons and the enemy weapons with a static mesh), or empty.
    static std::string weapon_mesh_path(const std::string& source);
    const std::string& slot_move(SlotId slot) const { return slots_[size_t(slot)].move_id; }
    const std::string& slot_error(SlotId slot) const { return slots_[size_t(slot)].error; }
    bool slot_ready(SlotId slot) const { return slots_[size_t(slot)].montage.alive(); }
    uint64_t slot_hits(SlotId slot) const { return slots_[size_t(slot)].hits; }
    int assigned() const;
    void tick(const engine::PlayerContext& player, uint64_t now_ms);
    bool stop();                                        // remove the hook and release roots
    bool hooked() const { return token_ != 0; }
    nlohmann::json status() const;
    const std::string& error() const { return error_; }
    // Slot for a player attack ability class name, or -1: ..._A1 L1, _A2 L2, _A3 L3,
    // _A_Finisher / _A3_Finisher LF, _A<n>_Hold LC; B likewise for the heavy chain.
    static int classify(const std::string& class_name);
private:
    struct PayloadBackup { engine::ObjectHandle payload; std::vector<std::pair<engine::FProperty*, std::vector<std::byte>>> values; };
    // "Game" feel: a runtime clone of the slot's own montage (its notifies, sections and settings)
    // whose animation track holds the move's animation, keyed by the original it was cloned from.
    struct Transplant { engine::ObjectHandle original, clone; };
    struct Slot {
        std::string move_id, path, error; engine::ObjectHandle montage; bool rooted{}, pending{}; uint64_t hits{};
        engine::ObjectHandle play;                     // what "Move's own" plays: a cleaned clone for enemy montages, the montage itself otherwise
        bool play_hold{};                              // that montage carries a hold handler of its own
        SlotTuning tuning;
        std::vector<Transplant> feel; bool feel_warned{}; bool was_ready{};
        std::vector<Transplant> holds; bool hold_warned{};   // "Move's own" copies that carry the original's hold-attack window
        std::string show_mesh_path; engine::ObjectHandle show_mesh; bool show_rooted{};   // the move's weapon mesh, loaded with the montage
        engine::ObjectHandle payload_source; std::vector<PayloadBackup> backups;          // original payload copied onto the replacement
    };
    // The reflected layout of a montage's notify rows, resolved once: no property lookup per swing.
    struct NotifyLayout { engine::UObject* montage_class{}; engine::FProperty* notifies{}, *notify{}, *state{}, *link{}; engine::UObject* hit_state{}, *hit_notify{}; };
    mutable NotifyLayout layout_{};
    const NotifyLayout& notify_layout() const;
    // What the hook needs to know about a montage the game plays, learned the first time it is
    // seen: whether it is a companion clip (left alone) and whether it carries a hold handler.
    struct MontageFacts { engine::UObject* montage{}; bool companion{}, hold{}; };
    std::unordered_map<uint64_t, MontageFacts> montage_facts_;
    const MontageFacts& montage_facts(engine::UObject* montage);
    std::vector<engine::UObject*> hit_payloads(engine::UObject* montage) const;
    float first_hit_time(engine::UObject* montage) const;
    engine::UObject* transplant(Slot& slot, engine::UObject* original, engine::UObject* replacement);
    engine::UObject* build_transplant(engine::UObject* original, engine::UObject* replacement);
    engine::UObject* clone_montage(engine::UObject* source);
    std::vector<std::string> strip_ai_notifies(engine::UObject* clone);
    void release_transplants(Slot& slot);
    // The charge window: the game's hold-handler notify state on the slot's original montage. A
    // "Move's own" replacement gets a copy of the original's handler, timed to its own wind-up.
    int hold_handler_index(engine::UObject* montage) const;
    engine::UObject* carry_hold(Slot& slot, engine::UObject* original, engine::UObject* replacement);
    engine::UObject* build_hold_carry(engine::UObject* original, engine::UObject* replacement);
    void release_holds(Slot& slot);
    void note_recent(engine::UObject* cls, int slot, const char* what);   // the last few player attacks, for status.json
    void apply_weapon_payload(Slot& slot, engine::UObject* original, engine::UObject* replacement);
    void restore_payload(Slot& slot);
    void show_weapon(engine::UObject* mesh, engine::UObject* montage, uint64_t now);
    void restore_weapon();
    void poll_weapon(const engine::PlayerContext& player, uint64_t now);
    void note_skip(const char* why, engine::UObject* ability);   // one log line per ability class the hook rejects
public:
    void weapon_changed();                              // the equipped weapon changed: put any shown mesh back at once
private:
    static void callback(void*, void*, void*, void*) noexcept;
    void observe(void* frame);
    bool player_outer(engine::UObject* object) const;
    void ensure_hook();
    void remove_hook();
    void load_pending(const engine::PlayerContext& player);
    void release(Slot& slot);
    void log(const std::string& line) const { if (deps_.log) deps_.log(line); }
    Deps deps_;
    bool enabled_{}, active_{};
    engine::ObjectHandle shown_component_, shown_original_, shown_montage_, shown_actor_;   // weapon mesh swapped for the current swing
    std::vector<engine::ObjectHandle> shown_materials_;
    uint64_t shown_since_{}; bool shown_seen_playing_{};
    std::array<Slot, size_t(SlotId::Count)> slots_{};
    engine::ObjectHandle function_, pawn_, asc_, skeleton_, world_;   // world_: what holds our references
    std::array<engine::FProperty*, 3> inputs_{};      // OwningAbility, MontageToPlay, Rate
    uint64_t token_{}, retry_after_{}, player_check_{}, seen_{}, swapped_{}, skipped_{}, failures_{}, maximum_us_{}, wrong_frame_{};
    std::unordered_map<uint64_t, int8_t> class_slots_;
    std::unordered_set<uint64_t> noted_;
    std::deque<std::string> recent_;
    std::string pawn_rig_;                              // skeleton of the body worn now, refreshed with the pawn
    bool pawn_humanoid_{};
    std::string error_;
};
}

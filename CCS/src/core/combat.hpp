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
    // Hold attacks are an upgrade in this game (GE_Unlock_Attack_Hold_Light/Heavy grant the
    // Character.Unlocked.HoldAttack tags). Without the tag the game's own charge check fails at
    // once, so the hold slots wait; read from the pawn twice a second.
    bool hold_unlocked(bool heavy) const { return hold_unlocked_[heavy ? 1 : 0]; }
    // Cheat: apply the game's own unlock effects (GE_Unlock_Attack_Hold_Light/Heavy) to the player
    // so charged attacks work without the Tarstones. Removed again when switched off.
    void set_hold_cheat(bool on) { hold_cheat_ = on; }
    // Where a carried chain window ends: "hit" after the replacement's first hit (the game's own
    // chaining) or "move" after its last hit (the whole move plays before the next attack).
    void set_chain(const std::string& mode);
    bool hold_cheat() const { return hold_cheat_; }
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
        unsigned play_kinds{};                         // RowKind bits that montage carries of its own (hold handler, turn window, weapon state, elemental trigger)
        SlotTuning tuning;
        std::vector<Transplant> feel; bool feel_warned{}; bool was_ready{};
        std::vector<Transplant> carries; bool carry_warned{};   // "Move's own" copies that carry the original's hold and turn windows
        std::vector<Transplant> overlays; bool overlay_warned{};   // "Armor"/"Steer" copies of whatever the slot plays, keyed by that montage
        std::string show_mesh_path; engine::ObjectHandle show_mesh; bool show_rooted{};   // the move's weapon mesh, loaded with the montage
        engine::ObjectHandle payload_source; std::vector<PayloadBackup> backups;          // original payload copied onto the replacement
    };
    // The reflected layout of a montage's notify rows, resolved once: no property lookup per swing.
    struct NotifyLayout { engine::UObject* montage_class{}; engine::FProperty* notifies{}, *notify{}, *state{}, *link{}, *duration{}, *rate_scale{}; engine::UObject* hit_state{}, *hit_notify{}; };
    mutable NotifyLayout layout_{};
    const NotifyLayout& notify_layout() const;
    // What the hook needs to know about a montage the game plays, learned the first time it is
    // seen: whether it is a companion clip (left alone) and which RowKind rows it carries: a hold
    // handler, a turn window (the game's rotate-to-face-target notify state), a weapon-state notify
    // (the Axatana's transform between katanas and axe, equip-state changes) or an elemental
    // Tarstone trigger after a hit.
    struct MontageFacts { engine::UObject* montage{}; bool companion{}; unsigned kinds{}; };
    std::unordered_map<uint64_t, MontageFacts> montage_facts_;
    const MontageFacts& montage_facts(engine::UObject* montage);
    std::vector<engine::UObject*> hit_payloads(engine::UObject* montage) const;
    float first_hit_time(engine::UObject* montage) const;
    std::pair<float, float> first_hit_span(engine::UObject* montage) const;   // begin and end of the earliest hit window, or {-1,-1}
    engine::UObject* transplant(Slot& slot, engine::UObject* original, engine::UObject* replacement);
    engine::UObject* build_transplant(engine::UObject* original, engine::UObject* replacement);
    engine::UObject* clone_montage(engine::UObject* source);
    std::vector<std::string> strip_ai_notifies(engine::UObject* clone);
    void release_transplants(Slot& slot);
    // Windows the slot's original montage authors for the player and a "Move's own" replacement
    // may lack: the charge window (the game's hold-handler notify state) and the turn window (the
    // rotate-to-face-target notify state that lets the stick steer the wind-up). A replacement
    // without them plays through a copy that carries the original's rows, timed to its own clip.
    enum class RowKind { None, Hold, Turn, State, Mechanic, Queue };
    static constexpr unsigned bit(RowKind k) { return k == RowKind::None ? 0u : 1u << (unsigned(k) - 1); }
    static RowKind row_kind(const std::string& notify_class);
    unsigned row_kinds(engine::UObject* montage) const;        // bit(kind) for every kind the montage carries
    std::vector<float> hit_begins(engine::UObject* montage) const;   // sorted begin times of the hit windows
    engine::UObject* carry_windows(Slot& slot, engine::UObject* original, engine::UObject* replacement, unsigned kinds);
    engine::UObject* build_carry(engine::UObject* original, engine::UObject* replacement, unsigned kinds);
    void release_carries(Slot& slot);
    std::string append_rows(engine::UObject* clone, engine::UObject* source, const std::vector<int>& rows, const std::function<void(int, float&, float&)>& time);
    // The player-feel overlay: rows from a donor player montage appended over whatever the slot
    // plays. "Armor: Hyper armor" adds the game's ANS_HyperArmor over the whole swing; "Steer:
    // Whole move" adds the weapon's turn window up to the last hit and the movement cancel after it.
    bool ensure_donor();
    bool donor_ready() const { return donor_.get() && donor_armor_ >= 0 && donor_turn_ >= 0 && donor_cancel_ >= 0; }
    engine::UObject* build_overlay(engine::UObject* source, bool armor, bool steer);
    engine::UObject* overlaid(Slot& slot, engine::UObject* source, bool armor, bool steer);
    void release_overlays(Slot& slot);
    float last_hit_end(engine::UObject* montage) const;
    engine::ObjectHandle donor_; int donor_armor_{-1}, donor_turn_{-1}, donor_cancel_{-1}; bool donor_failed_{};
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
    bool hold_unlocked_[2]{}; bool hold_check_warned_{}, hold_check_logged_{};
    struct EffectHandle { int32_t handle{-1}; bool passed{}; uint8_t pad[3]{}; };   // FActiveGameplayEffectHandle, 8 bytes
    bool hold_cheat_{}, hold_cheat_warned_{};
    std::string chain_{"hit"};
    EffectHandle hold_grant_[2]{}; engine::ObjectHandle hold_grant_asc_, hold_effect_world_;
    void sync_hold_cheat(const engine::PlayerContext& player);
    bool pawn_humanoid_{};
    std::string error_;
};
}

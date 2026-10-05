#pragma once
// Clean scene: while it is on, enemies (or enemies and NPCs) around the player are taken out of
// the scene and new ones do not appear, without pausing the world. Nothing is destroyed and
// nothing reaches the save: every character is frozen with the game's own Pause(), hidden,
// made untargetable and unkillable, and every prior value is put back in reverse on restore.
//
// How the game spawns (read from its blueprints, docs/SCENE-RESEARCH.md): the player pawn
// overlapping a BP_SpawnTrigger / BP_Spawner_Proximity starts a spawn, so those actors get their
// collision switched off; spawners show a stand-in mesh before the real enemy exists, so
// spawners are hidden too. A 0.5 s poll treats whatever still turns up (NPC boxes, streaming).
//
// All work is spread over ticks (a fixed number of characters per tick) so entering never
// hitches; restore() is the one synchronous path, for leaving and for stop().
// Game thread only. Never throws out of tick().
#include <cssx/client.hpp>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

class Scene {
public:
    enum class Mode { Off, Enemies, Everyone };
    explicit Scene(const cssx::Client& host) : host_(host) {}

    // The wanted state. Takes effect over the next ticks; Off restores over ticks as well.
    void want(Mode mode, bool keep_target);
    Mode wanted() const { return wanted_; }
    // 10 Hz. `seconds` is real time. Handles come from the rig (same world as the caller).
    void tick(double seconds, const Json& pawn, const Json& controller);
    // Everything back, now. Every step runs even when an earlier one fails; returns failures.
    std::vector<std::string> restore();
    // The world changed: every actor handle is dead. Player effects are cleaned up on the
    // next tick against the new pawn.
    void forget();

    bool busy() const { return !queue_.empty() || restoring_; }
    // Needs a tick even outside Cine World: the player's protection is still to be removed.
    bool pending() const { return player_cleanup_ || applied_ != Mode::Off || wanted_ != Mode::Off; }
    bool clean() const { return applied_ != Mode::Off; }
    size_t hidden() const { return characters_.size(); }
    // One line for the menu: what is hidden, or why it is not on.
    const std::string& status() const { return status_; }

    static constexpr double poll_seconds = 0.5;
    static constexpr size_t characters_per_tick = 6;

private:
    // One character taken out, with what it looked like before.
    struct Character {
        Json actor, mesh, movement, brain, hit, vox, foley, asc;
        std::vector<std::pair<Json, bool>> visibility;   // the actor and its attached actors: prior bHidden
        bool corpse = false;                             // hidden only; a dead body has nothing to pause
        // Prior values, read before each change.
        bool was_collision = true, was_pause_anims = false, was_mesh_tick = true, was_move_tick = true;
        bool was_brain_running = false, was_hit = true, was_vox = true, was_foley_loop = false, was_cloth_suspended = false;
        // What was actually changed, so restore undoes exactly that.
        bool did_pause = false, did_collision = false, did_hit = false, did_vox = false, did_foley = false, did_cloth = false;
        Json invisible, immune;                          // active effect handles, null when not applied
    };
    struct Toggled { Json actor; bool prior = false; bool solid = false; };   // solid: a projectile's collision was on

    bool resolve(const Json& pawn, const Json& controller);
    std::string refusal();                           // empty when clean scene may run
    void protect_player();
    void release_player(std::vector<std::string>* failures);
    void poll();                                     // find newcomers, drop dead entries
    void take(const Json& actor);                    // one character out
    void release_lock(uint64_t id);                  // drop the player's lock-on when it is on this character
    void put_back(Character& c, std::vector<std::string>* failures);
    void close_spawning();
    void open_spawning(std::vector<std::string>* failures);
    bool exempt(uint64_t id) const { return keep_target_ && id == kept_; }
    std::vector<Json> actors_of(const Json& cls);
    Json call(const Json& target, const std::string& function, const Json& args = Json::object()) const;
    bool valid(const Json& object) const;
    void say(std::string text);

    const cssx::Client& host_;
    Mode wanted_ = Mode::Off, applied_ = Mode::Off;
    bool keep_target_ = false, restoring_ = false, resolved_ = false, player_cleanup_ = false;
    bool protect_tried_ = false;                     // one attempt per entry, not one per tick
    uint64_t kept_ = 0;                              // the exempt lock-on target's handle id
    double poll_in_ = 0, spawn_poll_in_ = 0;
    Json pawn_, controller_, statics_, player_asc_;
    Json class_ai_, class_trigger_, class_proximity_, class_spawner_, class_boss_, class_projectile_;
    Json ge_invisible_, ge_immune_, ge_invulnerable_, ge_fall_, ge_undetectable_;
    std::vector<Json> player_effects_;               // handles applied to the player
    std::map<uint64_t, Character> characters_;
    std::map<uint64_t, Toggled> triggers_, proxies_, projectiles_;
    std::deque<Json> queue_;                         // characters waiting to be taken out
    std::string status_;
};

} // namespace cine

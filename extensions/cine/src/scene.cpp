#include "scene.hpp"
#include <algorithm>
#include <stdexcept>

namespace cine {
namespace {
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
uint64_t id_of(const Json& j) { return is_handle(j) ? j["$object"].get<uint64_t>() : 0; }
bool truthy(const Json& j) { return j.is_boolean() && j.get<bool>(); }

constexpr const char* path_ai = "/Game/Sparta/Core/AI/BP_AICharacter.BP_AICharacter_C";
constexpr const char* path_trigger = "/Game/Sparta/Characters/SpawnSystem/BP_SpawnTrigger.BP_SpawnTrigger_C";
constexpr const char* path_proximity = "/Game/Sparta/Characters/SpawnSystem/BP_Spawner_Proximity.BP_Spawner_Proximity_C";
constexpr const char* path_spawner = "/Game/Sparta/Characters/SpawnSystem/BP_Spawner.BP_Spawner_C";
constexpr const char* path_boss = "/Game/Sparta/Core/AI/Components/BPC_BossAI.BPC_BossAI_C";
constexpr const char* path_projectile = "/Script/Sparta.SpartaProjectile";
constexpr const char* path_invisible = "/Game/Sparta/Core/Effects/GE_AI_Invisible.GE_AI_Invisible_C";
constexpr const char* path_immune = "/Game/Sparta/Core/Effects/GE_HealthDamageImmunity.GE_HealthDamageImmunity_C";
constexpr const char* path_invulnerable = "/Game/Sparta/Core/Effects/GE_Invulnerability.GE_Invulnerability_C";
constexpr const char* path_fall = "/Game/Sparta/Core/Effects/GE_FallDamageImmunity.GE_FallDamageImmunity_C";
constexpr const char* path_undetectable = "/Game/Sparta/Core/Effects/GE_Undetectable.GE_Undetectable_C";

constexpr size_t toggles_per_tick = 40;          // triggers and spawners: two calls each
constexpr int recheck_polls = 10;                // 5 s of watching for the spawner's own un-hide
constexpr double spawn_poll_seconds = 2.0;

// An applied effect's handle, or null: -1 is the engine's "nothing was applied".
Json effect_handle(const Json& reply) {
    return reply.is_object() && reply.value("Handle", -1) != -1 ? reply : Json();
}
}

Json Scene::call(const Json& target, const std::string& function, const Json& args) const { return host_.call(target, function, args); }
bool Scene::valid(const Json& object) const {
    try { return is_handle(object) && host_.request({{"op", "valid"}, {"target", object}}) == true; } catch (...) { return false; }
}
void Scene::say(std::string text) {
    if (text == status_) return;
    status_ = std::move(text);
    try { host_.log(status_); host_.invalidate(); } catch (...) {}
}

std::vector<Json> Scene::actors_of(const Json& cls) {
    std::vector<Json> out;
    if (!is_handle(cls)) return out;
    const auto reply = host_.request({{"op", "call"}, {"target", statics_}, {"function", "GetAllActorsOfClass"},
                                      {"args", {{"WorldContextObject", pawn_}, {"ActorClass", cls}}}});
    const auto list = reply.is_object() ? reply.value("OutActors", Json::array()) : Json::array();
    if (list.is_array()) for (const auto& a : list) if (is_handle(a)) out.push_back(a);
    return out;
}

void Scene::want(Mode mode, bool keep_target) {
    wanted_ = mode;
    keep_target_ = keep_target;
    if (!keep_target) kept_ = 0;
    poll_in_ = 0;   // act on the next tick, not half a second later
}

bool Scene::resolve(const Json& pawn, const Json& controller) {
    if (!is_handle(pawn) || !is_handle(controller)) return false;
    if (resolved_ && id_of(pawn) == id_of(pawn_)) return true;
    auto load = [&](const char* path) -> Json {
        try { auto o = host_.request({{"op", "load"}, {"path", path}}); return is_handle(o) ? o : Json(); } catch (...) { return Json(); }
    };
    pawn_ = pawn; controller_ = controller;
    statics_ = host_.find("/Script/Engine.Default__GameplayStatics");
    class_ai_ = load(path_ai);
    if (!is_handle(statics_) || !is_handle(class_ai_)) return false;   // a game update moved the base class: do nothing
    class_trigger_ = load(path_trigger); class_proximity_ = load(path_proximity); class_spawner_ = load(path_spawner);
    class_boss_ = load(path_boss); class_projectile_ = load(path_projectile);
    ge_invisible_ = load(path_invisible); ge_immune_ = load(path_immune);
    ge_invulnerable_ = load(path_invulnerable); ge_fall_ = load(path_fall); ge_undetectable_ = load(path_undetectable);
    try { player_asc_ = host_.get(pawn_, "AbilitySystemComponent"); } catch (...) { player_asc_ = nullptr; }
    resolved_ = true;
    return true;
}

// ---------------------------------------------------------------- the player

void Scene::protect_player() {
    if (!is_handle(player_asc_)) return;
    for (const Json* effect : {&ge_invulnerable_, &ge_fall_, &ge_undetectable_}) {
        if (!is_handle(*effect)) continue;
        try {
            auto handle = effect_handle(call(player_asc_, "BP_ApplyGameplayEffectToSelf",
                                             {{"GameplayEffectClass", *effect}, {"Level", 0.0}, {"EffectContext", Json::object()}}));
            if (!handle.is_null()) player_effects_.push_back(std::move(handle));
        } catch (...) {}   // she is still safe from paused, hit-disabled enemies
    }
}

void Scene::release_player(std::vector<std::string>* failures) {
    for (const auto& handle : player_effects_) {
        try { call(player_asc_, "RemoveActiveGameplayEffect", {{"Handle", handle}, {"StacksToRemove", -1}}); }
        catch (const std::exception& e) { if (failures) failures->push_back(std::string("player protection: ") + e.what()); }
    }
    player_effects_.clear();
}

// ---------------------------------------------------------------- spawning

void Scene::close_spawning() {
    size_t budget = toggles_per_tick;
    // Trigger volumes and proximity spawners: the player's overlap starts a spawn.
    for (const Json* cls : {&class_trigger_, &class_proximity_}) {
        for (const auto& actor : actors_of(*cls)) {
            const auto id = id_of(actor);
            if (triggers_.contains(id)) continue;
            if (budget == 0) { spawn_poll_in_ = 0; return; }   // the rest on the next tick
            --budget;
            try {
                const bool prior = truthy(call(actor, "GetActorEnableCollision"));
                if (prior) call(actor, "SetActorEnableCollision", {{"bNewActorEnableCollision", false}});
                triggers_[id] = {actor, prior};
            } catch (...) {}
        }
    }
    // Spawners show a stand-in mesh (and props) where an enemy will appear.
    for (const auto& actor : actors_of(class_spawner_)) {
        const auto id = id_of(actor);
        if (proxies_.contains(id)) continue;
        if (budget == 0) { spawn_poll_in_ = 0; return; }
        --budget;
        try {
            std::vector<Json> parts{actor};
            try { const auto addons = host_.get(actor, "UnitProxyAddons"); if (addons.is_array()) for (const auto& a : addons) if (is_handle(a)) parts.push_back(a); } catch (...) {}
            for (const auto& part : parts) {
                const bool prior = truthy(host_.get(part, "bHidden"));
                if (!prior) call(part, "SetActorHiddenInGame", {{"bNewHidden", true}});
                proxies_[id_of(part)] = {part, prior};
            }
        } catch (...) {}
    }
}

void Scene::open_spawning(std::vector<std::string>* failures) {
    auto each = [&](std::map<uint64_t, Toggled>& map, const char* what, auto&& undo) {
        for (auto& [id, t] : map) {
            if (!valid(t.actor)) continue;   // streamed out while hidden: nothing to put back
            try { undo(t); } catch (const std::exception& e) { if (failures) failures->push_back(std::string(what) + ": " + e.what()); }
        }
        map.clear();
    };
    each(proxies_, "spawner", [&](Toggled& t) { if (!t.prior) call(t.actor, "SetActorHiddenInGame", {{"bNewHidden", false}}); });
    each(projectiles_, "projectile", [&](Toggled& t) { if (!t.prior) call(t.actor, "SetActorHiddenInGame", {{"bNewHidden", false}}); });
    // Last: a trigger the player now stands in fires its overlap again. A spawner whose enemy
    // is alive refuses to spawn twice (the game's own CanSpawnEnemy).
    each(triggers_, "spawn trigger", [&](Toggled& t) { if (t.prior) call(t.actor, "SetActorEnableCollision", {{"bNewActorEnableCollision", true}}); });
}

// ---------------------------------------------------------------- one character

// The lock-on marker outlives a hidden enemy, so the lock is dropped before its target goes.
// TargetSpecificCharacter(null) is the game's own "no target"; the toggle is the fallback.
void Scene::release_lock(uint64_t id) {
    try {
        const auto lock = host_.get(pawn_, "LockOnComponent");
        if (!is_handle(lock)) return;
        auto locked = [&] { const auto t = host_.get(lock, "LockedTarget"); return is_handle(t) && id_of(t) == id; };
        if (!locked()) return;
        try { call(lock, "TargetSpecificCharacter", {{"Target", nullptr}}); } catch (...) {}
        if (locked()) call(lock, "ToggleTargetLock", {{"TargetingTag", {{"TagName", "None"}}}});
    } catch (...) {}
}

void Scene::take(const Json& actor) {
    const auto id = id_of(actor);
    if (!id || characters_.contains(id) || !valid(actor)) return;
    Character fresh;
    fresh.actor = actor;
    // Visibility first: the actor and what is attached to it (weapons are separate actors).
    std::vector<Json> parts{actor};
    try {
        const auto reply = host_.request({{"op", "call"}, {"target", actor}, {"function", "GetAttachedActors"},
                                          {"args", {{"bResetArray", true}, {"bRecursivelyIncludeAttachedActors", true}}}});
        const auto list = reply.is_object() ? reply.value("OutActors", Json::array()) : Json::array();
        if (list.is_array()) for (const auto& a : list) if (is_handle(a)) parts.push_back(a);
    } catch (...) {}
    for (const auto& part : parts) {
        bool prior = false;
        try { prior = truthy(host_.get(part, "bHidden")); } catch (...) { continue; }
        fresh.visibility.emplace_back(part, prior);
    }
    try { const auto health = host_.get(actor, "HealthComponent"); fresh.corpse = is_handle(health) && truthy(call(health, "IsDeadOrDying")); } catch (...) {}
    // Tracked from here: whatever fails below, restore still undoes what was done.
    auto& c = characters_[id] = std::move(fresh);
    release_lock(id);
    auto hide = [&] {
        for (auto& [part, prior] : c.visibility) if (!prior) { try { call(part, "SetActorHiddenInGame", {{"bNewHidden", true}}); } catch (...) {} }
    };
    if (c.corpse) { hide(); return; }

    auto component = [&](const char* name) -> Json { try { auto o = host_.get(actor, name); return is_handle(o) ? o : Json(); } catch (...) { return Json(); } };
    c.mesh = component("Mesh"); c.movement = component("CharacterMovement");
    c.hit = component("HitTraceComponent"); c.vox = component("VoxComponent"); c.foley = component("FoleyComponent");
    c.asc = component("AbilitySystemComponent");
    try { const auto controller = component("Controller"); if (is_handle(controller)) { auto b = host_.get(controller, "BrainComponent"); if (is_handle(b)) c.brain = b; } } catch (...) {}

    // 1. Freeze with the game's own Pause() (movement tick, animation, mesh tick, behaviour tree).
    try {
        if (is_handle(c.mesh)) { c.was_pause_anims = truthy(host_.get(c.mesh, "bPauseAnims")); c.was_mesh_tick = truthy(call(c.mesh, "IsComponentTickEnabled")); }
        if (is_handle(c.movement)) c.was_move_tick = truthy(call(c.movement, "IsComponentTickEnabled"));
        if (is_handle(c.brain)) c.was_brain_running = truthy(call(c.brain, "IsRunning"));
        call(actor, "Pause");
        c.did_pause = true;
    } catch (...) {}
    // 2. A swing frozen mid-air must not land.
    try {
        if (is_handle(c.hit)) { c.was_hit = truthy(call(c.hit, "IsHitProcessingEnabled")); if (c.was_hit) { call(c.hit, "DisableHitProcessing"); c.did_hit = true; } }
    } catch (...) {}
    // 3. Out of sight.
    hide();
    // 4. Out of the way. Only once frozen: with movement still ticking and no collision the
    //    floor check finds nothing and the character falls out of the world.
    try {
        c.was_collision = truthy(call(actor, "GetActorEnableCollision"));
        if (c.did_pause && c.was_collision) { call(actor, "SetActorEnableCollision", {{"bNewActorEnableCollision", false}}); c.did_collision = true; }
    } catch (...) {}
    // 5. Not lockable, and unable to die while hidden (a death is what writes to the save).
    if (is_handle(c.asc) && is_handle(player_asc_)) {
        auto apply = [&](const Json& effect) -> Json {
            if (!is_handle(effect)) return Json();
            try { return effect_handle(call(player_asc_, "BP_ApplyGameplayEffectToTarget",
                                            {{"GameplayEffectClass", effect}, {"Target", c.asc}, {"Level", 0.0}, {"Context", Json::object()}})); }
            catch (...) { return Json(); }
        };
        c.invisible = apply(ge_invisible_);
        c.immune = apply(ge_immune_);
    }
    // 6. Quiet.
    try {
        if (is_handle(c.vox)) { c.was_vox = truthy(host_.get(c.vox, "m_bVoxEnabled")); if (c.was_vox) { call(c.vox, "SetVocalizationsEnabled", {{"in_bEnabled", false}}); c.did_vox = true; } }
    } catch (...) {}
    try {
        if (is_handle(c.foley)) {
            c.was_foley_loop = truthy(call(c.foley, "IsPlayingFoleyLoop"));
            if (c.was_foley_loop) { call(c.foley, "StopFoleyLoop"); c.did_foley = true; }
            call(c.foley, "InterruptPlayingIdsOnComponents");
        }
    } catch (...) {}
    // 7. Cloth would keep simulating against a frozen body.
    try {
        if (is_handle(c.mesh)) { c.was_cloth_suspended = truthy(call(c.mesh, "IsClothingSimulationSuspended")); if (!c.was_cloth_suspended) { call(c.mesh, "SuspendClothingSimulation"); c.did_cloth = true; } }
    } catch (...) {}
}

void Scene::put_back(Character& c, std::vector<std::string>* failures) {
    if (!valid(c.actor)) return;   // destroyed while hidden (streamed out, its spawner ended)
    auto step = [&](const char* what, auto&& undo) {
        try { undo(); } catch (const std::exception& e) { if (failures) failures->push_back(std::string(what) + ": " + e.what()); }
    };
    // Reverse of take().
    if (c.did_cloth) step("cloth", [&] { call(c.mesh, "ResumeClothingSimulation"); });
    if (c.did_foley) step("foley", [&] { call(c.foley, "PlayFoleyLoop"); });
    if (c.did_vox) step("voice", [&] { call(c.vox, "SetVocalizationsEnabled", {{"in_bEnabled", true}}); });
    for (const Json* handle : {&c.immune, &c.invisible})
        if (!handle->is_null()) step("effect", [&] { call(c.asc, "RemoveActiveGameplayEffect", {{"Handle", *handle}, {"StacksToRemove", -1}}); });
    if (c.did_collision) step("collision", [&] { call(c.actor, "SetActorEnableCollision", {{"bNewActorEnableCollision", true}}); });
    for (auto& [part, prior] : c.visibility)
        if (!prior && valid(part)) step("visibility", [&] { call(part, "SetActorHiddenInGame", {{"bNewHidden", false}}); });
    if (c.did_hit) step("hits", [&] { call(c.hit, "EnableHitProcessing"); });
    if (c.did_pause) {
        // Not the game's Resume(): that restarts the behaviour tree on everything, including a
        // character whose logic was stopped on purpose (a death, a scripted NPC). Write the
        // three saved values back and start the tree only if it was running.
        if (is_handle(c.mesh)) step("animation", [&] { host_.set(c.mesh, "bPauseAnims", c.was_pause_anims); call(c.mesh, "SetComponentTickEnabled", {{"bEnabled", c.was_mesh_tick}}); });
        if (is_handle(c.movement)) step("movement", [&] { call(c.movement, "SetComponentTickEnabled", {{"bEnabled", c.was_move_tick}}); });
        if (c.was_brain_running && valid(c.brain)) step("behaviour", [&] { call(c.brain, "StartLogic"); });
    }
}

// ---------------------------------------------------------------- entry guard and poll

std::string Scene::refusal() {
    try {
        if (is_handle(player_asc_)) {
            const auto count = call(player_asc_, "GetGameplayTagCount", {{"GameplayTag", {{"TagName", "State.InCutscene"}}}});
            if (count.is_number() && count.get<int>() > 0) return "Clean scene waits: a cutscene is playing.";
        }
    } catch (...) {}
    if (!is_handle(class_boss_)) return {};
    for (const auto& actor : actors_of(class_ai_)) {
        try {
            const auto boss = call(actor, "GetComponentByClass", {{"ComponentClass", class_boss_}});
            if (is_handle(boss) && truthy(host_.get(boss, "bIsAggroed"))) return "Clean scene is off during a boss fight.";
        } catch (...) {}
    }
    return {};
}

void Scene::poll() {
    for (auto it = characters_.begin(); it != characters_.end();) it = valid(it->second.actor) ? std::next(it) : characters_.erase(it);
    // The exempt character: whoever the player is locked on to, while that option is on.
    if (keep_target_) {
        try {
            const auto lock = host_.get(pawn_, "LockOnComponent");
            const auto target = is_handle(lock) ? host_.get(lock, "LockedTarget") : Json();
            if (is_handle(target)) kept_ = id_of(target);
        } catch (...) {}
        if (auto it = characters_.find(kept_); it != characters_.end()) { put_back(it->second, nullptr); characters_.erase(it); }
    }
    std::vector<uint64_t> waiting;
    for (const auto& q : queue_) waiting.push_back(id_of(q));
    for (const auto& actor : actors_of(class_ai_)) {
        const auto id = id_of(actor);
        if (characters_.contains(id) || exempt(id) || std::find(waiting.begin(), waiting.end(), id) != waiting.end()) continue;
        // Classify once per pass: team, and never a boss (its fight logic, bar and music are its own).
        std::string team;
        try { const auto t = host_.get(actor, "Team"); if (t.is_object()) team = t.value("TagName", std::string()); } catch (...) { continue; }
        if (team.starts_with("Team.Player")) continue;
        const bool hostile = team.starts_with("Team.Hostile");
        if (!hostile && applied_ != Mode::Everyone) continue;
        if (is_handle(class_boss_)) {
            try { if (is_handle(call(actor, "GetComponentByClass", {{"ComponentClass", class_boss_}}))) continue; } catch (...) { continue; }
        }
        queue_.push_back(actor);
    }
    // A spawner un-hides its enemy at the end of the spawn; hide it again while that can happen.
    for (auto& [id, c] : characters_) {
        for (auto& [part, prior] : c.visibility) {
            try { if (!truthy(host_.get(part, "bHidden"))) { call(part, "SetActorHiddenInGame", {{"bNewHidden", true}}); prior = false; } } catch (...) {}
        }
    }
    say("Clean scene: " + std::to_string(characters_.size() + queue_.size()) + (applied_ == Mode::Everyone ? " characters" : " enemies") + " hidden"
        + (keep_target_ && kept_ ? ", your target kept." : "."));
}

// ---------------------------------------------------------------- lifecycle

void Scene::tick(double seconds, const Json& pawn, const Json& controller) {
    try {
        if (player_cleanup_ && is_handle(pawn)) {
            // The world changed under an active clean scene. If the pawn survived the trip her
            // protection effects did too; their handles are gone, so remove by effect class.
            player_cleanup_ = false;
            if (resolve(pawn, controller) && is_handle(player_asc_))
                for (const Json* effect : {&ge_invulnerable_, &ge_fall_, &ge_undetectable_})
                    if (is_handle(*effect)) { try { call(player_asc_, "RemoveActiveGameplayEffectBySourceEffect",
                        {{"GameplayEffect", *effect}, {"InstigatorAbilitySystemComponent", player_asc_}, {"StacksToRemove", -1}}); } catch (...) {} }
        }
        if (wanted_ == Mode::Off && applied_ == Mode::Off) return;   // nothing to do, no calls
        if (!resolve(pawn, controller)) { say("Clean scene is unavailable in this game version."); wanted_ = Mode::Off; return; }

        if (wanted_ == Mode::Off) {
            // Put characters back a few per tick, then spawning and the player.
            restoring_ = true;
            queue_.clear();
            size_t budget = characters_per_tick;
            while (budget-- && !characters_.empty()) { auto it = characters_.begin(); put_back(it->second, nullptr); characters_.erase(it); }
            if (characters_.empty()) {
                open_spawning(nullptr);
                release_player(nullptr);
                applied_ = Mode::Off; restoring_ = false; kept_ = 0;
                say("Scene restored.");
            }
            return;
        }
        if (applied_ == Mode::Off) {
            poll_in_ -= seconds;
            if (poll_in_ > 0) return;
            poll_in_ = 1.0;   // while refused, look again once a second
            if (auto reason = refusal(); !reason.empty()) { say(std::move(reason)); return; }
            protect_player();
            // Projectiles already in the air: once, they expire on their own.
            for (const auto& actor : actors_of(class_projectile_)) {
                try {
                    const bool prior = truthy(host_.get(actor, "bHidden"));
                    if (!prior) call(actor, "SetActorHiddenInGame", {{"bNewHidden", true}});
                    call(actor, "SetActorEnableCollision", {{"bNewActorEnableCollision", false}});
                    projectiles_[id_of(actor)] = {actor, prior};
                } catch (...) {}
            }
            applied_ = wanted_;
            poll_in_ = 0; spawn_poll_in_ = 0;
        } else if (applied_ != wanted_) {
            // Everyone -> Enemies: friendly characters come back; the other way the poll adds them.
            if (wanted_ == Mode::Enemies) {
                for (auto it = characters_.begin(); it != characters_.end();) {
                    std::string team;
                    try { const auto t = host_.get(it->second.actor, "Team"); if (t.is_object()) team = t.value("TagName", std::string()); } catch (...) {}
                    if (!team.starts_with("Team.Hostile")) { put_back(it->second, nullptr); it = characters_.erase(it); } else ++it;
                }
                queue_.clear();
            }
            applied_ = wanted_;
            poll_in_ = 0;
        }

        spawn_poll_in_ -= seconds;
        if (spawn_poll_in_ <= 0) { spawn_poll_in_ = spawn_poll_seconds; close_spawning(); }
        poll_in_ -= seconds;
        if (poll_in_ <= 0) { poll_in_ = poll_seconds; poll(); }
        size_t budget = characters_per_tick;
        while (budget-- && !queue_.empty()) { const Json actor = std::move(queue_.front()); queue_.pop_front(); if (!exempt(id_of(actor))) take(actor); }
    } catch (const std::exception& e) {
        say(std::string("Clean scene: ") + e.what());
    }
}

std::vector<std::string> Scene::restore() {
    std::vector<std::string> failures;
    queue_.clear();
    for (auto& [id, c] : characters_) put_back(c, &failures);
    characters_.clear();
    open_spawning(&failures);
    release_player(&failures);
    const bool was_clean = applied_ != Mode::Off;
    applied_ = Mode::Off; wanted_ = Mode::Off; restoring_ = false; kept_ = 0;
    if (was_clean) say(failures.empty() ? "Scene restored." : "Scene restored, with problems: " + failures.front());
    return failures;
}

void Scene::forget() {
    player_cleanup_ = player_cleanup_ || !player_effects_.empty();
    player_effects_.clear();
    characters_.clear(); triggers_.clear(); proxies_.clear(); projectiles_.clear(); queue_.clear();
    resolved_ = false; restoring_ = false; kept_ = 0;
    applied_ = Mode::Off; wanted_ = Mode::Off;
    pawn_ = controller_ = statics_ = player_asc_ = nullptr;
}

} // namespace cine

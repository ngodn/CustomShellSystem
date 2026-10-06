#pragma once
// The world audit: what the loaded level is made of and what in it costs time without
// showing. It walks every actor through reflection in small batches (a few hundred calls a
// tick, no hitch), counts classes, ticking actors, skeletal meshes animating while nobody
// sees them, shadow-casting lights and live particle systems, and writes a report. One fix
// comes out of it, reversible: skeletal meshes that animate fully while off screen are told
// to animate only what is seen, the engine's own default, except the player and anything
// another mod dresses.
#include <cssx/client.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace tuner {
using Json = nlohmann::json;
inline uint64_t id_of(const Json& j) { return j.is_object() && j.contains("$object") ? j["$object"].get<uint64_t>() : 0; }

struct AuditSummary {
    int actors = 0, ticking = 0, skeletal = 0, skeletal_always = 0, skeletal_always_unseen = 0, skeletal_no_uro = 0;
    int lights = 0, lights_shadow = 0, lights_shadow_movable = 0, niagara = 0, niagara_active = 0;
    std::map<std::string, int> classes, ticking_classes, always_classes;
    std::string when;
    std::vector<std::string> truncated;   // families the bridge could not list whole
    Json json() const;
    static AuditSummary parse(const Json& j);
};

class Audit {
public:
    explicit Audit(const cssx::Client& host) : host_(host) {}
    // Fetch the actors of the pawn's world and resolve the classes. Throws when the world has none.
    void start(const Json& pawn);
    bool running() const { return running_; }
    // Process up to `calls` reflected calls' worth of actors. Returns true when done.
    bool step(int calls);
    double progress() const { return actors_.empty() ? 0 : double(next_) / double(actors_.size()); }
    void cancel() { running_ = false; }
    const AuditSummary& summary() const { return summary_; }
    std::string report() const;

    // The fix: every skeletal mesh component that always animates, on an actor that is not
    // the player, is set to animate only when seen (montages still run). Returns how many
    // were changed; restore puts every one back. Needs a finished audit of this world.
    int apply_anim_fix(const Json& pawn);
    int restore_anim_fix();
    int anim_fix_count() const { return int(anim_fixed_.size()); }
    // Meshes the fix would change (always animating, not the player's), after an audit.
    int fixable() const { int n = 0; for (const auto& [part, _] : always_components_) if (id_of(part) != player_) ++n; return n; }

private:
    cssx::Client host_;
    Json statics_, class_skeletal_, class_light_, class_niagara_;
    std::vector<Json> actors_;
    size_t next_ = 0;
    bool running_ = false;
    AuditSummary summary_;
    // Components found animating always, for the fix: component handle and the option it had.
    std::vector<std::pair<Json, Json>> always_components_;
    std::vector<std::pair<Json, Json>> anim_fixed_;
    uint64_t player_ = 0;
    void examine(const Json& actor, int& calls);
    std::string class_name(const Json& object, int& calls);
};
}

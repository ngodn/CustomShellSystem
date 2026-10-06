#include "audit.hpp"
#include <algorithm>
#include <ctime>
#include <stdexcept>

namespace tuner {
namespace {
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
// EVisibilityBasedAnimTickOption: 0 AlwaysTickPoseAndRefreshBones, 1 AlwaysTickPose,
// 2 OnlyTickMontagesWhenNotRendered, 3 OnlyTickMontagesAndRefreshBonesWhenPlayingMontages,
// 4 OnlyTickPoseWhenRendered. The bridge may hand the enum back as a number or as its name.
int tick_option(const Json& v) {
    if (v.is_number()) return v.get<int>();
    if (v.is_string()) {
        const auto s = v.get<std::string>();
        if (s.find("AlwaysTickPoseAndRefreshBones") != std::string::npos) return 0;
        if (s.find("AlwaysTickPose") != std::string::npos) return 1;
        if (s.find("OnlyTickMontagesAndRefreshBones") != std::string::npos) return 3;
        if (s.find("OnlyTickMontagesWhenNotRendered") != std::string::npos) return 2;
        if (s.find("OnlyTickPoseWhenRendered") != std::string::npos) return 4;
    }
    return -1;
}
std::string short_class(std::string name) {
    // A handle's class is a full name: "Class /Script/Engine.StaticMeshActor" or
    // "BlueprintGeneratedClass /Game/Blueprints/BP_Grunt.BP_Grunt_C". Keep the last word.
    if (const auto space = name.rfind(' '); space != std::string::npos) name = name.substr(space + 1);
    if (const auto dot = name.rfind('.'); dot != std::string::npos) name = name.substr(dot + 1);
    if (const auto slash = name.rfind('/'); slash != std::string::npos) name = name.substr(slash + 1);
    return name;
}
Json counts(const std::map<std::string, int>& m) { Json j = Json::object(); for (const auto& [k, v] : m) j[k] = v; return j; }
std::map<std::string, int> uncounts(const Json& j) { std::map<std::string, int> m; if (j.is_object()) for (auto& [k, v] : j.items()) if (v.is_number()) m[k] = v.get<int>(); return m; }
std::vector<std::pair<std::string, int>> top(const std::map<std::string, int>& m, size_t n) {
    std::vector<std::pair<std::string, int>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.size() > n) v.resize(n);
    return v;
}
}

Json AuditSummary::json() const {
    return {{"actors", actors}, {"ticking", ticking}, {"skeletal", skeletal}, {"skeletal_always", skeletal_always}, {"skeletal_always_unseen", skeletal_always_unseen},
            {"skeletal_no_uro", skeletal_no_uro}, {"lights", lights}, {"lights_shadow", lights_shadow}, {"lights_shadow_movable", lights_shadow_movable},
            {"niagara", niagara}, {"niagara_active", niagara_active}, {"classes", counts(classes)}, {"ticking_classes", counts(ticking_classes)}, {"always_classes", counts(always_classes)}, {"when", when}, {"truncated", truncated}};
}
AuditSummary AuditSummary::parse(const Json& j) {
    AuditSummary s;
    if (!j.is_object()) return s;
    s.actors = j.value("actors", 0); s.ticking = j.value("ticking", 0); s.skeletal = j.value("skeletal", 0); s.skeletal_always = j.value("skeletal_always", 0);
    s.skeletal_always_unseen = j.value("skeletal_always_unseen", 0); s.skeletal_no_uro = j.value("skeletal_no_uro", 0); s.lights = j.value("lights", 0);
    s.lights_shadow = j.value("lights_shadow", 0); s.lights_shadow_movable = j.value("lights_shadow_movable", 0); s.niagara = j.value("niagara", 0); s.niagara_active = j.value("niagara_active", 0);
    s.classes = uncounts(j.value("classes", Json())); s.ticking_classes = uncounts(j.value("ticking_classes", Json())); s.always_classes = uncounts(j.value("always_classes", Json()));
    s.when = j.value("when", std::string());
    if (j.value("truncated", Json()).is_array()) for (const auto& t : j["truncated"]) if (t.is_string()) s.truncated.push_back(t.get<std::string>());
    return s;
}

void Audit::start(const Json& pawn) {
    auto load = [&](const char* path) -> Json { try { auto o = host_.request({{"op", "load"}, {"path", path}}); return is_handle(o) ? o : Json(); } catch (...) { return Json(); } };
    statics_ = host_.find("/Script/Engine.Default__GameplayStatics");
    class_skeletal_ = load("/Script/Engine.SkeletalMeshComponent");
    class_light_ = load("/Script/Engine.LightComponent");
    class_niagara_ = load("/Script/Niagara.NiagaraComponent");
    if (!is_handle(statics_) || !is_handle(class_skeletal_)) throw std::runtime_error("The engine's actor classes could not be found.");
    // A level has more actors than the bridge hands back in one list (4096), most of them
    // static meshes that cannot animate or tick anything. The audit asks for the families
    // that can cost time: characters and other pawns, animated props, lights, particle
    // actors. A family too big for one list is counted as "more than 4096".
    actors_.clear(); summary_ = {}; always_components_.clear();
    std::vector<uint64_t> seen;
    static const char* families[] = {"/Script/Engine.Character", "/Script/Engine.Pawn", "/Script/Engine.SkeletalMeshActor", "/Script/Engine.Light", "/Script/Niagara.NiagaraActor"};
    for (const char* path : families) {
        const auto cls = load(path);
        if (!is_handle(cls)) continue;
        try {
            const auto reply = host_.request({{"op", "call"}, {"target", statics_}, {"function", "GetAllActorsOfClass"}, {"args", {{"WorldContextObject", pawn}, {"ActorClass", cls}}}});
            const auto list = reply.is_object() ? reply.value("OutActors", Json::array()) : Json::array();
            if (list.is_array()) for (const auto& a : list) {
                if (!is_handle(a) || std::find(seen.begin(), seen.end(), id_of(a)) != seen.end()) continue;
                seen.push_back(id_of(a)); actors_.push_back(a);
            }
        } catch (const std::exception& e) {
            summary_.truncated.push_back(short_class(path) + ": " + e.what());
        }
    }
    if (actors_.empty()) throw std::runtime_error("The world has no actors of the kinds the audit looks at.");
    player_ = id_of(pawn);
    next_ = 0; running_ = true;
}

std::string Audit::class_name(const Json& object, int&) {
    return object.is_object() && object.value("class", Json()).is_string() ? short_class(object["class"].get<std::string>()) : "?";
}

void Audit::examine(const Json& actor, int& calls) {
    const auto cls = class_name(actor, calls);
    ++summary_.actors;
    ++summary_.classes[cls];
    try { ++calls; if (host_.call(actor, "IsActorTickEnabled") == true) { ++summary_.ticking; ++summary_.ticking_classes[cls]; } } catch (...) {}
    const bool is_player = id_of(actor) == player_;
    try {
        ++calls;
        const auto parts = host_.call(actor, "K2_GetComponentsByClass", {{"ComponentClass", class_skeletal_}});
        if (parts.is_array()) for (const auto& part : parts) {
            if (!is_handle(part)) continue;
            ++summary_.skeletal;
            Json option; bool uro = true, seen = true;
            try { ++calls; option = host_.get(part, "VisibilityBasedAnimTickOption"); } catch (...) {}
            try { ++calls; uro = host_.get(part, "bEnableUpdateRateOptimizations") == true; } catch (...) {}
            try { ++calls; seen = host_.call(part, "WasRecentlyRendered", {{"Tolerance", 0.2}}) == true; } catch (...) {}
            if (!uro) ++summary_.skeletal_no_uro;
            const int opt = tick_option(option);
            if (opt == 0 || opt == 1) {
                ++summary_.skeletal_always; ++summary_.always_classes[cls];
                if (!seen) ++summary_.skeletal_always_unseen;
                if (!is_player) always_components_.emplace_back(part, option);
            }
        }
    } catch (...) {}
    if (is_handle(class_light_)) try {
        ++calls;
        const auto parts = host_.call(actor, "K2_GetComponentsByClass", {{"ComponentClass", class_light_}});
        if (parts.is_array()) for (const auto& part : parts) {
            if (!is_handle(part)) continue;
            ++summary_.lights;
            bool shadows = false; Json mobility;
            try { ++calls; shadows = host_.get(part, "CastShadows") == true; } catch (...) {}
            if (shadows) {
                ++summary_.lights_shadow;
                try { ++calls; mobility = host_.get(part, "Mobility"); } catch (...) {}
                const bool movable = mobility.is_number() ? mobility.get<int>() == 2 : mobility.is_string() && mobility.get<std::string>().find("Movable") != std::string::npos;
                if (movable) ++summary_.lights_shadow_movable;
            }
        }
    } catch (...) {}
    if (is_handle(class_niagara_)) try {
        ++calls;
        const auto parts = host_.call(actor, "K2_GetComponentsByClass", {{"ComponentClass", class_niagara_}});
        if (parts.is_array()) for (const auto& part : parts) {
            if (!is_handle(part)) continue;
            ++summary_.niagara;
            try { ++calls; if (host_.call(part, "IsActive") == true) ++summary_.niagara_active; } catch (...) {}
        }
    } catch (...) {}
}

bool Audit::step(int calls) {
    if (!running_) return true;
    int used = 0;
    const auto started = host_.now_us();
    while (next_ < actors_.size() && used < calls) {
        examine(actors_[next_++], used);
        const auto now = host_.now_us();
        if (started && now >= started && now - started >= 500) break;
    }
    if (next_ < actors_.size()) return false;
    running_ = false;
    char text[32] = ""; const std::time_t t = std::time(nullptr); std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::strftime(text, sizeof text, "%d %b %H:%M", &tm);
    summary_.when = text[0] == '0' ? text + 1 : text;
    return true;
}

std::string Audit::report() const {
    const auto& s = summary_;
    std::string r = "Perf Tuner: world audit, " + s.when + "\n\n";
    r += "Looked at the actors that can cost time: characters and other pawns, animated props, lights and particle actors (static scenery is not listed).\n";
    for (const auto& t : s.truncated) r += "  A family was too big for one list and was skipped: " + t + "\n";
    r += std::to_string(s.actors) + " actors, " + std::to_string(s.ticking) + " ticking every frame.\n";
    r += std::to_string(s.skeletal) + " skeletal meshes: " + std::to_string(s.skeletal_always) + " animate fully whether seen or not (" + std::to_string(s.skeletal_always_unseen) + " of them were not on screen at the time), "
       + std::to_string(s.skeletal_no_uro) + " without update-rate optimization.\n";
    r += std::to_string(s.lights) + " lights, " + std::to_string(s.lights_shadow) + " casting shadows, " + std::to_string(s.lights_shadow_movable) + " of those movable (shadow maps redrawn as they move).\n";
    r += std::to_string(s.niagara) + " particle systems, " + std::to_string(s.niagara_active) + " active.\n\n";
    r += "Most common actor classes:\n"; for (const auto& [k, v] : top(s.classes, 25)) r += "  " + std::to_string(v) + "  " + k + "\n";
    r += "\nTicking actors by class:\n"; for (const auto& [k, v] : top(s.ticking_classes, 25)) r += "  " + std::to_string(v) + "  " + k + "\n";
    r += "\nAlways-animating skeletal meshes by actor class:\n"; for (const auto& [k, v] : top(s.always_classes, 25)) r += "  " + std::to_string(v) + "  " + k + "\n";
    r += "\nWhat this means: an actor that ticks runs its Blueprint every frame whether you see it or not; a skeletal mesh that always animates evaluates its whole animation graph and refreshes its bones off screen too. "
         "The engine's default for a mesh is to animate only when seen and keep montages (attacks, reactions) running unseen. The 'Animate only what is seen' fix sets that default on every mesh here except yours; it is reversible and measurable.\n";
    return r;
}

int Audit::apply_anim_fix(const Json& pawn) {
    if (running_ || !anim_fixed_.empty()) return int(anim_fixed_.size());
    const auto player = id_of(pawn);
    int n = 0;
    for (const auto& [part, before] : always_components_) {
        if (id_of(part) == player) continue;
        try {
            if (host_.request({{"op", "valid"}, {"target", part}}) != true) continue;
            // 2: OnlyTickMontagesWhenNotRendered; ordinary unseen poses stop updating.
            anim_fixed_.emplace_back(part, before);
            host_.set(part, "VisibilityBasedAnimTickOption", 2);
            if (tick_option(host_.get(part, "VisibilityBasedAnimTickOption")) != 2) continue;
            ++n;
        } catch (...) {}
    }
    return n;
}

int Audit::restore_anim_fix() {
    int n = 0;
    std::vector<std::pair<Json, Json>> pending;
    for (const auto& [part, before] : anim_fixed_) {
        try {
            if (host_.request({{"op", "valid"}, {"target", part}}) != true) continue;
            host_.set(part, "VisibilityBasedAnimTickOption", before);
            if (tick_option(host_.get(part, "VisibilityBasedAnimTickOption")) != tick_option(before)) throw std::runtime_error("Animation tick option was not restored");
            ++n;
        } catch (...) { pending.emplace_back(part, before); }
    }
    anim_fixed_ = std::move(pending);
    return n;
}
}

#include "preset.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cine {
namespace {
double number(const Json& j, const char* key, double fallback, double lo, double hi) {
    if (!j.contains(key)) return fallback;
    const auto& v = j.at(key);
    if (!v.is_number()) throw std::invalid_argument(std::string("'") + key + "' must be a number");
    const double x = v.get<double>();
    if (!std::isfinite(x) || x < lo || x > hi)
        throw std::invalid_argument(std::string("'") + key + "' must be between " + std::to_string(lo) + " and " + std::to_string(hi));
    return x;
}
std::string text(const Json& j, const char* key, size_t limit, bool required) {
    if (!j.contains(key)) { if (required) throw std::invalid_argument(std::string("'") + key + "' is required"); return {}; }
    if (!j.at(key).is_string()) throw std::invalid_argument(std::string("'") + key + "' must be text");
    auto s = j.at(key).get<std::string>();
    if (s.size() > limit) throw std::invalid_argument(std::string("'") + key + "' is too long");
    return s;
}
// Camera geometry limits: a camera inside her or across the map is a typo, not a shot.
void camera_fields(const Json& j, double& az, double& dist, double& height, double& aim, double& fov) {
    az = number(j, "azimuth", 0, -720, 720);
    dist = number(j, "distance", 300, 30, 3000);
    height = number(j, "height", 0, -500, 500);
    aim = number(j, "aim", 0, -300, 300);
    fov = number(j, "fov", 40, 5, 120);
}
}

const char* kind_name(ShotKind k) {
    switch (k) { case ShotKind::Walk: return "walk"; case ShotKind::Glide: return "glide"; case ShotKind::Turntable: return "turntable"; default: return "photo"; }
}

Preset Preset::parse(const Json& j) {
    if (!j.is_object()) throw std::invalid_argument("A preset must be a JSON object");
    if (j.value("schema", 0) != 1) throw std::invalid_argument("'schema' must be 1");
    Preset p;
    p.id = text(j, "id", 64, true);
    if (p.id.empty() || !std::all_of(p.id.begin(), p.id.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '-' || c == '_'; }))
        throw std::invalid_argument("'id' must be lowercase letters, digits, '-' or '_'");
    p.name = text(j, "name", 64, true);
    p.description = text(j, "description", 512, false);
    const auto kind = text(j, "kind", 16, true);
    if (kind == "walk") p.kind = ShotKind::Walk;
    else if (kind == "glide") p.kind = ShotKind::Glide;
    else if (kind == "turntable") p.kind = ShotKind::Turntable;
    else if (kind == "photo") p.kind = ShotKind::Photo;
    else throw std::invalid_argument("'kind' must be walk, glide, turntable or photo");
    p.intro = number(j, "intro", p.intro, 0.5, 20);
    p.duration = number(j, "duration", p.duration, 1, 120);
    p.tail = number(j, "tail", p.tail, 3, 60);
    p.max_seconds = number(j, "max_seconds", p.max_seconds, 5, 300);
    p.lead = number(j, "lead", p.lead, 0.2, 2);
    p.zoom = number(j, "zoom", p.zoom, 0.3, 3);
    p.fov_scale = number(j, "fov_scale", p.fov_scale, 0.3, 3);
    p.hold = number(j, "hold", p.hold, 0.5, 30);
    p.reference = p.kind == ShotKind::Walk ? Preset::Reference::Capsule : Preset::Reference::Head;
    if (j.contains("reference")) {
        const auto r = text(j, "reference", 16, true);
        if (r == "capsule") p.reference = Preset::Reference::Capsule;
        else if (r == "head") p.reference = Preset::Reference::Head;
        else throw std::invalid_argument("'reference' must be capsule or head");
    }

    if (p.kind == ShotKind::Photo) {
        if (!j.contains("photos") || !j.at("photos").is_array() || j.at("photos").empty() || j.at("photos").size() > 32)
            throw std::invalid_argument("'photos' must list 1 to 32 shots");
        for (const auto& s : j.at("photos")) {
            if (!s.is_object()) throw std::invalid_argument("Each photo must be an object");
            PhotoShot shot; shot.name = text(s, "name", 64, true);
            camera_fields(s, shot.azimuth, shot.distance, shot.height, shot.aim, shot.fov);
            p.photos.push_back(std::move(shot));
        }
    } else {
        if (!j.contains("keys") || !j.at("keys").is_array() || j.at("keys").size() < 2 || j.at("keys").size() > 32)
            throw std::invalid_argument("'keys' must list 2 to 32 camera keys");
        for (const auto& k : j.at("keys")) {
            if (!k.is_object() || !k.contains("at")) throw std::invalid_argument("Each key needs 'at'");
            PresetKey key;
            const auto& at = k.at("at");
            if (at.is_string()) {
                key.anchor = at.get<std::string>();
                if (p.kind != ShotKind::Walk || (key.anchor != "start" && key.anchor != "walk"))
                    throw std::invalid_argument("'at' text is only 'start' or 'walk', in a walk preset");
            } else if (at.is_number()) {
                key.fraction = at.get<double>();
                if (!std::isfinite(key.fraction) || key.fraction < 0 || key.fraction > 1) throw std::invalid_argument("'at' must be between 0 and 1");
            } else throw std::invalid_argument("'at' must be a number or 'start'/'walk'");
            camera_fields(k, key.azimuth, key.distance, key.height, key.aim, key.fov);
            p.keys.push_back(std::move(key));
        }
    }
    if (j.contains("look")) {
        const auto& look = j.at("look");
        if (!look.is_object() || !look.contains("steps") || !look.at("steps").is_array() || look.at("steps").size() > 32)
            throw std::invalid_argument("'look.steps' must list up to 32 steps");
        p.look_ends_on_own = look.value("end_on_own", true);
        double previous = -1;
        for (const auto& s : look.at("steps")) {
            if (!s.is_object()) throw std::invalid_argument("Each look step must be an object");
            LookStep step;
            step.fraction = number(s, "at", 0, 0, 1);
            if (step.fraction <= previous) throw std::invalid_argument("Look steps must be in time order");
            previous = step.fraction;
            step.palette = text(s, "palette", 64, false);
            if (step.palette == "original") throw std::invalid_argument("'original' would clear the modder's values mid-shot; use end_on_own");
            if (s.contains("controls")) {
                if (!s.at("controls").is_object() || s.at("controls").size() > 64) throw std::invalid_argument("'controls' must map up to 64 parts");
                for (const auto& [id, value] : s.at("controls").items()) {
                    if (id.empty() || id.size() > 64) throw std::invalid_argument("Invalid control id in a look step");
                    std::array<double, 3> v{0, 0, 0};
                    if (value.is_number()) v[0] = value.get<double>();
                    else if (value.is_array() && !value.empty() && value.size() <= 4) {
                        for (size_t i = 0; i < 3 && i < value.size(); ++i) {
                            if (!value[i].is_number()) throw std::invalid_argument("Control values must be numbers");
                            v[i] = value[i].get<double>();
                        }
                    } else throw std::invalid_argument("A control value must be a number or 1-4 numbers");
                    for (double x : v) if (!std::isfinite(x)) throw std::invalid_argument("Control values must be finite");
                    step.controls[id] = v;
                }
            }
            if (step.palette.empty() && step.controls.empty()) throw std::invalid_argument("A look step needs a palette or controls");
            p.look.push_back(std::move(step));
        }
    }
    return p;
}

Timeline make_timeline(const Preset& p, double route_length_cm, double walk_speed_cm_s) {
    Timeline tl;
    auto key = [&](const PresetKey& k, double t) {
        return OrbitKey{t, k.azimuth, k.distance * p.zoom, k.height, k.aim, k.fov * p.fov_scale};
    };
    if (p.kind == ShotKind::Photo) {
        tl.total = double(p.photos.size()) * p.hold;
        return tl;
    }
    if (p.kind == ShotKind::Walk) {
        if (route_length_cm < 300) throw std::invalid_argument("Record a route of at least 3 m for a walk shot");
        if (walk_speed_cm_s <= 0) throw std::invalid_argument("Invalid walking speed");
        tl.walk_start = p.intro;
        tl.walk_time = route_length_cm / walk_speed_cm_s + 0.8;   // +0.8 s easing in and out
        tl.arrive = tl.walk_start + tl.walk_time + 0.6;
        const double tail = std::min(p.tail, p.max_seconds - tl.arrive);
        if (tail < 3) throw std::invalid_argument("This route is too long for the preset's " + std::to_string(int(p.max_seconds)) +
                                                  " s cap; record a shorter walk or raise max_seconds");
        tl.total = tl.arrive + tail;
        double previous = -1;
        for (const auto& k : p.keys) {
            double t = k.anchor == "start" ? 0 : k.anchor == "walk" ? tl.walk_start : tl.walk_start + 0.2 + k.fraction * tl.walk_time;
            t = std::max(t, previous + 0.05);   // keep strict time order after rounding
            tl.keys.push_back(key(k, t));
            previous = t;
        }
        for (const auto& s : p.look) tl.look.push_back({tl.walk_start + s.fraction * tl.walk_time, &s, false});
        if (p.look_ends_on_own && !p.look.empty()) tl.look.push_back({tl.arrive - 2.0, nullptr, true});
    } else {
        tl.total = p.duration;
        double previous = -1;
        for (const auto& k : p.keys) {
            const double t = std::max(k.fraction * p.duration, previous + 0.05);
            tl.keys.push_back(key(k, t));
            previous = t;
        }
        for (const auto& s : p.look) tl.look.push_back({s.fraction * p.duration, &s, false});
        if (p.look_ends_on_own && !p.look.empty()) tl.look.push_back({p.duration * 0.92, nullptr, true});
    }
    std::sort(tl.look.begin(), tl.look.end(), [](const auto& a, const auto& b) { return a.at < b.at; });
    return tl;
}

} // namespace cine

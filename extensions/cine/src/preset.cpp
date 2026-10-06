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
    switch (k) { case ShotKind::Walk: return "walk"; case ShotKind::Glide: return "glide"; case ShotKind::Turntable: return "turntable"; case ShotKind::Action: return "action"; case ShotKind::Sequence: return "sequence"; default: return "photo"; }
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
    else if (kind == "action") p.kind = ShotKind::Action;
    else if (kind == "sequence") p.kind = ShotKind::Sequence;
    else throw std::invalid_argument("'kind' must be walk, glide, turntable, photo, action or sequence");
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
    } else if (p.kind == ShotKind::Sequence) {
        p.sequence = Sequence::parse(j);
    } else if (p.kind == ShotKind::Action) {
        const Json a = j.value("action", Json::object());
        if (!a.is_object()) throw std::invalid_argument("'action' must be an object");
        auto& shot = p.action;
        const auto framing = a.contains("framing") ? text(a, "framing", 16, true) : std::string("duel");
        if (framing == "follow") shot.framing = Framing::Follow;
        else if (framing == "duel") shot.framing = Framing::Duel;
        else if (framing == "shoulder") shot.framing = Framing::Shoulder;
        else throw std::invalid_argument("'framing' must be follow, duel or shoulder");
        shot.azimuth = number(a, "azimuth", shot.azimuth, -360, 360);
        shot.orbit_speed = number(a, "orbit_speed", shot.orbit_speed, -90, 90);
        shot.distance = number(a, "distance", shot.distance, 80, 3000);
        shot.distance_max = number(a, "distance_max", std::max(shot.distance_max, shot.distance), shot.distance, 5000);
        shot.margin = number(a, "margin", shot.margin, 0, 1000);
        shot.height = number(a, "height", shot.height, -300, 800);
        shot.aim = number(a, "aim", shot.aim, -300, 300);
        shot.fov = number(a, "fov", shot.fov, 5, 120);
        shot.smooth = number(a, "smooth", shot.smooth, 0, 3);
        shot.shake = number(a, "shake", shot.shake, -1, 2);
        shot.fstop = number(a, "fstop", shot.fstop, 0, 32);
        shot.slowmo = number(a, "slowmo", shot.slowmo, 0.05, 1);
        shot.slowmo_hold = number(a, "slowmo_hold", shot.slowmo_hold, 0.1, 5);
        shot.tail = number(a, "tail", shot.tail, 0, 30);
        if (a.contains("game_cameras")) {
            const auto mode = text(a, "game_cameras", 16, true);
            if (mode == "respect") shot.respect_game_cameras = true;
            else if (mode == "override") shot.respect_game_cameras = false;
            else throw std::invalid_argument("'game_cameras' must be respect or override");
        }
        if (a.contains("beats")) {
            if (!a.at("beats").is_array() || a.at("beats").size() > 8) throw std::invalid_argument("'beats' must list up to 8 beats");
            for (const auto& b : a.at("beats")) {
                if (!b.is_object()) throw std::invalid_argument("Each beat must be an object");
                Beat beat;
                const auto on = text(b, "on", 16, true), what = text(b, "do", 16, true);
                if (on == "hit") beat.on = Beat::On::Hit; else if (on == "hurt") beat.on = Beat::On::Hurt;
                else if (on == "parry") beat.on = Beat::On::Parry; else if (on == "kill") beat.on = Beat::On::Kill;
                else throw std::invalid_argument("A beat's 'on' must be hit, hurt, parry or kill");
                if (what == "slowmo") beat.what = Beat::Do::Slowmo; else if (what == "punch") beat.what = Beat::Do::Punch;
                else throw std::invalid_argument("A beat's 'do' must be slowmo or punch");
                beat.cooldown = number(b, "cooldown", beat.cooldown, 0.2, 60);
                shot.beats.push_back(beat);
            }
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
            throw std::invalid_argument("'look.steps' must list up to 96 steps");
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
            if (s.contains("title")) { step.title = text(s, "title", 80, false); step.titled = true; }
            if (s.contains("controls")) {
                if (!s.at("controls").is_object() || s.at("controls").size() > 64) throw std::invalid_argument("'controls' must map up to 64 parts");
                for (const auto& [id, value] : s.at("controls").items()) {
                    if (id.empty() || id.size() > 64) throw std::invalid_argument("Invalid control id in a look step");
                    std::vector<double> v;
                    if (value.is_number()) v.push_back(value.get<double>());
                    else if (value.is_array() && !value.empty() && value.size() <= 4) {
                        for (size_t i = 0; i < 3 && i < value.size(); ++i) {
                            if (!value[i].is_number()) throw std::invalid_argument("Control values must be numbers");
                            v.push_back(value[i].get<double>());
                        }
                    } else throw std::invalid_argument("A control value must be a number or 1-4 numbers");
                    for (double x : v) if (!std::isfinite(x)) throw std::invalid_argument("Control values must be finite");
                    step.controls[id] = v;
                }
            }
            if (s.contains("physics")) {
                if (!s.at("physics").is_object() || s.at("physics").size() > 16) throw std::invalid_argument("'physics' must map up to 16 motion controls to a preset");
                for (const auto& [id, value] : s.at("physics").items()) {
                    if (!value.is_string() || value.get<std::string>().empty() || value.get<std::string>().size() > 48) throw std::invalid_argument("A physics preset is named by its id");
                    step.physics[id] = value.get<std::string>();
                }
            }
            if (step.palette.empty() && step.controls.empty() && !step.titled && step.physics.empty()) throw std::invalid_argument("A look step needs a palette, controls, physics or a title");
            p.look.push_back(std::move(step));
        }
    }
    if (j.contains("idle")) p.idle = text(j, "idle", 96, true);
    if (j.contains("frame")) {
        p.frame = text(j, "frame", 16, true);
        if (p.frame != "16:9" && p.frame != "9:16" && p.frame != "1:1" && p.frame != "2.39:1") throw std::invalid_argument("'frame' must be 16:9, 9:16, 1:1 or 2.39:1");
    }
    if (j.contains("audio")) {
        // A file inside the extension folder, never a path out of it.
        const auto file = text(j, "audio", 160, true);
        const bool wav = file.ends_with(".wav"), mp3 = file.ends_with(".mp3");
        if ((!wav && !mp3) || file.find("..") != std::string::npos || file.find(':') != std::string::npos || file.front() == '/' || file.front() == '\\')
            throw std::invalid_argument("'audio' must be a .wav or .mp3 file inside the CINE folder");
        p.audio = file;
    }
    if (j.contains("breakdown")) {
        const auto& breakdown = j.at("breakdown");
        if (!breakdown.is_object() || !breakdown.contains("steps") || !breakdown.at("steps").is_array() || breakdown.at("steps").size() > 96)
            throw std::invalid_argument("'breakdown.steps' must list up to 96 steps");
        double previous = -1;
        for (const auto& s : breakdown.at("steps")) {
            if (!s.is_object() || !s.contains("layers")) throw std::invalid_argument("Each breakdown step needs 'layers'");
            BreakdownStep step;
            // A sequence counts in bars like its shots do; every other kind in fractions of the take.
            if (p.kind == ShotKind::Sequence) {
                const double at = p.sequence.tempo.seconds(number(s, "at", 1, 0, 2000));
                if (at < 0 || at >= p.sequence.total) throw std::invalid_argument("A breakdown step is outside the sequence");
                step.fraction = at / p.sequence.total;
            } else step.fraction = number(s, "at", 0, 0, 1);
            if (step.fraction <= previous) throw std::invalid_argument("Breakdown steps must be in time order");
            previous = step.fraction;
            step.layers = layers_from(s.at("layers"));
            step.wipe = number(s, "wipe", step.wipe, 0, 10);
            // A sequence may give the wipe in beats instead, so it moves with the music.
            if (s.contains("beats")) {
                if (p.kind != ShotKind::Sequence) throw std::invalid_argument("'beats' is for sequence presets; use 'wipe' (seconds)");
                step.wipe = number(s, "beats", 1, 0, 64) * 60.0 / p.sequence.tempo.bpm;
            }
            if (s.contains("split") && s.at("split").is_string()) {
                if (s.at("split") != "body") throw std::invalid_argument("'split' is a place across the frame (0.05 to 1) or \"body\"");
                step.follow = true; step.split = 0.5;
                step.share = number(s, "share", 0.5, 0.1, 0.9);
            } else step.split = number(s, "split", 1.0, 0.05, 1.0);
            if (s.contains("title")) { step.title = text(s, "title", 80, false); step.titled = true; }
            p.breakdown.push_back(step);
        }
    }
    if (p.kind == ShotKind::Sequence) {
        p.cover = {bit(Layer::Clay) | bit(Layer::Skeleton) | bit(Layer::Springs)};
        if (j.contains("cover")) {
            if (!j.at("cover").is_array() || j.at("cover").empty() || j.at("cover").size() > 8) throw std::invalid_argument("'cover' must list 1 to 8 covers");
            p.cover.clear();
            for (const auto& item : j.at("cover")) {
                // One layer by name, or a list of layers shown together.
                const LayerSet set = item.is_string() ? layers_from(Json::array({item})) : layers_from(item);
                if (set == 0 || (set & bit(Layer::Lighting))) throw std::invalid_argument("a cover needs at least one layer");
                if ((set & bit(Layer::Clay)) && (set & bit(Layer::Wireframe))) throw std::invalid_argument("a cover takes clay or wireframe, not both");
                p.cover.push_back(set);
            }
        }
    }
    return p;
}

Timeline make_timeline(const Preset& p, double route_length_cm, double walk_speed_cm_s) {
    Timeline tl;
    auto key = [&](const PresetKey& k, double t) {
        return OrbitKey{t, k.azimuth, k.distance * p.zoom, k.height, k.aim, k.fov * p.fov_scale};
    };
    // Breakdown steps are fractions of the whole take, whatever kind it is.
    auto reveals = [&] { for (const auto& s : p.breakdown) tl.breakdown.push_back({s.fraction * tl.total, s.layers, s.wipe, s.split, s.title, s.titled, s.follow, s.share}); };
    if (p.kind == ShotKind::Photo) {
        tl.total = double(p.photos.size()) * p.hold;
        reveals();
        return tl;
    }
    if (p.kind == ShotKind::Action) {
        tl.total = p.max_seconds;   // or until the player ends it, or the target is down
        reveals();
        return tl;
    }
    if (p.kind == ShotKind::Sequence) {
        tl.total = p.sequence.total;
        reveals();
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
    reveals();
    return tl;
}

} // namespace cine

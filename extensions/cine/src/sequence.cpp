#include "sequence.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cine {
namespace {
constexpr size_t max_shots = 256, max_looks = 512;

double number(const Json& j, const char* key, double fallback, double lo, double hi) {
    if (!j.contains(key)) return fallback;
    if (!j.at(key).is_number()) throw std::invalid_argument(std::string("'") + key + "' must be a number");
    const double v = j.at(key).get<double>();
    if (!std::isfinite(v) || v < lo || v > hi) throw std::invalid_argument(std::string("'") + key + "' is out of range");
    return v;
}
std::string text(const Json& j, const char* key, size_t limit) {
    if (!j.contains(key)) return {};
    if (!j.at(key).is_string() || j.at(key).get<std::string>().size() > limit) throw std::invalid_argument(std::string("'") + key + "' must be a short string");
    return j.at(key).get<std::string>();
}
SeqPose pose(const Json& j, const SeqPose& base, const char* what) {
    if (!j.is_object()) throw std::invalid_argument(std::string("'") + what + "' must be an object");
    SeqPose p = base;
    p.azimuth = number(j, "azimuth", p.azimuth, -3600, 3600);
    p.distance = number(j, "distance", p.distance, 20, 5000);
    p.height = number(j, "height", p.height, -500, 1500);
    p.aim = number(j, "aim", p.aim, -500, 1500);
    p.fov = number(j, "fov", p.fov, 5, 120);
    p.roll = number(j, "roll", p.roll, -90, 90);
    return p;
}
Ease ease_from(const std::string& name) {
    if (name.empty() || name == "in_out") return Ease::InOut;
    if (name == "linear") return Ease::Linear;
    if (name == "in") return Ease::In;
    if (name == "out") return Ease::Out;
    if (name == "whip") return Ease::Whip;
    if (name == "smooth") return Ease::Smooth;
    throw std::invalid_argument("'ease' must be linear, in, out, in_out, whip or smooth");
}
double lerp(double a, double b, double u) { return a + (b - a) * u; }
}

double ease(Ease e, double u) {
    u = std::clamp(u, 0.0, 1.0);
    switch (e) {
        case Ease::Linear: return u;
        case Ease::In: return u * u;
        case Ease::Out: return 1 - (1 - u) * (1 - u);
        case Ease::Whip: { const double s = u * u * u * (u * (u * 6 - 15) + 10); return s * s * (3 - 2 * s); }   // smootherstep, sharpened
        default: return u * u * (3 - 2 * u);
    }
}

Sequence Sequence::parse(const Json& j) {
    Sequence s;
    if (!j.contains("tempo") || !j.at("tempo").is_object()) throw std::invalid_argument("A sequence needs 'tempo' {bpm, offset}");
    const auto& tempo = j.at("tempo");
    s.tempo.bpm = number(tempo, "bpm", 120, 30, 300);
    s.tempo.offset = number(tempo, "offset", 0, 0, 60);
    s.tempo.beats_per_bar = int(number(tempo, "beats_per_bar", 4, 1, 12));
    if (!j.contains("end")) throw std::invalid_argument("A sequence needs 'end' (the bar it stops on)");
    const double end_bar = number(j, "end", 1, 1, 2000);
    s.total = s.tempo.seconds(end_bar);
    if (!(s.total >= 2) || s.total > 300) throw std::invalid_argument("A sequence runs 2 to 300 seconds");

    if (!j.contains("shots") || !j.at("shots").is_array() || j.at("shots").empty() || j.at("shots").size() > max_shots)
        throw std::invalid_argument("'shots' must list 1 to 256 shots");
    SeqPose last;
    double previous = -1;
    for (const auto& item : j.at("shots")) {
        if (!item.is_object()) throw std::invalid_argument("Each shot must be an object");
        SeqShot shot;
        shot.at = std::max(0.0, s.tempo.seconds(number(item, "at", 1, 0, 2000)));
        if (shot.at <= previous) throw std::invalid_argument("Shots must be in time order");
        if (shot.at >= s.total) throw std::invalid_argument("A shot starts after the sequence ends");
        previous = shot.at;
        const bool first = s.shots.empty();
        shot.cut = !item.value("flow", false);
        if (first) shot.cut = true;
        if (item.contains("flow") && !item.at("flow").is_boolean()) throw std::invalid_argument("'flow' must be true or false");
        // A cut starts from its own pose (or holds one pose when only "to" or only "from" is
        // given); a flow starts where the shot before it ended.
        if (shot.cut) {
            shot.view = item.value("view", false);
            if (item.contains("view") && !item.at("view").is_boolean()) throw std::invalid_argument("'view' must be true or false");
            if (!shot.view && !item.contains("from") && !item.contains("to")) throw std::invalid_argument("A cut needs 'from' or 'to', or 'view': true");
            shot.from = shot.view && !item.contains("from") && !item.contains("to") ? last : pose(item.contains("from") ? item.at("from") : item.at("to"), last, "from");
        } else {
            if (item.contains("from")) throw std::invalid_argument("A flow shot starts where the last one ended: remove 'from'");
            shot.from = last;
        }
        shot.to = item.contains("to") ? pose(item.at("to"), shot.from, "to") : shot.from;
        shot.ease = ease_from(text(item, "ease", 16));
        shot.fstop = number(item, "fstop", 0, 0, 32);
        if (item.contains("fixed")) { if (!item.at("fixed").is_boolean()) throw std::invalid_argument("'fixed' must be true or false"); shot.fixed = item.at("fixed").get<bool>(); }
        shot.fit = number(item, "fit", 0, 0, 2000);
        if (shot.fit > 0 && shot.fit < 20) throw std::invalid_argument("'fit' is a height in centimetres, 20 or more");
        if (const auto part = text(item, "track", 16); !part.empty()) {
            static constexpr const char* parts[] = {"", "head", "neck", "chest", "pelvis", "knees", "body"};
            size_t found = 0;
            for (size_t i = 1; i < std::size(parts); ++i) if (part == parts[i]) found = i;
            if (!found) throw std::invalid_argument("'track' must be head, neck, chest, pelvis, knees or body");
            shot.track = static_cast<Subject>(found);
        }
        last = shot.to;
        s.shots.push_back(shot);
    }
    for (size_t i = 0; i < s.shots.size(); ++i) s.shots[i].length = (i + 1 < s.shots.size() ? s.shots[i + 1].at : s.total) - s.shots[i].at;

    if (j.contains("looks")) {
        if (!j.at("looks").is_array() || j.at("looks").size() > max_looks) throw std::invalid_argument("'looks' must list up to 512 steps");
        double before = -1;
        for (const auto& item : j.at("looks")) {
            if (!item.is_object()) throw std::invalid_argument("Each look step must be an object");
            SeqLook look;
            look.at = std::max(0.0, s.tempo.seconds(number(item, "at", 1, 0, 2000)));
            if (look.at < before) throw std::invalid_argument("Look steps must be in time order");
            if (look.at >= s.total) throw std::invalid_argument("A look step is after the sequence ends");
            before = look.at;
            look.outfit = text(item, "outfit", 96);
            look.variant = text(item, "variant", 96);
            // "current" is the outfit she wears at F8, variant and all; every other outfit names its variant.
            if (look.outfit == "current" && !look.variant.empty()) throw std::invalid_argument("A look step with the 'current' outfit names no 'variant'");
            if (look.outfit != "current" && look.outfit.empty() != look.variant.empty()) throw std::invalid_argument("A look step names 'outfit' and 'variant' together");
            look.palette = text(item, "palette", 96);
            if (item.contains("controls")) {
                if (!item.at("controls").is_object() || item.at("controls").size() > 64) throw std::invalid_argument("'controls' must be an object of up to 64 controls");
                for (const auto& [id, value] : item.at("controls").items()) {
                    std::vector<double> channels;
                    if (value.is_number()) channels.push_back(value.get<double>());
                    else if (value.is_array() && !value.empty() && value.size() <= 3) for (const auto& v : value) { if (!v.is_number()) throw std::invalid_argument("Control values must be numbers"); channels.push_back(v.get<double>()); }
                    else throw std::invalid_argument("A control takes a number or 1 to 3 numbers");
                    for (double v : channels) if (!std::isfinite(v)) throw std::invalid_argument("Control values must be finite");
                    look.controls[id] = std::move(channels);
                }
            }
            if (item.contains("title")) { look.title = text(item, "title", 80); look.titled = true; }
            if (item.contains("cover")) { if (!item.at("cover").is_boolean()) throw std::invalid_argument("A look step's 'cover' must be true or false"); look.cover = item.at("cover").get<bool>(); }
            look.idle = text(item, "idle", 96);
            for (const char* key : {"wipe", "hold"}) if (item.contains(key)) {
                if (!item.at(key).is_number() || item.at(key).get<double>() < 0 || item.at(key).get<double>() > 5) throw std::invalid_argument(std::string("A look step's '") + key + "' is seconds, 0 to 5");
                (key[0] == 'w' ? look.wipe : look.hold) = item.at(key).get<double>();
            }
            if (item.contains("physics")) {
                if (!item.at("physics").is_object() || item.at("physics").size() > 16) throw std::invalid_argument("'physics' must map up to 16 motion controls to a preset");
                for (const auto& [id, value] : item.at("physics").items()) {
                    if (!value.is_string() || value.get<std::string>().empty() || value.get<std::string>().size() > 48) throw std::invalid_argument("A physics preset is named by its id");
                    look.physics[id] = value.get<std::string>();
                }
            }
            if (item.contains("performance")) {
                if (item.at("performance").is_boolean()) { look.restart = item.at("performance").get<bool>(); look.hold_performance = !look.restart; }
                else if (item.at("performance").is_string()) {
                    look.performance_id = text(item, "performance", 96);
                    if (look.performance_id.empty()) throw std::invalid_argument("A look step's 'performance' names the performance to start");
                } else throw std::invalid_argument("A look step's 'performance' is true (start the performance on this beat), false (an outfit change that does not start it) or the id of a performance to start");
            }
            if (item.contains("performance_rate")) {
                if (!item.at("performance_rate").is_number() || item.at("performance_rate").get<double>() < 0.1 || item.at("performance_rate").get<double>() > 4) throw std::invalid_argument("A look step's 'performance_rate' is 0.1 to 4");
                look.performance_rate = item.at("performance_rate").get<double>();
            }
            look.expression = text(item, "expression", 96);
            if (item.contains("montage")) {
                if (!item.at("montage").is_string() || item.at("montage").get<std::string>().size() > 256) throw std::invalid_argument("A look step's 'montage' is an asset path, or \"\" to stop the one playing");
                look.montage = item.at("montage").get<std::string>(); look.montage_set = true;
            }
            if (item.contains("montage_rate")) {
                if (!item.at("montage_rate").is_number() || item.at("montage_rate").get<double>() < 0.1 || item.at("montage_rate").get<double>() > 4) throw std::invalid_argument("A look step's 'montage_rate' is 0.1 to 4");
                look.montage_rate = item.at("montage_rate").get<double>();
            }
            if (item.contains("reset")) { if (!item.at("reset").is_boolean()) throw std::invalid_argument("A look step's 'reset' must be true or false"); look.reset = item.at("reset").get<bool>(); }
            if (look.outfit.empty() && look.palette.empty() && look.controls.empty() && !look.titled && look.idle.empty() && !look.reset && look.physics.empty() && !look.restart
                && look.performance_id.empty() && look.expression.empty() && !look.montage_set) throw std::invalid_argument("A look step changes an outfit, a palette, controls, the idle, the expression or the title, or starts a performance or a montage");
            s.looks.push_back(std::move(look));
        }
    }
    return s;
}

std::vector<std::pair<std::string, std::string>> Sequence::wardrobe() const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& look : looks) {
        if (look.outfit.empty()) continue;
        const auto key = std::make_pair(look.outfit, look.variant);
        if (std::find(out.begin(), out.end(), key) == out.end()) out.push_back(key);
    }
    return out;
}

bool Sequence::needs_css() const {
    for (const auto& look : looks)
        if (!look.outfit.empty() || !look.palette.empty() || !look.controls.empty() || !look.physics.empty() || !look.idle.empty()
            || look.reset || !look.expression.empty() || !look.performance_id.empty() || look.restart) return true;
    return false;
}

ShotPlan choose_plan(const std::vector<double>& clear_by_turn) {
    ShotPlan plan;
    if (clear_by_turn.empty()) return plan;
    size_t best = 0;
    // Staying put wins unless another side is clearly freer: a tenth of the way more per step
    // of turning, so the camera keeps the angle the preset asked for when it can.
    for (size_t i = 1; i < clear_by_turn.size() && i < std::size(plan_turns); ++i)
        if (clear_by_turn[i] > clear_by_turn[best] + 0.10 + 0.03 * double(i)) best = i;
    if (clear_by_turn[0] >= 0.98) best = 0;
    plan.turn = plan_turns[best];
    plan.scale = std::clamp(clear_by_turn[best], 0.25, 1.0);
    return plan;
}

SeqPose planned_pose(const Sequence& s, const std::vector<ShotPlan>& plan, double t) {
    SeqPose pose = pose_at(s, t);
    if (plan.size() != s.shots.size() || s.shots.empty()) return pose;
    const size_t index = shot_at(s, t);
    const auto& shot = s.shots[index];
    double scale = plan[index].scale;
    if (!shot.cut && index > 0) {
        // Flowing on: from the scale the last shot ended on to this shot's own.
        const double u = shot.length > 1e-6 ? ease(shot.ease, (t - shot.at) / shot.length) : 1.0;
        scale = plan[index - 1].scale + (plan[index].scale - plan[index - 1].scale) * u;
    }
    pose.azimuth += plan[index].turn;
    pose.distance *= scale;
    return pose;
}

size_t shot_at(const Sequence& s, double t) {
    size_t index = 0;
    for (size_t i = 0; i < s.shots.size(); ++i) { if (s.shots[i].at <= t) index = i; else break; }
    return index;
}

SeqPose pose_at(const Sequence& s, double t) {
    if (s.shots.empty()) return {};
    const size_t index = shot_at(s, t);
    const auto& shot = s.shots[index];
    if (shot.ease == Ease::Smooth && shot.length > 1e-6) {
        // One segment of a spline through the chain's poses (cubic Hermite, tangents from the
        // neighbouring segments). Where the chain starts or ends the tangent is the segment's
        // own, so the camera arrives from a cut already moving and leaves into one still moving.
        const bool before = index > 0 && !shot.cut && s.shots[index - 1].ease == Ease::Smooth && s.shots[index - 1].length > 1e-6;
        const bool after = index + 1 < s.shots.size() && !s.shots[index + 1].cut && s.shots[index + 1].ease == Ease::Smooth && s.shots[index + 1].length > 1e-6;
        const double u = std::clamp((t - shot.at) / shot.length, 0.0, 1.0), u2 = u * u, u3 = u2 * u;
        const double h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
        auto along = [&](double SeqPose::*field) {
            const double p1 = shot.from.*field, p2 = shot.to.*field, own = p2 - p1;
            const double m1 = before ? 0.5 * (own + (p1 - s.shots[index - 1].from.*field) * shot.length / s.shots[index - 1].length) : own;
            const double m2 = after ? 0.5 * (own + (s.shots[index + 1].to.*field - p2) * shot.length / s.shots[index + 1].length) : own;
            return h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;
        };
        return {along(&SeqPose::azimuth), std::max(20.0, along(&SeqPose::distance)), along(&SeqPose::height), along(&SeqPose::aim),
                std::clamp(along(&SeqPose::fov), 5.0, 120.0), along(&SeqPose::roll)};
    }
    const double u = shot.length > 1e-6 ? ease(shot.ease, (t - shot.at) / shot.length) : 1.0;
    return {lerp(shot.from.azimuth, shot.to.azimuth, u), lerp(shot.from.distance, shot.to.distance, u), lerp(shot.from.height, shot.to.height, u),
            lerp(shot.from.aim, shot.to.aim, u), lerp(shot.from.fov, shot.to.fov, u), lerp(shot.from.roll, shot.to.roll, u)};
}

} // namespace cine

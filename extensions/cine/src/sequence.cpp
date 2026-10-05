#include "sequence.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cine {
namespace {
constexpr size_t max_shots = 256, max_looks = 512;
constexpr double flash_in = 0.05, flash_out = 0.22;

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
    throw std::invalid_argument("'ease' must be linear, in, out, in_out or whip");
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
            if (!item.contains("from") && !item.contains("to")) throw std::invalid_argument("A cut needs 'from' or 'to'");
            shot.from = pose(item.contains("from") ? item.at("from") : item.at("to"), last, "from");
        } else {
            if (item.contains("from")) throw std::invalid_argument("A flow shot starts where the last one ended: remove 'from'");
            shot.from = last;
        }
        shot.to = item.contains("to") ? pose(item.at("to"), shot.from, "to") : shot.from;
        shot.ease = ease_from(text(item, "ease", 16));
        shot.fstop = number(item, "fstop", 0, 0, 32);
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
            if (look.outfit.empty() != look.variant.empty()) throw std::invalid_argument("A look step names 'outfit' and 'variant' together");
            look.palette = text(item, "palette", 96);
            look.flash = number(item, "flash", look.outfit.empty() ? 0 : 0.12, 0, 2);
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
            if (look.outfit.empty() && look.palette.empty() && look.controls.empty()) throw std::invalid_argument("A look step changes an outfit, a palette or controls");
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

size_t shot_at(const Sequence& s, double t) {
    size_t index = 0;
    for (size_t i = 0; i < s.shots.size(); ++i) { if (s.shots[i].at <= t) index = i; else break; }
    return index;
}

SeqPose pose_at(const Sequence& s, double t) {
    if (s.shots.empty()) return {};
    const auto& shot = s.shots[shot_at(s, t)];
    const double u = shot.length > 1e-6 ? ease(shot.ease, (t - shot.at) / shot.length) : 1.0;
    return {lerp(shot.from.azimuth, shot.to.azimuth, u), lerp(shot.from.distance, shot.to.distance, u), lerp(shot.from.height, shot.to.height, u),
            lerp(shot.from.aim, shot.to.aim, u), lerp(shot.from.fov, shot.to.fov, u), lerp(shot.from.roll, shot.to.roll, u)};
}

double flash_alpha(double t, double from, double release) {
    if (t < from) return 0;
    const double rise = std::min(1.0, (t - from) / flash_in);
    if (release < 0 || t < release) return rise;
    return std::max(0.0, std::min(rise, 1.0 - (t - release) / flash_out));
}

} // namespace cine

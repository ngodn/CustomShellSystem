#pragma once
// Sequence takes: a list of shots on a tempo grid, for a take cut to music. Pure data and
// maths, unit-tested off-game.
//
// Times in a preset are in BARS, counted from 1 like a musician does: 10 is the downbeat of
// bar 10, 10.5 is half a bar later (beat 3 in 4/4), 10.25 is beat 2. A bar is
// `beats_per_bar * 60 / bpm` seconds and bar 1 starts `offset` seconds into the take, so the
// take can start on the track's first sample and still land on its beats.
//
// A shot moves the camera from one pose to another around her (azimuth, distance, height,
// aim height, field of view, roll) with an ease. It starts on a cut (the camera is placed at
// once) or flows on from where the shot before it ended, which is how a long unbroken move
// and its whips are written. A look step changes what she wears or how it is set up. One
// that changes the outfit is covered by a transition (the director wipes the picture into a
// frame pass and back), because wearing an outfit is not instant.
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

struct Tempo {
    double bpm = 120, offset = 0;
    int beats_per_bar = 4;
    double bar_seconds() const { return beats_per_bar * 60.0 / bpm; }
    double seconds(double bar) const { return offset + (bar - 1.0) * bar_seconds(); }   // bar 1 = offset
    double bar(double seconds) const { return (seconds - offset) / bar_seconds() + 1.0; }
};

// Where the camera is, around her: degrees and centimetres as in every other preset, plus
// roll (a dutch tilt) in degrees.
struct SeqPose { double azimuth = 0, distance = 300, height = 0, aim = 0, fov = 40, roll = 0; };

enum class Ease { Linear, In, Out, InOut, Whip };
// 0..1 -> 0..1. Whip leaves slowly, crosses fast and settles slowly: most of the move
// happens in the middle third.
double ease(Ease e, double u);

struct SeqShot {
    double at = 0, length = 0;   // seconds into the take; length runs to the next shot or the end
    bool cut = true;             // false: flows on from the previous shot's end pose
    SeqPose from, to;
    Ease ease = Ease::InOut;
    double fstop = 0;            // depth of field for this shot, 0 off
};

// One change of look. `outfit` empty leaves the outfit alone (palette and controls only).
struct SeqLook {
    double at = 0;               // seconds
    std::string outfit, variant, palette;
    std::map<std::string, std::vector<double>> controls;   // 1 to 3 channels each
};

struct Sequence {
    Tempo tempo;
    double total = 0;            // seconds
    std::vector<SeqShot> shots;  // in time order, the first at 0 or later
    std::vector<SeqLook> looks;  // in time order
    // From a preset's "tempo", "end", "shots" and "looks". Throws std::invalid_argument
    // naming the field.
    static Sequence parse(const Json& preset);
    // Every distinct outfit/variant the take wears, in first-use order.
    std::vector<std::pair<std::string, std::string>> wardrobe() const;
};

// The shot playing at `t` seconds (the last one that has started); 0 before the first.
size_t shot_at(const Sequence& s, double t);
// The camera pose `t` seconds into the take.
SeqPose pose_at(const Sequence& s, double t);

} // namespace cine

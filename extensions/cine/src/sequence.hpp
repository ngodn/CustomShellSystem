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
#include <cstdint>
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

enum class Ease { Linear, In, Out, InOut, Whip, Smooth };
// What a shot is framed on. None measures its heights from her capsule centre, a fixed point.
// The others measure them from a part of her body, which the camera then follows: a face shot
// stays on her face whatever her height, her boots or her pose.
// Body is the middle of her, halfway between her head and her feet.
enum class Subject : uint8_t { None, Head, Neck, Chest, Pelvis, Knees, Body, Count };
// 0..1 -> 0..1. Whip leaves slowly, crosses fast and settles slowly: most of the move
// happens in the middle third. Smooth is not a curve of its own shot: a run of flowing
// "smooth" shots is one spline through their poses, so the camera glides through each pose
// without stopping or turning a corner there (as a value of u it is linear).
double ease(Ease e, double u);

struct SeqShot {
    double at = 0, length = 0;   // seconds into the take; length runs to the next shot or the end
    bool cut = true;             // false: flows on from the previous shot's end pose
    SeqPose from, to;
    Ease ease = Ease::InOut;
    double fstop = 0;            // depth of field for this shot, 0 off
    Subject track = Subject::None;   // "track": "head", "neck", "chest", "pelvis", "knees" or "body"
    // "fit": how many centimetres of height the frame shows where she stands. The lens is then
    // set from that and from how far the camera really is, so a whole-figure shot shows her
    // from head to foot in any window shape, and still does when a wall brings the camera in.
    // 0 leaves the pose's own field of view.
    double fit = 0;
    // "fixed": the side this shot is taken from is part of the shot. When something is in the
    // way it may come closer, never swing round to another side.
    bool fixed = false;
    // "view": the camera as the player framed it in Cine World when the take started (its
    // place around her, height, tilt and lens), held for this shot. Resolved at F8.
    bool view = false;
};

// One change of look. `outfit` empty leaves the outfit alone (palette and controls only).
struct SeqLook {
    double at = 0;               // seconds
    // palette: a palette id, or "original" for the outfit's own colours. An outfit of "current"
    // (no variant) is whatever she wears when the take starts, with the customization saved on it.
    std::string outfit, variant, palette;
    std::map<std::string, std::vector<double>> controls;   // 1 to 3 channels each
    // What the caption says from this step on ("Palette  //  Ember"), so a change has a name.
    // An empty title clears the caption. A step may be a title alone.
    std::string title; bool titled = false;
    bool cover = true;           // an outfit change: false leaves it uncovered (hide it in a whip)
    // How the cover moves for this change: the sweep in and out, and how long the picture
    // stays covered before the step's beat (the outfit lands under it). 0 = the tempo's
    // default (a quarter beat and one beat). A slow sweep with a longer hold reads as a
    // transition rather than a cut.
    double wipe = 0, hold = 0;
    std::string idle;            // a wardrobe idle to switch to (its id); put it on a cut, a pose change is not blended
    bool reset = false;          // back to the customization the take opened with, before this step's own changes
    // "performance": true starts the take's performance again from its first frame on this
    // beat; false keeps an outfit change from starting it the moment the look lands (a later
    // step will, after the change has been staged). Absent: an outfit change starts it on landing.
    bool restart = false, hold_performance = false;
    std::map<std::string, std::string> physics;   // a motion control and the wardrobe's preset for it ("chest_motion": "natural")
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

// What the world allows each shot, found by tracing before the take rolls. A shot is turned
// as a whole to a clearer side (`turn`, degrees added to its azimuths) and brought closer as a
// whole (`scale` on its distances), so it keeps its shape. Shots that flow into each other
// share one turn, or the unbroken move would break.
struct ShotPlan { double turn = 0, scale = 1; };
// The angles a blocked shot may be turned by, nearest first.
inline constexpr double plan_turns[] = {0, 35, -35, 70, -70, 120, -120, 180};
// From the clear fraction of each candidate turn (1 = the whole camera line is free, 0.3 =
// something stands at 30 percent of the way): the turn to use and the scale that clears it.
// A turn is only worth it when it clears clearly more than staying put.
ShotPlan choose_plan(const std::vector<double>& clear_by_turn);
// The pose `t` seconds into the take with the plan applied. Within a chain of flowing shots
// the scale eases from one shot's value to the next, so the camera never jumps.
SeqPose planned_pose(const Sequence& s, const std::vector<ShotPlan>& plan, double t);

// The shot playing at `t` seconds (the last one that has started); 0 before the first.
size_t shot_at(const Sequence& s, double t);
// The camera pose `t` seconds into the take.
SeqPose pose_at(const Sequence& s, double t);

} // namespace cine

#pragma once
// CINE presets: data files a modder copies and edits (presets/*.cine.json). Parsing is
// strict and bounded: a broken preset is refused with a message naming the field, it never
// reaches the camera.
#include "layers.hpp"
#include "sequence.hpp"
#include "shot.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

enum class ShotKind { Walk, Glide, Turntable, Photo, Action, Sequence };
const char* kind_name(ShotKind);

// A camera key as written in a preset. For Walk, `at` is "start", "walk" or a fraction of
// the walking time; for Glide and Turntable, a fraction of the duration.
struct PresetKey { std::string anchor; double fraction = 0; double azimuth = 0, distance = 300, height = 0, aim = 0, fov = 40; };

// One look step: a palette and per-control values (one number, or 1-3 channels; colour
// controls take three as a swatch), at a fraction of the walk (Walk) or duration (others). Controls a worn outfit
// does not have are skipped, not errors, so one preset serves many outfits.
// A step may also carry a title for the caption ("Palette  //  Crimson"), or be a title alone.
struct LookStep { double fraction = 0; std::string palette; std::map<std::string, std::vector<double>> controls; std::string title; bool titled = false;
                  std::map<std::string, std::string> physics; };   // controls: 1-3 channels as given; physics: a motion control and the wardrobe's preset for it

struct PhotoShot { std::string name; double azimuth = 0, distance = 300, height = 0, aim = 0, fov = 40; };

// One breakdown step: from this point of the take the frame shows exactly these layers,
// reached by a left-to-right wipe of `wipe` seconds (0 cuts). An empty set is the final image.
// `split` is where the wipe line comes to rest (a fraction of the frame's width): 1 is the
// whole frame; less holds the layers left of the line and the final picture right of it.
// `follow` ("split": "body") keeps the line on the middle of her body instead of a fixed place.
// `share`, with follow: how much of her width the layers get (0.5 is her centre line).
struct BreakdownStep { double fraction = 0; LayerSet layers = 0; double wipe = 1.2; double split = 1.0; std::string title; bool titled = false; bool follow = false; double share = 0.5;
                       bool labels = true; };   // "labels": false keeps the arrows off while this step shows

// Something that happens in a fight, and what the camera does about it.
struct Beat {
    enum class On { Hit, Hurt, Parry, Kill } on = On::Hit;
    enum class Do { Slowmo, Punch } what = Do::Punch;
    double cooldown = 2.0;   // seconds before this beat can fire again
};

// An action take: the player plays, the camera films. Everything is measured in the world,
// not from her facing, because she spins in a fight.
struct ActionShot {
    Framing framing = Framing::Duel;
    double azimuth = 70;          // degrees: from the duel axis (duel, shoulder) or world north (follow)
    double orbit_speed = 6;       // degrees per second; 0 holds the angle
    double distance = 420;        // closest the camera sits
    double distance_max = 1100;   // duel: how far it backs off to keep both in frame
    double margin = 150;          // duel: room around the fighters
    double height = 35, aim = 10; // camera and aim height from the capsule centre
    double fov = 38;
    double smooth = 0.45;         // seconds of smoothing on what the camera follows
    double shake = 0.5;           // the game's camera shake scale during the take; negative leaves it
    double fstop = 0;             // depth of field, 0 off; focus stays on the fight
    double slowmo = 0.35;         // time scale of a slow-motion beat
    double slowmo_hold = 0.7;     // real seconds a slow-motion beat lasts
    double tail = 3.0;            // seconds the take runs on after the target dies
    bool respect_game_cameras = true;   // let the game's own finisher cameras play
    std::vector<Beat> beats;
};

struct Preset {
    std::string id, name, description;
    ShotKind kind = ShotKind::Glide;
    double intro = 4.5;          // Walk: seconds before she sets off
    double duration = 5.0;       // Glide / Turntable: seconds
    double tail = 13.0;          // Walk: seconds on the final shot
    double max_seconds = 49.0;   // Walk: hard cap on the take
    double lead = 0.8;           // seconds the camera target runs ahead of now
    double zoom = 1.0;           // distance multiplier
    double fov_scale = 1.0;      // FOV multiplier (portrait screens want more)
    double hold = 1.5;           // Photo: seconds each still is held
    // Heights and aims are measured from her capsule centre (walks: the route height) or from
    // her head bone (poses: a seated or kneeling head sits a metre lower than a standing one).
    enum class Reference { Capsule, Head } reference = Reference::Head;
    std::vector<PresetKey> keys;
    std::vector<PhotoShot> photos;
    std::vector<LookStep> look;  // optional
    std::vector<BreakdownStep> breakdown;   // optional, any kind
    ActionShot action;           // Action only
    Sequence sequence;           // Sequence only: shots and looks on a tempo grid
    std::string audio;           // optional: a pack audio id (e.g. iris-out-x4), or a .wav / .mp3 in the extension folder;
                                 // played from the take's first frame
    std::string audio_asset;     // set by the loader: the SoundWave an audio id resolves to
    std::string pack;            // set by the loader: the pack token it came from, empty for the presets folder
    bool audio_is_file() const { return audio.ends_with(".wav") || audio.ends_with(".mp3"); }
    std::string idle;            // optional: the idle every look plays during the take (a CSS idle id)
    // Optional: a performance (a CSS shared dance or pose id) the take plays from its first
    // frame. In a sequence with look steps it is stopped for every outfit change and started
    // again from its first frame the moment the new look is on, so each pass dances from
    // the top. "menu" plays the performance chosen on the CINE page.
    std::string performance; double performance_rate = 1.0;
    std::string frame;           // optional: the frame the take is laid out for ("9:16"...), whatever guide is chosen
    // Sequence: what takes the whole frame while she changes outfit, used in turn. The rig
    // (clay with the bones in it) by default: the outfit changes under one clay figure.
    std::vector<LayerSet> cover;
    bool look_ends_on_own = true;   // rebuild the modder's own look for the final shot

    static Preset parse(const Json&);
};

// A shot ready to play: absolute key times, the take length and when things happen.
struct Timeline {
    double total = 0;            // seconds of the take
    double walk_start = 0;       // Walk only
    double walk_time = 0;        // Walk only
    double arrive = 0;           // Walk only: when she reaches the final spot
    std::vector<OrbitKey> keys;
    struct Step { double at = 0; const LookStep* step = nullptr; bool own = false; };
    std::vector<Step> look;
    struct Reveal { double at = 0; LayerSet layers = 0; double wipe = 0; double stop = 1.0; std::string title; bool titled = false; bool follow = false; double share = 0.5;
                    bool labels = true; };
    std::vector<Reveal> breakdown;
};
// Walk needs the route length; others ignore it. Throws std::invalid_argument when the
// take would not fit max_seconds.
Timeline make_timeline(const Preset&, double route_length_cm, double walk_speed_cm_s);

} // namespace cine

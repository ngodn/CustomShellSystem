#pragma once
// CINE extension: menu, settings, route recording, final-shot capture and the take director.
// All callbacks arrive on the game thread from CSSX. Outside Cine World tick() and render()
// return at once; inside, the director issues a handful of cached bridge calls per tick.
#include "breakdown.hpp"
#include "callouts.hpp"
#include "guides.hpp"
#include "preset.hpp"
#include "rig.hpp"
#include "scene.hpp"
#include "shot.hpp"
#include <cssx/client.hpp>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cine {

class Extension {
public:
    explicit Extension(const CssxHost* host);
    void tick(double seconds);
    int render(const CssxFrame* frame);
    Json model() const;
    void event(const Json& event);
    Json status() const;
    bool stop();

    // For tests: the director without a frame stream.
    struct FrameData {
        bool valid = false, ready = false, in_menu = false;
        uint32_t world = 0; uint64_t pawn = 0, controller = 0;
        Vec3 position, velocity; double yaw = 0; double seconds = 0;
        bool has_camera = false; Vec3 camera; Rotator camera_rotation;   // from CSSX 1.3.1 on
    };
    void feed_frame(const FrameData& f) { frame_ = f; }

private:
    enum class Mode { Idle, RecordWait, Recording, Armed, Preparing, Countdown, Running };
    struct Settings {
        std::string preset = "fashion-walk-360";
        double zoom = 1.0, height = 0.0, fov_scale = 1.0, countdown = 3.0;
        std::string guide = "off", look = "off";   // the look track only runs when asked for
        bool walls = true;
        std::string scene = "off";        // clean scene: off | enemies | everyone
        bool keep_target = false;         // clean scene leaves the lock-on target in
        std::string breakdown = "off";    // breakdown track: off | preset | reel
        bool labels = true;               // callouts and caption during a breakdown
        bool breakdown_target = false;    // also break down the lock-on target
        bool signature = false;           // the signature block (signature.txt) during takes
    };
    // One take, built while Preparing and played while Running.
    struct Take {
        Preset preset; Timeline timeline;
        std::unique_ptr<OrbitCurve> curve;
        std::optional<Route> route; std::unique_ptr<Track> track;
        Vec3 base; double base_yaw = 0; bool from_head = false;
        std::vector<double> wall_raw; size_t wall_next = 0; WallProfile walls;
        Json dolly, final_wide, final_close;
        // Look track: CSS snapshot and one apply payload per step.
        bool look = false; Json snapshot; std::vector<std::pair<double, Json>> steps; size_t step_next = 0;
        // Clock and filters.
        double clock = 0, countdown = 0, yaw_f = 0, ctl_yaw = 0, fov_applied = -1; Vec3 vel_f;
        // Walk state.
        bool walking = false, arrived = false; size_t target = 0; double speed = 0, speed_applied = -1, stalled_at = -1;
        bool cut_wide = false, cut_close = false; int photo = -1;
        uint32_t world = 0; uint64_t pawn = 0; int prepare_step = 0;
        // Breakdown track.
        std::vector<Timeline::Reveal> reveals; size_t reveal_next = 0; LayerSet layers = 0; double build_wait = 0;
        // Action take: who she fights, smoothed positions, the fight's state, the beats.
        Json opponent; bool had_opponent = false, opponent_down = false, opponent_seen = false;
        double opponent_down_at = -1, opponent_health = -1, player_health = -1;
        Vec3 subject_f, opponent_f;
        double azimuth = 0, punch = 0, punch_until = -1, slow_until = -1, scale = 1, scale_applied = 1, focus_applied = -1, reassert_at = 0;
        int parry_count = 0, poll = 0;
        std::vector<double> beat_ready;   // per beat: when it may fire again
        std::string note;   // why part of the take is off (look track), shown with every take status
    };

    // Settings, presets, persistence.
    void load_presets();
    void load_state();
    void save_state() const;
    const Preset* current_preset() const;

    // Modes.
    void enter();
    void exit_world(const std::string& why);
    void start_take();
    void prepare();
    void run();
    void finish_take(const std::string& why, bool failed);
    void begin_countdown();
    void action_tick(double dt);
    void fire(Beat::On what);
    void apply_scene();
    std::vector<Timeline::Reveal> reel(double total) const;
    std::vector<std::string> spring_bones();
    void record_tick();
    void capture_final_now();
    void poll_keys();
    void report(const std::string& message, bool error = false);
    std::string with_note(const std::string& message) const;
    bool restore_look(const Json& snapshot);
    void queue_look_restore(const Json& snapshot);
    void retry_look_restore(double dt);

    // Take helpers.
    Vec3 her_position() const { return frame_.position; }
    void steer(double dt);
    void place_dolly(double dt, bool teleport);
    Json look_payload(const LookStep& step, const Json& describe) const;

    cssx::Client host_;
    Rig rig_;
    Guides guides_;
    Scene scene_;
    Breakdown breakdown_, breakdown_opponent_;
    Typeface typeface_;   // the label font, shared by the guides and the callouts
    Callouts callouts_;
    // Breakdown labels. Sampled on the tick: where each labelled component is relative to
    // her, and how the lens maps onto the HUD. Drawn every frame from that with one host call
    // (where the camera is now) and the frame's own player position.
    struct Label { std::string text; Vec3 before, now; };
    std::vector<Label> labels_;
    std::vector<CalloutTarget> callout_targets_;
    Lens lens_;
    double label_age_ = 0;       // seconds since the last sample, for easing between two
    bool labels_live_ = false;
    bool pak_missing_ = false;   // read once on entering Cine World, for the menu
    std::vector<std::string> signature_;   // lines of signature.txt, read on entering Cine World
    void load_signature();
    void sample_labels();
    double tick_dt_ = 0.1;
    Settings settings_;
    std::map<std::string, Preset> presets_;
    std::vector<std::string> preset_problems_;
    std::optional<Route> route_;
    std::optional<FinalShot> final_;
    std::vector<Sample> samples_;
    double record_clock_ = 0, still_since_ = -1;
    FrameData frame_;
    Mode mode_ = Mode::Idle;
    uint32_t world_ = 0;   // world generation Cine World or the recording started in
    Mode record_return_ = Mode::Idle;   // where route recording goes back to (Idle or Armed)
    std::unique_ptr<Take> take_;
    bool capture_pending_ = false; double capture_after_ = 0;
    bool start_pending_ = false;
    std::vector<bool> keys_previous_;
    std::string status_ = "Ready", error_;
    bool stopped_ = false;
    // A take's look the restore could not put back yet (world change, CSS busy): retried once a
    // second for up to two minutes, in any mode.
    static constexpr double look_restore_window = 120.0;
    Json look_restore_; double look_restore_wait_ = 0, look_restore_left_ = 0; std::string look_restore_error_;
};

} // namespace cine

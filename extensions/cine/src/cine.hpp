#pragma once
// CINE extension: menu, settings, route recording, final-shot capture and the take director.
// All callbacks arrive on the game thread from CSSX. Outside Cine World tick() and render()
// return at once; inside, the director issues a handful of cached bridge calls per tick.
#include "audio.hpp"
#include "breakdown.hpp"
#include "callouts.hpp"
#include "guides.hpp"
#include "preset.hpp"
#include "performance.hpp"
#include "expression.hpp"
#include "rig.hpp"
#include "scene.hpp"
#include "shot.hpp"
#include <cssx/client.hpp>
#include <map>
#include <chrono>
#include <deque>
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
        std::string picture = "sharp";    // render quality during a take: game | sharp | full
        PerformanceChoice performance;
        std::string expression = "unchanged";
    };
    // One take, built while Preparing and played while Running.
    struct Take {
        Preset preset; Timeline timeline;
        PerformanceChoice performance;
        std::string expression = "unchanged";
        bool expression_started = false, expression_ready = false;
        double expression_wait = 0, expression_poll = 0;
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
        Json look_before;   // her CSS look when a breakdown that changes materials started, for a repaint
        // Sequence take: the clock runs on frames so a cut lands within a frame of its beat.
        double seq_clock = 0;
        bool wall_clock = false;       // music is playing: the clock is real time since the first frame
        std::chrono::steady_clock::time_point rolled_at{};
        size_t shot_index = static_cast<size_t>(-1);   // the shot whose start has been applied
        bool seq_looks = false, wore = false, dof_on = false;
        Json own_look;                 // describe() before the take: outfit, variant, customize
        Json look_now;                 // describe() of what she wears now, for control ids
        size_t look_next = 0;
        // An outfit change: the picture wipes into a frame pass, she changes under it, and it
        // wipes back on the beat.
        enum class Wear { Idle, Covering, Waiting } wearing = Wear::Idle;
        double cover_from = -1, wear_sent_at = 0, transition_until = -1;
        bool covered = false, adopted = false;   // adopted: the breakdown already follows the look that just landed
        // What the breakdown track wants on screen now. A cover takes the picture over for
        // the length of an outfit change and hands it back to this.
        Timeline::Reveal desired; bool desired_dirty = false;
        std::string title;             // the caption a look or breakdown step last set
        std::vector<std::pair<double, std::string>> titles; size_t title_next = 0;   // look-track titles of a take that is not a sequence
        Json opening;                  // her customization as the take opened, for a look step's "reset"
        // A split that follows her body: three points of her centre line (pelvis, chest, head)
        // relative to her position, sampled each tick and eased between, and where that line
        // crosses the middle of the frame, as a fraction of its width.
        bool follows = false, body_live = false;
        bool fits = false;             // a shot fits a height into the frame: the lens ratio is kept measured
        static constexpr size_t body_parts = 19;
        Vec3 body_before[body_parts], body_now[body_parts];   // head, torso, arms, hands, legs, feet (see sample_body)
        double body_age = 0, body_place = 0.5;
        bool body_placed = false;
        Json body_mesh;
        // The parts of her body shots are framed on (Subject): where each is relative to her
        // position, sampled each tick, eased between samples and smoothed, so the camera
        // follows her without picking up every twitch of the idle.
        static constexpr size_t tracks = static_cast<size_t>(Subject::Count);
        bool track_used[tracks]{}, track_ok[tracks]{}, track_live = false;
        Vec3 track_before[tracks], track_now[tracks], track_at[tracks];
        double track_age = 0;
        // tan(half the vertical angle of view) over tan(half the field of view the camera is
        // set to): what a "fit" shot needs to turn a height into a lens. Measured from the
        // game's own projection, since it depends on the window's shape; 16:9 until then.
        double lens_ratio = 0.5625;
        double wall_scale = 1.0;       // 1, or less while something stands between the camera and her
        double wall_target = 1.0;      // where the last trace wants it; the scale eases there frame by frame
        // The plan against the world, made before the take rolls: a turn and a scale per shot.
        std::vector<ShotPlan> plan;
        std::vector<std::vector<double>> clear;   // per shot, per candidate turn: how much of the camera line is free
        std::vector<double> chain_clear;
        size_t plan_chain = 0, plan_turn = 0, plan_shot = 0, plan_sample = 0;
        bool idle_set = false;         // CSS was asked for one idle across the take's looks
        int wear_poll = 0, transitions = 0;
        // What to do before the take rolls: wear each look once, build the breakdown on the
        // look it belongs to, end on the opening look.
        struct Prep { bool build = false; std::string outfit, variant; bool own = false, begin = false; };
        std::deque<Prep> prep;
        bool prep_sent = false, build_started = false;
        bool learning = false;         // the breakdown is reading the look just worn
        bool many_looks = false;       // the breakdown was taught several looks and follows outfit changes
        double prep_wait = 0;
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
    std::string performance_conflict(const Preset&) const;
    const Preset* current_preset() const;

    // Modes.
    void enter();
    void exit_world(const std::string& why);
    void start_take();
    void prepare();
    void run();
    void finish_take(const std::string& why, bool failed);
    void begin_countdown();
    void begin_breakdown();
    void prepare_looks();
    bool plan_surroundings();   // true when done; a slice of traces per tick
    void sequence_frame(const CssxFrame& frame);
    void sequence_tick();
    struct SeqView { Vec3 camera; Rotator rotation; double fov = 40, focus = 300; };
    SeqView sequence_view(const SeqPose& pose, Vec3 anchor, double time) const;
    double coverage_at(double time, double distance, double fov) const;   // the height a "fit" shot shows at her, or 0
    Vec3 anchor_at(double time) const;   // the point a sequence's shot is measured from at this moment
    void sample_tracks(bool snap);
    Json wear_request(const std::string& outfit, const std::string& variant, bool own, bool persist) const;
    bool worn(const Json& described, const std::string& outfit, const std::string& variant) const;
    void apply_look(const SeqLook& look);
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
    Audio audio_;
    PerformanceCatalog performance_catalog_;
    PerformanceTake performance_;
    double performance_cleanup_wait_ = 0;
    ExpressionCatalog expression_catalog_;
    ExpressionTake expression_;
    double expression_cleanup_wait_ = 0;
    void roll();   // the take's first frame: clocks to zero, music on
    Typeface typeface_;   // the label font, shared by the guides and the callouts
    Callouts callouts_;
    // Breakdown labels. Sampled on the tick: where each labelled component is relative to
    // her, and how the lens maps onto the HUD. Drawn every frame from that with one host call
    // (where the camera is now) and the frame's own player position.
    struct Label { std::string text; Vec3 before, now; bool pair = false; Vec3 other_before, other_now; };
    std::vector<Label> labels_;
    std::vector<CalloutTarget> callout_targets_;
    Lens lens_;
    double label_age_ = 0;       // seconds since the last sample, for easing between two
    double viewport_h_ = 0;      // the viewport's height in HUD units, from the last frame
    bool labels_live_ = false;
    bool pak_missing_ = false;   // read once on entering Cine World, for the menu
    CssxFrame last_frame_{};     // the newest frame, for drawing outside render()
    // Her own look while a sequence has her in others, kept until CSS confirms she wears it
    // again. A take that starts before then must not mistake a borrowed look for hers.
    Json own_look_owed_;
    double own_look_check_ = 0;
    std::vector<std::string> signature_;   // lines of signature.txt, read on entering Cine World
    void load_signature();
    void sample_labels();
    void sample_body();
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

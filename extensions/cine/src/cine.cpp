#include "cine.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>

namespace cine {
namespace fs = std::filesystem;
namespace {
constexpr double walk_cm_s = 184.0;        // the game's force-walk speed
constexpr double start_cm_s = 70.0;        // she eases up from this instead of snapping
constexpr double yaw_tau = 0.9, vel_tau = 0.8;
constexpr double wall_step = 0.25;         // seconds between wall traces
constexpr size_t traces_per_tick = 20;     // preparing never hitches
constexpr size_t max_samples = 12000;      // 20 minutes at 10 Hz
constexpr double max_route_points = 600;
// Keyboard only: the D-pad uses items in Mortal Shell II, so it would drink a flask mid-take.
const std::vector<std::string> hotkeys{"F7", "F8", "F9", "F10"};
enum Hotkey { CaptureKey, TakeKey, CancelKey, GuideKey };
const std::vector<std::string> guide_ids{"off", "16:9", "9:16", "1:1", "2.39:1"};
const char* help_line = "CINE   F8 take   F9 cancel / exit   F10 frame guide   F7 use this view as final shot";
const std::vector<std::string> scene_ids{"off", "enemies", "everyone"};
const std::vector<std::string> breakdown_ids{"off", "preset", "reel"};
constexpr double build_timeout = 30.0;     // seconds a breakdown may take to build before the take rolls without the rest
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
Json handle(uint64_t id) { return id ? Json{{"$object", id}} : Json(); }

Json vec_json(Vec3 v) { return Json::array({v.x, v.y, v.z}); }
Vec3 json_vec(const Json& j) { return {j.at(0).get<double>(), j.at(1).get<double>(), j.at(2).get<double>()}; }
double clampd(double v, double lo, double hi) { return std::isfinite(v) ? std::clamp(v, lo, hi) : lo; }
}

Extension::Extension(const CssxHost* host) : host_(host), rig_(host_), scene_(host_), breakdown_(host_), breakdown_opponent_(host_) {
    // Passive start (ABI contract): read files and state, change nothing in the game.
    guides_.attach(host_.hud(), host_.context());
    callouts_.attach(host_.hud(), host_.context());
    load_presets();
    load_state();
}

// ---------------------------------------------------------------- presets and state

void Extension::load_presets() {
    presets_.clear(); preset_problems_.clear();
    try {
        const auto info = host_.request({{"op", "extension.info"}});
        const fs::path dir = fs::path(info.at("directory").get<std::string>()) / "presets";
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) { preset_problems_.push_back("presets folder is missing"); return; }
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            const auto name = entry.path().filename().string();
            if (name.size() < 10 || name.substr(name.size() - 10) != ".cine.json") continue;
            try {
                if (entry.file_size(ec) > 256 * 1024) throw std::invalid_argument("file is larger than 256 KiB");
                std::ifstream in(entry.path(), std::ios::binary);
                auto preset = Preset::parse(Json::parse(in));
                if (presets_.contains(preset.id)) throw std::invalid_argument("duplicate id " + preset.id);
                presets_.emplace(preset.id, std::move(preset));
            } catch (const std::exception& e) { preset_problems_.push_back(name + ": " + e.what()); }
            if (presets_.size() >= 128) break;
        }
    } catch (const std::exception& e) { preset_problems_.push_back(e.what()); }
    if (!presets_.empty() && !presets_.contains(settings_.preset)) settings_.preset = presets_.begin()->first;
    for (const auto& p : preset_problems_) host_.log("Preset skipped: " + p, "warning");
}

void Extension::load_state() {
    Json state;
    try { state = host_.request({{"op", "state.load"}}); } catch (...) { return; }
    if (!state.is_object()) return;
    const auto s = state.value("settings", Json::object());
    if (s.is_object()) {
        if (s.contains("preset") && s["preset"].is_string() && presets_.contains(s["preset"].get<std::string>())) settings_.preset = s["preset"];
        if (s.contains("zoom") && s["zoom"].is_number()) settings_.zoom = clampd(s["zoom"], 0.5, 2.0);
        if (s.contains("height") && s["height"].is_number()) settings_.height = clampd(s["height"], -150, 150);
        if (s.contains("fov_scale") && s["fov_scale"].is_number()) settings_.fov_scale = clampd(s["fov_scale"], 0.5, 2.0);
        if (s.contains("countdown") && s["countdown"].is_number()) settings_.countdown = clampd(s["countdown"], 0, 10);
        if (s.contains("guide") && s["guide"].is_string() && std::count(guide_ids.begin(), guide_ids.end(), s["guide"].get<std::string>())) settings_.guide = s["guide"];
        if (s.contains("look") && s["look"].is_string()) { const auto l = s["look"].get<std::string>(); if (l == "off" || l == "preset" || l == "palettes") settings_.look = l; }
        if (s.contains("walls") && s["walls"].is_boolean()) settings_.walls = s["walls"];
        if (s.contains("scene") && s["scene"].is_string() && std::count(scene_ids.begin(), scene_ids.end(), s["scene"].get<std::string>())) settings_.scene = s["scene"];
        if (s.contains("keep_target") && s["keep_target"].is_boolean()) settings_.keep_target = s["keep_target"];
        if (s.contains("breakdown") && s["breakdown"].is_string() && std::count(breakdown_ids.begin(), breakdown_ids.end(), s["breakdown"].get<std::string>())) settings_.breakdown = s["breakdown"];
        if (s.contains("labels") && s["labels"].is_boolean()) settings_.labels = s["labels"];
        if (s.contains("breakdown_target") && s["breakdown_target"].is_boolean()) settings_.breakdown_target = s["breakdown_target"];
    }
    try {
        const auto r = state.value("route", Json());
        if (r.is_array() && r.size() >= 2 && r.size() <= max_route_points) {
            Route route;
            for (const auto& p : r) route.points.push_back(json_vec(p));
            set_start(route);
            if (route.length() >= 300) route_ = std::move(route);
        }
        const auto f = state.value("final", Json());
        if (f.is_object()) {
            FinalShot shot;
            shot.position = json_vec(f.at("position")); shot.yaw = f.at("yaw");
            shot.camera = json_vec(f.at("camera")); shot.pitch = f.at("pitch"); shot.camera_yaw = f.at("camera_yaw");
            shot.fov = clampd(f.at("fov"), 5, 170);
            final_ = shot;
        }
    } catch (...) { route_.reset(); final_.reset(); host_.log("Saved route or final shot was unreadable and was dropped", "warning"); }
}

void Extension::save_state() const {
    Json value{{"settings", {{"preset", settings_.preset}, {"zoom", settings_.zoom}, {"height", settings_.height}, {"fov_scale", settings_.fov_scale},
                             {"countdown", settings_.countdown}, {"guide", settings_.guide}, {"look", settings_.look}, {"walls", settings_.walls},
                             {"scene", settings_.scene}, {"keep_target", settings_.keep_target}, {"breakdown", settings_.breakdown},
                             {"labels", settings_.labels}, {"breakdown_target", settings_.breakdown_target}}}};
    if (route_) { Json pts = Json::array(); for (const auto& p : route_->points) pts.push_back(vec_json(p)); value["route"] = std::move(pts); }
    if (final_) value["final"] = {{"position", vec_json(final_->position)}, {"yaw", final_->yaw}, {"camera", vec_json(final_->camera)},
                                  {"pitch", final_->pitch}, {"camera_yaw", final_->camera_yaw}, {"fov", final_->fov}};
    try { host_.request({{"op", "state.save"}, {"value", value}}); } catch (const std::exception& e) { host_.log(std::string("Could not save CINE settings: ") + e.what(), "warning"); }
}

const Preset* Extension::current_preset() const {
    const auto it = presets_.find(settings_.preset);
    return it == presets_.end() ? nullptr : &it->second;
}

std::string Extension::with_note(const std::string& message) const {
    return take_ && !take_->note.empty() ? message + "  " + take_->note : message;
}

void Extension::report(const std::string& message, bool error) {
    if (error) error_ = message; else { status_ = message; error_.clear(); }
    try { host_.log(message, error ? "warning" : "info"); host_.invalidate(); } catch (...) {}
}

// ---------------------------------------------------------------- callbacks

int Extension::render(const CssxFrame* f) {
    if (stopped_ || !f) return 1;
    frame_.valid = true; frame_.ready = f->world_ready != 0; frame_.in_menu = f->in_menu != 0;
    frame_.world = f->world_generation; frame_.pawn = f->pawn; frame_.controller = f->controller;
    frame_.position = {f->player_x, f->player_y, f->player_z};
    frame_.velocity = {f->velocity_x, f->velocity_y, f->velocity_z};
    frame_.yaw = f->player_yaw; frame_.seconds = f->seconds;
    if (mode_ == Mode::Idle || mode_ == Mode::RecordWait) { guides_.hide(); callouts_.hide(); return 1; }
    Overlay o;
    o.visible = true;
    const bool framing = mode_ == Mode::Armed || mode_ == Mode::Recording;
    o.guide = framing ? guide_from(settings_.guide) : Guide::Off;
    if (mode_ == Mode::Armed) o.help = help_line;
    else if (mode_ == Mode::Recording) o.help = "CINE   recording your route: walk it, then stand still for 3 s (F9 stops)";
    if (mode_ == Mode::Countdown && take_) o.countdown = "CINE  " + std::to_string(int(std::ceil(std::max(0.0, take_->countdown))));
    if (mode_ == Mode::Running || mode_ == Mode::Preparing) o.visible = false;   // nothing of CINE in the recording
    if (frame_.in_menu) o.visible = false;                                       // nor over a menu
    guides_.update(*f, o);
    // Breakdown labels are part of the picture, so they stay on during the take. One host call
    // a frame: where the camera is. The rest is the tick's samples and plain maths.
    if (mode_ == Mode::Running && labels_live_ && lens_.ok && !frame_.in_menu) {
        label_age_ += frame_.seconds;
        callout_targets_.clear();
        try {
            Vec3 camera; Rotator rotation;
            if (rig_.view_point(camera, rotation)) {
                const auto basis = view_basis(rotation);
                const double ease = std::clamp(label_age_ / 0.1, 0.0, 1.0);
                for (const auto& label : labels_) {
                    CalloutTarget target;
                    target.text = label.text;
                    target.visible = project(camera, basis, lens_, frame_.position + label.before + (label.now - label.before) * ease, target.x, target.y);
                    callout_targets_.push_back(std::move(target));
                }
            }
        } catch (...) { callout_targets_.clear(); }
        callouts_.update(*f, callout_targets_, breakdown_.caption());
    } else callouts_.hide();
    return 1;
}

void Extension::tick(double seconds) {
    if (stopped_) return;
    try {
        if (mode_ == Mode::Idle && !capture_pending_ && look_restore_.is_null() && !scene_.pending() && !breakdown_.busy() && !breakdown_opponent_.busy())
            return;   // free outside Cine World
        const double dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0, 1.0) : 0.0;
        tick_dt_ = dt;
        // A loading screen, death or travel tears the world down: every handle is dead. The CSS
        // look is not a world object, so a look the take changed is still put back (retried).
        if (mode_ != Mode::Idle && mode_ != Mode::RecordWait && frame_.valid && frame_.world != world_) {
            if (take_ && !take_->snapshot.is_null() && take_->step_next > 0) queue_look_restore(take_->snapshot);
            labels_live_ = false; labels_.clear(); lens_ = {};
            rig_.forget(); guides_.forget(); callouts_.forget(); scene_.forget(); breakdown_.forget(); breakdown_opponent_.forget();
            take_.reset(); samples_.clear(); mode_ = Mode::Idle; start_pending_ = false;
            report("The world changed (loading, travel or death); CINE stopped and the game camera is back.");
            return;
        }
        if (!look_restore_.is_null()) retry_look_restore(dt);
        // Clean scene and breakdown teardown keep their own pace, whatever the mode.
        if (frame_.valid && scene_.pending()) scene_.tick(dt, handle(frame_.pawn), handle(frame_.controller));
        if (breakdown_.busy()) breakdown_.tick();
        if (breakdown_opponent_.busy()) breakdown_opponent_.tick();
        if (take_ && frame_.valid && frame_.pawn != take_->pawn && (mode_ == Mode::Running || mode_ == Mode::Countdown)) {
            finish_take("The player character changed; the take was cancelled.", true); return;
        }
        if (frame_.in_menu && (mode_ == Mode::Running || mode_ == Mode::Countdown || mode_ == Mode::Preparing)) {
            finish_take("A menu opened; the take was cancelled.", true); return;
        }
        if (capture_pending_ && frame_.valid && !frame_.in_menu) {
            capture_after_ -= dt;
            if (capture_after_ <= 0) { capture_pending_ = false; capture_final_now(); }
        }
        if (start_pending_ && mode_ == Mode::Armed && frame_.valid && !frame_.in_menu) { start_pending_ = false; start_take(); }
        poll_keys();
        switch (mode_) {
            case Mode::RecordWait:
                if (frame_.valid && frame_.ready && !frame_.in_menu) { if (!rig_.resolved()) rig_.resolve(); world_ = frame_.world; keys_previous_.clear(); samples_.clear(); record_clock_ = 0; still_since_ = -1; mode_ = Mode::Recording; report("Recording your route: walk it, then stand still for 3 seconds."); }
                break;
            case Mode::Recording: record_clock_ += dt; record_tick(); break;
            case Mode::Preparing: prepare(); break;
            case Mode::Countdown:
                take_->countdown -= dt;
                if (take_->countdown <= 0) { take_->clock = 0; mode_ = Mode::Running; report(with_note("Rolling.")); }
                break;
            case Mode::Running: take_->clock += dt; run(); break;
            default: break;
        }
    } catch (const std::exception& e) {
        try { if (take_) finish_take(std::string("Take stopped: ") + e.what(), true); else report(e.what(), true); } catch (...) {}
    }
}

void Extension::poll_keys() {
    if (mode_ != Mode::Armed && mode_ != Mode::Recording && mode_ != Mode::Countdown && mode_ != Mode::Running && mode_ != Mode::Preparing) return;
    if (frame_.in_menu || !rig_.resolved()) { keys_previous_.clear(); return; }
    std::vector<bool> down;
    if (!rig_.key_down(hotkeys, down)) return;
    if (keys_previous_.size() != down.size()) keys_previous_.assign(down.size(), true);   // no edge on the first read
    auto pressed = [&](int k) { return down[k] && !keys_previous_[k]; };
    const bool capture = pressed(CaptureKey), take = pressed(TakeKey), cancel = pressed(CancelKey), guide = pressed(GuideKey);
    keys_previous_ = down;
    if (cancel) {
        if (mode_ == Mode::Recording) { mode_ = record_return_; samples_.clear(); report("Route recording cancelled."); return; }
        if (take_) { finish_take("Take cancelled.", false); return; }
        if (mode_ == Mode::Armed) { exit_world("Left Cine World."); return; }
    }
    if (mode_ != Mode::Armed) return;
    if (capture) capture_final_now();
    if (take) start_take();
    if (guide) {
        auto it = std::find(guide_ids.begin(), guide_ids.end(), settings_.guide);
        settings_.guide = (it == guide_ids.end() || std::next(it) == guide_ids.end()) ? guide_ids.front() : *std::next(it);
        save_state(); host_.invalidate();
    }
}

// ---------------------------------------------------------------- Cine World

void Extension::enter() {
    if (mode_ != Mode::Idle) return;
    if (!frame_.valid || !frame_.ready) throw std::runtime_error("Load into the world first");
    rig_.resolve();
    world_ = frame_.world;
    rig_.hide_hud();
    keys_previous_.clear();
    mode_ = Mode::Armed;
    pak_missing_ = !rig_.asset_present("/Game/CSSX/CINE/MPC_Cine.MPC_Cine");
    apply_scene();
    try { host_.request({{"op", "menu.close"}}); } catch (...) {}
    report("In Cine World. Frame your shot, then press F8 to roll. F9 leaves.");
}

void Extension::exit_world(const std::string& why) {
    if (take_) finish_take(why, false);
    std::vector<std::string> failures;
    try { failures = rig_.restore_all(); } catch (const std::exception& e) { failures.push_back(e.what()); }
    try { for (auto& f : scene_.restore()) failures.push_back(std::move(f)); } catch (const std::exception& e) { failures.push_back(e.what()); }
    guides_.hide(); callouts_.hide();
    mode_ = Mode::Idle; start_pending_ = false; capture_pending_ = false; samples_.clear();
    if (failures.empty()) report(why);
    else report(why + " Some settings could not be restored: " + failures.front(), true);
}

void Extension::start_take() {
    if (mode_ != Mode::Armed) return;
    if (!current_preset()) throw std::runtime_error("No preset is available");
    // A look from the last take still waiting to go back would become the new "own look".
    if (!look_restore_.is_null()) {
        if (!restore_look(look_restore_)) throw std::runtime_error("Still putting your look back from the last take; try again in a moment");
        look_restore_ = nullptr;
    }
    take_ = std::make_unique<Take>();
    take_->world = frame_.world; take_->pawn = frame_.pawn;
    mode_ = Mode::Preparing;
    report("Preparing the take...");
}

Json Extension::look_payload(const LookStep& step, const Json& describe) const {
    Json commands = Json::array();
    std::map<std::string, std::string> kinds;
    for (const auto& c : describe.value("controls", Json::array())) kinds[c.value("id", std::string())] = c.value("kind", std::string());
    bool palette_known = step.palette.empty();
    for (const auto& p : describe.value("palettes", Json::array())) if (p.value("id", std::string()) == step.palette) palette_known = true;
    if (!step.palette.empty() && palette_known) commands.push_back({{"action", "palette"}, {"palette", step.palette}});
    for (const auto& [id, values] : step.controls) {
        const auto kind = kinds.find(id);
        if (kind == kinds.end()) continue;   // this outfit has no such part: skip, one preset serves many outfits
        if (kind->second == "color" && values.size() == 3) commands.push_back({{"action", "control"}, {"control", id}, {"rgb", values}});
        else for (size_t ch = 0; ch < values.size(); ++ch) commands.push_back({{"action", "control"}, {"control", id}, {"channel", ch}, {"value", values[ch]}});
    }
    if (commands.empty()) return Json();
    return {{"action", "apply"}, {"persist", false}, {"commands", std::move(commands)}};
}

void Extension::prepare() {
    auto& t = *take_;
    if (t.prepare_step == 0) {
        if (!rig_.resolved()) rig_.resolve();
        t.preset = *current_preset();
        auto& p = t.preset;
        p.zoom *= settings_.zoom; p.fov_scale *= settings_.fov_scale;
        for (auto& k : p.keys) { k.height += settings_.height; k.aim += settings_.height; }
        for (auto& s : p.photos) { s.height += settings_.height; s.aim += settings_.height; s.distance *= settings_.zoom; s.fov *= settings_.fov_scale; }
        if (p.kind == ShotKind::Walk) {
            if (!route_) throw std::runtime_error("Record a route first (Route > Record route)");
            t.route = final_ ? branch_into_final(*route_, *final_) : *route_;
            t.track = std::make_unique<Track>(*t.route);
            t.timeline = make_timeline(p, t.route->length(), walk_cm_s);
            t.base = t.route->start; t.base_yaw = t.route->start_yaw;
        } else if (p.kind == ShotKind::Action) {
            auto& a = p.action;
            a.distance *= p.zoom; a.distance_max *= p.zoom; a.fov *= p.fov_scale;
            a.height += settings_.height; a.aim += settings_.height;
            t.timeline = make_timeline(p, 0, walk_cm_s);
            t.base = her_position(); t.base_yaw = frame_.yaw;
        } else {
            t.timeline = make_timeline(p, 0, walk_cm_s);
            t.from_head = p.reference == Preset::Reference::Head;
            t.base = t.from_head ? rig_.head_position() : her_position();
            t.base_yaw = frame_.yaw;
        }
        if (p.kind != ShotKind::Photo && p.kind != ShotKind::Action) t.curve = std::make_unique<OrbitCurve>(t.timeline.keys);
        // Breakdown track: the preset's own steps, or the built-in reel.
        if (settings_.breakdown == "preset") t.reveals = t.timeline.breakdown;
        else if (settings_.breakdown == "reel") t.reveals = reel(t.timeline.total);
        for (const auto& r : t.reveals) t.layers |= r.layers;
        if (settings_.breakdown == "preset" && t.reveals.empty()) t.note = "Breakdown off: this preset has no breakdown steps.";
        // Clay takes her material slots; a palette change mid-take would fight it.
        if (t.layers && settings_.look != "off") t.note = "Look track off while a breakdown runs.";
        // Look track: CSS answers describe, or the take runs without it and says why.
        if (settings_.look != "off" && !t.layers) {
            try {
                const auto d = rig_.css({{"action", "describe"}});
                t.snapshot = d.at("customize");
                std::vector<LookStep> auto_steps;
                const std::vector<LookStep>* steps = settings_.look == "preset" ? &p.look : nullptr;
                if (settings_.look == "palettes") {
                    std::vector<std::string> ids;
                    for (const auto& pal : d.value("palettes", Json::array())) if (pal.value("id", std::string()) != "original") ids.push_back(pal.at("id"));
                    for (size_t i = 0; i < ids.size(); ++i) { LookStep s; s.fraction = 0.08 + 0.8 * double(i) / double(std::max<size_t>(1, ids.size())); s.palette = ids[i]; auto_steps.push_back(s); }
                    steps = &auto_steps;
                }
                const double span = p.kind == ShotKind::Walk ? t.timeline.walk_time : t.timeline.total;
                const double start = p.kind == ShotKind::Walk ? t.timeline.walk_start : 0;
                if (steps) for (const auto& s : *steps) { auto pay = look_payload(s, d); if (!pay.is_null()) t.steps.emplace_back(start + s.fraction * span, std::move(pay)); }
                if (!t.steps.empty() && p.look_ends_on_own) {
                    const double own_at = p.kind == ShotKind::Walk ? t.timeline.arrive - 2.0 : t.timeline.total * 0.92;
                    t.steps.emplace_back(own_at, Json{{"action", "apply"}, {"persist", false}, {"commands", Json::array({{{"action", "restore"}, {"customize", t.snapshot}}})}});
                }
                std::sort(t.steps.begin(), t.steps.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                t.look = !t.steps.empty();
                if (!t.look && settings_.look == "preset") t.note = "Look track off: this preset has no look steps.";
            } catch (const std::exception& e) { t.look = false; t.note = std::string("Look track off: ") + e.what() + "."; report(t.note); }
        }
        if (p.kind == ShotKind::Walk) rig_.place_player(t.route->start, t.route->start_yaw);
        t.wall_raw.clear(); t.wall_next = 0;
        t.prepare_step = settings_.walls && t.curve ? 1 : 2;
        return;
    }
    if (t.prepare_step == 1) {
        // Wall check, a slice per tick: where the curve would meet a wall, ease the camera in.
        const size_t total = size_t(t.timeline.total / wall_step) + 2;
        for (size_t n = 0; n < traces_per_tick && t.wall_next < total; ++n, ++t.wall_next) {
            const double when = double(t.wall_next) * wall_step;
            Vec3 base = t.base; double yaw = t.base_yaw;
            if (t.track) {
                const double s = std::max(0.0, when - t.timeline.walk_start) * walk_cm_s;
                base = t.track->at(s); yaw = t.track->heading_at(s);
            }
            const Pose pose = t.curve->pose(when);
            const Vec3 cam = to_world(base, yaw, pose.offset);
            const Vec3 chest = base + Vec3{0, 0, t.from_head ? 0.0 : 40.0};
            double hit_at = 0;
            const double reach = std::max(1.0, std::hypot(std::hypot(cam.x - chest.x, cam.y - chest.y), cam.z - chest.z));
            t.wall_raw.push_back(rig_.trace(chest, cam, &hit_at) ? std::max(0.4, (hit_at - 50) / reach) : 1.0);
        }
        if (t.wall_next >= total) { t.walls = WallProfile(wall_step, t.wall_raw); t.prepare_step = 2; }
        return;
    }
    if (t.prepare_step == 3) {
        // The breakdown builds its pieces a slice per tick; roll when they are all there.
        t.build_wait += tick_dt_;
        const bool built = (!breakdown_.active() || breakdown_.ready()) && (!breakdown_opponent_.active() || breakdown_opponent_.ready());
        if (!built && t.build_wait < build_timeout) return;
        if (!breakdown_.note().empty()) t.note = t.note.empty() ? breakdown_.note() : t.note + "  " + breakdown_.note();
        begin_countdown();
        return;
    }
    // Cameras, gait, view, then the countdown.
    if (t.preset.kind == ShotKind::Action) {
        const auto& a = t.preset.action;
        t.subject_f = her_position();
        // Follow framing counts its angle from the world; start it from where she faces.
        t.azimuth = a.framing == Framing::Follow ? frame_.yaw + a.azimuth : a.azimuth;
        t.beat_ready.assign(a.beats.size(), 0.0);
        t.opponent = rig_.locked_target();
        Vec3 where; const Vec3* target = nullptr;
        if (is_handle(t.opponent)) {
            t.had_opponent = true;
            try { where = rig_.actor_location(t.opponent); t.opponent_f = where; t.opponent_seen = true; target = &t.opponent_f; } catch (...) {}
            rig_.hide_target_ui(t.opponent);
        }
        ActionFrame f{a.framing, t.azimuth, a.distance, a.distance_max, a.margin, a.height, a.aim, a.fov, 0};
        const auto view = action_view(f, t.subject_f, target);
        t.dolly = rig_.spawn_camera(view.camera, look_at(view.camera, view.aim), view.fov);
        t.fov_applied = view.fov;
        if (a.shake >= 0) rig_.shake_scale(a.shake);
        if (a.fstop > 0) { rig_.depth_of_field(t.dolly, a.fstop, view.focus); t.focus_applied = view.focus; }
    } else if (t.curve) {
        const Pose pose = t.curve->pose(0, t.walls.at(0));
        const Vec3 cam = to_world(t.base, t.base_yaw, pose.offset);
        t.dolly = rig_.spawn_camera(cam, look_at(cam, to_world(t.base, t.base_yaw, pose.aim)), pose.fov);
        t.fov_applied = pose.fov;
    } else {
        const auto& s = t.preset.photos.front();
        const double rad = s.azimuth * std::numbers::pi / 180;
        const Vec3 cam = to_world(t.base, t.base_yaw, {s.distance * std::cos(rad), s.distance * std::sin(rad), s.height});
        t.dolly = rig_.spawn_camera(cam, look_at(cam, to_world(t.base, t.base_yaw, {0, 0, s.aim})), s.fov);
        t.photo = 0;
    }
    if (t.preset.kind == ShotKind::Walk && final_) {
        const Rotator r{final_->pitch, final_->camera_yaw, 0};
        t.final_close = rig_.spawn_attached(final_->camera, r, final_->fov);
        // Dolly back level: a low shot backed off along its view line can sink under water.
        const double back = final_->camera_yaw * std::numbers::pi / 180;
        t.final_wide = rig_.spawn_attached(final_->camera - Vec3{90 * std::cos(back), 90 * std::sin(back), 0}, r, final_->fov);
    }
    if (t.preset.kind == ShotKind::Walk) rig_.force_walk();   // before the take: no stall at the walk start
    t.yaw_f = frame_.yaw; t.vel_f = {};
    t.ctl_yaw = t.route ? t.route->start_yaw : frame_.yaw;
    rig_.view(t.dolly, 0.0);
    if (t.layers) {
        breakdown_.begin(rig_.pawn(), rig_.camera_component(t.dolly), t.layers, spring_bones());
        if (settings_.breakdown_target) {
            const auto other = is_handle(t.opponent) ? t.opponent : rig_.locked_target();
            // The frame passes cover the whole picture already; the second character gets the rest.
            if (is_handle(other)) breakdown_opponent_.begin(other, Json(), t.layers & ~frame_layers, {});
        }
        t.prepare_step = 3;
        report("Building the breakdown layers...");
        return;
    }
    begin_countdown();
}

void Extension::begin_countdown() {
    take_->countdown = settings_.countdown;
    mode_ = Mode::Countdown;
    report(with_note(settings_.countdown > 0 ? "Get ready: start your recorder." : "Rolling."));
}

// The built-in reel: the final image, then one layer after another, then the final image again.
std::vector<Timeline::Reveal> Extension::reel(double total) const {
    const LayerSet clay = bit(Layer::Clay), wire = bit(Layer::Wireframe), bones = bit(Layer::Skeleton);
    const std::pair<double, std::pair<LayerSet, double>> steps[]{
        {0.07, {clay, 1.2}},
        {0.17, {clay | wire, 1.2}},
        {0.29, {clay | bones, 1.2}},
        {0.40, {clay | bones | bit(Layer::VirtualBones) | bit(Layer::IkBones), 0.9}},
        {0.50, {clay | bones | bit(Layer::Springs), 0.9}},
        {0.60, {bit(Layer::Collision) | bit(Layer::Capsule), 0.8}},
        {0.70, {bit(Layer::BaseColor), 1.2}},
        {0.78, {bit(Layer::Normals), 1.0}},
        {0.85, {bit(Layer::Depth), 1.0}},
        {0.92, {0, 1.4}},
    };
    std::vector<Timeline::Reveal> out;
    for (const auto& [fraction, step] : steps) out.push_back({fraction * total, step.first, std::min(step.second, total * 0.06)});
    return out;
}

// The worn outfit's own spring bones, when its wardrobe mod says which they are. Empty means
// "go by the usual names".
std::vector<std::string> Extension::spring_bones() {
    std::vector<std::string> bones;
    try {
        const auto described = rig_.css({{"action", "describe"}});
        for (const auto& control : described.value("controls", Json::array())) {
            const auto kind = control.value("kind", std::string());
            if (kind != "spring" && kind != "dynamics" && kind != "rig") continue;
            for (const auto& node : control.value("nodes", Json::array())) if (node.is_string() && bones.size() < 256) bones.push_back(node.get<std::string>());
        }
    } catch (...) {}
    return bones;
}

// 10 Hz while a breakdown shows: where each labelled component sits relative to her, and a
// fresh measure of the lens (the field of view changes along a shot).
void Extension::sample_labels() {
    const bool wanted = settings_.labels && breakdown_.active() && breakdown_.shown() != 0;
    if (!wanted) { labels_live_ = false; labels_.clear(); return; }
    try {
        const auto& callouts = breakdown_.callouts();
        const size_t count = std::min(callouts.size(), max_callouts);
        std::vector<Label> next(count);
        for (size_t i = 0; i < count; ++i) {
            next[i].text = callouts[i].text;
            next[i].now = rig_.socket_location(callouts[i].component, callouts[i].socket) - frame_.position;
            // The same label as last time eases from where it was drawn; a new one starts in place.
            const bool same = i < labels_.size() && labels_[i].text == next[i].text;
            next[i].before = same ? labels_[i].now : next[i].now;
        }
        labels_ = std::move(next);
        label_age_ = 0;
        Vec3 camera; Rotator rotation;
        double cx = 0, cy = 0, ox = 0, oy = 0;
        if (rig_.view_point(camera, rotation)) {
            const auto basis = view_basis(rotation);
            if (rig_.project(lens_point(camera, basis, false), cx, cy) && rig_.project(lens_point(camera, basis, true), ox, oy)) lens_ = measure_lens(cx, cy, ox, oy);
        }
        labels_live_ = true;
    } catch (...) { labels_live_ = false; }
}

void Extension::apply_scene() {
    const auto mode = settings_.scene == "enemies" ? Scene::Mode::Enemies : settings_.scene == "everyone" ? Scene::Mode::Everyone : Scene::Mode::Off;
    scene_.want(mode, settings_.keep_target);
}

void Extension::fire(Beat::On what) {
    auto& t = *take_;
    const auto& a = t.preset.action;
    for (size_t i = 0; i < a.beats.size(); ++i) {
        if (a.beats[i].on != what || t.clock < t.beat_ready[i]) continue;
        t.beat_ready[i] = t.clock + a.beats[i].cooldown;
        if (a.beats[i].what == Beat::Do::Slowmo) t.slow_until = t.clock + a.slowmo_hold;
        else t.punch_until = t.clock + 1.1;
    }
}

// One 10 Hz step of an action take: where the fighters are, how the fight is going, and where
// the camera goes next. About six bridge calls.
void Extension::action_tick(double dt) {
    auto& t = *take_;
    const auto& a = t.preset.action;
    ++t.poll;
    // A new lock-on target becomes the opponent.
    if (t.poll % 5 == 1) {
        const auto locked = rig_.locked_target();
        if (is_handle(locked) && (!is_handle(t.opponent) || locked["$object"] != t.opponent["$object"])) {
            t.opponent = locked; t.had_opponent = true; t.opponent_down = false; t.opponent_seen = false; t.opponent_health = -1; t.opponent_down_at = -1;
            rig_.hide_target_ui(locked);
        }
    }
    const double k = a.smooth > 0 ? smooth_factor(dt, a.smooth, 0.25) : 1.0;
    t.subject_f = t.subject_f + (her_position() - t.subject_f) * k;
    const Vec3* target = nullptr;
    if (is_handle(t.opponent) && !t.opponent_down) {
        try {
            const Vec3 at = rig_.actor_location(t.opponent);
            t.opponent_f = t.opponent_seen ? t.opponent_f + (at - t.opponent_f) * k : at;
            t.opponent_seen = true;
        } catch (...) { t.opponent_down = true; t.opponent_down_at = t.clock; }
    }
    if (t.opponent_seen) target = &t.opponent_f;   // after it falls the camera stays on the spot
    // The fight, on alternate ticks.
    if (t.poll % 2 == 0) {
        if (is_handle(t.opponent) && !t.opponent_down) {
            const double health = rig_.health_fraction(t.opponent);
            if (t.opponent_health >= 0 && health >= 0 && health < t.opponent_health - 0.004) fire(Beat::On::Hit);
            if (health >= 0) t.opponent_health = health;
            if (rig_.dead(t.opponent)) { t.opponent_down = true; t.opponent_down_at = t.clock; fire(Beat::On::Kill); }
        }
    } else {
        const double health = rig_.health_fraction(rig_.pawn());
        if (t.player_health >= 0 && health >= 0 && health < t.player_health - 0.004) fire(Beat::On::Hurt);
        if (health >= 0) t.player_health = health;
        const int parry = rig_.tag_count("State.Parry");
        if (parry > 0 && t.parry_count == 0) fire(Beat::On::Parry);
        t.parry_count = parry;
        if (health == 0) { finish_take("The take ended: she fell.", false); return; }
    }
    // Slow motion and the punch-in ease toward where they should be.
    const double want = t.clock < t.slow_until ? a.slowmo : 1.0;
    t.scale += (want - t.scale) * 0.55;
    if (std::abs(t.scale - want) < 0.02) t.scale = want;
    // Written in steps while it eases, and always once it has arrived (so it ends on exactly 1).
    if (t.scale != t.scale_applied && (std::abs(t.scale - t.scale_applied) > 0.015 || t.scale == want)) { rig_.time_scale(t.scale); t.scale_applied = t.scale; }
    t.punch += ((t.clock < t.punch_until ? 1.0 : 0.0) - t.punch) * 0.35;
    t.azimuth = wrap_degrees(t.azimuth + a.orbit_speed * dt);
    ActionFrame f{a.framing, t.azimuth, a.distance, a.distance_max, a.margin, a.height, a.aim, a.fov, t.punch};
    const auto view = action_view(f, t.subject_f, target);
    // The game's own cameras (finishers, grabs): let them play, then take the view back.
    if (!rig_.viewing(t.dolly)) {
        const bool games = a.respect_game_cameras && rig_.tag_count("State.CustomCamera") > 0;
        if (games) return;
        if (t.clock >= t.reassert_at) { rig_.view(t.dolly, 0.35); t.reassert_at = t.clock + 1.0; }
    }
    // The engine interpolates the move on game time: scale it so it still lands in real time.
    const double lead = std::max(0.25, a.smooth + 0.15);
    rig_.move_camera(t.dolly, view.camera, look_at(view.camera, view.aim), (lead + 0.05) * t.scale);
    if (std::abs(view.fov - t.fov_applied) > 0.15) { rig_.set_fov(t.dolly, view.fov); t.fov_applied = view.fov; }
    if (a.fstop > 0 && std::abs(view.focus - t.focus_applied) > 12) { rig_.depth_of_field(t.dolly, a.fstop, view.focus); t.focus_applied = view.focus; }
    if (t.had_opponent && t.opponent_down && t.clock >= t.opponent_down_at + a.tail) finish_take("Take finished: the target is down.", false);
}

void Extension::steer(double dt) {
    auto& t = *take_;
    const auto& pts = t.route->points;
    const Vec3 here = her_position();
    if (t.target + 1 < pts.size() && flat_distance(here, pts[t.target - 1]) < 140) {
        ++t.target;
        rig_.move_to(pts[t.target]);
    }
    const double left = flat_distance(here, pts.back());
    t.speed += (walk_cm_s - t.speed) * std::min(1.0, dt * 2.5);
    double speed = t.speed;
    if (t.target + 1 == pts.size() && left < 220) speed = std::min(speed, std::max(110.0, walk_cm_s * left / 220));   // settle, do not brake
    if (std::abs(speed - t.speed_applied) > 2) { rig_.speed_limit(speed); t.speed_applied = speed; }
    const double v = std::hypot(frame_.velocity.x, frame_.velocity.y);
    if (v > 60) {
        // She faces the control rotation while moving: point it down her velocity, smoothed.
        const double want = std::atan2(frame_.velocity.y, frame_.velocity.x) * 180 / std::numbers::pi;
        t.ctl_yaw += wrap_degrees(want - t.ctl_yaw) * std::min(1.0, dt * 3.0);
        rig_.control_yaw(t.ctl_yaw);
    }
    if (t.target + 1 == pts.size() && left < 40) { rig_.stop_moving(); t.walking = false; t.arrived = true; return; }
    // A controller blip or a nudge cancels path following: resume toward the same goal.
    if (v < 20) {
        if (t.stalled_at < 0) t.stalled_at = t.clock;
        else if (t.clock - t.stalled_at > 0.4) { rig_.move_to(pts[t.target]); t.stalled_at = -1; }
    } else t.stalled_at = -1;
}

void Extension::place_dolly(double dt, bool teleport) {
    auto& t = *take_;
    const double lead = t.preset.lead;
    const double a_yaw = teleport ? 1.0 : smooth_factor(dt, yaw_tau), a_vel = teleport ? 1.0 : smooth_factor(dt, vel_tau);
    t.yaw_f += wrap_degrees(frame_.yaw - t.yaw_f) * a_yaw;
    t.vel_f = t.vel_f + (frame_.velocity - t.vel_f) * a_vel;
    Vec3 base = t.base; double yaw = t.base_yaw;
    if (t.track) {
        // Lead along the route curve from where she really is; height from the route
        // (smooth through stairs), not from her stepping capsule.
        const Vec3 here = her_position();
        const double s_now = t.track->project(here);
        const double ds = std::hypot(t.vel_f.x, t.vel_f.y) * lead;
        const Vec3 on = t.track->at(s_now), ahead = t.track->at(s_now + ds);
        base = here + (ahead - on); base.z = ahead.z;
        yaw = t.yaw_f;
    }
    const double when = t.clock + lead;
    const Pose pose = t.curve->pose(when, t.walls.at(when));
    const Vec3 cam = to_world(base, yaw, pose.offset);
    const Rotator rot = look_at(cam, to_world(base, yaw, pose.aim));
    if (teleport) rig_.place_camera(t.dolly, cam, rot);
    else rig_.move_camera(t.dolly, cam, rot, lead + 0.05);
    if (std::abs(pose.fov - t.fov_applied) > 0.15) { rig_.set_fov(t.dolly, pose.fov); t.fov_applied = pose.fov; }
}

void Extension::run() {
    auto& t = *take_;
    const auto& tl = t.timeline;
    const bool walk = t.preset.kind == ShotKind::Walk;
    if (walk && !t.walking && !t.arrived && t.clock >= tl.walk_start) {
        t.target = std::min<size_t>(2, t.route->points.size() - 1);
        t.speed = start_cm_s; t.speed_applied = -1;
        rig_.speed_limit(start_cm_s);
        rig_.move_to(t.route->points[t.target]);
        t.walking = true;
    }
    if (t.walking) steer(frame_.seconds > 0 ? std::max(frame_.seconds, 0.05) : 0.1);
    while (t.reveal_next < t.reveals.size() && t.clock >= t.reveals[t.reveal_next].at) {
        const auto& r = t.reveals[t.reveal_next++];
        breakdown_.show(r.layers, r.wipe);
        if (breakdown_opponent_.active()) breakdown_opponent_.show(r.layers & ~frame_layers, r.wipe);
    }
    if (!t.reveals.empty()) sample_labels();
    if (t.preset.kind == ShotKind::Action) {
        action_tick(tick_dt_);
        if (!take_) return;   // the take ended inside the step
    }
    while (t.look && t.step_next < t.steps.size() && t.clock >= t.steps[t.step_next].first) {
        try { rig_.css(t.steps[t.step_next].second); }
        catch (const std::exception& e) { t.look = false; t.note = std::string("Look track stopped: ") + e.what() + "."; report(t.note); }
        ++t.step_next;
    }
    if (walk && final_) {
        if (!t.cut_wide && t.clock >= tl.arrive - 1.2) { rig_.view(t.final_wide, 3.5); t.cut_wide = true; }
        if (!t.cut_close && t.clock >= tl.arrive + 2.8) { rig_.view(t.final_close, std::max(0.5, tl.total - (tl.arrive + 2.8) - 0.6)); t.cut_close = true; }
    }
    if (t.preset.kind == ShotKind::Photo) {
        const int idx = std::min<int>(int(t.preset.photos.size()) - 1, int(t.clock / t.preset.hold));
        if (idx != t.photo) {
            const auto& s = t.preset.photos[size_t(idx)];
            const double rad = s.azimuth * std::numbers::pi / 180;
            const Vec3 cam = to_world(t.base, t.base_yaw, {s.distance * std::cos(rad), s.distance * std::sin(rad), s.height});
            rig_.place_camera(t.dolly, cam, look_at(cam, to_world(t.base, t.base_yaw, {0, 0, s.aim})));
            rig_.set_fov(t.dolly, s.fov);
            t.photo = idx;
            report("Photo " + std::to_string(idx + 1) + " of " + std::to_string(t.preset.photos.size()) + ": " + s.name);
        }
    } else if (t.preset.kind != ShotKind::Action && (!walk || !final_ || t.clock < tl.arrive + 4.0)) {
        place_dolly(0.1, false);
    }
    if (t.clock >= tl.total) finish_take("Take finished.", false);
}

// Put the player's own look back and let CSS save it (persist=true). False when CSS refused.
bool Extension::restore_look(const Json& snapshot) {
    try { rig_.css({{"action", "apply"}, {"persist", true}, {"commands", Json::array({{{"action", "restore"}, {"customize", snapshot}}})}}); return true; }
    catch (const std::exception& e) { look_restore_error_ = e.what(); return false; }
}

void Extension::queue_look_restore(const Json& snapshot) {
    look_restore_ = snapshot; look_restore_wait_ = 0; look_restore_left_ = look_restore_window;
}

void Extension::retry_look_restore(double dt) {
    look_restore_wait_ -= dt; look_restore_left_ -= dt;
    if (look_restore_wait_ > 0) return;
    look_restore_wait_ = 1.0;
    if (restore_look(look_restore_)) { look_restore_ = nullptr; report("Your own look is back."); return; }
    if (look_restore_left_ <= 0) {
        look_restore_ = nullptr;
        report("Could not put your look back (" + look_restore_error_ + "). Pick your palette again on the CSS page.", true);
    }
}

void Extension::finish_take(const std::string& why, bool failed) {
    if (!take_) return;
    std::vector<std::string> failures;
    // Breakdown first: its frame passes sit on the take camera, which the rig destroys next.
    for (Breakdown* b : {&breakdown_, &breakdown_opponent_}) {
        try { for (auto& f : b->end()) failures.push_back(std::move(f)); } catch (const std::exception& e) { failures.push_back(e.what()); }
    }
    callouts_.hide(); labels_live_ = false; labels_.clear(); lens_ = {};
    try { for (auto& f : rig_.restore_take()) failures.push_back(std::move(f)); } catch (const std::exception& e) { failures.push_back(e.what()); }
    if (!take_->snapshot.is_null() && take_->step_next > 0) {
        // Put the modder's own look back and let CSS save it (persist=true).
        if (!restore_look(take_->snapshot)) queue_look_restore(take_->snapshot);   // CSS busy or between looks: retried each second
    }
    const std::string note = take_->note;
    take_.reset();
    mode_ = Mode::Armed;
    keys_previous_.clear();
    if (!failures.empty()) report(why + " Could not restore: " + failures.front(), true);
    else report(note.empty() ? why : why + "  " + note, failed);
}

// ---------------------------------------------------------------- route and final shot

void Extension::record_tick() {
    if (!frame_.valid || frame_.in_menu) return;
    const double speed = std::hypot(frame_.velocity.x, frame_.velocity.y);
    samples_.push_back({record_clock_, frame_.position, frame_.yaw, speed});
    double walked = 0;
    for (size_t i = 1; i < samples_.size(); ++i) walked += flat_distance(samples_[i - 1].position, samples_[i].position);
    if (speed < 20) { if (still_since_ < 0) still_since_ = record_clock_; } else still_since_ = -1;
    const bool done = (walked >= 300 && still_since_ >= 0 && record_clock_ - still_since_ >= 3.0) || samples_.size() >= max_samples;
    if (!done) return;
    mode_ = record_return_;
    try {
        auto route = extract_route(samples_);
        if (route.points.size() > max_route_points) throw std::invalid_argument("The route is too long; record a shorter walk");
        route_ = std::move(route);
        save_state();
        report("Route saved: " + std::to_string(int(route_->length() / 100)) + " m. Press F7 where the last shot should be, after framing it.");
    } catch (const std::exception& e) { report(e.what(), true); }
    samples_.clear();
}

void Extension::capture_final_now() {
    if (!frame_.valid || !frame_.ready) throw std::runtime_error("Load into the world first");
    if (!rig_.resolved()) rig_.resolve();
    final_ = rig_.capture_final(her_position(), frame_.yaw);
    save_state();
    report("Final shot saved from the current view.");
}

// ---------------------------------------------------------------- menu

Json Extension::model() const {
    Json options = Json::array();
    for (const auto& [id, p] : presets_) options.push_back({{"id", id}, {"label", p.name}});
    if (options.empty()) options.push_back({{"id", "none"}, {"label", "No presets found"}});
    const auto* p = current_preset();
    std::string route = route_ ? std::to_string(int(route_->length() / 100)) + " m route saved" : "No route recorded";
    std::string final_shot = final_ ? "Final shot saved" : "No final shot (the walk ends on the last camera key)";
    std::string look = settings_.look == "off" ? "Look track off" : settings_.look == "palettes" ? "Cycles every palette, then your own look" :
                       (p && !p->look.empty() ? "Uses this preset's look steps" : "This preset has no look steps");
    std::string scene = settings_.scene == "off" ? "Enemies and NPCs stay as they are."
                      : !scene_.status().empty() && mode_ != Mode::Idle ? scene_.status()
                      : settings_.scene == "enemies" ? "Enemies are hidden while you are in Cine World." : "Enemies and NPCs are hidden while you are in Cine World.";
    std::string breakdown = settings_.breakdown == "off" ? "Breakdown off"
                          : settings_.breakdown == "reel" ? "Final image, clay, wireframe, skeleton, bones, collision, frame passes, final image"
                          : (p && !p->breakdown.empty() ? "Uses this preset's breakdown steps" : "This preset has no breakdown steps");
    if (pak_missing_ && settings_.breakdown != "off")
        breakdown = "CINE's asset pak is not installed (see the README): only physics bodies and the capsule can show.";
    const bool recording = mode_ == Mode::Recording || mode_ == Mode::RecordWait;
    const bool in_world = recording ? record_return_ == Mode::Armed : mode_ != Mode::Idle;
    const bool busy = mode_ == Mode::Preparing || mode_ == Mode::Countdown || mode_ == Mode::Running;
    Json disabled = Json::object();
    if (in_world) disabled["enter"] = "Already in Cine World";
    if (!in_world) { disabled["exit"] = "Not in Cine World"; disabled["take"] = "Enter Cine World first"; }
    if (p && p->kind == ShotKind::Walk && !route_) disabled["take"] = "Record a route first";
    if (settings_.scene == "off") disabled["keep_target"] = "Turn clean scene on first";
    if (settings_.breakdown == "off") { disabled["labels"] = "Turn the breakdown on first"; disabled["breakdown_target"] = "Turn the breakdown on first"; }
    if (recording && !in_world) disabled["enter"] = "Finish the recording first";
    if (recording) { disabled["record"] = "Recording now"; disabled["take"] = "Finish the recording first"; disabled["capture"] = "Finish the recording first"; }
    if (busy) { disabled["record"] = "A take is running"; disabled["capture"] = "A take is running"; disabled["take"] = "A take is running"; }
    if (!route_) disabled["clear_route"] = "No route";
    if (!final_) disabled["clear_final"] = "No final shot";
    Json enabled = Json::object();
    for (const auto& id : {"enter", "exit", "take", "record", "capture", "clear_route", "clear_final", "keep_target", "labels", "breakdown_target"}) enabled[id] = !disabled.contains(id);
    std::string summary = status_;
    if (!preset_problems_.empty()) summary += "  (" + std::to_string(preset_problems_.size()) + " preset file(s) skipped; see CSSX log)";
    return {{"values", {{"status", summary}, {"preset", presets_.empty() ? std::string("none") : settings_.preset},
                        {"preset_info", p ? p->description : std::string()}, {"zoom", settings_.zoom}, {"height", settings_.height},
                        {"fov_scale", settings_.fov_scale}, {"countdown", settings_.countdown}, {"guide", settings_.guide},
                        {"walls", settings_.walls}, {"look", settings_.look}, {"look_info", look}, {"route_info", route}, {"final_info", final_shot},
                        {"scene", settings_.scene}, {"keep_target", settings_.keep_target}, {"scene_info", scene},
                        {"breakdown", settings_.breakdown}, {"labels", settings_.labels}, {"breakdown_target", settings_.breakdown_target}, {"breakdown_info", breakdown}}},
            {"options", {{"preset", options}}},
            {"enabled", enabled}, {"disabled", disabled}, {"status", summary}, {"error", error_}};
}

void Extension::event(const Json& e) {
    const auto id = e.value("id", std::string());
    try {
        error_.clear();
        if (id == "enter") enter();
        else if (id == "exit") exit_world("Left Cine World.");
        else if (id == "take") { if (mode_ != Mode::Armed) throw std::runtime_error("Enter Cine World first"); start_pending_ = true; host_.request({{"op", "menu.close"}}); report("Closing the menu; the take starts in a moment."); }
        else if (id == "record") {
            if (mode_ == Mode::Recording || mode_ == Mode::RecordWait) return;
            if (mode_ != Mode::Idle && mode_ != Mode::Armed) throw std::runtime_error("Wait for the take to finish");
            record_return_ = mode_; mode_ = Mode::RecordWait;
            host_.request({{"op", "menu.close"}}); report("Close the menu, walk your route, then stand still for 3 seconds.");
        }
        else if (id == "capture") {
            if (mode_ != Mode::Idle && mode_ != Mode::Armed) throw std::runtime_error("Wait for the take to finish");
            capture_pending_ = true; capture_after_ = 1.5; host_.request({{"op", "menu.close"}}); report("Saving the game camera's view 1.5 s after the menu closes.");
        }
        else if (id == "clear_route") { route_.reset(); save_state(); report("Route cleared."); }
        else if (id == "clear_final") { final_.reset(); save_state(); report("Final shot cleared."); }
        else if (id == "reload") { load_presets(); report("Presets reloaded: " + std::to_string(presets_.size()) + "."); }
        else if (id == "preset") { const auto v = e.at("value").get<std::string>(); if (!presets_.contains(v)) throw std::runtime_error("Unknown preset"); settings_.preset = v; save_state(); }
        else if (id == "zoom") { settings_.zoom = clampd(e.at("value").get<double>(), 0.5, 2.0); save_state(); }
        else if (id == "height") { settings_.height = clampd(e.at("value").get<double>(), -150, 150); save_state(); }
        else if (id == "fov_scale") { settings_.fov_scale = clampd(e.at("value").get<double>(), 0.5, 2.0); save_state(); }
        else if (id == "countdown") { settings_.countdown = clampd(e.at("value").get<double>(), 0, 10); save_state(); }
        else if (id == "guide") { const auto v = e.at("value").get<std::string>(); if (!std::count(guide_ids.begin(), guide_ids.end(), v)) throw std::runtime_error("Unknown guide"); settings_.guide = v; save_state(); }
        else if (id == "walls") { settings_.walls = e.at("value").get<bool>(); save_state(); }
        else if (id == "look") { const auto v = e.at("value").get<std::string>(); if (v != "off" && v != "preset" && v != "palettes") throw std::runtime_error("Unknown look mode"); settings_.look = v; save_state(); }
        else if (id == "scene") {
            const auto v = e.at("value").get<std::string>();
            if (!std::count(scene_ids.begin(), scene_ids.end(), v)) throw std::runtime_error("Unknown clean scene mode");
            settings_.scene = v; save_state();
            if (mode_ != Mode::Idle) apply_scene();   // live while in Cine World
        }
        else if (id == "keep_target") { settings_.keep_target = e.at("value").get<bool>(); save_state(); if (mode_ != Mode::Idle) apply_scene(); }
        else if (id == "breakdown") { const auto v = e.at("value").get<std::string>(); if (!std::count(breakdown_ids.begin(), breakdown_ids.end(), v)) throw std::runtime_error("Unknown breakdown mode"); settings_.breakdown = v; save_state(); }
        else if (id == "labels") { settings_.labels = e.at("value").get<bool>(); save_state(); }
        else if (id == "breakdown_target") { settings_.breakdown_target = e.at("value").get<bool>(); save_state(); }
        else throw std::runtime_error("Unknown control: " + id);
        host_.invalidate();
    } catch (const std::exception& ex) {
        error_ = ex.what();
        try { host_.invalidate(); } catch (...) {}
        throw;
    }
}

Json Extension::status() const {
    const bool active = mode_ != Mode::Idle;
    return {{"summary", active ? status_ : (route_ ? "Ready, route saved" : "Ready")}, {"active", active}};
}

bool Extension::stop() {
    if (stopped_) return true;
    try { exit_world("CINE stopped."); } catch (...) {}
    if (!look_restore_.is_null() && restore_look(look_restore_)) look_restore_ = nullptr;   // last chance before unload
    stopped_ = true;
    return true;   // never hold the DLL: everything owned was attempted
}

} // namespace cine

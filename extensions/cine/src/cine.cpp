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
const std::vector<std::string> picture_ids{"game", "sharp", "full"};
constexpr double own_reach = 75;   // cm from the point the camera aims at: nearer hits are the character's own props
// Her body as a handful of balls: a bone and about how far her flesh reaches from it, in cm.
// Projected, they give the part of her that is in the picture and how wide it is there.
namespace {
struct BodyBall { const char* bone; double radius; };
constexpr BodyBall body_shape[] = {
    {"Head", 11}, {"neck_01", 7}, {"spine_04", 16}, {"spine_02", 14}, {"Pelvis", 17},
    {"upperarm_l", 6}, {"upperarm_r", 6}, {"lowerarm_l", 5}, {"lowerarm_r", 5}, {"Hand_L", 6}, {"Hand_R", 6},
    {"thigh_l", 9}, {"thigh_r", 9}, {"calf_l", 7}, {"calf_r", 7}, {"Foot_L", 7}, {"Foot_R", 7},
    // Her ponytail swings well clear of her body: it is part of what is seen of her.
    {"CSS_Hair_Ponytail_04", 6}, {"CSS_Hair_Ponytail_08", 6}};
}

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
    typeface_.attach(&host_, host_.hud(), host_.context());
    guides_.attach(host_.hud(), host_.context(), &typeface_);
    callouts_.attach(host_.hud(), host_.context(), &typeface_);
    load_presets();
    load_state();
}

// ---------------------------------------------------------------- presets and state

void Extension::load_presets() {
    presets_.clear(); preset_problems_.clear(); packs_.clear(); audio_ids_.clear();
    // Packs are read from tick, one per tick, once the game is up (creation stays passive).
    pack_queue_.clear(); packs_loading_ = true;
    try {
        std::vector<std::string> problems;
        pack_queue_ = find_pack_folders(mods_folder(), problems);
        for (auto& p : problems) preset_problems_.push_back(std::move(p));
    } catch (const std::exception& e) { preset_problems_.push_back(std::string("CINE packs: ") + e.what()); }
    if (pack_queue_.empty()) packs_loading_ = false;
    load_folder_presets();
    finish_presets();
}

// Your own presets: JSON files in this extension's presets folder (copy one from a pack to edit it).
void Extension::load_folder_presets() {
    try {
        const auto info = host_.request({{"op", "extension.info"}});
        const fs::path dir = fs::path(info.at("directory").get<std::string>()) / "presets";
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            const auto name = entry.path().filename().string();
            if (name.size() < 10 || name.substr(name.size() - 10) != ".cine.json") continue;
            try {
                if (entry.file_size(ec) > 256 * 1024) throw std::invalid_argument("file is larger than 256 KiB");
                std::ifstream in(entry.path(), std::ios::binary);
                add_preset(Preset::parse(Json::parse(in)));
            } catch (const std::exception& e) { preset_problems_.push_back(name + ": " + e.what()); }
            if (presets_.size() >= 256) break;
        }
    } catch (const std::exception& e) { preset_problems_.push_back(e.what()); }
}

void Extension::add_preset(Preset preset) {
    if (presets_.contains(preset.id)) {
        const auto& other = presets_.at(preset.id);
        throw std::invalid_argument("id " + preset.id + " is already used by " + (other.pack.empty() ? std::string("a preset file") : "the " + other.pack + " pack"));
    }
    if (!preset.pack.empty()) preset_order_.push_back(preset.id);
    presets_.emplace(preset.id, std::move(preset));
}

void Extension::load_next_pack() {
    const auto token = pack_queue_.front();
    pack_queue_.erase(pack_queue_.begin());
    try {
        if (!rig_.resolved()) {
            const auto p = host_.player();
            if (!p.is_object() || !is_handle(p.value("controller", Json()))) { pack_queue_.insert(pack_queue_.begin(), token); pack_wait_ = 1.0; return; }   // not in the game yet
        }
        Pack pack = read_pack(host_, token);
        for (const auto& [id, audio] : pack.audio) audio_ids_.emplace(id, audio.asset);   // first pack wins a shared id
        for (const auto& id : pack.presets) {
            try {
                auto preset = Preset::parse(pack.rows.at("preset." + id));
                if (preset.id != id) throw std::invalid_argument("row preset." + id + " holds preset " + preset.id);
                preset.pack = pack.name;
                if (!preset.audio.empty() && !preset.audio_is_file()) {
                    const auto a = pack.audio.find(preset.audio);
                    if (a == pack.audio.end()) throw std::invalid_argument("plays " + preset.audio + ", which the pack does not carry");
                    preset.audio_asset = a->second.asset;
                }
                add_preset(std::move(preset));
            } catch (const std::exception& e) { preset_problems_.push_back(pack.name + " / " + id + ": " + e.what()); }
        }
        pack.rows.clear();   // presets are parsed; recipes and styles are read when used
        packs_.push_back(std::move(pack));
    } catch (const std::exception& e) { preset_problems_.push_back("CINE_" + token + "_eins0fx_P: " + e.what()); }
    if (pack_queue_.empty()) { packs_loading_ = false; finish_presets(); }
}

// After the folder or the last pack: user presets naming a pack's audio id find its SoundWave,
// the selection stays valid, and every problem reaches the log once.
void Extension::finish_presets() {
    for (auto& [id, preset] : presets_)
        if (preset.pack.empty() && !preset.audio.empty() && !preset.audio_is_file()) {
            const auto a = audio_ids_.find(preset.audio);
            preset.audio_asset = a == audio_ids_.end() ? std::string() : a->second;
        }
    if (packs_loading_) return;
    if (!presets_.empty() && !presets_.contains(settings_.preset)) {
        const auto saved = saved_preset_;
        settings_.preset = presets_.contains(saved) ? saved : preset_order_.empty() ? presets_.begin()->first : preset_order_.front();
    }
    for (const auto& p : preset_problems_) host_.log("Preset skipped: " + p, "warning");
    host_.log("CINE presets: " + std::to_string(presets_.size()) + " from " + std::to_string(packs_.size()) + " pack(s) and the presets folder");
}

void Extension::load_state() {
    Json state;
    try { state = host_.request({{"op", "state.load"}}); } catch (...) { return; }
    if (!state.is_object()) return;
    const auto s = state.value("settings", Json::object());
    if (s.is_object()) {
        if (s.contains("preset") && s["preset"].is_string()) {
            saved_preset_ = s["preset"].get<std::string>();   // packs load later; finish_presets() applies it
            if (presets_.contains(saved_preset_)) settings_.preset = saved_preset_;
        }
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
        if (s.contains("signature") && s["signature"].is_boolean()) settings_.signature = s["signature"];
        if (s.contains("picture") && s["picture"].is_string() && std::count(picture_ids.begin(), picture_ids.end(), s["picture"].get<std::string>())) settings_.picture = s["picture"];
        if (s.contains("expression")) {
            try { settings_.expression = expression_choice(s.at("expression")); }
            catch (const std::exception& e) { host_.log(std::string("Saved expression ignored: ") + e.what(), "warning"); }
        }
        if (s.contains("performance")) {
            try { settings_.performance = PerformanceChoice::parse(s.at("performance")); }
            catch (const std::exception& e) { host_.log(std::string("Saved performance ignored: ") + e.what(), "warning"); }
        }
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
                             {"labels", settings_.labels}, {"breakdown_target", settings_.breakdown_target}, {"signature", settings_.signature},
                             {"picture", settings_.picture}, {"performance", settings_.performance.json()}, {"expression", settings_.expression}}}};
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
    last_frame_ = *f;
    frame_.valid = true; frame_.ready = f->world_ready != 0; frame_.in_menu = f->in_menu != 0;
    frame_.world = f->world_generation; frame_.pawn = f->pawn; frame_.controller = f->controller;
    frame_.position = {f->player_x, f->player_y, f->player_z};
    frame_.velocity = {f->velocity_x, f->velocity_y, f->velocity_z};
    frame_.yaw = f->player_yaw; frame_.seconds = f->seconds;
    frame_.has_camera = CSSX_FRAME_HAS_CAMERA(f);
    if (frame_.has_camera) { frame_.camera = {f->camera_x, f->camera_y, f->camera_z}; frame_.camera_rotation = {f->camera_pitch, f->camera_yaw, f->camera_roll}; }
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
    // Breakdown labels are part of the picture, so they stay on during the take. The camera
    // pose comes with the frame (one host call on an older CSSX); the rest is the tick's
    // samples and plain maths.
    viewport_h_ = f->viewport_h;
    if (mode_ == Mode::Running && take_ && take_->preset.kind == ShotKind::Sequence && !frame_.in_menu) sequence_frame(*f);
    // All of it is laid out inside the chosen frame guide's opening, so it survives the crop.
    const bool signing = mode_ == Mode::Running && settings_.signature && !signature_.empty() && !frame_.in_menu;
    // A preset made for one frame (a 9:16 cut) is laid out in that frame whatever guide is chosen.
    const bool own_frame = take_ && (mode_ == Mode::Running || mode_ == Mode::Countdown) && !take_->preset.frame.empty();
    const Rect opening = guide_opening(f->viewport_w, f->viewport_h, guide_aspect(guide_from(own_frame ? take_->preset.frame : settings_.guide)));
    static const std::vector<std::string> unsigned_take;
    if (mode_ == Mode::Running && take_ && take_->body_live && lens_.ok && frame_.has_camera && !frame_.in_menu) {
        auto& t = *take_;
        t.body_age += frame_.seconds;
        const double ease = std::clamp(t.body_age / 0.1, 0.0, 1.0);
        const auto basis = view_basis(frame_.camera_rotation);
        // What of her is in the picture, and how wide: every part whose ball touches the frame's
        // height, from its left edge to its right. The line goes at the layers' share of that
        // width, so a raised arm, a turn or a pose on the ground moves it with her.
        double left = 1e12, right = -1e12;
        const Vec3 forward = basis.forward;
        for (size_t i = 0; i < Take::body_parts; ++i) {
            const Vec3 world = frame_.position + t.body_before[i] + (t.body_now[i] - t.body_before[i]) * ease;
            double x = 0, y = 0;
            if (!project(frame_.camera, basis, lens_, world, x, y)) continue;
            const Vec3 d = world - frame_.camera;
            const double depth = d.x * forward.x + d.y * forward.y + d.z * forward.z;
            if (depth < 5) continue;
            const double reach = body_shape[i].radius * lens_.kx / depth;
            if (y + reach < opening.y || y - reach > opening.y + opening.h) continue;   // above or below the picture
            left = std::min(left, x - reach); right = std::max(right, x + reach);
        }
        if (right > left && f->viewport_w > 1) {
            const double share = t.desired.follow ? t.desired.share : 0.5;
            const double target = std::clamp((left + (right - left) * share) / f->viewport_w, 0.08, 0.92);
            // Eased: a hand entering the frame moves the line over a moment, not in one frame.
            t.body_place = t.body_placed ? t.body_place + (target - t.body_place) * (1.0 - std::exp(-std::clamp(frame_.seconds, 0.0, 0.25) / 0.25)) : target;
            t.body_placed = true;
            if (t.desired.follow && t.wearing == Take::Wear::Idle && t.seq_clock >= t.transition_until) breakdown_.follow(t.body_place);
        }
    }
    // An outfit change has the picture: its cover is a transition, not a breakdown step, and
    // the labels go the frame it starts (the tick that samples them is up to 100 ms behind).
    const bool covering = take_ && (take_->wearing != Take::Wear::Idle || take_->seq_clock < take_->transition_until);
    const bool titled = mode_ == Mode::Running && take_ && settings_.labels && !take_->title.empty() && !frame_.in_menu;
    if (mode_ == Mode::Running && labels_live_ && lens_.ok && !frame_.in_menu && !covering) {
        label_age_ += frame_.seconds;
        callout_targets_.clear();
        try {
            Vec3 camera = frame_.camera; Rotator rotation = frame_.camera_rotation;
            if (frame_.has_camera || rig_.view_point(camera, rotation)) {
                const auto basis = view_basis(rotation);
                const double ease = std::clamp(label_age_ / 0.1, 0.0, 1.0);
                for (const auto& label : labels_) {
                    CalloutTarget target;
                    target.text = label.text;
                    target.visible = project(camera, basis, lens_, frame_.position + label.before + (label.now - label.before) * ease, target.x, target.y);
                    // Every further place the label names gets its own arrow. With nothing of the
                    // first place in view, the first further one in view leads.
                    for (size_t k = 0; k < label.more_now.size(); ++k) {
                        double x = 0, y = 0;
                        if (!project(camera, basis, lens_, frame_.position + label.more_before[k] + (label.more_now[k] - label.more_before[k]) * ease, x, y)) continue;
                        if (!target.visible) { target.x = x; target.y = y; target.visible = true; }
                        else target.more.push_back({x, y});
                    }
                    callout_targets_.push_back(std::move(target));
                }
            }
        } catch (...) { callout_targets_.clear(); }
        // A split breakdown keeps its labels on the layers' side of the line.
        const double line = breakdown_.edge() < 1.0 ? f->viewport_w * breakdown_.edge() : 1e12;
        // The caption is the step's own title when it has one, else the layers on screen.
        callouts_.update(*f, opening, callout_targets_, take_->title.empty() ? "Breakdown  //  " + breakdown_.caption() : take_->title, signing ? signature_ : unsigned_take, line);
    } else if (signing || (titled && !covering)) {
        callout_targets_.clear();
        callouts_.update(*f, opening, callout_targets_, titled && !covering ? take_->title : std::string(), signing ? signature_ : unsigned_take);
    } else callouts_.hide();
    return 1;
}

void Extension::tick(double seconds) {
    if (stopped_) return;
    try {
        if (!pack_queue_.empty() && mode_ == Mode::Idle) {
            // Waiting for the game (main menu, loading): ask about once a second, not every tick.
            if (pack_wait_ > 0) { pack_wait_ -= std::isfinite(seconds) ? seconds : 0.1; return; }
            load_next_pack();
            return;
        }
        if (mode_ == Mode::Idle && !performance_.owned() && !expression_.owned() && !capture_pending_ && look_restore_.is_null() && own_look_owed_.is_null() && !scene_.pending() && !breakdown_.busy() && !breakdown_opponent_.busy())
            return;   // free outside Cine World
        const double dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0, 1.0) : 0.0;
        tick_dt_ = dt;
        if (mode_ != Mode::Running && performance_.owned() && (performance_cleanup_wait_ -= dt) <= 0) {
            performance_cleanup_wait_ = .5;
            try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {}
        }
        if (!take_ && expression_.owned() && (expression_cleanup_wait_ -= dt) <= 0) {
            expression_cleanup_wait_ = .5;
            try { expression_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {}
        }
        // A loading screen, death or travel tears the world down: every handle is dead. The CSS
        // look is not a world object, so a look the take changed is still put back (retried).
        if (mode_ != Mode::Idle && mode_ != Mode::RecordWait && frame_.valid && frame_.world != world_) {
            try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {}
            if (take_ && !take_->snapshot.is_null() && take_->step_next > 0) queue_look_restore(take_->snapshot);
            if (take_ && take_->idle_set) { try { rig_.css({{"action", "idle"}, {"value", ""}}); } catch (...) {} }
            if (take_ && take_->wore && take_->own_look.is_object()) { own_look_owed_ = take_->own_look; own_look_check_ = 0.5; queue_look_restore(take_->own_look); }
            labels_live_ = false; labels_.clear(); lens_ = {};
            audio_.stop(); rig_.stop_music();
            try { rig_.restore_picture(); } catch (...) {}   // console variables outlive the world
            rig_.forget(); guides_.forget(); callouts_.forget(); typeface_.forget(); scene_.forget(); breakdown_.forget(); breakdown_opponent_.forget();
            take_.reset(); samples_.clear(); mode_ = Mode::Idle; start_pending_ = false;
            report("The world changed (loading, travel or death); CINE stopped and the game camera is back.");
            return;
        }
        if (!look_restore_.is_null()) retry_look_restore(dt);
        // Is she back in her own look? Asked twice a second until CSS says so.
        if (!own_look_owed_.is_null() && !take_ && look_restore_.is_null() && (own_look_check_ -= dt) <= 0) {
            own_look_check_ = 0.5;
            try { if (worn(rig_.css({{"action", "describe"}}), own_look_owed_.value("outfit", std::string()), own_look_owed_.value("variant", std::string()))) own_look_owed_ = nullptr; } catch (...) {}
        }
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
                if (take_->countdown <= 0) roll();
                break;
            case Mode::Running:
                if (const auto why = performance_.poll([&](const Json& r) { return rig_.css(r); }); !why.empty()) {
                    // In a sequence the wardrobe interrupts the dance for its own reasons (a
                    // look step, the clay coming off her materials): the camera keeps rolling
                    // and the dance is started again where it was.
                    if (take_->preset.kind == ShotKind::Sequence) {
                        auto& t = *take_;
                        if (!t.performance_resume) { t.performance_resume = true; t.performance_retry = 0; t.performance_give_up = t.seq_clock + 6.0; report("Performance interrupted (" + why + "); starting it again where it was."); }
                    } else { finish_take("Performance interrupted: " + why, true); break; }
                }
                if (take_->preset.kind == ShotKind::Sequence) resume_performance();
                take_->clock += dt; run(); break;
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
    load_signature();
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
    if (performance_.owned()) throw std::runtime_error("Wait for the previous performance to stop");
    bool idle_taken = false;
    if (const auto chosen = take_performance(*current_preset()); chosen.id != "off") {
        if (const auto why = performance_conflict(*current_preset()); !why.empty()) throw std::runtime_error(why);
        // Her saved idle may itself be a dance on the same performance layer (CSS plays it as
        // her idle). The preset's own idle replaces it for the take, which frees the layer;
        // finish_take hands her saved idle back.
        if (const auto& idle = current_preset()->idle; !idle.empty()) {
            try { rig_.css({{"action", "idle"}, {"value", idle}}); idle_taken = true; } catch (...) {}
        }
        performance_catalog_.refresh([&](const Json& r) { return rig_.css(r); });
        // "In use" is the layer still blending out of her idle dance: the roll starts the
        // performance a moment later, and retries.
        if (const auto why = performance_catalog_.unavailable(chosen); !why.empty() && why.find("in use") == std::string::npos) {
            if (idle_taken) { try { rig_.css({{"action", "idle"}, {"value", ""}}); } catch (...) {} }
            throw std::runtime_error(why);
        }
    }
    if (expression_.owned()) throw std::runtime_error("Wait for the previous expression to be restored");
    if (settings_.expression != "unchanged") {
        const auto& preset = *current_preset();
        if (settings_.look != "off" || (preset.kind == ShotKind::Sequence && !preset.sequence.looks.empty()))
            throw std::runtime_error("Use As set in CSS for takes that change looks, or turn the look track off.");
        expression_catalog_.refresh([&](const Json& r) { return rig_.css(r); });
        if (const auto why = expression_catalog_.unavailable(settings_.expression); !why.empty()) throw std::runtime_error(why);
    }
    // A look from the last take still waiting to go back would become the new "own look".
    if (!look_restore_.is_null()) {
        if (!restore_look(look_restore_)) throw std::runtime_error("Still putting your look back from the last take; try again in a moment");
        look_restore_ = nullptr;
    }
    take_ = std::make_unique<Take>();
    take_->performance = settings_.performance;
    if (const auto* p = current_preset(); p && !p->performance.empty() && p->performance != "menu") {
        take_->performance = PerformanceChoice{};
        take_->performance.id = p->performance; take_->performance.rate = p->performance_rate;
    }
    take_->expression = settings_.expression;
    take_->idle_set = idle_taken;
    take_->world = frame_.world; take_->pawn = frame_.pawn;
    mode_ = Mode::Preparing;
    report("Preparing the take...");
}

Json Extension::look_payload(const LookStep& step, const Json& describe) const {
    Json commands = Json::array();
    std::map<std::string, std::string> kinds;
    for (const auto& c : describe.value("controls", Json::array())) kinds[c.value("id", std::string())] = c.value("kind", std::string());
    bool palette_known = step.palette.empty() || step.palette == "original";   // the outfit's own colours: always there, not always listed
    for (const auto& p : describe.value("palettes", Json::array())) if (p.value("id", std::string()) == step.palette) palette_known = true;
    if (!step.palette.empty() && palette_known) commands.push_back({{"action", "palette"}, {"palette", step.palette}});
    for (const auto& [id, values] : step.controls) {
        const auto kind = kinds.find(id);
        if (kind == kinds.end()) continue;   // this outfit has no such part: skip, one preset serves many outfits
        if (kind->second == "color" && values.size() == 3) commands.push_back({{"action", "control"}, {"control", id}, {"rgb", values}});
        else for (size_t ch = 0; ch < values.size(); ++ch) commands.push_back({{"action", "control"}, {"control", id}, {"channel", ch}, {"value", values[ch]}});
    }
    for (const auto& [id, preset] : step.physics) if (kinds.contains(id)) commands.push_back({{"action", "physics_preset"}, {"control", id}, {"preset", preset}});
    if (commands.empty()) return Json();
    return {{"action", "apply"}, {"persist", false}, {"commands", std::move(commands)}};
}

void Extension::prepare() {
    auto& t = *take_;
    if (!t.expression_ready) {
        t.expression_wait += tick_dt_;
        if (t.expression_wait > 5.) throw std::runtime_error("CSS did not finish applying the facial expression");
        if (!t.expression_started) {
            expression_.start([&](const Json& r) { return rig_.css(r); }, t.expression);
            t.expression_started = true;
        }
        if ((t.expression_poll -= tick_dt_) > 0) return;
        t.expression_poll = .1;
        if (!expression_.ready([&](const Json& r) { return rig_.css(r); })) return;
        t.expression_ready = true;
    }
    if (t.prepare_step == 0) {
        if (!rig_.resolved()) rig_.resolve();
        // Sharper shadows (and full resolution, when asked) for this take; restore_take puts the game's values back.
        rig_.picture(settings_.picture == "full" ? 2 : settings_.picture == "sharp" ? 1 : 0);
        t.preset = *current_preset();
        auto& p = t.preset;
        // The pack's music loads now, so starting it on the first frame costs nothing. A missing
        // sound is reported when the take rolls; the take still runs on its own clock.
        if (!p.audio.empty() && !p.audio_is_file() && !p.audio_asset.empty()) {
            try { t.sound = rig_.load_sound(p.audio_asset); } catch (const std::exception& e) { host_.log(std::string("Scene music: ") + e.what(), "warning"); }
        }
        // A "view" shot is the camera as the player framed it in Cine World: its place around
        // her (in her own frame, so it follows her if the take is rolled elsewhere), height,
        // tilt and lens, held. The camera looks at her axis; a framing off to one side keeps
        // its height and tilt but centres her.
        if (p.kind == ShotKind::Sequence && std::any_of(p.sequence.shots.begin(), p.sequence.shots.end(), [](const SeqShot& s) { return s.view; })) {
            const auto view = rig_.capture_final(her_position(), frame_.yaw);
            const double horizontal = std::max(60.0, std::hypot(view.camera.x, view.camera.y));
            SeqPose pose;
            pose.azimuth = std::atan2(view.camera.y, view.camera.x) * 180 / std::numbers::pi;
            pose.distance = horizontal / std::max(0.05, p.zoom);
            pose.height = view.camera.z - settings_.height;
            pose.aim = view.camera.z + std::tan(view.pitch * std::numbers::pi / 180) * horizontal - settings_.height;
            pose.fov = view.fov / std::max(0.05, p.fov_scale);
            for (auto& shot : p.sequence.shots) if (shot.view) { shot.from = shot.to = pose; shot.track = Subject::None; shot.fit = 0; shot.fixed = true; shot.fstop = 0; }
        }
        // A look step may name "current": whatever she wears at F8, with the customization
        // saved on it, so the player chooses that pass's outfit by wearing it before the take.
        if (p.kind == ShotKind::Sequence) {
            bool any = false;
            for (const auto& look : p.sequence.looks) any = any || look.outfit == "current";
            if (any) {
                const auto d = rig_.css({{"action", "describe"}});
                const auto outfit = d.value("outfit", std::string()), variant = d.value("variant", std::string());
                if (outfit.empty()) throw std::runtime_error("A look step wants the outfit you wear now, and CSS has none on");
                for (auto& look : p.sequence.looks) if (look.outfit == "current") { look.outfit = outfit; look.variant = variant; }
            }
        }
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
        const bool sequence = p.kind == ShotKind::Sequence;
        if (!p.idle.empty() && !t.idle_set) {
            // One idle on every look the take wears, so her pose does not snap between them,
            // and what plays under a performance between its passes (a saved idle that is
            // itself a dance would take the performance over). CSS keeps it out of the saved
            // choices; finish_take ends it.
            try { rig_.css({{"action", "idle"}, {"value", p.idle}}); t.idle_set = true; } catch (...) {}
        }
        if (p.kind != ShotKind::Photo && p.kind != ShotKind::Action && !sequence) t.curve = std::make_unique<OrbitCurve>(t.timeline.keys);
        // Breakdown track: the preset's own steps, or the built-in reel.
        if (settings_.breakdown == "preset") t.reveals = t.timeline.breakdown;
        else if (settings_.breakdown == "reel") t.reveals = reel(t.timeline.total);
        for (const auto& r : t.reveals) { t.layers |= r.layers; t.follows = t.follows || r.follow; }
        if (settings_.breakdown == "preset" && t.reveals.empty()) t.note = "Breakdown off: this preset has no breakdown steps.";
        if (sequence) {
            try {
                const auto described = rig_.css({{"action", "describe"}});
                if (!own_look_owed_.is_null()) t.own_look = own_look_owed_;   // the last take's restore has not landed yet
                else {
                    // A change still on its way means "outfit" is the one she is leaving: wait for it.
                    if (described.value("busy", false) && (t.prep_wait += tick_dt_) < 5.0) return;
                    t.own_look = described;
                }
                t.prep_wait = 0;
                t.look_now = described;
                remember_custom(t.own_look);
                t.seq_looks = !p.sequence.looks.empty();
            } catch (const std::exception& e) {
                // No CSS look to read (CSS off, or the game's own shell worn): steps that need it
                // are off; steps that only play a game montage or set a title still run.
                t.own_look = nullptr; t.seq_looks = !p.sequence.looks.empty() && !p.sequence.needs_css();
                if (p.sequence.needs_css()) t.note = std::string("Looks off: ") + e.what() + ".";
            }
            // Outfit changes are covered by a frame pass, so the passes are built even when the
            // breakdown track itself is off.
            if (t.seq_looks && !p.sequence.wardrobe().empty()) for (const LayerSet cover : p.cover) t.layers |= cover;
            // A sequence wears its own looks, and the breakdown runs across all of them: it
            // starts on her own look, is taught each look as the take tries it on, and builds
            // its pieces once it knows them all.
            t.many_looks = t.seq_looks && t.layers != 0;
            if (t.many_looks) { Take::Prep begin; begin.begin = true; t.prep.push_back(begin); }
            if (t.seq_looks) for (const auto& [outfit, variant] : p.sequence.wardrobe()) { Take::Prep w; w.outfit = outfit; w.variant = variant; t.prep.push_back(w); }
            if (t.layers) { Take::Prep build; build.build = true; t.prep.push_back(build); }
            if (t.seq_looks && t.own_look.is_object()) {   // no CSS look read (the game's own shell): nothing to open on
                Take::Prep opening; opening.own = true;
                for (const auto& look : p.sequence.looks) if (!look.outfit.empty() && look.at <= 1e-6) { opening.own = false; opening.outfit = look.outfit; opening.variant = look.variant; }
                t.prep.push_back(opening);
            }
        }
        // Clay takes her material slots; a palette change mid-take would fight it. A breakdown
        // without clay (wireframe, bones) only uses the overlay slot and shares the take.
        const bool clay_track = (t.layers & bit(Layer::Clay)) != 0;
        if (!sequence && clay_track && settings_.look != "off") t.note = "Look track off while a breakdown with clay runs.";
        // Look track: CSS answers describe, or the take runs without it and says why.
        if (!sequence && settings_.look != "off" && !clay_track) {
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
                if (steps) for (const auto& s : *steps) {
                    auto pay = look_payload(s, d);
                    if (!pay.is_null()) t.steps.emplace_back(start + s.fraction * span, std::move(pay));
                    if (s.titled) t.titles.emplace_back(start + s.fraction * span, s.title);
                }
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
    if (t.prepare_step == 6) { if (plan_surroundings()) t.prepare_step = 5; return; }
    if (t.prepare_step == 5) { prepare_looks(); return; }
    // Cameras, gait, view, then the countdown.
    if (t.preset.kind == ShotKind::Sequence) {
        for (const auto& shot : t.preset.sequence.shots) { t.track_used[static_cast<size_t>(shot.track)] = true; t.fits = t.fits || shot.fit > 0; }
        sample_tracks(true);   // where her head, hips and the rest are, before any shot is placed or planned
        const auto view = sequence_view(pose_at(t.preset.sequence, 0), anchor_at(0), 0);
        t.dolly = rig_.spawn_camera(view.camera, view.rotation, view.fov);
        t.fov_applied = view.fov;
        t.yaw_f = frame_.yaw; t.vel_f = {};
        t.ctl_yaw = frame_.yaw;
        rig_.view(t.dolly, 0.0);
        t.plan.assign(t.preset.sequence.shots.size(), ShotPlan{});
        t.prepare_step = settings_.walls ? 6 : 5;
        if (settings_.walls) report("Checking the surroundings for every shot...");
        return;
    }
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
        begin_breakdown();
        t.prepare_step = 3;
        report("Building the breakdown layers...");
        return;
    }
    begin_countdown();
}

void Extension::begin_breakdown() {
    auto& t = *take_;
    // Clay and wireframe change her materials. If the take cannot put one back exactly,
    // the wardrobe mod paints her again from this.
    if (t.layers & (bit(Layer::Clay) | bit(Layer::Wireframe))) { try { t.look_before = rig_.css({{"action", "describe"}}).at("customize"); } catch (...) { t.look_before = nullptr; } }
    if (t.many_looks) {
        // Clay stays out of her material slots when a look step repaints her (a palette, a
        // control, a reset): CSS must be free to paint under the breakdown. Outfit changes
        // alone are fine: the breakdown lifts clay out of the slots for the wear and puts it
        // back on the new look, so clay and wireframe can then share her (slots and overlay).
        bool repaints = false;
        for (const auto& look : t.preset.sequence.looks) repaints = repaints || !look.palette.empty() || !look.controls.empty() || look.reset;
        breakdown_.begin(rig_.pawn(), rig_.camera_component(t.dolly), t.layers, {}, {.looks = true, .overlay_only = repaints});
        return;
    }
    breakdown_.begin(rig_.pawn(), rig_.camera_component(t.dolly), t.layers, spring_bones());
    if (settings_.breakdown_target && t.preset.kind != ShotKind::Sequence) {
        const auto other = is_handle(t.opponent) ? t.opponent : rig_.locked_target();
        // The frame passes cover the whole picture already; the second character gets the rest.
        if (is_handle(other)) breakdown_opponent_.begin(other, Json(), t.layers & ~frame_layers, {});
    }
}

// ---------------------------------------------------------------- sequence takes

Json Extension::wear_request(const std::string& outfit, const std::string& variant, bool own, bool persist) const {
    const auto& t = *take_;
    Json request{{"action", "wear"}, {"persist", persist}};
    if (own) { request["outfit"] = t.own_look.at("outfit"); request["variant"] = t.own_look.at("variant"); request["customize"] = t.own_look.at("customize"); }
    else {
        request["outfit"] = outfit; request["variant"] = variant;
        // The customization this outfit opened with: CSS keeps what the variant can take.
        if (const auto found = t.look_custom.find(outfit); found != t.look_custom.end()) request["customize"] = found->second;
    }
    return request;
}

// The first customization seen on an outfit in this take is the one its wears carry.
void Extension::remember_custom(const Json& described) {
    if (!take_ || !described.is_object() || !described.contains("customize") || !described.at("customize").is_object()) return;
    const auto outfit = described.value("outfit", std::string());
    if (!outfit.empty() && !take_->look_custom.contains(outfit)) take_->look_custom[outfit] = described.at("customize");
}

bool Extension::worn(const Json& d, const std::string& outfit, const std::string& variant) const {
    return d.is_object() && d.value("outfit", std::string()) == outfit && d.value("variant", std::string()) == variant && !d.value("busy", false);
}

// Before a sequence rolls: every shot is traced against the world, from her to where its
// camera would be, at five moments of the shot and for each angle it could be turned to. A
// run of shots that flow into each other is judged as one and turned as one. The result is a
// turn and a scale per shot (choose_plan), so a shot near a wall keeps its shape on a clearer
// side or closer in, instead of flying into the wall. A slice of traces per tick.
bool Extension::plan_surroundings() {
    auto& t = *take_;
    const auto& seq = t.preset.sequence;
    const size_t shots = seq.shots.size(), turns = std::size(plan_turns);
    constexpr size_t traces_a_tick = 60;
    constexpr int samples = 5;
    if (t.clear.size() != shots) { t.clear.assign(shots, std::vector<double>(turns, 1.0)); t.plan_chain = t.plan_turn = t.plan_shot = t.plan_sample = 0; t.chain_clear.clear(); }
    size_t budget = traces_a_tick;
    while (t.plan_chain < shots) {
        size_t chain_end = t.plan_chain + 1;
        while (chain_end < shots && !seq.shots[chain_end].cut) ++chain_end;
        while (t.plan_turn < turns) {
            if (t.plan_shot < t.plan_chain) t.plan_shot = t.plan_chain;
            for (; t.plan_shot < chain_end; ++t.plan_shot, t.plan_sample = 0) {
                const auto& shot = seq.shots[t.plan_shot];
                for (; t.plan_sample < size_t(samples); ++t.plan_sample) {
                    if (budget == 0) return false;   // the rest on the next tick
                    --budget;
                    const double when = shot.at + shot.length * 0.999 * double(t.plan_sample) / double(samples - 1);
                    SeqPose pose = pose_at(seq, when);
                    pose.azimuth += plan_turns[t.plan_turn];
                    const Vec3 anchor = anchor_at(when);
                    const auto view = sequence_view(pose, anchor, when);
                    const Vec3 aim = to_world(anchor, t.base_yaw, {0, 0, pose.aim + settings_.height});
                    double hit = 0, free = 1.0;
                    // A hit within arm's reach is her own: a hand, a sword at her hip. Not a wall.
                    try { if (rig_.trace(aim, view.camera, &hit) && hit > own_reach && hit < view.focus - 5) free = std::clamp((hit - 25) / view.focus, 0.0, 1.0); } catch (...) {}
                    auto& worst = t.clear[t.plan_shot][t.plan_turn];
                    worst = std::min(worst, free);
                }
            }
            double chain = 1.0;
            for (size_t i = t.plan_chain; i < chain_end; ++i) chain = std::min(chain, t.clear[i][t.plan_turn]);
            t.chain_clear.push_back(chain);
            t.plan_shot = t.plan_chain;
            ++t.plan_turn;
            if (t.chain_clear.size() == 1 && chain >= 0.98) break;   // open as written: no need to look elsewhere
            // A shot that must keep its side holds the whole chain there: only the distance may give.
            bool fixed = false;
            for (size_t i = t.plan_chain; i < chain_end; ++i) fixed = fixed || seq.shots[i].fixed;
            if (fixed) break;
        }
        const ShotPlan chosen = choose_plan(t.chain_clear);
        const size_t turn = size_t(std::find(std::begin(plan_turns), std::end(plan_turns), chosen.turn) - std::begin(plan_turns));
        for (size_t i = t.plan_chain; i < chain_end; ++i) t.plan[i] = {chosen.turn, std::clamp(t.clear[i][std::min(turn, turns - 1)], 0.25, 1.0)};
        t.chain_clear.clear();
        t.plan_chain = chain_end; t.plan_turn = 0; t.plan_shot = chain_end; t.plan_sample = 0;
    }
    return true;
}

// Before a sequence rolls: every look is worn once (its first-time loading stays out of the
// recording), the breakdown is built on the look it belongs to, and she ends on the opening
// look. One step per tick; a look that will not land within eight seconds is given up on.
void Extension::prepare_looks() {
    auto& t = *take_;
    if (t.prep.empty()) { begin_countdown(); return; }
    if (breakdown_.active() && !breakdown_.learned()) return;   // still reading the look she wears
    auto& step = t.prep.front();
    auto key = [](const std::string& outfit, const std::string& variant) { return outfit + "/" + variant; };
    if (step.begin) {
        // Her own look opens the take unless a look step sits on its first beat: it is the first look taught.
        begin_breakdown();
        breakdown_.learn(key(t.own_look.value("outfit", std::string()), t.own_look.value("variant", std::string())), spring_bones());
        t.prep.pop_front();
        return;
    }
    if (step.build) {
        if (!t.build_started) {
            if (t.many_looks) breakdown_.assemble(); else begin_breakdown();
            t.build_started = true; t.build_wait = 0; report("Building the breakdown layers..."); return;
        }
        t.build_wait += tick_dt_;
        const bool built = (!breakdown_.active() || breakdown_.ready()) && (!breakdown_opponent_.active() || breakdown_opponent_.ready());
        if (!built && t.build_wait < build_timeout) return;
        if (!breakdown_.note().empty()) t.note = t.note.empty() ? breakdown_.note() : t.note + "  " + breakdown_.note();
        t.prep.pop_front();
        return;
    }
    const std::string outfit = step.own ? t.own_look.value("outfit", std::string()) : step.outfit;
    const std::string variant = step.own ? t.own_look.value("variant", std::string()) : step.variant;
    try {
        const auto described = rig_.css({{"action", "describe"}});
        const bool landed = worn(described, outfit, variant) && (t.prep_sent || !step.own);
        const bool untouched = worn(described, outfit, variant) && step.own && !t.wore;   // still her own look
        if (landed || untouched) {
            if (t.many_looks && breakdown_.active()) {
                // Before the pieces exist the breakdown reads this look's bones (a few ticks);
                // after, it only has to know which look she wears as the take opens.
                if (t.build_started) breakdown_.adopt(key(outfit, variant));
                else if (!t.learning) { breakdown_.learn(key(outfit, variant), spring_bones()); t.learning = true; return; }
            }
            t.learning = false;
            t.look_now = described; remember_custom(described); t.prep.pop_front(); t.prep_sent = false;
            return;
        }
        if (!t.prep_sent) {
            rig_.css(wear_request(outfit, variant, step.own, false));
            t.prep_sent = true; t.prep_wait = 0; t.wore = true;
            report("Preparing the looks (" + std::to_string(t.prep.size()) + " to go)...");
            return;
        }
    } catch (const std::exception& e) {
        // "CSS is busy" and the like: the next tick asks again.
        if (t.prep_wait > 8) { t.seq_looks = false; t.note = std::string("Looks off: ") + e.what() + "."; t.prep.pop_front(); t.prep_sent = false; return; }
    }
    t.prep_wait += tick_dt_;
    if (t.prep_wait > 8) { t.note = "A look did not load in time: " + outfit + "."; t.prep.pop_front(); t.prep_sent = false; }
}

// The point a shot's heights and circle are measured from: her capsule centre as the take
// began, or the part of her body the shot tracks. A shot that flows on from one framed on
// another part carries the point across over its own length.
Vec3 Extension::anchor_at(double time) const {
    const auto& t = *take_;
    const auto& seq = t.preset.sequence;
    if (seq.shots.empty()) return t.base;
    const size_t index = shot_at(seq, time);
    const auto& shot = seq.shots[index];
    auto of = [&](Subject track) {
        const auto k = static_cast<size_t>(track);
        // A tracked part is where she is now: a performance that carries her across the
        // ground (After Brute Battle walks eleven metres) keeps the camera with her. The
        // capsule reference stays the fixed point the take opened on.
        return track == Subject::None || !t.track_ok[k] ? t.base : frame_.position + t.track_at[k];
    };
    if (!shot.cut && index > 0 && seq.shots[index - 1].track != shot.track && shot.length > 1e-6) {
        const double u = std::clamp((time - shot.at) / shot.length, 0.0, 1.0), s = u * u * (3 - 2 * u);
        const Vec3 from = of(seq.shots[index - 1].track), to = of(shot.track);
        return from + (to - from) * s;
    }
    return of(shot.track);
}

// Where the tracked parts of her body are now. 10 Hz, and once before the shots are planned.
void Extension::sample_tracks(bool snap) {
    auto& t = *take_;
    // Up to three bones a part, averaged with these weights (body: her head, and her two feet).
    struct Part { const char* bone[3]; double weight[3]; };
    static constexpr Part parts[Take::tracks] = {{{"", "", ""}, {0, 0, 0}}, {{"Head", "", ""}, {1, 0, 0}}, {{"neck_01", "", ""}, {1, 0, 0}}, {{"spine_04", "", ""}, {1, 0, 0}},
                                                 {{"Pelvis", "", ""}, {1, 0, 0}}, {{"calf_l", "calf_r", ""}, {0.5, 0.5, 0}}, {{"Head", "Foot_L", "Foot_R"}, {0.5, 0.25, 0.25}}};
    try {
        if (!is_handle(t.body_mesh)) t.body_mesh = host_.get(rig_.pawn(), "Mesh");
        if (!t.track_live) {
            // A part this character does not have cannot be tracked: its shots fall back to the capsule.
            std::vector<std::string> names;
            const auto all = host_.call(t.body_mesh, "GetAllSocketNames");
            if (all.is_array()) for (const auto& n : all) if (n.is_string()) { auto name = n.get<std::string>(); std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); }); names.push_back(std::move(name)); }
            for (size_t k = 1; k < Take::tracks; ++k) {
                t.track_ok[k] = true;
                for (const char* bone : parts[k].bone) {
                    std::string want = bone;
                    std::transform(want.begin(), want.end(), want.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (!want.empty() && std::find(names.begin(), names.end(), want) == names.end()) t.track_ok[k] = false;
                }
                if (t.track_used[k] && !t.track_ok[k]) t.note = t.note.empty() ? "This character lacks a bone some shots are framed on; those use her centre." : t.note;
            }
        }
        for (size_t k = 1; k < Take::tracks; ++k) {
            if (!t.track_used[k] || !t.track_ok[k]) continue;
            Vec3 now{};
            for (size_t b = 0; b < 3; ++b) if (parts[k].bone[b][0]) now = now + rig_.socket_location(t.body_mesh, parts[k].bone[b]) * parts[k].weight[b];
            now = now - frame_.position;
            t.track_before[k] = t.track_live && !snap ? t.track_now[k] : now;
            t.track_now[k] = now;
            if (!t.track_live || snap) t.track_at[k] = now;
        }
        t.track_age = 0;
        t.track_live = true;
    } catch (...) {}
}

// The height of picture a shot shows where she stands, when it (or the shot it flows on from)
// is a "fit" shot; 0 when the pose's own field of view stands. Between a fit shot and one
// without, the height is carried across the flowing shot so the lens never jumps.
double Extension::coverage_at(double time, double distance, double fov) const {
    const auto& t = *take_;
    const auto& seq = t.preset.sequence;
    if (seq.shots.empty()) return 0;
    const size_t index = shot_at(seq, time);
    const auto& shot = seq.shots[index];
    const bool flows = !shot.cut && index > 0;
    const double before = flows ? seq.shots[index - 1].fit : shot.fit;
    if (shot.fit <= 0 && before <= 0) return 0;
    const double own = 2 * distance * t.lens_ratio * std::tan(fov * std::numbers::pi / 360);
    const double here = shot.fit > 0 ? shot.fit : own;
    if (!flows || before == shot.fit || shot.length <= 1e-6) return here;
    const double u = std::clamp((time - shot.at) / shot.length, 0.0, 1.0), s = u * u * (3 - 2 * u);
    const double was = before > 0 ? before : own;
    return was + (here - was) * s;
}

Extension::SeqView Extension::sequence_view(const SeqPose& pose, Vec3 anchor, double time) const {
    const auto& t = *take_;
    const double a = pose.azimuth * std::numbers::pi / 180, distance = std::max(55.0, pose.distance * t.preset.zoom * t.wall_scale);
    SeqView view;
    view.camera = to_world(anchor, t.base_yaw, {distance * std::cos(a), distance * std::sin(a), pose.height + settings_.height});
    const Vec3 aim = to_world(anchor, t.base_yaw, {0, 0, pose.aim + settings_.height});
    view.rotation = look_at(view.camera, aim);
    view.rotation.roll = pose.roll;
    view.fov = std::clamp(pose.fov * t.preset.fov_scale, 5.0, 150.0);
    const Vec3 d = aim - view.camera;
    view.focus = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    // A fit shot: the lens that shows that much height at this distance.
    if (const double height = coverage_at(time, view.focus, view.fov); height > 0 && t.lens_ratio > 1e-6)
        view.fov = std::clamp(2 * std::atan(height / (2 * view.focus * t.lens_ratio)) * 180 / std::numbers::pi, 8.0, 85.0);
    return view;
}

void Extension::apply_look(const SeqLook& look) {
    auto& t = *take_;
    if (look.restart && t.performance.id != "off" && mode_ == Mode::Running) {
        // A new pass on the look she already wears: the dance from the top, on this beat.
        try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {}
        // The pass starts now whatever CSS says: if it is not ready (her materials just changed
        // under the breakdown), the dance joins as soon as it is, at the point the music reached.
        t.performance_at = t.seq_clock;
        try { performance_.start([&](const Json& r) { return rig_.css(r); }, t.performance); t.performance_resume = false; }
        catch (...) { t.performance_resume = true; t.performance_retry = t.seq_clock + 0.2; t.performance_give_up = t.seq_clock + 6.0; }
    }
    if (!look.expression.empty()) {
        // Before a performance on the same beat: a face change while the clip plays makes CSS
        // interrupt it; this way the start waits for the face instead (a beat at most).
        rig_.css({{"action", "apply"}, {"persist", false}, {"commands", Json::array({{{"action", "expression"}, {"expression", look.expression}}})}});
        t.wore = true;   // her own face goes back with her look at the end
    }
    // A game montage, or an idle she is to hold, has her body from this beat: the take's
    // performance ends here for good. Left running, CSS would interrupt it on the montage and
    // CINE would start it again between moves, and the dance and the move would fight for her
    // (seen on the Mark of the Beast hunt, and on the reclining idle after it).
    const bool takes_her_body = (look.montage_set && !look.montage.empty()) || (!look.idle.empty() && look.performance_id.empty());
    if (takes_her_body && t.performance.id != "off") {
        if (performance_.owned()) { try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {} }
        t.performance.id = "off"; t.performance_resume = false;
    }
    if (!look.performance_id.empty() && mode_ == Mode::Running) {
        // Another performance from this beat: it is the take's performance now, so an
        // interruption resumes this one, at the point the music has reached.
        PerformanceChoice choice; choice.id = look.performance_id; choice.rate = look.performance_rate > 0 ? look.performance_rate : t.performance.rate;
        if (performance_.owned()) { try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {} }
        t.performance = choice; t.performance_at = t.seq_clock;
        try { performance_.start([&](const Json& r) { return rig_.css(r); }, choice); t.performance_resume = false; }
        catch (...) { t.performance_resume = true; t.performance_retry = t.seq_clock + 0.2; t.performance_give_up = t.seq_clock + 6.0; }
    }
    if (look.montage_set && look.montage.empty()) rig_.stop_montage();
    if (!look.idle.empty()) { rig_.css({{"action", "idle"}, {"value", look.idle}}); t.idle_set = true; }
    if (look.reset && t.opening.is_object()) { rig_.css({{"action", "apply"}, {"persist", false}, {"commands", Json::array({{{"action", "restore"}, {"customize", t.opening}}})}}); t.wore = true; }
    if (!look.physics.empty()) {
        // Only the controls this outfit has: one preset serves many outfits.
        Json commands = Json::array();
        for (const auto& control : t.look_now.value("controls", Json::array())) {
            const auto found = look.physics.find(control.value("id", std::string()));
            if (found != look.physics.end()) commands.push_back({{"action", "physics_preset"}, {"control", found->first}, {"preset", found->second}});
        }
        if (!commands.empty()) { rig_.css({{"action", "apply"}, {"persist", false}, {"commands", std::move(commands)}}); t.wore = true; }
    }
    if (!look.palette.empty() || !look.controls.empty()) {
        LookStep step; step.palette = look.palette; step.controls = look.controls;
        const auto payload = look_payload(step, t.look_now);   // parts this outfit does not have are skipped
        if (!payload.is_null()) { rig_.css(payload); t.wore = true; }   // her own look goes back at the end
    }
    // The montage last: CSS holds an appearance change back while one is active.
    if (look.montage_set && !look.montage.empty()) {
        try { rig_.play_montage(look.montage, look.montage_rate); }
        catch (const std::exception& e) { const std::string note = std::string("Montage: ") + e.what() + "."; t.note = t.note.empty() ? note : t.note + "  " + note; }
    }
}

// Every frame of a sequence take: the clock, cuts on their beat, look steps and the
// transitions that cover an outfit change.
// Never throws; a failing look step turns the looks off for the rest of the take.
void Extension::sequence_frame(const CssxFrame& f) {
    auto& t = *take_;
    const auto& seq = t.preset.sequence;
    if (t.wall_clock) t.seq_clock = std::chrono::duration<double>(std::chrono::steady_clock::now() - t.rolled_at).count();
    else t.seq_clock += std::clamp(f.seconds, 0.0, 0.25);
    const double now = t.seq_clock;
    try {
        const size_t index = shot_at(seq, now);
        if (index != t.shot_index) {
            const bool first = t.shot_index == static_cast<size_t>(-1);
            t.shot_index = index;
            const auto& shot = seq.shots[index];
            if (shot.cut && !first) {
                t.wall_scale = t.wall_target = 1.0;   // the plan already cleared this shot; the live check starts afresh
                // A cut starts on her as she is now, not gliding over from where the last shot's subject was.
                if (const auto k = static_cast<size_t>(shot.track); k && t.track_live && t.track_ok[k]) t.track_at[k] = t.track_now[k];
                breakdown_.depth_range(std::max(sequence_view(planned_pose(seq, t.plan, now), anchor_at(now), now).focus * 6.0, 2500.0));
            }
            if (shot.fstop <= 0 && t.dof_on) { rig_.depth_of_field(t.dolly, 32, 1000); t.dof_on = false; t.focus_applied = -1; }   // deep focus again
            else if (shot.fstop > 0) t.focus_applied = -1;   // a new shot: its aperture and focus are set below
        }
        // The camera is put exactly where the shot has it, every frame. Steering it from the
        // tick (ten straight segments a second) showed as a stutter in every slow move.
        // Something in the way brings the camera closer, and back out when it has gone, as a
        // slow drift either way: a sudden pull-in reads as a jump cut.
        const double dt = std::clamp(f.seconds, 0.0, 0.25);
        t.wall_scale += (t.wall_target - t.wall_scale) * (1.0 - std::exp(-dt / 0.4));
        // The tracked parts of her body: eased between the tick's samples, then smoothed.
        if (t.track_live && (performance_.owned() || rig_.montage_playing())) {
            // A performance can carry her metres in a few frames (After Brute Battle leaps),
            // and the game's own moves lunge and turn her (a parry, a riposte, a stab): the
            // tick's ten samples a second trail her out of the frame, so while one plays the
            // tracked parts are read every frame and the anchor is exact.
            sample_tracks(true);
        } else if (t.track_live) {
            t.track_age += dt;
            const double ease = std::clamp(t.track_age / 0.1, 0.0, 1.0);
            for (size_t k = 1; k < Take::tracks; ++k) {
                if (!t.track_used[k] || !t.track_ok[k]) continue;
                const Vec3 target = t.track_before[k] + (t.track_now[k] - t.track_before[k]) * ease;
                // A part that moves fast (a leap in a performance: metres in a fraction of a
                // second) is followed tightly, or she leaves the frame; a slow one is smoothed.
                const Vec3 step = t.track_now[k] - t.track_before[k];
                const double speed = std::sqrt(step.x * step.x + step.y * step.y + step.z * step.z) / 0.1;
                const double follow = 1.0 - std::exp(-dt / (speed > 250 ? 0.06 : 0.3));
                t.track_at[k] = t.track_at[k] + (target - t.track_at[k]) * follow;
            }
        }
        const auto view = sequence_view(planned_pose(seq, t.plan, now), anchor_at(now), now);
        rig_.place_camera(t.dolly, view.camera, view.rotation);
        if (std::abs(view.fov - t.fov_applied) > 0.02) { rig_.set_fov(t.dolly, view.fov); t.fov_applied = view.fov; }
        // A shot with an aperture keeps its focus on her as the camera comes closer or backs
        // off: set again whenever the distance has moved by more than a hundredth. The focus
        // sits a little short of the point the shot is framed on, on the surface facing the lens.
        if (const auto& shot = seq.shots[t.shot_index]; shot.fstop > 0) {
            const double focus = std::max(20.0, view.focus - 6.0);
            if (t.focus_applied < 0 || std::abs(focus - t.focus_applied) > 0.01 * focus) { rig_.depth_of_field(t.dolly, shot.fstop, focus); t.focus_applied = focus; t.dof_on = true; }
        }
    } catch (...) {}
    if (t.seq_looks) {
        try {
            // An outfit change rides the beat: a quick sweep into a frame pass, the pass held
            // for the beat she changes under, and a quick sweep back on the step's own beat.
            // The pass is only there to hide the change, so it gets no more time than that.
            const double beat = 60.0 / seq.tempo.bpm;
            auto timing = [&](size_t i) {
                struct { double wipe, hold; } out{beat / 4, std::max(beat, 0.45)};
                if (i < seq.looks.size()) { if (seq.looks[i].wipe > 0) out.wipe = seq.looks[i].wipe; if (seq.looks[i].hold > 0) out.hold = seq.looks[i].hold; }
                return out;
            };
            while (t.wearing == Take::Wear::Idle && t.look_next < seq.looks.size()) {
                const auto& look = seq.looks[t.look_next];
                if (!look.outfit.empty()) {
                    const auto [wipe, hold] = timing(t.look_next);
                    if (now < look.at - hold - wipe) break;
                    // The cover takes the whole frame, whatever the breakdown track had up: a
                    // split line there sweeps on to the right edge and comes back afterwards.
                    const auto& covers = t.preset.cover;
                    const LayerSet cover = covers.empty() ? 0 : covers[size_t(t.transitions) % covers.size()];
                    t.covered = look.cover && breakdown_.can_show(cover);
                    if (t.covered) breakdown_.show(cover, wipe, 1.0);
                    ++t.transitions;
                    t.cover_from = now; t.wearing = Take::Wear::Covering;
                } else {
                    if (now < look.at) break;
                    apply_look(look);
                    if (look.titled) t.title = look.title;
                    ++t.look_next;
                }
            }
            if (t.wearing == Take::Wear::Covering) {
                const auto& look = seq.looks[t.look_next];
                // Asked for once the picture is covered: the beat of hold is what it has to land in.
                if (now >= look.at - timing(t.look_next).hold) {
                    // Clay in her own slots would be in the wardrobe mod's way; an overlay is not.
                    if (breakdown_.in_slots()) breakdown_.suspend();
                    // The outfit change would end the performance anyway; ending it here, before
                    // the wear goes out, keeps CSS from reporting it as an interruption.
                    if (performance_.owned()) { try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {} }
                    rig_.stop_montage();   // CSS holds a wear back while a montage is active
                    rig_.css(wear_request(look.outfit, look.variant, false, false));
                    t.wore = true; t.wearing = Take::Wear::Waiting; t.wear_sent_at = now; t.wear_poll = 0; t.adopted = false;
                }
            } else if (t.wearing == Take::Wear::Waiting) {
                const auto& look = seq.looks[t.look_next];
                // Every frame until she has changed: a part of the new outfit gets our overlay
                // the frame it appears, so the cover never shows a piece of the finished look.
                bool landed = t.adopted;
                if (!landed) {
                    breakdown_.reskin();
                    const auto described = rig_.css({{"action", "describe"}});
                    landed = worn(described, look.outfit, look.variant);
                    if (landed) {
                        t.look_now = described; remember_custom(described);
                        breakdown_.adopt(look.outfit + "/" + look.variant);
                        apply_look(look);
                        t.adopted = true;
                    }
                }
                // Back to the picture on the beat once she has changed, or after three seconds whatever happened.
                if ((landed && now >= look.at) || now > t.wear_sent_at + 3.0) {
                    if (!landed) breakdown_.adopt(look.outfit + "/" + look.variant);
                    // Back to what the breakdown track wants now. A step that arrived under the
                    // cover brings its own wipe: the track says how the picture comes back (a cut
                    // on a hit, a sweep on a bar line).
                    const double wipe = t.desired_dirty ? t.desired.wipe : timing(t.look_next).wipe;
                    if (look.titled) t.title = look.title;
                    if (t.desired_dirty && t.desired.titled) t.title = t.desired.title;
                    if (t.covered) { breakdown_.show(t.desired.layers, wipe, t.desired.follow ? t.body_place : t.desired.stop); t.desired_dirty = false; t.transition_until = now + wipe + 0.05; }
                    t.covered = false; t.wearing = Take::Wear::Idle; ++t.look_next;
                    // The new look dances from the top: this instant is the pass's first beat.
                    if (t.performance.id != "off" && !performance_.owned() && !look.hold_performance && !look.restart) {
                        t.performance_at = t.seq_clock;
                        try { performance_.start([&](const Json& r) { return rig_.css(r); }, t.performance); t.performance_resume = false; }
                        catch (...) { t.performance_resume = true; t.performance_retry = t.seq_clock + 0.2; t.performance_give_up = t.seq_clock + 6.0; }
                    }
                }
            }
        } catch (const std::exception& e) {
            t.seq_looks = false; t.wearing = Take::Wear::Idle;
            breakdown_.adopt(t.look_now.is_object() ? t.look_now.value("outfit", std::string()) + "/" + t.look_now.value("variant", std::string()) : std::string());
            if (t.covered) { breakdown_.show(t.desired.layers, 0.2, t.desired.stop); t.desired_dirty = false; t.covered = false; }
            t.note = std::string("Looks stopped: ") + e.what() + ".";
        }
    }
    // Breakdown steps sit on beats: they land on their frame, not on the next tick. While an
    // outfit change has the picture, a step only moves what the cover hands back to.
    while (t.reveal_next < t.reveals.size() && now >= t.reveals[t.reveal_next].at) { t.desired = t.reveals[t.reveal_next++]; t.desired_dirty = true; }
    if (t.desired_dirty && t.wearing == Take::Wear::Idle) {
        t.desired_dirty = false;
        if (t.desired.titled) t.title = t.desired.title;
        // A step on the take's first instant is the opening state: it is there at once, not
        // wiped in over the first beat with the finished look showing underneath.
        const double wipe = t.desired.at <= 1e-6 && now < 0.5 ? 0.0 : t.desired.wipe;
        breakdown_.show(t.desired.layers, wipe, t.desired.follow ? t.body_place : t.desired.stop);
        t.transition_until = std::max(t.transition_until, now + wipe);   // the line follows her once it has arrived
    }
}

// 10 Hz: the world may have moved since the plan was made. One trace a tick: something in the
// way pulls the camera in at once; it eases back out when the way is clear. The camera itself
// is placed every frame (sequence_frame).
void Extension::sequence_tick() {
    auto& t = *take_;
    if (!settings_.walls) return;
    const auto& seq = t.preset.sequence;
    const double when = std::min(t.seq_clock + 0.1, seq.total);
    const auto pose = planned_pose(seq, t.plan, when);
    const Vec3 anchor = anchor_at(when);
    const double keep = t.wall_scale;
    t.wall_scale = 1.0;
    const auto full = sequence_view(pose, anchor, when);
    t.wall_scale = keep;
    const Vec3 aim = to_world(anchor, t.base_yaw, {0, 0, pose.aim + settings_.height});
    double hit = 0;
    // A hit within arm's reach is her own (a hand, a sword at her hip), not something in the way.
    t.wall_target = rig_.trace(aim, full.camera, &hit) && hit > own_reach && hit < full.focus - 5 ? std::clamp((hit - 25) / full.focus, 0.35, 1.0) : 1.0;
}

// After the wardrobe interrupted the dance in a sequence: start it again at the point it
// would have reached, once CSS is free again. Gives up quietly after a few seconds.
void Extension::resume_performance() {
    auto& t = *take_;
    if (!t.performance_resume || performance_.owned() || t.wearing != Take::Wear::Idle) return;
    if (t.seq_clock < t.performance_retry) return;
    t.performance_retry = t.seq_clock + 0.3;
    if (t.seq_clock > t.performance_give_up) { t.performance_resume = false; report("Performance not resumed; the camera take goes on without it.", true); return; }
    PerformanceChoice choice = t.performance;
    choice.seconds = std::max(0.0, t.seq_clock - t.performance_at);
    try { performance_.start([&](const Json& r) { return rig_.css(r); }, choice); t.performance_resume = false; }
    catch (...) {}
}

// The take's first frame. Music starts here, and with music on the clock of a sequence is
// real time from this instant: a long frame (an outfit landing) then costs picture, never sync.
void Extension::roll() {
    auto& t = *take_;
    // A sequence rolls whatever CSS says: a refused start (the layer still blending out of her
    // idle) joins a moment later, in time with the music. Other takes need it at once.
    if (t.preset.kind == ShotKind::Sequence) {
        try { performance_.start([&](const Json& r) { return rig_.css(r); }, t.performance); }
        catch (...) { t.performance_resume = t.performance.id != "off"; t.performance_retry = 0.2; t.performance_give_up = 6.0; }
    } else performance_.start([&](const Json& r) { return rig_.css(r); }, t.performance);
    t.clock = 0; t.seq_clock = 0; t.performance_at = 0;
    mode_ = Mode::Running;
    if (t.preset.kind == ShotKind::Sequence) {
        try {
            sample_tracks(true);
            const auto view = sequence_view(planned_pose(t.preset.sequence, t.plan, 0), anchor_at(0), 0);
            rig_.place_camera(t.dolly, view.camera, view.rotation);
            rig_.set_fov(t.dolly, view.fov); t.fov_applied = view.fov;
            breakdown_.depth_range(std::max(view.focus * 6.0, 2500.0));
        } catch (...) {}
    }
    // A look step on the take's first instant is the opening look: she wears it already (the
    // preparation ended on it), so only its palette, controls and title are left to apply.
    if (t.preset.kind == ShotKind::Sequence && t.seq_looks) {
        const auto& looks = t.preset.sequence.looks;
        while (t.look_next < looks.size() && looks[t.look_next].at <= 1e-6) {
            const auto& look = looks[t.look_next++];
            try { apply_look(look); } catch (...) {}
            if (look.titled) t.title = look.title;
        }
        try { t.opening = rig_.css({{"action", "describe"}}).at("customize"); } catch (...) { t.opening = nullptr; }
    }
    if (!t.preset.audio.empty()) {
        std::string why;
        try {
            if (!t.preset.audio_is_file()) {
                // A pack's SoundWave, loaded while the take prepared.
                if (t.sound.is_null()) throw std::runtime_error(t.preset.audio_asset.empty() ? "install the pack that carries it" : "it did not load");
                rig_.play_music(t.sound);
                t.wall_clock = true; t.rolled_at = std::chrono::steady_clock::now();
            } else {
                const auto info = host_.request({{"op", "extension.info"}});
                if (audio_.play(fs::path(info.at("directory").get<std::string>()) / t.preset.audio, why)) { t.wall_clock = true; t.rolled_at = std::chrono::steady_clock::now(); }
            }
        } catch (const std::exception& e) { why = e.what(); }
        if (!audio_.playing() && !rig_.music_playing()) { const std::string note = "No music (" + t.preset.audio + ": " + why + ")."; t.note = t.note.empty() ? note : t.note + "  " + note; }
    }
    report(with_note("Rolling."));
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
    // A pass that covers an outfit change is a transition, not a breakdown step: no labels.
    const bool transition = take_ && (take_->wearing != Take::Wear::Idle || take_->seq_clock < take_->transition_until);
    const bool wanted = settings_.labels && (!take_ || take_->desired.labels) && breakdown_.active() && breakdown_.shown() != 0 && !transition;
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
            for (size_t k = 0; k < callouts[i].more.size(); ++k) {
                next[i].more_now.push_back(rig_.socket_location(callouts[i].component, callouts[i].more[k]) - frame_.position);
                next[i].more_before.push_back(same && k < labels_[i].more_now.size() ? labels_[i].more_now[k] : next[i].more_now.back());
            }
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

// 10 Hz while a split follows her body: where the parts of her are, and the lens when the
// labels are not measuring it already.
void Extension::sample_body() {
    static_assert(std::size(body_shape) == Take::body_parts, "one sample slot for each part of her body");
    auto& t = *take_;
    try {
        if (!is_handle(t.body_mesh)) t.body_mesh = host_.get(rig_.pawn(), "Mesh");
        for (size_t i = 0; i < Take::body_parts; ++i) {
            const Vec3 now = rig_.socket_location(t.body_mesh, body_shape[i].bone) - frame_.position;
            t.body_before[i] = t.body_live ? t.body_now[i] : now;
            t.body_now[i] = now;
        }
        t.body_age = 0;
        t.body_live = true;
        if (!labels_live_) {
            Vec3 camera; Rotator rotation;
            double cx = 0, cy = 0, ox = 0, oy = 0;
            if (rig_.view_point(camera, rotation)) {
                const auto basis = view_basis(rotation);
                if (rig_.project(lens_point(camera, basis, false), cx, cy) && rig_.project(lens_point(camera, basis, true), ox, oy)) lens_ = measure_lens(cx, cy, ox, oy);
            }
        }
        // What the window's shape makes of the camera's field of view, for shots that fit a height.
        if (lens_.ok && lens_.ky > 1 && viewport_h_ > 1 && t.fov_applied > 1) {
            const double ratio = (viewport_h_ / 2 / lens_.ky) / std::tan(t.fov_applied * std::numbers::pi / 360);
            if (ratio > 0.1 && ratio < 5) t.lens_ratio = ratio;
        }
    } catch (...) { t.body_live = false; }
}

// signature.txt in the extension folder: the first line is the name, the rest small print.
// Read on entering Cine World, so an edit shows without a restart.
void Extension::load_signature() {
    signature_.clear();
    try {
        const auto info = host_.request({{"op", "extension.info"}});
        std::ifstream in(fs::path(info.at("directory").get<std::string>()) / "signature.txt");
        for (std::string line; signature_.size() < max_signature_lines && std::getline(in, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
            if (line.size() > 120) line.resize(120);
            if (!line.empty()) signature_.push_back(std::move(line));
        }
    } catch (...) { signature_.clear(); }
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
    const bool sequence = t.preset.kind == ShotKind::Sequence;
    if (sequence) {
        t.clock = t.seq_clock;   // the frame clock is the take's clock
        if (std::any_of(std::begin(t.track_used) + 1, std::end(t.track_used), [](bool used) { return used; })) sample_tracks(false);
        sequence_tick();
    }
    while (!sequence && t.title_next < t.titles.size() && t.clock >= t.titles[t.title_next].first) t.title = t.titles[t.title_next++].second;
    while (!sequence && t.reveal_next < t.reveals.size() && t.clock >= t.reveals[t.reveal_next].at) {
        const auto& r = t.reveals[t.reveal_next++];
        t.desired = r;   // a split that follows her body is moved from the frame callback
        if (r.titled) t.title = r.title;
        breakdown_.show(r.layers, r.wipe, r.follow ? t.body_place : r.stop);
        if (breakdown_opponent_.active()) breakdown_opponent_.show(r.layers & ~frame_layers, r.wipe);
    }
    if (!t.reveals.empty()) sample_labels();
    if (t.follows || t.fits) sample_body();
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
    } else if (t.preset.kind != ShotKind::Action && !sequence && (!walk || !final_ || t.clock < tl.arrive + 4.0)) {
        place_dolly(0.1, false);
    }
    if (t.clock >= tl.total) finish_take("Take finished.", false);
}

// Put the player's own look back and let CSS save it (persist=true). False when CSS refused.
// `snapshot` is a customization (palette and values), or a whole describe() with the outfit:
// then she wears that outfit again with that customization.
bool Extension::restore_look(const Json& snapshot) {
    try {
        if (snapshot.is_object() && snapshot.contains("outfit"))
            rig_.css({{"action", "wear"}, {"persist", true}, {"outfit", snapshot.at("outfit")}, {"variant", snapshot.at("variant")}, {"customize", snapshot.at("customize")}});
        else rig_.css({{"action", "apply"}, {"persist", true}, {"commands", Json::array({{{"action", "restore"}, {"customize", snapshot}}})}});
        return true;
    }
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
    try { performance_.stop([&](const Json& r) { return rig_.css(r); }); }
    catch (const std::exception& e) { failures.push_back(std::string("animation: ") + e.what()); }
    // Breakdown first: its frame passes sit on the take camera, which the rig destroys next.
    for (Breakdown* b : {&breakdown_, &breakdown_opponent_}) {
        try { for (auto& f : b->end()) failures.push_back(std::move(f)); } catch (const std::exception& e) { failures.push_back(e.what()); }
    }
    callouts_.hide(); labels_live_ = false; labels_.clear(); lens_ = {};
    audio_.stop(); rig_.stop_music();
    if (take_->idle_set) { try { rig_.css({{"action", "idle"}, {"value", ""}}); } catch (...) {} }
    std::string repainted;
    const bool wore = take_->wore && take_->own_look.is_object();
    if (wore) {
        // A sequence wore its own looks: hers goes back whole, outfit and customization, and is saved.
        own_look_owed_ = take_->own_look; own_look_check_ = 0.5;
        if (!restore_look(take_->own_look)) queue_look_restore(take_->own_look);
    } else if (breakdown_.needs_repaint()) {
        // One of her materials could not be put back as it was (its slot was cleared instead).
        if (!take_->look_before.is_null()) {
            if (!restore_look(take_->look_before)) queue_look_restore(take_->look_before);
            repainted = "Her look was repainted after the breakdown.";
        } else repainted = "A material could not be put back after the breakdown; wear the outfit again to refresh it.";
    }
    try { for (auto& f : rig_.restore_take()) failures.push_back(std::move(f)); } catch (const std::exception& e) { failures.push_back(e.what()); }
    if (!take_->snapshot.is_null() && take_->step_next > 0) {
        // Put the modder's own look back and let CSS save it (persist=true).
        if (!restore_look(take_->snapshot)) queue_look_restore(take_->snapshot);   // CSS busy or between looks: retried each second
    }
    try { expression_.stop([&](const Json& r) { return rig_.css(r); }); }
    catch (const std::exception& e) { failures.push_back(std::string("expression: ") + e.what()); }
    std::string note = take_->note;
    if (!repainted.empty()) note = note.empty() ? repainted : note + "  " + repainted;
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

// The performance a take of this preset plays: the preset's own, or the one chosen on the page.
PerformanceChoice Extension::take_performance(const Preset& p) const {
    if (p.performance.empty() || p.performance == "menu") return settings_.performance;
    PerformanceChoice c; c.id = p.performance; c.rate = p.performance_rate;
    return c;
}

std::string Extension::performance_conflict(const Preset& p) const {
    if (take_performance(p).id == "off") return {};
    if (p.kind == ShotKind::Walk || p.kind == ShotKind::Action)
        return "Choose a stationary camera preset for a performance, or set Performance to Off.";
    // A sequence's look steps stop the performance for the change and start it again on the
    // new look; the older look track (palette steps on a glide) cannot, so it stays refused.
    if (settings_.look != "off" && p.kind != ShotKind::Sequence)
        return "Turn the look track off before playing a performance. Change the outfit between takes.";
    return {};
}

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
    if (!busy && p && !performance_conflict(*p).empty()) disabled["take"] = performance_conflict(*p);
    if (!busy && !performance_catalog_.unavailable(settings_.performance).empty()) disabled["take"] = performance_catalog_.unavailable(settings_.performance);
    if (busy || recording) for (const auto* id : {"performance", "performance_library", "performance_rate", "performance_start", "performance_refresh"}) disabled[id] = "Wait for the take or route recording to finish";
    if (busy || recording) for (const auto* id : {"expression", "expression_refresh"}) disabled[id] = "Wait for the take or route recording to finish";
    if (!busy && !expression_catalog_.unavailable(settings_.expression).empty()) disabled["take"] = expression_catalog_.unavailable(settings_.expression);
    if (!busy && settings_.expression != "unchanged" && (settings_.look != "off" || (p && p->kind == ShotKind::Sequence && !p->sequence.looks.empty())))
        disabled["take"] = "Use As set in CSS for takes that change looks, or turn the look track off.";
    if (!route_) disabled["clear_route"] = "No route";
    if (!final_) disabled["clear_final"] = "No final shot";
    Json enabled = Json::object();
    for (const auto& id : {"enter", "exit", "take", "record", "capture", "clear_route", "clear_final", "keep_target", "labels", "breakdown_target",
        "performance", "performance_library", "performance_rate", "performance_start", "performance_refresh", "expression", "expression_refresh"}) enabled[id] = !disabled.contains(id);
    std::string summary = status_;
    if (!preset_problems_.empty()) summary += "  (" + std::to_string(preset_problems_.size()) + " preset file(s) skipped; see CSSX log)";
    return {{"values", {{"status", summary}, {"preset", presets_.empty() ? std::string("none") : settings_.preset},
                        {"preset_info", p ? p->description : std::string()}, {"zoom", settings_.zoom}, {"height", settings_.height},
                        {"fov_scale", settings_.fov_scale}, {"countdown", settings_.countdown}, {"guide", settings_.guide},
                        {"walls", settings_.walls}, {"look", settings_.look}, {"look_info", look}, {"route_info", route}, {"final_info", final_shot},
                        {"scene", settings_.scene}, {"keep_target", settings_.keep_target}, {"scene_info", scene},
                        {"breakdown", settings_.breakdown}, {"labels", settings_.labels}, {"breakdown_target", settings_.breakdown_target}, {"breakdown_info", breakdown},
                        {"signature", settings_.signature}, {"picture", settings_.picture},
                        {"performance", settings_.performance.id}, {"performance_library", performance_catalog_.library(settings_.performance)},
                        {"performance_rate", settings_.performance.rate}, {"performance_start", settings_.performance.seconds},
                        {"performance_info", performance_catalog_.description(settings_.performance)},
                        {"expression", settings_.expression}, {"expression_info", expression_catalog_.description(settings_.expression)}}},
            {"options", {{"expression", expression_catalog_.options(settings_.expression)}, {"preset", options}, {"performance", performance_catalog_.options(settings_.performance)},
                         {"performance_library", performance_catalog_.libraries(settings_.performance)}}},
            {"enabled", enabled}, {"disabled", disabled}, {"status", summary}, {"error", error_}};
}

void Extension::event(const Json& e) {
    const auto id = e.value("id", std::string());
    try {
        error_.clear();
        if (id == "expression" || id == "expression_refresh") {
            if (mode_ != Mode::Idle && mode_ != Mode::Armed) throw std::runtime_error("Wait for the take or route recording to finish");
            if (id == "expression_refresh") expression_catalog_.refresh([&](const Json& r) { return rig_.css(r); });
            else {
                const auto choice = expression_choice(e.at("value"));
                if (const auto why = expression_catalog_.unavailable(choice); !why.empty()) throw std::runtime_error(why);
                settings_.expression = choice; save_state();
            }
        }
        else if (id.starts_with("performance")) {
            if (mode_ != Mode::Idle && mode_ != Mode::Armed) throw std::runtime_error("Wait for the take or route recording to finish");
            auto next = settings_.performance;
            if (id == "performance_refresh") performance_catalog_.refresh([&](const Json& r) { return rig_.css(r); });
            else if (id == "performance") {
                next.id = e.at("value").get<std::string>();
                if (next.id != "off") {
                    const auto* entry = performance_catalog_.find(next.id);
                    if (!entry) throw std::runtime_error("Unknown performance. Refresh animations first.");
                    next.library = entry->library;
                }
                next.seconds = 0;
            } else if (id == "performance_library") {
                const auto value = e.at("value").get<std::string>(); bool found = false;
                for (const auto& o : performance_catalog_.libraries(next)) if (o.at("id") == value) found = true;
                if (!found) throw std::runtime_error("Unknown animation library");
                next.library = value; next.id = "off"; next.seconds = 0;
            } else if (id == "performance_rate") next.rate = e.at("value").get<double>();
            else if (id == "performance_start") next.seconds = e.at("value").get<double>();
            else throw std::runtime_error("Unknown performance control");
            settings_.performance = PerformanceChoice::parse(next.json()); save_state();
        }
        else if (id == "enter") enter();
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
        else if (id == "reload") { preset_order_.clear(); load_presets(); report(packs_loading_ ? std::string("Reading the CINE packs again.") : "Presets reloaded: " + std::to_string(presets_.size()) + "."); }
        else if (id == "preset") { const auto v = e.at("value").get<std::string>(); if (!presets_.contains(v)) throw std::runtime_error("Unknown preset"); settings_.preset = v; save_state(); }
        else if (id == "zoom") { settings_.zoom = clampd(e.at("value").get<double>(), 0.5, 2.0); save_state(); }
        else if (id == "height") { settings_.height = clampd(e.at("value").get<double>(), -150, 150); save_state(); }
        else if (id == "fov_scale") { settings_.fov_scale = clampd(e.at("value").get<double>(), 0.5, 2.0); save_state(); }
        else if (id == "countdown") { settings_.countdown = clampd(e.at("value").get<double>(), 0, 10); save_state(); }
        else if (id == "guide") { const auto v = e.at("value").get<std::string>(); if (!std::count(guide_ids.begin(), guide_ids.end(), v)) throw std::runtime_error("Unknown guide"); settings_.guide = v; save_state(); }
        else if (id == "walls") { settings_.walls = e.at("value").get<bool>(); save_state(); }
        else if (id == "picture") { const auto v = e.at("value").get<std::string>(); if (!std::count(picture_ids.begin(), picture_ids.end(), v)) throw std::runtime_error("Unknown picture mode"); settings_.picture = v; save_state(); }
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
        else if (id == "signature") { settings_.signature = e.at("value").get<bool>(); save_state(); }
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
    // The pieces go now: after this the extension may be unloaded and no tick drains the queue.
    for (Breakdown* b : {&breakdown_, &breakdown_opponent_}) { try { b->flush(); } catch (...) {} }
    if (!look_restore_.is_null() && restore_look(look_restore_)) look_restore_ = nullptr;   // last chance before unload
    audio_.stop(); rig_.stop_music();
    try { performance_.stop([&](const Json& r) { return rig_.css(r); }); } catch (...) {}
    try { if (!expression_.stop([&](const Json& r) { return rig_.css(r); })) return false; }
    catch (...) { return false; }
    stopped_ = true;
    return true;
}

} // namespace cine

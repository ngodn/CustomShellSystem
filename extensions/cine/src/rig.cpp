#include <cstdio>
#include "rig.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>
#include <stdexcept>

namespace cine {
namespace {
// A fixed latent id per camera actor: a new MoveComponentTo with the same (target, UUID)
// retargets the running move in place instead of stopping it.
constexpr int latent_uuid = 7301;
constexpr const char* speed_tag = "Movement.Speed.CinematicWalk";
constexpr const char* hud_class = "/Game/Sparta/UI/Player/WBP_Player_HUD.WBP_Player_HUD_C";
constexpr const char* player_library = "/Game/Sparta/Core/Player/BPFL_Player.BPFL_Player_C";   // load takes the path
constexpr const char* player_library_class = "BPFL_Player_C";                                     // class_default takes the class name
constexpr int visibility_hidden = 2;

Json vec(Vec3 v) { return {{"X", v.x}, {"Y", v.y}, {"Z", v.z}}; }
Json rot(Rotator r) { return {{"Pitch", r.pitch}, {"Yaw", r.yaw}, {"Roll", r.roll}}; }
Vec3 vec3(const Json& j) { return {j.at("X").get<double>(), j.at("Y").get<double>(), j.at("Z").get<double>()}; }
Json transform(Vec3 at) {
    return {{"Rotation", {{"X", 0.0}, {"Y", 0.0}, {"Z", 0.0}, {"W", 1.0}}}, {"Translation", vec(at)},
            {"Scale3D", {{"X", 1.0}, {"Y", 1.0}, {"Z", 1.0}}}};
}
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
}

Json Rig::call(const Json& target, const std::string& function, const Json& args) const { return host_.call(target, function, args); }
Json Rig::call_raw(const Json& target, const std::string& function, const Json& args) const {
    return host_.request({{"op", "call"}, {"target", target}, {"function", function}, {"args", args}});
}
Json Rig::cdo(const std::string& path) const {
    auto h = host_.find(path);
    if (!is_handle(h)) throw std::runtime_error("Missing engine object: " + path);
    return h;
}

void Rig::resolve() {
    const auto p = host_.player();
    if (!p.is_object() || !is_handle(p.value("pawn", Json())) || !is_handle(p.value("controller", Json())))
        throw std::runtime_error("No player in the world yet");
    pawn_ = p.at("pawn"); controller_ = p.at("controller");
    world_ = p.value("world", Json());
    movement_ = host_.get(pawn_, "CharacterMovement");
    mesh_ = host_.get(pawn_, "Mesh");
    camera_manager_ = host_.get(controller_, "PlayerCameraManager");
    statics_ = cdo("/Script/Engine.Default__GameplayStatics");
    kismet_ = cdo("/Script/Engine.Default__KismetSystemLibrary");
    ai_ = cdo("/Script/AIModule.Default__AIBlueprintHelperLibrary");
    widgets_ = cdo("/Script/UMG.Default__WidgetBlueprintLibrary");
    camera_class_ = host_.request({{"op", "load"}, {"path", "/Script/Engine.CameraActor"}});
    host_.request({{"op", "load"}, {"path", player_library}});
    player_lib_ = host_.request({{"op", "class_default"}, {"class", player_library_class}});
    const std::pair<const Json*, const char*> parts[]{{&movement_, "movement"}, {&mesh_, "mesh"}, {&camera_manager_, "camera manager"},
                                                      {&camera_class_, "CameraActor class"}, {&player_lib_, "BPFL_Player"}};
    for (const auto& [h, what] : parts)
        if (!is_handle(*h)) { forget(); throw std::runtime_error(std::string("The player is not fully loaded yet (no ") + what + ")"); }
    // Optional: only action takes and callouts use these, and each use checks its handle.
    auto optional = [&](auto&& read) -> Json { try { auto o = read(); return is_handle(o) ? o : Json(); } catch (...) { return Json(); } };
    lock_ = optional([&] { return host_.get(pawn_, "LockOnComponent"); });
    asc_ = optional([&] { return host_.get(pawn_, "AbilitySystemComponent"); });
    layout_ = optional([&] { return host_.find("/Script/UMG.Default__WidgetLayoutLibrary"); });
    world_settings_ = optional([&] { return is_handle(world_) ? call(world_, "K2_GetWorldSettings", Json::object()) : Json(); });
}

void Rig::forget() {
    world_ = world_settings_ = lock_ = asc_ = layout_ = Json();
    hidden_components_.clear(); hidden_widgets_.clear(); parts_.clear();
    time_scaled_ = shake_set_ = false;
    pawn_ = controller_ = movement_ = mesh_ = camera_manager_ = Json();
    statics_ = kismet_ = ai_ = widgets_ = player_lib_ = camera_class_ = Json();
    cameras_.clear(); hud_.clear();
    hud_hidden_ = view_changed_ = speed_set_ = moving_ = false;
    force_walk_sets_ = 0;
}

void Rig::hide_hud() {
    if (hud_hidden_) return;
    const auto cls = host_.request({{"op", "load"}, {"path", hud_class}});
    if (!is_handle(cls)) return;   // a game update renamed the HUD: leave it, never guess
    const auto found = call_raw(widgets_, "GetAllWidgetsOfClass", {{"WorldContextObject", controller_}, {"WidgetClass", cls}, {"TopLevelOnly", false}});
    hud_.clear();
    for (const auto& w : found.value("FoundWidgets", Json::array())) {
        if (!is_handle(w)) continue;
        const int before = call(w, "GetVisibility", Json::object()).get<int>();
        hud_.emplace_back(w, before);
        call(w, "SetVisibility", {{"InVisibility", visibility_hidden}});
    }
    hud_hidden_ = true;
}

void Rig::restore_hud() {
    for (const auto& [widget, visibility] : hud_) {
        try {
            if (host_.request({{"op", "valid"}, {"target", widget}}).get<bool>()) call(widget, "SetVisibility", {{"InVisibility", visibility}});
        } catch (...) {}
    }
    hud_.clear(); hud_hidden_ = false;
}

Json Rig::spawn_camera(Vec3 position, Rotator rotation, double fov) {
    const auto xf = transform(position);
    const auto actor = call(statics_, "BeginDeferredActorSpawnFromClass",
                            {{"WorldContextObject", controller_}, {"ActorClass", camera_class_}, {"SpawnTransform", xf},
                             {"CollisionHandlingOverride", 1}, {"Owner", nullptr}, {"TransformScaleMethod", 1}});
    if (!is_handle(actor)) throw std::runtime_error("Could not create a camera");
    cameras_.push_back(actor);   // owned from here, even if a later call fails
    call(statics_, "FinishSpawningActor", {{"Actor", actor}, {"SpawnTransform", xf}, {"TransformScaleMethod", 1}});
    place_camera(actor, position, rotation);
    const auto component = host_.get(actor, "CameraComponent");
    host_.set(component, "bConstrainAspectRatio", false);
    host_.set(component, "FieldOfView", fov);
    return actor;
}

Json Rig::spawn_attached(Vec3 relative, Rotator relative_rotation, double fov) {
    const auto here = vec3(call(pawn_, "K2_GetActorLocation", Json::object()));
    const auto actor = spawn_camera(here, {}, fov);
    // Snap onto her: KeepRelative after spawning at her location adds her position twice.
    call(actor, "K2_AttachToActor", {{"ParentActor", pawn_}, {"SocketName", "None"}, {"LocationRule", 2}, {"RotationRule", 2},
                                     {"ScaleRule", 0}, {"bWeldSimulatedBodies", false}});
    call(host_.get(actor, "RootComponent"), "K2_SetRelativeLocationAndRotation",
         {{"NewLocation", vec(relative)}, {"NewRotation", rot(relative_rotation)}, {"bSweep", false}, {"bTeleport", true}});
    return actor;
}

void Rig::place_camera(const Json& camera, Vec3 position, Rotator rotation) {
    call(camera, "K2_SetActorLocationAndRotation", {{"NewLocation", vec(position)}, {"NewRotation", rot(rotation)}, {"bSweep", false}, {"bTeleport", true}});
}

void Rig::move_camera(const Json& camera, Vec3 position, Rotator rotation, double over_time) {
    call(kismet_, "MoveComponentTo",
         {{"Component", part(camera, "RootComponent")}, {"TargetRelativeLocation", vec(position)}, {"TargetRelativeRotation", rot(rotation)},
          {"bEaseOut", false}, {"bEaseIn", false}, {"OverTime", over_time}, {"bForceShortestRotationPath", true}, {"MoveAction", 0},
          {"LatentInfo", {{"Linkage", -1}, {"UUID", latent_uuid}, {"ExecutionFunction", "None"}, {"CallbackTarget", camera}}}});
}

void Rig::set_fov(const Json& camera, double fov) { host_.set(host_.get(camera, "CameraComponent"), "FieldOfView", fov); }

void Rig::view(const Json& target, double blend) {
    call(controller_, "SetViewTargetWithBlend", {{"NewViewTarget", target}, {"BlendTime", blend}, {"BlendFunc", 4}, {"BlendExp", 2.0}, {"bLockOutgoing", false}});
    view_changed_ = true;
}
void Rig::view_player() {
    call(controller_, "SetViewTargetWithBlend", {{"NewViewTarget", pawn_}, {"BlendTime", 0.0}, {"BlendFunc", 0}, {"BlendExp", 0.0}, {"bLockOutgoing", false}});
    view_changed_ = false;
}

void Rig::force_walk() { call(player_lib_, "SetForceWalk", {{"__WorldContext", controller_}}); ++force_walk_sets_; }
void Rig::speed_limit(double cm_s) {
    call(movement_, "SetMaxSpeedAdjustment", {{"ID", {{"TagName", speed_tag}}}, {"Speed", cm_s}, {"DirectionalSpeedModifierCurve", nullptr}});
    speed_set_ = true;
}
void Rig::move_to(Vec3 goal) { call(ai_, "SimpleMoveToLocation", {{"Controller", controller_}, {"Goal", vec(goal)}}); moving_ = true; }
void Rig::stop_moving() { call(controller_, "StopMovement", Json::object()); moving_ = false; }
void Rig::control_yaw(double yaw) { call(controller_, "SetControlRotation", {{"NewRotation", rot({0, yaw, 0})}}); }
void Rig::place_player(Vec3 position, double yaw) {
    call(pawn_, "K2_SetActorLocationAndRotation", {{"NewLocation", vec(position)}, {"NewRotation", rot({0, yaw, 0})}, {"bSweep", false}, {"bTeleport", true}});
    control_yaw(yaw);
}

bool Rig::trace(Vec3 from, Vec3 to, double* distance) {
    const Json clear{{"R", 0}, {"G", 0}, {"B", 0}, {"A", 0}};
    // Passing her as the world context makes bIgnoreSelf skip her (the bridge cannot grow
    // an ActorsToIgnore array).
    const auto r = call_raw(kismet_, "LineTraceSingle",
                            {{"WorldContextObject", pawn_}, {"Start", vec(from)}, {"End", vec(to)}, {"TraceChannel", 1}, {"bTraceComplex", false},
                             {"ActorsToIgnore", Json::array()}, {"DrawDebugType", 0}, {"bIgnoreSelf", true}, {"TraceColor", clear},
                             {"TraceHitColor", clear}, {"DrawTime", 0.0}});
    const bool hit = r.value("ReturnValue", false);
    if (distance) *distance = hit ? r.at("OutHit").value("Distance", 0.0) : 0.0;
    return hit;
}

Vec3 Rig::head_position() { return vec3(call(mesh_, "GetSocketLocation", {{"InSocketName", "head"}})); }

FinalShot Rig::capture_final(Vec3 her, double her_yaw) {
    const auto at = vec3(call(camera_manager_, "GetCameraLocation", Json::object()));
    const auto r = call(camera_manager_, "GetCameraRotation", Json::object());
    FinalShot f;
    f.position = her; f.yaw = her_yaw;
    const Vec3 d = at - her;
    const double c = std::cos(her_yaw * std::numbers::pi / 180), s = std::sin(her_yaw * std::numbers::pi / 180);
    f.camera = {d.x * c + d.y * s, -d.x * s + d.y * c, d.z};
    f.pitch = r.at("Pitch").get<double>();
    f.camera_yaw = wrap_degrees(r.at("Yaw").get<double>() - her_yaw);
    f.fov = call(camera_manager_, "GetFOVAngle", Json::object()).get<double>();
    return f;
}

// CSS's look service, through the CSSX service bus. CINE works without it: the caller turns
// the look track off with the reason below.
Json Rig::css(const Json& request) {
    try { return host_.request({{"op", "service.call"}, {"service", "css.customize"}, {"version", 1}, {"request", request}}); }
    catch (const std::exception& e) {
        const std::string_view why = e.what();
        if (why.starts_with("No service named")) throw std::runtime_error("CSS 1.0.0-beta.9 or newer is not running");
        if (why.find("service.call") != std::string_view::npos) throw std::runtime_error("the look track needs CSSX 1.3.0 or newer");
        throw;
    }
}

// ---------------------------------------------------------------- action takes and callouts

Json Rig::part(const Json& actor, const char* property) {
    if (!is_handle(actor)) return Json();
    const auto key = std::make_pair(actor["$object"].get<uint64_t>(), std::string(property));
    if (const auto it = parts_.find(key); it != parts_.end()) return it->second;
    auto found = host_.get(actor, property);
    if (is_handle(found) && parts_.size() < 64) parts_[key] = found;
    return found;
}
Json Rig::locked_target() {
    if (!is_handle(lock_)) return Json();
    const auto target = host_.get(lock_, "LockedTarget");
    return is_handle(target) ? target : Json();
}
Vec3 Rig::actor_location(const Json& actor) { return vec3(call(actor, "K2_GetActorLocation", Json::object())); }
double Rig::health_fraction(const Json& actor) {
    try {
        const auto health = part(actor, "HealthComponent");
        if (!is_handle(health)) return -1;
        const auto value = call(health, "GetHealthNormalized", Json::object());
        return value.is_number() ? value.get<double>() : -1;
    } catch (...) { return -1; }
}
bool Rig::dead(const Json& actor) {
    try {
        const auto health = part(actor, "HealthComponent");
        const auto value = is_handle(health) ? call(health, "IsDeadOrDying", Json::object()) : Json(false);
        return value.is_boolean() && value.get<bool>();
    } catch (...) { return true; }   // gone is as good as dead
}
int Rig::tag_count(const char* tag) {
    if (!is_handle(asc_)) return 0;
    const auto count = call(asc_, "GetGameplayTagCount", {{"GameplayTag", {{"TagName", tag}}}});
    return count.is_number() ? count.get<int>() : 0;
}
void Rig::time_scale(double scale) {
    if (!is_handle(world_settings_)) return;
    host_.set(world_settings_, "CinematicTimeDilation", std::clamp(scale, 0.05, 1.0));
    time_scaled_ = scale < 0.999;
}
void Rig::shake_scale(double scale) {
    if (!shake_set_) {
        const auto before = call(camera_manager_, "GetGlobalCameraShakeScale", Json::object());
        shake_before_ = before.is_number() ? before.get<double>() : 1.0;
        shake_set_ = true;
    }
    call(camera_manager_, "SetGlobalCameraShakeScale", {{"NewGlobalCameraShakeScale", std::clamp(scale, 0.0, 2.0)}});
}
Json Rig::camera_component(const Json& camera) { return host_.get(camera, "CameraComponent"); }
void Rig::depth_of_field(const Json& camera, double fstop, double focus_cm) {
    const auto component = camera_component(camera);
    const Json settings{{"bOverride_DepthOfFieldFstop", true}, {"DepthOfFieldFstop", std::clamp(fstop, 0.7, 32.0)},
                        {"bOverride_DepthOfFieldFocalDistance", true}, {"DepthOfFieldFocalDistance", std::max(10.0, focus_cm)}};
    try {
        host_.request({{"op", "set"}, {"target", component}, {"property", "PostProcessSettings"}, {"value", settings}, {"echo", false}});
    } catch (const std::exception& e) {
        // CSSX 1.3.0 writes the fields, then fails echoing a struct of more than 256 fields.
        if (std::string_view(e.what()).find("Struct field exceeds bounds") == std::string_view::npos) throw;
    }
}
bool Rig::viewing(const Json& camera) {
    const auto active = host_.get(camera_manager_, "ActiveCameraActor");
    return is_handle(active) && is_handle(camera) && active["$object"] == camera["$object"];
}
void Rig::hide_target_ui(const Json& enemy) {
    try {
        const auto bar = host_.get(enemy, "Enemy Health Widget");
        if (is_handle(bar)) {
            const auto visible = call(bar, "IsVisible", Json::object());
            hidden_components_.emplace_back(bar, !visible.is_boolean() || visible.get<bool>());
            call(bar, "SetVisibility", {{"bNewVisibility", false}, {"bPropagateToChildren", true}});
        }
    } catch (...) {}
    try {
        const auto marker = host_.get(enemy, "BPC_LockOnTarget");
        const auto widget = is_handle(marker) ? host_.get(marker, "WBP_LockOnIndicator") : Json();
        if (is_handle(widget)) {
            const auto before = call(widget, "GetVisibility", Json::object());
            hidden_widgets_.emplace_back(widget, before.is_number() ? before.get<int>() : 0);
            call(widget, "SetVisibility", {{"InVisibility", visibility_hidden}});
        }
    } catch (...) {}
}
Vec3 Rig::socket_location(const Json& component, const std::string& socket) {
    return vec3(call(component, "GetSocketLocation", {{"InSocketName", socket.empty() ? std::string("None") : socket}}));
}
bool Rig::asset_present(const std::string& path) const {
    try { return is_handle(host_.request({{"op", "load"}, {"path", path}})); } catch (...) { return false; }
}
bool Rig::view_point(Vec3& location, Rotator& rotation) {
    const auto reply = host_.request({{"op", "call"}, {"target", controller_}, {"function", "GetPlayerViewPoint"}, {"args", Json::object()}});
    if (!reply.is_object() || !reply.contains("Location") || !reply.contains("Rotation")) return false;
    location = vec3(reply["Location"]);
    const auto& r = reply["Rotation"];
    rotation = {r.value("Pitch", 0.0), r.value("Yaw", 0.0), r.value("Roll", 0.0)};
    return true;
}
bool Rig::project(Vec3 world, double& x, double& y) {
    if (!is_handle(layout_)) return false;
    const auto reply = host_.request({{"op", "call"}, {"target", layout_}, {"function", "ProjectWorldLocationToWidgetPosition"},
                                      {"args", {{"PlayerController", controller_}, {"WorldLocation", vec(world)}, {"bPlayerViewportRelative", false}}}});
    if (!reply.is_object() || !reply.value("ReturnValue", false) || !reply.contains("ScreenPosition")) return false;
    x = reply["ScreenPosition"].value("X", 0.0); y = reply["ScreenPosition"].value("Y", 0.0);
    return true;
}

bool Rig::key_down(const std::vector<std::string>& keys, std::vector<bool>& out) {
    out.assign(keys.size(), false);
    if (!is_handle(controller_)) return false;
    const auto r = host_.request({{"op", "input.keys"}, {"target", controller_}, {"keys", keys}});
    for (size_t i = 0; i < keys.size(); ++i) out[i] = r.value(keys[i], false);
    return true;
}

bool Rig::owns_take_changes() const {
    return !cameras_.empty() || view_changed_ || speed_set_ || moving_ || force_walk_sets_ > 0 || time_scaled_ || shake_set_
        || !hidden_components_.empty() || !hidden_widgets_.empty();
}

namespace {
// A console variable CINE raises for a take: the value for sharp shadows, the value for the
// full picture (`leave` at a level leaves it alone), and whether lower is the better direction.
struct PictureSetting { const char* name; double sharp, full; bool lower_is_better; };
constexpr double leave = -1e9;
// What a held close-up showed in this game (work/live/quality_probe.py, 6 Oct): the shadow of
// hair and lace on skin is stair-stepped and dotted because the light's shadow map is too
// coarse for a lens that close, at any resolution setting. What clears it is more shadow
// rays with a wider texel dither, the engine's own answer to "insufficient shadow
// resolution". Nothing here resizes a render target or the shadow page pool: a take that
// raised the pool (512 to 4096 pages, back at its end) ended in a GPU crash the same night,
// and the pool made no visible difference in the probe.
constexpr PictureSetting picture_settings[] = {
    {"r.Shadow.Virtual.SMRT.RayCountDirectional", 16, 16, false},
    {"r.Shadow.Virtual.SMRT.SamplesPerRayDirectional", 8, 8, false},
    {"r.Shadow.Virtual.SMRT.RayCountLocal", 16, 16, false},
    {"r.Shadow.Virtual.SMRT.SamplesPerRayLocal", 8, 8, false},
    {"r.Shadow.Virtual.SMRT.TexelDitherScaleDirectional", 12, 12, false},
    {"r.Shadow.Virtual.SMRT.TexelDitherScaleLocal", 12, 12, false},
    {"r.ScreenPercentage", leave, 100, false},
};
std::string console_number(double value) {
    char text[32];
    std::snprintf(text, sizeof text, "%.6g", value);
    return text;
}
}

void Rig::picture(int level) {
    if (level <= 0 || !picture_saved_.empty()) return;
    const auto kismet = cdo("/Script/Engine.Default__KismetSystemLibrary");
    for (const auto& setting : picture_settings) {
        const double target = level >= 2 ? setting.full : setting.sharp;
        if (target == leave) continue;
        try {
            const auto now = call(kismet, "GetConsoleVariableFloatValue", {{"VariableName", setting.name}});
            if (!now.is_number()) continue;
            const double value = now.get<double>();
            if (setting.lower_is_better ? value <= target : value >= target) continue;   // already as good or better
            call(kismet, "ExecuteConsoleCommand", {{"WorldContextObject", pawn_}, {"Command", std::string(setting.name) + " " + console_number(target)}, {"SpecificPlayer", nullptr}});
            picture_saved_.emplace_back(setting.name, value);
        } catch (...) {}   // a variable this build does not have: the picture stays as the game renders it
    }
}

std::vector<std::string> Rig::restore_picture() {
    std::vector<std::string> failures;
    if (picture_saved_.empty()) return failures;
    Json kismet;
    try { kismet = cdo("/Script/Engine.Default__KismetSystemLibrary"); } catch (const std::exception& e) { failures.push_back(std::string("picture: ") + e.what()); return failures; }
    bool pawn_alive = false;
    try { pawn_alive = pawn_.is_object() && host_.request({{"op", "valid"}, {"target", pawn_}}).get<bool>(); } catch (...) {}
    for (const auto& [name, value] : picture_saved_) {
        try { call(kismet, "ExecuteConsoleCommand", {{"WorldContextObject", pawn_alive ? pawn_ : Json(nullptr)}, {"Command", name + " " + console_number(value)}, {"SpecificPlayer", nullptr}}); }
        catch (const std::exception& e) { failures.push_back("picture " + name + ": " + e.what()); }
    }
    picture_saved_.clear();
    return failures;
}

std::vector<std::string> Rig::restore_take() {
    std::vector<std::string> failures;
    auto step = [&](const char* what, auto&& fn) { try { fn(); } catch (const std::exception& e) { failures.push_back(std::string(what) + ": " + e.what()); } };
    for (auto& failure : restore_picture()) failures.push_back(std::move(failure));
    if (moving_) step("stop walking", [&] { stop_moving(); });
    if (speed_set_) step("speed limit", [&] { call(movement_, "RemoveMaxSpeedAdjustment", {{"ID", {{"TagName", speed_tag}}}}); speed_set_ = false; });
    if (force_walk_sets_ > 0) step("force walk", [&] {
        // It may have been set more than once; clear until the game reports it off.
        for (int i = 0; i < 4; ++i) {
            call(player_lib_, "RemoveForceWalk", {{"__WorldContext", controller_}});
            if (!call(player_lib_, "HasForcedWalk", {{"__WorldContext", controller_}}).get<bool>()) break;
        }
        force_walk_sets_ = 0;
    });
    if (time_scaled_) step("time", [&] { host_.set(world_settings_, "CinematicTimeDilation", 1.0); time_scaled_ = false; });
    if (shake_set_) step("camera shake", [&] { call(camera_manager_, "SetGlobalCameraShakeScale", {{"NewGlobalCameraShakeScale", shake_before_}}); shake_set_ = false; });
    for (const auto& [component, visible] : hidden_components_) step("health bar", [&] {
        if (visible && host_.request({{"op", "valid"}, {"target", component}}).get<bool>()) call(component, "SetVisibility", {{"bNewVisibility", true}, {"bPropagateToChildren", true}});
    });
    for (const auto& [widget, visibility] : hidden_widgets_) step("lock-on marker", [&] {
        if (host_.request({{"op", "valid"}, {"target", widget}}).get<bool>()) call(widget, "SetVisibility", {{"InVisibility", visibility}});
    });
    hidden_components_.clear(); hidden_widgets_.clear(); parts_.clear();
    if (view_changed_) step("view", [&] { view_player(); });
    for (const auto& camera : cameras_) step("camera", [&] {
        if (host_.request({{"op", "valid"}, {"target", camera}}).get<bool>()) call(camera, "K2_DestroyActor", Json::object());
    });
    cameras_.clear();
    moving_ = speed_set_ = view_changed_ = false;
    return failures;
}

std::vector<std::string> Rig::restore_all() {
    auto failures = restore_take();
    try { restore_hud(); } catch (const std::exception& e) { failures.push_back(std::string("HUD: ") + e.what()); }
    return failures;
}

} // namespace cine

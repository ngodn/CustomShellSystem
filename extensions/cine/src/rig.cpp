#include "rig.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cine {
namespace {
// A fixed latent id per camera actor: a new MoveComponentTo with the same (target, UUID)
// retargets the running move in place instead of stopping it.
constexpr int latent_uuid = 7301;
constexpr const char* speed_tag = "Movement.Speed.CinematicWalk";
constexpr const char* hud_class = "/Game/Sparta/UI/Player/WBP_Player_HUD.WBP_Player_HUD_C";
constexpr const char* player_library = "/Game/Sparta/Core/Player/BPFL_Player.BPFL_Player_C";
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
    movement_ = host_.get(pawn_, "CharacterMovement");
    mesh_ = host_.get(pawn_, "Mesh");
    camera_manager_ = host_.get(controller_, "PlayerCameraManager");
    statics_ = cdo("/Script/Engine.Default__GameplayStatics");
    kismet_ = cdo("/Script/Engine.Default__KismetSystemLibrary");
    ai_ = cdo("/Script/AIModule.Default__AIBlueprintHelperLibrary");
    widgets_ = cdo("/Script/UMG.Default__WidgetBlueprintLibrary");
    camera_class_ = host_.request({{"op", "load"}, {"path", "/Script/Engine.CameraActor"}});
    host_.request({{"op", "load"}, {"path", player_library}});
    player_lib_ = host_.request({{"op", "class_default"}, {"class", player_library}});
    for (const auto* h : {&movement_, &mesh_, &camera_manager_, &camera_class_, &player_lib_})
        if (!is_handle(*h)) { forget(); throw std::runtime_error("The player is not fully loaded yet"); }
}

void Rig::forget() {
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
         {{"Component", host_.get(camera, "RootComponent")}, {"TargetRelativeLocation", vec(position)}, {"TargetRelativeRotation", rot(rotation)},
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

Json Rig::css(const Json& request) { return host_.request({{"op", "css.customize"}, {"request", request}}); }

bool Rig::key_down(const std::vector<std::string>& keys, std::vector<bool>& out) {
    out.assign(keys.size(), false);
    if (!is_handle(controller_)) return false;
    const auto r = host_.request({{"op", "input.keys"}, {"target", controller_}, {"keys", keys}});
    for (size_t i = 0; i < keys.size(); ++i) out[i] = r.value(keys[i], false);
    return true;
}

bool Rig::owns_take_changes() const { return !cameras_.empty() || view_changed_ || speed_set_ || moving_ || force_walk_sets_ > 0; }

std::vector<std::string> Rig::restore_take() {
    std::vector<std::string> failures;
    auto step = [&](const char* what, auto&& fn) { try { fn(); } catch (const std::exception& e) { failures.push_back(std::string(what) + ": " + e.what()); } };
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

#include "teleport.hpp"

// The meteor arrival transition, reproduced from the game's own launcher dive (read
// off a live traverse): the player stays VISIBLE and plays the skydive montage
// (A_Shared_Skydive_Sequence_Full_Montage) with the comet (PS_Skydive_Comet)
// attached, under the game's animation camera state; on touchdown it plays the
// 4-point landing montage under the combat camera state, with the game's impact VFX,
// rumble, audio and shake. The character is never hidden (that is how the real dive
// looks). A short warmup lets the streaming teleport finish loading and fading in, so
// the whole descent is on screen instead of behind the load. Every asset is
// shell-agnostic, so a stock shell and a CSS custom shell behave the same. The fall
// is driven once per rendered frame (smooth) and cleanup_meteor() restores all state
// on any exit path, so the player can never be left flying, rumbling or stuck in a
// dive camera.

namespace teleport {

namespace {
bool is_object(const Json& j) { return j.is_object() && j.contains("$object"); }
constexpr int MOVE_Walking = 1, MOVE_Flying = 5;
constexpr int ATTACH_KeepRelative = 0;
const char* kDiveTag = "Traverse.MeteorDive";
const char* kLandTag = "Traverse.MeteorLand";
}

bool Extension::load_meteor_assets() {
    if (meteor_assets_ready_) return true;
    auto load = [&](const char* path) -> Json {
        try { auto o = host_.request({{"op", "load"}, {"path", path}}); return is_object(o) ? o : Json(); }
        catch (...) { return Json(); }
    };
    try {
        niagara_lib_    = host_.find("/Script/Niagara.Default__NiagaraFunctionLibrary");
        ww_statics_     = host_.find("/Script/WaveWeaver.Default__WaveWeaverStatics");
        cam_anim_class_   = load("/Game/Sparta/Core/Camera/CameraStates/CameraState_Animation.CameraState_Animation_C");
        cam_combat_class_ = load("/Game/Sparta/Core/Camera/CameraStates/CameraState_Combat.CameraState_Combat_C");
        a_comet_        = load("/Game/Sparta/FX/VFX/SkyDive/Dive/Particles/PS_Skydive_Comet.PS_Skydive_Comet");
        a_impact_       = load("/Game/Sparta/FX/VFX/SkyDive/Impact/Particles/PS_SkyDive_Impact.PS_SkyDive_Impact");
        a_dive_montage_ = load("/Game/Sparta/FX/VFX/SkyDive/Animation/SkyDive/A_Shared_Skydive_Sequence_Full_Montage.A_Shared_Skydive_Sequence_Full_Montage");
        a_land_montage_ = load("/Game/Sparta/Characters/Shells/_Shared/Animation/Traversal/Landing/A_Shared_Traversal_Landing_4point_Montage.A_Shared_Traversal_Landing_4point_Montage");
        a_ff_dive_      = load("/Game/Sparta/Core/ForceFeedback/FF_Launcher_Dive.FF_Launcher_Dive");
        a_ff_land_      = load("/Game/Sparta/Core/ForceFeedback/FF_Launcher_Landing.FF_Launcher_Landing");
        a_shake_        = load("/Game/Sparta/Core/Camera/CameraShakes/CameraShake_SkyDive_Sequence.CameraShake_SkyDive_Sequence_C");
        a_snd_land_     = load("/Game/Sparta/Core/Interaction/Audio/SkyDive/AE_Interactable_Skydive_Player_Land.AE_Interactable_Skydive_Player_Land");
    } catch (...) {}
    // The dive montage, the landing montage, the comet and the Niagara library are the
    // parts we cannot fake; the rest degrade gracefully if a path ever moves.
    meteor_assets_ready_ = is_object(a_dive_montage_) && is_object(a_land_montage_)
                        && is_object(a_comet_) && is_object(niagara_lib_);
    return meteor_assets_ready_;
}

void Extension::begin_meteor(const Json& ground_loc, const Json& rot) {
    if (!ground_loc.is_object()) return;
    try {
        auto p = host_.player();
        meteor_pawn_ = p.value("pawn", Json());
        if (!is_object(meteor_pawn_)) { meteor_pawn_ = nullptr; return; }
        try { meteor_mesh_ = host_.get(meteor_pawn_, "Mesh"); } catch (...) { meteor_mesh_ = nullptr; }
        try { meteor_scm_ = host_.call(controller_, "GetSpartaCameraManager"); } catch (...) { meteor_scm_ = nullptr; }

        meteor_x_ = ground_loc.value("X", 0.0);
        meteor_y_ = ground_loc.value("Y", 0.0);
        meteor_ground_z_ = ground_loc.value("Z", 0.0);
        meteor_yaw_ = rot.is_object() ? rot.value("Yaw", 0.0) : 0.0;

        // Hide the character only through the warmup: the streaming teleport's fade
        // is black over that window, so hiding is invisible and it avoids the player
        // being seen standing at the destination before the dive lifts them up. It is
        // shown again the instant the visible descent starts. Stop gravity so our
        // per-frame descent is the only thing that moves them.
        try { host_.call(meteor_pawn_, "SetActorHiddenInGame", {{"bNewHidden", true}}); } catch (...) {}
        try { auto mv = host_.get(meteor_pawn_, "CharacterMovement");
              if (is_object(mv)) host_.call(mv, "SetMovementMode", {{"NewMovementMode", MOVE_Flying}, {"NewCustomMode", 0}}); } catch (...) {}

        meteor_phase_ = MeteorPhase::Warmup;
        meteor_warmup_ = kMeteorWarmup;
        meteor_elapsed_ = 0;
        meteor_comet_ = nullptr;
    } catch (...) { meteor_phase_ = MeteorPhase::Idle; }
}

void Extension::tick_meteor(double seconds) {
    if (meteor_phase_ == MeteorPhase::Idle) return;

    // Settling only holds the landing camera for a beat, then releases it. It needs
    // no pawn, so handle it before the pawn-validity guard.
    if (meteor_phase_ == MeteorPhase::Settling) {
        meteor_settle_ -= seconds;
        if (meteor_settle_ > 0) return;
        if (is_object(cam_combat_class_) && is_object(meteor_scm_))
            try { host_.call(meteor_scm_, "RemoveAllCameraStatesByClass", {{"CameraState", cam_combat_class_}}); } catch (...) {}
        meteor_phase_ = MeteorPhase::Idle;
        meteor_scm_ = nullptr; meteor_pawn_ = nullptr; meteor_mesh_ = nullptr;
        return;
    }

    bool pawn_ok = false;
    try { pawn_ok = is_object(meteor_pawn_) && host_.request({{"op", "valid"}, {"target", meteor_pawn_}}) == true; } catch (...) {}
    if (!pawn_ok) { cleanup_meteor(false); return; }

    if (meteor_phase_ == MeteorPhase::Warmup) {
        meteor_warmup_ -= seconds;
        if (meteor_warmup_ > 0) return;
        // The world has loaded and faded in: lift into the sky in the skydive pose,
        // attach the comet, take the animation camera and start the dive rumble.
        const Json top = {{"X", meteor_x_}, {"Y", meteor_y_}, {"Z", meteor_ground_z_ + kMeteorHeight}};
        try { host_.call(meteor_pawn_, "K2_SetActorLocation", {{"NewLocation", top}, {"bSweep", false}, {"bTeleport", true}}); } catch (...) {}
        try { host_.call(meteor_pawn_, "SetActorHiddenInGame", {{"bNewHidden", false}}); } catch (...) {}   // reveal for the visible dive
        try { host_.call(meteor_pawn_, "PlayAnimMontage", {{"AnimMontage", a_dive_montage_}, {"InPlayRate", 1.0}, {"StartSectionName", "None"}}); } catch (...) {}
        if (is_object(meteor_mesh_)) {
            try {
                meteor_comet_ = host_.call(niagara_lib_, "SpawnSystemAttached",
                    {{"SystemTemplate", a_comet_}, {"AttachToComponent", meteor_mesh_}, {"AttachPointName", "None"},
                     {"Location", {{"X", 0.0}, {"Y", 0.0}, {"Z", 0.0}}}, {"Rotation", {{"Pitch", 0.0}, {"Yaw", 0.0}, {"Roll", 0.0}}},
                     {"LocationType", ATTACH_KeepRelative}, {"bAutoDestroy", false}, {"PoolingMethod", 0}, {"bAutoActivate", true}, {"bPreCullCheck", false}});
            } catch (...) { meteor_comet_ = nullptr; }
        }
        if (is_object(cam_anim_class_) && is_object(meteor_scm_))
            try { host_.call(meteor_scm_, "AddCameraStateByClass", {{"CameraState", cam_anim_class_}}); } catch (...) {}
        if (is_object(a_ff_dive_))
            try { host_.call(controller_, "K2_ClientPlayForceFeedback",
                {{"ForceFeedbackEffect", a_ff_dive_}, {"Tag", kDiveTag}, {"bLooping", true}, {"bIgnoreTimeDilation", false}, {"bPlayWhilePaused", false}}); } catch (...) {}
        meteor_phase_ = MeteorPhase::Descending;
        meteor_elapsed_ = 0;
        return;
    }

    // Descending: accelerate straight down (ease-in) for a meteor fall. The comet is
    // attached to the mesh, so moving the pawn carries it.
    meteor_elapsed_ += seconds;
    double t = meteor_elapsed_ / kMeteorFall;
    if (t > 1.0) t = 1.0;
    const double z = meteor_ground_z_ + kMeteorHeight * (1.0 - t * t);
    const Json here = {{"X", meteor_x_}, {"Y", meteor_y_}, {"Z", z}};
    try { host_.call(meteor_pawn_, "K2_SetActorLocation", {{"NewLocation", here}, {"bSweep", false}, {"bTeleport", true}}); } catch (...) {}
    if (t >= 1.0) cleanup_meteor(true);
}

void Extension::cleanup_meteor(bool landed) {
    const bool was_active = meteor_phase_ != MeteorPhase::Idle && meteor_phase_ != MeteorPhase::Settling;
    // Tear down the comet and the looping dive rumble first (needed on every path).
    if (is_object(meteor_comet_)) {
        try { if (host_.request({{"op", "valid"}, {"target", meteor_comet_}}) == true) host_.call(meteor_comet_, "DestroyComponent"); } catch (...) {}
    }
    meteor_comet_ = nullptr;
    if (is_object(a_ff_dive_))
        try { host_.call(controller_, "ClientStopForceFeedback", {{"ForceFeedbackEffect", a_ff_dive_}, {"Tag", kDiveTag}}); } catch (...) {}
    // Drop the animation (dive) camera; keep the combat camera only when we landed.
    if (is_object(cam_anim_class_) && is_object(meteor_scm_)) {
        bool ok = false; try { ok = host_.request({{"op", "valid"}, {"target", meteor_scm_}}) == true; } catch (...) {}
        if (ok) try { host_.call(meteor_scm_, "RemoveAllCameraStatesByClass", {{"CameraState", cam_anim_class_}}); } catch (...) {}
    }

    const Json ground = {{"X", meteor_x_}, {"Y", meteor_y_}, {"Z", meteor_ground_z_}};
    if (is_object(meteor_pawn_)) {
        bool ok = false; try { ok = host_.request({{"op", "valid"}, {"target", meteor_pawn_}}) == true; } catch (...) {}
        if (ok) {
            if (landed) try { host_.call(meteor_pawn_, "K2_SetActorLocation", {{"NewLocation", ground}, {"bSweep", false}, {"bTeleport", true}}); } catch (...) {}
            try { host_.call(meteor_pawn_, "SetActorHiddenInGame", {{"bNewHidden", false}}); } catch (...) {}   // always end visible
            try { auto mv = host_.get(meteor_pawn_, "CharacterMovement");
                  if (is_object(mv)) {
                      host_.call(mv, "SetMovementMode", {{"NewMovementMode", MOVE_Walking}, {"NewCustomMode", 0}});
                      host_.call(mv, "StopMovementImmediately");   // no leftover velocity carried out of the flying descent
                  } } catch (...) {}
        }
    }

    if (landed && was_active) {
        // Landing beats: combat camera, 4-point pose, impact VFX, rumble, sound, shake.
        if (is_object(cam_combat_class_) && is_object(meteor_scm_))
            try { host_.call(meteor_scm_, "AddCameraStateByClass", {{"CameraState", cam_combat_class_}}); } catch (...) {}
        if (is_object(meteor_pawn_) && is_object(a_land_montage_))
            try { host_.call(meteor_pawn_, "PlayAnimMontage", {{"AnimMontage", a_land_montage_}, {"InPlayRate", 1.0}, {"StartSectionName", "None"}}); } catch (...) {}
        if (is_object(a_ff_land_))
            try { host_.call(controller_, "K2_ClientPlayForceFeedback",
                {{"ForceFeedbackEffect", a_ff_land_}, {"Tag", kLandTag}, {"bLooping", false}, {"bIgnoreTimeDilation", false}, {"bPlayWhilePaused", false}}); } catch (...) {}
        if (is_object(a_impact_))
            try { host_.call(niagara_lib_, "SpawnSystemAtLocation",
                {{"SystemTemplate", a_impact_}, {"Location", ground}, {"Rotation", {{"Pitch", 0.0}, {"Yaw", 0.0}, {"Roll", 0.0}}},
                 {"Scale", {{"X", 1.5}, {"Y", 1.5}, {"Z", 1.5}}}, {"WorldContextObject", world_},
                 {"bAutoDestroy", true}, {"PoolingMethod", 0}, {"bAutoActivate", true}, {"bPreCullCheck", false}}); } catch (...) {}
        if (is_object(a_shake_))
            try { auto cam = host_.get(controller_, "PlayerCameraManager");
                  if (is_object(cam)) host_.call(cam, "StartCameraShake",
                    {{"ShakeClass", a_shake_}, {"Scale", 1.0}, {"PlaySpace", 0}, {"UserPlaySpaceRot", {{"Pitch", 0.0}, {"Yaw", 0.0}, {"Roll", 0.0}}}}); } catch (...) {}
        if (is_object(a_snd_land_) && is_object(ww_statics_))
            try { host_.call(ww_statics_, "SpawnWaveWeaverComponentAtLocation",
                {{"in_Event", a_snd_land_}, {"in_Location", ground}, {"in_Orientation", {{"Pitch", 0.0}, {"Yaw", 0.0}, {"Roll", 0.0}}},
                 {"in_Subclass", nullptr}, {"in_WCO", world_}, {"in_bAutoDestroy", true}, {"in_bAutoPost", true}}); } catch (...) {}
        // Hold the combat camera for a beat, then release it in Settling.
        meteor_phase_ = MeteorPhase::Settling;
        meteor_settle_ = kMeteorSettle;
        return;
    }

    // Interrupt / world change: also drop the combat camera if we somehow had it.
    if (is_object(cam_combat_class_) && is_object(meteor_scm_)) {
        bool ok = false; try { ok = host_.request({{"op", "valid"}, {"target", meteor_scm_}}) == true; } catch (...) {}
        if (ok) try { host_.call(meteor_scm_, "RemoveAllCameraStatesByClass", {{"CameraState", cam_combat_class_}}); } catch (...) {}
    }
    meteor_phase_ = MeteorPhase::Idle;
    meteor_pawn_ = nullptr; meteor_mesh_ = nullptr; meteor_scm_ = nullptr;
}

}

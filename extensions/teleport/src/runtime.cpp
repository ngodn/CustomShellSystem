#include "teleport.hpp"
#include <string>

// The world-map watch. render() runs once per rendered frame. Outside a menu it does
// nothing except drive an in-flight meteor. While the world map is open it finds the
// hovered map icon (the one flagged Selected under the crosshair), shows the native
// Traverse prompt in the map's prompt bar, and on the key opens the game's
// confirmation dialog (or, with confirmation off, traverses straight away). The scan
// is throttled to ~16 Hz; the only per-frame work is the meteor descent while it is
// running.
//
// Path, verified live:
//   controller -> "User Interface Handler Component" -> ActiveMenu (WBP_Menu_Game)
//   -> WBP_Menu_Main -> WBP_MGT_WorldMap (bOpen == map tab active)
//   -> WBP_Icons[] -> the icon with Selected == true -> OwnerActor (teleport handler).
// The destination transform comes from that handler (see destination_transform).

namespace teleport {

namespace {
bool is_object(const Json& j) { return j.is_object() && j.contains("$object"); }
}

void Extension::invalidate_handles() {
    handles_ready_ = false;
    icons_ready_ = false;
    controller_ = world_ = ui_handler_ = map_screen_ = nullptr;
    icons_ = nullptr;
    map_prompts_ = prompt_box_ = nullptr;
}

bool Extension::refresh_handles() {
    icons_ready_ = false;
    icons_ = nullptr;
    map_prompts_ = prompt_box_ = nullptr;
    try {
        auto p = host_.player();
        controller_ = p.value("controller", Json());
        world_ = p.value("world", Json());
        if (!is_object(controller_)) return false;
        ui_handler_ = host_.get(controller_, "User Interface Handler Component");
        if (!is_object(ui_handler_)) return false;
        auto active = host_.get(ui_handler_, "ActiveMenu");   // null unless a menu is open
        if (!is_object(active)) return false;
        auto main = host_.get(active, "WBP_Menu_Main");
        if (!is_object(main)) return false;
        map_screen_ = host_.get(main, "WBP_MGT_WorldMap");
        if (!is_object(map_screen_)) return false;
        handles_ready_ = true;
        return true;
    } catch (...) {
        handles_ready_ = false;
        return false;
    }
}

bool Extension::map_open() {
    try { auto o = host_.get(map_screen_, "bOpen"); return o.is_boolean() && o.get<bool>(); }
    catch (...) { handles_ready_ = false; return false; }
}

Json Extension::hovered_icon() {
    if (!icons_ready_) {
        try { icons_ = host_.get(map_screen_, "WBP_Icons"); }
        catch (...) { handles_ready_ = false; return nullptr; }
        if (!icons_.is_array()) { icons_ = nullptr; return nullptr; }
        icons_ready_ = true;
    }
    for (const auto& ic : icons_) {
        if (!is_object(ic)) continue;
        bool sel = false;
        try { auto s = host_.get(ic, "Selected"); sel = s.is_boolean() && s.get<bool>(); }
        catch (...) { handles_ready_ = false; icons_ready_ = false; return nullptr; }
        if (sel) return ic;
    }
    return nullptr;
}

bool Extension::edge(const std::vector<std::string>& keys, bool& latch) {
    bool down = false;
    try {
        Json k = Json::array();
        for (const auto& s : keys) k.push_back(s);
        auto r = host_.request({{"op", "input.keys"}, {"target", controller_}, {"keys", k}});
        for (const auto& s : keys) if (r.value(s, false)) down = true;
    } catch (...) { down = false; }
    const bool rising = down && !latch;
    latch = down;
    return rising;
}

std::optional<Json> Extension::destination_transform(const Json& owner) {
    // Turn a game FTransform (quaternion rotation) into the location + rotators
    // the streaming teleport wants.
    auto from_tf = [&](const Json& tf) -> std::optional<Json> {
        if (!tf.is_object() || !tf.contains("Translation")) return std::nullopt;
        const auto loc = tf["Translation"];
        Json rot;
        try {
            auto math = host_.request({{"op", "find"}, {"path", "/Script/Engine.Default__KismetMathLibrary"}});
            if (is_object(math)) {
                auto br = host_.request({{"op", "call"}, {"target", math}, {"function", "BreakTransform"}, {"args", {{"InTransform", tf}}}});
                if (br.is_object()) rot = br.value("Rotation", Json());
            }
        } catch (...) {}
        if (loc.is_object() && rot.is_object()) return Json{{"loc", loc}, {"rot", rot}, {"control", rot}, {"zone", nullptr}};
        return std::nullopt;
    };
    // Beacon (BP_LandingAreaBase): GetStartTransform(false) is where the player
    // stands after arriving. Verified to match the game's own landing spot.
    try {
        auto r = host_.request({{"op", "call"}, {"target", owner}, {"function", "GetStartTransform"}, {"args", {{"InvertRotation", false}}}});
        if (r.is_object()) { auto d = from_tf(r.value("ReturnValue", Json())); if (d) return d; }
    } catch (...) {}
    // STH handler (dungeon / gate / well): GetOptionalTeleportDestination is this
    // point's own transform (not the linked exit the seamless getters return).
    try {
        auto r = host_.request({{"op", "call"}, {"target", owner}, {"function", "GetOptionalTeleportDestination"}});
        if (r.is_object() && r.value("Success", false)) { auto d = from_tf(r.value("ReturnValue", Json())); if (d) return d; }
    } catch (...) {}
    // Fallbacks: seamless getters, then the actor transform.
    try {
        auto loc = host_.call(owner, "GetSeamlessTeleportLocation");
        auto rot = host_.call(owner, "GetSeamlessTeleportRotation");
        auto ctl = host_.call(owner, "GetSeamlessTeleportControlRotation");
        if (loc.is_object() && rot.is_object()) {
            if (!ctl.is_object()) ctl = rot;
            return Json{{"loc", loc}, {"rot", rot}, {"control", ctl}, {"zone", nullptr}};
        }
    } catch (...) {}
    try {
        auto loc = host_.call(owner, "K2_GetActorLocation");
        auto rot = host_.call(owner, "K2_GetActorRotation");
        if (loc.is_object() && rot.is_object()) return Json{{"loc", loc}, {"rot", rot}, {"control", rot}, {"zone", nullptr}};
    } catch (...) {}
    return std::nullopt;
}

bool Extension::teleport_to(const Json& owner) {
    auto dest = destination_transform(owner);
    if (!dest) { report("Could not read that point's traverse location."); return false; }
    if (!is_object(world_)) { report("No world to traverse in."); return false; }

    hide_dialog();
    try { host_.call(ui_handler_, "HandleGameMenu", {{"SubTabIndex", 0}, {"AllowClose", true}}); } catch (...) {}

    // With the meteor on, the arrival is a full skydive under the game's own camera
    // states; with it off, it is a plain streaming teleport that keeps the game view.
    const bool meteor_run = meteor_ && load_meteor_assets();

    Json lib;
    try { lib = host_.request({{"op", "class_default"}, {"class", "BPFL_WorldStreaming_C"}}); }
    catch (...) { report("Traverse library is unavailable."); return false; }
    if (!is_object(lib)) { report("Traverse library is unavailable."); return false; }

    const Json args = {
        {"Location", (*dest)["loc"]},
        {"Rotation", (*dest)["rot"]},
        {"ControlRotation", (*dest)["control"]},
        {"ScreenTransitionClass", nullptr},
        {"TransitionZOrder", 0},
        {"FadeInDuration", 0.4},
        {"FadeOutDuration", 0.5},
        {"OptionalZoneData", nullptr},
        {"__WorldContext", world_},
    };
    try { host_.call(lib, "TeleportPlayerWithStreaming", args); }
    catch (const std::exception& e) { report(std::string("Traverse failed: ") + e.what()); return false; }

    // Arrive as a meteor: hide the character now (the teleport fade covers it) and
    // let render() drive the fall. begin_meteor is a no-op if it cannot grab the pawn.
    if (meteor_run) begin_meteor((*dest)["loc"], (*dest)["rot"]);

    my_prompt_ = nullptr;
    map_prompts_ = prompt_box_ = nullptr;
    prompt_text_shown_.clear();
    confirming_ = false;
    hovered_owner_ = nullptr;
    icons_ready_ = false;
    press_latch_ = cancel_latch_ = true;
    report("Traversing to " + hovered_name_ + ".");
    return true;
}

void Extension::scan(const CssxFrame* frame) {
    (void)frame;
    if (!handles_ready_ && !refresh_handles()) { hide_prompt(); hide_dialog(); confirming_ = false; return; }
    if (!map_open()) {
        icons_ready_ = false;
        icons_ = nullptr;
        map_prompts_ = prompt_box_ = nullptr;
        hide_prompt();
        hide_dialog();
        confirming_ = false;
        hovered_owner_ = nullptr;
        press_latch_ = cancel_latch_ = false;
        return;
    }

    // Confirmation dialog is up. Its own listener cannot fire (it is not the
    // focused input layer), so we drive it: left/right cycles Traverse (0) and
    // Cancel (1) with a live highlight, the game's confirm button acts on the
    // selection, the game's back button cancels. The map is frozen underneath.
    if (confirming_) {
        hide_prompt();
        bool alive = false;
        try { alive = is_object(my_dialog_) && host_.request({{"op", "valid"}, {"target", my_dialog_}}) == true; } catch (...) {}
        if (!alive) { confirming_ = false; hide_dialog(); return; }
        // Auto-cancel so a dialog can never wedge the feature (e.g. controller input
        // not reaching us). ~10 s at the 16 Hz scan rate.
        if (++confirm_scans_ > 160) { confirming_ = false; hide_dialog(); report("Traverse cancelled."); return; }
        bool left = false, right = false;
        try {
            auto r = host_.request({{"op", "input.keys"}, {"target", controller_},
                {"keys", {"Left", "A", "Gamepad_DPad_Left", "Gamepad_LeftStick_Left",
                          "Right", "D", "Gamepad_DPad_Right", "Gamepad_LeftStick_Right"}}});
            left = r.value("Left", false) || r.value("A", false)
                || r.value("Gamepad_DPad_Left", false) || r.value("Gamepad_LeftStick_Left", false);
            right = r.value("Right", false) || r.value("D", false)
                || r.value("Gamepad_DPad_Right", false) || r.value("Gamepad_LeftStick_Right", false);
        } catch (...) {}
        const bool nav = left || right;
        if (nav && !nav_latch_) {
            const int want = left ? 0 : 1;
            if (want != last_option_index_) { last_option_index_ = want; highlight_option(want); }
        }
        nav_latch_ = nav;
        if (edge({"Enter", "E", "SpaceBar", "Gamepad_FaceButton_Bottom"}, press_latch_)) {
            confirming_ = false; hide_dialog();
            if (last_option_index_ == 0) teleport_to(confirm_owner_); else report("Traverse cancelled.");
            return;
        }
        if (edge({"Escape", "BackSpace", "Gamepad_FaceButton_Right"}, cancel_latch_)) { confirming_ = false; hide_dialog(); report("Traverse cancelled."); }
        return;
    }

    Json icon = hovered_icon();
    Json owner;
    if (is_object(icon)) { try { owner = host_.get(icon, "OwnerActor"); } catch (...) {} }

    bool eligible = true;
    if (is_object(owner)) {
        for (const char* fn : {"IsUnlocked", "CanFastTravel", "EligibleForFastTravel"}) {
            try { auto v = host_.call(owner, fn); if (v.is_boolean()) { eligible = v.get<bool>(); break; } } catch (...) {}
        }
    }

    const bool showable = is_object(owner) && (eligible || allow_locked_);
    if (!showable) {
        hide_prompt();
        hovered_owner_ = nullptr;
        press_latch_ = false;
        return;
    }

    std::string name = "this point";
    if (is_object(icon)) {
        try { auto tt = host_.get(icon, "MapTooltip"); if (is_object(tt)) { auto n = host_.get(tt, "Name"); if (n.is_string() && !n.get<std::string>().empty()) name = n.get<std::string>(); } }
        catch (...) {}
    }
    hovered_owner_ = owner;
    hovered_name_ = name;

    show_prompt("Traverse");
    if (edge({"T", "Gamepad_LeftThumbstick"}, press_latch_)) {
        if (confirmation_ == "skip") { teleport_to(owner); return; }
        confirm_owner_ = owner;
        confirming_ = true;
        last_option_index_ = 0;
        confirm_scans_ = 0;
        hide_prompt();
        cancel_latch_ = true;
        show_dialog(name);
    }
}

int Extension::render(const CssxFrame* frame) {
    if (stopped_ || !frame) return 1;
    if (frame->world_generation != world_gen_) {
        world_gen_ = frame->world_generation;
        // A world swap invalidates every cached handle, including the in-flight
        // meteor's; drop it without touching stale objects, then forget the assets.
        cleanup_meteor(false);
        meteor_assets_ready_ = false;
        niagara_lib_ = ww_statics_ = cam_anim_class_ = cam_combat_class_ = nullptr;
        a_comet_ = a_impact_ = a_dive_montage_ = a_land_montage_ = a_ff_dive_ = a_ff_land_ = a_shake_ = a_snd_land_ = nullptr;
        // Tear the prompt and dialog down through their own paths so the map input
        // listeners frozen for the confirmation are re-enabled and the widgets are
        // unparented; both tolerate stale handles. Then drop the cached references.
        hide_prompt();
        hide_dialog();
        my_prompt_ = nullptr;
        dialog_class_ = nullptr;
        prompt_text_shown_.clear();
        prompt_refs_ready_ = false;
        widget_lib_ = prompt_class_ = prompt_template_ = nullptr;
        confirming_ = false;
        invalidate_handles();
    }
    // The meteor plays out after the map closes, so drive it every frame (for a
    // smooth fall) before the in-menu gate and let it own the frame while active.
    if (meteor_phase_ != MeteorPhase::Idle) { tick_meteor(frame->seconds); return 1; }
    if (!enable_ || !frame->in_menu) {
        if (is_object(my_prompt_)) { hide_prompt(); my_prompt_ = nullptr; }
        if (is_object(my_dialog_)) { hide_dialog(); }
        confirming_ = false;
        hovered_owner_ = nullptr;
        press_latch_ = cancel_latch_ = false;
        handles_ready_ = false;
        scan_acc_ = 1e9;
        return 1;
    }
    scan_acc_ += frame->seconds;
    if (scan_acc_ < 0.06) return 1;   // ~16 Hz while a menu is open
    scan_acc_ = 0;
    try { scan(frame); }
    catch (...) { handles_ready_ = false; try { hide_prompt(); hide_dialog(); } catch (...) {} confirming_ = false; }
    return 1;
}

}

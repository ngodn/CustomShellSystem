#include "teleport.hpp"
#include <string>

// The world-map watch. render() runs once per rendered frame while a world is
// ready. Outside a menu it does nothing (one branch), so there is no gameplay
// cost. Inside a menu it scans at ~20 Hz over cached handles to find the hovered
// map point, shows the prompt, reads the Teleport key, and fires the game's own
// streaming teleport. No reflected work happens on the gameplay path.

namespace teleport {

namespace {
bool is_object(const Json& j) { return j.is_object() && j.contains("$object"); }
}

void Extension::invalidate_handles() {
    handles_ready_ = false;
    controller_ = world_ = ui_handler_ = map_handler_ = nullptr;
}

bool Extension::refresh_handles() {
    try {
        auto p = host_.player();
        controller_ = p.value("controller", Json());
        world_ = p.value("world", Json());
        if (!is_object(controller_)) return false;
        ui_handler_ = host_.get(controller_, "User Interface Handler Component");
        map_handler_ = host_.get(controller_, "World Map Handler");
        if (!is_object(map_handler_)) return false;
        handles_ready_ = true;
        return true;
    } catch (...) {
        handles_ready_ = false;
        return false;
    }
}

Json Extension::focused_selector() {
    Json widgets;
    try { widgets = host_.get(map_handler_, "MapActorWidgets"); }
    catch (...) { handles_ready_ = false; return nullptr; }
    if (!widgets.is_array() || widgets.empty()) return nullptr;
    if (!diag_logged_) {
        diag_logged_ = true;
        try { host_.log("world map open: " + std::to_string(widgets.size()) + " map actor widgets", "debug"); } catch (...) {}
    }
    for (const auto& w : widgets) {
        if (!is_object(w)) continue;
        if (w.value("class", std::string{}).find("LandingAreaSelector") == std::string::npos) continue;
        bool shown = false;
        try { auto s = host_.get(w, "IsShown"); shown = s.is_boolean() && s.get<bool>(); } catch (...) {}
        if (!shown) continue;
        Json widget;
        try { widget = host_.get(w, "MapActorWidget"); } catch (...) { continue; }
        if (!is_object(widget)) continue;
        bool focus = false;
        try { auto fo = host_.get(widget, "HasFocus"); focus = fo.is_boolean() && fo.get<bool>(); } catch (...) {}
        if (focus) return widget;
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

std::optional<Json> Extension::destination_transform(const Json& area) {
    // Preferred: the landing area's beacon teleport object gives the authored
    // landing spot (already placed in front of the point), as an FRotator pair.
    try {
        auto beacon = host_.get(area, "TeleportHandlerObject");
        if (is_object(beacon)) {
            auto loc = host_.call(beacon, "GetTeleportLocation");
            auto rot = host_.call(beacon, "GetTeleportRotation");
            auto ctl = host_.call(beacon, "GetTeleportControlRotation");
            if (loc.is_object() && rot.is_object()) {
                if (!ctl.is_object()) ctl = rot;
                return Json{{"loc", loc}, {"rot", rot}, {"control", ctl}};
            }
        }
    } catch (...) {}
    // Fallback: the landing area actor's own transform.
    try {
        auto loc = host_.call(area, "K2_GetActorLocation");
        auto rot = host_.call(area, "K2_GetActorRotation");
        if (loc.is_object() && rot.is_object()) return Json{{"loc", loc}, {"rot", rot}, {"control", rot}};
    } catch (...) {}
    return std::nullopt;
}

void Extension::push_front_camera() {
    Json cam;
    try { cam = host_.get(controller_, "PlayerCameraManager"); } catch (...) { return; }
    if (!is_object(cam)) return;
    Json pawn;
    try { pawn = host_.player().value("pawn", Json()); } catch (...) { return; }
    if (!is_object(pawn)) return;
    Json fwd;
    try { fwd = host_.call(pawn, "GetActorForwardVector"); } catch (...) { return; }
    if (!fwd.is_object()) return;
    const double x = fwd.value("X", 0.0), y = fwd.value("Y", 0.0), z = fwd.value("Z", 0.0);
    if (x == 0.0 && y == 0.0 && z == 0.0) return;
    // View direction that looks at the character from the front: opposite the pawn's
    // facing. bDisableOnLookInput stays false so the player is never locked out.
    const Json dir = {{"X", -x}, {"Y", -y}, {"Z", -z}};
    try {
        host_.call(cam, "SetDesiredViewFromDirection",
                   {{"BlendTime", 0.5}, {"bDisableOnLookInput", false}, {"Direction", dir}});
    } catch (...) {}
}

void Extension::clear_camera() {
    // Nothing to undo: the departure view is a one-shot desired-view blend that the
    // gameplay camera resumes on its own. Kept for lifecycle symmetry.
    camera_clear_in_ = 0;
    camera_state_ = nullptr;
}

bool Extension::teleport_to(const Json& area) {
    auto dest = destination_transform(area);
    if (!dest) { report("Could not read that point's teleport location."); return false; }
    if (!is_object(world_)) { report("No world to teleport in."); return false; }

    // Close the world map so the jump plays in the world, not behind the menu.
    try { host_.call(ui_handler_, "HandleGameMenu", {{"SubTabIndex", 0}, {"AllowClose", true}}); } catch (...) {}

    if (camera_ == "front") { try { push_front_camera(); } catch (...) {} }

    Json lib;
    try { lib = host_.request({{"op", "class_default"}, {"class", "BPFL_WorldStreaming_C"}}); }
    catch (...) { report("Teleport library is unavailable."); return false; }
    if (!is_object(lib)) { report("Teleport library is unavailable."); return false; }

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
    catch (const std::exception& e) { report(std::string("Teleport failed: ") + e.what()); return false; }

    hide_prompt();
    hide_confirm();
    confirming_ = false;
    hovered_area_ = nullptr;
    press_latch_ = cancel_latch_ = true;   // swallow the release of the confirming press
    camera_clear_in_ = 4.0;
    report("Teleporting to " + hovered_name_ + ".");
    return true;
}

void Extension::scan(const CssxFrame* frame) {
    if (!handles_ready_ && !refresh_handles()) {
        if (prompt_visible_) hide_prompt();
        if (confirm_visible_) hide_confirm();
        return;
    }
    // While confirming, the target is locked to what was hovered when the prompt
    // was pressed, so moving the map cursor behind the dialog cannot retarget it.
    if (confirming_) {
        if (prompt_visible_) hide_prompt();
        show_confirm(frame, confirm_name_);
        if (edge({"T", "Gamepad_RightThumbstick"}, press_latch_)) { confirming_ = false; hide_confirm(); teleport_to(confirm_area_); return; }
        if (edge({"Escape", "Gamepad_FaceButton_Right"}, cancel_latch_)) { confirming_ = false; hide_confirm(); report("Teleport cancelled."); }
        return;
    }
    Json sel = focused_selector();
    if (!is_object(sel)) {
        if (confirming_) { confirming_ = false; hide_confirm(); }
        if (prompt_visible_) hide_prompt();
        hovered_area_ = nullptr;
        press_latch_ = cancel_latch_ = false;
        return;
    }

    // Resolve the hovered landing area (or a gate-only selector).
    Json area;
    try { auto r = host_.call(sel, "GetSelectedLandingArea"); if (r.is_object()) area = r.value("Output", Json()); } catch (...) {}
    if (!is_object(area)) {
        try { auto g = host_.get(sel, "LinkedGate"); if (is_object(g)) area = g; } catch (...) {}
    }
    if (!is_object(area)) {
        if (prompt_visible_) hide_prompt();
        hovered_area_ = nullptr;
        return;
    }

    // Eligibility: honour "allow locked".
    bool eligible = true;
    for (const char* fn : {"CanFastTravel", "EligibleForFastTravel", "IsUnlocked"}) {
        try { auto v = host_.call(area, fn); if (v.is_boolean()) { eligible = v.get<bool>(); break; } } catch (...) {}
    }
    hovered_eligible_ = eligible;
    if (!eligible && !allow_locked_) {
        if (prompt_visible_) hide_prompt();
        if (confirming_) { confirming_ = false; hide_confirm(); }
        hovered_area_ = nullptr;
        return;
    }

    // Name.
    std::string name = "this point";
    try { auto n = host_.get(area, "Area Name"); if (n.is_string() && !n.get<std::string>().empty()) name = n.get<std::string>(); } catch (...) {}
    if (name == "this point") {
        try { auto n = host_.call(area, "GetLocationName"); if (n.is_string() && !n.get<std::string>().empty()) name = n.get<std::string>(); } catch (...) {}
    }
    hovered_area_ = area;
    hovered_name_ = name;

    show_prompt(frame, eligible ? name : name + "  (locked)");
    if (edge({"T", "Gamepad_RightThumbstick"}, press_latch_)) {
        if (confirmation_ == "skip") { teleport_to(area); return; }
        confirm_area_ = area;
        confirm_name_ = name;
        confirming_ = true;
        hide_prompt();
        cancel_latch_ = true;   // ignore a cancel key that happens to be down this instant
        show_confirm(frame, name);
    }
}

int Extension::render(const CssxFrame* frame) {
    if (stopped_ || !frame) return 1;
    if (frame->world_generation != world_gen_) {
        world_gen_ = frame->world_generation;
        invalidate_handles();
        // The host dropped our layers on the world change; forget their ids.
        prompt_bg_ = prompt_key_ = prompt_label_ = 0;
        confirm_bg_ = confirm_title_ = confirm_msg_ = confirm_yes_ = confirm_no_ = 0;
        prompt_visible_ = confirm_visible_ = false;
        panel_tex_ = 0;
        tex_tried_ = false;
        diag_logged_ = false;
        confirming_ = false;
    }
    if (!enable_ || !frame->in_menu) {
        if (prompt_visible_) hide_prompt();
        if (confirm_visible_) hide_confirm();
        confirming_ = false;
        hovered_area_ = nullptr;
        press_latch_ = cancel_latch_ = false;
        scan_acc_ = 1e9;   // scan immediately when a menu next opens
        return 1;
    }
    scan_acc_ += frame->seconds;
    if (scan_acc_ < 0.05) return 1;   // ~20 Hz while a menu is open
    scan_acc_ = 0;
    try { scan(frame); }
    catch (...) {
        handles_ready_ = false;
        if (prompt_visible_) hide_prompt();
        if (confirm_visible_) hide_confirm();
    }
    return 1;
}

}

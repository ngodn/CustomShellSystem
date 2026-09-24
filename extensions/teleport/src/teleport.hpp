#pragma once
#include <cssx/client.hpp>
#include <optional>
#include <string>
#include <vector>

namespace teleport {
using cssx::Json;

// CSSX Teleport. A pure bridge + HUD extension: it reaches the game only through
// the host request bridge and draws only through the HUD surface, so it holds no
// raw engine pointer across a core swap and does no reflected work on the gameplay
// frame path. The world-map watch runs only while a game menu is open, throttled.
class Extension {
public:
    explicit Extension(const CssxHost* host);

    // ABI surface (main.cpp adapts these to the C table).
    Json model();
    void event(const Json& event);
    void tick(double seconds);
    int  render(const CssxFrame* frame);
    bool stop();
    Json status();

private:
    cssx::Client host_;
    const CssxHudApi* hud_ = nullptr;
    void* ctx_ = nullptr;

    // ---- settings (persisted to CSSX/state/eins0fx.teleport.json) ----
    bool enable_ = true;
    std::string confirmation_ = "show";   // "show" | "skip"
    bool allow_locked_ = true;
    std::string camera_ = "front";        // "front" | "default"
    void load_settings();
    void save_settings();

    std::string status_ = "Open the world map and hover a point to teleport.";
    std::string error_;
    void report(const std::string& text);

    // ---- world-map watch (runtime.cpp) ----
    uint32_t world_gen_ = 0;
    double scan_acc_ = 1e9;               // forces a scan on the first in-menu frame
    Json controller_, world_, ui_handler_, map_handler_;
    bool handles_ready_ = false;
    bool refresh_handles();
    void invalidate_handles();

    Json hovered_area_;                    // ABP_LandingAreaBase_C handle or null
    std::string hovered_name_;
    bool hovered_eligible_ = true;         // false = locked/not fast-travellable
    void scan(const CssxFrame* frame);
    Json focused_selector();

    // input edge state
    bool press_latch_ = false;            // teleport key held last read
    bool cancel_latch_ = false;
    bool edge(const std::vector<std::string>& keys, bool& latch);

    bool confirming_ = false;             // confirmation overlay is up
    Json confirm_area_;                    // target locked when the confirmation opened
    std::string confirm_name_;

    // one-shot diagnostics: log the map layout once per map session so a first
    // live run pinpoints anything that differs from the RE, then goes quiet.
    bool diag_logged_ = false;

    // ---- teleport execution ----
    bool teleport_to(const Json& area);
    std::optional<Json> destination_transform(const Json& area);  // {"loc","rot","control"}
    void push_front_camera();
    void clear_camera();
    Json camera_state_;                   // pushed UCSCameraState handle, if any
    double camera_clear_in_ = 0;          // seconds until the pushed camera is released

    // ---- overlay (overlay.cpp) ----
    uint64_t panel_tex_ = 0;
    bool tex_tried_ = false;
    CssxLayer prompt_bg_ = 0, prompt_key_ = 0, prompt_label_ = 0;
    CssxLayer confirm_bg_ = 0, confirm_title_ = 0, confirm_msg_ = 0, confirm_yes_ = 0, confirm_no_ = 0;
    bool prompt_visible_ = false, confirm_visible_ = false;
    std::string prompt_label_text_;
    void ensure_texture();
    void show_prompt(const CssxFrame* frame, const std::string& name);
    void hide_prompt();
    void show_confirm(const CssxFrame* frame, const std::string& name);
    void hide_confirm();
    void drop_overlay();
    // small helpers over the HUD table
    CssxLayer make_image(CssxLayer parent);
    CssxLayer make_text(CssxLayer parent);

    bool stopped_ = false;
};
}

#pragma once
#include <cssx/client.hpp>
#include <optional>
#include <string>
#include <vector>

namespace teleport {
using cssx::Json;

// CSSX Teleport. A pure bridge extension: it reaches the game only through the host
// request bridge, holds no raw engine pointer across a core swap, and does no
// reflected work on the gameplay frame path. On the world map it shows a native
// prompt (the game's own WBP_PromptWithText, added to the map's prompt bar) and, on
// the key, teleports through the game's own streaming teleport.
class Extension {
public:
    explicit Extension(const CssxHost* host);

    Json model();
    void event(const Json& event);
    void tick(double seconds);
    int  render(const CssxFrame* frame);
    bool stop();
    Json status();

private:
    cssx::Client host_;

    // ---- settings (persisted to CSSX/state/eins0fx.teleport.json) ----
    bool enable_ = true;
    std::string confirmation_ = "show";   // "show" | "skip"
    bool allow_locked_ = true;
    void load_settings();
    void save_settings();

    std::string status_ = "Open the world map and hover a point to teleport.";
    std::string error_;
    void report(const std::string& text);

    // ---- world-map watch (runtime.cpp) ----
    uint32_t world_gen_ = 0;
    double scan_acc_ = 1e9;               // forces a scan on the first in-menu frame
    Json controller_, world_, ui_handler_, map_screen_;   // map_screen_ = WBP_MGT_WorldMap
    Json icons_;                          // cached WBP_Icons array (stable per map open)
    bool handles_ready_ = false;
    bool icons_ready_ = false;
    bool refresh_handles();               // resolve the map screen from the controller
    void invalidate_handles();
    bool map_open();                      // WBP_MGT_WorldMap.bOpen

    Json hovered_owner_;                   // the hovered icon's OwnerActor (teleport handler)
    std::string hovered_name_;
    void scan(const CssxFrame* frame);
    Json hovered_icon();                  // the WBP_WMI_* icon with Selected==true, or null

    // input edge state
    bool press_latch_ = false;            // teleport key held last read
    bool cancel_latch_ = false;
    bool edge(const std::vector<std::string>& keys, bool& latch);

    // confirmation via the game's own dialog widget (WBP_ConfirmationPrompt_Default),
    // created, shown over the map and driven by our own key reads.
    bool confirming_ = false;
    Json confirm_owner_;
    int last_option_index_ = 0;           // dialog's selected option (0 = Traverse, 1 = Cancel)
    bool nav_latch_ = false;              // left/right navigation edge state
    int confirm_scans_ = 0;               // scans the dialog has been open (auto-cancel guard)
    void highlight_option(int index);     // drive the selected option's native highlight state
    Json dialog_class_;                   // WBP_ConfirmationPrompt_Default class
    Json my_dialog_;                      // active confirmation dialog, or null
    Json opt_primary_, opt_secondary_;    // the two WBP_ButtonPrompt options (Traverse / Cancel)
    Json frozen_listeners_;               // map input listeners disabled while the dialog is up
    void show_dialog(const std::string& name);
    void hide_dialog();
    // Put the input glyph beside an option label (native WBP_PromptWithText: glyph + text),
    // hiding the plain label so the button reads "[A] Traverse" / "[B] Cancel".
    void add_option_glyph(const Json& option, const std::string& label, int kbm, int controller);


    // ---- teleport execution ----
    bool teleport_to(const Json& owner);
    std::optional<Json> destination_transform(const Json& owner);  // {"loc","rot","control"}

    // ---- meteor arrival transition (meteor.cpp) ----
    // On arrival the character is hidden and shown as the game's own comet
    // (PS_Skydive_Comet) falling under the launcher camera state; on touchdown it
    // reappears with the 4-point landing montage plus the game's impact VFX, rumble,
    // audio and camera shake. Assets are all shell-agnostic (_Shared / shared VFX),
    // so it works the same on a stock shell or a CSS custom shell. Driven per frame
    // for a smooth fall; torn down on every exit path so no state can leak.
    bool meteor_ = true;
    enum class MeteorPhase { Idle, Warmup, Descending, Settling };
    MeteorPhase meteor_phase_ = MeteorPhase::Idle;
    Json meteor_pawn_, meteor_mesh_, meteor_scm_;   // pawn, its mesh, SpartaCameraManager
    Json meteor_comet_;                   // spawned comet NiagaraComponent (attached to mesh)
    double meteor_x_ = 0, meteor_y_ = 0, meteor_ground_z_ = 0, meteor_yaw_ = 0;
    double meteor_elapsed_ = 0, meteor_warmup_ = 0, meteor_settle_ = 0;
    static constexpr double kMeteorHeight = 2200.0;   // spawn this far above the point
    static constexpr double kMeteorFall = 1.6;        // seconds of visible descent
    static constexpr double kMeteorWarmup = 0.75;     // let the streaming teleport load + fade in
    static constexpr double kMeteorSettle = 2.2;      // hold the combat landing camera, then release
    // cached, world-scoped meteor assets
    bool meteor_assets_ready_ = false;
    Json niagara_lib_, ww_statics_, cam_anim_class_, cam_combat_class_;
    Json a_comet_, a_impact_, a_dive_montage_, a_land_montage_, a_ff_dive_, a_ff_land_, a_shake_, a_snd_land_;
    bool load_meteor_assets();
    void begin_meteor(const Json& ground_loc, const Json& rot);
    void tick_meteor(double seconds);
    void cleanup_meteor(bool landed);     // landed=false on interrupt (no impact beats)

    // ---- native prompt in the map's prompt bar (prompt.cpp) ----
    Json widget_lib_;                     // /Script/UMG.Default__WidgetBlueprintLibrary
    Json prompt_class_;                   // WBP_PromptWithText class
    Json prompt_template_;                // an existing FF_PromptGenData, for the sizes
    bool prompt_refs_ready_ = false;
    Json map_prompts_, prompt_box_;       // MapPrompts (WBP_PromptsContainer) + its HB_Prompts
    Json my_prompt_;                      // our created WBP_PromptWithText, or null
    std::string prompt_text_shown_;
    bool ensure_prompt_refs();            // resolve the widget lib, class and a data template
    bool resolve_bar();                   // resolve MapPrompts + HB_Prompts for this map
    void show_prompt(const std::string& text);   // ensure our prompt is present with this text
    void hide_prompt();                   // remove our prompt from the bar
    bool prompt_present();                // our prompt exists and is parented

    bool stopped_ = false;
};
}

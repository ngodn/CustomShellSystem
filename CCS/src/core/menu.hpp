#pragma once
// The CCS menu: a tab of the game's Player Menu built from Mortal Shell II's own widget
// blueprints (Change Shade list rows and category headers, the options menu's selector and
// slider rows, the Inventory details window, its prompts and the confirmation dialog), the way
// the CSS and CSSX tabs are. The game widgets are views only: every native result (a click, a
// confirm, an arrow) is a Blueprint delegate the menu cannot bind, so the menu reads the game's
// menu keys itself and polls each widget's transparent button for mouse clicks.
//
// The page shows a model: sections of typed controls (controls.hpp) that the core builds from
// the combat engine, presets and settings. Widgets are created once and pooled. A build runs
// immediate-mode page code, but each call takes the next pooled widget of its kind and changes
// only what differs, so moving the selection is a few restyles, never a teardown. Creating a
// game widget costs about a millisecond, so a build creates at most `budget_per_build` and
// continues next frame. Closed menu: no widgets touched, no per-frame work beyond one cached
// flag read.
#include "engine.hpp"
#include "controls.hpp"
#include "search.hpp"
#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>

namespace ccs {
namespace fs = std::filesystem;
class Menu {
public:
    struct Deps {
        std::function<void(const std::string&)> log;
        std::function<Json()> model;                       // the page: {"sections":[...],"status":"..."}
        std::function<uint64_t()> revision;                // moves whenever the model would differ; the menu fetches only then
        std::function<void(const Json&)> event;            // a validated control event; throws with the error to show
        std::function<double()> ui_scale;                  // 0.75 .. 1.5
        std::string title{"Custom Combat System"}, version;
        fs::path root;                                     // Mods/CCS (assets/logo.png)
        bool css_present = false, cssx_present = false;
    };
    explicit Menu(Deps deps) : deps_(std::move(deps)) {}
    ~Menu() { try { detach(); } catch (...) {} }
    bool is_open() const { return active_; }
    bool attached() const { return page_.Get() != nullptr; }
    void close();                      // leave the page: close the Player Menu like the game does
    void detach();                     // remove the tab and page (menu instance changed or core stop)
    // Once per engine tick from the core. Attaches to the Player Menu when it appears, polls
    // input and rebuilds while the CCS page is showing, and warms the page up (skeleton,
    // textures, pooled rows) while the Player Menu is open on another tab.
    void tick(const engine::PlayerContext& player, double delta);
    void invalidate() { model_revision_ = 0; dirty_ = true; }
    Json diagnostics() const;
    struct BuildCost { uint64_t builds = 0, build_us = 0, widgets = 0, last_build_us = 0, max_build_us = 0, created = 0; };
    const BuildCost& cost() const { return cost_; }
private:
    using UObject = RC::Unreal::UObject;
    using WeakObject = engine::WeakObject;
    using Color = engine::Color;
    Deps deps_;
    // ---- hosting
    bool active_ = false, was_active_ = false, dirty_ = true, enter_ = false;
    uint64_t attach_wait_since_ = 0, discover_after_ = 0;
    WeakObject pc_, handler_, main_, tabs_, switcher_, page_, tab_, tree_, canvas_;
    int tab_index_ = -1;
    std::array<double, 2> viewport_{};
    uint64_t layout_check_ = 0, last_tick_ = 0;
    // ---- navigation state
    Json model_;
    uint64_t model_revision_ = 0, model_check_ = 0;
    int section_ = 0, row_ = 0, cand_ = 0;   // on a slots section row_ is the active slot, cand_ the candidate
    std::string error_, text_key_, text_draft_;
    Json confirm_;                  // {"event":..., "message":...}
    bool picker_ = false;
    OptionSearch options_;
    std::string search_query_;
    uint64_t wheel_after_ = 0;
    // ---- input
    struct Binding { std::string action; std::vector<std::string> keys; bool down = false; uint64_t repeat = 0; WeakObject input_action; };
    std::vector<Binding> bindings_;
    bool bindings_ready_ = false; uint64_t bind_retry_ = 0;
    uint64_t bindings_generation_ = 0, strip_glyph_generation_ = ~0ull;
    bool gamepad_ = false, typing_now_ = false;
    std::string slot_options_key_;            // which slot's candidates options_ currently holds
    int panel_focus_ = -1;                    // which per-slot setting row in the window has keyboard focus, -1 = the candidate list
    const Json* highlighted_setting() const;  // the focused per-slot setting control, or null
    struct Hit { WeakObject widget; Json action; bool down = false; std::vector<std::pair<WeakObject, Json>> parts; WeakObject glyph; };
    std::vector<Hit> hits_;
    struct SliderHit { WeakObject bar, value_block, row; Json control; double previous, low, high, step; std::string unit; };
    std::vector<SliderHit> sliders_;
    int drag_slider_ = -1;
    bool mouse_was_down_ = false, left_was_down_ = false;
    WeakObject search_input_, name_input_, input_prompt_;
    BuildCost cost_;
    // ---- textures: imported once per file and rooted for the menu's lifetime
    struct Texture { WeakObject object; bool missing = false; };
    std::map<std::string, Texture> textures_;
    std::vector<WeakObject> rooted_;
    size_t warm_texture_ = 0;
    // ---- the native page pool (menu_page.cpp)
    enum class Kind : uint8_t { none, tab, header, row, option, slider, divider, action, input, paragraph, picture };
    struct Item {
        Kind kind = Kind::none;
        WeakObject widget, hit, hit_left, hit_right, text_block, value_block, extra, extra2;
        std::string text, value, glyph;
        int selected = -1, badge = -1, shown = -1, enabled = -1, icon_shown = -1, arrows = -1;
        const void* icon = nullptr; float fill = -2.f;
        std::array<float, 4> color{-1, -1, -1, -1};
    };
    struct Cell { WeakObject holder; std::vector<Item> kinds; int shown = -1; };
    struct Stack { WeakObject box; std::deque<Cell> cells; size_t used = 0; };
    Stack tab_items_, list_, head_, top_, panel_, actions_, footer_;
    // The slot grid: ten of the game's equipment slot tiles with a label over each, built once
    // in the centre column and restyled per build.
    struct Tile { WeakObject widget, label, hit; int selected = -1, shown = -1, dimmed = -1; const void* icon = nullptr; std::string text, icon_path; };
    std::array<Tile, 11> tiles_{};
    WeakObject grid_root_, banner_image_;
    int banner_shown_ = -1;
    WeakObject design_, left_root_, right_root_, list_scroll_, strip_scroll_, details_, panel_scroll_, panel_size_,
               status_text_, title_text_, subtitle_text_, logo_image_, strip_previous_, strip_next_, strip_previous_glyph_, strip_next_glyph_;
    double design_w_ = 0, design_h_ = 0, design_scale_ = 0;
    int budget_ = 0;
    static constexpr int budget_per_build = 4;
    int shown_section_ = -1, revealed_row_ = -1;
    size_t list_first_ = 0;                                  // first candidate row shown; moves only when the highlight leaves the window
    std::string panel_context_;
    const void* panel_revealed_ = nullptr;
    bool panel_fit_pending_ = false;
    float panel_max_ = 720.f;
    struct PendingReveal { WeakObject scroll, target; uint8_t destination = 0; int frames = 0; };
    std::array<PendingReveal, 2> pending_reveals_{};
    std::string detail_title_, detail_sub_, detail_body_, status_shown_, title_shown_, subtitle_shown_;
    int status_error_shown_ = -1, logo_shown_ = -1;
    uint64_t transition_started_ = 0;
    // ---- confirmation dialog: the game's WBP_ConfirmationPrompt_Default
    WeakObject dialog_, dialog_primary_, dialog_secondary_;
    std::array<WeakObject, 2> dialog_glyphs_{};
    uint64_t dialog_glyph_generation_ = 0;
    std::vector<WeakObject> frozen_listeners_;
    std::string dialog_shown_;
    int dialog_focus_ = 0, dialog_focus_shown_ = -1;
    // ---- implementation: hosting and input (menu.cpp)
    bool attach(const engine::PlayerContext& player);
    void order_tabs();
    void navigate(int index);
    void forget();
    void bind_inputs();
    bool typing() const;
    void poll_input(const engine::PlayerContext& player, uint64_t now, bool typing);
    void poll_mouse(const engine::PlayerContext& player);
    double along_bar(const SliderHit& slider, bool& inside) const;
    void key(const std::string& action);
    void act(const Json& action);
    void refresh_model(bool force, uint64_t now);
    void send_event(const Json& event);
    const Json* current_control() const;
    void warm(uint64_t now);
    // ---- implementation: the native page (menu_page.cpp)
    bool page(double width, double height);
    void forget_page();
    Item& take(Stack& stack, Kind kind);
    bool ready(const Stack& stack, Kind kind) const;
    void finish(Stack& stack);
    void setup(Item& item);
    void slot_padding(Kind kind, UObject* slot);
    void invalidate_page();
    void show(Item& item, bool on);
    void text(UObject* block, std::string& shown, const std::string& value);
    void state(Item& item, bool selected);
    void glyph(UObject* prompt, const std::string& action, uint8_t fallback, uint8_t keyboard = 255);
    UObject* texture_at(const fs::path& file);
    UObject* game_icon(const std::string& path);   // a game texture by object path, rooted for the menu's lifetime
    void root(UObject* object);
    void unroot_all();
    void build();
    void build_page(bool& deferred);
    void build_slots(const Json& section, bool& deferred);
    bool ensure_tiles();
    void build_picker(bool& deferred);
    void header(const std::string& title, const std::string& subtitle);
    void detail(const std::string& title, const std::string& subtitle, std::string body, UObject* icon = nullptr);
    const void* detail_icon_ = nullptr;
    void status_line(const std::string& value, bool error);
    void fit_panel();
    void reveal_pending();
    void animate(uint64_t now);
    void dialog();
    void dialog_close(bool destroy = false);
    struct RowLook { UObject* icon = nullptr; bool badge = false, enabled = true; std::string value; };
    std::map<UObject*, bool> held_;
    void fill_row(Item& item, const std::string& title, const RowLook& look, bool selected);
    void paragraph(Stack& stack, const std::string& value, Color color);
    void bind(const WeakObject& widget, Json action, const std::map<UObject*, bool>& held);
    void strip(const std::vector<std::string>& names, int selected, const std::string& action);
    void action_prompt(const std::string& binding, const std::string& label, Json action, uint8_t icon, bool enabled = true);
    void bar_prompt(const std::string& label, Json action, const std::string& first, uint8_t first_icon, uint8_t first_key,
                    const std::string& second = {}, uint8_t second_icon = 0, uint8_t second_key = 255);
    void bar_prompt_actions(const std::string& label, const std::string& first, uint8_t first_icon, const std::string& second = {}, uint8_t second_icon = 0);
    void option_row(const std::string& name, const std::string& value, bool focused, Json minus, Json plus, bool arrows, Json select = Json{});
    void slider_row(const std::string& name, const Json& control, bool focused, Json minus, Json plus);
};
}

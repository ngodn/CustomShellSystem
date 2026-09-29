#pragma once
// The CSSX menu: a tab of the game's Player Menu built from Mortal Shell II's own widget
// blueprints (the Change Shade list rows and category headers, the options menu's selector
// and slider rows, the Inventory details window, its prompts and the confirmation dialog),
// the way the CSS tab is. The game widgets are views only: every native result (a click, a
// confirm, an arrow) is a Blueprint delegate the menu cannot bind, so the menu reads the
// game's menu keys itself and polls each widget's transparent button for mouse clicks.
//
// Widgets are created once and pooled. A build runs immediate-mode page code, but each call
// takes the next pooled widget of its kind and changes only what differs, so moving the
// selection is a few restyles, never a teardown. Creating a game widget costs about a
// millisecond, so a build creates at most `budget_per_build` and continues next frame.
// Closed menu: no widgets touched, no per-frame work beyond one cached flag read.
#include "engine.hpp"
#include "extensions.hpp"
#include "settings.hpp"
#include "search.hpp"
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <optional>

namespace cssx {
class Menu {
public:
    struct Deps {
        Runtime* runtime=nullptr;                 // may be null (no extensions loaded)
        Settings* settings=nullptr;
        std::function<void(const std::string&)> log;
        std::function<Json()> notice;             // framework notice for the library (migration etc.)
        std::function<void()> save_settings;
        std::string version;
        fs::path root;                            // Mods/CSSX (assets/logo.png, assets/banner.png)
        std::function<Json()> perf;               // {hz, median_ms, core_mean_us, core_max_us, core_p99_us}
    };
    explicit Menu(Deps deps):deps_(std::move(deps)) {}
    ~Menu() { try { detach(); } catch(...) {} }
    // Hosted in the game's Player Menu: CSSX is the last tab, right after CSS when
    // it is present, otherwise straight after the native tabs (Inventory, Tarstones,
    // Map). is_open() means the CSSX page is the active tab of an open Player Menu.
    bool is_open() const { return active_; }
    bool player_menu_open() const;             // the game's Player Menu is open (any tab), one cached flag read
    bool attached() const { return page_.Get()!=nullptr; }
    // Ask the game to open its Player Menu and select the CSSX tab. Returns
    // false with a reason when the menu cannot be opened right now.
    bool open(const engine::PlayerContext& player,std::string* reason=nullptr);
    void close();                      // leave the CSSX page: close the Player Menu like the game does
    void detach();                     // remove the tab and page (menu instance changed or core stop)
    // Once per engine tick from the core. Attaches to the Player Menu when it appears,
    // polls input and rebuilds while the CSSX page is showing, and warms the page up
    // (skeleton, textures, pooled rows) while the Player Menu is open on another tab.
    void tick(const engine::PlayerContext& player,double delta);
    void invalidate() { dirty_=true; }
    void set_runtime(Runtime* runtime) { deps_.runtime=runtime; library_revision_=0; dirty_=true; }
    void set_css_present(bool present) { css_present_=present; }
    void drive_key(const std::string& action) { if(active_) key(action); }
    void drive(const Json& action) { if(active_) act(action); }
    Json diagnostics() const;
    struct BuildCost { uint64_t builds=0, build_us=0, widgets=0, last_build_us=0, max_build_us=0, created=0; };
    const BuildCost& cost() const { return cost_; }
private:
    using UObject=RC::Unreal::UObject;
    using WeakObject=engine::WeakObject;
    using Color=engine::Color;
    Deps deps_;
    // ---- hosting
    bool active_=false, was_active_=false, dirty_=true, enter_=false, css_present_=false, open_requested_=false;
    uint64_t attach_wait_since_=0, open_requested_at_=0, discover_after_=0;
    WeakObject pc_, handler_, main_, tabs_, switcher_, page_, tab_, tree_, canvas_;
    int tab_index_=-1;
    std::array<double,2> viewport_{};
    uint64_t layout_check_=0, last_tick_=0;
    // ---- navigation state
    enum class Screen { Library, Extension, Settings } screen_=Screen::Library;
    std::string extension_id_;
    Json library_, model_;
    uint64_t library_revision_=0, library_check_=0;
    // library_row_: 0 is the CSSX entry, i+1 the i-th extension.
    int library_row_=0, section_=0, row_=0, settings_row_=0;
    std::string error_, text_key_, text_draft_;
    Json confirm_;                  // {"event":..., "message":...}
    bool picker_=false;
    OptionSearch options_;
    std::string search_query_;
    uint64_t wheel_after_=0;
    // ---- input
    struct Binding { std::string action; std::vector<std::string> keys; bool down=false; uint64_t repeat=0; WeakObject input_action; };
    std::vector<Binding> bindings_;
    bool bindings_ready_=false; uint64_t bind_retry_=0, bind_started_=0; bool bindings_fallback_=false;
    void fallback_bindings(uint64_t now);   // the game's own default menu keys, when Enhanced Input never answers
    // Bumped whenever the bindings are re-read, so every cached glyph is redrawn with the real keys.
    uint64_t bindings_generation_=0, strip_glyph_generation_=~0ull;
    bool gamepad_=false;
    // `parts`: glyphs inside the hit that each do their own thing (W / S on one prompt).
    // `glyph`: the WBP_Prompt that flashes when the hit is clicked, as it does for its key.
    struct Hit { WeakObject widget; Json action; bool down=false; std::vector<std::pair<WeakObject,Json>> parts; WeakObject glyph; };
    std::vector<Hit> hits_;
    // A native slider row: the bar the value is read off when dragged, the readout, the
    // control it edits (for snapping and the event), and the last value shown.
    struct SliderHit { WeakObject bar, value_block, row; Json control; double previous, low, high, step; std::string unit; };
    std::vector<SliderHit> sliders_;
    int drag_slider_=-1;
    bool mouse_was_down_=false, left_was_down_=false;
    WeakObject search_input_, name_input_, input_prompt_;
    BuildCost cost_;
    // ---- textures: imported once per file and rooted, so the game's garbage collector
    // never drops one between menu opens (the blueprints' Construct resets every brush).
    struct Texture { WeakObject object; bool missing=false; };
    std::map<std::string,Texture> textures_;
    std::vector<WeakObject> rooted_;
    size_t warm_texture_=0;
    // ---- the native page pool (menu_page.cpp)
    enum class Kind : uint8_t { none, tab, header, row, option, slider, divider, action, input, paragraph, picture };
    struct Item {
        Kind kind=Kind::none;
        WeakObject widget, hit, hit_left, hit_right, text_block, value_block, extra, extra2;
        std::string text, value, glyph;             // what is on screen now
        int selected=-1, badge=-1, shown=-1, enabled=-1, icon_shown=-1, arrows=-1;
        const void* icon=nullptr; float fill=-2.f;
        std::array<float,4> color{-1,-1,-1,-1};
    };
    // A stack is a column of fixed cells. Each cell keeps one widget per kind it has ever
    // shown and switches which one is visible, so a layout change never creates, removes
    // or reparents a widget once each shape has been seen (both cost about a millisecond
    // per game widget; a visibility switch costs almost nothing).
    struct Cell { WeakObject holder; std::vector<Item> kinds; int shown=-1; };
    struct Stack { WeakObject box; std::deque<Cell> cells; size_t used=0; };   // deque: taken items keep their address
    Stack tab_items_, list_, head_, panel_, actions_, footer_;                // head_: fixed, above the window's scroll
    WeakObject design_, left_root_, right_root_, list_scroll_, strip_scroll_, details_, panel_scroll_, panel_size_,
               status_text_, title_text_, subtitle_text_, logo_image_, strip_previous_, strip_next_, strip_previous_glyph_, strip_next_glyph_;
    double design_w_=0, design_h_=0, design_scale_=0;
    int budget_=0;
    static constexpr int budget_per_build=4;
    int shown_section_=-1, revealed_row_=-1;
    std::string panel_context_;
    const void* panel_revealed_=nullptr;
    bool panel_fit_pending_=false;
    float panel_max_=720.f;
    struct PendingReveal { WeakObject scroll, target; uint8_t destination=0; int frames=0; };
    std::array<PendingReveal,2> pending_reveals_{};   // the list, the details window
    std::string detail_title_, detail_sub_, detail_body_, status_shown_, title_shown_, subtitle_shown_;
    int status_error_shown_=-1, logo_shown_=-1;
    uint64_t transition_started_=0;
    // ---- confirmation dialog: the game's WBP_ConfirmationPrompt_Default
    WeakObject dialog_, dialog_primary_, dialog_secondary_;
    std::array<WeakObject,2> dialog_glyphs_{};
    uint64_t dialog_glyph_generation_=0;
    std::vector<WeakObject> frozen_listeners_;
    std::string dialog_shown_;
    int dialog_focus_=0, dialog_focus_shown_=-1;
    // ---- implementation: hosting and input (menu.cpp)
    bool attach(const engine::PlayerContext& player);
    void order_tabs();
    void navigate(int index);
    void forget();
    void bind_inputs();
    bool typing() const;
    void poll_input(const engine::PlayerContext& player,uint64_t now,bool typing);
    void poll_mouse(const engine::PlayerContext& player);
    double along_bar(const SliderHit& slider,bool& inside) const;
    void key(const std::string& action);
    void act(const Json& action);
    void refresh_library(bool force,uint64_t now);
    const Json& library_entries() const;
    void refresh_model();
    void send_event(const Json& event);
    const Json* current_control() const;
    std::string perf_line(bool brief=false) const;
    std::string perf_detail() const;
    void warm(uint64_t now);
    // ---- implementation: the native page (menu_page.cpp)
    bool page(double width,double height);
    void forget_page();
    Item& take(Stack& stack,Kind kind);
    bool ready(const Stack& stack,Kind kind) const;
    void finish(Stack& stack);
    void setup(Item& item);
    void slot_padding(Kind kind,UObject* slot);
    void invalidate_page();
    void show(Item& item,bool on);
    void text(UObject* block,std::string& shown,const std::string& value);
    void state(Item& item,bool selected);
    void glyph(UObject* prompt,const std::string& action,uint8_t fallback,uint8_t keyboard=255);
    UObject* texture(const std::string& file);
    UObject* texture_at(const fs::path& file);
    void root(UObject* object);
    void unroot_all();
    void build();
    void build_library(bool& deferred);
    void build_extension(bool& deferred);
    void build_settings();
    void build_picker(bool& deferred);
    void header(const std::string& title,const std::string& subtitle);
    void detail(const std::string& title,const std::string& subtitle,std::string body);
    void status_line(const std::string& value,bool error);
    void fit_panel();
    void reveal_pending();
    void animate(uint64_t now);
    void dialog();
    void dialog_close(bool destroy=false);
    // Item fillers shared by the screens. `held_` carries mouse-button state across a build
    // so a button held down through it is not seen as a fresh click.
    struct RowLook { UObject* icon=nullptr; bool badge=false, enabled=true; std::string value; };
    std::map<UObject*,bool> held_;
    void fill_row(Item& item,const std::string& title,const RowLook& look,bool selected);
    void paragraph(Stack& stack,const std::string& value,Color color);
    void bind(const WeakObject& widget,Json action,const std::map<UObject*,bool>& held);
    void strip(const std::vector<std::string>& names,int selected,const std::string& action);
    void action_prompt(const std::string& binding,const std::string& label,Json action,uint8_t icon,bool enabled=true);
    void bar_prompt(const std::string& label,Json action,const std::string& first,uint8_t first_icon,uint8_t first_key,
                    const std::string& second={},uint8_t second_icon=0,uint8_t second_key=255);
    void bar_prompt_actions(const std::string& label,const std::string& first,uint8_t first_icon,const std::string& second={},uint8_t second_icon=0);
    void option_row(const std::string& name,const std::string& value,bool focused,Json minus,Json plus,bool arrows);
    void slider_row(const std::string& name,const Json& control,bool focused,Json minus,Json plus);
};
}

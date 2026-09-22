#pragma once
// The CSSX menu: one full-screen UMG page built by reflection, opened through
// the game's UI handler so pause, cursor, HUD and input mapping behave like a
// native menu. Retained widgets; rebuilt only when the model, selection or
// viewport changes. Closed menu: no widgets, no per-frame work.
#include "engine.hpp"
#include "extensions.hpp"
#include "settings.hpp"
#include "search.hpp"
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
    // Hosted in the game's Player Menu: CSSX is a tab after CSS (after
    // Inventory when CSS is absent) and before Tarstones. is_open() means the
    // CSSX page is the active tab of an open Player Menu.
    bool is_open() const { return active_; }
    bool attached() const { return page_.Get()!=nullptr; }
    // Ask the game to open its Player Menu and select the CSSX tab. Returns
    // false with a reason when the menu cannot be opened right now.
    bool open(const engine::PlayerContext& player,std::string* reason=nullptr);
    void close();                      // leave the CSSX page: close the Player Menu like the game does
    void detach();                     // remove the tab and page (menu instance changed or core stop)
    // Per tick: attach when the Player Menu is open, track the active tab,
    // poll input and rebuild while the CSSX page is showing.
    void tick(const engine::PlayerContext& player,double delta);
    void invalidate() { dirty_=true; }
    void set_runtime(Runtime* runtime) { deps_.runtime=runtime; library_revision_=0; dirty_=true; }
    void set_css_present(bool present) { css_present_=present; }
    void drive_key(const std::string& action) { if(active_) key(action); }
    void drive(const Json& action) { if(active_) act(action); }
    Json diagnostics() const;
    struct BuildCost { uint64_t builds=0, build_us=0, widgets=0, last_build_us=0; };
    const BuildCost& cost() const { return cost_; }
private:
    Deps deps_;
    bool active_=false, was_active_=false, dirty_=true, enter_=false, css_present_=false, open_requested_=false;
    uint64_t attach_wait_since_=0, open_requested_at_=0, discover_after_=0;
    engine::WeakObject pc_, handler_, main_, tabs_, switcher_, page_, tab_, tree_, canvas_, prompt_;
    int tab_index_=-1;
    std::array<double,2> viewport_{};
    uint64_t layout_check_=0;
    // Navigation state
    enum class Screen { Library, Extension, Settings } screen_=Screen::Library;
    std::string extension_id_;
    Json library_, model_;
    uint64_t library_revision_=0, library_check_=0;
    int library_row_=0, section_=0, row_=0, first_row_=0;
    std::string error_, text_key_, text_draft_;
    Json confirm_;                  // {"event":..., "message":...}
    bool details_=false, picker_=false;
    float details_offset_=0;
    OptionSearch options_;
    std::string search_query_;
    engine::WeakObject search_input_, search_results_, search_count_, description_, name_input_;
    // Settings screen draft
    int settings_row_=0;
    // Input
    struct Binding { std::string action; std::vector<std::string> keys; bool down=false; uint64_t repeat=0; engine::WeakObject input_action; };
    std::vector<Binding> bindings_;
    bool bindings_ready_=false; uint64_t bind_retry_=0;
    bool gamepad_=false;
    struct Hit { engine::WeakObject widget; Json action; bool down=false; };
    std::vector<Hit> hits_;
    struct SliderHit { engine::WeakObject widget, label; Json control; float previous; };
    std::vector<SliderHit> sliders_;
    bool mouse_left_=false;
    BuildCost cost_;
    std::map<std::string,engine::WeakObject> textures_;
    // Implementation
    void bind_inputs();
    void poll_input(const engine::PlayerContext& player,uint64_t now);
    void poll_mouse(const engine::PlayerContext& player);
    void key(const std::string& action);
    void act(const Json& action);
    void build();
    void build_library(engine::Layout& ui,double width);
    void build_extension(engine::Layout& ui,double width);
    void build_settings(engine::Layout& ui,double width);
    void build_footer(engine::Layout& ui,double width,const std::vector<std::pair<std::string,std::string>>& left,const std::vector<std::pair<std::string,std::string>>& right);
    void build_modal(engine::Layout& ui,double width);
    void frame(engine::Layout& ui,double width,double column_w,const std::string& title,const std::string& subtitle,const std::vector<std::string>& tabs,int selected,const std::string& tab_action);
    std::string perf_line(bool brief=false) const;
    std::string perf_detail() const;
    void build_results();
    engine::UObject* prompt(engine::Layout& ui,const std::string& action,const std::string& text,double x,double y,double w,uint8_t icon);
    void refresh_library(bool force);
    void refresh_model();
    void send_event(const Json& event);
    const Json* current_control() const;
    bool attach(const engine::PlayerContext& player);
    void order_tabs();
    void navigate(int index);
    void forget();
    bool texture(engine::Layout& ui,const std::string& file,double x,double y,double w,double h);
};
}

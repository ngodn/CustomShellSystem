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
    };
    explicit Menu(Deps deps):deps_(std::move(deps)) {}
    ~Menu() { try { close(); } catch(...) {} }
    bool is_open() const { return open_; }
    // Open the menu for this player context. Returns false with a reason when
    // the game's UI handler or viewport is unavailable.
    bool open(const engine::PlayerContext& player,std::string* reason=nullptr);
    void close();
    // Per tick while open: lifecycle checks, input, rebuild when dirty.
    void tick(const engine::PlayerContext& player,double delta);
    void invalidate() { dirty_=true; }
    Json diagnostics() const;
    struct BuildCost { uint64_t builds=0, build_us=0, widgets=0, last_build_us=0; };
    const BuildCost& cost() const { return cost_; }
private:
    Deps deps_;
    bool open_=false, dirty_=true, enter_=false;
    engine::WeakObject pc_, handler_, widget_, tree_, canvas_, prompt_;
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
    bool gamepad_=false;
    struct Hit { engine::WeakObject widget; Json action; bool down=false; };
    std::vector<Hit> hits_;
    struct SliderHit { engine::WeakObject widget, label; Json control; float previous; };
    std::vector<SliderHit> sliders_;
    std::array<double,2> mouse_{};
    bool mouse_left_=false;
    float wheel_accumulator_=0;
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
    void build_results();
    engine::UObject* prompt(engine::Layout& ui,const std::string& action,const std::string& text,double x,double y,double w,uint8_t icon);
    void refresh_library(bool force);
    void refresh_model();
    void send_event(const Json& event);
    const Json* current_control() const;
    void restore_input(bool handler_alive);
    bool texture(engine::Layout& ui,const std::string& file,double x,double y,double w,double h);
};
}

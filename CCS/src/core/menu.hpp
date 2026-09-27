#pragma once
#include "engine.hpp"
#include "settings.hpp"
#include "storage.hpp"
#include "persistence.hpp"
#include "moveset_manager.hpp"
#include "menu_input.hpp"
#include "widget_pool.hpp"
#include "loaded_moves.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace ccs {

using RC::Unreal::UObject;

enum class MenuScreen : uint8_t {
    Customize = 0,
    Preset = 1,
    Settings = 2
};

enum class FocusArea : uint8_t {
    SlotGrid,
    LeftAccordion,
    PresetsList,
    SettingsList
};

class Menu {
public:
    struct Deps {
        runtime::Settings* settings{nullptr};
        runtime::Persistence* persistence{nullptr};
        MovesetManager* movesets{nullptr};
        const LoadedMoves* loaded_moves{nullptr};
        std::function<void(const std::string&)> log;
        bool css_present{false};
        bool cssx_present{false};
    };

    explicit Menu(Deps deps);
    ~Menu();

    bool is_open() const { return active_; }
    bool attached() const { return page_.Get() != nullptr; }
    const std::string& file_status() const { return file_status_; }
    const std::string& input_error() const { return input_error_; }

    bool open(const engine::PlayerContext& player);
    bool save_preset(const std::string& name);
    void close();
    void detach();

    void tick(const engine::PlayerContext& player, double delta);
    void invalidate() { dirty_ = true; }

    void on_key(const std::string& action);

private:
    bool attach(const engine::PlayerContext& player);
    void order_tabs();
    void tick_impl(const engine::PlayerContext& player);
    void process_files(const engine::PlayerContext& player);
    bool request_file(runtime::FileOperation operation, std::string name, std::string payload = {}, bool replace_existing = false);
    void save_from_field();
    void confirm_file_action(bool confirm);
    void poll_save_buttons();
    bool save_field_focused() const;
    void clear_save_confirmation();
    bool render_save_editor();
    void navigate(int index);
    void forget();
    bool rebuild_page(const engine::PlayerContext& player);
    void measure_page(uint64_t now);
    UObject* make_text(UObject* tree, const std::string& text, float size, engine::Color color, UObject* font = nullptr);
    UObject* make_border(UObject* tree, engine::Color color);
    UObject* place_widget(UObject* canvas, UObject* child, double x, double y, double width, double height);

    // Screen builders (implemented in menu_page.cpp)
    void render_customize_screen(UObject* canvas, UObject* tree);
    void render_preset_screen(UObject* canvas, UObject* tree);
    void render_settings_screen(UObject* canvas, UObject* tree);

    Deps deps_;

    // Hosting & Unreal UMG handles
    bool active_{false};
    bool dirty_{true};
    engine::WeakObject pc_;
    engine::WeakObject handler_;
    engine::WeakObject main_;
    engine::WeakObject tabs_;
    engine::WeakObject switcher_;
    engine::WeakObject page_;
    engine::WeakObject tab_;
    engine::WeakObject tree_;
    engine::WeakObject canvas_;
    int tab_index_{-1};
    uint64_t attach_wait_since_{0};
    uint64_t discover_after_{0};
    uint64_t order_check_after_{0};
    uint64_t open_requested_at_{0};
    uint64_t generation_check_after_{0};
    uint64_t retry_after_{0};
    std::string last_error_;
    std::optional<uint64_t> list_request_, action_request_, settings_request_;
    uint64_t generation_{}, action_generation_{};
    bool refresh_presets_{true}, settings_dirty_{};
    std::string file_status_;
    std::optional<runtime::FileOperation> file_error_operation_;
    std::string pending_delete_;
    bool open_requested_{false};
    bool entered_{false};
    MenuInput input_;
    bool input_ready_{};
    uint64_t input_check_after_{};
    std::string input_error_;
    std::array<WidgetPool, 3> page_pools_;
    WidgetPool save_pool_;
    engine::WeakObject save_name_input_, save_button_, replace_button_, keep_button_;
    std::array<bool, 3> save_buttons_down_{};
    std::string save_name_, save_payload_, overwrite_name_;
    bool save_editor_ready_{}, focus_name_requested_{};
    bool save_mouse_suppressed_{true};
    int overwrite_focus_{1};
    engine::Vec2 page_extent_{};
    uint64_t layout_check_after_{};

    // Navigation state
    MenuScreen current_screen_{MenuScreen::Customize};
    FocusArea focus_area_{FocusArea::SlotGrid};
    SlotId selected_slot_{SlotId::LF};      // Default to LF as in screenshot!
    int accordion_section_{1};              // 0: Player's Weapon, 1: Tarstones, 2: Enemy's Weapon
    int accordion_item_{0};                 // Index within expanded accordion category
    int preset_index_{0};                   // Selected preset in list
    int settings_index_{0};                 // Selected setting in settings tab
    bool inspecting_details_{false};

    std::vector<std::string> cached_preset_names_;
};

} // namespace ccs

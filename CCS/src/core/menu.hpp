#pragma once
#include "engine.hpp"
#include "settings.hpp"
#include "storage.hpp"
#include "moveset_manager.hpp"
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
        runtime::Storage* storage{nullptr};
        MovesetManager* movesets{nullptr};
        std::function<void(const std::string&)> log;
    };

    explicit Menu(Deps deps);
    ~Menu();

    bool is_open() const { return active_; }
    bool attached() const { return page_.Get() != nullptr; }

    bool open(const engine::PlayerContext& player);
    void close();
    void detach();

    void tick(const engine::PlayerContext& player, double delta);
    void invalidate() { dirty_ = true; }

    void on_key(const std::string& action);

private:
    bool attach(const engine::PlayerContext& player);
    void order_tabs();
    void rebuild_page(const engine::PlayerContext& player);

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

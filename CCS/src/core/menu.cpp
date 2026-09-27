#include "menu.hpp"
#include "menu_keys.hpp"
#include <algorithm>
#include <windows.h>

namespace ccs {
using namespace engine;

Menu::Menu(Deps deps) : deps_(std::move(deps)) {}

Menu::~Menu() {
    try { detach(); } catch (...) {}
}

bool Menu::attach(const PlayerContext& player) {
    if (!player.pc) return false;
    auto* handler = object_of(player.pc, L"User Interface Handler Component");
    if (!handler) return false;
    auto* game = object_of(handler, L"WBP_Menu_Game");
    if (!game) return false;
    auto* main = object_of(game, L"WBP_Menu_Main");
    if (!main || !bool_of(main, L"bOpen")) return false;

    auto* tabs = object_of(main, L"BP_HBC_Menu_Game");
    auto* pages = object_of(main, L"BP_WS_Menu_Game");
    auto* original = object_of(main, L"WBP_NBM_Inventory");
    if (!tabs || !pages || !original) return false;

    const auto page_list = children(pages);
    const auto tab_list = children(tabs);

    // Wait until existing native & CSS/CSSX tabs have attached
    const auto now = GetTickCount64();
    if (page_list.size() < 3) {
        if (!attach_wait_since_) attach_wait_since_ = now;
        if (now - attach_wait_since_ < 2000) return false;
    }
    attach_wait_since_ = 0;

    // Create our native tab button
    auto* tab = create_widget(player.pc, original->GetClassPrivate());
    for (auto name : {L"FontData", L"RootSize", L"RootScale", L"DefaultColor", L"SelectedColor", L"bUseHighlight", L"HighlightY"}) {
        copy_property(tab, original, name);
    }
    text_property(tab, L"Text", "CCS");
    invoke(tab, L"UpdateText");
    invoke(tab, L"CommitSize");
    invoke(tab, L"CommitScale");

    // Create our page widget with a CanvasPanel root
    auto* page = create_widget(player.pc, static_cast<UClass*>(main->GetClassPrivate()->GetSuperStruct()));
    auto* tree = object_of(page, L"WidgetTree");
    if (!tree) return false;
    auto* canvas = construct(L"/Script/UMG.CanvasPanel", tree);
    object_property(tree, L"RootWidget", canvas);

    // Add to Player Menu
    { Call add(pages, L"AddChild", 2); add.set(L"content", page); add.run(); }
    { Call add(tabs, L"AddChildToHorizontalBox", 2); add.set(L"content", tab); add.run(); }

    pc_ = player.pc;
    handler_ = handler;
    main_ = main;
    tabs_ = tabs;
    switcher_ = pages;
    page_ = page;
    tab_ = tab;
    tree_ = tree;
    canvas_ = canvas;

    order_tabs();

    if (deps_.log) deps_.log("CCS: Attached tab to Player Menu at index " + std::to_string(tab_index_));
    dirty_ = true;
    return true;
}

void Menu::order_tabs() {
    auto tabs = children(tabs_.Get());
    auto pages = children(switcher_.Get());
    if (tabs.empty() || tabs.size() != pages.size()) return;

    // CCS sits at the end of the tabs list
    const int index = static_cast<int>(tabs.size()) - 1;
    tab_index_ = index;

    // Adjust top bar slot padding
    if (auto* slot = object_of(tab_.Get(), L"Slot")) {
        invoke(slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{2});
        invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
    }
    nav_children_refresh(tabs_.Get());
}

bool Menu::open(const PlayerContext& player) {
    if (!player.pc) return false;
    auto* handler = object_of(player.pc, L"User Interface Handler Component");
    if (!handler) return false;
    if (active_) return true;

    auto* main = object_of(object_of(handler, L"WBP_Menu_Game"), L"WBP_Menu_Main");
    if (main && bool_of(main, L"bOpen") && page_.Get()) {
        Call nav(main, L"BP_NavigateToIndex", 2);
        nav.set(L"Index", tab_index_);
        nav.run();
        active_ = true;
        dirty_ = true;
        return true;
    }

    Call open_call(handler, L"HandleGameMenu", 2);
    open_call.set(L"SubTabIndex", int32_t{0});
    open_call.set(L"AllowClose", false);
    open_call.run();
    return true;
}

void Menu::close() {
    if (!main_.Get() || !bool_of(main_.Get(), L"bOpen")) return;
    auto* handler = handler_.Get();
    if (!handler) return;

    Call close_call(handler, L"HandleGameMenu", 2);
    close_call.set(L"SubTabIndex", int32_t{0});
    close_call.set(L"AllowClose", true);
    close_call.run();
    active_ = false;
}

void Menu::detach() {
    if (auto* page = page_.Get()) {
        try { invoke(page, L"RemoveFromParent"); } catch (...) {}
    }
    if (auto* tab = tab_.Get()) {
        try { invoke(tab, L"RemoveFromParent"); } catch (...) {}
    }
    page_.Reset();
    tab_.Reset();
    tree_.Reset();
    canvas_.Reset();
    main_.Reset();
    tabs_.Reset();
    switcher_.Reset();
    handler_.Reset();
    pc_.Reset();
    active_ = false;
    tab_index_ = -1;
    dirty_ = true;
}

void Menu::tick(const PlayerContext& player, double /*delta*/) {
    if (!main_.Get() || !bool_of(main_.Get(), L"bOpen")) {
        if (page_.Get()) detach();
        return;
    }

    if (!page_.Get()) {
        if (!attach(player)) return;
    }

    // Check if player navigated to our tab
    if (main_.Get()) {
        Call current(main_.Get(), L"GetCurrentIndex", 1);
        current.run();
        int active_tab = current.get<int32_t>();
        bool was_active = active_;
        active_ = (active_tab == tab_index_);
        if (active_ && !was_active) {
            dirty_ = true;
        }
    }

    if (active_ && dirty_) {
        rebuild_page(player);
        dirty_ = false;
    }
}

void Menu::rebuild_page(const PlayerContext& /*player*/) {
    auto* canvas = canvas_.Get();
    auto* tree = tree_.Get();
    if (!canvas || !tree) return;

    // Clear previous children
    Call clear(canvas, L"ClearChildren", 0);
    clear.run();

    if (deps_.storage) {
        cached_preset_names_ = deps_.storage->list_presets();
    }

    switch (current_screen_) {
        case MenuScreen::Customize:
            render_customize_screen(canvas, tree);
            break;
        case MenuScreen::Preset:
            render_preset_screen(canvas, tree);
            break;
        case MenuScreen::Settings:
            render_settings_screen(canvas, tree);
            break;
    }
}

void Menu::on_key(const std::string& action) {
    if (!active_) return;

    // Global tab switching with LB / RB or Z / X
    if (action == NavAction::TabLeft) {
        int s = static_cast<int>(current_screen_);
        s = (s - 1 + 3) % 3;
        current_screen_ = static_cast<MenuScreen>(s);
        dirty_ = true;
        return;
    }
    if (action == NavAction::TabRight) {
        int s = static_cast<int>(current_screen_);
        s = (s + 1) % 3;
        current_screen_ = static_cast<MenuScreen>(s);
        dirty_ = true;
        return;
    }

    if (current_screen_ == MenuScreen::Customize) {
        if (focus_area_ == FocusArea::SlotGrid) {
            if (action == NavAction::Left) {
                int s = static_cast<int>(selected_slot_);
                if (s % 5 > 0) { selected_slot_ = static_cast<SlotId>(s - 1); dirty_ = true; }
                else { focus_area_ = FocusArea::LeftAccordion; dirty_ = true; }
            } else if (action == NavAction::Right) {
                int s = static_cast<int>(selected_slot_);
                if (s % 5 < 4) { selected_slot_ = static_cast<SlotId>(s + 1); dirty_ = true; }
            } else if (action == NavAction::Down) {
                int s = static_cast<int>(selected_slot_);
                if (s < 5) { selected_slot_ = static_cast<SlotId>(s + 5); dirty_ = true; }
            } else if (action == NavAction::Up) {
                int s = static_cast<int>(selected_slot_);
                if (s >= 5) { selected_slot_ = static_cast<SlotId>(s - 5); dirty_ = true; }
            } else if (action == NavAction::Unequip) {
                if (deps_.movesets) deps_.movesets->unequip_slot(selected_slot_);
                dirty_ = true;
            } else if (action == NavAction::Accept) {
                focus_area_ = FocusArea::LeftAccordion;
                dirty_ = true;
            } else if (action == NavAction::Details) {
                inspecting_details_ = !inspecting_details_;
                dirty_ = true;
            }
        } else if (focus_area_ == FocusArea::LeftAccordion) {
            if (action == NavAction::Right || action == NavAction::Cancel) {
                focus_area_ = FocusArea::SlotGrid;
                dirty_ = true;
            } else if (action == NavAction::Up) {
                if (accordion_item_ > 0) accordion_item_--;
                else if (accordion_section_ > 0) { accordion_section_--; accordion_item_ = 0; }
                dirty_ = true;
            } else if (action == NavAction::Down) {
                accordion_item_++;
                dirty_ = true;
            } else if (action == NavAction::Accept) {
                // Apply selection to active slot
                if (deps_.movesets) {
                    if (accordion_section_ == 1) { // Tarstones
                        auto stones = deps_.movesets->get_tarstones_for_slot(selected_slot_);
                        if (accordion_item_ >= 0 && accordion_item_ < static_cast<int>(stones.size())) {
                            deps_.movesets->assign_tarstone_to_slot(selected_slot_, stones[accordion_item_]);
                        }
                    } else { // Moves
                        auto moves = deps_.movesets->get_moves_for_slot(selected_slot_);
                        if (accordion_item_ >= 0 && accordion_item_ < static_cast<int>(moves.size())) {
                            deps_.movesets->assign_move_to_slot(selected_slot_, moves[accordion_item_]);
                        }
                    }
                }
                focus_area_ = FocusArea::SlotGrid;
                dirty_ = true;
            }
        }
    } else if (current_screen_ == MenuScreen::Preset) {
        if (action == NavAction::Up) {
            if (preset_index_ > 0) { preset_index_--; dirty_ = true; }
        } else if (action == NavAction::Down) {
            if (preset_index_ + 1 < static_cast<int>(cached_preset_names_.size())) { preset_index_++; dirty_ = true; }
        } else if (action == NavAction::Accept) {
            // Load preset
            if (deps_.storage && deps_.movesets && preset_index_ >= 0 && preset_index_ < static_cast<int>(cached_preset_names_.size())) {
                auto loaded = deps_.storage->load_preset(cached_preset_names_[preset_index_]);
                if (loaded) {
                    deps_.movesets->set_active_preset(*loaded);
                    if (deps_.settings) deps_.settings->set_startup_preset(cached_preset_names_[preset_index_]);
                }
            }
            dirty_ = true;
        } else if (action == NavAction::Unequip) {
            // Delete preset
            if (deps_.storage && preset_index_ >= 0 && preset_index_ < static_cast<int>(cached_preset_names_.size())) {
                deps_.storage->delete_preset(cached_preset_names_[preset_index_]);
                cached_preset_names_ = deps_.storage->list_presets();
                if (preset_index_ >= static_cast<int>(cached_preset_names_.size())) {
                    preset_index_ = std::max(0, static_cast<int>(cached_preset_names_.size()) - 1);
                }
            }
            dirty_ = true;
        }
    } else if (current_screen_ == MenuScreen::Settings) {
        if (action == NavAction::Up) {
            if (settings_index_ > 0) { settings_index_--; dirty_ = true; }
        } else if (action == NavAction::Down) {
            if (settings_index_ < 3) { settings_index_++; dirty_ = true; }
        } else if (action == NavAction::Left || action == NavAction::Right || action == NavAction::Accept) {
            if (deps_.settings) {
                if (settings_index_ == 0) {
                    deps_.settings->set_enabled(!deps_.settings->enabled());
                } else if (settings_index_ == 1) {
                    deps_.settings->set_preserve_weapon_mesh(!deps_.settings->preserve_weapon_mesh());
                } else if (settings_index_ == 2) {
                    deps_.settings->set_show_hud_notification(!deps_.settings->show_hud_notification());
                } else if (settings_index_ == 3) {
                    // Reset to vanilla
                    if (deps_.storage && deps_.movesets) {
                        auto def = deps_.storage->load_preset("default");
                        if (def) deps_.movesets->set_active_preset(*def);
                    }
                }
                deps_.settings->save();
            }
            dirty_ = true;
        }
    }
}

} // namespace ccs

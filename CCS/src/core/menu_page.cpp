#include "menu.hpp"
#include "menu_keys.hpp"
#include "visible_window.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

namespace ccs {
using namespace engine;

namespace {
constexpr const char* serif_font = "/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font";
constexpr const char* title_font = "/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font";

// Slate & UMG Colors
constexpr Color ink_title{0.90f, 0.82f, 0.65f, 1.0f};      // Warm gold/sand
constexpr Color ink_body{0.75f, 0.70f, 0.60f, 1.0f};       // Light beige
constexpr Color ink_muted{0.45f, 0.40f, 0.35f, 1.0f};      // Muted brown
constexpr Color ink_accent{0.95f, 0.55f, 0.15f, 1.0f};     // Highlight orange
constexpr Color ink_green{0.35f, 0.80f, 0.40f, 1.0f};      // Good/enabled
constexpr Color ink_dark_bg{0.05f, 0.04f, 0.03f, 0.85f};   // Panel background

} // namespace

UObject* Menu::make_text(UObject*, const std::string& text, float size, Color color, UObject* font) {
    return page_pools_[static_cast<size_t>(current_screen_)].text(text, size, color, font);
}
UObject* Menu::make_border(UObject*, Color color) {
    return page_pools_[static_cast<size_t>(current_screen_)].border(color);
}
UObject* Menu::place_widget(UObject*, UObject* child, double x, double y, double width, double height) {
    page_pools_[static_cast<size_t>(current_screen_)].place(child, x, y, width, height);
    return child;
}

void Menu::render_customize_screen(UObject* canvas, UObject* tree) {
    auto* font_title_obj = find_optional(wide(title_font).c_str());
    auto* font_serif_obj = find_optional(wide(serif_font).c_str());

    // -------------------------------------------------------------
    // 1. Top Header & Tabs Bar
    // -------------------------------------------------------------
    auto* logo_box = make_border(tree, ink_dark_bg);
    place_widget(canvas, logo_box, 80, 60, 480, 100);
    auto* logo_text = make_text(tree, "CUSTOM COMBAT SYSTEM", 22.0f, ink_title, font_title_obj);
    place_widget(canvas, logo_text, 100, 95, 0, 0);

    // Section labels use the current input mappings.
    std::string tabs_text = input_.hint(runtime::MenuAction::TabLeft) + "  < Customize >  Preset  Settings  " + input_.hint(runtime::MenuAction::TabRight);
    auto* tabs_widget = make_text(tree, tabs_text, 18.0f, ink_body, font_title_obj);
    place_widget(canvas, tabs_widget, 80, 175, 800, 40);

    // -------------------------------------------------------------
    // 2. Left Column: Selection Accordion
    // -------------------------------------------------------------
    auto* left_panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, left_panel, 80, 240, 520, 750);

    double left_y = 260;

    static constexpr std::array<const char*, 3> groups{"Player's Weapon", "Tarstones", "Enemy's Weapon"};
    for (int section = 0; section < 3; ++section) {
        const bool selected_group = accordion_section_ == section;
        const bool focused = selected_group && focus_area_ == FocusArea::LeftAccordion;
        auto* heading = make_text(tree, std::string(focused ? "> " : "  ") + groups[section], 18.0f,
                                  focused ? ink_accent : ink_title, font_title_obj);
        place_widget(canvas, heading, 110, left_y, 460, 30); left_y += 40;
        if (!selected_group) continue;
        const auto stones = deps_.movesets ? deps_.movesets->get_tarstones_for_slot(selected_slot_) : std::span<const TarstoneDefinition* const>{};
        const auto moves = deps_.movesets ? deps_.movesets->get_moves_for_slot(selected_slot_, section == 2) : std::span<const MoveDefinition* const>{};
        const size_t count = section == 1 ? stones.size() : moves.size();
        if (!count) {
            std::string message = section == 2 ? "No verified enemy moves discovered." :
                section == 1 ? "Owned Tarstones have not been verified yet." : "Reading current weapon moves...";
            if (section == 0 && deps_.loaded_moves) {
                if (const auto snapshot = deps_.loaded_moves->snapshot())
                    message = std::to_string(snapshot->candidates.size()) + (deps_.loaded_moves->state() == "reading" ?
                        " references from the last completed scan. Refreshing..." : " move references found. Compatibility checks are pending.");
                else if (deps_.loaded_moves->state() == "unavailable" || deps_.loaded_moves->state() == "budget_exceeded")
                    message = "Current weapon move data is unavailable.";
                else if (deps_.loaded_moves->state() == "waiting_for_player") message = "Waiting for an equipped weapon.";
                else if (deps_.loaded_moves->state() == "content_pending") message = "Checking installed game files...";
                else if (deps_.loaded_moves->state() == "content_unavailable") message = "Game file inspection failed. Move discovery is paused.";
                else if (deps_.loaded_moves->state() == "content_changed") message = "Game files changed. Restart the game before discovering moves.";
            }
            auto* empty = make_text(tree, message, 14.0f, ink_muted, font_serif_obj);
            place_widget(canvas, empty, 120, left_y, 450, 45); left_y += 55;
            continue;
        }
        const size_t chosen = static_cast<size_t>(std::clamp(accordion_item_, 0, static_cast<int>(count) - 1));
        const auto visible = runtime::visible_window(count, chosen, 8);
        for (size_t i = visible.begin; i < visible.end; ++i) {
            const bool selected = focused && i == chosen;
            const auto& label = section == 1 ? stones[i]->display_name : moves[i]->display_name;
            auto* row = make_text(tree, std::string(selected ? "> " : "  ") + label, 15.0f,
                                  selected ? ink_accent : ink_body, font_serif_obj);
            place_widget(canvas, row, 120, left_y, 450, 28); left_y += 32;
        }
        auto* source = make_text(tree, "Current weapon data", 13.0f, ink_muted, font_serif_obj);
        place_widget(canvas, source, 120, left_y, 450, 45); left_y += 60;
    }

    double grid_x = 650;
    double grid_y = 700;
    double slot_w = 95;
    double slot_h = 95;
    double gap_x = 35;
    double gap_y = 80;

    const char* row1_labels[5] = {"L1", "L2", "L3", "LF", "LC"};
    const char* row2_labels[5] = {"H1", "H2", "H3", "HF", "HC"};

    // Light Chain Row
    for (int col = 0; col < 5; ++col) {
        SlotId slot = static_cast<SlotId>(col);
        bool is_selected = (slot == selected_slot_);
        double x = grid_x + col * (slot_w + gap_x);
        double y = grid_y;

        auto* label = make_text(tree, row1_labels[col], 18.0f, is_selected ? ink_accent : ink_title, font_title_obj);
        place_widget(canvas, label, x + 35, y - 35, 0, 0);

        Color border_color = is_selected ? ink_accent : Color{0.25f, 0.22f, 0.18f, 1.0f};
        auto* box = make_border(tree, border_color);
        place_widget(canvas, box, x, y, slot_w, slot_h);

        // Display move / stone info inside
        if (deps_.movesets) {
            const auto& binding = deps_.movesets->active_preset().slots[static_cast<size_t>(slot)];
            std::string inside_text = binding.tarstone_id.empty() ? (binding.move_id.empty() ? "Empty" : "Move") : "Tarstone";
            auto* inside = make_text(tree, inside_text, 11.0f, is_selected ? ink_accent : ink_body, font_serif_obj);
            place_widget(canvas, inside, x + 10, y + 35, slot_w - 20, 45);
        }
    }

    // Heavy Chain Row
    for (int col = 0; col < 5; ++col) {
        SlotId slot = static_cast<SlotId>(col + 5);
        bool is_selected = (slot == selected_slot_);
        double x = grid_x + col * (slot_w + gap_x);
        double y = grid_y + slot_h + gap_y;

        auto* label = make_text(tree, row2_labels[col], 18.0f, is_selected ? ink_accent : ink_title, font_title_obj);
        place_widget(canvas, label, x + 35, y - 35, 0, 0);

        Color border_color = is_selected ? ink_accent : Color{0.25f, 0.22f, 0.18f, 1.0f};
        auto* box = make_border(tree, border_color);
        place_widget(canvas, box, x, y, slot_w, slot_h);

        if (deps_.movesets) {
            const auto& binding = deps_.movesets->active_preset().slots[static_cast<size_t>(slot)];
            std::string inside_text = binding.tarstone_id.empty() ? (binding.move_id.empty() ? "Empty" : "Move") : "Tarstone";
            auto* inside = make_text(tree, inside_text, 11.0f, is_selected ? ink_accent : ink_body, font_serif_obj);
            place_widget(canvas, inside, x + 10, y + 35, slot_w - 20, 45);
        }
    }

    // -------------------------------------------------------------
    // 4. Inspection state. Native item card integration is pending.
    // -------------------------------------------------------------
    auto* right_card = make_border(tree, ink_dark_bg);
    place_widget(canvas, right_card, 1340, 60, 500, 930);

    double right_y = 90;

    std::string card_title = "EMPTY SLOT";
    std::string card_sub = slot_to_string(selected_slot_);
    std::string card_desc = "Select a move or Tarstone from the browser.";
    if (deps_.movesets) {
        const auto& binding = deps_.movesets->active_preset().slots[static_cast<size_t>(selected_slot_)];
        if (!binding.tarstone_id.empty()) {
            if (const auto* stone = deps_.movesets->find_tarstone(binding.tarstone_id)) {
                card_title = stone->display_name;
                card_desc = stone->description;
                card_sub = "Extracted metadata. Ownership and level are not verified.";
            } else { card_title = "TARSTONE UNAVAILABLE"; card_desc = "This preset references a Tarstone that has not been discovered."; }
        } else if (!binding.move_id.empty()) {
            if (const auto* move = deps_.movesets->find_move(binding.move_id)) {
                card_title = move->display_name;
                card_desc = move->description;
                card_sub = move->source_name;
            } else { card_title = "MOVE UNAVAILABLE"; card_desc = "This preset references a move that has not been discovered."; }
        }
    }
    auto* rt_title = make_text(tree, card_title, 22.0f, ink_title, font_title_obj);
    place_widget(canvas, rt_title, 1380, right_y, 420, 50);
    auto* rt_sub = make_text(tree, card_sub, 14.0f, ink_muted, font_serif_obj);
    place_widget(canvas, rt_sub, 1380, right_y + 60, 420, 90);
    auto* rt_desc = make_text(tree, card_desc, 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, rt_desc, 1380, right_y + 180, 420, 500);

    // -------------------------------------------------------------
    // 5. Footer Bar
    // -------------------------------------------------------------
    auto* footer = make_text(tree, "Select: " + input_.hint(runtime::MenuAction::Accept) + "    Back: " + input_.hint(runtime::MenuAction::Cancel) + "    Unequip: " + input_.hint(runtime::MenuAction::Unequip), 15.0f, ink_muted, font_serif_obj);
    place_widget(canvas, footer, 80, 1010, 1760, 40);
}

void Menu::render_preset_screen(UObject* canvas, UObject* tree) {
    auto* font_title_obj = find_optional(wide(title_font).c_str());
    auto* font_serif_obj = find_optional(wide(serif_font).c_str());

    // Top Header & Tabs Bar
    auto* logo_box = make_border(tree, ink_dark_bg);
    place_widget(canvas, logo_box, 80, 60, 480, 100);
    auto* logo_text = make_text(tree, "CUSTOM COMBAT SYSTEM", 22.0f, ink_title, font_title_obj);
    place_widget(canvas, logo_text, 100, 95, 0, 0);

    std::string tabs_text = input_.hint(runtime::MenuAction::TabLeft) + "  Customize  < Preset >  Settings  " + input_.hint(runtime::MenuAction::TabRight);
    auto* tabs_widget = make_text(tree, tabs_text, 18.0f, ink_body, font_title_obj);
    place_widget(canvas, tabs_widget, 80, 175, 800, 40);

    // Presets List Panel
    auto* list_panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, list_panel, 80, 240, 800, 730);

    auto* header = make_text(tree, "PRESETS", 18.0f, ink_title, font_title_obj);
    place_widget(canvas, header, 110, 260, 0, 0);

    double py = 300;
    if (cached_preset_names_.empty()) {
        auto* empty_text = make_text(tree, list_request_ ? "Loading presets..." : "No presets saved yet.", 16.0f, ink_muted, font_serif_obj);
        place_widget(canvas, empty_text, 110, py, 0, 0);
    } else {
        const size_t count = cached_preset_names_.size();
        const size_t selected = static_cast<size_t>(std::clamp(preset_index_, 0, static_cast<int>(count) - 1));
        const auto visible = runtime::visible_window(count, selected, 14);
        for (size_t i = visible.begin; i < visible.end; ++i) {
            bool is_selected = (preset_index_ == static_cast<int>(i));
            std::string prefix = is_selected ? "  ►  " : "     ";
            auto* item = make_text(tree, prefix + cached_preset_names_[i] + ".json", 18.0f, is_selected ? ink_accent : ink_body, font_serif_obj);
            place_widget(canvas, item, 110, py, 0, 0);
            py += 45;
        }
    }

    // Right Side: Active Preset Summary
    auto* info_panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, info_panel, 920, 190, 920, 780);

    double iy = 220;
    auto* info_header = make_text(tree, "ACTIVE PRESET DETAILS", 20.0f, ink_title, font_title_obj);
    place_widget(canvas, info_header, 950, iy, 0, 0);
    iy += 50;

    if (deps_.movesets) {
        const auto& p = deps_.movesets->active_preset();
        auto* name_w = make_text(tree, "Name: " + p.name, 18.0f, ink_accent, font_serif_obj);
        place_widget(canvas, name_w, 950, iy, 0, 0);
        iy += 35;
        auto* author_w = make_text(tree, "Author: " + p.author, 16.0f, ink_body, font_serif_obj);
        place_widget(canvas, author_w, 950, iy, 0, 0);
        iy += 35;
        auto* base_w = make_text(tree, "Base weapon: " + (p.base_weapon.empty() ? "Not specified" : p.base_weapon), 16.0f, ink_body, font_serif_obj);
        place_widget(canvas, base_w, 950, iy, 0, 0);
        iy += 45;
        auto* desc_w = make_text(tree, "Description: " + p.description, 16.0f, ink_muted, font_serif_obj);
        place_widget(canvas, desc_w, 950, iy, 800, 240);
    }

    // Footer Prompts
    auto* footer = make_text(tree, "Load: " + input_.hint(runtime::MenuAction::Accept) + "    Delete: " + input_.hint(runtime::MenuAction::Unequip) + "    Name a copy: " + input_.hint(runtime::MenuAction::Details) + "    " + file_status_, 15.0f, ink_muted, font_serif_obj);
    place_widget(canvas, footer, 80, 1010, 1760, 40);
}

bool Menu::render_save_editor() {
    save_editor_ready_ = false;
    auto& page = page_pools_[static_cast<size_t>(MenuScreen::Preset)];
    if (!save_pool_.begin(canvas_.Get(), tree_.Get(), tab_.Get(), page_extent_, page.remaining_budget())) return false;
    auto line = [&](const std::string& value, double y, float size, Color color) {
        auto* widget = save_pool_.text(value, size, color, nullptr);
        save_pool_.place(widget, 950, y, 820, 65);
    };
    const bool confirming = !overwrite_name_.empty() || !pending_delete_.empty();
    if (confirming) {
        const bool deleting = !pending_delete_.empty();
        line(deleting ? "Delete saved preset \"" + pending_delete_ + "\"?" :
            "Replace saved preset \"" + overwrite_name_ + "\"?", 650, 18, ink_title);
        line("Your active moves remain unchanged.", 725, 15, ink_body);
        auto* replace = save_pool_.button(deleting ? "Delete" : "Replace", 17, overwrite_focus_ == 0 ? ink_accent : ink_body);
        save_pool_.place(replace, 950, 810, 330, 65); replace_button_ = replace;
        auto* keep = save_pool_.button("Cancel", 17, overwrite_focus_ == 1 ? ink_accent : ink_body);
        save_pool_.place(keep, 1350, 810, 330, 65); keep_button_ = keep;
    } else {
        line("Save a copy", 650, 18, ink_title);
        auto* input = save_pool_.editable(save_name_, 18, !action_request_.has_value());
        save_pool_.place(input, 950, 725, 550, 60); save_name_input_ = input;
        auto* save = save_pool_.button("Save", 17, ink_body, !action_request_.has_value());
        save_pool_.place(save, 1570, 725, 200, 60); save_button_ = save;
        line("Letters, numbers, dots, hyphens and underscores. Maximum 96 characters.", 805, 14, ink_body);
        line(file_status_, 875, 14, ink_body);
    }
    save_editor_ready_ = save_pool_.finish();
    if (save_editor_ready_ && focus_name_requested_ && !confirming && save_name_input_.Get()) {
        invoke(save_name_input_.Get(), L"SetKeyboardFocus"); focus_name_requested_ = false;
    }
    return save_editor_ready_;
}

void Menu::render_settings_screen(UObject* canvas, UObject* tree) {
    auto* font_title_obj = find_optional(wide(title_font).c_str());
    auto* font_serif_obj = find_optional(wide(serif_font).c_str());

    // Top Header & Tabs Bar
    auto* logo_box = make_border(tree, ink_dark_bg);
    place_widget(canvas, logo_box, 80, 60, 480, 100);
    auto* logo_text = make_text(tree, "CUSTOM COMBAT SYSTEM", 22.0f, ink_title, font_title_obj);
    place_widget(canvas, logo_text, 100, 95, 0, 0);

    std::string tabs_text = input_.hint(runtime::MenuAction::TabLeft) + "  Customize  Preset  < Settings >  " + input_.hint(runtime::MenuAction::TabRight);
    auto* tabs_widget = make_text(tree, tabs_text, 18.0f, ink_body, font_title_obj);
    place_widget(canvas, tabs_widget, 80, 175, 800, 40);

    // Settings Panel
    auto* panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, panel, 80, 240, 1760, 730);

    double sy = 270;

    auto* header = make_text(tree, "CONFIG ITEM                                              VALUE                       DESCRIPTION", 17.0f, ink_title, font_title_obj);
    place_widget(canvas, header, 120, sy, 0, 0);
    sy += 50;

    bool master_enabled = deps_.settings ? deps_.settings->enabled() : true;
    bool mesh_preserve = deps_.settings ? deps_.settings->preserve_weapon_mesh() : true;
    bool hud_notify = deps_.settings ? deps_.settings->show_hud_notification() : true;

    struct SettingRow {
        std::string label;
        std::string val;
        std::string desc;
        Color val_color;
    };

    std::vector<SettingRow> rows = {
        {
            "Custom Combat System (Master)",
            master_enabled ? "[ < REQUESTED ON > ]" : "[ < REQUESTED OFF > ]",
            "Requested setting. Combat routing is currently unavailable.",
            master_enabled ? ink_green : ink_accent
        },
        {
            "Preserve Weapon Mesh on Enemy Moves",
            mesh_preserve ? "[ < ON > ]" : "[ < OFF > ]",
            "Requested setting. Enemy attack compatibility is not yet verified.",
            ink_body
        },
        {
            "Show Combat Move HUD Notification",
            hud_notify ? "[ < ON > ]" : "[ < OFF > ]",
            "Requested setting. Combat notifications are not yet implemented.",
            ink_body
        },
        {
            "Reset All Slots to Vanilla Defaults",
            "[ UNAVAILABLE ]",
            "Requires the current weapon's verified live bindings.",
            ink_muted
        }
    };

    for (size_t i = 0; i < rows.size(); ++i) {
        bool is_sel = (settings_index_ == static_cast<int>(i));
        std::string prefix = is_sel ? "► " : "  ";
        auto* label_w = make_text(tree, prefix + rows[i].label, 16.0f, is_sel ? ink_accent : ink_body, font_serif_obj);
        place_widget(canvas, label_w, 120, sy, 0, 0);

        auto* val_w = make_text(tree, rows[i].val, 16.0f, rows[i].val_color, font_title_obj);
        place_widget(canvas, val_w, 650, sy, 0, 0);

        auto* desc_w = make_text(tree, rows[i].desc, 15.0f, ink_muted, font_serif_obj);
        place_widget(canvas, desc_w, 1000, sy, 0, 0);

        sy += 55;
    }

    // Footer Prompts
    auto* footer = make_text(tree, "Toggle: " + input_.hint(runtime::MenuAction::Accept) + "    " + file_status_, 15.0f, ink_muted, font_serif_obj);
    place_widget(canvas, footer, 80, 1010, 1760, 40);
}

} // namespace ccs

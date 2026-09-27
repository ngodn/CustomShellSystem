#include "menu.hpp"
#include "menu_keys.hpp"
#include <windows.h>
#include <string>
#include <vector>

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

UObject* place_widget(UObject* canvas, UObject* child, double x, double y, double w, double h) {
    Call add(canvas, L"AddChildToCanvas", 2);
    add.set(L"content", child);
    add.run();
    auto* slot = add.get<UObject*>();
    invoke(slot, L"SetPosition", L"InPosition", Vec2{x, y});
    if (w > 0 && h > 0) {
        invoke(slot, L"SetSize", L"InSize", Vec2{w, h});
    } else {
        invoke(slot, L"SetAutoSize", L"InbAutoSize", true);
    }
    return slot;
}

UObject* make_text(UObject* tree, const std::string& text, float size, Color color, UObject* font = nullptr) {
    auto* block = construct(L"/Script/UMG.TextBlock", tree);
    font_size(block, size, font);
    invoke(block, L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{color});
    text_value(block, text);
    return block;
}

UObject* make_border(UObject* tree, Color bg_color) {
    auto* border = construct(L"/Script/UMG.Border", tree);
    invoke(border, L"SetBrushColor", L"InBrushColor", bg_color);
    return border;
}

} // namespace

void Menu::render_customize_screen(UObject* canvas, UObject* tree) {
    auto* font_title_obj = load(title_font);
    auto* font_serif_obj = load(serif_font);

    // -------------------------------------------------------------
    // 1. Top Header & Tabs Bar
    // -------------------------------------------------------------
    auto* logo_box = make_border(tree, ink_dark_bg);
    place_widget(canvas, logo_box, 80, 60, 480, 100);
    auto* logo_text = make_text(tree, "CUSTOM COMBAT SYSTEM", 22.0f, ink_title, font_title_obj);
    place_widget(canvas, logo_text, 100, 95, 0, 0);

    // Tabs: [Z] CUSTOMIZE | PRESET | SETTINGS [X]
    std::string tabs_text = "[Z]   < CUSTOMIZE >      PRESET      SETTINGS   [X]";
    auto* tabs_widget = make_text(tree, tabs_text, 18.0f, ink_body, font_title_obj);
    place_widget(canvas, tabs_widget, 620, 95, 0, 0);

    // -------------------------------------------------------------
    // 2. Left Column: Selection Accordion
    // -------------------------------------------------------------
    auto* left_panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, left_panel, 80, 190, 520, 800);

    double left_y = 210;

    // Category 1: Player's Weapon
    bool cat1_sel = (focus_area_ == FocusArea::LeftAccordion && accordion_section_ == 0);
    auto* cat1_title = make_text(tree, cat1_sel ? "> PLAYER'S WEAPON" : "  PLAYER'S WEAPON", 18.0f, cat1_sel ? ink_accent : ink_title, font_title_obj);
    place_widget(canvas, cat1_title, 110, left_y, 0, 0);
    left_y += 35;

    auto* w1 = make_text(tree, "    Axe & Dagger", 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, w1, 110, left_y, 0, 0);
    left_y += 28;
    auto* w2 = make_text(tree, "    Clockwork Scythe", 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, w2, 110, left_y, 0, 0);
    left_y += 28;
    auto* w3 = make_text(tree, "    The Iconoclast (Hadern)", 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, w3, 110, left_y, 0, 0);
    left_y += 45;

    // Category 2: Tarstones (Smart Filtered)
    bool cat2_sel = (focus_area_ == FocusArea::LeftAccordion && accordion_section_ == 1);
    auto* cat2_title = make_text(tree, cat2_sel ? "> TARSTONES (COMPATIBLE)" : "  TARSTONES (COMPATIBLE)", 18.0f, cat2_sel ? ink_accent : ink_title, font_title_obj);
    place_widget(canvas, cat2_title, 110, left_y, 0, 0);
    left_y += 35;

    if (deps_.movesets) {
        auto stones = deps_.movesets->get_tarstones_for_slot(selected_slot_);
        if (stones.empty()) {
            auto* no_stone = make_text(tree, "    (No compatible Tarstone for this slot)", 14.0f, ink_muted, font_serif_obj);
            place_widget(canvas, no_stone, 110, left_y, 0, 0);
            left_y += 28;
        } else {
            for (size_t i = 0; i < stones.size(); ++i) {
                bool is_item_sel = (cat2_sel && accordion_item_ == static_cast<int>(i));
                std::string prefix = is_item_sel ? "  ► " : "    ";
                auto* stone_text = make_text(tree, prefix + stones[i].display_name, 15.0f, is_item_sel ? ink_accent : ink_body, font_serif_obj);
                place_widget(canvas, stone_text, 110, left_y, 0, 0);
                left_y += 28;
            }
        }
    }
    left_y += 30;

    // Category 3: Enemy's Weapon / Movesets
    bool cat3_sel = (focus_area_ == FocusArea::LeftAccordion && accordion_section_ == 2);
    auto* cat3_title = make_text(tree, cat3_sel ? "> ENEMY MOVESETS" : "  ENEMY MOVESETS", 18.0f, cat3_sel ? ink_accent : ink_title, font_title_obj);
    place_widget(canvas, cat3_title, 110, left_y, 0, 0);
    left_y += 35;

    auto* e1 = make_text(tree, "    The Brigands", 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, e1, 110, left_y, 0, 0);
    left_y += 28;
    auto* e2 = make_text(tree, "    Cultists & Spear Ladies", 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, e2, 110, left_y, 0, 0);
    left_y += 28;
    auto* e3 = make_text(tree, "    The Sicario Assassins", 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, e3, 110, left_y, 0, 0);

    // -------------------------------------------------------------
    // 3. Center Area: Moveset 10-Slot Matrix (Concept Screenshot Layout)
    // -------------------------------------------------------------
    double grid_x = 650;
    double grid_y = 360;
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
            std::string inside_text = binding.tarstone_name.empty() ? binding.move_id.substr(0, 6) : "STONE";
            auto* inside = make_text(tree, inside_text, 11.0f, is_selected ? ink_accent : ink_body, font_serif_obj);
            place_widget(canvas, inside, x + 15, y + 40, 0, 0);
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
            std::string inside_text = binding.tarstone_name.empty() ? binding.move_id.substr(0, 6) : "STONE";
            auto* inside = make_text(tree, inside_text, 11.0f, is_selected ? ink_accent : ink_body, font_serif_obj);
            place_widget(canvas, inside, x + 15, y + 40, 0, 0);
        }
    }

    // -------------------------------------------------------------
    // 4. Right Column: Native Inspection Card (Concept Screenshot)
    // -------------------------------------------------------------
    auto* right_card = make_border(tree, ink_dark_bg);
    place_widget(canvas, right_card, 1340, 60, 500, 930);

    double right_y = 90;

    std::string card_title = "STILLBLADE'S STONE";
    std::string card_sub = "LEVEL 3  —  MAXED";
    std::string card_desc = "Grants access to a special Light Combo Finisher.";
    std::vector<std::string> perks = {
        "• The light finisher deals 10 Break Damage",
        "• The light finisher deals 15 Break Damage",
        "• The light finisher deals 25 Break Damage"
    };

    if (deps_.movesets) {
        const auto& binding = deps_.movesets->active_preset().slots[static_cast<size_t>(selected_slot_)];
        if (!binding.tarstone_name.empty()) {
            card_title = binding.tarstone_name;
            const auto* stone = deps_.movesets->find_tarstone(binding.tarstone_id);
            if (stone) card_desc = stone->description;
        } else if (!binding.move_id.empty()) {
            const auto* move = deps_.movesets->find_move(binding.move_id);
            if (move) {
                card_title = move->display_name;
                card_sub = move->category + " (" + move->source_name + ")";
                card_desc = move->description;
                perks = {
                    "• Damage Multiplier: " + std::to_string(move->damage_multiplier).substr(0, 4) + "x",
                    "• Poise Damage: " + std::to_string(static_cast<int>(move->poise_damage)),
                    "• Reaction: " + move->reaction_tag
                };
            }
        }
    }

    auto* rt_title = make_text(tree, card_title, 22.0f, ink_title, font_title_obj);
    place_widget(canvas, rt_title, 1380, right_y, 0, 0);
    right_y += 35;

    auto* rt_sub = make_text(tree, card_sub, 14.0f, ink_muted, font_title_obj);
    place_widget(canvas, rt_sub, 1380, right_y, 0, 0);
    right_y += 60;

    // 3D Preview / Icon Frame placeholder
    auto* preview_box = make_border(tree, Color{0.10f, 0.08f, 0.06f, 1.0f});
    place_widget(canvas, preview_box, 1450, right_y, 280, 280);
    auto* icon_label = make_text(tree, "[ TARSTONE 3D ICON ]", 14.0f, ink_muted, font_title_obj);
    place_widget(canvas, icon_label, 1490, right_y + 130, 0, 0);
    right_y += 320;

    auto* rt_desc = make_text(tree, card_desc, 15.0f, ink_body, font_serif_obj);
    place_widget(canvas, rt_desc, 1380, right_y, 420, 0);
    right_y += 65;

    for (const auto& perk : perks) {
        auto* p_widget = make_text(tree, perk, 14.0f, ink_accent, font_serif_obj);
        place_widget(canvas, p_widget, 1380, right_y, 0, 0);
        right_y += 32;
    }

    right_y += 40;
    auto* prompt1 = make_text(tree, "[X] Unequip Tarstone", 15.0f, ink_body, font_title_obj);
    place_widget(canvas, prompt1, 1420, right_y, 0, 0);
    right_y += 30;
    auto* prompt2 = make_text(tree, "[F] Show Details", 15.0f, ink_body, font_title_obj);
    place_widget(canvas, prompt2, 1420, right_y, 0, 0);

    // -------------------------------------------------------------
    // 5. Footer Bar
    // -------------------------------------------------------------
    auto* footer = make_text(tree, "[A] Select Move/Tarstone    [B] Back    [X] Unequip    [F] Details    [LB/RB] Switch Tab", 15.0f, ink_muted, font_serif_obj);
    place_widget(canvas, footer, 80, 1010, 0, 0);
}

void Menu::render_preset_screen(UObject* canvas, UObject* tree) {
    auto* font_title_obj = load(title_font);
    auto* font_serif_obj = load(serif_font);

    // Top Header & Tabs Bar
    auto* logo_box = make_border(tree, ink_dark_bg);
    place_widget(canvas, logo_box, 80, 60, 480, 100);
    auto* logo_text = make_text(tree, "CUSTOM COMBAT SYSTEM", 22.0f, ink_title, font_title_obj);
    place_widget(canvas, logo_text, 100, 95, 0, 0);

    std::string tabs_text = "[Z]   CUSTOMIZE      < PRESET >      SETTINGS   [X]";
    auto* tabs_widget = make_text(tree, tabs_text, 18.0f, ink_body, font_title_obj);
    place_widget(canvas, tabs_widget, 620, 95, 0, 0);

    // Presets List Panel
    auto* list_panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, list_panel, 80, 190, 800, 780);

    auto* header = make_text(tree, "COMMUNITY PRESETS (ue4ss/Mods/CCS/presets/*.json)", 18.0f, ink_title, font_title_obj);
    place_widget(canvas, header, 110, 220, 0, 0);

    double py = 280;
    if (cached_preset_names_.empty()) {
        auto* empty_text = make_text(tree, "No presets found in ue4ss/Mods/CCS/presets/", 16.0f, ink_muted, font_serif_obj);
        place_widget(canvas, empty_text, 110, py, 0, 0);
    } else {
        for (size_t i = 0; i < cached_preset_names_.size(); ++i) {
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
        auto* base_w = make_text(tree, "Base Weapon: " + p.base_weapon, 16.0f, ink_body, font_serif_obj);
        place_widget(canvas, base_w, 950, iy, 0, 0);
        iy += 45;
        auto* desc_w = make_text(tree, "Description: " + p.description, 16.0f, ink_muted, font_serif_obj);
        place_widget(canvas, desc_w, 950, iy, 800, 0);
    }

    // Footer Prompts
    auto* footer = make_text(tree, "[A] Load Preset    [Y] Save Current Build    [X] Delete Preset    [LB/RB] Switch Tab", 15.0f, ink_muted, font_serif_obj);
    place_widget(canvas, footer, 80, 1010, 0, 0);
}

void Menu::render_settings_screen(UObject* canvas, UObject* tree) {
    auto* font_title_obj = load(title_font);
    auto* font_serif_obj = load(serif_font);

    // Top Header & Tabs Bar
    auto* logo_box = make_border(tree, ink_dark_bg);
    place_widget(canvas, logo_box, 80, 60, 480, 100);
    auto* logo_text = make_text(tree, "CUSTOM COMBAT SYSTEM", 22.0f, ink_title, font_title_obj);
    place_widget(canvas, logo_text, 100, 95, 0, 0);

    std::string tabs_text = "[Z]   CUSTOMIZE      PRESET      < SETTINGS >   [X]";
    auto* tabs_widget = make_text(tree, tabs_text, 18.0f, ink_body, font_title_obj);
    place_widget(canvas, tabs_widget, 620, 95, 0, 0);

    // Settings Panel
    auto* panel = make_border(tree, ink_dark_bg);
    place_widget(canvas, panel, 80, 190, 1760, 780);

    double sy = 230;

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
            master_enabled ? "[ < ENABLED > ]" : "[ < DISABLED > ]",
            "Master toggle. When disabled, stock movesets are restored with 0ms overhead.",
            master_enabled ? ink_green : ink_accent
        },
        {
            "Preserve Weapon Mesh on Enemy Moves",
            mesh_preserve ? "[ < ON > ]" : "[ < OFF > ]",
            "Keeps player weapon visible and attached during enemy attack animations.",
            ink_body
        },
        {
            "Show Combat Move HUD Notification",
            hud_notify ? "[ < ON > ]" : "[ < OFF > ]",
            "Briefly displays the custom move or Tarstone name when executed.",
            ink_body
        },
        {
            "Reset All Slots to Vanilla Defaults",
            "[ PRESS [A] TO RESET ]",
            "Restores all 10 combo slots to stock game defaults.",
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
    auto* footer = make_text(tree, "[A] Toggle / Apply    [Up/Down] Navigate    [LB/RB] Switch Tab", 15.0f, ink_muted, font_serif_obj);
    place_widget(canvas, footer, 80, 1010, 0, 0);
}

} // namespace ccs

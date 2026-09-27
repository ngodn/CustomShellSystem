// The CCS page built from the game's own widget blueprints: the skeleton, the widget pool, the
// page, the inline search picker and the confirmation dialog. The CSS/CSSX recipe, with no
// dependency on either being installed.
#include "menu.hpp"
#include "menu_keys.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
using namespace engine;
namespace {
constexpr const char* row_class = "/Game/Sparta/UI/Menu/LandingArea/WBP_NB_SkinOption.WBP_NB_SkinOption_C";
constexpr const char* header_class = "/Game/Sparta/UI/Menu/LandingArea/WBP_Skin_Category.WBP_Skin_Category_C";
constexpr const char* option_class = "/Game/Sparta/UI/Menu/WBP_NB_Option.WBP_NB_Option_C";
constexpr const char* slider_class = "/Game/Sparta/UI/Menu/WBP_NB_Option_Slider.WBP_NB_Option_Slider_C";
constexpr const char* divider_class = "/Game/Sparta/UI/Menu/Options/WBP_SettingDivider.WBP_SettingDivider_C";
constexpr const char* tab_class = "/Game/Sparta/UI/Menu/WBP_NB_Menu.WBP_NB_Menu_C";
constexpr const char* prompt_class = "/Game/Sparta/UI/Core/Navigation/WBP_Prompt.WBP_Prompt_C";
constexpr const char* details_class = "/Game/Sparta/UI/Menu/Equipment/WBP_Equipment_Description.WBP_Equipment_Description_C";
constexpr const char* dialog_class = "/Game/Sparta/UI/Menu/Misc/WBP_ConfirmationPrompt_Default.WBP_ConfirmationPrompt_Default_C";
constexpr const char* listener_class = "/Game/Sparta/UI/Core/Navigation/WBP_InputListener.WBP_InputListener_C";
constexpr const char* fade_class = "/Game/Sparta/UI/Core/WBP_ScrollBoxFadeHandler.WBP_ScrollBoxFadeHandler_C";
constexpr const char* serif_font = "/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font";
constexpr const char* title_font = "/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font";
constexpr double native_height = 2160, native_column = 1136;
constexpr double window_x = 95, window_top = 150, window_w = 945;
constexpr uint8_t collapsed = 1, hidden = 2, shown_passive = 3, shown_self_passive = 4;
constexpr uint8_t glyph_accept = 3, glyph_secondary = 4, glyph_back = 5, glyph_left_bumper = 8, glyph_right_bumper = 9, glyph_dpad_vertical = 10, glyph_dpad_horizontal = 11, glyph_up = 13, glyph_down = 14, glyph_left = 15, glyph_right = 16, glyph_none = 45;
constexpr Color body{.49f, .42f, .30f, 1};
constexpr Color muted{.24f, .21f, .17f, 1};
constexpr Color title_ink{.223f, .186f, .133f, 1};
constexpr Color subtitle_ink{.223f, .197f, .176f, 1};
constexpr Color danger{.72f, .28f, .22f, 1}, warning{.78f, .58f, .22f, 1};

uint64_t monotonic_us() { return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
std::string path_utf8(const fs::path& path) { auto text = path.u8string(); return std::string(text.begin(), text.end()); }
UClass* game_class(const char* path) { return static_cast<UClass*>(load(path)); }
UObject* game_texture(const char* name) { std::string path = "/Game/Sparta/UI/Common/Textures/"; path += name; path += "."; path += name; return load(path); }
UObject* part(UObject* widget, const wchar_t* name) {
    auto* found = object_of(widget, name);
    if (!found) throw std::runtime_error("Native widget part is missing: " + narrow(name));
    return found;
}
UObject* nav_button(UObject* widget, const wchar_t* member) { return part(part(widget, member), L"ButtonWidget"); }
UObject* add_child(UObject* panel, UObject* child) { Call add(panel, L"AddChild", 2); add.set(L"content", child); add.run(); return add.get<UObject*>(); }
UObject* place(UObject* canvas, UObject* child, double x, double y, double w = 0, double h = 0) {
    Call add(canvas, L"AddChildToCanvas", 2); add.set(L"content", child); add.run();
    auto* slot = add.get<UObject*>();
    invoke(slot, L"SetPosition", L"InPosition", Vec2{x, y});
    if (w > 0) invoke(slot, L"SetSize", L"InSize", Vec2{w, h});
    else invoke(slot, L"SetAutoSize", L"InbAutoSize", true);
    return slot;
}
void padding(UObject* slot, Margin value) { if (slot) invoke(slot, L"SetPadding", L"InPadding", value); }
UObject* fill(UObject* slot) {
    invoke(slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{0});
    invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{0});
    return slot;
}
void visibility(UObject* widget, uint8_t value) { invoke(widget, L"SetVisibility", L"InVisibility", value); }
void brush(UObject* image, UObject* texture, bool match = false) {
    Call set(image, L"SetBrushFromTexture", 2); set.set(L"Texture", texture); set.set(L"bMatchSize", match); set.run();
}
UObject* image_widget(UObject* tree, UObject* texture, Color tint = {1, 1, 1, 1}) {
    auto* image = construct(L"/Script/UMG.Image", tree);
    if (texture) brush(image, texture);
    invoke(image, L"SetColorAndOpacity", L"InColorAndOpacity", tint);
    visibility(image, shown_passive);
    return image;
}
UObject* text_block(UObject* tree, float size, UObject* font, Color color, bool wrap) {
    auto* block = construct(L"/Script/UMG.TextBlock", tree);
    font_size(block, size, font);
    invoke(block, L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{color});
    invoke(block, L"SetAutoWrapText", L"InAutoTextWrap", wrap);
    if (!wrap) { invoke(block, L"SetTextOverflowPolicy", L"InOverflowPolicy", uint8_t{1}); invoke(block, L"SetClipping", L"InClipping", uint8_t{1}); }
    visibility(block, shown_passive);
    return block;
}
UObject* flat(UObject* tree) {
    auto* button = construct(L"/Script/UMG.Button", tree);
    flat_button(button, false, true);
    auto* focusable = button->GetPropertyByNameInChain(L"IsFocusable");
    if (!focusable || !focusable->IsA<FBoolProperty>()) throw std::runtime_error("Button focus property mismatch");
    static_cast<FBoolProperty*>(focusable)->SetPropertyValueInContainer(button, false);
    return button;
}
void call_text(UObject* widget, const wchar_t* function, const wchar_t* param, const std::string& value) {
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_StringToText", 2);
    convert.set(L"InString", FString(wide(value).c_str())); convert.run();
    Call set(widget, function, 1); set.copy(param, convert, L"ReturnValue"); set.run();
}
std::string effect_label(const Json& c) {
    const auto effect = c.value("effect", std::string{});
    if (effect == "irreversible") return "Irreversible: this changes your save";
    if (effect == "persistent") return "Persists on disk";
    if (effect == "reversible") return "Reversible while CCS owns it";
    return {};
}
std::string state_word(const Json& c) {
    if (c.value("busy", false)) return "Working...";
    if (!c.value("enabled", true)) return c.value("disabled_label", std::string("Unavailable"));
    const auto type = c.at("type").get<std::string>();
    if (type == "button" || type == "label") return c.value("state", std::string{});
    auto value = display_value(c);
    if (!value.empty() && c.contains("unit") && (type == "number" || type == "slider")) value += c.value("unit", std::string{});
    return value;
}
}

// ---- textures
void Menu::root(UObject* object) {
    if (!object || object->IsRootSet()) return;
    object->SetRootSet(); rooted_.push_back(WeakObject(object));
}
void Menu::unroot_all() {
    for (auto& weak : rooted_) if (auto* object = weak.Get()) { try { object->ClearRootSet(); } catch (...) {} }
    rooted_.clear();
}
UObject* Menu::texture_at(const fs::path& file) {
    if (file.empty()) return nullptr;
    const auto key = path_utf8(file);
    auto& saved = textures_[key];
    if (auto* object = saved.object.Get()) return object;
    if (saved.missing) return nullptr;
    std::error_code ec;
    if (!fs::exists(file, ec)) { saved.missing = true; return nullptr; }
    auto* pc = pc_.Get(); if (!pc) return nullptr;
    Call import(find_cached(L"/Script/Engine.Default__KismetRenderingLibrary"), L"ImportFileAsTexture2D", 3);
    import.set(L"WorldContextObject", pc); import.set(L"Filename", FString(file.c_str())); import.run();
    auto* object = import.get<UObject*>();
    if (!object) { saved.missing = true; return nullptr; }
    saved.object = object; root(object);
    return object;
}

// A texture from the game's own paks (the weapon icons), loaded once and rooted so a reopen
// never has to load it again.
UObject* Menu::game_icon(const std::string& path) {
    if (path.empty()) return nullptr;
    auto& saved = textures_[path];
    if (auto* object = saved.object.Get()) return object;
    if (saved.missing) return nullptr;
    try { auto* object = load(path); saved.object = object; root(object); return object; }
    catch (...) { saved.missing = true; return nullptr; }
}

// ---- item state
void Menu::show(Item& item, bool on) {
    if (item.shown == int(on)) return;
    if (auto* widget = item.widget.Get()) visibility(widget, on ? shown_self_passive : collapsed);
    item.shown = on;
}
void Menu::text(UObject* block, std::string& shown, const std::string& value) {
    if (!block || shown == value) return;
    text_value(block, value); shown = value;
}
void Menu::state(Item& item, bool selected) {
    if (item.selected == int(selected)) return;
    if (auto* widget = item.widget.Get()) invoke(widget, selected ? L"OnSelectedState" : L"OnNullState");
    item.selected = selected;
}
void Menu::glyph(UObject* widget, const std::string& action, uint8_t fallback, uint8_t keyboard) {
    bool bound = false;
    for (const auto& b : bindings_) if (b.action == action) {
        bound = true;
        object_property(widget, L"InputAction", b.input_action.Get());
        for (const auto& key : b.keys) if (!key.starts_with("Gamepad_")) {
            if (const auto icon = keyboard_icon(key); icon != 255) raw_value(widget, L"KBMPrompt", icon);
            break;
        }
        break;
    }
    if (!bound) object_property(widget, L"InputAction", nullptr);
    if (keyboard != 255) raw_value(widget, L"KBMPrompt", keyboard);
    raw_value(widget, L"ControllerPrompt", fallback);
    raw_value(widget, L"PromptSize", Vec2{80, 80});
    raw_value(widget, L"OverrideControllerSize", Vec2{80, 80});
    raw_value(widget, L"OverrideKBMSize", Vec2{80, 80});
    invoke(widget, L"UpdatePrompt"); invoke(widget, L"UpdatePromptSize");
}

// ---- the skeleton
void Menu::forget_page() {
    for (auto* stack : {&tab_items_, &list_, &head_, &panel_, &actions_, &footer_}) { stack->cells.clear(); stack->used = 0; stack->box.Reset(); }
    design_.Reset(); left_root_.Reset(); right_root_.Reset(); list_scroll_.Reset(); strip_scroll_.Reset(); details_.Reset(); panel_scroll_.Reset(); panel_size_.Reset();
    status_text_.Reset(); title_text_.Reset(); subtitle_text_.Reset(); logo_image_.Reset(); strip_previous_.Reset(); strip_next_.Reset(); strip_previous_glyph_.Reset(); strip_next_glyph_.Reset();
    input_prompt_.Reset(); search_input_.Reset(); name_input_.Reset();
    design_w_ = design_h_ = design_scale_ = 0; shown_section_ = revealed_row_ = -1;
    panel_context_.clear(); panel_revealed_ = nullptr; panel_fit_pending_ = false; panel_max_ = 720.f; pending_reveals_ = {};
    detail_title_.clear(); detail_sub_.clear(); detail_body_ = "\x01"; status_shown_.clear(); title_shown_.clear(); subtitle_shown_.clear();
    status_error_shown_ = -1; logo_shown_ = -1; transition_started_ = 0; detail_icon_ = reinterpret_cast<const void*>(1);
}
bool Menu::page(double width, double height) {
    const double user = deps_.ui_scale ? std::clamp(deps_.ui_scale(), 0.75, 1.5) : 1.0;
    const double scale = height / native_height * user, design_width = width / scale, design_height = native_height / user;
    if (design_.Get() && std::abs(design_width - design_w_) < 1 && std::abs(scale - design_scale_) < 1e-4) return true;
    auto* canvas = canvas_.Get(); auto* page_widget = page_.Get(); auto* pc = pc_.Get(); auto* tree = tree_.Get();
    if (!canvas || !page_widget || !pc || !tree) return false;
    forget_page();
    invoke(canvas, L"ClearChildren");
    auto* serif = load(serif_font); auto* trajan = load(title_font);
    auto* scaler = construct(L"/Script/UMG.ScaleBox", tree);
    invoke(scaler, L"SetStretch", L"InStretch", uint8_t{7});
    invoke(scaler, L"SetUserSpecifiedScale", L"InUserSpecifiedScale", float(scale));
    place(canvas, scaler, 0, 0, width, height);
    auto* frame = construct(L"/Script/UMG.SizeBox", tree);
    invoke(frame, L"SetWidthOverride", L"InWidthOverride", float(design_width));
    invoke(frame, L"SetHeightOverride", L"InHeightOverride", float(design_height));
    content(scaler, frame);
    auto* design = construct(L"/Script/UMG.CanvasPanel", tree); content(frame, design);
    design_ = design; design_w_ = design_width; design_h_ = design_height; design_scale_ = scale;
    auto* character = object_of(main_.Get(), L"WBP_MGT_Character");
    auto faded = [&](UObject* holder, UObject* scroll, const wchar_t* source, UObject* host) {
        auto* retainer = construct(L"/Script/UMG.RetainerBox", tree);
        content(holder, retainer); content(retainer, scroll);
        auto* native = object_of(character, source);
        if (!native) return;
        auto* handler = create_widget(pc, game_class(fade_class));
        place(host, handler, 0, 0, 1, 1);
        for (auto name : {L"FadeMaterial", L"EdgeStart", L"EdgeEnd"}) copy_property(handler, native, name);
        Call init(handler, L"Initialize", 2); init.set(L"RetainerBox", retainer); init.set(L"ScrollBox", scroll); init.run();
        invoke(handler, L"SetEnabledState", L"Enabled", true);
    };
    auto column = [&](double x, double w) {
        auto* root_widget = construct(L"/Script/UMG.CanvasPanel", tree);
        place(design, root_widget, x, 0, w, design_height);
        return root_widget;
    };
    auto* left = column(0, native_column); auto* right = column(design_width - native_column, native_column);
    left_root_ = left; right_root_ = right;
    auto* logo = image_widget(tree, nullptr); place(left, logo, 40, 100, 260, 260); logo_image_ = logo; visibility(logo, collapsed); logo_shown_ = 0;
    auto* title = text_block(tree, 50, trajan, title_ink, false); place(left, title, 330, 171, 780, 70); title_text_ = title;
    auto* subtitle = text_block(tree, 30, trajan, subtitle_ink, false); place(left, subtitle, 332, 243, 780, 48); subtitle_text_ = subtitle;
    auto* strip_frame = construct(L"/Script/UMG.Overlay", tree);
    place(left, strip_frame, 40, 390, 1056, 150);
    fill(add_child(strip_frame, image_widget(tree, game_texture("T_UI_Nav_TitleBG"))));
    auto* strip = construct(L"/Script/UMG.HorizontalBox", tree);
    auto* strip_slot = add_child(strip_frame, strip);
    invoke(strip_slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{2});
    invoke(strip_slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
    auto prompt = [&](const std::string& action, uint8_t fallback, WeakObject& hit, WeakObject& shown) {
        auto* button = flat(tree);
        auto* slot = add_child(strip, button);
        invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
        auto* widget = create_widget(pc, game_class(prompt_class));
        content(button, widget);
        glyph(widget, action, fallback);
        visibility(widget, shown_passive);
        hit = button; shown = widget;
        return widget;
    };
    input_prompt_ = prompt("previous_section", glyph_left_bumper, strip_previous_, strip_previous_glyph_);
    auto* clip = construct(L"/Script/UMG.SizeBox", tree);
    invoke(clip, L"SetWidthOverride", L"InWidthOverride", 780.f); invoke(clip, L"SetHeightOverride", L"InHeightOverride", 80.f);
    auto* clip_slot = add_child(strip, clip);
    invoke(clip_slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
    padding(clip_slot, Margin{18, 0, 18, 0});
    auto* strip_scroll = construct(L"/Script/UMG.ScrollBox", tree);
    invoke(strip_scroll, L"SetOrientation", L"NewOrientation", uint8_t{0});
    invoke(strip_scroll, L"SetScrollBarVisibility", L"NewScrollBarVisibility", uint8_t{1});
    faded(clip, strip_scroll, L"WBP_SBFH_InventoryFilter", left); strip_scroll_ = strip_scroll;
    auto* tabs = construct(L"/Script/UMG.HorizontalBox", tree);
    add_child(strip_scroll, tabs); tab_items_.box = tabs;
    prompt("next_section", glyph_right_bumper, strip_next_, strip_next_glyph_);
    place(left, image_widget(tree, game_texture("T_UI_Nav_Title_Divider")), 0, 560, native_column, 6);
    auto* list_size = construct(L"/Script/UMG.SizeBox", tree);
    invoke(list_size, L"SetWidthOverride", L"InWidthOverride", 1040.f);
    invoke(list_size, L"SetHeightOverride", L"InHeightOverride", float(design_height - 780));
    place(left, list_size, 48, 590);
    auto* list_scroll = construct(L"/Script/UMG.ScrollBox", tree);
    if (auto* bar = object_of(object_of(character, L"WBP_CSB_Style2"), L"Image_Bar")) {
        auto* style = list_scroll->GetPropertyByNameInChain(L"WidgetBarStyle");
        auto* bar_brush = bar->GetPropertyByNameInChain(L"Brush");
        if (style && style->IsA<FStructProperty>() && bar_brush) {
            auto* info = find_cached(L"/Script/SlateCore.ScrollBarStyle");
            for (auto name : {L"NormalThumbImage", L"HoveredThumbImage", L"DraggedThumbImage"}) {
                auto* target = info->GetPropertyByNameInChain(name);
                if (target && target->SameType(bar_brush)) target->CopyCompleteValue(reinterpret_cast<std::byte*>(list_scroll) + style->GetOffset_Internal() + target->GetOffset_Internal(), reinterpret_cast<std::byte*>(bar) + bar_brush->GetOffset_Internal());
            }
        }
    }
    invoke(list_scroll, L"SetAllowOverscroll", L"NewAllowOverscroll", false);
    invoke(list_scroll, L"SetAnimateWheelScrolling", L"bShouldAnimateWheelScrolling", true);
    invoke(list_scroll, L"SetScrollbarThickness", L"NewScrollbarThickness", Vec2{8, 8});
    faded(list_size, list_scroll, L"WBP_SBFH_Inventory", left); list_scroll_ = list_scroll;
    auto* list = construct(L"/Script/UMG.VerticalBox", tree);
    add_child(list_scroll, list); list_.box = list;
    auto* footer = construct(L"/Script/UMG.HorizontalBox", tree);
    place(left, footer, 56, design_height - 140); footer_.box = footer;
    auto* details = create_widget(pc, game_class(details_class));
    place(right, details, window_x, window_top);
    details_ = details;
    invoke(details, L"Show");
    invoke(part(details, L"SizeBox_Main"), L"SetWidthOverride", L"InWidthOverride", float(window_w));
    invoke(details, L"CollapseDetails");
    for (auto name : {L"VB_AbilityList", L"VB_EffectList", L"VB_TooltipList", L"VB_CustomWidgets", L"VB_DynamicPrompts"})
        if (auto* box = object_of(details, name)) invoke(box, L"ClearChildren");
    if (auto* sample = object_of(details, L"DetailsPrompt")) visibility(sample, collapsed);
    visibility(part(details, L"Size_SubHeader"), collapsed);
    for (auto name : {L"MyIcon", L"WidgetSwitcher_IconBG", L"Overlay_Icon", L"Spacer_83"}) if (auto* w = object_of(details, name)) visibility(w, collapsed);
    auto* head = construct(L"/Script/UMG.VerticalBox", tree);
    add_child(part(details, L"VB_CustomWidgets"), head);
    head_.box = head;
    auto* panel_size = construct(L"/Script/UMG.SizeBox", tree);
    invoke(panel_size, L"SetMaxDesiredHeight", L"InMaxDesiredHeight", panel_max_);
    invoke(panel_size, L"SetMinDesiredWidth", L"InMinDesiredWidth", float(window_w - 40));
    add_child(part(details, L"VB_CustomWidgets"), panel_size);
    panel_size_ = panel_size;
    auto* panel_scroll = construct(L"/Script/UMG.ScrollBox", tree);
    invoke(panel_scroll, L"SetScrollBarVisibility", L"NewScrollBarVisibility", uint8_t{1});
    invoke(panel_scroll, L"SetAllowOverscroll", L"NewAllowOverscroll", false);
    invoke(panel_scroll, L"SetAnimateWheelScrolling", L"bShouldAnimateWheelScrolling", true);
    faded(panel_size, panel_scroll, L"WBP_SBFH_Inventory", right); panel_scroll_ = panel_scroll;
    auto* panel = construct(L"/Script/UMG.VerticalBox", tree);
    add_child(panel_scroll, panel); panel_.box = panel;
    auto* actions = construct(L"/Script/UMG.VerticalBox", tree);
    padding(add_child(part(details, L"VB_DynamicPrompts"), actions), Margin{0, 10, 0, 10});
    actions_.box = actions;
    auto* status = text_block(tree, 30, serif, muted, true);
    place(right, status, window_x, design_height - 170, window_w, 110);
    status_text_ = status;
    return true;
}
void Menu::invalidate_page() {
    for (auto* stack : {&tab_items_, &list_, &head_, &panel_, &actions_, &footer_})
        for (auto& cell : stack->cells) {
            cell.shown = -1;
            for (auto& item : cell.kinds) {
                item.text.clear(); item.value.clear(); item.glyph.clear();
                item.selected = item.badge = item.shown = item.enabled = item.icon_shown = item.arrows = -1;
                item.icon = nullptr; item.fill = -2.f; item.color = {-1, -1, -1, -1};
                try { setup(item); } catch (...) {}
            }
        }
    detail_title_.clear(); detail_sub_.clear(); detail_body_ = "\x01"; status_shown_.clear(); title_shown_.clear(); subtitle_shown_.clear();
    status_error_shown_ = -1; logo_shown_ = -1; detail_icon_ = reinterpret_cast<const void*>(1);
    if (auto* details = details_.Get()) {
        try {
            invoke(details, L"Show");
            invoke(part(details, L"SizeBox_Main"), L"SetWidthOverride", L"InWidthOverride", float(window_w));
            invoke(details, L"CollapseDetails");
            if (auto* sample = object_of(details, L"DetailsPrompt")) visibility(sample, collapsed);
            for (auto name : {L"MyIcon", L"WidgetSwitcher_IconBG", L"Overlay_Icon", L"Spacer_83"}) if (auto* w = object_of(details, name)) visibility(w, collapsed);
        } catch (...) {}
    }
    panel_context_.clear(); panel_revealed_ = nullptr; revealed_row_ = -1; shown_section_ = -1;
    panel_fit_pending_ = true;
}

// ---- the pool
bool Menu::ready(const Stack& stack, Kind kind) const {
    if (stack.used < stack.cells.size())
        for (const auto& item : stack.cells[stack.used].kinds) if (item.kind == kind && item.widget.Get()) return true;
    return budget_ > 0;
}
Menu::Item& Menu::take(Stack& stack, Kind kind) {
    auto* column = stack.box.Get();
    if (!column) throw std::runtime_error("Page container is unavailable");
    auto* tree = tree_.Get(); auto* pc = pc_.Get();
    if (!tree || !pc) throw std::runtime_error("Page tree is unavailable");
    if (stack.used == stack.cells.size()) {
        auto* holder = construct(L"/Script/UMG.Overlay", tree);
        add_child(column, holder);
        visibility(holder, shown_self_passive);
        stack.cells.push_back({WeakObject(holder), {}, 1});
    }
    auto& cell = stack.cells[stack.used++];
    auto* box = cell.holder.Get();
    if (!box) throw std::runtime_error("Page cell is unavailable");
    if (cell.shown != 1) { visibility(box, shown_self_passive); cell.shown = 1; }
    Item* wanted = nullptr;
    for (auto& item : cell.kinds) if (item.kind == kind && item.widget.Get()) wanted = &item;
    for (auto& item : cell.kinds) if (&item != wanted) show(item, false);
    if (wanted) { show(*wanted, true); return *wanted; }
    auto* serif = load(serif_font);
    Item item; item.kind = kind; item.shown = 1;
    --budget_; ++cost_.created;
    UObject* widget = nullptr; UObject* slot = nullptr;
    switch (kind) {
    case Kind::row:
        widget = create_widget(pc, game_class(row_class)); slot = add_child(box, widget);
        item.text_block = part(widget, L"Button_Text");
        item.hit = nav_button(widget, L"MyNavigationButton");
        break;
    case Kind::header:
        widget = create_widget(pc, game_class(header_class)); slot = add_child(box, widget);
        item.text_block = part(widget, L"Text_Category");
        break;
    case Kind::option: case Kind::slider: {
        const bool slider = kind == Kind::slider;
        widget = create_widget(pc, game_class(slider ? slider_class : option_class)); slot = add_child(box, widget);
        item.text_block = part(widget, L"Text_Option");
        item.value_block = part(widget, L"Text_Option_Value");
        item.hit = nav_button(widget, L"WBP_NavButton_Main");
        if (slider) {
            item.hit_left = part(part(widget, L"WBP_ArrowButton_L"), L"ArrowButton");
            item.hit_right = part(part(widget, L"WBP_ArrowButton_R"), L"ArrowButton");
            item.extra = part(widget, L"WBP_GenericBar");
        } else {
            item.hit_left = part(widget, L"Button_Left");
            item.hit_right = part(widget, L"Button_Right");
        }
        break;
    }
    case Kind::divider:
        widget = create_widget(pc, game_class(divider_class)); slot = add_child(box, widget);
        item.text_block = part(widget, L"Text_DividerName");
        break;
    case Kind::tab: {
        widget = create_widget(pc, game_class(tab_class)); slot = add_child(box, widget);
        auto* filter = object_of(object_of(main_.Get(), L"WBP_MGT_Character"), L"BP_HBC_InventoryFilter");
        auto siblings = filter ? children(filter, 64) : std::vector<UObject*>{};
        if (!siblings.empty() && siblings.front()) {
            for (auto name : {L"FontData", L"SelectedColor", L"DefaultColor", L"HighlightY", L"bUseHighlight"}) copy_property(widget, siblings.front(), name);
            if (auto* native_slot = object_of(siblings.front(), L"Slot")) padding(slot, read<Margin>(native_slot, L"Padding"));
        } else padding(slot, Margin{25, 0, 25, 0});
        item.hit = nav_button(widget, L"WBP_NavigationButton");
        break;
    }
    case Kind::action: {
        widget = flat(tree); slot = add_child(box, widget);
        auto* line = construct(L"/Script/UMG.HorizontalBox", tree); content(widget, line);
        UObject* glyphs[2]{};
        for (auto*& g : glyphs) {
            g = create_widget(pc, game_class(prompt_class));
            auto* glyph_slot = add_child(line, g);
            invoke(glyph_slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
            padding(glyph_slot, Margin{0, 0, 4, 0});
            visibility(g, shown_passive);
        }
        visibility(glyphs[1], collapsed);
        auto* label = text_block(tree, 32, serif, body, false);
        auto* label_slot = add_child(line, label);
        invoke(label_slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
        padding(label_slot, Margin{14, 0, 0, 0});
        item.hit = widget; item.extra = glyphs[0]; item.extra2 = glyphs[1]; item.text_block = label;
        break;
    }
    case Kind::input: {
        widget = construct(L"/Script/UMG.Overlay", tree); slot = add_child(box, widget);
        fill(add_child(widget, image_widget(tree, game_texture("T_UI_Resource_BG_02"))));
        auto* input = construct(L"/Script/UMG.EditableText", tree);
        Call current(input, L"GetFont", 1); current.run();
        Call set(input, L"SetFont", 1); set.copy(L"InFontInfo", current, L"ReturnValue");
        auto* font = set.param(L"InFontInfo"); auto* info = find_cached(L"/Script/SlateCore.SlateFontInfo");
        member(set.data(font), font->GetElementSize(), info, L"FontObject", serif);
        member(set.data(font), font->GetElementSize(), info, L"Size", 34.f);
        member(set.data(font), font->GetElementSize(), info, L"TypefaceFontName", FName(L"Regular")); set.run();
        padding(fill(add_child(widget, input)), Margin{28, 16, 28, 16});
        item.extra = input;
        break;
    }
    case Kind::paragraph: {
        widget = text_block(tree, 30, serif, muted, true); slot = add_child(box, widget);
        item.text_block = widget;
        break;
    }
    case Kind::picture: {
        widget = construct(L"/Script/UMG.SizeBox", tree); slot = add_child(box, widget);
        invoke(widget, L"SetWidthOverride", L"InWidthOverride", float(window_w - 80));
        invoke(widget, L"SetHeightOverride", L"InHeightOverride", 360.f);
        auto* fitter = construct(L"/Script/UMG.ScaleBox", tree);
        invoke(fitter, L"SetStretch", L"InStretch", uint8_t{2});
        content(widget, fitter);
        auto* image = image_widget(tree, nullptr);
        content(fitter, image);
        item.extra = image;
        break;
    }
    case Kind::none: throw std::runtime_error("Page item without a kind");
    }
    item.widget = widget;
    slot_padding(kind, slot);
    setup(item);
    cell.kinds.push_back(std::move(item));
    return cell.kinds.back();
}
void Menu::setup(Item& item) {
    auto* widget = item.widget.Get();
    if (!widget) return;
    switch (item.kind) {
    case Kind::row: {
        auto* name = part(widget, L"SizeBox_Name");
        invoke(name, L"SetWidthOverride", L"InWidthOverride", 760.f);
        invoke(object_of(name, L"Slot"), L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{1});
        padding(object_of(name, L"Slot"), Margin{15, 0, 0, 52});
        invoke(item.text_block.Get(), L"SetTextOverflowPolicy", L"InOverflowPolicy", uint8_t{1});
        invoke(item.text_block.Get(), L"SetJustification", L"InJustification", uint8_t{0});
        visibility(part(widget, L"SizeBox_Icon"), collapsed);
        break;
    }
    case Kind::option: case Kind::slider:
        if (auto* name = object_of(widget, L"SizeBox_OptionName")) invoke(name, L"SetWidthOverride", L"InWidthOverride", 300.f);
        if (item.kind == Kind::slider) {
            if (auto* arrows = object_of(widget, L"SizeBox_SliderAndArrows")) invoke(arrows, L"SetWidthOverride", L"InWidthOverride", 430.f);
        } else {
            invoke(part(widget, L"SizeBox_OptionValueAndArrows"), L"SetWidthOverride", L"InWidthOverride", 560.f);
            invoke(part(widget, L"SizeBox_OptionValue"), L"SetWidthOverride", L"InWidthOverride", 440.f);
            invoke(part(widget, L"Spacer_57"), L"SetSize", L"InSize", Vec2{24, 0});
        }
        break;
    default: break;
    }
}
void Menu::slot_padding(Kind kind, UObject* slot) {
    fill(slot);
    switch (kind) {
    case Kind::divider: padding(slot, Margin{30, 24, 30, 6}); break;
    case Kind::action: padding(slot, Margin{40, 4, 20, 4}); break;
    case Kind::input: padding(slot, Margin{40, 12, 40, 12}); break;
    case Kind::paragraph: padding(slot, Margin{40, 10, 40, 10}); break;
    case Kind::picture: padding(slot, Margin{40, 16, 40, 8}); break;
    default: break;
    }
}
void Menu::finish(Stack& stack) {
    for (size_t i = stack.used; i < stack.cells.size(); ++i) {
        auto& cell = stack.cells[i];
        if (cell.shown == 0) continue;
        if (auto* holder = cell.holder.Get()) visibility(holder, collapsed);
        cell.shown = 0;
    }
}

// ---- shared fillers
void Menu::bind(const WeakObject& widget, Json action, const std::map<UObject*, bool>& held) {
    auto* w = widget.Get(); if (!w || action.is_null()) return;
    auto it = held.find(w);
    hits_.push_back({widget, std::move(action), it != held.end() && it->second, {}, {}});
}
void Menu::fill_row(Item& item, const std::string& title, const RowLook& look, bool selected) {
    auto* widget = item.widget.Get();
    text(item.text_block.Get(), item.text, title);
    if (item.badge != int(look.badge)) { visibility(part(widget, L"O_Equipped"), look.badge ? shown_self_passive : hidden); item.badge = look.badge; }
    const int mode = look.icon ? 1 : 0;
    if (item.icon_shown != mode) {
        auto* box = part(widget, L"SizeBox_Icon");
        visibility(box, mode ? shown_self_passive : collapsed);
        if (mode) { invoke(box, L"SetWidthOverride", L"InWidthOverride", 110.f); invoke(box, L"SetHeightOverride", L"InHeightOverride", 110.f); }
        visibility(part(widget, L"Image_Icon"), shown_self_passive);
        item.icon_shown = mode;
    }
    if (mode && item.icon != look.icon) { brush(part(widget, L"Image_Icon"), look.icon); item.icon = look.icon; }
    if (item.enabled != int(look.enabled)) { invoke(widget, L"SetRenderOpacity", L"InOpacity", look.enabled ? 1.f : .45f); item.enabled = look.enabled; }
    if (item.value != look.value) {
        auto* block = item.value_block.Get();
        if (!block && !look.value.empty()) {
            block = text_block(tree_.Get(), 28, load(serif_font), muted, false);
            auto* slot = add_child(part(widget, L"Overlay_Main"), block);
            invoke(slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{1});
            invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{3});
            padding(slot, Margin{15, 0, 0, 4});
            item.value_block = block;
        }
        if (block) {
            if (!look.value.empty()) text_value(block, look.value);
            visibility(block, look.value.empty() ? collapsed : shown_passive);
        }
        item.value = look.value;
    }
    state(item, selected);
}
void Menu::paragraph(Stack& stack, const std::string& value, Color color) {
    auto& item = take(stack, Kind::paragraph);
    text(item.text_block.Get(), item.text, value);
    const std::array<float, 4> wanted{color.r, color.g, color.b, color.a};
    if (item.color != wanted) { invoke(item.text_block.Get(), L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{color}); item.color = wanted; }
}
void Menu::header(const std::string& title, const std::string& subtitle) {
    text(title_text_.Get(), title_shown_, title);
    text(subtitle_text_.Get(), subtitle_shown_, subtitle);
    if (auto* logo = logo_image_.Get()) {
        auto* art = texture_at(deps_.root / "assets/logo.png");
        const int wanted = art ? 1 : 0;
        if (logo_shown_ != wanted) { if (art) brush(logo, art); visibility(logo, art ? shown_passive : collapsed); logo_shown_ = wanted; }
    }
}
void Menu::detail(const std::string& title, const std::string& subtitle, std::string body_text, UObject* icon) {
    constexpr size_t description_limit = 360;
    if (body_text.size() > description_limit) { paragraph(panel_, body_text, body); body_text.clear(); }
    auto* d = details_.Get(); if (!d) return;
    if (detail_icon_ != icon) {
        auto* big = part(d, L"MyIcon"); auto* backing = part(d, L"WidgetSwitcher_IconBG");
        if (icon) { brush(part(big, L"Image_LazyIcon"), icon); invoke(backing, L"SetActiveWidgetIndex", L"Index", int32_t{1}); }
        visibility(big, icon ? shown_self_passive : collapsed);
        visibility(backing, icon ? shown_self_passive : collapsed);
        if (auto* overlay = object_of(d, L"Overlay_Icon")) visibility(overlay, icon ? shown_self_passive : collapsed);
        detail_icon_ = icon;
    }
    if (detail_title_ != title) { text_value(part(d, L"MyHeader"), title); detail_title_ = title; }
    if (detail_sub_ != subtitle) {
        auto* box = part(d, L"Size_SubHeader");
        if (!subtitle.empty()) text_value(part(d, L"MySubHeader"), subtitle);
        visibility(box, subtitle.empty() ? collapsed : shown_self_passive);
        detail_sub_ = subtitle;
    }
    if (detail_body_ != body_text) { call_text(d, L"SetDescription", L"InText", body_text); detail_body_ = body_text; }
}
void Menu::status_line(const std::string& value, bool error) {
    auto* block = status_text_.Get(); if (!block) return;
    text(block, status_shown_, value);
    if (status_error_shown_ != int(error)) { invoke(block, L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{error ? danger : muted}); status_error_shown_ = error; }
}
void Menu::fit_panel() {
    if (!panel_fit_pending_) return;
    panel_fit_pending_ = false;
    auto* details = details_.Get(); auto* size = panel_size_.Get();
    if (!details || !size) return;
    Call window(details, L"GetDesiredSize", 1); window.run();
    Call panel(size, L"GetDesiredSize", 1); panel.run();
    const float status_top = float(design_h_ - 170), gap = 24.f;
    const float fixed = float(window.get<Vec2>().y - panel.get<Vec2>().y);
    const float room = std::clamp(status_top - gap - float(window_top) - fixed, 320.f, std::max(320.f, float(design_h_ - 960)));
    if (std::abs(room - panel_max_) <= 2.f) return;
    invoke(size, L"SetMaxDesiredHeight", L"InMaxDesiredHeight", room);
    panel_max_ = room;
    panel_fit_pending_ = true;
}
void Menu::reveal_pending() {
    for (auto& pending : pending_reveals_) {
        if (pending.frames <= 0 || --pending.frames > 0) continue;
        auto* scroll = pending.scroll.Get(); auto* target = pending.target.Get();
        if (!scroll || !target) continue;
        Call call(scroll, L"ScrollWidgetIntoView", 4);
        call.set(L"WidgetToFind", target); call.set(L"AnimateScroll", false);
        call.set(L"ScrollDestination", pending.destination); call.set(L"Padding", 120.f); call.run();
    }
}
void Menu::animate(uint64_t now) {
    if (!transition_started_) return;
    const double t = std::clamp(double(now - transition_started_) / 400., 0., 1.);
    const double eased = 1 - (1 - t) * (1 - t);
    for (auto [weak, dx] : {std::pair{&left_root_, -150.}, std::pair{&right_root_, 150.}}) if (auto* w = weak->Get()) {
        invoke(w, L"SetRenderOpacity", L"InOpacity", float(eased));
        invoke(w, L"SetRenderTranslation", L"Translation", Vec2{dx * (1 - eased), 0});
    }
    if (t >= 1.) transition_started_ = 0;
}

// ---- builds
void Menu::build() {
    auto* pc = pc_.Get(); if (!pc || !page_.Get()) return;
    const auto started = monotonic_us();
    if (viewport_[0] < 320 || viewport_[1] < 240) {
        Call geometry(switcher_.Get(), L"GetCachedGeometry", 1); geometry.run();
        Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"GetLocalSize", 2); size.copy(L"Geometry", geometry, L"ReturnValue"); size.run();
        const auto extent = size.get<Vec2>(); viewport_ = {extent.x, extent.y};
        if (viewport_[0] < 320 || viewport_[1] < 240) { dirty_ = false; return; }
    }
    if (!page(viewport_[0], viewport_[1])) return;
    std::map<UObject*, bool> held;
    for (const auto& hit : hits_) if (auto* w = hit.widget.Get()) held[w] = hit.down;
    held_ = std::move(held);
    hits_.clear(); sliders_.clear(); name_input_.Reset(); search_input_.Reset();
    for (auto* stack : {&tab_items_, &list_, &head_, &panel_, &actions_, &footer_}) stack->used = 0;
    budget_ = budget_per_build;
    bool deferred = false;
    build_page(deferred);
    for (auto* stack : {&tab_items_, &list_, &head_, &panel_, &actions_, &footer_}) finish(*stack);
    if (!confirm_.is_null()) dialog(); else dialog_close();
    if (enter_) { transition_started_ = active_ ? GetTickCount64() : 0; enter_ = false; }
    panel_fit_pending_ = true;
    dirty_ = deferred;
    const auto took = monotonic_us() - started;
    ++cost_.builds; cost_.build_us += took; cost_.last_build_us = took; cost_.max_build_us = std::max(cost_.max_build_us, took);
    cost_.widgets = 0; for (const auto* stack : {&tab_items_, &list_, &head_, &panel_, &actions_, &footer_}) cost_.widgets += stack->used;
}
void Menu::strip(const std::vector<std::string>& names, int selected, const std::string& action) {
    UObject* selected_tab = nullptr;
    for (size_t i = 0; i < names.size(); ++i) {
        auto& tab = take(tab_items_, Kind::tab);
        auto* widget = tab.widget.Get();
        if (tab.text != names[i]) { text_property(widget, L"Text", names[i]); invoke(widget, L"UpdateText"); tab.text = names[i]; }
        state(tab, int(i) == selected);
        if (int(i) == selected) selected_tab = widget;
        bind(tab.hit, {{"action", action}, {"section", int(i)}}, held_);
    }
    finish(tab_items_);
    for (const bool next : {false, true}) {
        const auto& hit = next ? strip_next_ : strip_previous_;
        bind(hit, {{"action", "press"}, {"binding", next ? "next_section" : "previous_section"}}, held_);
        if (!hits_.empty() && hits_.back().widget.Get() == hit.Get()) hits_.back().glyph = next ? strip_next_glyph_ : strip_previous_glyph_;
    }
    const bool many = names.size() > 1;
    for (const auto& hit : {strip_previous_, strip_next_}) if (auto* w = hit.Get()) visibility(w, many ? shown_self_passive : hidden);
    if (strip_glyph_generation_ != bindings_generation_) {
        if (auto* w = strip_previous_glyph_.Get()) glyph(w, "previous_section", glyph_left_bumper);
        if (auto* w = strip_next_glyph_.Get()) glyph(w, "next_section", glyph_right_bumper);
        strip_glyph_generation_ = bindings_generation_;
    }
    const int key = selected + int(names.size()) * 1000;
    if (shown_section_ != key && selected_tab) if (auto* scroll = strip_scroll_.Get()) {
        Call reveal(scroll, L"ScrollWidgetIntoView", 4); reveal.set(L"WidgetToFind", selected_tab);
        reveal.set(L"AnimateScroll", true); reveal.set(L"ScrollDestination", uint8_t{2}); reveal.set(L"Padding", 0.f); reveal.run();
    }
    shown_section_ = key;
}
void Menu::action_prompt(const std::string& binding, const std::string& label, Json action, uint8_t icon, bool enabled) {
    if (binding.empty() && gamepad_) return;
    auto& item = take(actions_, Kind::action);
    text(item.text_block.Get(), item.text, label);
    const auto signature = binding + "/" + std::to_string(icon) + "#" + std::to_string(bindings_generation_);
    if (item.glyph != signature) {
        auto* prompt = item.extra.Get();
        visibility(prompt, shown_passive);
        if (binding.empty()) glyph(prompt, "", glyph_none, 0);
        else glyph(prompt, binding, icon);
        visibility(item.extra2.Get(), collapsed);
        item.glyph = signature;
    }
    if (item.enabled != int(enabled)) { invoke(item.widget.Get(), L"SetRenderOpacity", L"InOpacity", enabled ? 1.f : .45f); item.enabled = enabled; }
    if (enabled) { bind(item.hit, std::move(action), held_); if (!binding.empty() && !hits_.empty()) hits_.back().glyph = item.extra; }
}
void Menu::bar_prompt(const std::string& label, Json action, const std::string& first, uint8_t first_icon, uint8_t first_key, const std::string& second, uint8_t second_icon, uint8_t second_key) {
    auto& item = take(footer_, Kind::action);
    text(item.text_block.Get(), item.text, label);
    const auto signature = first + "/" + std::to_string(first_icon) + "/" + std::to_string(first_key) + "|" + second + "/" + std::to_string(second_icon) + "/" + std::to_string(second_key) + "#" + std::to_string(bindings_generation_);
    if (item.glyph != signature) {
        glyph(item.extra.Get(), first, first_icon, first_key);
        auto* extra = item.extra2.Get();
        const bool two = !second.empty() || second_key != 255;
        visibility(extra, two ? shown_passive : collapsed);
        if (two) glyph(extra, second, second_icon, second_key);
        item.glyph = signature;
    }
    if (item.enabled != 1) { invoke(item.widget.Get(), L"SetRenderOpacity", L"InOpacity", 1.f); item.enabled = 1; }
    if (action.is_null() && !first.empty()) {
        auto press = [](const std::string& binding) { return Json{{"action", "press"}, {"binding", binding}}; };
        bind(item.hit, press(second.empty() ? first : second), held_);
        if (hits_.empty()) return;
        hits_.back().glyph = second.empty() ? item.extra : item.extra2;
        if (!second.empty()) hits_.back().parts = {{item.extra, press(first)}, {item.extra2, press(second)}};
        return;
    }
    bind(item.hit, std::move(action), held_);
    if (!hits_.empty() && hits_.back().widget.Get() == item.hit.Get()) hits_.back().glyph = item.extra;
}
void Menu::option_row(const std::string& name, const std::string& value, bool focused, Json minus, Json plus, bool arrows) {
    auto& item = take(panel_, Kind::option);
    text(item.text_block.Get(), item.text, name);
    text(item.value_block.Get(), item.value, value);
    if (item.arrows != int(arrows)) {
        for (const auto& hit : {item.hit_left, item.hit_right}) if (auto* w = hit.Get()) visibility(w, arrows ? shown_self_passive : hidden);
        item.arrows = arrows;
    }
    state(item, focused);
    if (arrows) { bind(item.hit_left, std::move(minus), held_); bind(item.hit_right, std::move(plus), held_); }
}
void Menu::slider_row(const std::string& name, const Json& control, bool focused, Json minus, Json plus) {
    auto& item = take(panel_, Kind::slider);
    const double value = control.at("value").get<double>(), low = control.at("min").get<double>(), high = control.at("max").get<double>(), step = control.at("step").get<double>();
    const auto unit = control.value("unit", std::string{});
    text(item.text_block.Get(), item.text, name);
    text(item.value_block.Get(), item.value, display_value(control) + unit);
    const float fraction = high > low ? float(std::clamp((value - low) / (high - low), 0., 1.)) : 0.f;
    if (std::abs(item.fill - fraction) > 1e-4f) { invoke(item.extra.Get(), L"UpdateProgressBar", L"InPercent", fraction); item.fill = fraction; }
    state(item, focused);
    bind(item.hit_left, std::move(minus), held_); bind(item.hit_right, std::move(plus), held_);
    sliders_.push_back({item.extra, item.value_block, item.widget, control, value, low, high, step, unit});
}
void Menu::bar_prompt_actions(const std::string& label, const std::string& first, uint8_t first_icon, const std::string& second, uint8_t second_icon) {
    auto& item = take(actions_, Kind::action);
    text(item.text_block.Get(), item.text, label);
    const auto signature = first + "/" + std::to_string(first_icon) + "|" + second + "/" + std::to_string(second_icon) + "#" + std::to_string(bindings_generation_);
    if (item.glyph != signature) {
        glyph(item.extra.Get(), first, first_icon, 255);
        auto* extra = item.extra2.Get();
        visibility(extra, second.empty() ? collapsed : shown_passive);
        if (!second.empty()) glyph(extra, second, second_icon, 255);
        item.glyph = signature;
    }
    if (item.enabled != 1) { invoke(item.widget.Get(), L"SetRenderOpacity", L"InOpacity", 1.f); item.enabled = 1; }
    if (!first.empty()) {
        auto press = [](const std::string& binding) { return Json{{"action", "press"}, {"binding", binding}}; };
        bind(item.hit, press(second.empty() ? first : second), held_);
        if (hits_.empty()) return;
        hits_.back().glyph = second.empty() ? item.extra : item.extra2;
        if (!second.empty()) hits_.back().parts = {{item.extra, press(first)}, {item.extra2, press(second)}};
    }
}
void Menu::build_picker(bool& deferred) {
    const auto* c = current_control();
    detail("Choose " + (c ? c->value("label", std::string{}) : std::string{}), "Search by name or keyword", "Type to narrow the list. Up / Down to move, then Select.");
    auto& input = take(head_, Kind::input);
    search_input_ = input.extra;
    if (input.value != search_query_) {
        Call focus(input.extra.Get(), L"HasKeyboardFocus", 1); focus.run();
        if (!focus.get<bool>()) text_value(input.extra.Get(), search_query_);
        input.value = search_query_;
    }
    if (input.text != "hint") { call_text(input.extra.Get(), L"SetHintText", L"InHintText", "Type a name or keyword"); input.text = "hint"; }
    const auto& search = options_;
    paragraph(head_, search.matches.empty() ? "No matches. Try fewer or shorter words."
        : search.matches.size() == search.options.size() ? std::to_string(search.options.size()) + " options"
        : std::to_string(search.matches.size()) + " of " + std::to_string(search.options.size()) + " match", muted);
    constexpr size_t shown = 100;
    const size_t first = search.selected < shown ? 0 : search.selected - shown + 1;
    UObject* focus_widget = nullptr;
    for (size_t i = first; i < std::min(first + shown, search.matches.size()); ++i) {
        if (!ready(panel_, Kind::row)) { deferred = true; break; }
        const auto& found = search.options[search.matches[i]];
        auto& item = take(panel_, Kind::row);
        RowLook look; look.badge = c && found.at("id") == c->at("value"); look.value = found.value("group", std::string{}); look.icon = game_icon(found.value("icon", std::string{}));
        fill_row(item, found.value("label", std::string{}), look, i == search.selected);
        if (i == search.selected) focus_widget = item.widget.Get();
        bind(item.hit, {{"action", "pick_row"}, {"row", i}}, held_);
    }
    action_prompt("accept", "Select", {{"action", "pick_apply"}}, glyph_accept);
    action_prompt("close", "Back", {{"action", "pick_cancel"}}, glyph_back);
    const std::string context = "picker/" + (c ? c->value("id", std::string{}) : std::string{});
    if (auto* scroll = panel_scroll_.Get(); scroll && (context != panel_context_ || focus_widget != panel_revealed_)) {
        pending_reveals_[1] = {};
        if (context != panel_context_) invoke(scroll, L"ScrollToStart");
        if (focus_widget && context != panel_context_) pending_reveals_[1] = {panel_scroll_, WeakObject(focus_widget), 2, 2};
        else if (focus_widget) { Call reveal(scroll, L"ScrollWidgetIntoView", 4); reveal.set(L"WidgetToFind", focus_widget); reveal.set(L"AnimateScroll", true); reveal.set(L"ScrollDestination", uint8_t{0}); reveal.set(L"Padding", 120.f); reveal.run(); }
    }
    panel_context_ = context; panel_revealed_ = focus_widget;
}
// The page: sections in the strip, the section's controls as list rows, the selected control's
// editor in the details window, the status line and the prompt bar.
void Menu::build_page(bool& deferred) {
    header(deps_.title, "CCS " + deps_.version);
    if (!model_.is_object() || !model_.contains("sections")) {
        strip({}, 0, "section");
        paragraph(list_, error_.empty() ? "The page is not ready yet." : error_, danger);
        detail(deps_.title, "", "");
        finish(list_); finish(head_); finish(panel_); finish(actions_);
        status_line(error_, !error_.empty());
        bar_prompt("Close", {{"action", "close"}}, "close", glyph_back, 255);
        finish(footer_);
        return;
    }
    const auto& sections = model_["sections"]; const int count = int(sections.size());
    section_ = std::clamp(section_, 0, std::max(0, count - 1));
    std::vector<std::string> names; for (const auto& s : sections) names.push_back(s.value("title", std::string{}));
    strip(names, section_, "section");
    const auto& controls = count ? sections[section_]["controls"] : Json::array();
    const int rows = int(controls.size());
    row_ = std::clamp(row_, 0, std::max(0, rows - 1));
    const auto section_help = count ? sections[section_].value("description", std::string{}) : std::string{};
    if (!section_help.empty()) paragraph(list_, section_help, muted);
    UObject* selected_widget = nullptr; UObject* heading = nullptr; UObject* selected_heading = nullptr;
    std::string last_group;
    for (int i = 0; i < rows; ++i) {
        const auto& c = controls[i];
        const auto group = c.value("group", std::string{});
        if (!group.empty() && group != last_group) {
            if (!ready(list_, Kind::header)) { deferred = true; break; }
            auto& item = take(list_, Kind::header); text(item.text_block.Get(), item.text, group); heading = item.widget.Get(); last_group = group;
        }
        if (!ready(list_, Kind::row)) { deferred = true; break; }
        auto& item = take(list_, Kind::row);
        RowLook look; look.value = state_word(c); look.icon = game_icon(c.value("icon", std::string{}));
        look.enabled = c.value("enabled", true) && !c.value("busy", false);
        look.badge = (c.at("type") == "toggle" && c.value("value", false) && look.enabled) || c.value("badge", false);
        fill_row(item, c.value("label", std::string{}), look, i == row_);
        bind(item.hit, {{"action", "row"}, {"row", i}}, held_);
        if (i == row_) { selected_widget = item.widget.Get(); selected_heading = heading; }
        heading = nullptr;
    }
    if (!rows) paragraph(list_, "This section has nothing to show.", muted);
    if (picker_) { build_picker(deferred); }
    else {
        if (model_.contains("notice") && model_["notice"].is_object()) {
            const auto& n = model_["notice"];
            const auto note = n.value("text", std::string{}), label = n.value("label", std::string{}), action = n.value("action", std::string{});
            if (!note.empty()) paragraph(head_, note, warning);
            if (!label.empty() && !action.empty()) {
                auto& item = take(head_, Kind::action);
                text(item.text_block.Get(), item.text, label);
                const auto signature = "secondary/notice#" + std::to_string(bindings_generation_);
                if (item.glyph != signature) { glyph(item.extra.Get(), "secondary", glyph_secondary); visibility(item.extra.Get(), shown_passive); visibility(item.extra2.Get(), collapsed); item.glyph = signature; }
                if (item.enabled != 1) { invoke(item.widget.Get(), L"SetRenderOpacity", L"InOpacity", 1.f); item.enabled = 1; }
                bind(item.hit, {{"action", "run"}, {"id", action}}, held_);
                if (!hits_.empty()) hits_.back().glyph = item.extra;
            }
        }
        if (rows) {
            const auto& c = controls[row_]; const auto type = c.at("type").get<std::string>();
            const bool enabled = interactive(c);
            detail(c.value("title", c.value("label", std::string{})), c.value("subtitle", effect_label(c)), c.value("description", std::string{}), game_icon(c.value("icon", std::string{})));
            if (const auto hint = c.value("hint", std::string{}); !hint.empty()) paragraph(panel_, hint, muted);
            if (!enabled && type != "label" && type != "progress" && type != "loading") paragraph(panel_, c.value("busy", false) ? "Working..." : c.value("disabled_label", std::string("Unavailable")), muted);
            const Json minus = {{"action", "adjust"}, {"delta", -1}}, plus = {{"action", "adjust"}, {"delta", 1}};
            bool horizontal = false;
            if (type == "toggle") {
                const bool on = c.value("value", false);
                option_row("State", on ? "On" : "Off", true, minus, plus, enabled);
                if (enabled) action_prompt("accept", on ? "Turn off" : "Turn on", {{"action", "activate"}}, glyph_accept);
                horizontal = enabled;
            } else if (type == "number") {
                option_row("Value", display_value(c) + c.value("unit", std::string{}), true, minus, plus, enabled);
                horizontal = enabled;
            } else if (type == "slider") {
                slider_row("Value", c, true, minus, plus);
                horizontal = enabled;
            } else if (type == "choice") {
                option_row(c.value("value_label", std::string("Choice")), display_value(c), true, minus, plus, enabled);
                if (enabled) action_prompt("accept", c.at("options").size() > 8 ? "Browse and search" : "Next option", {{"action", c.at("options").size() > 8 ? "pick" : "activate"}}, glyph_accept);
                horizontal = enabled;
            } else if (type == "radio") {
                for (const auto& option : c.at("options")) {
                    if (!ready(panel_, Kind::row)) { deferred = true; break; }
                    auto& item = take(panel_, Kind::row);
                    RowLook look; look.badge = option.at("id") == c.at("value"); look.enabled = enabled;
                    fill_row(item, option.value("label", std::string{}), look, look.badge);
                    if (enabled) bind(item.hit, {{"action", "value"}, {"value", option.at("id")}}, held_);
                }
                horizontal = enabled;
            } else if (type == "text") {
                const auto key = c.at("id").get<std::string>();
                if (text_key_ != key) { text_key_ = key; text_draft_ = c.value("value", std::string{}); }
                auto& input = take(head_, Kind::input);
                name_input_ = input.extra;
                if (input.text != "text") { call_text(input.extra.Get(), L"SetHintText", L"InHintText", "Type here"); input.text = "text"; }
                Call focus(input.extra.Get(), L"HasKeyboardFocus", 1); focus.run();
                if (input.value != text_draft_ && !focus.get<bool>()) { text_value(input.extra.Get(), text_draft_); input.value = text_draft_; }
                invoke(input.extra.Get(), L"SetIsEnabled", L"bInIsEnabled", enabled);
                if (enabled) action_prompt("accept", c.value("action_label", std::string("Save text")), {{"action", "text"}}, glyph_accept);
            } else if (type == "progress" || type == "loading") {
                option_row(type == "progress" ? "Progress" : "State", display_value(c), false, Json{}, Json{}, false);
            } else if (type == "label") {
            } else if (type == "button") {
                if (enabled) action_prompt("accept", c.value("action_label", c.value("label", std::string{})), {{"action", "activate"}}, glyph_accept);
            }
            if (c.contains("lines") && c["lines"].is_array()) for (const auto& line : c["lines"]) if (line.is_string()) paragraph(panel_, line.get<std::string>(), muted);
            if (horizontal) { if (gamepad_) bar_prompt_actions("Adjust", "", glyph_dpad_horizontal); else bar_prompt_actions("Adjust", "left", glyph_left, "right", glyph_right); }
            if (c.contains("confirm") && enabled) paragraph(panel_, "Asks for confirmation", muted);
        } else detail(deps_.title, "", "");
        const std::string context = std::to_string(section_) + "/" + std::to_string(row_);
        if (context != panel_context_) { if (auto* scroll = panel_scroll_.Get()) invoke(scroll, L"ScrollToStart"); panel_context_ = context; panel_revealed_ = nullptr; }
    }
    finish(list_); finish(head_); finish(panel_); finish(actions_);
    const auto status = model_.value("status", std::string{});
    status_line(error_.empty() ? status : error_, !error_.empty());
    const int key = section_ + count * 1000;
    if (revealed_row_ != row_ || shown_section_ != key) {
        if (selected_widget) {
            const bool upward = row_ < revealed_row_;
            auto* target = upward && selected_heading ? selected_heading : selected_widget;
            if (shown_section_ == key) { pending_reveals_[0] = {}; if (auto* scroll = list_scroll_.Get()) { Call reveal(scroll, L"ScrollWidgetIntoView", 4); reveal.set(L"WidgetToFind", target); reveal.set(L"AnimateScroll", true); reveal.set(L"ScrollDestination", uint8_t{0}); reveal.set(L"Padding", 120.f); reveal.run(); } }
            else pending_reveals_[0] = {list_scroll_, WeakObject(target), 0, 2};
        }
        revealed_row_ = row_;
    }
    bar_prompt("Close", {{"action", "close"}}, "close", glyph_back, 255);
    if (gamepad_) bar_prompt("Browse", Json{}, "", glyph_dpad_vertical, 255);
    else bar_prompt("Browse", Json{}, "up", glyph_up, 255, "down", glyph_down, 255);
    finish(footer_);
}

// ---- the confirmation dialog
void Menu::dialog() {
    const auto* c = current_control();
    std::string effect = c ? effect_label(*c) : std::string{};
    const auto message = confirm_.value("message", std::string("Continue?"));
    const auto description = effect.empty() ? message : effect + "\n" + message;
    auto* pc = pc_.Get(); if (!pc) return;
    auto* dialog = dialog_.Get();
    if (!dialog) {
        dialog = create_widget(pc, game_class(dialog_class));
        dialog_ = dialog; dialog_shown_.clear(); dialog_focus_shown_ = -1;
        dialog_primary_ = object_of(dialog, L"PrimaryOption");
        dialog_secondary_ = object_of(dialog, L"SecondaryOption");
        auto* tree = object_of(dialog, L"WidgetTree");
        auto* trajan = load(title_font);
        auto glyph_label = [&](UObject* option, const std::string& label, const std::string& binding, uint8_t fallback, WeakObject& prompt_out) {
            if (!option || !tree) return;
            auto* overlay = object_of(option, L"Overlay_Main");
            auto* label_block = object_of(option, L"Button_Text");
            if (!overlay || !label_block) return;
            visibility(label_block, collapsed);
            auto* line = construct(L"/Script/UMG.HorizontalBox", tree);
            auto* slot = add_child(overlay, line);
            invoke(slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{2});
            invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
            visibility(line, shown_passive);
            auto* prompt = create_widget(pc, game_class(prompt_class));
            invoke(add_child(line, prompt), L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
            glyph(prompt, binding, fallback);
            visibility(prompt, shown_passive);
            prompt_out = prompt;
            auto* name = text_block(tree, 32, trajan, Color{.12f, .11f, .09f, 1}, false);
            text_value(name, label);
            auto* name_slot = add_child(line, name);
            invoke(name_slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
            padding(name_slot, Margin{12, 0, 0, 0});
        };
        glyph_label(dialog_primary_.Get(), "Confirm", "accept", glyph_accept, dialog_glyphs_[0]);
        glyph_label(dialog_secondary_.Get(), "Cancel", "close", glyph_back, dialog_glyphs_[1]);
        dialog_glyph_generation_ = bindings_generation_;
    }
    if (dialog_glyph_generation_ != bindings_generation_) {
        if (auto* w = dialog_glyphs_[0].Get()) glyph(w, "accept", glyph_accept);
        if (auto* w = dialog_glyphs_[1].Get()) glyph(w, "close", glyph_back);
        dialog_glyph_generation_ = bindings_generation_;
    }
    if (dialog_shown_ != description) {
        const bool opening = dialog_shown_.empty();
        text_property(dialog, L"PromptText", "Confirm");
        text_property(dialog, L"PrimaryOptionText", "Confirm");
        text_property(dialog, L"SecondaryOptionText", "Cancel");
        text_property(dialog, L"OptionalDescription", description);
        invoke(dialog, L"InitData");
        if (opening) invoke(dialog, L"AddToViewport", L"ZOrder", int32_t{1000});
        invoke(dialog, L"DisableInputListener");
        invoke(dialog, L"HandleDescription", L"Valid", true);
        for (const auto& option : {dialog_primary_, dialog_secondary_}) if (auto* o = option.Get()) if (auto* label_block = object_of(o, L"Button_Text")) visibility(label_block, collapsed);
        dialog_shown_ = description; dialog_focus_shown_ = -1;
        if (opening) {
            Call all(find_cached(L"/Script/UMG.Default__WidgetBlueprintLibrary"), L"GetAllWidgetsOfClass", 4);
            all.set(L"WorldContextObject", pc); all.set(L"WidgetClass", game_class(listener_class)); all.set(L"TopLevelOnly", false); all.run();
            auto* found = all.param(L"FoundWidgets");
            if (found->IsA<FArrayProperty>()) {
                FScriptArrayHelper listeners(static_cast<FArrayProperty*>(found), all.data(found));
                auto* menu_root = main_.Get();
                for (int i = 0; i < std::min(listeners.Num(), 256); ++i) {
                    UObject* listener{}; std::memcpy(&listener, listeners.GetRawPtr(i), sizeof(listener));
                    if (!listener) continue;
                    bool inside = false;
                    for (auto* outer = listener->GetOuterPrivate(); outer; outer = outer->GetOuterPrivate()) if (outer == menu_root) { inside = true; break; }
                    if (!inside) continue;
                    Call enabled(listener, L"IsEnabled", 1); enabled.run();
                    if (!enabled.get<bool>()) continue;
                    invoke(listener, L"SetEnabledState", L"bEnabled", false);
                    frozen_listeners_.emplace_back(listener);
                }
            }
        }
    }
    if (dialog_focus_shown_ != dialog_focus_) {
        auto* on = (dialog_focus_ == 0 ? dialog_primary_ : dialog_secondary_).Get();
        auto* off = (dialog_focus_ == 0 ? dialog_secondary_ : dialog_primary_).Get();
        if (off) invoke(off, L"OnNullState");
        if (on) invoke(on, L"OnHighlightedState");
        dialog_focus_shown_ = dialog_focus_;
    }
    if (auto* primary = dialog_primary_.Get()) hits_.push_back({WeakObject(nav_button(primary, L"WBP_NavigationButton")), {{"action", "confirm"}}, false, {}, {}});
    if (auto* secondary = dialog_secondary_.Get()) hits_.push_back({WeakObject(nav_button(secondary, L"WBP_NavigationButton")), {{"action", "cancel"}}, false, {}, {}});
}
void Menu::dialog_close(bool destroy) {
    for (auto& listener : frozen_listeners_) if (auto* l = listener.Get()) { try { invoke(l, L"SetEnabledState", L"bEnabled", true); } catch (...) {} }
    frozen_listeners_.clear();
    if (!dialog_shown_.empty()) if (auto* dialog = dialog_.Get()) { try { invoke(dialog, L"RemoveFromParent"); } catch (...) {} }
    dialog_shown_.clear(); dialog_focus_shown_ = -1;
    if (destroy) { dialog_.Reset(); dialog_primary_.Reset(); dialog_secondary_.Reset(); dialog_glyphs_ = {}; }
}
}

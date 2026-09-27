// Hosting in the Player Menu, input, navigation and the model flow. The page itself (game
// widgets, pooling, builds) is menu_page.cpp. Ported from the CSSX 1.2.0 menu; CCS depends on
// neither CSS nor CSSX at runtime.
#include "menu.hpp"
#include "menu_keys.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_map>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
using namespace engine;
namespace {
// Each mapped key's FName is made once; the menu asks for every key every frame.
FName key_name(const std::string& key) {
    static std::unordered_map<std::string, FName> names;
    if (auto it = names.find(key); it != names.end()) return it->second;
    return names.emplace(key, FName(wide(key).c_str())).first->second;
}
bool key_down(UObject* pc, const std::string& key) {
    Call call(pc, L"IsInputKeyDown", 2); auto* p = call.param(L"Key");
    member(call.data(p), p->GetElementSize(), find_cached(L"/Script/InputCore.Key"), L"KeyName", key_name(key));
    call.run(); return call.get<bool>();
}
// The title a Player Menu tab shows, or empty when it cannot be read safely.
std::string tab_title(UObject* widget) {
    try { return widget ? text_property_string(widget, L"Text", 64) : std::string{}; } catch (...) { return {}; }
}
bool has_focus(UObject* widget) {
    if (!widget) return false;
    Call focus(widget, L"HasKeyboardFocus", 1); focus.run(); return focus.get<bool>();
}
}
void Menu::navigate(int index) {
    auto* tabs = tabs_.Get(); if (!tabs) return;
    if (index == tab_index_ && page_.Get() && tab_.Get() && switcher_.Get()) {
        // Another mod may reorder the strip after attachment: resolve our paired children when opening.
        Call page_index(switcher_.Get(), L"GetChildIndex", 2); page_index.set(L"content", page_.Get()); page_index.run();
        Call tab_index(tabs, L"GetChildIndex", 2); tab_index.set(L"content", tab_.Get()); tab_index.run();
        index = page_index.get<int32_t>();
        if (index < 0 || index != tab_index.get<int32_t>()) throw std::runtime_error("CCS tab/page pairing is inconsistent");
        tab_index_ = index;
    }
    Call nav(tabs, L"NavigateToCustomIndex", 3); nav.set(L"Index", int32_t{index}); nav.run();
    if (!nav.get<bool>(L"Success")) throw std::runtime_error("Player Menu tab navigation rejected the index");
}
bool Menu::attach(const PlayerContext& player) {
    auto* handler = object_of(player.pc, L"User Interface Handler Component");
    auto* game = object_of(handler, L"WBP_Menu_Game");
    auto* main = object_of(game, L"WBP_Menu_Main");
    if (!main || !bool_of(main, L"bOpen")) return false;
    auto* tabs = object_of(main, L"BP_HBC_Menu_Game"); auto* pages = object_of(main, L"BP_WS_Menu_Game"); auto* original = object_of(main, L"WBP_NBM_Inventory");
    if (!tabs || !pages || !original) throw std::runtime_error("Player Menu layout is unavailable");
    const auto page_list = children(pages); const auto tab_list = children(tabs);
    if (page_list.size() != tab_list.size() || page_list.size() < 3 || page_list.size() > 8) return false;
    // CSS attaches first when it is installed (it expects exactly three pages). Wait for it for
    // two seconds; a broken CSS must never hide CCS. CSSX orders itself last whenever it attaches.
    // CSS attaches first when installed (it expects exactly three pages), and the shipped CSSX
    // 1.2.0 refuses to attach once any extra tab precedes it. So CCS waits for both, up to three
    // seconds each, and takes the last place. Nothing in CSS or CSSX needs to know about CCS.
    const auto now = GetTickCount64();
    bool css_seen = false, cssx_seen = false;
    for (size_t i = 3; i < tab_list.size(); ++i) { const auto title = tab_title(tab_list[i]); if (title == "CSS") css_seen = true; if (title == "CSSX") cssx_seen = true; if (title == "CCS") throw std::runtime_error("Another CCS tab is already attached"); }
    if ((deps_.css_present && !css_seen) || (deps_.cssx_present && !cssx_seen)) {
        if (!attach_wait_since_) attach_wait_since_ = now;
        if (now - attach_wait_since_ < 3000) return false;
        if (deps_.log) deps_.log("CCS: proceeding without waiting further for the other menu tabs");
    }
    attach_wait_since_ = 0;
    struct Construction {
        std::vector<UObject*> roots; UObject* tab{}; UObject* page{}; bool committed{};
        void hold(UObject* object) { if (object && !object->IsRootSet()) { roots.push_back(object); object->SetRootSet(); } }
        ~Construction() {
            if (!committed) for (auto* widget : {page, tab}) if (widget) { try { invoke(widget, L"RemoveFromParent"); } catch (...) {} }
            for (auto* object : roots) object->ClearRootSet();
        }
    } construction;
    auto* tab = create_widget(player.pc, original->GetClassPrivate());
    construction.tab = tab; construction.hold(tab);
    for (auto name : {L"FontData", L"RootSize", L"RootScale", L"DefaultColor", L"SelectedColor", L"bUseHighlight", L"HighlightY"}) copy_property(tab, original, name);
    text_property(tab, L"Text", "CCS");
    invoke(tab, L"UpdateText"); invoke(tab, L"CommitSize"); invoke(tab, L"CommitScale");
    auto* page = create_widget(player.pc, static_cast<UClass*>(main->GetClassPrivate()->GetSuperStruct()));
    construction.page = page; construction.hold(page);
    auto* tree = object_of(page, L"WidgetTree"); if (!tree) throw std::runtime_error("CCS page has no widget tree");
    auto* canvas = construct(L"/Script/UMG.CanvasPanel", tree); construction.hold(canvas); object_property(tree, L"RootWidget", canvas);
    { Call add(pages, L"AddChild", 2); add.set(L"content", page); add.run(); }
    { Call add(tabs, L"AddChildToHorizontalBox", 2); add.set(L"content", tab); add.run(); }
    pc_ = player.pc; handler_ = handler; main_ = main; tabs_ = tabs; switcher_ = pages; page_ = page; tab_ = tab; tree_ = tree; canvas_ = canvas;
    try { order_tabs(); } catch (...) { forget(); throw; }
    construction.committed = true;
    if (deps_.log) deps_.log("CCS tab attached to the Player Menu at index " + std::to_string(tab_index_));
    return true;
}
// Inventory, Tarstones, Map, CSS, CSSX, CCS: CCS is appended last, so the other mods' tab indices
// never move under them.
void Menu::order_tabs() {
    auto tabs = children(tabs_.Get()); auto pages = children(switcher_.Get());
    if (tabs.size() != pages.size() || tabs.size() < 4 || tabs.size() > 8) throw std::runtime_error("Player Menu tab count is unexpected");
    if (tabs.back() != tab_.Get() || pages.back() != page_.Get()) throw std::runtime_error("CCS tab is not the last Player Menu tab");
    const auto& new_tabs = tabs;
    // Share the original top bar spacing across the extra title(s), as CSS does.
    const float factor = new_tabs.size() >= 5 ? .6f : .75f;
    std::array<float, 4> reference{80, 0, 80, 0};
    for (auto* child : new_tabs) if (child != tab_.Get()) if (auto* slot = object_of(child, L"Slot")) { reference = read<std::array<float, 4>>(slot, L"Padding"); break; }
    if (auto* slot = object_of(tab_.Get(), L"Slot")) {
        auto padding = reference; padding[0] *= factor; padding[2] *= factor;
        invoke(slot, L"SetPadding", L"InPadding", padding);
        invoke(slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", uint8_t{2});
        invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", uint8_t{2});
    }
    nav_children_refresh(tabs_.Get());
    tab_index_ = int(new_tabs.size()) - 1;
}
void Menu::close() {
    if (!main_.Get() || !bool_of(main_.Get(), L"bOpen")) return;
    auto* handler = handler_.Get(); if (!handler) return;
    if (deps_.log) deps_.log("Menu: closing the Player Menu from the CCS page");
    try { dialog_close(); } catch (...) {}
    confirm_ = nullptr; picker_ = false; hits_.clear(); sliders_.clear(); drag_slider_ = -1;
    Call close_call(handler, L"HandleGameMenu", 2); close_call.set(L"SubTabIndex", int32_t{0}); close_call.set(L"AllowClose", true); close_call.run();
    active_ = was_active_ = false;
}
void Menu::forget() {
    try { dialog_close(true); } catch (...) {}
    forget_page();
    unroot_all(); textures_.clear(); warm_texture_ = 0;
    page_.Reset(); tab_.Reset(); tree_.Reset(); canvas_.Reset(); main_.Reset(); tabs_.Reset(); switcher_.Reset(); handler_.Reset(); pc_.Reset();
    hits_.clear(); sliders_.clear(); bindings_.clear(); drag_slider_ = -1;
    search_input_.Reset(); name_input_.Reset(); input_prompt_.Reset();
    active_ = was_active_ = false; tab_index_ = -1; attach_wait_since_ = 0; viewport_ = {}; dirty_ = true;
}
void Menu::detach() {
    if (auto* tabs = tabs_.Get(); tabs && tab_.Get() && main_.Get() && active_) { try { navigate(0); } catch (...) {} }
    if (auto* page = page_.Get()) { try { invoke(page, L"RemoveFromParent"); } catch (...) {} }
    if (auto* tab = tab_.Get()) { try { invoke(tab, L"RemoveFromParent"); } catch (...) {} }
    if (auto* tabs = tabs_.Get()) { try { nav_children_refresh(tabs); } catch (...) {} }
    forget();
}
void Menu::bind_inputs() {
    bindings_.clear();
    auto* pc = pc_.Get(); auto* handler = handler_.Get();
    auto* mapping = object_of(handler, L"InputMapping");
    auto* p = mapping ? mapping->GetPropertyByNameInChain(L"Mappings") : nullptr;
    if (!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Menu input mapping is unavailable");
    Call subsystem(find_cached(L"/Script/Engine.Default__SubsystemBlueprintLibrary"), L"GetLocalPlayerSubSystemFromPlayerController", 3);
    subsystem.set(L"PlayerController", pc); subsystem.set(L"Class", static_cast<UClass*>(find_cached(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem"))); subsystem.run();
    auto* input = subsystem.get<UObject*>();
    if (!input) throw std::runtime_error("Player input subsystem is unavailable");
    static const std::map<std::wstring, std::string> actions = {
        {L"IA_Menu_Up", "up"}, {L"IA_Menu_Down", "down"}, {L"IA_Menu_Left_Primary", "left"}, {L"IA_Menu_Right_Primary", "right"},
        {L"IA_Menu_Left_Tertiary", "previous_section"}, {L"IA_Menu_Right_Tertiary", "next_section"},
        {L"IA_Menu_Confirm_Primary_Press", "accept"}, {L"IA_Menu_Confirm_Secondary_Press", "secondary"}, {L"IA_Menu_Back", "close"},
        {L"IA_Menu_Confirm_Tertiary_Press", "search"}};   // the third face button (Y / triangle) and its keyboard key focus the search field
    auto* a = static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a, reinterpret_cast<std::byte*>(mapping) + p->GetOffset_Internal());
    if (values.Num() < 0 || values.Num() > 256) throw std::runtime_error("Input map exceeds bound");
    auto* mapping_struct = find_cached(L"/Script/EnhancedInput.EnhancedActionKeyMapping");
    auto* ap = field(mapping_struct, L"Action", 8);
    auto* key_field = mapping_struct->GetPropertyByNameInChain(L"Key");
    auto* kn = field(find_cached(L"/Script/InputCore.Key"), L"KeyName", sizeof(FName));
    if (!key_field || key_field->GetOffset_Internal() < 0 || kn->GetOffset_Internal() + int(sizeof(FName)) > key_field->GetElementSize()) throw std::runtime_error("Input mapping key layout mismatch");
    const int element = a->GetInner()->GetElementSize();
    if (ap->GetOffset_Internal() + 8 > element || key_field->GetOffset_Internal() + key_field->GetElementSize() > element) throw std::runtime_error("Input mapping element layout mismatch");
    auto usable = [](const std::string& name) { return !(name == "Gamepad_LeftX" || name == "Gamepad_LeftY" || name == "Gamepad_RightX" || name == "Gamepad_RightY"); };
    std::map<UObject*, size_t> index;
    for (int i = 0; i < values.Num(); ++i) {
        UObject* action{}; std::memcpy(&action, values.GetRawPtr(i) + ap->GetOffset_Internal(), 8);
        if (!action) continue;
        const auto found = actions.find(action->GetName());
        if (found == actions.end()) continue;
        auto it = index.find(action);
        if (it == index.end()) { Binding binding; binding.input_action = action; binding.action = found->second; bindings_.push_back(std::move(binding)); it = index.emplace(action, bindings_.size() - 1).first; }
        FName key{}; std::memcpy(&key, values.GetRawPtr(i) + key_field->GetOffset_Internal() + kn->GetOffset_Internal(), sizeof(key));
        auto name = narrow(key.ToString());
        auto& keys = bindings_[it->second].keys;
        if (usable(name) && !name.empty() && name != "None" && std::find(keys.begin(), keys.end(), name) == keys.end()) keys.push_back(name);
    }
    static const std::map<std::string, std::vector<std::string>> stick = {{"up", {"Gamepad_LeftStick_Up"}}, {"down", {"Gamepad_LeftStick_Down"}}, {"left", {"Gamepad_LeftStick_Left"}}, {"right", {"Gamepad_LeftStick_Right"}}};
    for (auto& binding : bindings_) if (auto extra = stick.find(binding.action); extra != stick.end()) for (const auto& k : extra->second) if (std::find(binding.keys.begin(), binding.keys.end(), k) == binding.keys.end()) binding.keys.push_back(k);
    for (auto& binding : bindings_) {
        try {
            Call query(input, L"QueryKeysMappedToAction", 2); query.set(L"Action", binding.input_action.Get()); query.run();
            auto* out = query.param(L"ReturnValue");
            if (!out->IsA<FArrayProperty>()) continue;
            auto* array = static_cast<FArrayProperty*>(out); FScriptArrayHelper keys(array, query.data(out));
            if (keys.Num() <= 0 || keys.Num() > 32 || kn->GetOffset_Internal() + 8 > array->GetInner()->GetElementSize()) continue;
            std::vector<std::string> applied;
            for (int n = 0; n < keys.Num(); ++n) { FName key{}; std::memcpy(&key, keys.GetRawPtr(n) + kn->GetOffset_Internal(), sizeof(key)); auto name = narrow(key.ToString()); if (usable(name)) applied.push_back(name); }
            if (!applied.empty()) { for (const auto& k : binding.keys) if (k.starts_with("Gamepad_LeftStick_") && std::find(applied.begin(), applied.end(), k) == applied.end()) applied.push_back(k); binding.keys = std::move(applied); }
        } catch (...) {}
    }
    if (bindings_.empty()) throw std::runtime_error("No menu navigation actions were found in the input mapping");
}
bool Menu::typing() const { return has_focus(search_input_.Get()) || has_focus(name_input_.Get()); }
void Menu::poll_input(const PlayerContext& player, uint64_t now, bool typing) {
    auto* pc = player.pc; if (!pc) return;
    for (auto& b : bindings_) {
        bool down = false, allowed = !typing;
        for (const auto& name : b.keys) {
            const bool pad = name.starts_with("Gamepad_");
            if (pad != gamepad_ && input_prompt_.Get() && !typing) continue;
            if (key_down(pc, name)) { down = true; if (typing && (pad || name == "Escape")) allowed = true; break; }
        }
        const bool repeating = b.action == "up" || b.action == "down" || b.action == "left" || b.action == "right";
        const bool trigger = allowed && down && (!b.down || (repeating && now >= b.repeat));
        if (down && !b.down) b.repeat = now + 400;
        else if (trigger) b.repeat = now + 90;
        b.down = down;
        if (trigger) { key(b.action); return; }
    }
}
double Menu::along_bar(const SliderHit& slider, bool& inside) const {
    inside = false;
    auto* bar = slider.bar.Get(); if (!bar) return 0;
    Call geometry(bar, L"GetCachedGeometry", 1); geometry.run();
    Call pointer(find_cached(L"/Script/UMG.Default__WidgetLayoutLibrary"), L"GetMousePositionOnPlatform", 1); pointer.run();
    Call local(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"AbsoluteToLocal", 3);
    local.copy(L"Geometry", geometry, L"ReturnValue"); local.set(L"AbsoluteCoordinate", pointer.get<Vec2>()); local.run();
    Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"GetLocalSize", 2);
    size.copy(L"Geometry", geometry, L"ReturnValue"); size.run();
    const auto at = local.get<Vec2>(); const auto extent = size.get<Vec2>();
    if (extent.x < 1 || extent.y < 1) return 0;
    inside = at.x >= 0 && at.x <= extent.x && at.y >= -extent.y * .75 && at.y <= extent.y * 1.75;
    return std::clamp(at.x / extent.x, 0., 1.);
}
void Menu::poll_mouse(const PlayerContext& player) {
    if (!player.pc) return;
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    const bool left_now = (GetAsyncKeyState(swapped ? VK_RBUTTON : VK_LBUTTON) & 0x8000) != 0;
    const bool mouse_now = left_now || (GetAsyncKeyState(swapped ? VK_LBUTTON : VK_RBUTTON) & 0x8000) != 0;
    const bool mouse = mouse_now || mouse_was_down_;
    const bool left_pressed = left_now && !left_was_down_;
    const bool left_released = !left_now && left_was_down_;
    mouse_was_down_ = mouse_now; left_was_down_ = left_now;
    if (!mouse) { for (auto& hit : hits_) hit.down = false; drag_slider_ = -1; return; }
    if (left_pressed) {
        drag_slider_ = -1;
        for (size_t i = 0; i < sliders_.size(); ++i) { bool inside = false; along_bar(sliders_[i], inside); if (inside) { drag_slider_ = int(i); break; } }
    }
    if (drag_slider_ >= 0 && drag_slider_ < int(sliders_.size())) {
        auto& slider = sliders_[drag_slider_];
        if (left_now) {
            bool inside = false; const double fraction = along_bar(slider, inside);
            double v = slider.low + fraction * (slider.high - slider.low);
            if (slider.step > 0) v = slider.low + std::round((v - slider.low) / slider.step) * slider.step;
            v = std::clamp(v, slider.low, slider.high);
            if (std::abs(v - slider.previous) > 1e-9) {
                slider.previous = v;
                auto shown = slider.control; shown["value"] = v;
                if (auto* block = slider.value_block.Get()) text_value(block, display_value(shown) + slider.unit);
                if (auto* bar = slider.bar.Get()) invoke(bar, L"UpdateProgressBar", L"InPercent", float(slider.high > slider.low ? (v - slider.low) / (slider.high - slider.low) : 0));
            }
            return;
        }
        if (left_released) {
            const double v = slider.previous; drag_slider_ = -1;
            if (std::abs(v - slider.control.at("value").get<double>()) > 1e-9) act({{"action", "value"}, {"value", v}});
            return;
        }
        drag_slider_ = -1;
    }
    for (auto& hit : hits_) if (auto* widget = hit.widget.Get()) {
        Call pressed(widget, L"IsPressed", 1); pressed.run(); const bool down = pressed.get<bool>();
        const bool click = down && !hit.down; hit.down = down;
        if (!click) continue;
        if (!hit.parts.empty()) {
            Call pointer(find_cached(L"/Script/UMG.Default__WidgetLayoutLibrary"), L"GetMousePositionOnPlatform", 1); pointer.run();
            for (const auto& [part, action] : hit.parts) if (auto* glyph = part.Get()) {
                Call geometry(glyph, L"GetCachedGeometry", 1); geometry.run();
                Call under(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"IsUnderLocation", 3);
                under.copy(L"Geometry", geometry, L"ReturnValue"); under.set(L"AbsoluteCoordinate", pointer.get<Vec2>()); under.run();
                if (under.get<bool>()) { try { invoke(glyph, L"TriggerInputAnim"); } catch (...) {} act(action); return; }
            }
        }
        if (auto* glyph = hit.glyph.Get()) { try { invoke(glyph, L"TriggerInputAnim"); } catch (...) {} }
        act(hit.action); return;
    }
}
void Menu::tick(const PlayerContext& player, double) {
    const auto now = GetTickCount64();
    if (page_.Get() && (player.pc != pc_.Get() || !main_.Get() || !handler_.Get())) { if (deps_.log) deps_.log("CCS tab dropped with its Player Menu instance"); forget(); }
    if (!page_.Get()) {
        if (now < discover_after_) return; discover_after_ = now + 250;
        if (!player.pc) return;
        try { if (!attach(player)) return; } catch (const std::exception& e) { if (deps_.log) deps_.log(std::string("CCS tab attach failed: ") + e.what()); discover_after_ = now + 2000; return; }
    }
    auto* main = main_.Get(); auto* switcher = switcher_.Get(); if (!main || !switcher) { forget(); return; }
    const bool menu_open = bool_of(main, L"bOpen");
    if (!menu_open) {
        active_ = false; was_active_ = false; transition_started_ = 0;
        if (!frozen_listeners_.empty() || !dialog_shown_.empty()) { try { dialog_close(); } catch (...) {} }
        return;
    }
    Call selected(switcher, L"GetActiveWidget", 1); selected.run();
    active_ = selected.get<UObject*>() == page_.Get();
    if (active_ && !was_active_) {
        bindings_ready_ = false; bind_retry_ = 0;
        try { bind_inputs(); ++bindings_generation_; } catch (const std::exception& e) { if (deps_.log) deps_.log(std::string("Menu input binding failed: ") + e.what()); }
        for (auto& b : bindings_) { b.down = true; b.repeat = now + 400; }
        mouse_was_down_ = left_was_down_ = true; dirty_ = true; enter_ = true; error_.clear(); confirm_ = nullptr; picker_ = false; drag_slider_ = -1;
        refresh_model(true, now);
        invalidate_page();
    }
    if (!active_ && was_active_) { hits_.clear(); sliders_.clear(); transition_started_ = 0; }
    was_active_ = active_;
    if (!active_) {
        if (!frozen_listeners_.empty() || !dialog_shown_.empty()) { try { dialog_close(); } catch (...) {} }
        warm(now); return;
    }
    if (!bindings_ready_ && now >= bind_retry_) {
        bind_retry_ = now + 200;
        try {
            bind_inputs(); ++bindings_generation_;
            bindings_ready_ = std::any_of(bindings_.begin(), bindings_.end(), [](const Binding& b) { return !b.keys.empty(); });
            for (auto& b : bindings_) { b.down = true; b.repeat = now + 400; }
            if (bindings_ready_) dirty_ = true;
        } catch (...) {}
    }
    if (now >= layout_check_) {
        layout_check_ = now + 500;
        Call geometry(switcher, L"GetCachedGeometry", 1); geometry.run();
        Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"GetLocalSize", 2); size.copy(L"Geometry", geometry, L"ReturnValue"); size.run();
        const auto extent = size.get<Vec2>();
        if (std::abs(extent.x - viewport_[0]) > .5 || std::abs(extent.y - viewport_[1]) > .5) { viewport_ = {extent.x, extent.y}; dirty_ = true; }
    }
    if (auto* prompt = input_prompt_.Get()) { try { const bool gamepad = read<uint8_t>(prompt, L"InputType") == 1; if (gamepad != gamepad_) { gamepad_ = gamepad; dirty_ = true; } } catch (...) {} }
    if (now >= model_check_) { model_check_ = now + 250; refresh_model(false, now); }
    bool typing_now = false;
    if (auto* search = search_input_.Get()) {
        try { const auto query = text_of(search, 256); if (query != search_query_) { search_query_ = query; if (options_.filter(query)) dirty_ = true; } }
        catch (const std::exception& e) { error_ = e.what(); }
    }
    try { typing_now = typing(); } catch (...) {}
    typing_now_ = typing_now;
    fit_panel();
    reveal_pending();
    if (dirty_) { try { build(); } catch (const std::exception& e) { if (deps_.log) deps_.log(std::string("Menu build failed: ") + e.what()); error_ = e.what(); dirty_ = false; } }
    try { animate(now); } catch (const std::exception& e) { transition_started_ = 0; if (deps_.log) deps_.log(std::string("Menu transition failed: ") + e.what()); }
    if (!active_) return;
    if (picker_ && !typing_now && now >= wheel_after_) {
        try {
            Call wheel(player.pc, L"GetInputAnalogKeyState", 2); auto* key = wheel.param(L"Key");
            member(wheel.data(key), key->GetElementSize(), find_cached(L"/Script/InputCore.Key"), L"KeyName", key_name("MouseWheelAxis")); wheel.run();
            const float scroll = wheel.get<float>();
            if (std::abs(scroll) > .01f) { wheel_after_ = now + 100; options_.move(scroll < 0 ? 1 : -1); dirty_ = true; }
        } catch (...) {}
    }
    try { poll_input(player, now, typing_now); poll_mouse(player); }
    catch (const std::exception& e) { error_ = e.what(); dirty_ = true; }
}
// While the Player Menu is open on another tab the game is paused and the CCS page is not on
// screen: the expensive one-offs go here (skeleton, textures, the first pooled rows).
void Menu::warm(uint64_t now) {
    if (!page_.Get()) return;
    try {
        if (!design_.Get()) {
            if (now < layout_check_) return; layout_check_ = now + 500;
            Call geometry(switcher_.Get(), L"GetCachedGeometry", 1); geometry.run();
            Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"GetLocalSize", 2); size.copy(L"Geometry", geometry, L"ReturnValue"); size.run();
            const auto extent = size.get<Vec2>();
            if (extent.x < 320 || extent.y < 240) return;
            viewport_ = {extent.x, extent.y};
            page(extent.x, extent.y);
            return;
        }
        if (warm_texture_ < 1) { texture_at(deps_.root / "assets/logo.png"); ++warm_texture_; return; }
        if (now >= model_check_) { model_check_ = now + 250; refresh_model(false, now); }
        if (dirty_) build();
    } catch (const std::exception& e) { if (deps_.log) deps_.log(std::string("Menu warm-up failed: ") + e.what()); dirty_ = false; }
}
// The model is asked for at most four times a second while the page shows; a build happens
// only when it changed.
void Menu::refresh_model(bool force, uint64_t) {
    if (!deps_.model) { model_ = nullptr; return; }
    try {
        const auto revision = deps_.revision ? deps_.revision() : model_revision_ + 1;
        if (!force && revision == model_revision_) return;
        model_ = deps_.model(); model_revision_ = revision; dirty_ = true;
    } catch (const std::exception& e) { error_ = e.what(); dirty_ = true; }
}
const Json* Menu::current_control() const {
    if (!model_.is_object() || !model_.contains("sections")) return nullptr;
    const auto& sections = model_["sections"]; if (section_ < 0 || section_ >= int(sections.size())) return nullptr;
    const auto& controls = sections[section_]["controls"]; if (row_ < 0 || row_ >= int(controls.size())) return nullptr;
    return &controls[row_];
}
void Menu::send_event(const Json& event) {
    try { validate_event(model_, event); if (deps_.event) deps_.event(event); error_.clear(); }
    catch (const std::exception& e) { error_ = e.what(); }
    refresh_model(true, GetTickCount64()); dirty_ = true;
}
void Menu::key(const std::string& action) {
    if (!confirm_.is_null()) {
        if (action == "accept") act({{"action", "confirm"}});
        else if (action == "close") act({{"action", "cancel"}});
        else if (action == "left" || action == "right" || action == "up" || action == "down") { dialog_focus_ = action == "left" || action == "up" ? 0 : 1; dirty_ = true; }
        return;
    }
    if (picker_) {
        if (action == "close") act({{"action", "pick_cancel"}});
        else if (action == "up" || action == "down" || action == "previous_section" || action == "next_section") { options_.move(action == "up" ? -1 : action == "down" ? 1 : action == "previous_section" ? -8 : 8); dirty_ = true; }
        else if (action == "accept") act({{"action", "pick_apply"}});
        return;
    }
    if (model_.is_object() && model_.contains("sections") && section_ < int(model_["sections"].size()) && model_["sections"][section_].value("kind", std::string{}) == "slots") {
        if (action == "search") { act({{"action", "search"}}); return; }
        if (action == "close" && typing_now_) {   // Escape in the search field clears it instead of leaving the page
            search_query_.clear(); if (auto* search = search_input_.Get()) { try { text_value(search, ""); } catch (...) {} }
            try { options_.filter(""); } catch (...) {}
            dirty_ = true; return;
        }
        if (action == "previous_section" || action == "next_section") { act({{"action", "section_delta"}, {"delta", action == "previous_section" ? -1 : 1}}); return; }
        if (action == "left" || action == "right") { act({{"action", "slot_delta"}, {"delta", action == "left" ? -1 : 1}}); return; }
        if (action == "up" || action == "down") { act({{"action", "cand_delta"}, {"delta", action == "up" ? -1 : 1}}); return; }
        if (action == "accept") { act({{"action", "assign"}}); return; }
        if (action == "secondary") { act({{"action", "clear"}}); return; }
        if (action == "close") { close(); return; }
        return;
    }
    if (action == "close") { close(); return; }   // like CSS: the page closes the Player Menu itself
    if (action == "secondary") { if (model_.is_object() && model_.contains("notice") && model_["notice"].is_object()) act({{"action", "run"}, {"id", model_["notice"].value("action", std::string{})}}); return; }
    if (action == "previous_section" || action == "next_section") { act({{"action", "section_delta"}, {"delta", action == "previous_section" ? -1 : 1}}); return; }
    if (action == "up" || action == "down") { act({{"action", "row_delta"}, {"delta", action == "up" ? -1 : 1}}); return; }
    if (action == "left" || action == "right") { act({{"action", "adjust"}, {"delta", action == "left" ? -1 : 1}}); return; }
    if (action == "accept") act({{"action", "activate"}});
}
void Menu::act(const Json& action) {
    const auto name = action.value("action", std::string{});
    if (name != "value") error_.clear();
    if (name == "press") { key(action.value("binding", std::string{})); return; }
    if (name == "close") { close(); return; }
    if (picker_) {
        if (name == "pick_cancel") { picker_ = false; dirty_ = true; return; }
        if (name == "pick_row") { options_.selected = std::min(action.at("row").get<size_t>(), options_.matches.empty() ? size_t{} : options_.matches.size() - 1); dirty_ = true; return; }
        if (name == "pick_apply") {
            const auto value = options_.value(); if (value.is_null()) return;
            const auto* c = current_control(); if (!c) return;
            Json event = {{"id", c->at("id")}, {"value", value}};
            picker_ = false;
            if (c->contains("confirm")) { confirm_ = {{"event", event}, {"message", c->at("confirm")}}; dialog_focus_ = 0; }
            else send_event(event);
            dirty_ = true; return;
        }
        return;
    }
    if (!confirm_.is_null()) {
        if (name == "confirm") { auto event = confirm_.at("event"); event["confirmed"] = true; confirm_ = nullptr; try { dialog_close(); } catch (...) {} send_event(event); }
        else if (name == "cancel") { confirm_ = nullptr; try { dialog_close(); } catch (...) {} dirty_ = true; }
        return;
    }
    if (name == "run" && model_.is_object()) {
        const auto id = action.value("id", std::string{});
        for (const auto& section : model_.value("sections", Json::array())) for (const auto& c : section.value("controls", Json::array())) if (c.value("id", std::string{}) == id) {
            if (!interactive(c)) { error_ = "That action is not available right now."; dirty_ = true; return; }
            Json event = {{"id", id}};
            if (c.contains("confirm")) { confirm_ = {{"event", event}, {"message", c.at("confirm")}}; dialog_focus_ = 0; dirty_ = true; }
            else send_event(event);
            return;
        }
        return;
    }
    if (!model_.is_object() || !model_.contains("sections")) return;
    const auto& sections = model_["sections"]; const int count = int(sections.size());
    if (name == "section" || name == "section_delta") {
        if (count) section_ = name == "section" ? std::clamp(action.at("section").get<int>(), 0, count - 1) : (section_ + action.at("delta").get<int>() + count) % count;
        row_ = 0; dirty_ = true; enter_ = true; return;
    }
    if (!count) return;
    const auto& controls = sections[section_]["controls"]; const int rows = int(controls.size());
    if (name == "row_delta") { row_ = std::clamp(row_ + action.at("delta").get<int>(), 0, std::max(0, rows - 1)); dirty_ = true; return; }
    if (name == "row") { row_ = std::clamp(action.at("row").get<int>(), 0, std::max(0, rows - 1)); dirty_ = true; return; }
    // ---- the slot grid (a section of kind "slots": each control is a slot, its options the candidates)
    // Candidates are searched: options_ holds the active slot's list, options_.matches the rows
    // shown for the query and options_.selected the highlighted row. The build resets options_
    // whenever the slot changes (slot_options_key_).
    if (name == "search") {   // put the caret in the search field; typing then filters the list
        if (auto* search = search_input_.Get()) { try { Call focus(search, L"SetKeyboardFocus", 0); focus.run(); } catch (const std::exception& e) { error_ = e.what(); dirty_ = true; } }
        return;
    }
    if (name == "slot" || name == "slot_delta") {
        const int wanted = name == "slot" ? action.at("index").get<int>() : row_ + action.at("delta").get<int>();
        row_ = rows ? (wanted % rows + rows) % rows : 0;
        slot_options_key_.clear();   // the build reloads the candidates and lands on the assigned one
        dirty_ = true; return;
    }
    if (name == "cand_delta" || name == "cand") {
        if (name == "cand") options_.selected = std::min(action.at("index").get<size_t>(), options_.matches.empty() ? size_t{} : options_.matches.size() - 1);
        else options_.move(action.at("delta").get<int>());
        dirty_ = true; return;
    }
    if (name == "assign" || name == "clear") {
        const auto* c = current_control(); if (!c) return;
        if (name == "assign" && action.contains("index")) options_.selected = std::min(action.at("index").get<size_t>(), options_.matches.empty() ? size_t{} : options_.matches.size() - 1);
        Json value = "";
        if (name == "assign") {
            if (options_.matches.empty()) return;
            const auto& option = options_.options.at(options_.matches.at(options_.selected));
            if (!option.value("enabled", true)) { error_ = option.value("disabled_label", std::string("Not available")); dirty_ = true; return; }
            value = option.at("id");
        }
        send_event({{"id", c->at("id")}, {"value", value}}); return;
    }
    const auto* c = current_control(); if (!c || !interactive(*c)) return;
    const auto type = c->at("type").get<std::string>();
    if (type == "choice" && (name == "pick" || (name == "activate" && c->at("options").size() > 8))) {
        options_.reset(c->at("options"), [](const std::string& text) {
            const auto source = wide(text);
            const int length = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, source.data(), int(source.size()), nullptr, 0, nullptr, nullptr, 0);
            if (length <= 0) return OptionSearch::ascii_fold(text);
            std::wstring result(size_t(length), L'\0');
            if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, source.data(), int(source.size()), result.data(), length, nullptr, nullptr, 0)) return OptionSearch::ascii_fold(text);
            return narrow(result);
        });
        for (size_t i = 0; i < options_.matches.size(); ++i) if (options_.options[i].at("id") == c->at("value")) options_.selected = i;
        search_query_.clear(); picker_ = true; dirty_ = true; return;
    }
    Json event = {{"id", c->at("id")}};
    if (name == "value" || name == "text") {
        if (name == "text" && type == "text") event["value"] = text_of(name_input_.Get(), 4096);
        else if (name == "value" && (type == "radio" || type == "choice" || type == "slider" || type == "number")) event["value"] = (type == "slider" || type == "number") ? Json(snap_value(*c, action.at("value").get<double>())) : action.at("value");
        else return;
    } else if (name == "activate" || name == "adjust") {
        if (type == "toggle") event["value"] = !c->at("value").get<bool>();
        else if (adjustable(*c) && (name == "adjust" || type == "choice" || type == "radio")) event["value"] = adjusted_value(*c, action.value("delta", 1));
        else if (type == "text" && name == "activate") event["value"] = text_of(name_input_.Get(), 4096);
        else if (type != "button" || name != "activate") return;
    } else return;
    if (c->contains("confirm")) { confirm_ = {{"event", event}, {"message", c->at("confirm")}}; dialog_focus_ = 0; dirty_ = true; return; }
    send_event(event);
}
Json Menu::diagnostics() const {
    Json bindings = Json::object(); for (const auto& b : bindings_) bindings[b.action] = b.keys;
    int page_widgets = 0, nested = 0;
    for (const auto* stack : {&tab_items_, &list_, &head_, &panel_, &actions_, &footer_}) { page_widgets += int(stack->used); for (const auto& cell : stack->cells) nested += int(cell.kinds.size()); }
    return {{"bindings", bindings}, {"open", active_}, {"attached", page_.Get() != nullptr}, {"skeleton", design_.Get() != nullptr}, {"tab_index", tab_index_},
            {"section", section_}, {"row", row_}, {"picker", picker_}, {"confirm", !confirm_.is_null()},
            {"error", error_}, {"hits", hits_.size()}, {"widgets", cost_.widgets}, {"page_widgets", page_widgets}, {"nested_widgets", nested}, {"created", cost_.created},
            {"builds", cost_.builds}, {"last_build_us", cost_.last_build_us}, {"max_build_us", cost_.max_build_us}, {"viewport", viewport_}, {"gamepad", gamepad_}, {"textures", textures_.size()}};
}
}

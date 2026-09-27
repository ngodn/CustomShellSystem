#include "menu.hpp"
#include "menu_keys.hpp"
#include "tab_order.hpp"
#include <algorithm>
#include <utility>
#include <cmath>
#include <windows.h>

namespace ccs {
using namespace engine;

Menu::Menu(Deps deps) : deps_(std::move(deps)) {
    if (deps_.movesets) save_name_ = deps_.movesets->active_preset().name;
}

Menu::~Menu() = default;

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

    const auto now = GetTickCount64();
    if (page_list.size() != tab_list.size() || page_list.size() < 3 || page_list.size() >= 64) return false;
    if (tab_list.front() != original) throw std::runtime_error("Player Menu native order is unsupported");
    const auto expected = 3u + static_cast<unsigned>(deps_.css_present) + static_cast<unsigned>(deps_.cssx_present);
    if (page_list.size() < expected) {
        if (!attach_wait_since_) attach_wait_since_ = now;
        if (now - attach_wait_since_ < 3000) return false;
        if (deps_.log) deps_.log("CCS: proceeding after another menu tab did not attach within three seconds");
    }
    attach_wait_since_ = 0;

    struct Construction {
        std::vector<UObject*> roots;
        UObject* tab{};
        UObject* page{};
        bool committed{};
        Construction() { roots.reserve(3); }
        void hold(UObject* object) {
            if (!object) throw std::runtime_error("Player Menu widget construction failed");
            if (!object->IsRootSet()) { roots.push_back(object); object->SetRootSet(); }
        }
        ~Construction() {
            if (!committed) {
                for (auto* widget : {page, tab}) {
                    if (widget) { try { invoke(widget, L"RemoveFromParent"); } catch (...) {} }
                }
            }
            for (auto* object : roots) object->ClearRootSet();
        }
    } construction;

    auto* tab = create_widget(player.pc, original->GetClassPrivate());
    construction.tab = tab; construction.hold(tab);
    for (auto name : {L"FontData", L"RootSize", L"RootScale", L"DefaultColor", L"SelectedColor", L"bUseHighlight", L"HighlightY"}) {
        copy_property(tab, original, name);
    }
    text_property(tab, L"Text", "CCS");
    invoke(tab, L"UpdateText");
    invoke(tab, L"CommitSize");
    invoke(tab, L"CommitScale");

    auto* page = create_widget(player.pc, static_cast<UClass*>(main->GetClassPrivate()->GetSuperStruct()));
    construction.page = page; construction.hold(page);
    auto* tree = object_of(page, L"WidgetTree");
    if (!tree) return false;
    auto* canvas = construct(L"/Script/UMG.CanvasPanel", tree);
    construction.hold(canvas);
    object_property(tree, L"RootWidget", canvas);

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

    try { order_tabs(); }
    catch (...) { forget(); throw; }
    construction.committed = true;

    if (deps_.log) deps_.log("CCS: Attached tab to Player Menu at index " + std::to_string(tab_index_));
    dirty_ = true;
    return true;
}

void Menu::order_tabs() {
    auto tabs = children(tabs_.Get());
    auto pages = children(switcher_.Get());
    if (tabs.size() < 4 || tabs.size() != pages.size() || tabs.size() > 64)
        throw std::runtime_error("Player Menu tab/page count is inconsistent");
    using Role = runtime::TabRole;
    std::vector<Role> roles(tabs.size(), Role::Other);
    roles[0] = Role::Inventory; roles[1] = Role::Tarstones; roles[2] = Role::Map;
    for (size_t i = 3; i < tabs.size(); ++i) {
        if (tabs[i] == tab_.Get()) roles[i] = Role::Ccs;
        else {
            const auto title = text_property_string(tabs[i], L"Text", 64);
            if (title == "CSS") roles[i] = Role::Css;
            else if (title == "CSSX") roles[i] = Role::Cssx;
        }
    }
    const auto plan = runtime::player_menu_order(roles);
    const auto own = std::find(tabs.begin(), tabs.end(), tab_.Get());
    if (own == tabs.end() || pages[static_cast<size_t>(own - tabs.begin())] != page_.Get())
        throw std::runtime_error("CCS tab and page are no longer paired");
    std::vector<UObject*> ordered_tabs, ordered_pages;
    ordered_tabs.reserve(tabs.size()); ordered_pages.reserve(pages.size());
    for (const auto index : plan.indices) {
        ordered_tabs.push_back(tabs[index]); ordered_pages.push_back(pages[index]);
    }
    if (tabs == ordered_tabs && pages == ordered_pages) {
        tab_index_ = static_cast<int>(plan.ccs_index);
        return;
    }
    Call selected(switcher_.Get(), L"GetActiveWidget", 1); selected.run();
    auto* selected_page = selected.get<UObject*>();
    reorder(switcher_.Get(), ordered_pages);
    try { reorder(tabs_.Get(), ordered_tabs); }
    catch (...) { try { reorder(switcher_.Get(), pages); } catch (...) {} throw; }
    tab_index_ = static_cast<int>(plan.ccs_index);
    nav_children_refresh(tabs_.Get());
    if (selected_page) {
        const auto current = std::find(ordered_pages.begin(), ordered_pages.end(), selected_page);
        if (current != ordered_pages.end()) navigate(static_cast<int>(current - ordered_pages.begin()));
    }
}

void Menu::navigate(int index) {
    auto* tabs = tabs_.Get();
    if (!tabs || index < 0) throw std::runtime_error("Player Menu navigation target is unavailable");
    Call nav(tabs, L"NavigateToCustomIndex", 3); nav.set(L"Index", int32_t{index}); nav.run();
    if (!nav.get<bool>(L"Success")) throw std::runtime_error("Player Menu rejected tab navigation");
}

bool Menu::open(const PlayerContext& player) {
    if (!player.pc) return false;
    auto* handler = object_of(player.pc, L"User Interface Handler Component");
    if (!handler) return false;
    if (active_) return true;
    open_requested_ = true;
    open_requested_at_ = GetTickCount64();
    discover_after_ = 0;
    auto* main = object_of(object_of(handler, L"WBP_Menu_Game"), L"WBP_Menu_Main");
    if (!main || !bool_of(main, L"bOpen")) {
        Call open_call(handler, L"HandleGameMenu", 2);
        open_call.set(L"SubTabIndex", int32_t{0}); open_call.set(L"AllowClose", false); open_call.run();
    }
    return true;
}

void Menu::close() {
    clear_save_confirmation(); pending_delete_.clear();
    if (!main_.Get() || !bool_of(main_.Get(), L"bOpen")) return;
    auto* handler = handler_.Get();
    if (!handler) return;

    Call close_call(handler, L"HandleGameMenu", 2);
    close_call.set(L"SubTabIndex", int32_t{0});
    close_call.set(L"AllowClose", true);
    close_call.run();
    active_ = false;
    open_requested_ = false;
    entered_ = false;
}

void Menu::forget() {
    for (auto& pool : page_pools_) pool.reset();
    save_pool_.reset();
    save_name_input_.Reset(); save_button_.Reset(); replace_button_.Reset(); keep_button_.Reset();
    save_editor_ready_ = false; focus_name_requested_ = false; save_buttons_down_.fill(false);
    clear_save_confirmation();
    page_extent_ = {}; layout_check_after_ = 0;
    page_.Reset(); tab_.Reset(); tree_.Reset(); canvas_.Reset(); main_.Reset();
    tabs_.Reset(); switcher_.Reset(); handler_.Reset(); pc_.Reset();
    pending_delete_.clear();
    input_.reset(); input_ready_ = false; input_check_after_ = 0;
    ++generation_;
    active_ = false; entered_ = false; tab_index_ = -1; attach_wait_since_ = 0;
    discover_after_ = 0; order_check_after_ = 0; dirty_ = true;
}

void Menu::detach() {
    open_requested_ = false;
    if (active_ && main_.Get() && bool_of(main_.Get(), L"bOpen")) { try { navigate(0); } catch (...) {} }
    if (auto* page = page_.Get()) { try { invoke(page, L"RemoveFromParent"); } catch (...) {} }
    if (auto* tab = tab_.Get()) { try { invoke(tab, L"RemoveFromParent"); } catch (...) {} }
    if (tabs_.Get()) { try { nav_children_refresh(tabs_.Get()); } catch (...) {} }
    forget();
}

void Menu::tick(const PlayerContext& player, double /*delta*/) {
    if (GetTickCount64() < retry_after_) return;
    try {
        process_files(player);
        tick_impl(player);
        last_error_.clear();
    } catch (const std::exception& error) {
        retry_after_ = GetTickCount64() + 1000;
        active_ = false;
        if (last_error_ != error.what()) {
            last_error_ = error.what();
            if (deps_.log) deps_.log("CCS: Player Menu paused: " + last_error_);
        }
    }
}

void Menu::tick_impl(const PlayerContext& player) {
    const auto now = GetTickCount64();
    if (open_requested_ && now - open_requested_at_ > 5000) {
        open_requested_ = false;
        if (deps_.log) deps_.log("CCS: Player Menu open request timed out");
    }
    if (page_.Get() && now >= generation_check_after_) {
        generation_check_after_ = now + 500;
        auto* handler = object_of(player.pc, L"User Interface Handler Component");
        auto* main = object_of(object_of(handler, L"WBP_Menu_Game"), L"WBP_Menu_Main");
        if (pc_.Get() != player.pc || handler != handler_.Get() || main != main_.Get() ||
            !tabs_.Get() || !switcher_.Get() || !tab_.Get()) {
            const auto pending = open_requested_;
            detach();
            open_requested_ = pending;
        }
    }
    if (!page_.Get()) {
        if (now < discover_after_) return;
        discover_after_ = now + 250;
        if (!attach(player)) return;
    }
    auto* main = main_.Get(); auto* pages = switcher_.Get();
    if (!main || !pages) { forget(); return; }
    if (!bool_of(main, L"bOpen")) {
        active_ = false; entered_ = false; pending_delete_.clear(); clear_save_confirmation(); return;
    }
    if (now >= order_check_after_) {
        order_check_after_ = now + 500;
        order_tabs();
    }
    if (open_requested_) { navigate(tab_index_); open_requested_ = false; }
    Call selected(pages, L"GetActiveWidget", 1); selected.run();
    active_ = selected.get<UObject*>() == page_.Get();
    if (!active_ && entered_) { pending_delete_.clear(); clear_save_confirmation(); }
    if (active_ && !entered_) {
        dirty_ = true; refresh_presets_ = true;
        input_.reset(); input_ready_ = false; input_check_after_ = 0;
        for (auto& pool : page_pools_) pool.invalidate();
        save_pool_.invalidate();
        layout_check_after_ = 0;
    }
    entered_ = active_;
    if (active_ && now >= input_check_after_) {
        try {
            const auto revision = input_.revision();
            input_ready_ = input_.refresh(player.pc, handler_.Get());
            if (input_.revision() != revision) dirty_ = true;
            input_error_.clear();
        } catch (const std::exception& error) {
            input_.reset(); input_ready_ = false;
            if (input_error_ != error.what()) {
                input_error_ = error.what();
                if (deps_.log) deps_.log("CCS: menu input unavailable: " + input_error_);
            }
        }
        input_check_after_ = now + (input_ready_ ? 2000 : 200);
    }
    if (active_ && input_ready_) {
        if (const auto action = input_.poll(now, save_field_focused())) {
            static constexpr std::array<const char*, runtime::menu_action_count> names{
                NavAction::Cancel, NavAction::Accept, NavAction::Unequip, NavAction::Details,
                NavAction::TabLeft, NavAction::TabRight, NavAction::Up, NavAction::Down, NavAction::Left, NavAction::Right
            };
            on_key(names[static_cast<size_t>(*action)]);
        }
    }
    if (active_ && current_screen_ == MenuScreen::Preset && save_editor_ready_) poll_save_buttons();
    if (active_) measure_page(now);
    if (active_ && dirty_) dirty_ = !rebuild_page(player);
}

bool Menu::request_file(runtime::FileOperation operation, std::string name, std::string payload, bool replace_existing) {
    if (!deps_.persistence || action_request_) {
        file_status_ = "Another preset operation is still pending";
        return false;
    }
    action_request_ = deps_.persistence->submit({operation, std::move(name), std::move(payload), replace_existing});
    if (!action_request_) { file_status_ = "Preset request rejected or file worker is full"; return false; }
    action_generation_ = generation_;
    file_error_operation_.reset();
    file_status_ = "Preset operation pending";
    return true;
}

bool Menu::save_preset(const std::string& name) {
    if (!deps_.movesets) return false;
    if (!runtime::valid_preset_name(name)) {
        file_status_ = "Use 1-96 letters, numbers, dots, hyphens or underscores. Reserved filenames are unavailable.";
        dirty_ = true; return false;
    }
    auto preset = deps_.movesets->active_preset();
    preset.name = name;
    auto payload = runtime::Storage::preset_to_json(preset).dump();
    const bool queued = request_file(runtime::FileOperation::SavePreset, name, payload);
    if (queued) { save_name_ = name; save_payload_ = std::move(payload); overwrite_name_.clear(); }
    dirty_ = true;
    return queued;
}

bool Menu::save_field_focused() const {
    auto* input = save_name_input_.Get();
    if (current_screen_ != MenuScreen::Preset || !input || !overwrite_name_.empty() || !pending_delete_.empty()) return false;
    Call focus(input, L"HasKeyboardFocus", 1); focus.run(); return focus.get<bool>();
}
void Menu::save_from_field() {
    if (!save_editor_ready_ || action_request_) return;
    try {
        save_name_ = text_of(save_name_input_.Get(), 97);
        save_preset(save_name_);
    } catch (const std::exception&) {
        file_status_ = "Preset name could not be read. Use at most 96 filename characters."; dirty_ = true;
    }
}
void Menu::clear_save_confirmation() {
    overwrite_name_.clear(); save_payload_.clear(); overwrite_focus_ = 1;
    save_buttons_down_.fill(false);
    save_mouse_suppressed_ = true;
}
void Menu::confirm_file_action(bool confirm) {
    if (!confirm) {
        clear_save_confirmation(); pending_delete_.clear(); file_status_ = "Cancelled";
    } else if (!pending_delete_.empty()) {
        if (request_file(runtime::FileOperation::DeletePreset, pending_delete_)) pending_delete_.clear();
    } else if (!overwrite_name_.empty() && !save_payload_.empty()) {
        if (request_file(runtime::FileOperation::SavePreset, overwrite_name_, save_payload_, true)) overwrite_name_.clear();
    }
    dirty_ = true;
}
void Menu::poll_save_buttons() {
    const bool confirming = !overwrite_name_.empty() || !pending_delete_.empty();
    const std::array<WeakObject, 3> buttons{save_button_, replace_button_, keep_button_};
    std::array<bool, 3> sample{};
    for (size_t i = 0; i < buttons.size(); ++i) {
        if ((i == 0) == confirming) continue;
        auto* widget = buttons[i].Get();
        if (!widget) continue;
        Call pressed(widget, L"IsPressed", 1); pressed.run();
        sample[i] = pressed.get<bool>();
    }
    const auto previous = save_buttons_down_; save_buttons_down_ = sample;
    if (save_mouse_suppressed_) {
        if (std::none_of(sample.begin(), sample.end(), [](bool down) { return down; })) save_mouse_suppressed_ = false;
        return;
    }
    for (const auto i : {2u, 1u, 0u}) {
        const bool released = previous[i] && !sample[i];
        if (!released) continue;
        auto* widget = buttons[i].Get();
        if (!widget) continue;
        Call geometry(widget, L"GetCachedGeometry", 1); geometry.run();
        Call pointer(find(L"/Script/UMG.Default__WidgetLayoutLibrary"), L"GetMousePositionOnPlatform", 1); pointer.run();
        Call inside(find(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"IsUnderLocation", 3);
        auto* target_geometry = inside.param(L"Geometry"); auto* target_point = inside.param(L"AbsoluteCoordinate");
        auto* source_geometry = geometry.param(L"ReturnValue"); auto* source_point = pointer.param(L"ReturnValue");
        if (!target_geometry->SameType(source_geometry) || !target_point->SameType(source_point))
            throw std::runtime_error("Native button hit-test layout changed");
        target_geometry->CopyCompleteValue(inside.data(target_geometry), geometry.data(source_geometry));
        target_point->CopyCompleteValue(inside.data(target_point), pointer.data(source_point));
        inside.run();
        if (!inside.get<bool>()) continue;
        if (i == 0) save_from_field(); else confirm_file_action(i == 1);
        return;
    }
}

void Menu::process_files(const PlayerContext& player) {
    if (!deps_.persistence) return;
    if (list_request_ || action_request_ || settings_request_) {
        for (size_t i = 0; i < 8; ++i) {
            auto result = deps_.persistence->poll();
            if (!result) break;
            if (list_request_ == result->id) {
                list_request_.reset();
                if (result->success) {
                    std::string selected;
                    if (preset_index_ >= 0 && preset_index_ < static_cast<int>(cached_preset_names_.size()))
                        selected = cached_preset_names_[preset_index_];
                    cached_preset_names_ = std::move(result->names);
                    const auto found = std::find(cached_preset_names_.begin(), cached_preset_names_.end(), selected);
                    preset_index_ = found != cached_preset_names_.end() ? static_cast<int>(found - cached_preset_names_.begin()) : 0;
                }
            } else if (action_request_ == result->id) {
                action_request_.reset();
                if (result->operation == runtime::FileOperation::SavePreset && result->exists &&
                    action_generation_ == generation_ && active_ && current_screen_ == MenuScreen::Preset &&
                    !save_payload_.empty()) {
                    overwrite_name_ = result->name; overwrite_focus_ = 1; save_buttons_down_.fill(false);
                    save_mouse_suppressed_ = true; input_.suppress_held();
                    file_status_ = "Choose Replace or Cancel";
                } else if (result->operation == runtime::FileOperation::SavePreset) clear_save_confirmation();
                if (result->success && result->operation == runtime::FileOperation::LoadPreset && result->preset) {
                    auto* handler = object_of(player.pc, L"User Interface Handler Component");
                    auto* main = object_of(object_of(handler, L"WBP_Menu_Game"), L"WBP_Menu_Main");
                    if (action_generation_ == generation_ && pc_.Get() == player.pc &&
                        handler_.Get() == handler && main_.Get() == main && deps_.movesets) {
                        deps_.movesets->set_active_preset(std::move(*result->preset));
                        if (deps_.settings) { deps_.settings->set_startup_preset(result->name); settings_dirty_ = true; }
                    } else {
                        result->success = false; result->error = "Player Menu changed before preset load completed";
                    }
                }
                if (result->success && (result->operation == runtime::FileOperation::DeletePreset ||
                    result->operation == runtime::FileOperation::SavePreset)) refresh_presets_ = true;
            } else if (settings_request_ == result->id) settings_request_.reset();
            const bool recovered = result->success && file_error_operation_ == result->operation;
            if (!result->success && !result->exists) file_error_operation_ = result->operation;
            else if (recovered) file_error_operation_.reset();
            if (pending_delete_.empty() && overwrite_name_.empty() && (!result->success || (!file_error_operation_ &&
                (result->operation != runtime::FileOperation::ListPresets || recovered))))
                file_status_ = result->success ? "File operation completed" : result->error;
            if (!result->success && !result->exists && deps_.log) deps_.log("CCS: " + result->error);
            dirty_ = true;
        }
    }
    if (refresh_presets_ && !list_request_) {
        list_request_ = deps_.persistence->submit({runtime::FileOperation::ListPresets, {}, {}});
        if (list_request_) refresh_presets_ = false;
    }
    if (settings_dirty_ && !settings_request_ && deps_.settings) {
        settings_request_ = deps_.persistence->submit({runtime::FileOperation::SaveSettings, {}, deps_.settings->to_json().dump()});
        if (settings_request_) settings_dirty_ = false;
    }
}

void Menu::measure_page(uint64_t now) {
    if (now < layout_check_after_ || !switcher_.Get()) return;
    layout_check_after_ = now + 500;
    Call geometry(switcher_.Get(), L"GetCachedGeometry", 1); geometry.run();
    Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"), L"GetLocalSize", 2);
    auto* source = geometry.param(L"ReturnValue"); auto* target = size.param(L"Geometry");
    if (!source->SameType(target) || source->GetElementSize() != target->GetElementSize())
        throw std::runtime_error("Menu geometry type changed");
    target->CopyCompleteValue(size.data(target), geometry.data(source)); size.run();
    const auto extent = size.get<Vec2>();
    if (!std::isfinite(extent.x) || !std::isfinite(extent.y) || extent.x < 320 || extent.y < 240) return;
    if (std::abs(extent.x - page_extent_.x) > .5 || std::abs(extent.y - page_extent_.y) > .5) {
        page_extent_ = extent; dirty_ = true;
    }
}

bool Menu::rebuild_page(const PlayerContext& /*player*/) {
    auto* canvas = canvas_.Get();
    auto* tree = tree_.Get();
    if (!canvas || !tree) return false;
    const auto screen = static_cast<size_t>(current_screen_);
    if (!page_pools_[screen].begin(canvas, tree, tab_.Get(), page_extent_)) return false;
    for (size_t i = 0; i < page_pools_.size(); ++i) if (i != screen) page_pools_[i].show(false);

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
    const bool complete = page_pools_[screen].finish();
    if (current_screen_ == MenuScreen::Preset) return render_save_editor() && complete;
    save_pool_.show(false); save_editor_ready_ = false;
    return complete;
}

void Menu::on_key(const std::string& action) {
    if (!active_) return;
    if (!overwrite_name_.empty() || !pending_delete_.empty()) {
        if (action == NavAction::Cancel) confirm_file_action(false);
        else if (action == NavAction::Accept) confirm_file_action(overwrite_focus_ == 0);
        else if (action == NavAction::Left || action == NavAction::Up) { overwrite_focus_ = 0; dirty_ = true; }
        else if (action == NavAction::Right || action == NavAction::Down) { overwrite_focus_ = 1; dirty_ = true; }
        return;
    }
    if (action == NavAction::Cancel && pending_delete_.empty() &&
        !(current_screen_ == MenuScreen::Customize && focus_area_ == FocusArea::LeftAccordion)) {
        close(); return;
    }

    if (action == NavAction::TabLeft) {
        pending_delete_.clear();
        clear_save_confirmation(); focus_name_requested_ = false;
        int s = static_cast<int>(current_screen_);
        s = (s - 1 + 3) % 3;
        current_screen_ = static_cast<MenuScreen>(s);
        dirty_ = true;
        return;
    }
    if (action == NavAction::TabRight) {
        pending_delete_.clear();
        clear_save_confirmation(); focus_name_requested_ = false;
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
                const auto count = !deps_.movesets ? size_t{0} : accordion_section_ == 1 ?
                    deps_.movesets->get_tarstones_for_slot(selected_slot_).size() :
                    deps_.movesets->get_moves_for_slot(selected_slot_, accordion_section_ == 2).size();
                if (accordion_item_ + 1 < static_cast<int>(count)) ++accordion_item_;
                else if (accordion_section_ < 2) { ++accordion_section_; accordion_item_ = 0; }
                dirty_ = true;
            } else if (action == NavAction::Accept) {
                // Apply selection to active slot
                if (deps_.movesets) {
                    if (accordion_section_ == 1) { // Tarstones
                        auto stones = deps_.movesets->get_tarstones_for_slot(selected_slot_);
                        if (accordion_item_ >= 0 && accordion_item_ < static_cast<int>(stones.size())) {
                            deps_.movesets->assign_tarstone_to_slot(selected_slot_, *stones[accordion_item_]);
                        }
                    } else { // Moves
                        auto moves = deps_.movesets->get_moves_for_slot(selected_slot_, accordion_section_ == 2);
                        if (accordion_item_ >= 0 && accordion_item_ < static_cast<int>(moves.size())) {
                            deps_.movesets->assign_move_to_slot(selected_slot_, *moves[accordion_item_]);
                        }
                    }
                }
                focus_area_ = FocusArea::SlotGrid;
                dirty_ = true;
            }
        }
    } else if (current_screen_ == MenuScreen::Preset) {
        if (action == NavAction::Details) {
            focus_name_requested_ = true; dirty_ = true;
        } else if (action == NavAction::Up) {
            if (preset_index_ > 0) { preset_index_--; dirty_ = true; }
        } else if (action == NavAction::Down) {
            if (preset_index_ + 1 < static_cast<int>(cached_preset_names_.size())) { preset_index_++; dirty_ = true; }
        } else if (action == NavAction::Accept) {
            bool save_focused = false;
            if (auto* button = save_button_.Get(); button && save_editor_ready_) {
                Call focus(button, L"HasKeyboardFocus", 1); focus.run(); save_focused = focus.get<bool>();
            }
            if (save_focused) save_from_field();
            else if (deps_.movesets && preset_index_ >= 0 && preset_index_ < static_cast<int>(cached_preset_names_.size()))
                request_file(runtime::FileOperation::LoadPreset, cached_preset_names_[preset_index_]);
            dirty_ = true;
        } else if (action == NavAction::Unequip) {
            if (!action_request_ && preset_index_ >= 0 && preset_index_ < static_cast<int>(cached_preset_names_.size())) {
                pending_delete_ = cached_preset_names_[preset_index_];
                overwrite_focus_ = 1; save_buttons_down_.fill(false);
                save_mouse_suppressed_ = true; input_.suppress_held();
                file_status_ = "Delete " + pending_delete_ + "?";
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
                    file_status_ = "Vanilla reset is unavailable until live weapon bindings are verified";
                    if (deps_.log) deps_.log("CCS: " + file_status_);
                }
                if (settings_index_ < 3) {
                    settings_dirty_ = true; file_error_operation_.reset(); file_status_ = "Settings save pending";
                }
            }
            dirty_ = true;
        }
    }
}

} // namespace ccs

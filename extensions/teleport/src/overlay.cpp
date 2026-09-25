#include "teleport.hpp"

// The on-map prompt is the game's own widget. We instantiate a WBP_PromptWithText
// (the same class the map's Back/Move/Zoom prompts use), set its data to the
// Traverse action with the T / L3 glyphs, and add it to the map's prompt bar
// (HB_Prompts). The game rebuilds that bar on hover/input changes, which unparents
// our widget; scan() re-adds it, so it tracks the hovered point. No custom drawing.

namespace teleport {

namespace {
bool is_object(const Json& j) { return j.is_object() && j.contains("$object"); }
constexpr int KBM_T = 52;              // T_UI_Prompt_KBM_T
constexpr int CTRL_OPEN = 20;          // T_UI_Prompt_XSX_LeftStickButton (L3)
}

bool Extension::ensure_prompt_refs() {
    if (prompt_refs_ready_) return true;
    try {
        widget_lib_ = host_.find("/Script/UMG.Default__WidgetBlueprintLibrary");
        prompt_class_ = host_.find("/Game/Sparta/UI/Core/Navigation/WBP_PromptWithText.WBP_PromptWithText_C");
        if (!is_object(widget_lib_) || !is_object(prompt_class_)) return false;
        prompt_refs_ready_ = true;
        return true;
    } catch (...) { return false; }
}

bool Extension::resolve_bar() {
    if (is_object(map_prompts_) && is_object(prompt_box_)) return true;
    try {
        map_prompts_ = host_.get(map_screen_, "MapPrompts");
        if (!is_object(map_prompts_)) return false;
        prompt_box_ = host_.get(map_prompts_, "HB_Prompts");
        if (!is_object(prompt_box_)) return false;
        if (!prompt_template_.is_object()) {
            auto all = host_.get(map_prompts_, "AllWidgets");
            if (all.is_array() && !all.empty() && is_object(all[0])) prompt_template_ = host_.get(all[0], "PromptGenData");
        }
        return true;
    } catch (...) { map_prompts_ = prompt_box_ = nullptr; return false; }
}

bool Extension::prompt_present() {
    if (!is_object(my_prompt_)) return false;
    try {
        if (host_.request({{"op", "valid"}, {"target", my_prompt_}}) != true) return false;
        return is_object(host_.get(my_prompt_, "Slot"));
    } catch (...) { return false; }
}

void Extension::show_prompt(const std::string& text) {
    if (!ensure_prompt_refs() || !resolve_bar()) return;
    try {
        const bool alive = is_object(my_prompt_) && host_.request({{"op", "valid"}, {"target", my_prompt_}}) == true;
        if (!alive) {
            auto created = host_.call(widget_lib_, "Create",
                {{"WorldContextObject", controller_}, {"WidgetType", prompt_class_}, {"OwningPlayer", controller_}});
            if (!is_object(created)) { my_prompt_ = nullptr; return; }
            my_prompt_ = created;
            Json gd = prompt_template_.is_object() ? prompt_template_ : Json::object();
            bool set_any = false;
            for (auto it = gd.begin(); it != gd.end(); ++it) {
                const auto& k = it.key();
                if (k.rfind("PromptText", 0) == 0) { it.value() = text; set_any = true; }
                else if (k.rfind("KBMPrompt", 0) == 0) it.value() = KBM_T;
                else if (k.rfind("ControllerPrompt", 0) == 0) it.value() = CTRL_OPEN;
                else if (k.rfind("LeftRightPadding", 0) == 0) it.value() = {{"X", 30.0}, {"Y", 0.0}};
            }
            if (!set_any) gd = {{"PromptText", text}, {"KBMPrompt", KBM_T}, {"ControllerPrompt", CTRL_OPEN}};
            try { host_.set(my_prompt_, "PromptGenData", gd); } catch (...) {}
            prompt_text_shown_.clear();
        }
        if (!is_object(host_.get(my_prompt_, "Slot"))) {
            auto slot = host_.call(prompt_box_, "AddChildToHorizontalBox", {{"content", my_prompt_}});
            // Match the bar's inter-button spacing (each slot carries a 30px left margin).
            if (is_object(slot)) { try { host_.set(slot, "Padding", {{"Left", 30.0}, {"Top", 0.0}, {"Right", 0.0}, {"Bottom", 0.0}}); } catch (...) {} }
            try { host_.call(my_prompt_, "ConstructPrompt (Horizontal)", {{"ParentSlot", slot}}); } catch (...) {}
            // Match the bar's text colour (a muted grey), so it reads as one of the row.
            try {
                auto tp = host_.get(my_prompt_, "Text_Prompt");
                if (is_object(tp)) host_.set(tp, "ColorAndOpacity",
                    {{"SpecifiedColor", {{"R", 0.547}, {"G", 0.547}, {"B", 0.547}, {"A", 1.0}}}, {"ColorUseRule", 0}});
            } catch (...) {}
            prompt_text_shown_.clear();
        }
        if (text != prompt_text_shown_) {
            try { host_.call(my_prompt_, "UpdateText", {{"NewText", text}}); prompt_text_shown_ = text; } catch (...) {}
        }
    } catch (...) {}
}

void Extension::hide_prompt() {
    if (!is_object(my_prompt_)) return;
    try { if (host_.request({{"op", "valid"}, {"target", my_prompt_}}) == true) host_.call(my_prompt_, "RemoveFromParent"); }
    catch (...) {}
    prompt_text_shown_.clear();
}

// Glyph indices harvested from the game's own live prompts (E_UI_Prompt_KBM /
// E_UI_Prompt_XSX): confirm reads as Enter / (A), back reads as Esc / (B).
namespace {
constexpr int KBM_CONFIRM = 12, CTRL_CONFIRM = 0;   // "Select" / "Ok" glyph
constexpr int KBM_BACK = 11, CTRL_BACK = 5;         // "Back" / "Close" glyph
}

// The confirmation is the game's own WBP_ConfirmationPrompt_Default. We create it,
// set its title and options, show it over the map, and disable its own input
// listener so we drive Traverse/Cancel with our own key reads (its result is a
// delegate an extension cannot receive). Selection is the button's native
// highlight state, driven by highlight_option(), and each option carries the
// input glyph so the controls read at a glance.
void Extension::show_dialog(const std::string& name) {
    hide_dialog();
    if (!ensure_prompt_refs()) return;   // uses widget_lib_ / prompt_class_
    try {
        if (!is_object(dialog_class_))
            dialog_class_ = host_.find("/Game/Sparta/UI/Menu/Misc/WBP_ConfirmationPrompt_Default.WBP_ConfirmationPrompt_Default_C");
        if (!is_object(dialog_class_)) return;
        auto w = host_.call(widget_lib_, "Create",
            {{"WorldContextObject", controller_}, {"WidgetType", dialog_class_}, {"OwningPlayer", controller_}});
        if (!is_object(w)) return;
        my_dialog_ = w;
        opt_primary_ = opt_secondary_ = nullptr;
        try { host_.set(my_dialog_, "PromptText", std::string("Traverse to ") + name + "?"); } catch (...) {}
        try { host_.set(my_dialog_, "PrimaryOptionText", std::string("Traverse")); } catch (...) {}
        try { host_.set(my_dialog_, "SecondaryOptionText", std::string("Cancel")); } catch (...) {}
        try { host_.set(my_dialog_, "OptionalDescription", std::string("")); } catch (...) {}
        // InitData applies the texts and builds the option buttons.
        try { host_.call(my_dialog_, "InitData"); } catch (...) {}
        try { host_.call(my_dialog_, "AddToViewport", {{"ZOrder", 1000}}); } catch (...) {}
        // Added straight to the viewport, the dialog is not the focused input layer,
        // so its own listener never fires; we drive Traverse/Cancel ourselves and
        // disable its listener so it does not also try (and fail) to handle input.
        try { host_.call(my_dialog_, "DisableInputListener"); } catch (...) {}
        try { host_.call(my_dialog_, "HandleDescription", {{"Valid", false}}); } catch (...) {}
        // Make the whole dialog non-hit-testable (ESlateVisibility::HitTestInvisible)
        // so the mouse can no longer hover the options and fight our selection: with
        // this off, the only highlight is the one highlight_option() sets.
        try { host_.set(my_dialog_, "Visibility", 3); } catch (...) {}
        // Cache the option buttons and put the input glyph beside each label.
        try { opt_primary_ = host_.get(my_dialog_, "PrimaryOption"); } catch (...) {}
        try { opt_secondary_ = host_.get(my_dialog_, "SecondaryOption"); } catch (...) {}
        add_option_glyph(opt_primary_, "Traverse", KBM_CONFIRM, CTRL_CONFIRM);
        add_option_glyph(opt_secondary_, "Cancel", KBM_BACK, CTRL_BACK);
        // Own the context: while the dialog is up, deactivate every input listener
        // that is currently live, not just the map's own ones. The menu's tab-nav
        // listener (WBP_IL_GameplayMenu) sits above the map and, left active, would
        // read dpad-left as "open the Map tab" underneath the confirmation. We
        // disable each active listener and restore exactly those on close. Our own
        // Traverse/Cancel reads are raw key polls, so freezing listeners never blocks
        // them.
        frozen_listeners_ = Json::array();
        bool swept = false;
        try {
            auto il_class = host_.find("/Game/Sparta/UI/Core/Navigation/WBP_InputListener.WBP_InputListener_C");
            if (is_object(il_class) && is_object(widget_lib_) && is_object(world_)) {
                auto res = host_.call(widget_lib_, "GetAllWidgetsOfClass",
                    {{"WorldContextObject", world_}, {"WidgetClass", il_class}, {"TopLevelOnly", false}});
                Json found = res.is_object() ? res.value("FoundWidgets", Json()) : Json();
                if (found.is_array()) {
                    swept = true;
                    for (const auto& l : found) {
                        if (!is_object(l)) continue;
                        try {
                            auto en = host_.call(l, "IsEnabled");
                            const bool active = en.is_object() && en.value("ReturnValue", false);
                            if (active) { host_.call(l, "SetEnabledState", {{"bEnabled", false}}); frozen_listeners_.push_back(l); }
                        } catch (...) {}
                    }
                }
            }
        } catch (...) {}
        // Fallback to the map's named listeners if the sweep could not run.
        if (!swept) {
            for (const char* ln : {"WBP_IL_WorldMap", "WBP_IL_MapTracker", "WBP_IL_FocusToPlayer", "WBP_IL_Filters"}) {
                try {
                    auto l = host_.get(map_screen_, ln);
                    if (is_object(l)) { host_.call(l, "SetEnabledState", {{"bEnabled", false}}); frozen_listeners_.push_back(l); }
                } catch (...) {}
            }
        }
        highlight_option(0);   // start with Traverse selected
    } catch (...) {}
}

void Extension::add_option_glyph(const Json& option, const std::string& label, int kbm, int controller) {
    if (!is_object(option) || !is_object(widget_lib_) || !is_object(prompt_class_)) return;
    try {
        auto overlay = host_.get(option, "Overlay_Main");
        if (!is_object(overlay)) return;
        // Hide the plain centred label; the glyph widget carries its own copy of it.
        try { auto bt = host_.get(option, "Button_Text"); if (is_object(bt)) host_.set(bt, "Visibility", 1); } catch (...) {}
        auto gw = host_.call(widget_lib_, "Create",
            {{"WorldContextObject", controller_}, {"WidgetType", prompt_class_}, {"OwningPlayer", controller_}});
        if (!is_object(gw)) return;
        Json gd = prompt_template_.is_object() ? prompt_template_ : Json::object();
        bool set_any = false;
        for (auto it = gd.begin(); it != gd.end(); ++it) {
            const auto& k = it.key();
            if (k.rfind("PromptText", 0) == 0) { it.value() = label; set_any = true; }
            else if (k.rfind("KBMPrompt", 0) == 0) it.value() = kbm;
            else if (k.rfind("ControllerPrompt", 0) == 0) it.value() = controller;
            else if (k.rfind("InputAction", 0) == 0) it.value() = nullptr;
            else if (k.rfind("LeftRightPadding", 0) == 0) it.value() = {{"X", 12.0}, {"Y", 0.0}};
        }
        if (!set_any) gd = {{"PromptText", label}, {"KBMPrompt", kbm}, {"ControllerPrompt", controller}};
        try { host_.set(gw, "PromptGenData", gd); } catch (...) {}
        auto slot = host_.call(overlay, "AddChildToOverlay", {{"Content", gw}});
        if (is_object(slot)) {
            try { host_.set(slot, "HorizontalAlignment", 2); } catch (...) {}   // centre
            try { host_.set(slot, "VerticalAlignment", 2); } catch (...) {}
        }
        try { host_.call(gw, "ConstructPrompt (Horizontal)", {{"ParentSlot", slot}}); } catch (...) {}
    } catch (...) {}
}

// Exactly one option is highlighted: the native highlight state (a backing glow on
// the button) shows on the selection and is cleared on the other, so there is never
// a stale second highlight.
void Extension::highlight_option(int index) {
    const Json& sel = index == 0 ? opt_primary_ : opt_secondary_;
    const Json& other = index == 0 ? opt_secondary_ : opt_primary_;
    try { if (is_object(other)) host_.call(other, "OnNullState"); } catch (...) {}
    try { if (is_object(sel)) host_.call(sel, "OnHighlightedState"); } catch (...) {}
}

void Extension::hide_dialog() {
    // Re-enable the map's input listeners we froze.
    if (frozen_listeners_.is_array()) {
        for (const auto& l : frozen_listeners_) {
            try { if (host_.request({{"op", "valid"}, {"target", l}}) == true) host_.call(l, "SetEnabledState", {{"bEnabled", true}}); } catch (...) {}
        }
    }
    frozen_listeners_ = Json::array();
    opt_primary_ = opt_secondary_ = nullptr;
    if (!is_object(my_dialog_)) return;
    try { if (host_.request({{"op", "valid"}, {"target", my_dialog_}}) == true) host_.call(my_dialog_, "RemoveFromParent"); }
    catch (...) {}
    my_dialog_ = nullptr;
}

}

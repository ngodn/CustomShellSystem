// Traverse confirmation-dialog tests against a mock CSSX host that plays the game's side:
// the world map with a hovered beacon, the menu's input listeners (which act on a key the
// moment it goes down, while they are bound), and the dialog's two option buttons with the
// game's own WBP_Navigable mouse behaviour (hover selects, a click confirms; both dropped
// under UI.Input.Block.All unless the button's IgnoreBlockAll is set). Behaviour copied
// from the game's blueprints, see work/export-dialog.
#include "teleport.hpp"
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

using teleport::Json;
namespace {
unsigned checks = 0;
void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
Json object(uint64_t id) { return {{"$object", id}}; }
uint64_t id_of(const Json& j) { return j.is_object() && j.contains("$object") ? j["$object"].get<uint64_t>() : 0; }

enum : uint64_t {
    Pawn = 1, Controller, World, UiHandler = 10, Menu, MenuMain, MapScreen, MapPrompts, PromptBox,
    Icon = 20, Owner, Tooltip, Zone,
    TrackerListener = 30, BackListener, TabListener, IdleListener,   // IdleListener is unbound throughout
    WidgetLib = 50, PromptClass, DialogClass, ListenerClass, MathLib, StreamLib = 60, TeleportManager,
};

struct Host {
    std::map<uint64_t, Json> props;
    std::set<uint64_t> listeners{TrackerListener, BackListener, TabListener, IdleListener};
    uint64_t next = 1000;
    Json keys = Json::object(), state = {{"meteor", false}};
    // What the game did.
    int pins = 0, back_closes = 0, tab_switches = 0, teleports = 0;
    bool menu_open = true, block_all_tag = false;
    uint64_t dialog = 0, primary = 0, secondary = 0, dialog_listener = 0;
    bool dialog_shown = false;
    std::vector<std::string> unexpected;
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};

    Host() {
        props[Controller] = {{"User Interface Handler Component", object(UiHandler)}};
        props[UiHandler] = {{"ActiveMenu", object(Menu)}};
        props[Menu] = {{"WBP_Menu_Main", object(MenuMain)}};
        props[MenuMain] = {{"WBP_MGT_WorldMap", object(MapScreen)}};
        props[MapScreen] = {{"bOpen", true}, {"WBP_Icons", Json::array({object(Icon)})}, {"MapPrompts", object(MapPrompts)}};
        props[MapPrompts] = {{"HB_Prompts", object(PromptBox)}, {"AllWidgets", Json::array()}};
        props[Icon] = {{"Selected", true}, {"OwnerActor", object(Owner)}, {"MapTooltip", object(Tooltip)}};
        props[Tooltip] = {{"Name", "Sester's Gate"}};
        props[Owner] = {{"ZoneData", object(Zone)}};
        props[TeleportManager] = {{"bIsInDungeon", false}};
        for (auto id : {TrackerListener, BackListener, TabListener}) props[id] = {{"bEnabled", true}, {"IgnoreBlockAll", false}};
        props[IdleListener] = {{"bEnabled", false}, {"IgnoreBlockAll", false}};
    }
    static int request(void* context, const char* value, CssxSink sink, void* output) {
        try { auto r = static_cast<Host*>(context)->handle(Json::parse(value)).dump(); sink(output, r.data(), r.size()); return 1; }
        catch (const std::exception& e) { auto r = Json{{"error", e.what()}}.dump(); sink(output, r.data(), r.size()); return 0; }
    }
    bool bound(uint64_t listener) { return props[listener].value("bEnabled", false); }
    // The real IsEnabled(): bound, and not blocked by the tag.
    bool listening(uint64_t listener) { return bound(listener) && !(block_all_tag && !props[listener].value("IgnoreBlockAll", false)); }

    // ---- the player's hands ----
    void key_down(const std::string& key) {
        keys[key] = true;
        if ((key == "SpaceBar" || key == "Gamepad_FaceButton_Bottom") && listening(TrackerListener)) ++pins;
        if ((key == "Escape" || key == "Gamepad_FaceButton_Right") && listening(BackListener)) ++back_closes;
        if ((key == "E" || key == "Q") && listening(TabListener)) ++tab_switches;
    }
    void key_up(const std::string& key) { keys[key] = false; }
    bool button_live(uint64_t option) {
        const auto button = id_of(props[option].value("WBP_NavigationButton", Json()));
        return dialog_shown && !(block_all_tag && !props[button].value("IgnoreBlockAll", false));
    }
    void hover(uint64_t option) {   // WBP_Navigable MouseHover, type 2, bSelectOnHover
        if (!button_live(option)) return;
        const int s = props[option].value("NavigableState", 0);
        if (s == 2 || s == 3) return;
        props[option]["NavigableState"] = 2; props[option]["lit"] = true;
    }
    void click(uint64_t option) {   // HandleNavigableInput(13) -> TriggerConfirmedState
        if (!button_live(option)) return;
        props[option]["NavigableState"] = 3;
    }

    Json make_option() {
        const auto option = next++, button = next++, overlay = next++, text = next++;
        props[button] = {{"IgnoreBlockAll", false}};
        props[option] = {{"NavigableState", 0}, {"lit", false}, {"WBP_NavigationButton", object(button)},
                         {"Overlay_Main", object(overlay)}, {"Button_Text", object(text)}};
        props[overlay] = Json::object(); props[text] = Json::object();
        return object(option);
    }
    Json handle(const Json& j) {
        const auto op = j.at("op").get<std::string>();
        if (op == "state.load") return state;
        if (op == "state.save") { state = j.at("value"); return true; }
        if (op == "log" || op == "invalidate") return nullptr;
        if (op == "player") return {{"pawn", object(Pawn)}, {"controller", object(Controller)}, {"world", object(World)}};
        if (op == "valid") return props.contains(id_of(j.at("target")));
        if (op == "describe") return {{"class", "BP_LandingAreaBase_C"}};
        if (op == "class_default") return j.at("class") == "BPFL_WorldStreaming_C" ? object(StreamLib) : Json(nullptr);
        if (op == "find") {
            const auto path = j.at("path").get<std::string>();
            if (path.ends_with("WidgetBlueprintLibrary")) return object(WidgetLib);
            if (path.ends_with("WBP_PromptWithText_C")) return object(PromptClass);
            if (path.ends_with("WBP_ConfirmationPrompt_Default_C")) return object(DialogClass);
            if (path.ends_with("WBP_InputListener_C")) return object(ListenerClass);
            if (path.ends_with("KismetMathLibrary")) return object(MathLib);
            return nullptr;
        }
        if (op == "input.keys") { Json r = Json::object(); for (const auto& k : j.at("keys")) r[k.get<std::string>()] = keys.value(k.get<std::string>(), false); return r; }
        const auto target = id_of(j.at("target"));
        if (!props.contains(target) && target != WidgetLib && target != MathLib && target != StreamLib && target != World)
            throw std::runtime_error("Object handle expired");
        if (op == "get") { const auto& p = props[target]; const auto name = j.at("property").get<std::string>(); return p.contains(name) ? p.at(name) : Json(nullptr); }
        if (op == "set") { props[target][j.at("property").get<std::string>()] = j.at("value"); return true; }
        if (op != "call") throw std::runtime_error("unexpected op " + op);
        const auto fn = j.at("function").get<std::string>();
        const auto a = j.value("args", Json::object());
        if (target == WidgetLib && fn == "Create") {
            const auto type = id_of(a.at("WidgetType"));
            const auto widget = next++;
            props[widget] = Json::object();
            if (type == DialogClass) {
                dialog = widget; dialog_listener = next++;
                props[dialog_listener] = {{"bEnabled", true}, {"IgnoreBlockAll", true}};   // auto-activates on Construct
                listeners.insert(dialog_listener);
                const auto p = make_option(), s = make_option();
                primary = id_of(p); secondary = id_of(s);
                props[widget] = {{"PrimaryOption", p}, {"SecondaryOption", s}};
            }
            return {{"ReturnValue", object(widget)}};
        }
        if (target == WidgetLib && fn == "GetAllWidgetsOfClass") {
            Json found = Json::array();
            if (id_of(a.at("WidgetClass")) == ListenerClass) for (auto id : listeners) found.push_back(object(id));
            return {{"FoundWidgets", found}};
        }
        if (listeners.contains(target)) {
            if (fn == "SetEnabledState") { props[target]["bEnabled"] = a.at("bEnabled"); return Json::object(); }
            if (fn == "IsEnabled") return {{"ReturnValue", listening(target)}};
        }
        if (target == dialog) {
            if (fn == "InitData" || fn == "HandleDescription") return Json::object();
            if (fn == "AddToViewport") { dialog_shown = true; return Json::object(); }
            if (fn == "DisableInputListener") { props[dialog_listener]["bEnabled"] = false; return Json::object(); }
            if (fn == "RemoveFromParent") { dialog_shown = false; listeners.erase(dialog_listener); return Json::object(); }
        }
        if (target == primary || target == secondary) {
            if (fn == "OnHighlightedState") { props[target]["lit"] = true; return Json::object(); }
            if (fn == "OnNullState") { props[target]["lit"] = false; return Json::object(); }
            if (fn == "TriggerNullState") { props[target]["lit"] = false; props[target]["NavigableState"] = 0; return Json::object(); }
        }
        if (fn == "AddChildToOverlay" || fn == "AddChildToHorizontalBox") {
            const auto slot = next++; props[slot] = Json::object();
            const auto content = id_of(a.contains("Content") ? a.at("Content") : a.at("content"));
            props[content]["Slot"] = object(slot);
            return {{"ReturnValue", object(slot)}};
        }
        if (fn == "ConstructPrompt (Horizontal)" || fn == "UpdateText") return Json::object();
        if (fn == "RemoveFromParent") { props[target]["Slot"] = nullptr; return Json::object(); }
        if (target == Owner && fn == "IsUnlocked") return {{"ReturnValue", true}};
        if (target == Owner && fn == "GetStartTransform")
            return {{"ReturnValue", {{"Translation", {{"X", 100.0}, {"Y", 200.0}, {"Z", 300.0}}}, {"Rotation", {{"X", 0.0}, {"Y", 0.0}, {"Z", 0.0}, {"W", 1.0}}}}}};
        if (target == MathLib && fn == "BreakTransform") return {{"Location", a.at("InTransform").at("Translation")}, {"Rotation", {{"Pitch", 0.0}, {"Yaw", 90.0}, {"Roll", 0.0}}}};
        if (target == StreamLib && fn == "GetTeleportManager") return {{"ReturnValue", object(TeleportManager)}};
        if (target == StreamLib && fn == "TeleportPlayerWithStreaming") {
            if (id_of(a.at("OptionalZoneData")) != Zone) throw std::runtime_error("teleport without the zone");
            ++teleports; return Json::object();
        }
        if (target == UiHandler && fn == "HandleGameMenu") { menu_open = false; props[UiHandler]["ActiveMenu"] = nullptr; props[MapScreen]["bOpen"] = false; return Json::object(); }
        unexpected.push_back(fn);
        throw std::runtime_error("unexpected call " + fn);
    }
};

struct Rig {
    Host host;
    teleport::Extension ext{&host.api};
    CssxFrame frame{};
    uint32_t world = 1;
    void scan() {
        frame.abi = CSSX_ABI; frame.size = sizeof(CssxFrame); frame.seconds = 0.07;   // past the 0.06 s scan gate
        frame.world_generation = world; frame.world_ready = 1; frame.in_menu = host.menu_open ? 1 : 0;
        frame.viewport_w = 2560; frame.viewport_h = 1440; frame.pawn = Pawn; frame.controller = Controller;
        ext.render(&frame);
    }
    void scans(int n) { for (int i = 0; i < n; ++i) scan(); }
    // Down for two scans (about 120 ms), like a real tap.
    void tap(const std::string& key) { host.key_down(key); scans(2); host.key_up(key); scan(); }
    void open_dialog() {
        scans(2);
        expect(!host.dialog_shown, "no dialog before the key");
        tap("T");
        expect(host.dialog_shown, "T on a hovered point opens the confirmation");
    }
    bool lit(uint64_t option) { return host.props[option].value("lit", false); }
    bool map_listeners_bound() { return host.bound(TrackerListener) && host.bound(BackListener) && host.bound(TabListener); }
    void expect_restored(const std::string& when) {
        expect(!host.dialog_shown, when + ": the dialog is gone");
        expect(map_listeners_bound(), when + ": the map's listeners are bound again");
        expect(!host.bound(IdleListener), when + ": a listener that was off stays off");
        expect(host.unexpected.empty(), when + ": no unexpected game call" + (host.unexpected.empty() ? "" : " (" + host.unexpected.front() + ")"));
    }
};
}

int main() {
    try {
        {   // While the dialog is up the menu underneath hears nothing; Space confirms without a pin.
            Rig r;
            r.open_dialog();
            expect(!r.host.bound(TrackerListener) && !r.host.bound(BackListener) && !r.host.bound(TabListener), "every bound listener is frozen under the dialog");
            expect(!r.host.bound(r.host.dialog_listener), "the dialog's own listener stays off");
            expect(r.lit(r.host.primary) && !r.lit(r.host.secondary), "Traverse starts selected");
            r.tap("SpaceBar");
            expect(r.host.teleports == 1, "Space on Traverse traverses");
            expect(r.host.pins == 0, "confirming with Space places no pin");
            r.expect_restored("after a traverse");
        }
        {   // Choosing Cancel with the keys, then Space: no traverse, no pin.
            Rig r;
            r.open_dialog();
            r.tap("D");
            expect(r.lit(r.host.secondary) && !r.lit(r.host.primary), "D moves the selection to Cancel");
            r.tap("SpaceBar");
            expect(r.host.teleports == 0 && r.host.pins == 0, "Space on Cancel cancels and places no pin");
            r.expect_restored("after a cancel");
            expect(r.host.menu_open, "the map stays open after a cancel");
            // The map works again afterwards: Space places a pin, as the game does.
            r.tap("SpaceBar");
            expect(r.host.pins == 1, "the map's own Space works once the dialog is gone");
        }
        {   // Gamepad A, E and Enter confirm; none of them reaches the menu underneath.
            for (const char* key : {"Gamepad_FaceButton_Bottom", "E", "Enter"}) {
                Rig r;
                r.open_dialog();
                r.tap(key);
                expect(r.host.teleports == 1 && r.host.pins == 0 && r.host.tab_switches == 0, std::string(key) + " confirms cleanly");
                r.expect_restored(std::string("after ") + key);
            }
        }
        {   // Escape cancels the dialog only; the map's back listener does not close the menu.
            Rig r;
            r.open_dialog();
            r.tap("Escape");
            expect(r.host.back_closes == 0 && r.host.menu_open && r.host.teleports == 0, "Escape cancels without closing the map");
            r.expect_restored("after Escape");
        }
        {   // Mouse: clicking Traverse traverses.
            Rig r;
            r.open_dialog();
            r.host.hover(r.host.primary); r.scan();
            r.host.click(r.host.primary); r.scan();
            expect(r.host.teleports == 1 && r.host.pins == 0, "clicking Traverse traverses");
            r.expect_restored("after a click on Traverse");
        }
        {   // Mouse: clicking Cancel cancels.
            Rig r;
            r.open_dialog();
            r.host.hover(r.host.secondary); r.scan();
            expect(r.lit(r.host.secondary) && !r.lit(r.host.primary), "hovering Cancel moves the one highlight to it");
            r.host.click(r.host.secondary); r.scan();
            expect(r.host.teleports == 0, "clicking Cancel does not traverse");
            r.expect_restored("after a click on Cancel");
            expect(r.host.menu_open, "the map stays open");
        }
        {   // The keys act on what is lit: hover Cancel, then Space cancels.
            Rig r;
            r.open_dialog();
            r.host.hover(r.host.secondary); r.scan();
            r.tap("SpaceBar");
            expect(r.host.teleports == 0 && r.host.pins == 0, "Space acts on the hovered option");
            r.expect_restored("after hover then Space");
        }
        {   // Mouse then keys then mouse again: one highlight throughout, and a re-hover still counts.
            Rig r;
            r.open_dialog();
            r.host.hover(r.host.secondary); r.scan();
            r.tap("A");
            expect(r.lit(r.host.primary) && !r.lit(r.host.secondary), "A takes the selection back to Traverse");
            expect(r.host.props[r.host.secondary].value("NavigableState", -1) == 0, "the option the mouse left can be hovered again");
            r.scans(5);
            expect(r.lit(r.host.primary) && !r.lit(r.host.secondary), "the selection does not flip back by itself");
            r.host.hover(r.host.secondary); r.scan();
            expect(r.lit(r.host.secondary) && !r.lit(r.host.primary), "hovering Cancel again selects it again");
            r.host.hover(r.host.primary); r.scan();
            expect(r.lit(r.host.primary) && !r.lit(r.host.secondary), "and back to Traverse");
            r.tap("Enter");
            expect(r.host.teleports == 1, "Enter acts on Traverse");
        }
        {   // The player carries UI.Input.Block.All: the buttons still take the mouse.
            Rig r;
            r.host.block_all_tag = true;
            r.open_dialog();
            r.host.hover(r.host.secondary); r.scan();
            r.host.click(r.host.secondary); r.scan();
            r.expect_restored("after a click under the block tag");
            expect(r.host.teleports == 0, "the click on Cancel was heard");
        }
        {   // Every other way out restores the listeners too.
            { Rig r; r.open_dialog(); r.scans(200); r.expect_restored("after the 10 s auto-cancel"); }
            { Rig r; r.open_dialog(); r.host.props[MapScreen]["bOpen"] = false; r.scan(); r.expect_restored("after the map tab closes"); }
            { Rig r; r.open_dialog(); r.host.menu_open = false; r.scan(); r.expect_restored("after the menu closes"); }
            { Rig r; r.open_dialog(); r.world = 2; r.scan(); r.expect_restored("after a world change"); }
            { Rig r; r.open_dialog(); r.ext.event({{"id", "enable"}, {"value", false}}); r.expect_restored("after Traverse is switched off"); }
            { Rig r; r.open_dialog(); expect(r.ext.stop(), "stop succeeds"); r.expect_restored("after stop"); }
        }
        {   // A pan key held while the dialog opens does not move the selection until it is pressed again.
            Rig r;
            r.scans(2);
            r.host.key_down("D");
            r.tap("T");
            expect(r.host.dialog_shown && r.lit(r.host.primary) && !r.lit(r.host.secondary), "a held D leaves Traverse selected");
            r.host.key_up("D"); r.scan();
            r.tap("D");
            expect(r.lit(r.host.secondary), "a fresh D press selects Cancel");
        }
        {   // Confirmation off: T traverses straight away and never touches a listener.
            Rig r;
            r.host.state["confirmation"] = "skip";
            r.ext.~Extension(); new (&r.ext) teleport::Extension(&r.host.api);   // reload the setting
            r.scans(2);
            r.tap("T");
            expect(r.host.teleports == 1 && r.host.dialog == 0, "with confirmation off, T traverses with no dialog");
            expect(r.map_listeners_bound() && r.host.unexpected.empty(), "listeners untouched");
        }
        {   // A dialog reopened after a cancel starts clean.
            Rig r;
            r.open_dialog();
            r.host.hover(r.host.secondary); r.scan();
            r.host.click(r.host.secondary); r.scan();
            r.scan();
            r.tap("T");
            expect(r.host.dialog_shown, "T opens it again");
            expect(r.lit(r.host.primary) && !r.lit(r.host.secondary), "the new dialog starts on Traverse");
            expect(!r.host.bound(TrackerListener), "and freezes the map again");
            r.tap("SpaceBar");
            expect(r.host.teleports == 1 && r.host.pins == 0, "and confirms cleanly");
        }
        std::cout << checks << " Traverse dialog checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}

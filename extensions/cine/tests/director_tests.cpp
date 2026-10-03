// CINE director tests against a mock CSSX host that answers the calls CINE makes and moves
// her toward the path-following goal like the game would. Run with the extension directory
// as argv[1].
#include "cine.hpp"
#include "manifest.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <set>

using cine::Json;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
Json object(uint64_t id) { return {{"$object", id}}; }
uint64_t id_of(const Json& j) { return j.is_object() && j.contains("$object") ? j["$object"].get<uint64_t>() : 0; }

struct Host {
    fs::path dir;
    Json state = Json::object();
    uint64_t next = 100;
    std::set<uint64_t> cameras, destroyed;
    uint64_t view = 1;                  // current view target ($object id); 1 = pawn
    int hud_visibility = 4, force_walk = 0, move_component = 0, speed_adjust_removed = 0, place_player = 0;
    bool speed_adjusted = false, moving = false, fail_move = false, hit_walls = false;
    double speed_limit = 0;
    cine::Vec3 pos{0, 0, 100}, goal{0, 0, 100};
    double yaw = 0;
    Json keys = Json::object();
    std::vector<Json> css_applies;
    bool css_present = true;
    size_t requests = 0;
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};
    static int request(void* context, const char* value, CssxSink sink, void* output) {
        try { auto r = static_cast<Host*>(context)->handle(Json::parse(value)).dump(); sink(output, r.data(), r.size()); return 1; }
        catch (const std::exception& e) { auto r = Json{{"error", e.what()}}.dump(); sink(output, r.data(), r.size()); return 0; }
    }
    Json vec(cine::Vec3 v) const { return {{"X", v.x}, {"Y", v.y}, {"Z", v.z}}; }
    Json handle(const Json& j) {
        ++requests;
        const auto op = j.at("op").get<std::string>();
        if (op == "state.load") return state;
        if (op == "state.save") { state = j.at("value"); return true; }
        if (op == "log" || op == "invalidate" || op == "menu.close") return nullptr;
        if (op == "extension.info") return {{"directory", dir.string()}, {"id", "eins0fx.cine"}};
        if (op == "player") return {{"pawn", object(1)}, {"controller", object(2)}};
        if (op == "find" || op == "load") return object(next++);
        // Like the real bridge: class_default resolves a class name, never an asset path.
        if (op == "class_default") { const auto c = j.at("class").get<std::string>(); return c.find('/') == std::string::npos && c.size() <= 96 ? object(next++) : Json(nullptr); }
        if (op == "valid") return !destroyed.contains(id_of(j.at("target")));
        if (op == "input.keys") { Json r = Json::object(); for (const auto& k : j.at("keys")) r[k.get<std::string>()] = keys.value(k.get<std::string>(), false); return r; }
        if (op == "get") return object(next++);
        if (op == "set") return true;
        if (op == "service.call") {
            if (j.at("service") != "css.customize" || j.value("version", 0) != 1) throw std::runtime_error("unexpected service call");
            if (!css_present) throw std::runtime_error("No service named css.customize is loaded");
            const auto& r = j.at("request");
            if (r.at("action") == "describe")
                return {{"palette", "original"}, {"customize", {{"palette", "original"}, {"values", {{"coat", {1, 0, 0, 1}}}}}},
                        {"palettes", Json::array({{{"id", "original"}}, {{"id", "crimson"}}, {{"id", "midnight"}}})},
                        {"controls", Json::array({{{"id", "coat"}, {"kind", "toggle"}}, {{"id", "cloth"}, {"kind", "color"}}})}};
            css_applies.push_back(r);
            return {{"palette", "x"}};
        }
        if (op != "call") throw std::runtime_error("unexpected op " + op);
        const auto fn = j.at("function").get<std::string>();
        const auto& a = j.value("args", Json::object());
        const uint64_t target = id_of(j.at("target"));
        if (destroyed.contains(target)) throw std::runtime_error("Object is no longer valid");
        if (fn == "K2_GetActorLocation") return {{"ReturnValue", vec(pos)}};
        if (fn == "GetSocketLocation") return {{"ReturnValue", vec(pos + cine::Vec3{0, 0, 65})}};
        if (fn == "BeginDeferredActorSpawnFromClass") { const auto id = next++; cameras.insert(id); return {{"ReturnValue", object(id)}}; }
        if (fn == "FinishSpawningActor" || fn == "K2_AttachToActor" || fn == "K2_SetRelativeLocationAndRotation") return {{"ReturnValue", true}};
        if (fn == "K2_SetActorLocationAndRotation") { if (target == 1) { pos = cine::Vec3{a["NewLocation"]["X"], a["NewLocation"]["Y"], a["NewLocation"]["Z"]}; ++place_player; } return {{"ReturnValue", true}}; }
        if (fn == "MoveComponentTo") { if (fail_move) throw std::runtime_error("ProcessEvent failed"); ++move_component; return Json::object(); }
        if (fn == "SetViewTargetWithBlend") { view = id_of(a.at("NewViewTarget")); return Json::object(); }
        if (fn == "GetAllWidgetsOfClass") return {{"FoundWidgets", Json::array({object(50)})}};
        if (fn == "GetVisibility") return {{"ReturnValue", hud_visibility}};
        if (fn == "SetVisibility") { hud_visibility = a.at("InVisibility"); return Json::object(); }
        if (fn == "SetForceWalk") { ++force_walk; return Json::object(); }
        if (fn == "RemoveForceWalk") { force_walk = std::max(0, force_walk - 1); return Json::object(); }
        if (fn == "HasForcedWalk") return {{"ReturnValue", force_walk > 0}};
        if (fn == "SetMaxSpeedAdjustment") { speed_adjusted = true; speed_limit = a.at("Speed"); return Json::object(); }
        if (fn == "RemoveMaxSpeedAdjustment") { speed_adjusted = false; ++speed_adjust_removed; return Json::object(); }
        if (fn == "SimpleMoveToLocation") { goal = cine::Vec3{a["Goal"]["X"], a["Goal"]["Y"], a["Goal"]["Z"]}; moving = true; return Json::object(); }
        if (fn == "StopMovement") { moving = false; return Json::object(); }
        if (fn == "SetControlRotation" || fn == "SetFieldOfView") return Json::object();
        if (fn == "K2_DestroyActor") { destroyed.insert(target); cameras.erase(target); return {{"ReturnValue", true}}; }
        if (fn == "LineTraceSingle") return {{"ReturnValue", hit_walls}, {"OutHit", {{"Distance", 150.0}}}};
        if (fn == "GetCameraLocation") return {{"ReturnValue", vec(pos + cine::Vec3{0, 300, 0})}};
        if (fn == "GetCameraRotation") return {{"ReturnValue", {{"Pitch", 10.0}, {"Yaw", -90.0}, {"Roll", 0.0}}}};
        if (fn == "GetFOVAngle") return {{"ReturnValue", 70.0}};
        throw std::runtime_error("unexpected call " + fn);
    }
    // The game's side of one 0.1 s tick: path following moves her toward the goal.
    void advance(double dt) {
        if (!moving) return;
        const double speed = speed_adjusted ? std::min(184.0, speed_limit) : 540.0;
        const double d = cine::flat_distance(pos, goal);
        if (d < 1) return;
        const double step = std::min(d, speed * dt);
        pos.x += (goal.x - pos.x) / d * step; pos.y += (goal.y - pos.y) / d * step;
        yaw = std::atan2(goal.y - pos.y, goal.x - pos.x) * 180 / 3.14159265358979;
    }
};

struct Rig {
    Host host;
    std::unique_ptr<cine::Extension> ext;
    CssxFrame frame{};
    uint32_t world = 1;
    bool menu = false;
    explicit Rig(const fs::path& dir) { host.dir = dir; ext = std::make_unique<cine::Extension>(&host.api); }
    void step(double dt = 0.1) {
        const auto before = host.pos;
        host.advance(dt);
        frame.abi = CSSX_ABI; frame.size = sizeof(CssxFrame); frame.seconds = dt; frame.world_generation = world; frame.world_ready = 1;
        frame.in_menu = menu ? 1 : 0; frame.player_x = host.pos.x; frame.player_y = host.pos.y; frame.player_z = host.pos.z; frame.player_yaw = host.yaw;
        frame.velocity_x = (host.pos.x - before.x) / dt; frame.velocity_y = (host.pos.y - before.y) / dt; frame.velocity_z = 0;
        frame.viewport_w = 2560; frame.viewport_h = 1440; frame.pawn = 1; frame.controller = 2;
        ext->render(&frame);
        ext->tick(dt);
    }
    void steps(int n) { for (int i = 0; i < n; ++i) step(); }
    void press(const std::string& key) { host.keys[key] = false; step(); host.keys[key] = true; step(); host.keys[key] = false; step(); }
    std::string status() const { return ext->model()["values"]["status"].get<std::string>(); }
    std::string error() const { return ext->model()["error"].get<std::string>(); }
};

fs::path temp_extension(const fs::path& shipped) {
    const auto dir = fs::temp_directory_path() / ("cine-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(dir); fs::create_directories(dir / "presets");
    for (const auto& e : fs::directory_iterator(shipped / "presets")) fs::copy_file(e.path(), dir / "presets" / e.path().filename());
    std::ofstream(dir / "presets" / "broken.cine.json") << "{\"schema\":1,\"id\":\"broken\"";
    return dir;
}
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: cine_director_tests <extension dir>\n"; return 2; }
    const fs::path shipped = argv[1];
    const auto definition = cssx::read_json(cssx::utf8_path((shipped / "menu.json").string()));
    const auto dir = temp_extension(shipped);
    try {
        {   // Idle costs nothing; menu binds; broken preset files are skipped, not fatal.
            Rig r(dir);
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            expect(r.ext->model()["values"]["status"].get<std::string>().find("skipped") != std::string::npos, "a broken preset is reported");
            r.step();
            const auto before = r.host.requests;
            r.steps(50);
            expect(r.host.requests == before, "idle CINE must not call the host");
        }
        {   // Enter, glide take, everything restored, still in Cine World; then leave.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-3"}});
            r.ext->event({{"id", "countdown"}, {"value", 1}});
            r.ext->event({{"id", "enter"}});
            expect(r.host.hud_visibility == 2, "entering hides the game HUD");
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            r.press("F8");
            r.steps(10);   // prepare (walls in slices) + countdown
            expect(!r.host.cameras.empty() && r.host.view != 1, "the take views through CINE's camera");
            expect(r.host.force_walk == 0, "a pose take does not force walk");
            r.steps(40);
            expect(r.host.move_component > 10, "the camera glides with engine-interpolated moves");
            expect(r.host.cameras.empty() && r.host.view == 1, "after the take: cameras destroyed, view back to her");
            expect(r.host.hud_visibility == 2, "still in Cine World, HUD still hidden");
            r.press("F9");
            expect(r.host.hud_visibility == 4, "leaving restores the HUD");
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
        }
        {   // A walk take needs a route; the error names the fix.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "fashion-walk-360"}});
            r.ext->event({{"id", "enter"}});
            r.press("F8");
            r.steps(3);
            expect(r.error().find("Record a route") != std::string::npos, "walk without a route explains itself");
            expect(r.host.cameras.empty() && r.host.view == 1, "a refused take leaves nothing behind");
        }
        {   // Record a route, capture a final shot, roll a walk take with the palette look track.
            Rig r(dir);
            r.step();
            r.menu = true; r.ext->event({{"id", "record"}}); r.step();
            r.menu = false; r.step(); r.step();
            r.host.moving = true; r.host.goal = {2000, 0, 100}; r.host.speed_adjusted = true; r.host.speed_limit = 184;
            r.steps(115);                       // ~21 m of walking
            r.host.moving = false; r.steps(35);  // stand still 3.5 s
            expect(r.host.state.contains("route") && r.host.state["route"].size() >= 10, "the route is saved");
            expect(r.host.hud_visibility == 4 && r.ext->model()["enabled"]["enter"].get<bool>(), "recording from outside goes back outside; Enter still works");
            r.ext->event({{"id", "enter"}});
            r.host.pos = {2300, 150, 100}; r.host.yaw = 30; r.step();
            r.press("F7");
            expect(r.host.state.contains("final"), "the final shot is saved: " + r.status() + " / " + r.error() + " / " + r.host.state.dump().substr(0, 300));
            r.ext->event({{"id", "preset"}, {"value", "fashion-walk-360"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.press("F8");
            r.steps(40);
            expect(r.host.place_player == 1, "she is put on the route start");
            expect(r.host.force_walk > 0, "force walk is on for a walk take");
            r.steps(500);   // the whole take (max 49 s)
            expect(r.host.force_walk == 0 && !r.host.speed_adjusted && !r.host.moving, "walk settings are restored after the take: " + r.status() + " / " + r.error());
            expect(r.host.cameras.empty() && r.host.view == 1, "cameras gone, view back to her");
            expect(cine::flat_distance(r.host.pos, {2300, 150, 100}) < 120, "she walks into the final shot's spot");
            std::set<std::string> palettes;
            for (const auto& a : r.host.css_applies) for (const auto& c : a["commands"]) if (c.value("action", std::string()) == "palette") palettes.insert(c["palette"]);
            expect(palettes == std::set<std::string>{"crimson", "midnight"}, "every palette except Original is shown");
            bool transient = false, own = false, saved = false;
            for (const auto& a : r.host.css_applies) {
                if (a.value("persist", true) == false && a["commands"][0]["action"] == "palette") transient = true;
                if (a.value("persist", true) == false && a["commands"][0]["action"] == "restore") own = true;
                if (a.value("persist", false) == true && a["commands"][0]["action"] == "restore") saved = true;
            }
            expect(transient && own && saved, "palettes cycle unsaved, own look returns, the restore is saved");
            for (const auto& a : r.host.css_applies) for (const auto& c : a["commands"]) expect(c.value("palette", std::string()) != "original", "Original is never used mid-shot");
        }
        {   // F9 cancels a recording started in Cine World and stays there; a loading screen while armed drops to idle.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "enter"}});
            r.ext->event({{"id", "record"}});
            r.step(); r.step();
            expect(r.status().find("Recording") != std::string::npos, "recording started");
            r.press("F9");
            expect(r.status().find("cancelled") != std::string::npos && r.host.hud_visibility == 2, "F9 cancels the recording and stays in Cine World");
            expect(!r.ext->model()["enabled"]["enter"].get<bool>() && r.ext->model()["enabled"]["exit"].get<bool>(), "the menu shows Cine World");
            r.world = 2; r.step();
            const auto before = r.host.requests;
            r.steps(10);
            expect(r.host.requests == before && r.ext->model()["enabled"]["enter"].get<bool>(), "a loading screen while armed returns to idle quietly");
        }
        {   // A menu opening mid-take cancels it and restores.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "turntable"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(10);
            expect(!r.host.cameras.empty(), "take running");
            r.menu = true; r.step(); r.menu = false; r.step();
            expect(r.host.cameras.empty() && r.host.view == 1, "a menu cancels the take and restores");
            expect(r.status().find("menu") != std::string::npos || r.error().find("menu") != std::string::npos, "the reason is shown");
        }
        {   // A bridge failure mid-take stops it cleanly; tick never throws.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-5"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            r.host.fail_move = true; r.steps(3);
            expect(r.host.cameras.empty() && r.host.view == 1, "a failed call ends the take and restores");
            expect(r.error().find("ProcessEvent failed") != std::string::npos, "the failure is shown");
        }
        {   // The world changing mid-take (loading screen) drops everything without calling dead handles.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-5"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            r.world = 2; r.step();
            const auto before = r.host.requests;
            r.steps(20);
            expect(r.host.requests == before, "after a world change CINE is idle");
            expect(r.status().find("world changed") != std::string::npos, "the user is told why");
        }
        {   // stop() mid-take restores everything including the HUD; CSS missing keeps the camera working.
            Rig r(dir);
            r.host.css_present = false;
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-3"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            expect(!r.host.cameras.empty(), "the camera works without CSS");
            expect(r.host.css_applies.empty(), "no look changes without CSS");
            expect(r.status().find("CSS 1.0.0-beta.9") != std::string::npos || r.error().find("CSS 1.0.0-beta.9") != std::string::npos, "the missing look service is explained: " + r.status());
            expect(r.ext->stop(), "stop succeeds");
            expect(r.host.cameras.empty() && r.host.view == 1 && r.host.hud_visibility == 4, "stop restores everything");
        }
        {   // Bad menu input is refused with the error kept for the menu.
            Rig r(dir);
            r.step();
            bool threw = false;
            try { r.ext->event({{"id", "preset"}, {"value", "nope"}}); } catch (...) { threw = true; }
            expect(threw && !r.error().empty(), "an unknown preset is refused");
            r.ext->event({{"id", "zoom"}, {"value", 99}});
            expect(r.ext->model()["values"]["zoom"].get<double>() <= 2.0, "values are clamped");
        }
        fs::remove_all(dir);
        std::cout << checks << " CINE director checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        fs::remove_all(dir);
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}

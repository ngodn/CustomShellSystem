// Perf Tuner tests against a mock CSSX host: the catalog's rules, the trial maths, and the
// tuner's apply/measure/restore over the bridge. Run with the extension directory as argv[1].
#include "tuner.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

using tuner::Json;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
template<class F> std::string rejects(F f) { try { f(); } catch (const std::exception& e) { return e.what(); } throw std::runtime_error("Expected a rejection"); }
Json object(uint64_t id) { return {{"$object", id}}; }

// A host with a console: variables with values, a frame clock whose median depends on which
// variables are set, and a player that can come and go.
struct Host {
    fs::path dir;
    Json state = Json::object();
    std::map<std::string, double> cvars;
    std::map<std::string, double> cost_ms;      // extra frame time per variable while it is at its catalog value, negative saves time
    std::map<std::string, double> catalog_value;
    std::map<std::string, double> refuse;       // variables this build refuses: they keep their value
    double base_ms = 20.0, noise_ms = 0;
    bool player = true;
    std::vector<std::string> commands, logs;
    std::map<std::string, std::string> outputs;   // output.write files
    // A small modelled world for the audit: actors with a class, a tick flag and components.
    struct Part { std::string kind; Json option = 2; bool uro = true, seen = true, shadows = false, active = true; };
    struct Thing { std::string cls; bool tick = false; std::vector<uint64_t> parts; };
    std::map<uint64_t, Thing> things;
    std::map<uint64_t, Part> parts;
    double anim_cost_ms = 0;   // per always-animating non-player mesh
    bool world_model = false;
    std::string saved_dir;                        // GetProjectSavedDirectory
    size_t requests = 0;
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};
    static int request(void* context, const char* value, CssxSink sink, void* output) {
        try { auto r = static_cast<Host*>(context)->handle(Json::parse(value)).dump(); sink(output, r.data(), r.size()); return 1; }
        catch (const std::exception& e) { auto r = Json{{"error", e.what()}}.dump(); sink(output, r.data(), r.size()); return 0; }
    }
    double frame_ms() const {
        double ms = base_ms + noise_ms;
        for (const auto& [id, p] : parts) if (p.kind == "skeletal" && (tuner::console_number(p.option.is_number() ? p.option.get<double>() : 0) == "0" || tuner::console_number(p.option.is_number() ? p.option.get<double>() : 0) == "1") && id != 11) ms += anim_cost_ms;
        for (const auto& [name, cost] : cost_ms) { auto it = cvars.find(name); if (it != cvars.end() && catalog_value.count(name) && it->second == catalog_value.at(name)) ms += cost; }
        return ms;
    }
    Json handle(const Json& j) {
        ++requests;
        const auto op = j.at("op").get<std::string>();
        if (op == "state.load") return state;
        if (op == "state.save") { state = j.at("value"); return true; }
        if (op == "log") { logs.push_back(j.at("message").get<std::string>()); return nullptr; }
        if (op == "invalidate") return nullptr;
        if (op == "output.write") { outputs[j.at("file").get<std::string>()] = j.at("text").get<std::string>(); return true; }
        if (op == "extension.info") return {{"directory", dir.string()}, {"id", "eins0fx.perf-tuner"}};
        if (op == "player") return player ? Json{{"pawn", object(1)}, {"controller", object(2)}} : Json{{"pawn", nullptr}};
        if (op == "find") return object(50);
        if (op == "load") { const auto path = j.at("path").get<std::string>(); return object(path.ends_with("Character") ? 60 : path.ends_with("SkeletalMeshComponent") ? 61 : path.ends_with("LightComponent") ? 62 : path.ends_with("NiagaraComponent") ? 63 : 64); }
        if (op == "valid") return things.count(j.at("target").at("$object").get<uint64_t>()) || parts.count(j.at("target").at("$object").get<uint64_t>());
        if (op == "get" && world_model) {
            const auto id = j.at("target").at("$object").get<uint64_t>(); const auto name = j.at("property").get<std::string>();
            const auto& p = parts.at(id);
            if (name == "VisibilityBasedAnimTickOption") return p.option;
            if (name == "bEnableUpdateRateOptimizations") return p.uro;
            if (name == "CastShadows") return p.shadows;
            if (name == "Mobility") return 2;
            throw std::runtime_error("Unknown property " + name);
        }
        if (op == "set" && world_model) {
            const auto id = j.at("target").at("$object").get<uint64_t>();
            if (j.at("property") == "VisibilityBasedAnimTickOption") { parts.at(id).option = j.at("value"); return j.at("value"); }
            throw std::runtime_error("Unknown property");
        }
        if (op == "frame.brief") { const double ms = frame_ms(); return {{"hz", 1000 / ms}, {"median_ms", ms}, {"mean_ms", ms}, {"p99_ms", ms * 1.5}, {"frames", 400}}; }
        if (op == "call") {
            const auto fn = j.at("function").get<std::string>();
            const auto& args = j.at("args");
            if (fn == "GetProjectSavedDirectory") return {{"ReturnValue", saved_dir}};
            if (fn == "GetAllActorsOfClass") {
                // Characters (class 60) are the first family; every other family lists everything else, with a class on each handle like the bridge does.
                const bool characters = args.at("ActorClass").at("$object").get<uint64_t>() == 60;
                Json list = Json::array();
                for (const auto& [id, t] : things) if (characters == t.cls.starts_with("BP_")) { Json h = object(id); h["class"] = "BlueprintGeneratedClass /Game/X/" + t.cls + "." + t.cls; list.push_back(h); }
                return {{"OutActors", list}};
            }
            if (fn == "IsActorTickEnabled") return {{"ReturnValue", things.at(j.at("target").at("$object").get<uint64_t>()).tick}};
            if (fn == "K2_GetComponentsByClass") {
                const auto id = j.at("target").at("$object").get<uint64_t>(); const auto cls = args.at("ComponentClass").at("$object").get<uint64_t>();
                const std::string want = cls == 61 ? "skeletal" : cls == 62 ? "light" : "niagara";
                Json list = Json::array(); for (auto pid : things.at(id).parts) if (parts.at(pid).kind == want) list.push_back(object(pid));
                return {{"ReturnValue", list}};
            }
            if (fn == "WasRecentlyRendered") return {{"ReturnValue", parts.at(j.at("target").at("$object").get<uint64_t>()).seen}};
            if (fn == "IsActive") return {{"ReturnValue", parts.at(j.at("target").at("$object").get<uint64_t>()).active}};
            if (fn == "GetConsoleVariableStringValue") { const auto n = args.at("VariableName").get<std::string>(); auto it = cvars.find(n); return {{"ReturnValue", it == cvars.end() ? std::string() : tuner::console_number(it->second)}}; }
            if (fn == "GetConsoleVariableFloatValue") { const auto n = args.at("VariableName").get<std::string>(); auto it = cvars.find(n); return {{"ReturnValue", it == cvars.end() ? 0.0 : it->second}}; }
            if (fn == "ExecuteConsoleCommand") {
                const auto c = args.at("Command").get<std::string>();
                commands.push_back(c);
                const auto sp = c.find(' ');
                const auto name = c.substr(0, sp); const double v = std::stod(c.substr(sp + 1));
                if (!refuse.count(name)) cvars[name] = v;
                return nullptr;
            }
            throw std::runtime_error("Unknown function " + fn);
        }
        throw std::runtime_error("Unknown op " + op);
    }
};
Json catalog_json() {
    return Json::parse(R"({"schema":1,"tweaks":[
        {"id":"alpha","title":"Alpha","group":"GPU","what":"Saves time on the GPU without changing a pixel.","side":"gpu","set":[{"cvar":"r.Alpha","value":1}]},
        {"id":"beta","title":"Beta","group":"CPU","what":"Saves time on the game thread.","side":"cpu","on":false,"set":[{"cvar":"r.Beta","value":2},{"cvar":"r.Beta.Two","value":0.5}]},
        {"id":"gamma","title":"Gamma","group":"GPU","what":"Costs more, for the test.","side":"gpu","picture":"check","set":[{"cvar":"r.Gamma","value":4}]}
    ]})");
}
void write_catalog(const fs::path& dir, const Json& j) { fs::create_directories(dir); std::ofstream(dir / "tweaks.json") << j.dump(2); }
// Run ticks until the trial is over or the time runs out.
void run(tuner::Tuner& t, double seconds, double step = 0.1) { for (double s = 0; s < seconds; s += step) t.tick(step); }
}

int main(int argc, char** argv) {
    try {
        const fs::path extension = argc > 1 ? fs::path(argv[1]) : fs::path(".");
        const fs::path scratch = fs::temp_directory_path() / "perf-tuner-tests";
        fs::remove_all(scratch);

        // ---- catalog rules
        {
            using tuner::Catalog;
            const auto c = Catalog::parse(catalog_json());
            expect(c.tweaks.size() == 3 && c.find("beta") && !c.find("beta")->on && c.find("beta")->set.size() == 2, "catalog parses");
            auto bad = [&](const char* patch) { Json j = catalog_json(); j["tweaks"][0].merge_patch(Json::parse(patch)); return rejects([&] { Catalog::parse(j); }); };
            expect(bad(R"({"set":[{"cvar":"sg.ShadowQuality","value":0}]})").find("quality levels") != std::string::npos, "scalability groups are refused");
            expect(bad(R"({"set":[{"cvar":"r.ScreenPercentage","value":50}]})").find("picture quality") != std::string::npos, "screen percentage is refused");
            expect(bad(R"({"set":[{"cvar":"t.MaxFPS","value":60}]})").find("frame caps") != std::string::npos, "frame caps are refused");
            expect(bad(R"({"set":[{"cvar":"r.Nanite.MaxPixelsPerEdge","value":1.5}]})").find("Nanite") != std::string::npos, "Nanite detail is refused");
            expect(bad(R"({"set":[{"cvar":"r.Streaming.MipBias","value":1}]})").find("blurs") != std::string::npos, "mip bias is refused");
            expect(bad(R"({"set":[{"cvar":"r.NGX.DLSS.Quality","value":1}]})").find("upscaler") != std::string::npos, "upscaler choice is refused");
            expect(bad(R"({"picture":"lower"})").find("picture") != std::string::npos, "a picture-lowering tweak is refused");
            expect(bad(R"({"set":[{"cvar":"r.Beta","value":1}]})").find("already set") != std::string::npos, "two tweaks may not set the same variable");
            expect(bad(R"({"set":[{"cvar":"r.X","value":"1"}]})").find("numeric") != std::string::npos, "values are numbers");
            expect(bad(R"({"set":[{"cvar":"r.X Y","value":1}]})").find("not a console variable") != std::string::npos, "names are validated");
            expect(bad(R"({"what":""})").find("'what'") != std::string::npos, "every tweak says what it does");
            expect(tuner::console_number(1) == "1" && tuner::console_number(0.5) == "0.5" && tuner::console_number(3000) == "3000" && tuner::console_number(61.1) == "61.1", "console numbers print plainly");
            // The shipped catalog obeys the same rules.
            std::ifstream shipped(extension / "tweaks.json");
            expect(bool(shipped), "the shipped tweaks.json exists");
            const auto real = Catalog::parse(Json::parse(shipped));
            expect(!real.tweaks.empty(), "the shipped catalog has tweaks");
            for (const auto& t : real.tweaks) expect(t.what.find(" ") != std::string::npos && t.what.back() == '.', "shipped tweak '" + t.id + "' explains itself in sentences");
        }

        // ---- trial maths
        {
            using tuner::Sample; using tuner::judge;
            auto s = [](double median, double p99 = 0) { Sample x; x.median_ms = median; x.p99_ms = p99 ? p99 : median * 1.4; x.mean_ms = median; x.frames = 300; return x; };
            auto r = judge({s(20), s(20.4)}, {s(18.5), s(18.9)});
            expect(r.verdict == "gain" && r.consistent && r.gain_pct > 6 && r.gain_pct < 8, "a consistent gain is a gain");
            r = judge({s(20), s(20.4)}, {s(19.9), s(20.3)});
            expect(r.verdict == "same", "a change inside the threshold is 'same'");
            r = judge({s(20), s(20.4)}, {s(21.5), s(21.2)});
            expect(r.verdict == "loss" && r.gain_pct < -1.5, "a consistent loss is a loss");
            r = judge({s(20), s(22)}, {s(18.5), s(21)});
            expect(r.verdict == "unclear" && !r.consistent, "overlapping rounds are unclear even with a good average");
            r = judge({}, {s(1)});
            expect(r.verdict == "unclear", "missing windows are unclear");
            tuner::Trial trial;
            tuner::TrialPlan plan; plan.settle = 1; plan.measure = 4; plan.rounds = 2;
            trial.start(0, plan);
            int switches = 0, samples = 0; std::vector<bool> conditions;
            for (double now = 0; now <= 60 && trial.running(); now += 0.1) {
                const auto step = trial.advance(now);
                if (step == tuner::Trial::Step::Switch) { ++switches; conditions.push_back(trial.change_on()); }
                if (step == tuner::Trial::Step::Sample) { ++samples; trial.record(s(trial.change_on() ? 18 : 20)); }
            }
            expect(switches == 4 && samples == 4 && conditions == std::vector<bool>{false, true, false, true}, "off/on/off/on, one sample per window");
            expect(!trial.running() && trial.result().verdict == "gain", "the trial ends with a verdict");
            trial.start(0, plan);
            trial.advance(0); trial.advance(3);
            expect(trial.progress(3) > 0.1 && trial.progress(3) < 0.2, "progress counts settle and measure time");
        }

        // ---- the tuner over the bridge
        {
            Host host; host.dir = scratch / "ext"; write_catalog(host.dir, catalog_json());
            host.cvars = {{"r.Alpha", 0}, {"r.Beta", 1}, {"r.Beta.Two", 1}, {"r.Gamma", 2}};
            host.catalog_value = {{"r.Alpha", 1}, {"r.Beta", 2}, {"r.Beta.Two", 0.5}, {"r.Gamma", 4}};
            host.cost_ms = {{"r.Alpha", -2.0}, {"r.Gamma", +3.0}};
            tuner::Tuner t(&host.api);
            expect(t.catalog_problem().empty() && !t.enabled() && t.tweak_on("alpha") && !t.tweak_on("beta"), "fresh state: off, catalog defaults");
            auto m = t.model();
            expect(m.at("sections").size() == 5, "menu: tuner, two groups, world, check");
            expect(host.commands.empty(), "nothing is set while the tuner is off");

            t.event({{"id", "enable"}, {"value", true}});
            expect(host.cvars["r.Alpha"] == 1 && host.cvars["r.Beta"] == 1 && host.cvars["r.Gamma"] == 4, "enabling applies the tweaks that are on and leaves the off one");
            expect(t.applied().count("alpha") && t.applied().at("alpha").size() == 1 && t.applied().at("alpha")[0].before == 0, "what was read before is remembered");
            expect(host.state.value("enable", false), "the choice is saved");

            t.event({{"id", "tweak.beta"}, {"value", true}});
            expect(host.cvars["r.Beta"] == 2 && host.cvars["r.Beta.Two"] == 0.5 && t.applied().count("beta"), "turning a tweak on applies it live");
            t.event({{"id", "tweak.beta"}, {"value", false}});
            expect(host.cvars["r.Beta"] == 1 && host.cvars["r.Beta.Two"] == 1 && !t.applied().count("beta"), "turning it off puts both variables back");

            // Measure alpha: the host's frame is 2 ms faster with it; the verdict must say so and the live copy must come back.
            t.event({{"id", "measure_target"}, {"value", "alpha"}});
            t.event({{"id", "measure"}});
            run(t, 70);
            const auto saved = host.state.at("results").at("alpha");
            expect(saved.at("verdict") == "gain" && saved.value("gain_pct", 0.0) > 8 && saved.value("gain_pct", 0.0) < 9.5, "alpha measures as a gain of 2 ms in 23 (gamma is live too)");
            expect(host.cvars["r.Alpha"] == 1 && t.applied().count("alpha") && !t.applied().count("trial.alpha"), "after the trial the live copy is back and the trial copy is gone");
            // Measure gamma: slower.
            t.event({{"id", "measure_target"}, {"value", "gamma"}});
            t.event({{"id", "measure"}});
            run(t, 70);
            expect(host.state.at("results").at("gamma").at("verdict") == "loss", "gamma measures as a loss");
            expect(host.cvars["r.Gamma"] == 4, "a loss does not turn the tweak off by itself; that is the player's call");

            // The probe: at 20 ms with a 'GPU' that scales with pixels.
            host.cvars["r.ScreenPercentage"] = 67;
            host.base_ms = 18; // frame_ms will not change with SP in this mock: that reads as CPU-bound
            t.event({{"id", "probe"}});
            run(t, 25);
            expect(host.state.at("probe").at("verdict") == "fixed" && host.cvars["r.ScreenPercentage"] == 67, "a frame that ignores the pixel count does not scale with pixels, and the resolution is put back");

            // A refused variable: the tweak is skipped and nothing half-applied stays.
            host.refuse["r.Beta.Two"] = 1;
            t.event({{"id", "tweak.beta"}, {"value", true}});
            expect(host.cvars["r.Beta"] == 1 && !t.applied().count("beta"), "a tweak with a refused variable is rolled back whole");
            expect(t.model().dump().find("keeps one of its variables") != std::string::npos, "and the menu says why");

            // The world goes away and comes back: nothing re-set twice, nothing lost.
            host.player = false; run(t, 2); host.player = true; run(t, 2);
            expect(host.cvars["r.Alpha"] == 1 && t.applied().count("alpha"), "after a world change the tweak is owned again");

            // Stop restores everything that was changed.
            t.stop();
            expect(host.cvars["r.Alpha"] == 0 && host.cvars["r.Gamma"] == 2 && t.applied().empty(), "stop puts every variable back");
        }
        // ---- the player's Engine.ini
        {
            using tuner::scan_engine_ini;
            const std::string text = "; Ultimate Engine Tweaks - Created by P40L0\r\n[/Script/Engine.Engine]\r\nbSmoothFrameRate=0\r\n[ConsoleVariables]\r\nr.Alpha=5 ; a comment\r\nr.Nanite.MaxPixelsPerEdge=1.5\r\nr.Gone=1\r\nr.RayTracing.Enable=1\r\nr.Kept=3\r\n+r.Plus=1\r\n\r\n[Other]\r\nIgnored=1\r\n";
            const auto scan = scan_engine_ini(text);
            expect(scan.pack == "Ultimate Engine Tweaks (P40L0)" && !scan.written_by_game && scan.lines.size() == 7, "the pack is recognised and the lines counted");
            expect(scan.lines[0].setting && scan.lines[0].name == "bSmoothFrameRate" && scan.lines[1].name == "r.Alpha" && scan.lines[1].value == "5" && scan.lines[6].name == "r.Plus", "settings, comments and + prefixes are handled");
            expect(tuner::game_owned_cvar("r.RayTracing.Enable") && tuner::game_owned_cvar("t.Streamline.Reflex.Enable") && !tuner::game_owned_cvar("r.Kept"), "the game's own lines are known");
            expect(tuner::picture_cvar("r.Tonemapper.Sharpen") && tuner::picture_cvar("r.Shadow.Virtual.SMRT.RayCountDirectional") && tuner::picture_cvar("sg.ShadowQuality") && !tuner::picture_cvar("gc.TimeBetweenPurgingPendingKillObjects"), "picture variables are known");

            Host host; host.dir = scratch / "ini"; write_catalog(host.dir, catalog_json());
            host.cvars = {{"r.Alpha", 5}, {"r.Nanite.MaxPixelsPerEdge", 1.5}, {"r.RayTracing.Enable", 1}, {"r.Kept", 4}, {"r.Plus", 1}};
            host.saved_dir = (scratch / "saved").string() + "/";
            fs::create_directories(scratch / "saved" / "Config" / "Windows");
            std::ofstream(scratch / "saved" / "Config" / "Windows" / "Engine.ini") << text;
            tuner::Tuner t(&host.api);
            run(t, 2);   // the world is up: the file is read once
            const auto& v = t.ini_verdicts();
            expect(v.size() == 7, "every line gets a verdict");
            auto kind = [&](const char* name) { for (const auto& x : v) if (x.line.name == name) return x.kind; return std::string("missing"); };
            expect(kind("bSmoothFrameRate") == "setting" && kind("r.Alpha") == "managed" && kind("r.Nanite.MaxPixelsPerEdge") == "picture" && kind("r.Gone") == "dead" && kind("r.RayTracing.Enable") == "game" && kind("r.Kept") == "kept", "lines are judged: managed, picture, dead, game, kept");
            expect(host.outputs.count("engine-ini-report.txt") && host.outputs["engine-ini-report.txt"].find("[dead]  r.Gone=1") != std::string::npos, "the report file lists every line with its verdict");
            const auto menu = t.model().dump();
            expect(menu.find("Your Engine.ini") != std::string::npos && menu.find("r.Nanite.MaxPixelsPerEdge = 1.5") != std::string::npos && menu.find("r.Gone") == std::string::npos, "the menu shows the picture and managed lines, not the dead ones");
            expect(menu.find("1 change the look") != std::string::npos && menu.find("Ultimate Engine Tweaks") != std::string::npos, "the check sums the file up");
            // The real UET pack, when the research copy is there: nothing in it may crash the scanner.
            std::ifstream uet(extension / "work/research/uet/UET_-_Ultimate_Engine_Tweaks_-_Engine_ini_(No_VRR)/Engine.ini");
            if (uet) {
                const std::string whole((std::istreambuf_iterator<char>(uet)), std::istreambuf_iterator<char>());
                const auto real = scan_engine_ini(whole);
                expect(real.pack == "Ultimate Engine Tweaks (P40L0)" && real.lines.size() > 300, "the real UET file scans");
                size_t picture = 0; for (const auto& l : real.lines) if (!l.setting && tuner::picture_cvar(l.name)) ++picture;
                expect(picture >= 20, "the UET pack's picture-changing lines are recognised (" + std::to_string(picture) + ")");
            }
        }
        // ---- the world audit and its fix
        {
            Host host; host.dir = scratch / "world"; write_catalog(host.dir, catalog_json());
            host.cvars = {{"r.Alpha", 0}, {"r.Beta", 1}, {"r.Beta.Two", 1}, {"r.Gamma", 2}};
            host.world_model = true; host.anim_cost_ms = 1.0; host.base_ms = 20;
            // The player (id 1) animates always, as the game sets it; two grunts animate always off screen; a prop is seen; a torch and smoke.
            host.things[1] = {"BP_Player_C", true, {11}}; host.parts[11] = {"skeletal", 0};
            host.things[2] = {"BP_Grunt_C", true, {21, 22}}; host.parts[21] = {"skeletal", 1, false, false}; host.parts[22] = {"niagara"};
            host.things[3] = {"BP_Grunt_C", true, {31}}; host.parts[31] = {"skeletal", 0, true, false};
            host.things[4] = {"BP_Statue_C", false, {41}}; host.parts[41] = {"skeletal", 2};
            host.things[5] = {"BP_Torch_C", false, {51}}; host.parts[51] = {"light", 2, true, true, true};
            host.things[6] = {"StaticMeshActor", false, {}};
            tuner::Tuner t(&host.api);
            t.event({{"id", "audit"}});
            run(t, 3);
            const auto st = host.state.at("audit");
            expect(st.at("actors") == 6 && st.at("ticking") == 3 && st.at("skeletal") == 4 && st.at("skeletal_always") == 3 && st.at("skeletal_always_unseen") == 2 && st.at("skeletal_no_uro") == 1
                && st.at("lights") == 1 && st.at("lights_shadow") == 1 && st.at("lights_shadow_movable") == 1 && st.at("niagara") == 1 && st.at("niagara_active") == 1, "the audit counts the world");
            expect(st.at("classes").at("BP_Grunt_C") == 2 && st.at("always_classes").at("BP_Grunt_C") == 2, "classes are tallied");
            expect(host.outputs.count("world-audit-report.txt") && host.outputs["world-audit-report.txt"].find("2  BP_Grunt_C") != std::string::npos, "the report names the classes");
            expect(host.parts[21].option == 1 && host.parts[31].option == 0, "the audit changes nothing");
            // The fix: only with the tuner on; the player is never touched.
            t.event({{"id", "fix.anim"}, {"value", true}});
            expect(host.parts[21].option == 1, "the fix waits for the tuner");
            t.event({{"id", "enable"}, {"value", true}});
            expect(host.parts[21].option == 2 && host.parts[31].option == 2 && host.parts[11].option == 0 && host.parts[41].option == 2, "the fix sets the engine default on the grunts and leaves the player");
            // Measure it: 2 ms of 22 saved.
            t.event({{"id", "measure_target"}, {"value", "fix.anim"}});
            t.event({{"id", "measure"}});
            run(t, 70);
            expect(host.state.at("results").at("fix.anim").at("verdict") == "gain" && host.parts[21].option == 2, "the fix measures as a gain and stays applied after");
            t.event({{"id", "fix.anim"}, {"value", false}});
            expect(host.parts[21].option == 1 && host.parts[31].option == 0, "turning the fix off puts the meshes back as they were");
            t.event({{"id", "fix.anim"}, {"value", true}});
            expect(host.parts[21].option == 2, "and on again applies without a new audit");
            t.stop();
            expect(host.parts[21].option == 1 && host.parts[31].option == 0 && host.cvars["r.Alpha"] == 0, "stop restores the meshes and the variables");
        }
        // ---- Tune: the one button
        {
            Host host; host.dir = scratch / "tune";
            Json cat = catalog_json();
            cat["tweaks"][1]["on"] = false;   // beta: off by default, a gain when measured
            cat["tweaks"][2]["on"] = false;   // gamma: off by default, a loss when measured
            write_catalog(host.dir, cat);
            host.cvars = {{"r.Alpha", 0}, {"r.Beta", 1}, {"r.Beta.Two", 1}, {"r.Gamma", 2}, {"r.ScreenPercentage", 67}};
            host.catalog_value = {{"r.Alpha", 1}, {"r.Beta", 2}, {"r.Beta.Two", 0.5}, {"r.Gamma", 4}};
            host.cost_ms = {{"r.Alpha", -2.0}, {"r.Beta", -1.5}, {"r.Gamma", +3.0}};
            host.world_model = true; host.anim_cost_ms = 1.0; host.base_ms = 20;
            host.things[1] = {"BP_PlayerCharacter_C", true, {11}}; host.parts[11] = {"skeletal", 0};
            host.things[2] = {"BP_Grunt_C", true, {21}}; host.parts[21] = {"skeletal", 1, false, false};
            host.things[3] = {"BP_Grunt_C", true, {31}}; host.parts[31] = {"skeletal", 0, true, false};
            host.things[5] = {"BP_Torch_C", false, {51}}; host.parts[51] = {"light", 2, true, true, true};
            tuner::Tuner t(&host.api);
            run(t, 2);
            t.event({{"id", "tune"}});
            expect(t.enabled() && host.cvars["r.Alpha"] == 1, "tune turns the tuner on and applies the always-on set");
            bool saw_tuning = false;
            for (int i = 0; i < 4000; ++i) { t.tick(0.1); if (i == 50) saw_tuning = t.model().at("status").get<std::string>().find("Tuning, step") != std::string::npos; if (host.state.value("tune", Json()).is_object() && host.state["tune"].contains("when")) break; }
            expect(saw_tuning, "the status says which step runs");
            const auto res = host.state.at("tune");
            expect(res.contains("when"), "tune finishes within its budget");
            expect(host.cvars["r.Beta"] == 2 && host.state.at("choices").at("beta") == true, "a switch that wins is kept and live");
            expect(host.cvars["r.Gamma"] == 2 && host.state.at("choices").at("gamma") == false, "a switch that loses is left off");
            expect(host.state.at("anim_fix") == true && host.parts[21].option == 2 && host.parts[11].option == 0, "the world fix is kept when it wins, and the player is untouched");
            const auto kept = res.value("kept", std::string());
            expect(kept.find("Beta") != std::string::npos && kept.find("Animate only") != std::string::npos && res.value("dropped", std::string()).find("Gamma") != std::string::npos, "the summary names what was kept and dropped: " + kept);
            expect(res.value("whole_verdict", std::string()) == "gain" && res.value("whole_gain_pct", 0.0) > 10, "the kept set as a whole measures as a gain");
            expect(res.value("probe", std::string()) == "fixed", "the probe ran too");
            expect(t.model().dump().find("Kept Beta") != std::string::npos, "the Tune row shows what it kept");
            t.stop();
            expect(host.cvars["r.Alpha"] == 0 && host.cvars["r.Beta"] == 1 && host.parts[21].option == 1, "stop puts everything back after a tune");
        }
        // ---- fit rules
        {
            using tuner::Machine; using tuner::Tweak; using tuner::unfit_reason;
            Tweak t; t.side = "gpu";
            Machine win; win.gpu_vendor = "amd"; win.vram_mb = 8192;
            Machine proton = win; proton.wine = true; proton.gpu_vendor = "nvidia"; proton.vram_mb = 12288;
            Machine deck = proton; deck.steam_deck = true; deck.gpu_vendor = "amd";
            expect(unfit_reason(t, win).empty() && unfit_reason(t, proton).empty(), "a plain tweak fits everywhere");
            t.platforms = {"proton"};
            expect(!unfit_reason(t, win).empty() && unfit_reason(t, proton).empty(), "platform filter");
            t.platforms.clear(); t.gpus = {"nvidia"};
            expect(unfit_reason(t, win).find("NVIDIA") != std::string::npos && unfit_reason(t, proton).empty(), "gpu filter names the maker");
            t.gpus.clear(); t.min_vram_mb = 7000;
            Machine small = win; small.vram_mb = 6144;
            expect(unfit_reason(t, small).find("6 GB") != std::string::npos && unfit_reason(t, win).empty(), "memory floor");
            expect(unfit_reason(t, deck).find("Steam Deck") != std::string::npos, "Steam Deck keeps its profile");
            Tweak cpu; cpu.side = "cpu";
            expect(unfit_reason(cpu, deck).empty(), "CPU-side tweaks still fit the Deck");
            tuner::Setting pool; pool.value = 1500; pool.vram_fraction = 0.225; pool.vram_min = 800; pool.vram_max = 3000;
            expect(pool.resolve(0) == 1500 && pool.resolve(12288) == 2765 && pool.resolve(2048) == 800 && pool.resolve(32768) == 3000, "pool values scale with memory and clamp");
        }
        // A broken catalog: the menu says so, nothing applies.
        {
            Host host; host.dir = scratch / "broken"; fs::create_directories(host.dir); std::ofstream(host.dir / "tweaks.json") << "{\"schema\":1,\"tweaks\":[{\"id\":\"x\"}]}";
            tuner::Tuner t(&host.api);
            expect(!t.catalog_problem().empty(), "a broken catalog is reported");
            expect(t.model().dump().find("tweaks.json") != std::string::npos, "the menu shows the catalog problem");
            expect(rejects([&] { t.event({{"id", "enable"}, {"value", true}}); }).find("tweaks.json") != std::string::npos && host.commands.empty(), "a broken catalog cannot be enabled and nothing is set");
        }
        fs::remove_all(scratch);
        std::cout << checks << " Perf Tuner checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
}

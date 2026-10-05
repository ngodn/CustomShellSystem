// CINE logic tests: shot maths, presets and the shipped preset files. Run with the
// extension directory as argv[1] (CMake passes it).
#include "callouts.hpp"
#include "layers.hpp"
#include "preset.hpp"
#include "shot.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace cine;
namespace fs = std::filesystem;
static unsigned checks = 0;
static void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
template <class F> static void rejects(F f, const std::string& what) {
    bool threw = false; try { f(); } catch (const std::exception&) { threw = true; } expect(threw, "accepted: " + what);
}
static bool near(double a, double b, double eps) { return std::abs(a - b) <= eps; }

// A straight walk along +X: `idle` standing samples, then `meters` of walking at 184 cm/s.
static std::vector<Sample> straight_walk(double meters, int idle = 10, double y = 0) {
    std::vector<Sample> s; double t = 0;
    for (int i = 0; i < idle; ++i, t += 0.1) s.push_back({t, {0, y, 100}, 0, 0});
    for (double x = 0; x <= meters * 100; x += 18.4, t += 0.1) s.push_back({t, {x, y, 100}, 0, 184});
    for (int i = 0; i < idle; ++i, t += 0.1) s.push_back({t, {meters * 100, y, 100}, 0, 0});
    return s;
}

static void geometry() {
    expect(near(wrap_degrees(190), -170, 1e-9) && near(wrap_degrees(-180), 180, 1e-9) && near(wrap_degrees(540), 180, 1e-9), "wrap_degrees");
    expect(near(heading({0, 0, 0}, {0, 10, 0}), 90, 1e-9), "heading +Y is 90");
    const Vec3 w = to_world({100, 0, 0}, 90, {10, 0, 5});
    expect(near(w.x, 100, 1e-9) && near(w.y, 10, 1e-9) && near(w.z, 5, 1e-9), "to_world rotates forward onto +Y at yaw 90");
    const Rotator r = look_at({0, 0, 0}, {100, 0, 100});
    expect(near(r.pitch, 45, 1e-9) && near(r.yaw, 0, 1e-9), "look_at pitch/yaw");
    expect(smooth_factor(5.0, 0.6) <= 0.12 / 0.6 + 1e-12, "a slow frame must not make a filter leap");
    expect(smooth_factor(-1, 0.6) == 0 && smooth_factor(0.05, 0) == 1, "smooth_factor edges");
}

static void routes() {
    const auto walk = straight_walk(20);
    const Route r = build_route(walk, std::nullopt);
    expect(r.points.size() >= 15 && r.points.size() <= 19, "20 m decimates to ~17 points at 1.2 m");
    // Every segment is ~1.2 m except the last, which keeps the true end of the walk.
    for (size_t i = 1; i + 1 < r.points.size(); ++i) expect(flat_distance(r.points[i - 1], r.points[i]) >= 119, "spacing >= 1.2 m");
    expect(flat_distance(r.points[r.points.size() - 2], r.points.back()) > 0, "the end point is kept");
    expect(near(r.length(), 2000, 40), "route length");
    expect(near(r.start_yaw, 0, 1e-6), "she starts facing down the path");

    // A short setup nudge before the real walk is not the route.
    auto nudged = walk;
    std::vector<Sample> pre; double t = -3;
    for (int i = 0; i < 4; ++i, t += 0.1) pre.push_back({t, {-500 + i * 15.0, 0, 100}, 0, 150});
    for (int i = 0; i < 10; ++i, t += 0.1) pre.push_back({t, {-440, 0, 100}, 0, 0});
    nudged.insert(nudged.begin(), pre.begin(), pre.end());
    expect(near(build_route(nudged, std::nullopt).start.x, 0, 20), "the longest run is the route, not the nudge");

    rejects([] { build_route(std::vector<Sample>(30, Sample{0, {0, 0, 0}, 0, 0}), std::nullopt); }, "a recording with no walk");
    rejects([] { build_route(straight_walk(1.0), std::nullopt); }, "a walk under 3 m");

    // Branch into a final shot ahead and to the left, facing 30 degrees.
    FinalShot f; f.position = {2300, 150, 100}; f.yaw = 30;
    const Route b = build_route(walk, f);
    expect(near(b.points.back().x, 2300, 1e-6) && near(b.points.back().y, 150, 1e-6), "the route ends on the final spot");
    const auto& p = b.points;
    expect(near(heading(p[p.size() - 2], p.back()), 30, 12), "she arrives already facing the final direction");
    for (size_t i = 2; i < p.size(); ++i)
        expect(std::abs(wrap_degrees(heading(p[i - 1], p[i]) - heading(p[i - 2], p[i - 1]))) < 30, "no sharp turn on the curve");
    FinalShot behind; behind.position = {-800, 0, 100}; behind.yaw = 180;
    rejects([&] { build_route(walk, behind); }, "a final spot that needs a U-turn");
}

static void tracks() {
    const Route r = build_route(straight_walk(20), std::nullopt);
    Track tr(r);
    expect(near(tr.at(0).x, r.points.front().x, 1e-9) && near(tr.at(1e9).x, r.points.back().x, 1e-9), "track ends clamp");
    double last = -1;
    for (double x = 0; x <= 2000; x += 50) {
        const double s = tr.project({x, 30, 100});
        expect(s >= last - 1e-6, "projection never runs backwards on a forward walk");
        expect(near(s, std::min(x, tr.length()), 61), "projection lands near the true arc length");
        last = s;
    }
    expect(near(tr.heading_at(500), 0, 1e-6), "heading along a straight route");
    rejects([] { Route one; one.points = {{0, 0, 0}}; Track bad(one); }, "a one-point track");
}

static void curves() {
    std::vector<OrbitKey> keys{{0, 25, 410, -45, 5, 42}, {4.5, 30, 330, -28, 8, 44}, {8.9, 90, 360, -5, 10, 42},
                               {13.4, 175, 360, 25, 30, 42}, {17.3, 270, 350, -10, 10, 40}, {21.3, 320, 260, 15, 40, 36},
                               {24.9, 360, 150, 55, 58, 30}};
    const OrbitCurve c(keys);
    for (const auto& k : c.keys()) {
        const auto v = c.at(k.t);
        expect(near(v.distance, k.distance, 1e-6) && near(v.azimuth, k.azimuth, 1e-6), "the curve passes through every key");
    }
    expect(near(c.keys().back().azimuth, 360, 1e-9), "a rising orbit is not wrapped back to 0");
    // C1 continuity at a key: slopes from the left and the right agree.
    for (size_t i = 1; i + 1 < keys.size(); ++i) {
        const double t = keys[i].t, h = 1e-4;
        const double left = (c.at(t).azimuth - c.at(t - h).azimuth) / h, right = (c.at(t + h).azimuth - c.at(t).azimuth) / h;
        expect(near(left, right, std::max(1.0, std::abs(left)) * 0.02), "azimuth speed is continuous through a key (no stop-and-go)");
    }
    const OrbitCurve wrap({{0, 350, 300, 0, 0, 40}, {2, 10, 300, 0, 0, 40}});
    expect(near(wrap.keys().back().azimuth, 370, 1e-9), "350 -> 10 goes the short way (+20)");
    rejects([] { OrbitCurve bad({{1, 0, 300, 0, 0, 40}, {1, 10, 300, 0, 0, 40}}); }, "keys out of time order");
    rejects([] { OrbitCurve bad({}); }, "no keys");
    const Pose p = c.pose(0, 0.5);
    expect(near(std::hypot(p.offset.x, p.offset.y), 205, 1e-6), "distance scale applies to the flat distance");
}

static void walls() {
    std::vector<double> raw(40, 1.0); raw[20] = 0.5;
    const WallProfile w(0.25, raw, 1.0);
    expect(w.tight(), "a wall pulls the camera in");
    expect(w.at(5.0) <= 0.5 + 1e-9, "the tightest point is honoured");
    expect(w.at(4.0) < 1.0 && w.at(6.0) < 1.0, "the pull-in eases over the hold window");
    expect(near(w.at(0), 1.0, 1e-9) && near(w.at(100), 1.0, 1e-9), "far from the wall it is untouched");
    for (double t = 0; t < 10; t += 0.05) expect(w.at(t) > 0 && w.at(t) <= 1.0 + 1e-12, "scale stays in (0, 1]");
    expect(!WallProfile(0.25, std::vector<double>(10, 1.0)).tight() && WallProfile().at(3) == 1.0, "no wall, no change");
}

static Json walk_preset() {
    return Json::parse(R"({"schema":1,"id":"t","name":"T","kind":"walk","intro":4.5,"tail":16,"max_seconds":49,
        "keys":[{"at":"start","azimuth":25,"distance":410},{"at":"walk","azimuth":30,"distance":330},{"at":0.5,"azimuth":200}],
        "look":{"steps":[{"at":0.2,"palette":"crimson","controls":{"coat":[0,0,0,1]}},{"at":0.6,"controls":{"gloves":1}}]}})");
}

static void presets() {
    const Preset p = Preset::parse(walk_preset());
    expect(p.kind == ShotKind::Walk && p.reference == Preset::Reference::Capsule && p.keys.size() == 3 && p.look.size() == 2, "walk preset parses");
    expect(p.look[1].controls.at("gloves")[0] == 1, "a bare number is channel 0");
    const Timeline tl = make_timeline(p, 4000, 184);
    expect(tl.total <= 49 + 1e-9 && tl.arrive < tl.total && near(tl.walk_start, 4.5, 1e-9), "walk timeline fits the cap");
    for (size_t i = 1; i < tl.keys.size(); ++i) expect(tl.keys[i].t > tl.keys[i - 1].t, "timeline keys strictly ordered");
    expect(tl.look.size() == 3 && tl.look.back().own && near(tl.look.back().at, tl.arrive - 2, 1e-9), "the take ends on the modder's own look");
    rejects([&] { make_timeline(p, 9000, 184); }, "a route too long for the cap");
    rejects([&] { make_timeline(p, 100, 184); }, "a route under 3 m");

    auto bad = [&](auto edit, const std::string& what) { auto j = walk_preset(); edit(j); rejects([&] { Preset::parse(j); }, what); };
    bad([](Json& j) { j.erase("id"); }, "missing id");
    bad([](Json& j) { j["id"] = "Bad Id"; }, "an id with spaces/capitals");
    bad([](Json& j) { j["kind"] = "dolly"; }, "unknown kind");
    bad([](Json& j) { j["schema"] = 2; }, "a future schema");
    bad([](Json& j) { j["keys"][0]["distance"] = 5; }, "a camera inside her");
    bad([](Json& j) { j["keys"][0]["fov"] = 179; }, "an absurd FOV");
    bad([](Json& j) { j["keys"] = Json::array({j["keys"][0]}); }, "a single key");
    bad([](Json& j) { j["look"]["steps"][0]["palette"] = "original"; }, "Original mid-shot");
    bad([](Json& j) { j["look"]["steps"][1]["at"] = 0.1; }, "look steps out of order");
    bad([](Json& j) { j["look"]["steps"][0]["controls"]["coat"] = "on"; }, "a text control value");
    bad([](Json& j) { j["reference"] = "feet"; }, "an unknown reference");
    bad([](Json& j) { j["intro"] = std::nan(""); }, "a non-finite number");

    auto glide = Json::parse(R"({"schema":1,"id":"g","name":"G","kind":"glide","duration":5,
        "keys":[{"at":0,"azimuth":240},{"at":1,"azimuth":300}]})");
    const Preset g = Preset::parse(glide);
    expect(g.reference == Preset::Reference::Head, "pose shots default to the head reference");
    expect(near(make_timeline(g, 0, 184).total, 5, 1e-9), "glide length is its duration");
    glide["keys"][0]["at"] = "walk";
    rejects([&] { Preset::parse(glide); }, "'walk' anchor outside a walk preset");
}

// Rotate +Z by a unit quaternion (x, y, z, w).
static Vec3 rotate_z(Quat q) {
    return {2 * (q.x * q.z + q.w * q.y), 2 * (q.y * q.z - q.w * q.x), 1 - 2 * (q.x * q.x + q.y * q.y)};
}
// Horizontal angle, in degrees, between the camera's aim direction and a point.
static double off_axis(const ActionView& v, Vec3 point) {
    return std::abs(wrap_degrees(heading(v.camera, point) - heading(v.camera, v.aim)));
}

static void layers() {
    expect(layers_from(Json::array({"clay", "wireframe"})) == (bit(Layer::Clay) | bit(Layer::Wireframe)), "layer names parse");
    expect(layers_from(Json::array()) == 0, "an empty list is the final image");
    rejects([] { layers_from(Json::array({"wirefram"})); }, "a misspelt layer");
    rejects([] { layers_from(Json("clay")); }, "layers that are not a list");
    for (unsigned i = 0; i < static_cast<unsigned>(Layer::Count); ++i) {
        const auto l = static_cast<Layer>(i);
        expect(layer_from(layer_id(l)) == l && std::string(layer_label(l)).size() > 2, std::string("layer round-trips: ") + layer_id(l));
    }
    expect((frame_layers & bone_layers) == 0, "a layer is a frame pass or a bone layer, not both");

    expect(classify_bone("VB hand_r", {}) == BoneKind::Virtual, "VB prefix is a virtual bone");
    expect(classify_bone("ik_foot_l", {}) == BoneKind::Ik && classify_bone("IK_Hand_Root", {}) == BoneKind::Ik, "ik_ prefix, any case");
    expect(classify_bone("brust001", {}) == BoneKind::Spring && classify_bone("Butt002", {}) == BoneKind::Spring && classify_bone("belly", {}) == BoneKind::Spring, "usual spring names");
    expect(classify_bone("spine_03", {}) == BoneKind::Regular && classify_bone("thigh_twist_02_l", {}) == BoneKind::Regular, "ordinary bones");
    // When the outfit names its spring bones, only those count.
    expect(classify_bone("thigh_twist_02_l", {"Thigh_Twist_02_L"}) == BoneKind::Spring, "outfit list wins, any case");
    expect(classify_bone("brust001", {"skirt_01"}) == BoneKind::Regular, "a name outside the outfit's list is ordinary");
    expect(spring_group("brust001") == "chest" && spring_group("breast_l") == "chest" && spring_group("butt002") == "glutes" && spring_group("glute_r") == "glutes"
           && spring_group("belly") == "belly" && spring_group("skirt_01").empty(), "spring groups");

    for (const Vec3 d : {Vec3{0, 0, 1}, Vec3{1, 0, 0}, Vec3{0, -3, 0}, Vec3{1, 2, 3}, Vec3{-4, 1, -2}, Vec3{0, 0, -1}, Vec3{1e-3, 0, -1}}) {
        const double n = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        const Quat q = z_to(d);
        const Vec3 z = rotate_z(q);
        expect(near(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w, 1, 1e-9), "z_to gives a unit quaternion");
        expect(near(z.x, d.x / n, 1e-6) && near(z.y, d.y / n, 1e-6) && near(z.z, d.z / n, 1e-6), "z_to turns +Z onto the direction");
    }
    const Quat none = z_to({0, 0, 0});
    expect(none.w == 1 && none.x == 0, "no direction: no rotation");
}

static void action() {
    ActionFrame f;   // duel, azimuth 70, fov 38
    const Vec3 her{0, 0, 100};
    // Fighters from arm's length to far apart, on several headings: both stay inside the frame.
    for (double separation : {120.0, 300.0, 600.0, 900.0}) {
        for (double bearing : {0.0, 45.0, 135.0, -90.0}) {
            const double b = bearing * 3.14159265358979 / 180;
            const Vec3 foe{std::cos(b) * separation, std::sin(b) * separation, 100};
            for (double azimuth : {70.0, 90.0, 20.0, -110.0}) {
                f.azimuth = azimuth;
                const auto v = action_view(f, her, &foe);
                const double half = v.fov / 2;
                const double d = flat_distance(v.camera, {(her.x + foe.x) / 2, (her.y + foe.y) / 2, 0});
                expect(d >= f.distance - 1e-6 && d <= f.distance_max + 1e-6, "duel distance stays in its range");
                if (d < f.distance_max - 1) expect(off_axis(v, her) < half && off_axis(v, foe) < half, "both fighters are inside the field of view");
                expect(near(v.focus, std::sqrt(d * d + (f.height - f.aim) * (f.height - f.aim)), 1e-6), "focus is the distance to the aim point");
            }
        }
    }
    f.azimuth = 70;
    const Vec3 near_foe{300, 0, 100}, far_foe{900, 0, 100};
    expect(flat_distance(action_view(f, her, &far_foe).camera, {450, 0, 0}) > flat_distance(action_view(f, her, &near_foe).camera, {150, 0, 0}),
           "the camera backs off as they separate");
    // The angle is held against the duel axis: turn the pair and the camera turns with it.
    const Vec3 turned{0, 300, 100};
    const auto a = action_view(f, her, &near_foe), b = action_view(f, her, &turned);
    expect(near(wrap_degrees(heading({150, 0, 0}, a.camera) - 0), 70, 1e-6) && near(wrap_degrees(heading({0, 150, 0}, b.camera) - 90), 70, 1e-6), "azimuth is measured from the duel axis");
    // No target, or on the same spot: falls back to following her.
    const auto alone = action_view(f, her, nullptr);
    expect(near(flat_distance(alone.camera, her), f.distance, 1e-6) && near(alone.aim.z, her.z + f.aim, 1e-9), "no target: follow framing");
    const Vec3 same{5, 0, 100};
    expect(near(flat_distance(action_view(f, her, &same).camera, her), f.distance, 1e-6), "overlapping fighters: follow framing");
    // Punch-in tightens distance and field of view, and never past the limits.
    ActionFrame punch = f; punch.punch = 1; ActionFrame over = f; over.punch = 9;
    expect(action_view(punch, her, nullptr).fov < alone.fov && flat_distance(action_view(punch, her, nullptr).camera, her) < f.distance, "a punch-in tightens");
    expect(near(action_view(over, her, nullptr).fov, action_view(punch, her, nullptr).fov, 1e-9), "punch is clamped to 1");
    // Shoulder: behind her (away from the target), looking past her at it.
    ActionFrame shoulder = f; shoulder.framing = Framing::Shoulder;
    const auto s = action_view(shoulder, her, &near_foe);
    expect(s.camera.x < 0 && s.aim.x > 100, "shoulder camera sits behind her and aims toward the target");
    ActionFrame other = shoulder; other.azimuth = -70;
    expect(s.camera.y * action_view(other, her, &near_foe).camera.y < 0, "the azimuth's sign picks the shoulder");
}

static void action_presets() {
    const Json base = {{"schema", 1}, {"id", "duel"}, {"name", "Duel"}, {"kind", "action"}, {"max_seconds", 40}};
    const auto p = Preset::parse(base);
    expect(p.kind == ShotKind::Action && p.action.framing == Framing::Duel && p.keys.empty(), "an action preset needs no keys");
    expect(near(make_timeline(p, 0, 184).total, 40, 1e-9), "an action take runs to max_seconds");
    Json full = base;
    full["action"] = {{"framing", "shoulder"}, {"azimuth", -60}, {"orbit_speed", 0}, {"distance", 300}, {"distance_max", 800}, {"fstop", 2.0},
                      {"game_cameras", "override"}, {"beats", Json::array({{{"on", "kill"}, {"do", "slowmo"}}, {{"on", "parry"}, {"do", "punch"}, {"cooldown", 4}}})}};
    const auto q = Preset::parse(full);
    expect(q.action.framing == Framing::Shoulder && !q.action.respect_game_cameras && q.action.beats.size() == 2
           && q.action.beats[0].on == Beat::On::Kill && q.action.beats[1].what == Beat::Do::Punch && near(q.action.beats[1].cooldown, 4, 1e-9), "action fields parse");
    auto bad = [&](const char* key, Json value) { Json j = full; j["action"][key] = std::move(value); rejects([&] { Preset::parse(j); }, std::string("action.") + key); };
    bad("framing", "orbit"); bad("distance", 10); bad("distance_max", 200); bad("slowmo", 0); bad("game_cameras", "ignore");
    bad("beats", Json::array({{{"on", "dodge"}, {"do", "punch"}}})); bad("beats", Json::array({{{"on", "hit"}, {"do", "cut"}}}));

    // A breakdown track works on any kind and is a fraction of the whole take.
    Json reel = base;
    reel["breakdown"] = {{"steps", Json::array({{{"at", 0.1}, {"layers", {"clay"}}}, {{"at", 0.5}, {"layers", {"clay", "wireframe"}}, {"wipe", 0}}, {{"at", 0.9}, {"layers", Json::array()}}})}};
    const auto r = Preset::parse(reel);
    const auto tl = make_timeline(r, 0, 184);
    expect(tl.breakdown.size() == 3 && near(tl.breakdown[0].at, 4, 1e-9) && near(tl.breakdown[1].at, 20, 1e-9) && tl.breakdown[1].wipe == 0
           && tl.breakdown[1].layers == (bit(Layer::Clay) | bit(Layer::Wireframe)) && tl.breakdown[2].layers == 0, "breakdown steps land on the timeline");
    auto bad_reel = [&](Json steps) { Json j = base; j["breakdown"] = {{"steps", std::move(steps)}}; rejects([&] { Preset::parse(j); }, "a bad breakdown track"); };
    bad_reel(Json::array({{{"at", 0.5}, {"layers", {"clay"}}}, {{"at", 0.4}, {"layers", {"clay"}}}}));   // out of order
    bad_reel(Json::array({{{"at", 0.5}, {"layers", {"cla"}}}}));
    bad_reel(Json::array({{{"at", 0.5}}}));
    bad_reel(Json::array({{{"at", 0.5}, {"layers", {"clay"}}, {"wipe", 99}}}));
}

static void labels() {
    // The camera's axes: level and facing +X, right is +Y and up is +Z; yaw 90 faces +Y.
    auto b = view_basis({0, 0, 0});
    expect(near(b.forward.x, 1, 1e-12) && near(b.right.y, 1, 1e-12) && near(b.up.z, 1, 1e-12), "level axes");
    b = view_basis({0, 90, 0});
    expect(near(b.forward.y, 1, 1e-12) && near(b.right.x, -1, 1e-12), "yaw turns forward and right together");
    b = view_basis({-30, 40, 0});
    const auto fwd = look_at({0, 0, 0}, b.forward);
    expect(near(fwd.pitch, -30, 1e-9) && near(fwd.yaw, 40, 1e-9), "forward agrees with look_at");
    auto dot = [](Vec3 a, Vec3 c) { return a.x * c.x + a.y * c.y + a.z * c.z; };
    expect(near(dot(b.forward, b.right), 0, 1e-12) && near(dot(b.forward, b.up), 0, 1e-12) && near(dot(b.right, b.up), 0, 1e-12), "the axes are square");
    // A stand-in for the engine: an 80 degree lens on a 2560x1440 HUD that is scaled and letterboxed.
    const Vec3 camera{120, -40, 210};
    const double k = 1100 / std::tan(40 * std::numbers::pi / 180), cx = 1280, cy = 742;
    auto engine = [&](Vec3 p, double& x, double& y) {
        const Vec3 d = p - camera;
        x = cx + k * dot(d, b.right) / dot(d, b.forward); y = cy - k * dot(d, b.up) / dot(d, b.forward);
    };
    double x0, y0, x1, y1;
    engine(lens_point(camera, b, false), x0, y0); engine(lens_point(camera, b, true), x1, y1);
    const auto lens = measure_lens(x0, y0, x1, y1);
    expect(lens.ok && near(lens.cx, cx, 1e-6) && near(lens.kx, k, 1e-6) && near(lens.ky, k, 1e-6), "two engine points measure the lens");
    for (const Vec3 p : {Vec3{500, 300, 100}, Vec3{900, -200, 400}, Vec3{300, 100, 230}}) {
        if (dot(p - camera, b.forward) <= 10) continue;
        double ex, ey, x, y;
        engine(p, ex, ey);
        expect(project(camera, b, lens, p, x, y) && near(x, ex, 1e-6) && near(y, ey, 1e-6), "own projection matches the engine's");
    }
    double x, y;
    expect(!project(camera, b, lens, camera - b.forward * 200, x, y), "a point behind the camera is not drawn");
    expect(!project(camera, b, Lens{}, {500, 300, 100}, x, y) && !measure_lens(100, 100, 100, 100).ok, "no labels without a measured lens");

    // Layout: labels go to the side their target is on, in the order of their targets.
    std::vector<CalloutTarget> targets{{"Skeleton, 392 bones", 1200, 600, true}, {"Mesh wireframe", 1250, 300, true}, {"Movement capsule", 1400, 800, true},
                                       {"Behind the camera", 0, 0, false}, {"Off screen", 9000, 100, true}};
    const auto placed = layout_callouts(2560, 1440, targets);
    expect(placed.size() == 3, "only visible, on-screen targets get a label");
    for (const auto& p : placed) {
        const bool left = targets[p.target].x < 1280;
        expect(left ? p.label_x < 400 : p.label_x > 1700, "a label sits at the edge on its target's side");
        expect(near(p.to_x, targets[p.target].x, 1e-9) && near(p.to_y, targets[p.target].y, 1e-9), "its leader ends on the target");
        expect(left ? p.from_x > p.label_x : p.from_x < p.label_x, "and starts at the label's inner edge");
    }
    expect(placed[0].target == 1 && placed[1].target == 0 && placed[0].label_y < placed[1].label_y, "a column runs top to bottom like its targets, so leaders do not cross");
    std::vector<CalloutTarget> many;
    for (int i = 0; i < 12; ++i) many.push_back({"Label " + std::to_string(i), 600.0 + i * 10, 200.0 + i * 60, true});
    const auto capped = layout_callouts(2560, 1440, many);
    size_t on_left = 0;
    for (const auto& p : capped) if (p.label_x < 1280) ++on_left;
    expect(capped.size() == max_callouts && on_left == max_callouts / 2, "at most six labels, three a side");
    for (size_t i = 0; i < capped.size(); ++i) for (size_t j = i + 1; j < capped.size(); ++j)
        if ((capped[i].label_x < 1280) == (capped[j].label_x < 1280)) expect(std::abs(capped[i].label_y - capped[j].label_y) >= callout_line, "labels in a column never overlap");
    expect(layout_callouts(0, 0, targets).empty(), "no layout without a viewport");
}

static void shipped(const fs::path& root) {
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(root / "presets")) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream in(entry.path());
        const Preset p = Preset::parse(Json::parse(in));
        expect(entry.path().filename().string() == p.id + ".cine.json", "file name matches id: " + p.id);
        if (p.kind == ShotKind::Walk) {
            const auto tl = make_timeline(p, 4000, 184);
            OrbitCurve curve(tl.keys);
            expect(tl.total <= p.max_seconds + 1e-9, "shipped walk fits its cap");
        } else if (p.kind == ShotKind::Action) {
            const auto tl = make_timeline(p, 0, 184);
            expect(near(tl.total, p.max_seconds, 1e-9) && tl.keys.empty(), "shipped action preset runs to its cap: " + p.id);
            // The camera must sit off the fighters at every separation it can meet.
            for (double apart : {0.0, 150.0, 600.0, 2500.0}) {
                const Vec3 her{0, 0, 0}, foe{apart, 0, 0};
                const ActionFrame f{p.action.framing, p.action.azimuth, p.action.distance, p.action.distance_max, p.action.margin, p.action.height, p.action.aim, p.action.fov, 0};
                const auto view = action_view(f, her, &foe);
                expect(std::hypot(view.camera.x, view.camera.y) > 60 && view.focus > 60 && std::isfinite(view.fov), "shipped action preset keeps its distance: " + p.id);
            }
        } else if (p.kind != ShotKind::Photo) {
            const auto tl = make_timeline(p, 0, 184);
            OrbitCurve curve(tl.keys);
            for (size_t i = 1; i < tl.breakdown.size(); ++i) expect(tl.breakdown[i].at > tl.breakdown[i - 1].at, "shipped breakdown steps are in order: " + p.id);
            if (!tl.breakdown.empty()) expect(tl.breakdown.back().layers == 0, "a shipped breakdown ends on the final image: " + p.id);
        }
        ++count;
    }
    expect(count >= 11, "all shipped presets were checked");
}

int main(int argc, char** argv) {
    try {
        geometry(); routes(); tracks(); curves(); walls(); presets();
        layers(); action(); action_presets(); labels();
        if (argc > 1) shipped(fs::path(argv[1]));
        std::cout << checks << " CINE checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}

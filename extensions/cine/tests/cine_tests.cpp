// CINE logic tests: shot maths, presets and the shipped preset files. Run with the
// extension directory as argv[1] (CMake passes it).
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
        } else if (p.kind != ShotKind::Photo) {
            OrbitCurve curve(make_timeline(p, 0, 184).keys);
        }
        ++count;
    }
    expect(count >= 7, "all shipped presets were checked");
}

int main(int argc, char** argv) {
    try {
        geometry(); routes(); tracks(); curves(); walls(); presets();
        if (argc > 1) shipped(fs::path(argv[1]));
        std::cout << checks << " CINE checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}

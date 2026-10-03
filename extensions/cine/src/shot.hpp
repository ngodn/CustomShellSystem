#pragma once
// Pure shot maths for CINE: no engine calls, so all of it is unit-tested off-game.
// Units are Unreal's: centimetres, degrees, seconds. "Relative" means her actor frame:
// X forward, Y right, Z up from the capsule centre.
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cine {

struct Vec3 { double x = 0, y = 0, z = 0; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double flat_distance(Vec3 a, Vec3 b);
double wrap_degrees(double degrees);              // (-180, 180]
double heading(Vec3 from, Vec3 to);               // yaw of the flat direction, degrees

struct Rotator { double pitch = 0, yaw = 0, roll = 0; };
Rotator look_at(Vec3 from, Vec3 to);
// Relative offset in her frame -> world, for a frame turned `yaw` degrees at `base`.
Vec3 to_world(Vec3 base, double yaw, Vec3 relative);

// One 10 Hz sample of the player's own walk while recording a route.
struct Sample { double t = 0; Vec3 position; double yaw = 0; double speed = 0; };

// Where the take ends and how the modder framed it: her position and facing, and the
// game camera relative to her (offset, pitch, yaw relative to her facing, FOV).
struct FinalShot { Vec3 position; double yaw = 0; Vec3 camera; double pitch = 0; double camera_yaw = 0; double fov = 70; };

struct Route {
    std::vector<Vec3> points;    // ~1.2 m apart, last point is where she stops
    Vec3 start; double start_yaw = 0;   // where she stands before walking (faces the path)
    double length() const;
    bool empty() const { return points.size() < 2; }
};

// From recorded samples: keep the longest walking run (setup nudges before it and steps
// after it are not the route) and decimate it. Throws std::invalid_argument when the samples
// hold no walk of at least 3 m.
Route extract_route(const std::vector<Sample>& samples);
// Branch into the final shot with a cubic curve that arrives already facing the final
// direction (no turn on the spot). Throws when the spot is not reachable smoothly.
Route branch_into_final(Route route, const FinalShot& final_shot);
// extract_route, then branch_into_final when a final shot is given.
Route build_route(const std::vector<Sample>& samples, const std::optional<FinalShot>& final_shot);
// The start pose from the route's first segment (she starts facing down the path).
void set_start(Route& route);

// Arc-length access to a route, with a projection that searches near its last match.
class Track {
public:
    explicit Track(const Route& route);
    double length() const { return cumulative_.back(); }
    Vec3 at(double s) const;
    double heading_at(double s) const;
    double project(Vec3 position);   // nearest arc length; remembers the segment
private:
    std::vector<Vec3> points_;
    std::vector<double> cumulative_;
    size_t segment_ = 0;
};

// Camera key in orbit terms: azimuth from her front (+90 = her right), flat distance,
// camera height and aim height above the capsule centre, horizontal FOV.
struct OrbitKey { double t = 0, azimuth = 0, distance = 300, height = 0, aim = 0, fov = 40; };
struct Pose { Vec3 offset; Vec3 aim; double fov = 40; };

// One Catmull-Rom curve through the keys (non-uniform tangents): the camera never slows to
// a stop between shots. Azimuth is unwrapped so the camera arcs the short or the intended
// way: keys whose azimuth only increases make a full orbit.
class OrbitCurve {
public:
    explicit OrbitCurve(std::vector<OrbitKey> keys);   // throws unless 1..64 keys in time order
    OrbitKey at(double t) const;
    Pose pose(double t, double distance_scale = 1.0) const;
    const std::vector<OrbitKey>& keys() const { return keys_; }
private:
    std::vector<OrbitKey> keys_;
};

// Distance scale over time that eases the camera in where it would meet a wall: hold the
// tightest raw value for `hold` seconds either side, then a 7-sample moving average.
class WallProfile {
public:
    WallProfile() = default;
    WallProfile(double step, std::vector<double> raw, double hold = 1.0);
    double at(double t) const;
    bool tight() const;   // any part pulled in
private:
    double step_ = 0.25;
    std::vector<double> scale_;
};

// Exponential smoothing that never leaps after a slow frame: dt is clamped first.
inline double smooth_factor(double dt, double tau, double max_dt = 0.12) {
    if (tau <= 0) return 1.0;
    const double step = dt < 0 ? 0 : dt > max_dt ? max_dt : dt;
    const double k = step / tau;
    return k > 1 ? 1 : k;
}

} // namespace cine

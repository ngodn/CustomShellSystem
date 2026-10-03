#include "shot.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cine {
namespace {
constexpr double deg = std::numbers::pi / 180.0;
constexpr double decimate_cm = 120.0;      // route point spacing
constexpr double walking_cm_s = 60.0;      // a sample counts as walking above this
constexpr size_t run_gap_samples = 5;      // a pause longer than this splits runs
}

double flat_distance(Vec3 a, Vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }
double wrap_degrees(double d) { d = std::fmod(d + 180.0, 360.0); if (d <= 0) d += 360.0; return d - 180.0; }
double heading(Vec3 from, Vec3 to) { return std::atan2(to.y - from.y, to.x - from.x) / deg; }

Rotator look_at(Vec3 from, Vec3 to) {
    const Vec3 d = to - from;
    return {std::atan2(d.z, std::hypot(d.x, d.y)) / deg, std::atan2(d.y, d.x) / deg, 0.0};
}

Vec3 to_world(Vec3 base, double yaw, Vec3 r) {
    const double c = std::cos(yaw * deg), s = std::sin(yaw * deg);
    return {base.x + r.x * c - r.y * s, base.y + r.x * s + r.y * c, base.z + r.z};
}

double Route::length() const {
    double total = 0;
    for (size_t i = 1; i < points.size(); ++i) total += flat_distance(points[i - 1], points[i]);
    return total;
}

Route build_route(const std::vector<Sample>& samples, const std::optional<FinalShot>& final_shot) {
    std::vector<size_t> moving;
    for (size_t i = 0; i < samples.size(); ++i) if (samples[i].speed > walking_cm_s) moving.push_back(i);
    if (moving.size() < 3) throw std::invalid_argument("The recording has no walk in it");
    // Longest moving run by covered distance; a pause splits runs.
    size_t best_first = moving.front(), best_last = moving.front(), first = moving.front();
    double best = -1;
    for (size_t k = 1; k <= moving.size(); ++k) {
        const bool split = k == moving.size() || moving[k] - moving[k - 1] > run_gap_samples;
        if (!split) continue;
        const size_t last = moving[k - 1];
        const double covered = flat_distance(samples[first].position, samples[last].position);
        if (covered > best) { best = covered; best_first = first; best_last = last; }
        if (k < moving.size()) first = moving[k];
    }
    Route route;
    route.points.push_back(samples[best_first].position);
    for (size_t i = best_first; i <= best_last; ++i)
        if (flat_distance(samples[i].position, route.points.back()) >= decimate_cm) route.points.push_back(samples[i].position);
    if (flat_distance(samples[best_last].position, route.points.back()) > 30) route.points.push_back(samples[best_last].position);
    if (route.points.size() < 2) throw std::invalid_argument("The recorded walk is too short (walk at least 3 m)");

    if (final_shot) {
        // Branch where she is already heading roughly toward the final spot with room to curve
        // in: 2-5 m ahead of it along the final facing, under 70 degrees of turn.
        const Vec3 goal = final_shot->position;
        const double gh = final_shot->yaw * deg;
        std::optional<size_t> branch;
        for (size_t i = 1; i < route.points.size(); ++i) {
            const Vec3 d = goal - route.points[i];
            const double ahead = d.x * std::cos(gh) + d.y * std::sin(gh);
            const double turn = std::abs(wrap_degrees(final_shot->yaw - heading(route.points[i - 1], route.points[i])));
            if (ahead >= 200 && ahead <= 500 && turn < 70) branch = i;
        }
        if (!branch) throw std::invalid_argument("The final shot spot is not reachable smoothly from this route; "
                                                 "end the recorded walk closer to it, walking toward it");
        const Vec3 p0 = route.points[*branch];
        const double h0 = heading(route.points[*branch - 1], p0) * deg;
        route.points.resize(*branch + 1);
        const double span = flat_distance(p0, goal) * 0.4;
        const Vec3 c1{p0.x + span * std::cos(h0), p0.y + span * std::sin(h0), 0};
        const Vec3 c2{goal.x - span * std::cos(gh), goal.y - span * std::sin(gh), 0};
        const int n = std::max(4, int(flat_distance(p0, goal) / 45.0));
        for (int k = 1; k <= n; ++k) {
            const double u = double(k) / n, a = (1 - u) * (1 - u) * (1 - u), b = 3 * (1 - u) * (1 - u) * u, c = 3 * (1 - u) * u * u, e = u * u * u;
            route.points.push_back({a * p0.x + b * c1.x + c * c2.x + e * goal.x,
                                    a * p0.y + b * c1.y + c * c2.y + e * goal.y,
                                    p0.z + (goal.z - p0.z) * u});
        }
    }
    route.start = route.points.front();
    route.start_yaw = heading(route.points[0], route.points[1]);   // start facing the path
    return route;
}

Track::Track(const Route& route) : points_(route.points) {
    if (points_.size() < 2) throw std::invalid_argument("A route needs at least two points");
    cumulative_.assign(1, 0.0);
    for (size_t i = 1; i < points_.size(); ++i) cumulative_.push_back(cumulative_.back() + flat_distance(points_[i - 1], points_[i]));
}

Vec3 Track::at(double s) const {
    s = std::clamp(s, 0.0, length());
    auto it = std::upper_bound(cumulative_.begin(), cumulative_.end(), s);
    size_t k = std::clamp<size_t>(size_t(it - cumulative_.begin()), 1, points_.size() - 1);
    const double d = cumulative_[k] - cumulative_[k - 1];
    const double u = d <= 0 ? 0 : (s - cumulative_[k - 1]) / d;
    return points_[k - 1] + (points_[k] - points_[k - 1]) * u;
}

double Track::heading_at(double s) const {
    const Vec3 a = at(s - 60), b = at(s + 60);
    return flat_distance(a, b) < 1 ? heading(points_[points_.size() - 2], points_.back()) : heading(a, b);
}

double Track::project(Vec3 p) {
    double best = 1e300, best_s = cumulative_[segment_];
    size_t best_k = segment_ + 1;
    const size_t lo = segment_ > 2 ? segment_ - 2 : 0, hi = std::min(points_.size() - 1, segment_ + 8);
    for (size_t k = lo + 1; k <= hi; ++k) {
        const Vec3 a = points_[k - 1], b = points_[k];
        const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
        const double u = l2 <= 0 ? 0 : std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0);
        const double ex = p.x - (a.x + u * dx), ey = p.y - (a.y + u * dy), d2 = ex * ex + ey * ey;
        if (d2 < best) { best = d2; best_s = cumulative_[k - 1] + u * std::sqrt(l2); best_k = k; }
    }
    segment_ = best_k - 1;
    return best_s;
}

OrbitCurve::OrbitCurve(std::vector<OrbitKey> keys) : keys_(std::move(keys)) {
    if (keys_.empty() || keys_.size() > 64) throw std::invalid_argument("A shot needs 1 to 64 camera keys");
    for (size_t i = 1; i < keys_.size(); ++i) {
        if (!(keys_[i].t > keys_[i - 1].t)) throw std::invalid_argument("Camera keys must be in time order");
        // Unwrap relative to the previous key so a curve never spins the long way round.
        keys_[i].azimuth = keys_[i - 1].azimuth + wrap_degrees(keys_[i].azimuth - keys_[i - 1].azimuth);
    }
}

OrbitKey OrbitCurve::at(double t) const {
    if (t <= keys_.front().t) return keys_.front();
    if (t >= keys_.back().t) return keys_.back();
    size_t i = 0;
    while (i + 1 < keys_.size() && keys_[i + 1].t < t) ++i;
    const OrbitKey& p0 = keys_[i ? i - 1 : 0]; const OrbitKey& p1 = keys_[i];
    const OrbitKey& p2 = keys_[i + 1]; const OrbitKey& p3 = keys_[std::min(i + 2, keys_.size() - 1)];
    const double h = p2.t - p1.t, u = (t - p1.t) / h, u2 = u * u, u3 = u2 * u;
    const double h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
    auto channel = [&](double a0, double a1, double a2, double a3) {
        const double m1 = (a2 - a0) / std::max(1e-6, p2.t - p0.t) * h;
        const double m2 = (a3 - a1) / std::max(1e-6, p3.t - p1.t) * h;
        return h00 * a1 + h10 * m1 + h01 * a2 + h11 * m2;
    };
    OrbitKey k;
    k.t = t;
    k.azimuth = channel(p0.azimuth, p1.azimuth, p2.azimuth, p3.azimuth);
    k.distance = channel(p0.distance, p1.distance, p2.distance, p3.distance);
    k.height = channel(p0.height, p1.height, p2.height, p3.height);
    k.aim = channel(p0.aim, p1.aim, p2.aim, p3.aim);
    k.fov = channel(p0.fov, p1.fov, p2.fov, p3.fov);
    return k;
}

Pose OrbitCurve::pose(double t, double distance_scale) const {
    const OrbitKey k = at(t);
    const double d = k.distance * distance_scale;
    return {{d * std::cos(k.azimuth * deg), d * std::sin(k.azimuth * deg), k.height}, {0, 0, k.aim}, k.fov};
}

WallProfile::WallProfile(double step, std::vector<double> raw, double hold) : step_(step > 0 ? step : 0.25) {
    const size_t n = raw.size();
    const size_t reach = size_t(std::max(0.0, hold) / step_ + 0.5);
    std::vector<double> held(n, 1.0);
    for (size_t i = 0; i < n; ++i) {
        double m = 1.0;
        for (size_t j = i > reach ? i - reach : 0; j <= std::min(n - 1, i + reach); ++j) m = std::min(m, raw[j]);
        held[i] = m;
    }
    scale_.assign(n, 1.0);
    for (size_t i = 0; i < n; ++i) {
        double sum = 0; size_t count = 0;
        for (size_t j = i > 3 ? i - 3 : 0; j <= std::min(n - 1, i + 3); ++j) { sum += held[j]; ++count; }
        scale_[i] = std::min(held[i], sum / double(count));
    }
}

double WallProfile::at(double t) const {
    if (scale_.empty()) return 1.0;
    const double x = t / step_;
    if (x <= 0) return scale_.front();
    const size_t i = size_t(x);
    if (i + 1 >= scale_.size()) return scale_.back();
    const double u = x - double(i);
    return scale_[i] + (scale_[i + 1] - scale_[i]) * u;
}

bool WallProfile::tight() const {
    return std::any_of(scale_.begin(), scale_.end(), [](double v) { return v < 0.999; });
}

} // namespace cine

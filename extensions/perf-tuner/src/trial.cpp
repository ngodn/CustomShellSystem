#include "trial.hpp"
#include <algorithm>
#include <cmath>

namespace tuner {
namespace {
double median_of(std::vector<double> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}
Json list(const std::vector<Sample>& s) { Json j = Json::array(); for (const auto& x : s) j.push_back(x.json()); return j; }
}

Json TrialResult::json() const {
    return {{"off", list(off)}, {"on", list(on)}, {"off_median_ms", off_median_ms}, {"on_median_ms", on_median_ms},
            {"off_p99_ms", off_p99_ms}, {"on_p99_ms", on_p99_ms}, {"gain_pct", gain_pct}, {"hitch_gain_pct", hitch_gain_pct},
            {"consistent", consistent}, {"verdict", verdict}};
}

TrialResult judge(const std::vector<Sample>& off, const std::vector<Sample>& on) {
    TrialResult r;
    r.off = off; r.on = on;
    if (off.empty() || on.empty()) { r.verdict = "unclear"; return r; }
    std::vector<double> om, nm, op, np;
    for (const auto& s : off) { om.push_back(s.median_ms); op.push_back(s.p99_ms); }
    for (const auto& s : on) { nm.push_back(s.median_ms); np.push_back(s.p99_ms); }
    r.off_median_ms = median_of(om); r.on_median_ms = median_of(nm);
    r.off_p99_ms = median_of(op); r.on_p99_ms = median_of(np);
    if (r.off_median_ms <= 0 || r.on_median_ms <= 0) { r.verdict = "unclear"; return r; }
    r.gain_pct = (r.off_median_ms - r.on_median_ms) / r.off_median_ms * 100;
    r.hitch_gain_pct = r.off_p99_ms > 0 ? (r.off_p99_ms - r.on_p99_ms) / r.off_p99_ms * 100 : 0;
    const double off_best = *std::min_element(om.begin(), om.end()), off_worst = *std::max_element(om.begin(), om.end());
    const double on_best = *std::min_element(nm.begin(), nm.end()), on_worst = *std::max_element(nm.begin(), nm.end());
    const bool all_faster = on_worst < off_best, all_slower = on_best > off_worst;
    r.consistent = all_faster || all_slower;
    if (r.gain_pct >= trial_threshold_pct) r.verdict = all_faster ? "gain" : "unclear";
    else if (r.gain_pct <= -trial_threshold_pct) r.verdict = all_slower ? "loss" : "unclear";
    else r.verdict = "same";
    return r;
}

void Trial::start(double now, TrialPlan plan) {
    plan_ = plan;
    plan_.settle = std::clamp(plan_.settle, 0.5, 10.0);
    plan_.measure = std::clamp(plan_.measure, 2.0, 30.0);
    plan_.rounds = std::clamp(plan_.rounds, 1, 5);
    running_ = true; on_ = false; switched_ = false; sampled_ = false;
    window_ = 0; window_start_ = now;
    off_samples_.clear(); on_samples_.clear(); result_ = {};
}

Trial::Step Trial::advance(double now) {
    if (!running_) return Step::None;
    if (!switched_) { switched_ = true; window_start_ = now; return Step::Switch; }   // the switch starts the window
    if (sampled_) return Step::None;   // waiting for record()
    if (now - window_start_ < plan_.settle + plan_.measure) return Step::None;
    sampled_ = true;
    return Step::Sample;
}

void Trial::record(const Sample& s) {
    if (!running_ || !sampled_) return;
    (on_ ? on_samples_ : off_samples_).push_back(s);
    ++window_;
    if (window_ >= plan_.rounds * 2) {
        result_ = judge(off_samples_, on_samples_);
        running_ = false;
        return;
    }
    on_ = window_ % 2 == 1;
    switched_ = false; sampled_ = false;
}

double Trial::progress(double now) const {
    if (!running_) return window_ > 0 ? 1.0 : 0.0;
    const double per = plan_.settle + plan_.measure, total = per * plan_.rounds * 2;
    const double in_window = switched_ ? std::clamp(now - window_start_, 0.0, per) : 0.0;
    return std::clamp((window_ * per + in_window) / total, 0.0, 1.0);
}
}

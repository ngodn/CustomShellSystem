#pragma once
// A trial measures one thing fairly: the frame time with a change off, then on, then off,
// then on again, in the same scene, back to back. Interleaving cancels the drift of the
// scene itself (a torch flickers, a crowd walks by); the second round shows whether the
// first was luck. Pure state machine with the clock injected, unit-tested off-game. The
// caller applies and restores the change when told, and feeds in a frame-time sample at
// the end of every window.
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace tuner {
using Json = nlohmann::json;

struct Sample {
    double median_ms = 0, mean_ms = 0, p99_ms = 0;
    unsigned frames = 0;
    Json json() const { return {{"median_ms", median_ms}, {"mean_ms", mean_ms}, {"p99_ms", p99_ms}, {"frames", frames}}; }
};

struct TrialPlan {
    // After a switch, Lumen's caches, the shadow pages and the upscaler's history take a few
    // seconds to settle; that time is not counted.
    double settle = 5.0;    // seconds after a switch before the window counts
    double measure = 10.0;  // seconds measured per window
    int rounds = 2;         // off/on pairs
};

struct TrialResult {
    std::vector<Sample> off, on;
    double off_median_ms = 0, on_median_ms = 0, off_p99_ms = 0, on_p99_ms = 0;
    double gain_pct = 0;          // frame time saved with the change on, in percent of the off time; negative is a loss
    double hitch_gain_pct = 0;    // the same for the slowest 1 percent of frames
    bool consistent = false;      // every on window beat every off window (or lost to every one)
    std::string verdict;          // "gain", "same", "loss", "unclear"
    Json json() const;
};

// gain_pct at or above this, consistently, is a gain; at or below minus this, a loss.
inline constexpr double trial_threshold_pct = 1.5;
// From the windows: medians, the gain and the verdict.
TrialResult judge(const std::vector<Sample>& off, const std::vector<Sample>& on);

class Trial {
public:
    enum class Step { None, Switch, Sample, Finished };
    void start(double now, TrialPlan plan = {});
    bool running() const { return running_; }
    bool change_on() const { return on_; }   // the condition the current window wants
    // Call often (10 Hz is plenty). Switch: apply or restore the change to match change_on()
    // (the first call after start is a Switch too, so the caller always knows the state).
    // Sample: the window ended; call record() with the frame times of the last plan.measure
    // seconds. Finished: result() is ready.
    Step advance(double now);
    void record(const Sample& s);
    void cancel() { running_ = false; }
    double progress(double now) const;   // 0..1
    const TrialResult& result() const { return result_; }
    const TrialPlan& plan() const { return plan_; }
private:
    TrialPlan plan_;
    bool running_ = false, on_ = false, switched_ = false, sampled_ = false;
    int window_ = 0;                     // 0-based; even windows are off, odd are on
    double window_start_ = 0;            // set when the window's switch is handed out
    std::vector<Sample> off_samples_, on_samples_;
    TrialResult result_;
};
}

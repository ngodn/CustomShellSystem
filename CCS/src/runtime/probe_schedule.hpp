#pragma once
#include <algorithm>
#include <cstdint>

namespace ccs::runtime {
class ProbeSchedule {
public:
    enum class State { Idle, Running, Complete, Cancelled, TimedOut, Failed };
    static constexpr uint64_t timeout_us = 15'000'000;
    static constexpr uint64_t max_step_us = 2'000;
    static constexpr unsigned max_steps = 512;
    void start(uint64_t now) {
        state_ = State::Running;
        started_ = now;
        steps_ = 0;
        maximum_ = 0;
        ++run_;
    }
    bool poll(uint64_t now) {
        if (state_ == State::Running && now - started_ >= timeout_us) state_ = State::TimedOut;
        return state_ == State::Running;
    }
    void step(uint64_t duration, bool done) {
        if (state_ != State::Running) return;
        maximum_ = std::max(maximum_, duration);
        if (++steps_ > max_steps || duration > max_step_us) state_ = State::Failed;
        else if (done) state_ = State::Complete;
    }
    void cancel() { if (state_ == State::Running) state_ = State::Cancelled; }
    void fail() { state_ = State::Failed; }
    State state() const { return state_; }
    uint64_t run() const { return run_; }
    unsigned steps() const { return steps_; }
    uint64_t maximum_us() const { return maximum_; }
private:
    State state_{State::Idle};
    uint64_t started_{0}, run_{0}, maximum_{0};
    unsigned steps_{0};
};
}

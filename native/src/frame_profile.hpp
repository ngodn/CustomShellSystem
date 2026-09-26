#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace css {
// Developer-only bounded sampling. The sampler adds no engine calls, allocations
// or file writes inside a measured phase. Engine delta and wall cadence are separate.
class FrameProfile {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    enum Phase { recovery, inventory, maintenance, attachments, seals, walk, misc, reconcile, phase_count };
    struct Row {
        double engine_ms{}, interval_ms{}, core_ms{};
        std::array<double, phase_count> phase_ms{};
        bool failed{};
    };
    static constexpr size_t capacity = 4096;
private:
    bool active_ = false, recording_ = false, have_previous_ = false;
    Time sample_after_{}, until_{}, started_{}, previous_{};
    Row current_{};
    std::string id_, reason_;
    std::vector<Row> rows_;
    static double ms(Time a, Time b) { return std::chrono::duration<double, std::milli>(b-a).count(); }
public:
    void arm(std::string id, double seconds, Time now = Clock::now()) {
        if(active_) throw std::runtime_error("Frame profile already running");
        if(id.empty() || id.size()>96 || !std::isfinite(seconds) || seconds<3 || seconds>30)
            throw std::runtime_error("Frame profile needs an ID and 3 to 30 seconds");
        rows_.clear(); rows_.reserve(capacity);
        id_=std::move(id); reason_.clear();
        sample_after_=now+std::chrono::seconds(1);
        until_=sample_after_+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        active_=true; recording_=false; have_previous_=false;
    }
    bool active() const { return active_; }
    const auto& rows() const { return rows_; }
    const auto& id() const { return id_; }
    const auto& reason() const { return reason_; }
    void begin(double engine_delta, Time now = Clock::now()) {
        recording_=active_ && now>=sample_after_;
        if(!active_) return;
        current_={};
        current_.engine_ms=engine_delta*1000;
        current_.interval_ms=have_previous_?ms(previous_,now):0;
        have_previous_=true; previous_=now; started_=now;
    }
    template<class Function> void measure(Phase phase, Function&& function) {
        if(!recording_) { function(); return; }
        const auto start=Clock::now();
        try { function(); }
        catch(...) { current_.phase_ms[phase]+=ms(start,Clock::now()); throw; }
        current_.phase_ms[phase]+=ms(start,Clock::now());
    }
    bool end(bool failed = false, Time now = Clock::now()) {
        if(!recording_) return false;
        recording_=false;
        current_.core_ms=ms(started_,now); current_.failed=failed;
        rows_.push_back(current_);
        if(now<until_ && rows_.size()<capacity) return false;
        active_=false;
        reason_=rows_.size()==capacity?"frame_capacity":"duration";
        return true;
    }
};
}

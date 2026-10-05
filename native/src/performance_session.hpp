#pragma once
#include "performance_clock.hpp"
#include <cstdint>
#include <limits>
#include <string>

namespace css {
// A delayed stop from an earlier take must not stop a newer one.
class PerformanceSession {
    PerformanceClock clock_;
    uint64_t token_=0;
    std::string clip_;
public:
    const PerformanceClock& clock() const noexcept { return clock_; }
    const std::string& clip() const noexcept { return clip_; }
    uint64_t token() const noexcept { return token_; }
    bool active() const noexcept { return clock_.active(); }
    void require(uint64_t token) const {
        if(!active() || token==0 || token!=token_)
            throw std::runtime_error("Performance session has ended or belongs to another take");
    }
    uint64_t begin(const std::string& clip,AnimationPlayback mode,double duration,double rate=1.,double position=0.) {
        if(active()) throw std::runtime_error("Stop the current performance first");
        if(clip.empty() || clip.size()>96 || token_==std::numeric_limits<uint64_t>::max())
            throw std::runtime_error("Invalid performance identity or exhausted session counter");
        auto next=clock_;
        next.begin(mode,duration,rate,position);
        auto next_clip=clip;
        clock_=next;clip_=std::move(next_clip);
        return ++token_;
    }
    void advance(double delta) { clock_.advance(delta); }
    void seek(uint64_t token,double position) { require(token);clock_.seek(position); }
    void rate(uint64_t token,double value) { require(token);clock_.set_rate(value); }
    void pause(uint64_t token,bool value) { require(token);clock_.pause(value); }
    void stop(uint64_t token) { require(token);clock_.stop(); }
    void interrupt() noexcept { clock_.stop(true); }
};
}

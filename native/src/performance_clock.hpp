#pragma once
#include "animation_library.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace css {
enum class PerformancePhase { Stopped, Playing, Holding, BlendingOut };

// Game-time clock for an explicit-time evaluator. No asset loads or engine writes.
class PerformanceClock {
    AnimationPlayback mode_=AnimationPlayback::Once;
    PerformancePhase phase_=PerformancePhase::Stopped;
    double duration_=0., position_=0., rate_=1., blend_elapsed_=0.;
    double entry_elapsed_=0., exit_weight_=0.;
    bool paused_=false;
    static void bounded(double value,double low,double high,const char* message) {
        if(!std::isfinite(value) || value<low || value>high) throw std::invalid_argument(message);
    }
    void leave(double elapsed) {
        if(phase_!=PerformancePhase::BlendingOut) exit_weight_=weight();
        paused_=false;
        blend_elapsed_=std::min(elapsed,blend_seconds);
        phase_=blend_elapsed_>=blend_seconds ? PerformancePhase::Stopped : PerformancePhase::BlendingOut;
    }
public:
    static constexpr double blend_seconds=.18;
    PerformancePhase phase() const noexcept { return phase_; }
    double position() const noexcept { return position_; }
    double duration() const noexcept { return duration_; }
    double rate() const noexcept { return rate_; }
    bool paused() const noexcept { return paused_; }
    bool active() const noexcept { return phase_!=PerformancePhase::Stopped; }
    bool enabled() const noexcept { return phase_==PerformancePhase::Playing || phase_==PerformancePhase::Holding; }
    double weight() const noexcept {
        auto ease=[](double x) { x=std::clamp(x,0.,1.);return x*x*(3.-2.*x); };
        if(phase_==PerformancePhase::Stopped) return 0.;
        if(phase_==PerformancePhase::BlendingOut) return exit_weight_*(1.-ease(blend_elapsed_/blend_seconds));
        return ease(entry_elapsed_/blend_seconds);
    }

    void begin(AnimationPlayback mode,double duration,double rate=1.,double position=0.) {
        bounded(duration,.000001,86400.,"Invalid performance duration");
        bounded(rate,.1,4.,"Invalid performance rate");
        bounded(position,0.,duration,"Invalid performance position");
        if(mode!=AnimationPlayback::Loop && mode!=AnimationPlayback::Once && mode!=AnimationPlayback::Pose)
            throw std::invalid_argument("Invalid performance mode");
        mode_=mode; duration_=duration; rate_=rate; position_=position;
        paused_=false; blend_elapsed_=0.; entry_elapsed_=0.; exit_weight_=0.;phase_=PerformancePhase::Playing;
        if(position_==duration_) {
            if(mode_==AnimationPlayback::Loop) position_=0.;
            else if(mode_==AnimationPlayback::Pose) phase_=PerformancePhase::Holding;
            else leave(0.);
        }
    }
    void set_rate(double rate) {
        bounded(rate,.1,4.,"Invalid performance rate");
        rate_=rate;
    }
    void pause(bool value) noexcept { paused_=enabled() && value; }
    void seek(double position) {
        bounded(position,0.,duration_,"Invalid performance position");
        if(!enabled()) throw std::logic_error("Cannot seek a stopped performance");
        position_=position;
        phase_=(mode_==AnimationPlayback::Pose && position_==duration_) ? PerformancePhase::Holding : PerformancePhase::Playing;
    }
    void stop(bool immediate=false) noexcept {
        if(immediate) { phase_=PerformancePhase::Stopped; paused_=false; blend_elapsed_=0.; }
        else if(enabled()) leave(0.);
        // Repeated stop requests must not restart an existing blend-out.
    }
    void advance(double delta) {
        bounded(delta,0.,86400.,"Invalid performance delta");
        if(phase_==PerformancePhase::BlendingOut) { leave(blend_elapsed_+delta); return; }
        if(paused_) return;
        if(phase_==PerformancePhase::Holding) {entry_elapsed_=std::min(entry_elapsed_+delta,blend_seconds);return;}
        if(phase_!=PerformancePhase::Playing) return;
        const double playing=mode_==AnimationPlayback::Once?std::min(delta,(duration_-position_)/rate_):delta;
        entry_elapsed_=std::min(entry_elapsed_+playing,blend_seconds);
        const double next=position_+delta*rate_;
        if(mode_==AnimationPlayback::Loop) { position_=std::fmod(next,duration_); return; }
        if(next<duration_) { position_=next; return; }
        const double remainder=(next-duration_)/rate_;
        position_=duration_;
        if(mode_==AnimationPlayback::Pose) phase_=PerformancePhase::Holding;
        else leave(remainder);
    }
};
}

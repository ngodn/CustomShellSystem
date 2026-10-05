#include "performance_clock.hpp"
#include <iostream>
#include <limits>

using namespace css;
namespace {
int checks=0;
void expect(bool value,const char* message) {
    ++checks;
    if(!value) throw std::runtime_error(message);
}
bool near(double a,double b) { return std::abs(a-b)<1.e-9; }
template<class F> void rejects(F action) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    expect(rejected,"Invalid clock input was accepted");
}
}
int main() {
    try {
        PerformanceClock a,b;
        expect(!a.active() && !a.enabled(),"Clock did not start inactive");
        a.begin(AnimationPlayback::Loop,7.25);
        b.begin(AnimationPlayback::Loop,7.25);
        for(int i=0;i<3600;++i) a.advance(1./60.);
        b.advance(60.);
        expect(near(a.position(),b.position()) && near(a.position(),2.),"Loop phase depends on tick partition");
        a.begin(AnimationPlayback::Once,2.);
        a.advance(1.95); a.advance(.1);
        expect(a.phase()==PerformancePhase::BlendingOut && near(a.position(),2.) && !a.enabled(),
               "One-shot did not retain its endpoint while blending out");
        a.stop(); a.stop(); a.advance(.129);
        expect(a.active(),"Repeated stop lost the remaining blend time");
        a.advance(.002);
        expect(!a.active(),"One-shot did not complete restoration window");
        a.begin(AnimationPlayback::Once,2.,4.);
        a.advance(.6);
        expect(a.phase()==PerformancePhase::BlendingOut,"Playback rate incorrectly scales blend-out time");
        a.advance(.081);
        expect(!a.active(),"Fast clip kept an expired blend-out alive");
        a.begin(AnimationPlayback::Pose,1./30.);
        a.advance(1./60.); a.advance(1./60.);
        expect(a.phase()==PerformancePhase::Holding && near(a.position(),1./30.),"Two-key photo pose failed to hold");
        a.advance(500.);
        expect(a.phase()==PerformancePhase::Holding && near(a.position(),1./30.),"Held pose drifted");
        a.seek(0.);
        expect(a.phase()==PerformancePhase::Playing,"Seeking a held pose could not restart it");
        a.pause(true); a.advance(10.);
        expect(near(a.position(),0.),"Paused clip advanced");
        a.stop(); a.advance(.2);
        expect(!a.active() && !a.paused(),"Pause prevented stop cleanup");
        a.begin(AnimationPlayback::Once,10.,1.,3.);
        a.advance(.6);
        expect(near(a.position(),3.6),"Game-time delta was not preserved");
        a.set_rate(.5); a.advance(.6);
        expect(near(a.position(),3.9),"Playback rate change jumped the clock");
        a.stop(true);
        expect(!a.active(),"Gameplay interruption kept a blend active");
        rejects([&]{a.seek(0.);});
        a.begin(AnimationPlayback::Loop,2.);
        a.seek(2.); a.pause(true); a.advance(1.);
        expect(near(a.position(),2.),"Paused endpoint seek wrapped early");
        a.pause(false); a.advance(.1);
        expect(near(a.position(),.1),"Loop failed after endpoint seek");
        const auto old=a.position();
        for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            rejects([&]{a.advance(bad);});
            rejects([&]{a.begin(AnimationPlayback::Once,bad);});
            rejects([&]{a.set_rate(bad);});
            expect(near(a.position(),old) && a.phase()==PerformancePhase::Playing,"Rejected request changed the active clock");
        }
        rejects([&]{a.begin(AnimationPlayback::Pose,0.);});
        rejects([&]{a.begin(AnimationPlayback::Once,1.,1.,2.);});
        rejects([&]{a.set_rate(4.01);});
        a.begin(AnimationPlayback::Once,1.); a.advance(10.);
        expect(!a.active() && near(a.position(),1.),"Large valid delta failed to finish a one-shot");
        a.begin(AnimationPlayback::Pose,1.,1.,1.);
        expect(a.phase()==PerformancePhase::Holding,"Starting at photo endpoint did not hold");
        a.begin(AnimationPlayback::Loop,1.,1.,1.);
        expect(near(a.position(),0.) && a.enabled(),"Starting a loop at its endpoint did not wrap");
        expect(near(a.weight(),0.),"New take did not start at zero weight");
        a.advance(.09);
        expect(near(a.weight(),.5),"Entry easing midpoint is incorrect");
        a.pause(true);a.advance(1.);
        expect(near(a.weight(),.5),"Pause advanced entry blend");
        a.stop();
        expect(near(a.weight(),.5),"Stopping during entry jumped the weight");
        a.advance(.09);
        expect(near(a.weight(),.25),"Exit did not ease from the captured entry weight");
        a.advance(.1);
        expect(!a.active() && near(a.weight(),0.),"Exit left a nonzero blend weight");
        a.begin(AnimationPlayback::Pose,1./30.);a.advance(1./30.);
        expect(a.weight()>0. && a.weight()<1.,"Short photo pose skipped the entry blend");
        a.advance(.2);
        expect(near(a.weight(),1.) && a.phase()==PerformancePhase::Holding,"Held photo pose never completed its entry blend");
        a.begin(AnimationPlayback::Once,1.);b.begin(AnimationPlayback::Once,1.);
        for(int i=0;i<65;++i) a.advance(1./60.);
        b.advance(65./60.);
        expect(near(a.weight(),b.weight()) && a.weight()>0.,"Exit weight depends on tick partition");
        std::cout<<checks<<" performance clock checks passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}

#include "frame_profile.hpp"
#include <iostream>
#include <limits>
using css::FrameProfile;
void require(bool value) { if(!value) throw std::runtime_error("Frame profile assertion failed"); }
int main() {
    using namespace std::chrono;
    FrameProfile p;
    const auto t=FrameProfile::Time{};
    p.begin(.016,t); require(!p.end(false,t)); require(p.rows().empty());
    bool rejected=false;
    try { p.arm("invalid",std::numeric_limits<double>::quiet_NaN(),t); }
    catch(const std::exception&) { rejected=true; }
    require(rejected && !p.active());
    p.arm("slow-frame",3,t);
    rejected=false;
    try { p.arm("overlap",3,t); } catch(const std::exception&) { rejected=true; }
    require(rejected && p.id()=="slow-frame");
    p.begin(.016,t+milliseconds(999)); require(!p.end(false,t+milliseconds(999)));
    p.begin(.045,t+milliseconds(1044)); require(!p.end(false,t+milliseconds(1064)));
    require(p.rows().size()==1);
    require(p.rows()[0].interval_ms==45 && p.rows()[0].core_ms==20 && p.rows()[0].engine_ms==45);
    p.begin(.045,t+seconds(4)); require(p.end(true,t+seconds(4)+milliseconds(20)));
    require(!p.active() && p.reason()=="duration" && p.rows().back().failed);
    p.arm("bounded",30,t); require(p.rows().empty());
    for(size_t i=0;i<FrameProfile::capacity;++i) {
        auto now=t+seconds(1)+microseconds(i*100);
        p.begin(.0001,now);
        require(p.end(false,now+microseconds(10))==(i+1==FrameProfile::capacity));
    }
    require(!p.active() && p.reason()=="frame_capacity" && p.rows().size()==FrameProfile::capacity);
    p.arm("measured-callback",3,t); p.begin(.016,t+seconds(1));
    bool called=false;
    try { p.measure(FrameProfile::cssx_tick,[&] { called=true; throw std::runtime_error("callback"); }); }
    catch(const std::exception&) {}
    require(called); p.end(true,t+seconds(4));
    require(p.rows()[0].failed && p.rows()[0].phase_ms[FrameProfile::cssx_tick]>=0);
    std::cout<<"Frame profile: slow-frame timing, warmup, failures, overlap, reset and bounded sampling passed\n";
}

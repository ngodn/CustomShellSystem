#include "probe_schedule.hpp"
#include <stdexcept>

using Schedule = ccs::runtime::ProbeSchedule;
using State = Schedule::State;
void check(bool condition) {
    if (!condition) throw std::runtime_error("Probe scheduler invariant failed");
}
int main() {
    Schedule probe;
    check(!probe.poll(0) && probe.state() == State::Idle);
    probe.start(100);
    check(probe.poll(100 + Schedule::timeout_us - 1));
    probe.step(50, false);
    probe.step(30, true);
    check(probe.state() == State::Complete && probe.steps() == 2 && probe.maximum_us() == 50);
    probe.cancel();
    check(probe.state() == State::Complete);
    probe.start(200);
    check(probe.run() == 2 && probe.steps() == 0 && probe.maximum_us() == 0);
    probe.cancel();
    check(probe.state() == State::Cancelled && !probe.poll(200));
    probe.start(300);
    check(!probe.poll(300 + Schedule::timeout_us) && probe.state() == State::TimedOut);
    probe.start(400);
    probe.step(Schedule::max_step_us + 1, true);
    check(probe.state() == State::Failed);
    probe.start(500);
    for (unsigned i = 0; i < Schedule::max_steps; ++i) probe.step(1, false);
    check(probe.state() == State::Running);
    probe.step(1, false);
    check(probe.state() == State::Failed);
    probe.start(600);
    probe.fail();
    probe.step(0, true);
    check(probe.state() == State::Failed && probe.steps() == 0);
}

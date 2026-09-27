#include "hook_target.hpp"
#include <memory>
#include <stdexcept>

using Target = ccs::runtime::HookTarget;
using Result = Target::Result;
void check(bool value) { if (!value) throw std::runtime_error("Hook lifetime invariant failed"); }
struct CoreState {
    Target* target{};
    unsigned calls{};
    bool detach_inside{}, recurse{}, clear_inside{};
    void* object{};
    void* frame{};
    void* result{};
};
void callback(void* user, void* object, void* frame, void* result) {
    auto& state = *static_cast<CoreState*>(user);
    ++state.calls;
    check(state.target->running() >= 1);
    check(object == state.object && frame == state.frame && result == state.result);
    if (state.detach_inside) check(!state.target->detach());
    if (state.recurse && state.calls == 1) {
        check(state.target->invoke(object, frame, result) == Result::Returned);
        check(state.target->running() == 1);
    }
    if (state.clear_inside) state.target->clear();
}
void throwing(void*, void*, void*, void*) { throw std::runtime_error("Core callback failure"); }
int main() {
    auto state = std::make_unique<CoreState>();
    int object{}, frame{}, result{};
    state->object = &object; state->frame = &frame; state->result = &result;
    auto target = std::make_shared<Target>(callback, state.get());
    state->target = target.get();
    const auto late_engine_closure = [target, &object, &frame, &result] {
        return target->invoke(&object, &frame, &result);
    };
    state->detach_inside = true; state->recurse = true;
    check(late_engine_closure() == Result::Returned);
    check(state->calls == 2 && target->running() == 0 && target->attached());
    check(target->detach());
    state.reset();
    // A retained engine closure cannot call the freed core state after detachment.
    check(late_engine_closure() == Result::Skipped && !target->attached());
    check(target->detach());
    Target failure(throwing, nullptr);
    check(failure.invoke(nullptr, nullptr, nullptr) == Result::Failed);
    check(failure.running() == 0 && !failure.attached());
    check(failure.invoke(nullptr, nullptr, nullptr) == Result::Skipped);
    CoreState clearing;
    clearing.clear_inside = true;
    Target stopped(callback, &clearing); clearing.target = &stopped;
    check(stopped.invoke(nullptr, nullptr, nullptr) == Result::Returned);
    check(stopped.running() == 0 && !stopped.attached() && clearing.calls == 1);
    check(stopped.invoke(nullptr, nullptr, nullptr) == Result::Skipped);
}

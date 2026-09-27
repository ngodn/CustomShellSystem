#pragma once
#include "ccs_hook_api.h"
#include <limits>

namespace ccs::runtime {
// The host serializes access. Engine closures retain this target, never a core callback copy.
class HookTarget {
public:
    enum class Result { Skipped, Returned, Failed };
    HookTarget(CcsNativePreHook callback, void* user) : callback_(callback), user_(user) {}
    Result invoke(void* object, void* frame, void* result) noexcept {
        if (!callback_ || running_ == std::numeric_limits<uint32_t>::max()) return Result::Skipped;
        ++running_;
        try { callback_(user_, object, frame, result); }
        catch (...) { --running_; clear(); return Result::Failed; }
        --running_;
        return Result::Returned;
    }
    bool detach() noexcept {
        if (running_) return false;
        clear(); return true;
    }
    void clear() noexcept { callback_ = nullptr; user_ = nullptr; }
    uint32_t running() const { return running_; }
    bool attached() const { return callback_ != nullptr; }
private:
    CcsNativePreHook callback_{};
    void* user_{};
    uint32_t running_{};
};
}

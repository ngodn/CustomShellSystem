#pragma once
#include "ccs_hook_api.h"
#include <memory>
#include <mutex>

class CcsHookService {
    struct State;
    std::shared_ptr<State> state_;
    CcsHookHost api_;
    static uint64_t add(void*, void*, CcsNativePreHook, void*) noexcept;
    static int remove(void*, uint64_t) noexcept;
    static int statistics(void*, CcsHookStats*) noexcept;
    static int on_game_thread(void*) noexcept;
public:
    explicit CcsHookService(std::shared_ptr<std::recursive_mutex> gate);
    ~CcsHookService();
    bool game_thread() noexcept;
    bool quiesce() noexcept;
    const CcsHookHost* api() const { return &api_; }
};

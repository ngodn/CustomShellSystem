#pragma once
#include "hook_api.hpp"
#include <memory>
#include <mutex>

class CssHookService {
    struct State;
    std::shared_ptr<State> state_;
    CssHookHost api_;
    static uint64_t add(void*,void*,CssHookCallback,void*) noexcept;
    static bool remove(void*,uint64_t) noexcept;
public:
    explicit CssHookService(std::shared_ptr<std::recursive_mutex> gate);
    ~CssHookService();
    void game_thread() noexcept;
    void quiesce() noexcept;
    const CssHookHost* api() const {return &api_;}
};

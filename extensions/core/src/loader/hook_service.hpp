#pragma once
// Loader-owned per-function hook registry. The engine callbacks registered
// with UE4SS live here, in the permanent DLL, and forward to a slot whose
// callable pointers the core can clear before it is unloaded.
#include "core_abi.h"
#include <memory>
#include <mutex>

class CssxHookService {
    struct State;
    std::shared_ptr<State> state_;
    CssxHookHost api_;
    static uint64_t add(void*,void* function,CssxHookCallback,void* user);
    static int remove(void*,uint64_t);
    static void stats(void*,uint64_t*,uint64_t*,int*);
public:
    explicit CssxHookService(std::shared_ptr<std::recursive_mutex> gate);
    ~CssxHookService();
    void game_thread();          // record the game thread id (called from the tick callback)
    void quiesce();              // shutdown: clear callables, stop dispatch
    const CssxHookHost* api() const { return &api_; }
};

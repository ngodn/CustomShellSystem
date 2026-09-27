#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*CcsNativePreHook)(void* user, void* object, void* frame, void* result);
typedef struct CcsHookStats {
    uint32_t size;
    uint32_t slots;
    uint32_t running;
    uint32_t stopped;
    uint64_t calls;
    uint64_t wrong_thread;
    uint64_t failures;
} CcsHookStats;
typedef struct CcsHookHost {
    uint32_t version;
    uint32_t size;
    void* context;
    uint64_t (*add_native_pre)(void* context, void* function, CcsNativePreHook callback, void* user);
    int (*remove)(void* context, uint64_t id);
    int (*statistics)(void* context, CcsHookStats* output);
    int (*on_game_thread)(void* context);
} CcsHookHost;
#define CCS_HOOK_ABI_VERSION 1
#ifdef __cplusplus
}
#endif

#pragma once
#include <stdint.h>

/* Contract between the permanent loader (dlls/main.dll) and the replaceable
 * runtime (core/cssx_core-<version>.dll). Versioned separately from the
 * extension ABI. Every call is serialized by the loader on the game thread
 * except where noted. Plain C so the layout is unambiguous. */
#ifdef __cplusplus
extern "C" {
#endif
#define CSSX_CORE_ABI 1u

/* Per-function engine hook service owned by the loader, so a core can be
 * replaced without leaving engine callbacks that point into freed code. The
 * callback receives (user, UObject* context, FFrame* stack, void* result). */
typedef void (*CssxHookCallback)(void* user, void* object, void* frame, void* result);
typedef struct CssxHookHost {
    uint32_t abi;
    uint32_t size;
    void* context;
    /* Register a post-call hook on a UFunction. Returns 0 on failure. Game thread only. */
    uint64_t (*add)(void* context, void* function, CssxHookCallback callback, void* user);
    /* Returns 0 while the slot is executing; the caller retries next tick. */
    int (*remove)(void* context, uint64_t id);
    /* Dispatch accounting for the performance record: total callbacks run and
     * total time in them, in QPC ticks; and whether the global script-function
     * interception is currently installed. */
    void (*stats)(void* context, uint64_t* calls, uint64_t* ticks, int* script_hook_installed);
} CssxHookHost;

/* Loader-owned ring of intervals between consecutive engine tick callbacks,
 * in QueryPerformanceCounter ticks. head is the index of the next write;
 * entries wrap. A reader takes a snapshot; entries may change underneath it,
 * which only matters for the newest one. */
typedef struct CssxFrameRing {
    uint32_t capacity;
    volatile uint32_t head;
    volatile uint64_t total;       /* frames recorded since load */
    const int64_t* intervals;      /* capacity entries */
    int64_t frequency;             /* QPC ticks per second */
} CssxFrameRing;

typedef struct CssxLoaderHost {
    uint32_t abi;
    uint32_t size;
    const wchar_t* root;           /* ue4ss/Mods/CSSX */
    const wchar_t* mods_root;      /* ue4ss/Mods */
    const char* loader_version;    /* product version compiled into main.dll */
    void (*log)(const char* utf8); /* appends to CSSX.log; any thread */
    const CssxHookHost* hooks;
    const CssxFrameRing* frames;
} CssxLoaderHost;

typedef struct CssxCoreApi {
    uint32_t abi;
    uint32_t size;
    const char* version;           /* product version compiled into the core */
    void* (*create)(const CssxLoaderHost* host);            /* passive; may run off the game thread */
    void (*tick)(void* core, void* engine, float delta);    /* game thread, every engine tick */
    /* Return 1 when every owned change is restored and the core may be freed.
     * Return 0 to keep it loaded; the loader retries on later ticks. */
    int (*stop)(void* core);
    void (*destroy)(void* core);
} CssxCoreApi;
typedef const CssxCoreApi* (*CssxGetCoreApi)(void);
#ifdef __cplusplus
}
#endif

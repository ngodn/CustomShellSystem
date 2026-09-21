#pragma once
#include <stddef.h>
#include <stdint.h>

/* CSSX extension ABI 3 (standalone CSSX 1.0.0).
 *
 * Every callback is synchronous on the game thread. Strings and sink data are
 * borrowed for the duration of the call; copy them before returning. Never let
 * a C++ exception cross this boundary; return 0 to report failure.
 *
 * Layout rule: ABI 3 structs are strict supersets of the ABI 1 and ABI 2
 * layouts that shipped with CSS 0.3.x/0.4.x. The host presents an extension a
 * CssxHost stamped with the ABI that extension declared and never reads past
 * the size the extension declared. An extension built against an older header
 * therefore loads unchanged; an extension declaring an unknown ABI is rejected
 * before create() runs. Member order below is frozen. */
#define CSSX_ABI 3u
#ifdef _WIN32
#define CSSX_EXPORT __declspec(dllexport)
#else
#define CSSX_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
struct CssxHudApi;   /* cssx/hud.h, ABI 2+ */
struct CssxFrame;    /* cssx/hud.h, ABI 2+ */

typedef void (*CssxSink)(void* context, const char* utf8, size_t length);

/* Log severities for CssxHost.log. */
enum CssxLogLevel { CSSX_LOG_DEBUG = 0, CSSX_LOG_INFO = 1, CSSX_LOG_WARNING = 2, CSSX_LOG_ERROR = 3 };

typedef struct CssxHost {
    uint32_t abi;
    uint32_t size;
    void* context;
    /* JSON request bridge. Nonzero return is success; the sink receives one
     * UTF-8 JSON value in one or more chunks. See docs/abi.md for the ops. */
    int (*request)(void* context, const char* json, CssxSink sink, void* output);
    /* ABI 2+. Null for ABI-1 extensions. Owned by the host, valid for the
     * extension's lifetime, callable only on the game thread. */
    const struct CssxHudApi* hud;
    /* ABI 3+. Direct services that avoid JSON on hot paths. */
    void (*log)(void* context, int level, const char* utf8, size_t length);
    /* Mark the extension's menu model stale so the UI re-reads model(). Cheap;
     * coalesced by the host. Call only when displayed data changed. */
    void (*invalidate)(void* context);
    /* Monotonic microseconds, for an extension's own cost accounting. */
    uint64_t (*now_us)(void* context);
} CssxHost;

typedef struct CssxExtension {
    uint32_t abi;
    uint32_t size;
    /* Return an instance, or null on failure. Start passively: no gameplay
     * change may happen because an extension loaded. */
    void* (*create)(const CssxHost* host);
    /* Optional. Coalesced to about 10 Hz. Return 1 on success. */
    int (*tick)(void* instance, double seconds);
    /* Emit the menu model or, with a static menu file, the value bindings. */
    int (*model)(void* instance, CssxSink sink, void* output);
    /* Handle a validated control event {"id":..., "value":..., "confirmed":...}. */
    int (*event)(void* instance, const char* json);
    /* Restore every owned change. Return 0 if cleanup must be retried; the
     * host then keeps the DLL loaded and refuses to unload it. */
    int (*stop)(void* instance);
    void (*destroy)(void* instance);
    /* ABI 2+, optional. Once per rendered frame while world_ready. Cheap work
     * only; push retained updates through CssxHost.hud. Return 0 to fail the
     * frame (suspends the extension like a failed tick). */
    int (*render)(void* instance, const struct CssxFrame* frame);
    /* ABI 3+, optional. Short JSON for the library card, read at most once per
     * second while the library is shown:
     *   {"summary":"2 cheats active","active":true}
     * Absent or failing status simply shows nothing. */
    int (*status)(void* instance, CssxSink sink, void* output);
} CssxExtension;
typedef const CssxExtension* (*CssxGetExtension)(void);

/* Byte sizes of the frozen historical layouts, used by the host's adapter. */
#define CSSX_ABI1_EXTENSION_SIZE ((uint32_t)offsetof(CssxExtension, render))
#define CSSX_ABI2_EXTENSION_SIZE ((uint32_t)offsetof(CssxExtension, status))
#define CSSX_ABI2_HOST_SIZE      ((uint32_t)offsetof(CssxHost, log))
#ifdef __cplusplus
}
#endif

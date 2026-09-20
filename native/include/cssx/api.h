#pragma once
#include <stddef.h>
#include <stdint.h>

/* All callbacks are synchronous on the game thread. Copy borrowed strings
 * before returning. Never retain a sink or let a C++ exception cross this ABI.
 *
 * ABI 2 adds an optional per-frame HUD surface (see cssx/hud.h): CssxExtension
 * gains an optional `render`, CssxHost gains a `hud` services table, CssxRuntime
 * gains a `render` dispatch. ABI 2 is a strict superset of ABI 1: the struct
 * layouts through the ABI-1 members are unchanged, so an ABI-1 extension built
 * against the old header still validates and loads. The host stamps each
 * extension's CssxHost.abi with the abi that extension declared, so an ABI-1
 * extension that checks `host->abi == 1` keeps working against an ABI-2 host. */
#define CSSX_ABI 2u
#ifdef _WIN32
#define CSSX_EXPORT __declspec(dllexport)
#else
#define CSSX_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
struct CssxHudApi;   /* cssx/hud.h — HUD services (host -> extension), null before ABI 2 */
struct CssxFrame;    /* cssx/hud.h — per-frame inputs passed to render() */

typedef void (*CssxSink)(void* context, const char* utf8, size_t length);
typedef struct CssxHost {
    uint32_t abi;
    uint32_t size;
    void* context;
    int (*request)(void* context, const char* json, CssxSink sink, void* output);
    /* ABI 2+. Null when the host is ABI 1 or the extension declared ABI 1.
     * The pointed-to table is owned by the host and valid for the extension's
     * lifetime; its calls are valid only on the game thread. */
    const struct CssxHudApi* hud;
} CssxHost;
typedef struct CssxExtension {
    uint32_t abi;
    uint32_t size;
    void* (*create)(const CssxHost* host);
    int (*tick)(void* instance, double seconds);
    int (*model)(void* instance, CssxSink sink, void* output);
    int (*event)(void* instance, const char* json);
    /* Return zero if restoring owned changes failed. The host keeps the DLL
     * loaded and refuses reload rather than freeing code still in use. */
    int (*stop)(void* instance);
    void (*destroy)(void* instance);
    /* ABI 2+, optional. Called once per rendered frame on the game thread with
     * the frame inputs the host precomputed. Do only cheap work and push HUD
     * updates through CssxHost.hud. Null-checked; may be omitted. Return zero to
     * fail the frame (suspends the extension, same as a failing tick). */
    int (*render)(void* instance, const struct CssxFrame* frame);
} CssxExtension;
typedef const CssxExtension* (*CssxGetExtension)(void);

typedef struct CssxRuntime {
    uint32_t abi;
    uint32_t size;
    void* (*create)(const CssxHost*, const wchar_t* root);
    int (*tick)(void*, double seconds);
    int (*request)(void*, const char* json, CssxSink, void*);
    int (*stop)(void*);
    void (*destroy)(void*);
    /* ABI 2+. Dispatch one frame to every loaded ABI-2 extension's render(). */
    int (*render)(void*, const struct CssxFrame*);
    /* Optional ABI-2 tail. A host checks size before reading this member.
     * Return nonzero only when a live extension consumes frame inputs. Older
     * runtimes without this query retain unconditional frame preparation. */
    int (*needs_frame)(void*);
} CssxRuntime;
typedef const CssxRuntime* (*CssxGetRuntime)(void);
#ifdef __cplusplus
}
#endif

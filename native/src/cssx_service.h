#pragma once
/* Copy of the published CSSX service contract (extensions/core/include/cssx/service.h). CSS
 * does not depend on CSSX: the layout is frozen at ABI 1, so a copy is the contract. */
/* CSSX services, ABI 1: a mod-neutral way for any native module in the game process to offer
 * JSON calls to CSSX extensions. CSSX knows no service names and no provider modules.
 *
 * A provider exports one C function:
 *
 *     extern "C" __declspec(dllexport) const CssxServiceTable* cssx_services(void);
 *
 * Rules for providers:
 *   - Return NULL while the module cannot serve (not started, shutting down, an old copy left
 *     behind by a hot swap). CSSX asks again before every call, so the answer may change.
 *   - The table and every string in it stay valid while the module is loaded.
 *   - Names are lowercase "provider.verb" ([a-z0-9._-], 3-64 bytes, at least one dot).
 *   - `version` goes up only when the request or reply format changes incompatibly.
 *   - `call` runs on the game thread. It must not throw or longjmp across the boundary.
 *     It sends exactly one JSON value to `sink` and returns 1, or sends {"error": "..."} and
 *     returns 0. It may refuse (return 0) when called at a bad moment, e.g. re-entrantly.
 *   - `sink` and `context` are only valid during the call.
 *
 * Providers do not need CSSX at build or run time: copy this header, keep the layout. CSSX
 * finds providers by scanning loaded modules for the export, at most once a second on a miss.
 * Extensions use the bridge ops service.list and service.call (docs/abi.md). */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSSX_SERVICE_ABI 1u
#define CSSX_SERVICES_EXPORT "cssx_services"

typedef void (*CssxServiceSink)(void* context, const char* data, size_t size);
typedef int (*CssxServiceCall)(void* provider, const char* request, size_t size, CssxServiceSink sink, void* context);

typedef struct CssxService {
    const char* name;          /* "provider.verb" */
    uint32_t version;          /* request/reply format version, 1 or higher */
    const char* description;   /* one line for service.list; may be NULL */
    CssxServiceCall call;
    void* provider;            /* passed back as the first argument of call */
} CssxService;

typedef struct CssxServiceTable {
    uint32_t abi;              /* CSSX_SERVICE_ABI */
    uint32_t size;             /* sizeof(CssxServiceTable) */
    uint32_t count;            /* entries in services, at most 64 */
    const CssxService* services;
} CssxServiceTable;

typedef const CssxServiceTable* (*CssxServicesFn)(void);

#ifdef __cplusplus
}
static_assert(sizeof(void*) != 8 || (sizeof(CssxService) == 40 && sizeof(CssxServiceTable) == 24), "CSSX service ABI 1 layout");
#endif

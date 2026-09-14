#pragma once
#include <stddef.h>
#include <stdint.h>

/* All callbacks are synchronous on the game thread. Copy borrowed strings
 * before returning. Never retain a sink or let a C++ exception cross this ABI. */
#define CSSX_ABI 1u
#ifdef _WIN32
#define CSSX_EXPORT __declspec(dllexport)
#else
#define CSSX_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*CssxSink)(void* context, const char* utf8, size_t length);
typedef struct CssxHost {
    uint32_t abi;
    uint32_t size;
    void* context;
    int (*request)(void* context, const char* json, CssxSink sink, void* output);
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
} CssxRuntime;
typedef const CssxRuntime* (*CssxGetRuntime)(void);
#ifdef __cplusplus
}
#endif

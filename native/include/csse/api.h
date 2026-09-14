#pragma once
#include <stddef.h>
#include <stdint.h>

/* All callbacks are synchronous on the game thread. Copy borrowed strings
 * before returning. Never retain a sink or let a C++ exception cross this ABI. */
#define CSSE_ABI 1u
#ifdef _WIN32
#define CSSE_EXPORT __declspec(dllexport)
#else
#define CSSE_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*CsseSink)(void* context, const char* utf8, size_t length);
typedef struct CsseHost {
    uint32_t abi;
    uint32_t size;
    void* context;
    int (*request)(void* context, const char* json, CsseSink sink, void* output);
} CsseHost;
typedef struct CsseExtension {
    uint32_t abi;
    uint32_t size;
    void* (*create)(const CsseHost* host);
    int (*tick)(void* instance, double seconds);
    int (*model)(void* instance, CsseSink sink, void* output);
    int (*event)(void* instance, const char* json);
    /* Return zero if restoring owned changes failed. The host keeps the DLL
     * loaded and refuses reload rather than freeing code still in use. */
    int (*stop)(void* instance);
    void (*destroy)(void* instance);
} CsseExtension;
typedef const CsseExtension* (*CsseGetExtension)(void);

typedef struct CsseRuntime {
    uint32_t abi;
    uint32_t size;
    void* (*create)(const CsseHost*, const wchar_t* root);
    int (*tick)(void*, double seconds);
    int (*request)(void*, const char* json, CsseSink, void*);
    int (*stop)(void*);
    void (*destroy)(void*);
} CsseRuntime;
typedef const CsseRuntime* (*CsseGetRuntime)(void);
#ifdef __cplusplus
}
#endif

#pragma once
#include <cstdint>

// Optional service exported by the permanent loader. The original CSS core
// ABI remains unchanged, so loaders and cores without this service still work.
using CssHookCallback = void(*)(void* user,void* object,void* frame,void* result) noexcept;
struct CssHookHost {
    uint32_t abi,size;
    void* context;
    uint64_t (*add)(void*,void* function,CssHookCallback,void* user) noexcept;
    bool (*remove)(void*,uint64_t) noexcept;
};
using CssGetHookHost = const CssHookHost*(*)() noexcept;

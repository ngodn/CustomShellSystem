#pragma once
#include <cstdint>

// Only the permanent loader registers callbacks with UE4SS. All calls are
// serialized by the loader; no core thread or callable escapes this boundary.
inline constexpr uint32_t css_abi = 1;
struct CssHost {
    uint32_t abi;
    const wchar_t* root;
    void (*log)(const char*) noexcept;
};
struct CssCore {
    uint32_t abi;
    void* (*create)(const CssHost*) noexcept;
    void (*tick)(void*, void* engine, float) noexcept;
    void (*render)(void*) noexcept;
    bool (*stop)(void*) noexcept;
    void (*destroy)(void*) noexcept;
};
using CssGetApi = const CssCore* (*)() noexcept;

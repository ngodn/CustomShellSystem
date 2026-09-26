#pragma once
#include <cstdint>

// Only the permanent loader registers callbacks with UE4SS. All calls are
// serialized by the loader; no core thread or callable escapes this boundary.
inline constexpr uint32_t css_abi = 1;        // the core table below; unchanged since 1.0
inline constexpr uint32_t css_host_abi = 2;   // the host table: 2 adds write_file
struct CssHost {
    uint32_t abi;
    const wchar_t* root;
    void (*log)(const char*) noexcept;
    // Host ABI 2. Queue an atomic file write (temp, read-back, replace) on the loader's own
    // writer thread, so the game thread never waits on the disk. `flags` bit 0 keeps the
    // previous file as <path>.bak. The bytes are copied before this returns. A core reads
    // this field only when abi >= 2; the loader hands a core the host shape it asked for:
    // a core exporting css_get_api2 understands this table, one exporting only css_get_api
    // gets abi 1.
    void (*write_file)(const wchar_t* path, const char* data, size_t size, uint32_t flags) noexcept;
};
struct CssCore {
    uint32_t abi;
    void* (*create)(const CssHost*) noexcept;
    void (*tick)(void*, void* engine, float) noexcept;
    void (*render)(void*) noexcept;
    bool (*stop)(void*) noexcept;
    void (*destroy)(void*) noexcept;
};
using CssGetApi = const CssCore* (*)() noexcept;   // exported as css_get_api (host abi 1) and css_get_api2 (host abi 2)

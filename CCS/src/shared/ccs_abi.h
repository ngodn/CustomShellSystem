#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ccs_hook_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CcsLoaderContext {
    uint32_t abi_version;
    uint32_t size;
    const wchar_t* mod_root;      // e.g. L"ue4ss/Mods/CCS"
    void (*log_info)(const char* msg);
    void (*log_warn)(const char* msg);
    void (*log_error)(const char* msg);
    void (*register_hook)(const wchar_t* function_name, void* pre_callback, void* post_callback);
    const CcsHookHost* hooks;     // Appended capability. The core checks context size before reading it.
} CcsLoaderContext;

typedef struct CcsPlayerContext {
    void* engine;
    void* player_controller;       // APlayerController*
    void* player_character;        // ASpartaCharacter*
    void* weapon_actor;            // ASpartaWeapon*
    void* ability_system_comp;     // USpartaAbilitySystemComponent*
} CcsPlayerContext;

typedef struct CcsCoreApi {
    uint32_t abi_version;
    uint32_t size;
    int (*init)(const CcsLoaderContext* loader);
    void (*tick)(const CcsPlayerContext* player, double delta_seconds);
    void (*on_hotkey)(uint32_t key_code);
    int (*stop)(void);             // Game thread: restore owned engine state before normal unload.
    void (*shutdown)(void);
    const char* (*get_status_json)(void);
} CcsCoreApi;

#define CCS_ABI_VERSION 1

#ifdef CCS_BUILD_CORE
#define CCS_CORE_EXPORT __declspec(dllexport)
#else
#define CCS_CORE_EXPORT __declspec(dllimport)
#endif

CCS_CORE_EXPORT const CcsCoreApi* ccs_get_core_api(void);

#ifdef __cplusplus
}
#endif

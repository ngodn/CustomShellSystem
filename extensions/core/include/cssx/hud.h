#pragma once
#include <stddef.h>
#include <stdint.h>

/* CSSX HUD surface (ABI 2, unchanged layout in ABI 3).
 *
 * Retained mode: build layers once, push cheap updates from render(). The host
 * epsilon-gates the actual widget writes, so pushing an unchanged value every
 * frame costs a compare. The host owns world lifecycle: when the HUD it built
 * on disappears it drops every layer and bumps CssxFrame.world_generation; on
 * a change the extension forgets its handles and rebuilds. */
#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t CssxLayer;   /* opaque; 0 means invalid. Parent 0 = HUD root. */

typedef struct CssxFrame {
    uint32_t abi;
    uint32_t size;
    double seconds;               /* delta since last render */
    uint32_t world_generation;    /* bumped on every world teardown / new HUD */
    int32_t  world_ready;         /* 1 when a playable HUD + pawn exist this frame */
    int32_t  in_menu;             /* 1 when a game menu (or CSSX's menu) is open */
    double camera_yaw;            /* degrees, normalized to (-180, 180] */
    double player_x, player_y, player_z;        /* world position, cm */
    double player_yaw;            /* degrees */
    double velocity_x, velocity_y, velocity_z;  /* cm/s */
    double viewport_w, viewport_h;               /* logical pixels */
    uint64_t pawn;                /* $object id of the player pawn, for request() */
    uint64_t controller;          /* $object id of the player controller */
} CssxFrame;

typedef struct CssxHudApi {
    uint32_t abi;
    uint32_t size;
    CssxLayer (*image)(void* ctx, CssxLayer parent);
    CssxLayer (*text)(void* ctx, CssxLayer parent);
    CssxLayer (*widget)(void* ctx, CssxLayer parent, const char* class_path, size_t length);
    void (*destroy)(void* ctx, CssxLayer layer);
    /* Import a PNG as a Texture2D, cached by path relative to the extension
     * directory. Returns an opaque nonzero texture id or 0. */
    uint64_t (*texture)(void* ctx, const char* path, size_t length);
    /* The layer's widget as a host $object id (0 if none). Off the hot path. */
    uint64_t (*layer_object)(void* ctx, CssxLayer layer);
    void (*set_brush)(void* ctx, CssxLayer layer, uint64_t texture);
    void (*set_rect)(void* ctx, CssxLayer layer, float x, float y, float w, float h,
                     float align_x, float align_y, int32_t z_order);
    void (*set_translation)(void* ctx, CssxLayer layer, float x, float y);
    void (*set_scale)(void* ctx, CssxLayer layer, float sx, float sy);
    void (*set_angle)(void* ctx, CssxLayer layer, float degrees);
    void (*set_pivot)(void* ctx, CssxLayer layer, float px, float py);
    void (*set_opacity)(void* ctx, CssxLayer layer, float alpha);
    void (*set_color)(void* ctx, CssxLayer layer, float r, float g, float b, float a);
    void (*set_visible)(void* ctx, CssxLayer layer, int32_t visible);
    void (*set_text)(void* ctx, CssxLayer layer, const char* utf8, size_t length);
    void (*set_font)(void* ctx, CssxLayer layer, float size);
    void (*set_clip)(void* ctx, CssxLayer layer, int32_t clip);
    void (*set_anchor)(void* ctx, CssxLayer layer, float min_x, float min_y, float max_x, float max_y);
    /* ABI 2 only: a CSS-specific minimap path. The standalone host keeps the
     * slot for layout compatibility and implements it as a no-op that logs
     * once. Use widget() + layer_object() + request() instead. */
    void (*minimap_update)(void* ctx, int32_t visible, float scale, float opacity,
                           float arrow_angle, float map_angle,
                           double pan_x, double pan_y, double zoom_m, uint32_t flags);
} CssxHudApi;

#ifdef __cplusplus
}
#endif

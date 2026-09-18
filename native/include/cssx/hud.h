#pragma once
#include <stddef.h>
#include <stdint.h>

/* CSSX ABI 2 HUD surface. The host (CSS core) owns every widget; an extension
 * holds only opaque CssxLayer handles and never touches a UObject directly. All
 * calls are valid only on the game thread, from create/tick/render/stop.
 *
 * Model: retained-mode. Build layers once (image/text/widget), then push cheap
 * per-frame updates from render(). The host epsilon-gates the actual Slate
 * writes, so pushing an unchanged value every frame is free. The host also owns
 * world lifecycle: when the world tears down it drops every layer and bumps
 * CssxFrame.world_generation; on a change the extension must forget its handles
 * (they are already invalid) and rebuild. This is the one hard rule. */
#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t CssxLayer;   /* opaque; 0 means invalid. Parent 0 = HUD root. */

/* Per-frame inputs, precomputed by the host and passed to CssxExtension.render. */
typedef struct CssxFrame {
    uint32_t abi;
    uint32_t size;
    double seconds;               /* delta since last render */
    uint32_t world_generation;    /* bumped on every world teardown / new HUD */
    int32_t  world_ready;         /* 1 when a playable HUD + pawn exist this frame */
    int32_t  in_menu;             /* 1 when a native menu/map is open (hide the HUD) */
    double camera_yaw;            /* degrees, normalized to (-180, 180] */
    double player_x, player_y, player_z;        /* world position, cm */
    double player_yaw;            /* degrees */
    double velocity_x, velocity_y, velocity_z;  /* cm/s; planar speed = hypot(x,y) */
    double viewport_w, viewport_h;               /* logical pixels */
    uint64_t pawn;                /* $object id of the player pawn, for host request() */
    uint64_t controller;          /* $object id of the player controller */
} CssxFrame;

/* HUD services table pointed to by CssxHost.hud. Versioned like every CSSX
 * struct. `ctx` is CssxHost.context (pass it back unchanged). */
typedef struct CssxHudApi {
    uint32_t abi;
    uint32_t size;

    /* construction — realized against the live HUD by the host. Return 0 on
     * failure. `parent` 0 attaches to the HUD root overlay. */
    CssxLayer (*image)(void* ctx, CssxLayer parent);
    CssxLayer (*text)(void* ctx, CssxLayer parent);
    /* Construct any UMG widget by class path (e.g.
     * "/Script/Sparta.SpartaMapWidget"). Drive its exotic setup off the hot path
     * via layer_object() + the host request() call/set ops. */
    CssxLayer (*widget)(void* ctx, CssxLayer parent, const char* class_path, size_t length);
    void (*destroy)(void* ctx, CssxLayer layer);

    /* Import a PNG as a Texture2D, cached by path. Returns a texture id (opaque,
     * nonzero) or 0. Path is resolved relative to the extension directory. */
    uint64_t (*texture)(void* ctx, const char* path, size_t length);
    /* The layer's underlying widget as a host $object id (0 if none), usable with
     * the host request() reflection ops. Off the hot path only. */
    uint64_t (*layer_object)(void* ctx, CssxLayer layer);

    /* retained appearance — write only when your own value changed. */
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
    // Set the layer's canvas-slot anchors (0..1 of the parent). E.g. (0.5,0,0.5,0)
    // with alignment (0.5,0) centres it horizontally regardless of viewport size.
    void (*set_anchor)(void* ctx, CssxLayer layer, float min_x, float min_y, float max_x, float max_y);
    // Direct minimap transform update (bypasses JSON serialization on hot per-frame paths).
    void (*minimap_update)(void* ctx, int32_t visible, float scale, float opacity,
                           float arrow_angle, float map_angle,
                           double pan_x, double pan_y, double zoom_m, uint32_t flags);
} CssxHudApi;

#ifdef __cplusplus
}
#endif

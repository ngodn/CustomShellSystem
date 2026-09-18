// Minimap component (Phase 1b). The heavy lifting — the SpartaMapWidget tile map,
// the circle RetainerBoxes, the arrow — lives natively in css_core (it must
// configure the map widget inline; a $object handle to it "expires"). This side
// just owns the settings and drives it per frame through the hud.minimap.* ops:
// build(config) on world change, update(pan/angle/scale/opacity) each frame.
#pragma once
#include <cssx/client.hpp>
#include <cssx/hud.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

namespace {
using cssx::Json;

struct Minimap {
    // BP_WorldMapSettings_2D calibration (pan uses these; zoom is applied core-side).
    static constexpr double SPAN = 315552.0, ORIGIN_X = 38050.053612, ORIGIN_Y = -134391.872342;

    // settings (persisted)
    bool  enabled = false;
    float size = 200.f, zoom_m = 1000.f, opacity = 0.9f, offx = 28.f, offy = 28.f;
    int   orientation = 1;   // 0 north, 1 camera, 2 player
    int   shape = 1;         // 0 square, 1 circle
    float jog_scale = 0.85f, jog_opacity = 0.6f, sprint_scale = 0.75f, sprint_opacity = 0.6f;
    // animation gates (Animation tab) — default on
    bool  anim_jog = true, anim_sprint = true;

    // runtime
    std::string status = "Minimap off.";
    uint32_t built_gen = 0xFFFFFFFFu;
    bool built = false;
    int gait = 0;
    float cur_scale = 1.f, cur_opacity = 0.9f;
    double map_after = 0;   // throttles the expensive pan (map re-render) to ~20 Hz.
    int dbg_gait = -2;      // diagnostic: log the scale decision when gait changes.

    static double mm_norm(double a) { a = std::fmod(a, 360.0); if(a > 180) a -= 360; if(a <= -180) a += 360; return a; }
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

    void hide(cssx::Client& host, void* ctx = nullptr, const CssxHudApi* hud = nullptr) {
        if(!built) return;
        if(hud && hud->size >= sizeof(CssxHudApi) && hud->minimap_update) {
            hud->minimap_update(ctx, 0, 1.0f, 1.0f, 0.0f, 0.0f, 0.0, 0.0, 0.0, 0x01);
            return;
        }
        try { host.request({{"op", "hud.minimap.update"}, {"update", {{"visible", 0}}}}); } catch(...) {}
    }

    void render(cssx::Client& host, void* ctx, const CssxHudApi* hud, const CssxFrame* f, const std::function<void(const std::string&)>& log) {
        const bool want = enabled && f->world_ready && !f->in_menu;
        if(!want) { hide(host, ctx, hud); return; }

        if(f->world_generation != built_gen) {
            built_gen = f->world_generation;
            // Tear the old widget down first: the core's build() no-ops if one is
            // still alive, so a size/shape/orientation change needs a fresh build.
            try { host.request({{"op", "hud.minimap.destroy"}}); } catch(...) {}
            try {
                auto r = host.request({{"op", "hud.minimap.build"}, {"config", {
                    {"size", size}, {"offx", offx}, {"offy", offy}, {"zoom_m", zoom_m},
                    {"orientation", orientation}, {"shape", shape}}}});
                built = r.is_boolean() ? r.get<bool>() : true;
                status = built ? "Minimap active." : "Minimap building...";
            } catch(...) { built = false; status = "Minimap build failed."; }
        }

        // gait -> target scale/opacity, gated by the Animation-tab toggles, eased.
        const double speed = std::hypot(f->velocity_x, f->velocity_y);
        constexpr double JOG = 300.0, SPRINT = 700.0, H = 40.0;
        if(gait < 2 && speed >= SPRINT + (gait < 2 ? H : 0)) gait = 2;
        else if(gait == 2 && speed < SPRINT - H) gait = 1;
        if(gait < 1 && speed >= JOG + (gait < 1 ? H : 0)) gait = 1;
        else if(gait == 1 && speed < JOG - H) gait = 0;
        float ts = 1.f, to = opacity;
        if(gait == 2 && anim_sprint) { ts = sprint_scale; to = sprint_opacity; }
        else if(gait == 1 && anim_jog) { ts = jog_scale; to = jog_opacity; }
        const float k = clampf(float(f->seconds) * 8.f, 0.f, 1.f);
        cur_scale += (ts - cur_scale) * k;
        cur_opacity += (to - cur_opacity) * k;

        const double panx = 0.5 - (f->player_x - ORIGIN_X) / SPAN;
        const double pany = 0.5 - (f->player_y - ORIGIN_Y) / SPAN;
        const double mapang = orientation == 0 ? 0.0 : orientation == 1 ? mm_norm(90.0 - f->camera_yaw) : mm_norm(90.0 - f->player_yaw);
        const double arrang = mm_norm(f->player_yaw - 90.0 + mapang);

        // Only SetPanCenterNormalized re-renders the map tiles (it invalidates the
        // retainer cache); the arrow angle, scale and opacity are post-cache transforms
        // that are effectively free. So the cheap transforms go every frame for a smooth
        // arrow, and the expensive pan (plus the map rotation it pairs with) is throttled
        // to ~20 Hz. Direct C ABI avoids JSON serialization on hot per-frame paths.
        uint32_t flags = 0x01 | 0x02 | 0x04 | 0x08; // visible, scale, opacity, arrow_angle
        map_after -= f->seconds;
        if(map_after <= 0.0) {
            map_after = 0.05;
            flags |= 0x10 | 0x20 | 0x40; // map_angle, pan, zoom
        }

        if(hud && hud->size >= sizeof(CssxHudApi) && hud->minimap_update) {
            hud->minimap_update(ctx, 1, cur_scale, cur_opacity, float(arrang), float(mapang), panx, pany, zoom_m, flags);
        } else {
            Json upd = {{"visible", 1}, {"arrow_angle", arrang}, {"scale", cur_scale}, {"opacity", cur_opacity}};
            if(flags & 0x20) {
                upd["pan_x"] = panx; upd["pan_y"] = pany; upd["map_angle"] = mapang; upd["zoom_m"] = zoom_m;
            }
            try { host.request({{"op", "hud.minimap.update"}, {"update", upd}}); } catch(...) {}
        }
    }

    // ---- settings plumbing ----
    void fill_values(Json& v) const {
        v["minimap_enabled"] = enabled; v["minimap_size"] = size; v["minimap_zoom"] = zoom_m;
        v["minimap_shape"] = shape == 0 ? "square" : "circle";
        v["minimap_orientation"] = orientation == 0 ? "north" : orientation == 1 ? "camera" : "player";
        v["minimap_opacity"] = opacity;
        v["minimap_jog_scale"] = jog_scale; v["minimap_jog_opacity"] = jog_opacity;
        v["minimap_sprint_scale"] = sprint_scale; v["minimap_sprint_opacity"] = sprint_opacity;
        v["anim_minimap_jog"] = anim_jog; v["anim_minimap_sprint"] = anim_sprint;
    }
    bool handle_event(const std::string& id, const Json& val) {
        if(id == "minimap_enabled") enabled = val.get<bool>();
        else if(id == "minimap_size") { size = val.get<float>(); built_gen = 0xFFFFFFFFu; }
        // Zoom and orientation are applied live in the per-frame update (SetZoom /
        // SetRenderTransformAngle), like the Cartographer reference, so they take effect
        // immediately without tearing the widget down. Only size/shape change the layout.
        else if(id == "minimap_zoom") { zoom_m = val.get<float>(); }
        else if(id == "minimap_shape") { shape = val.get<std::string>() == "square" ? 0 : 1; built_gen = 0xFFFFFFFFu; }
        else if(id == "minimap_orientation") { auto s = val.get<std::string>(); orientation = s == "north" ? 0 : s == "player" ? 2 : 1; }
        else if(id == "minimap_opacity") opacity = val.get<float>();
        else if(id == "minimap_jog_scale") jog_scale = val.get<float>();
        else if(id == "minimap_jog_opacity") jog_opacity = val.get<float>();
        else if(id == "minimap_sprint_scale") sprint_scale = val.get<float>();
        else if(id == "minimap_sprint_opacity") sprint_opacity = val.get<float>();
        else if(id == "anim_minimap_jog") anim_jog = val.get<bool>();
        else if(id == "anim_minimap_sprint") anim_sprint = val.get<bool>();
        else return false;
        return true;
    }
    void load_from(const Json& st) {
        enabled = st.value("minimap_enabled", enabled);
        size = st.value("minimap_size", size);
        zoom_m = st.value("minimap_zoom", zoom_m);
        shape = st.value("minimap_shape", shape);
        opacity = st.value("minimap_opacity", opacity);
        orientation = st.value("minimap_orientation", orientation);
        jog_scale = st.value("minimap_jog_scale", jog_scale);
        jog_opacity = st.value("minimap_jog_opacity", jog_opacity);
        sprint_scale = st.value("minimap_sprint_scale", sprint_scale);
        sprint_opacity = st.value("minimap_sprint_opacity", sprint_opacity);
        anim_jog = st.value("anim_minimap_jog", anim_jog);
        anim_sprint = st.value("anim_minimap_sprint", anim_sprint);
        cur_opacity = opacity;
    }
    void save_into(Json& v) const {
        v["minimap_enabled"] = enabled; v["minimap_size"] = size; v["minimap_zoom"] = zoom_m;
        v["minimap_shape"] = shape; v["minimap_opacity"] = opacity; v["minimap_orientation"] = orientation;
        v["minimap_jog_scale"] = jog_scale; v["minimap_jog_opacity"] = jog_opacity;
        v["minimap_sprint_scale"] = sprint_scale; v["minimap_sprint_opacity"] = sprint_opacity;
        v["anim_minimap_jog"] = anim_jog; v["anim_minimap_sprint"] = anim_sprint;
    }
};
} // namespace

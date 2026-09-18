// CSSX - Compass & Minimap. A native ABI-2 HUD extension.
//
// Phase 1a: the Elden-style heading tape (compass), rebuilt in C++ from
// KalilaViolette's MS2 Compass (nexus #193), plus the requested idle/jog/sprint
// shrink-and-fade that the original never had. The minimap tab is a placeholder
// until Phase 1 wires the SpartaMapWidget on this same HUD surface.
//
// The core owns the widgets; we hold opaque CssxLayer handles and push cheap
// per-frame updates. When the world tears down the core bumps world_generation
// and drops our layers, so we rebuild on any generation change and never touch a
// stale widget.
#include <cssx/client.hpp>
#include <cssx/hud.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include "minimap.hpp"

namespace {
using cssx::Json;
constexpr double PI = 3.14159265358979323846;

// Tape geometry, verbatim from the reference compass.
constexpr float HOST_W = 1000.f, HOST_H = 190.f;
constexpr float TAPE_X = 120.f, TAPE_Y = 9.f, TAPE_W = 760.f, TAPE_H = 102.f;
constexpr float TAPE_FULL_W = TAPE_W * 7.5f;      // 5700: three 360-degree cycles
constexpr float TAPE_CYCLE_W = TAPE_FULL_W / 3.f; // 1900: one 360-degree cycle
constexpr float TAPE_MIN_X = -(TAPE_FULL_W - TAPE_W); // -4940: furthest-left the strip may scroll
constexpr float TAPE_MAX_X = 0.f;                     // and the furthest-right, so the window always sees painted tape
constexpr float FRAME_X = 45.f, FRAME_Y = 4.f, FRAME_W = 910.f, FRAME_H = 128.f;
// Per-type marker icon geometry, verbatim from the reference (index 0=Lost Gloom,
// 1=dungeon, 2=map ping): width, height, top Y, z-order. The reference uses one size
// per marker family and sits them right on the tape; distance text is a centered box
// at (HOST_W/2, 114). Bearing places the marker; only the width/height/Y differ by type.
constexpr float MK_W[3] = {60.48f, 50.4f, 50.4f};
constexpr float MK_H[3] = {83.52f, 69.6f, 66.0f};
constexpr float MK_Y[3] = {-3.52f, 10.4f, 18.0f};
constexpr int   MK_Z[3] = {250, 54, 74};
constexpr float MK_DIST_Y = 114.f;   // distance readout baseline, below the tape

// Gait thresholds in cm/s, from native/src/walk_override.inl (the game jogs ~540,
// sprints ~800). Hysteresis keeps the size from flickering at a boundary.
constexpr double SPEED_JOG = 300.0, SPEED_SPRINT = 700.0, SPEED_HYST = 40.0;

double normalize360(double a) { a = std::fmod(a, 360.0); if(a < 0) a += 360.0; return a; }
float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

struct Settings {
    bool  enabled = true;
    float scale = 1.0f, opacity = 1.0f;
    float jog_scale = 0.85f, jog_opacity = 0.5f;
    float sprint_scale = 0.75f, sprint_opacity = 0.5f;
};

struct Ext {
    cssx::Client host;
    void* ctx = nullptr;
    const CssxHudApi* hud = nullptr;
    Settings s;
    Minimap minimap;
    std::function<void(const std::string&)> logfn;
    std::string status = "Compass ready.";

    // scene
    uint32_t built_generation = 0xFFFFFFFFu;
    bool built = false;
    CssxLayer group = 0, tape_clip = 0, tape = 0, frame = 0, center = 0;
    double vp_w = 0, vp_h = 0;
    // marker pool (Lost Gloom, dungeons, map pings): icon + distance text + (ping) number
    static constexpr int MAX_MK = 10;
    CssxLayer mk_icon[MAX_MK] = {}, mk_text[MAX_MK] = {}, mk_num[MAX_MK] = {};
    int mk_last_type[MAX_MK] = {};   // last type each pool slot was sized for (-1 = none)
    uint64_t tex_gloom = 0, tex_dungeon = 0, tex_ping = 0;
    double mk_after = 0;
    Json mk_data;

    // smoothing / scroll continuity
    float cur_scale = 1.0f, cur_opacity = 1.0f;
    double last_tape_x = 0; bool have_last_tape = false;
    int gait = 0; // 0 idle, 1 jog, 2 sprint (hysteretic)

    explicit Ext(const CssxHost* h) : host(h), ctx(h->context), hud(h->hud) {
        logfn = [this](const std::string& m){ try { host.log("[compass-minimap] " + m); } catch(...) {} };
        load();
    }

    void load() {
        try {
            auto st = host.request({{"op", "state.load"}});
            if(st.is_object()) {
                s.enabled = st.value("enabled", s.enabled);
                s.scale = st.value("scale", s.scale);
                s.opacity = st.value("opacity", s.opacity);
                s.jog_scale = st.value("jog_scale", s.jog_scale);
                s.jog_opacity = st.value("jog_opacity", s.jog_opacity);
                s.sprint_scale = st.value("sprint_scale", s.sprint_scale);
                s.sprint_opacity = st.value("sprint_opacity", s.sprint_opacity);
                minimap.load_from(st);
            }
        } catch(...) {}
        cur_scale = s.scale; cur_opacity = s.opacity;
    }
    void save() {
        try {
            Json v = {
                {"schema", 1}, {"enabled", s.enabled}, {"scale", s.scale}, {"opacity", s.opacity},
                {"jog_scale", s.jog_scale}, {"jog_opacity", s.jog_opacity},
                {"sprint_scale", s.sprint_scale}, {"sprint_opacity", s.sprint_opacity}};
            minimap.save_into(v);
            host.request({{"op", "state.save"}, {"value", v}});
        } catch(...) {}
    }

    uint64_t import(const char* file) {
        try {
            auto abs = host.request({{"op", "asset"}, {"file", file}});
            if(!abs.is_string()) return 0;
            auto p = abs.get<std::string>();
            return hud->texture(ctx, p.data(), p.size());
        } catch(...) { return 0; }
    }

    void forget_scene() {
        group = tape_clip = tape = frame = center = 0;
        built = false; have_last_tape = false;
    }

    bool build() {
        // A generation change means the core already dropped our widgets; start clean.
        forget_scene();
        uint64_t tex_tape = import("assets/CompassTape.png");
        uint64_t tex_frame = import("assets/CompassFrame.png");
        uint64_t tex_center = import("assets/CompassCenter.png");
        if(logfn) logfn("compass build tex tape=" + std::to_string(tex_tape != 0) + " frame=" + std::to_string(tex_frame != 0) + " center=" + std::to_string(tex_center != 0));
        if(!tex_tape || !tex_frame || !tex_center) { status = "Compass textures unavailable."; return false; }

        group = hud->widget(ctx, 0, "/Script/UMG.CanvasPanel", std::strlen("/Script/UMG.CanvasPanel"));
        if(logfn) logfn("compass build group=" + std::to_string(group));
        if(!group) return false;
        hud->set_pivot(ctx, group, 0.5f, 0.0f);
        // Anchor the group's top-centre to the viewport centre so the tape is
        // always horizontally centred regardless of resolution/DPI.
        hud->set_rect(ctx, group, 0.f, 16.f, HOST_W, HOST_H, 0.5f, 0.f, 9000);
        hud->set_anchor(ctx, group, 0.5f, 0.f, 0.5f, 0.f);

        tape_clip = hud->widget(ctx, group, "/Script/UMG.CanvasPanel", std::strlen("/Script/UMG.CanvasPanel"));
        if(!tape_clip) { forget_scene(); return false; }
        hud->set_clip(ctx, tape_clip, 1);
        hud->set_rect(ctx, tape_clip, TAPE_X, TAPE_Y, TAPE_W, TAPE_H, 0, 0, 10);

        tape = hud->image(ctx, tape_clip);
        if(!tape) { forget_scene(); return false; }
        hud->set_brush(ctx, tape, tex_tape);
        hud->set_rect(ctx, tape, 0, 0, TAPE_FULL_W, TAPE_H, 0, 0, 0);

        frame = hud->image(ctx, group);
        if(frame) { hud->set_brush(ctx, frame, tex_frame); hud->set_rect(ctx, frame, FRAME_X, FRAME_Y, FRAME_W, FRAME_H, 0, 0, 30); }

        center = hud->image(ctx, group);
        if(center) { hud->set_brush(ctx, center, tex_center); hud->set_rect(ctx, center, HOST_W * 0.5f - 24.f, 0.f, 48.f, 120.f, 0, 0, 32); }

        // marker pool: each an icon centred on the tape + a distance label below,
        // moved horizontally per frame by set_translation. Textures assigned live.
        tex_gloom = import("assets/LostGloomMarker.png");
        tex_dungeon = import("assets/DungeonMarker.png");
        tex_ping = import("assets/MapPingMarker.png");
        for(int i = 0; i < MAX_MK; ++i) {
            mk_last_type[i] = -1;
            // Icon rect is set per type on first show (sizes differ by marker family).
            mk_icon[i] = hud->image(ctx, group);
            if(mk_icon[i]) hud->set_visible(ctx, mk_icon[i], 0);
            // Distance readout: centered box at (HOST_W/2, 114), white, font 14 (reference).
            mk_text[i] = hud->text(ctx, group);
            if(mk_text[i]) { hud->set_rect(ctx, mk_text[i], HOST_W * 0.5f, MK_DIST_Y, 110.f, 22.f, 0.5f, 0.5f, 61); hud->set_font(ctx, mk_text[i], 14.f); hud->set_color(ctx, mk_text[i], 1.f, 1.f, 1.f, 1.f); hud->set_visible(ctx, mk_text[i], 0); }
            // Ping number label, offset +14 right and near the top (reference), shown only for pings.
            mk_num[i] = hud->text(ctx, group);
            if(mk_num[i]) { hud->set_rect(ctx, mk_num[i], HOST_W * 0.5f + 14.f, 1.f, 40.f, 22.f, 0.5f, 0.5f, 82); hud->set_font(ctx, mk_num[i], 15.f); hud->set_color(ctx, mk_num[i], 1.f, 1.f, 1.f, 1.f); hud->set_visible(ctx, mk_num[i], 0); }
        }
        mk_after = 0; mk_data = Json();

        built = true; status = "Compass active.";
        return true;
    }

    void place_group(double viewport_w, double viewport_h) {
        if(viewport_w <= 0) return;
        if(std::abs(viewport_w - vp_w) < 0.5 && std::abs(viewport_h - vp_h) < 0.5) return;
        vp_w = viewport_w; vp_h = viewport_h;
        const float x = float(viewport_w) * 0.5f - HOST_W * 0.5f;
        hud->set_rect(ctx, group, x, 16.f, HOST_W, HOST_H, 0, 0, 9000);
    }

    void render(const CssxFrame* f) {
        if(!hud) return;
        render_compass(f);
        minimap.render(host, ctx, hud, f, logfn);
    }
    void render_compass(const CssxFrame* f) {
        const bool want = s.enabled && f->world_ready && !f->in_menu;
        if(!want) { if(group) hud->set_visible(ctx, group, 0); return; }

        if(f->world_generation != built_generation) {
            built_generation = f->world_generation;
            build();
        }
        if(!built || !group) return;
        hud->set_visible(ctx, group, 1);

        // Heading tape: map camera yaw to a continuous horizontal scroll, wrapping
        // by whole 360-degree cycles so it never jumps at North.
        const double heading = normalize360(270.0 + f->camera_yaw);
        const double base = -((360.0 + heading - 72.0) / 1080.0) * TAPE_FULL_W;
        double tape_x = base;
        if(have_last_tape) {
            // Pick the texture cycle nearest last frame (the three identical cycles let
            // us cross North without a jump), then keep the coordinate inside the painted
            // range by shifting whole cycles. Same pixels, but bounded, so spinning the
            // camera forever can no longer drift the tape off its texture.
            const double off = std::round((last_tape_x - base) / TAPE_CYCLE_W);
            tape_x = base + off * TAPE_CYCLE_W;
            while(tape_x < TAPE_MIN_X + 2.0) tape_x += TAPE_CYCLE_W;
            while(tape_x > TAPE_MAX_X - 2.0) tape_x -= TAPE_CYCLE_W;
        }
        last_tape_x = tape_x; have_last_tape = true;
        hud->set_translation(ctx, tape, float(tape_x), 0.f);

        // Gait -> target size + opacity, with hysteresis on the thresholds.
        const double speed = std::hypot(f->velocity_x, f->velocity_y);
        const double up_jog = SPEED_JOG + (gait < 1 ? SPEED_HYST : 0);
        const double down_jog = SPEED_JOG - (gait >= 1 ? SPEED_HYST : 0);
        const double up_sprint = SPEED_SPRINT + (gait < 2 ? SPEED_HYST : 0);
        const double down_sprint = SPEED_SPRINT - (gait >= 2 ? SPEED_HYST : 0);
        if(gait < 2 && speed >= up_sprint) gait = 2;
        else if(gait == 2 && speed < down_sprint) gait = 1;
        if(gait < 1 && speed >= up_jog) gait = 1;
        else if(gait >= 1 && gait < 2 && speed < down_jog) gait = 0;

        float target_scale = s.scale, target_opacity = s.opacity;
        if(gait == 2) { target_scale = s.scale * s.sprint_scale; target_opacity = s.sprint_opacity; }
        else if(gait == 1) { target_scale = s.scale * s.jog_scale; target_opacity = s.jog_opacity; }

        // Ease toward the target (~8/s) so it glides rather than snaps.
        const float k = clampf(float(f->seconds) * 8.f, 0.f, 1.f);
        cur_scale += (target_scale - cur_scale) * k;
        cur_opacity += (target_opacity - cur_opacity) * k;
        hud->set_scale(ctx, group, cur_scale, cur_scale);
        hud->set_opacity(ctx, group, cur_opacity);

        // Markers on the tape (Lost Gloom / dungeons / map pings), world positions
        // from the native provider, projected onto the tape by bearing.
        // Markers (Lost Gloom / dungeons / pings) come from a native scan that walks the
        // UObject array, so it is deliberately infrequent: these POIs are static or move
        // slowly, and the compass only needs their bearing. 1.5 s keeps it off the frame
        // budget while still feeling live.
        mk_after -= f->seconds;
        if(mk_after <= 0.0) {
            mk_after = 1.5;
            try { mk_data = host.request({{"op", "hud.markers"}, {"radius", 500.0}}); } catch(...) {}
        }
        struct MK { double x, y; int type, num; };
        std::vector<MK> active;
        if(mk_data.is_object()) {
            if(mk_data.value("gloom", Json()).is_object()) active.push_back({mk_data["gloom"].value("x", 0.0), mk_data["gloom"].value("y", 0.0), 0, 0});
            for(const auto& d : mk_data.value("dungeons", Json::array())) { if((int)active.size() >= MAX_MK) break; active.push_back({d.value("x", 0.0), d.value("y", 0.0), 1, 0}); }
            int pn = 0;
            for(const auto& p : mk_data.value("pings", Json::array())) { if((int)active.size() >= MAX_MK) break; active.push_back({p.value("x", 0.0), p.value("y", 0.0), 2, ++pn}); }
        }
        const double PXPDEG = TAPE_W / 144.0;
        for(int i = 0; i < MAX_MK; ++i) {
            auto hide = [&] { if(mk_icon[i]) hud->set_visible(ctx, mk_icon[i], 0); if(mk_text[i]) hud->set_visible(ctx, mk_text[i], 0); if(mk_num[i]) hud->set_visible(ctx, mk_num[i], 0); };
            if(i >= (int)active.size()) { hide(); continue; }
            const auto& m = active[i];
            const double dx = m.x - f->player_x, dy = m.y - f->player_y;
            double relYaw = Minimap::mm_norm(std::atan2(dy, dx) * 180.0 / PI - f->camera_yaw);
            const double dist_m = std::hypot(dx, dy) / 100.0;
            bool show = std::abs(relYaw) <= 72.0;
            if(m.type == 0) { relYaw = relYaw < -72.0 ? -72.0 : relYaw > 72.0 ? 72.0 : relYaw; show = true; }  // gloom pins to edge
            if(!show) { hide(); continue; }
            const uint64_t tex = m.type == 0 ? tex_gloom : m.type == 1 ? tex_dungeon : tex_ping;
            const float tx = float(relYaw * PXPDEG);
            if(mk_icon[i]) {
                // Size the slot for this marker family only when the family changes.
                if(mk_last_type[i] != m.type) { mk_last_type[i] = m.type; hud->set_rect(ctx, mk_icon[i], HOST_W * 0.5f, MK_Y[m.type], MK_W[m.type], MK_H[m.type], 0.5f, 0.f, MK_Z[m.type]); }
                hud->set_brush(ctx, mk_icon[i], tex); hud->set_translation(ctx, mk_icon[i], tx, 0.f); hud->set_visible(ctx, mk_icon[i], 1);
            }
            if(mk_text[i]) { char buf[24]; std::snprintf(buf, sizeof(buf), "%d m", (int)(dist_m + 0.5)); hud->set_text(ctx, mk_text[i], buf, std::strlen(buf)); hud->set_translation(ctx, mk_text[i], tx, 0.f); hud->set_visible(ctx, mk_text[i], 1); }
            if(mk_num[i]) {
                if(m.type == 2) { char nb[8]; std::snprintf(nb, sizeof(nb), "%d", m.num); hud->set_text(ctx, mk_num[i], nb, std::strlen(nb)); hud->set_translation(ctx, mk_num[i], tx, 0.f); hud->set_visible(ctx, mk_num[i], 1); }
                else hud->set_visible(ctx, mk_num[i], 0);
            }
        }
    }

    Json model() {
        Json values = {
            {"enabled", s.enabled}, {"scale", s.scale}, {"opacity", s.opacity},
            {"jog_scale", s.jog_scale}, {"jog_opacity", s.jog_opacity},
            {"sprint_scale", s.sprint_scale}, {"sprint_opacity", s.sprint_opacity}};
        minimap.fill_values(values);
        return Json{{"values", values}, {"status", status + "   |   " + minimap.status}};
    }
    void event(const Json& e) {
        const auto id = e.value("id", std::string{});
        if(!e.contains("value")) return;
        const auto& v = e.at("value");
        if(id == "enabled") s.enabled = v.get<bool>();
        else if(id == "scale") s.scale = v.get<float>();
        else if(id == "opacity") s.opacity = v.get<float>();
        else if(id == "jog_scale") s.jog_scale = v.get<float>();
        else if(id == "jog_opacity") s.jog_opacity = v.get<float>();
        else if(id == "sprint_scale") s.sprint_scale = v.get<float>();
        else if(id == "sprint_opacity") s.sprint_opacity = v.get<float>();
        else if(minimap.handle_event(id, v)) {}
        else return;
        save();
    }
};

// ---- ABI thunks (never let an exception cross the C boundary) ----
void* create(const CssxHost* h) noexcept { try { return new Ext(h); } catch(...) { return nullptr; } }
int   tick(void*, double) noexcept { return 1; }
int   render(void* i, const CssxFrame* f) noexcept {
    try { if(f) static_cast<Ext*>(i)->render(f); return 1; } catch(...) { return 1; }
}
int   model(void* i, CssxSink sink, void* out) noexcept {
    try { auto t = static_cast<Ext*>(i)->model().dump(); if(sink) sink(out, t.data(), t.size()); return 1; }
    catch(...) { return 0; }
}
int   event(void* i, const char* j) noexcept {
    try { if(!j || std::strlen(j) > 1024 * 1024) return 0; static_cast<Ext*>(i)->event(Json::parse(j)); return 1; }
    catch(...) { return 0; }
}
int   stop(void*) noexcept { return 1; }  // the core drops our HUD layers; nothing else to restore
void  destroy(void* i) noexcept { delete static_cast<Ext*>(i); }
} // namespace

extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{CSSX_ABI, sizeof(CssxExtension),
        create, tick, model, event, stop, destroy, render};
    return &api;
}

#pragma once
// Retained HUD surface for ABI-2/3 extensions. The core owns every widget; an
// extension holds opaque layer ids. The HUD is resolved through the player
// controller's UI handler property, never by a class search, and only when
// the cached handle is dead.
#include "engine.hpp"
#include "cssx/abi.h"
#include "cssx/hud.h"
#include <functional>
#include <map>

namespace cssx {
class HudService {
    struct Layer {
        engine::WeakObject widget, slot;
        uint8_t kind=0; uintptr_t owner=0;
        float tx=1e30f,ty=1e30f,sx=1e30f,sy=1e30f,px=1e30f,py=1e30f,angle=1e30f,opacity=1e30f;
        int8_t visible=-1;
    };
    std::map<uint64_t,Layer> layers_;
    std::map<std::wstring,engine::WeakObject> textures_;
    engine::WeakObject hud_,tree_,overlay_;
    engine::WeakObject pc_;
    uint64_t next_layer_=1;
    uint32_t generation_=0;
    std::function<void(const std::string&)> log_;
    std::function<uint64_t(engine::UObject*)> minter_;
    bool minimap_warned_=false;
    uint64_t resolve_failures_=0;
    uint64_t hud_check_after_=0,viewport_after_=0;   // steady_clock ms deadlines
    const void* hud_pc_=nullptr;                      // controller the HUD was verified against
    double viewport_w_=0,viewport_h_=0;
    const void* verified_pawn_=nullptr;               // pawn whose direct transform read was cross-checked
    bool direct_transform_=true;                      // off for good if a cross-check ever disagrees
    void drop_scene();
    Layer* find_layer(uint64_t id);
    engine::UObject* parent_canvas(uint64_t parent);
public:
    HudService();
    ~HudService();
    const CssxHudApi* api() const;
    void set_logger(std::function<void(const std::string&)> fn) { log_=std::move(fn); }
    void set_object_minter(std::function<uint64_t(engine::UObject*)> fn) { minter_=std::move(fn); }
    // Per frame while an extension consumes frames. Fills `frame`; `in_menu`
    // is the game/CSSX menu state computed by the core.
    void update(const engine::PlayerContext& player,bool in_menu,CssxFrame& frame);
    void release();
    uint64_t create_layer(uint8_t kind,uint64_t parent,const char* path,size_t len,uintptr_t owner);
    void destroy_layer(uint64_t id);
    uint64_t import_texture(const char* path,size_t len);
    uint64_t layer_object(uint64_t id);
    void set_brush(uint64_t id,uint64_t texture);
    void set_rect(uint64_t id,float x,float y,float w,float h,float ax,float ay,int32_t z);
    void set_translation(uint64_t id,float x,float y);
    void set_scale(uint64_t id,float sx,float sy);
    void set_angle(uint64_t id,float deg);
    void set_pivot(uint64_t id,float px,float py);
    void set_opacity(uint64_t id,float a);
    void set_color(uint64_t id,float r,float g,float b,float a);
    void set_visible(uint64_t id,int32_t visible);
    void set_text(uint64_t id,const char* utf8,size_t len);
    void set_font(uint64_t id,float size);
    void set_clip(uint64_t id,int32_t clip);
    void set_anchor(uint64_t id,float minx,float miny,float maxx,float maxy);
    void minimap_unsupported();
    Json diagnostics() const;
};
}

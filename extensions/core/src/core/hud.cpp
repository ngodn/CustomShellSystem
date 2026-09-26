#include "hud.hpp"
#include <chrono>
#include <cmath>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FString.hpp>

namespace cssx {
using namespace engine;
namespace {
struct FVectorD { double x,y,z; };
struct FRotatorD { double pitch,yaw,roll; };
double normalize_deg(double a) { a=std::fmod(a,360.0); if(a>180.0) a-=360.0; if(a<=-180.0) a+=360.0; return a; }
constexpr uint8_t VIS_COLLAPSED=1, VIS_HITTEST_INVISIBLE=3;
UObject* first_canvas(UObject* widget,int depth) {
    if(!widget || depth>24) return nullptr;
    auto* canvas_class=static_cast<UClass*>(find(L"/Script/UMG.CanvasPanel"));
    if(widget->IsA(canvas_class)) return widget;
    Call count(widget,L"GetChildrenCount",1); count.run();
    const int n=count.get<int>();
    for(int i=0;i<n && i<64;++i) {
        Call at(widget,L"GetChildAt",2); at.set(L"Index",i); at.run();
        if(auto* found=first_canvas(at.get<UObject*>(),depth+1)) return found;
    }
    return nullptr;
}
HudService* g_hud=nullptr;
}
HudService::HudService() { g_hud=this; }
HudService::~HudService() { if(g_hud==this) g_hud=nullptr; }
void HudService::drop_scene() { layers_.clear(); overlay_.Reset(); tree_.Reset(); hud_.Reset(); }
HudService::Layer* HudService::find_layer(uint64_t id) {
    auto it=layers_.find(id); if(it==layers_.end()) return nullptr;
    if(!it->second.widget.Get()) { layers_.erase(it); return nullptr; }
    return &it->second;
}
UObject* HudService::parent_canvas(uint64_t parent) {
    if(parent==0) return overlay_.Get();
    auto* layer=find_layer(parent); return layer?layer->widget.Get():nullptr;
}
void HudService::release() {
    if(hud_.Get() && overlay_.Get()) { try { invoke(overlay_.Get(),L"RemoveFromParent"); } catch(...) {} }
    drop_scene(); textures_.clear();
}
void HudService::update(const PlayerContext& player,bool in_menu,CssxFrame& frame) {
    frame=CssxFrame{}; frame.abi=CSSX_ABI; frame.size=sizeof(CssxFrame); frame.world_generation=generation_;
    try {
        pc_=player.pc;
        // The live HUD comes from the controller's UI handler component: one
        // property read per frame, no object-array search.
        // Re-verify it (two reads plus a reflected IsInViewport) at 4 Hz, or at once when the
        // controller changes or the cached widget dies; in between, the cached widget stands.
        UObject* hud=nullptr;
        const uint64_t now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(player.pc && player.pc==hud_pc_ && now<hud_check_after_) hud=hud_.Get();
        else if(player.pc) {
            hud_check_after_=now+250; hud_pc_=player.pc;
            auto* handler=object_of(player.pc,L"User Interface Handler Component");
            hud=object_of(handler,L"WBP_Player_HUD");
            if(hud) { Call shown(hud,L"IsInViewport",1); shown.run(); if(!shown.get<bool>()) hud=nullptr; }
        } else hud_pc_=nullptr;
        const bool have_scene=overlay_.Get() && hud_.Get() && hud_.Get()==hud;
        if(!have_scene) {
            drop_scene();
            if(hud) {
                UObject* tree=object_of(hud,L"WidgetTree");
                UObject* root=object_of(tree,L"RootWidget");
                UObject* canvas=first_canvas(root,0);
                if(tree && canvas) {
                    UObject* overlay=construct(L"/Script/UMG.CanvasPanel",tree);
                    Call add(canvas,L"AddChildToCanvas",2); add.set(L"content",overlay); add.run();
                    if(auto* slot=add.get<UObject*>()) {
                        invoke(slot,L"SetAnchors",L"InAnchors",Anchors{{0,0},{1,1}});
                        invoke(slot,L"SetZOrder",L"InZOrder",int32_t{9000});
                        invoke(overlay,L"SetVisibility",L"InVisibility",VIS_HITTEST_INVISIBLE);
                        hud_=hud; tree_=tree; overlay_=overlay;
                        ++generation_; frame.world_generation=generation_;
                    }
                } else if(++resolve_failures_%600==1 && log_) log_("HUD overlay: player HUD has no widget tree or canvas yet");
            }
        }
        if(player.pawn) {
            // An unattached root component's relative transform is the actor's world transform,
            // so two reflected field reads replace two ProcessEvent calls per frame. Attached
            // roots (mounts, grabs, cutscene attachments) use the reflected calls. Each new pawn
            // is cross-checked once against the reflected calls; a mismatch disables the shortcut.
            bool direct=false;
            if(direct_transform_) {
                UObject* root=object_of(player.pawn,L"RootComponent");
                if(root && !object_of(root,L"AttachParent")) {
                    const auto p=read<FVectorD>(root,L"RelativeLocation"); const auto r=read<FRotatorD>(root,L"RelativeRotation");
                    frame.player_x=p.x; frame.player_y=p.y; frame.player_z=p.z; frame.player_yaw=normalize_deg(r.yaw);
                    direct=true;
                    if(player.pawn!=verified_pawn_) {
                        verified_pawn_=player.pawn;
                        Call loc(player.pawn,L"K2_GetActorLocation",1); loc.run(); const auto q=loc.get<FVectorD>();
                        Call rot(player.pawn,L"K2_GetActorRotation",1); rot.run();
                        const double dyaw=std::abs(normalize_deg(rot.get<FRotatorD>().yaw)-frame.player_yaw);
                        if(std::abs(q.x-p.x)>1 || std::abs(q.y-p.y)>1 || std::abs(q.z-p.z)>1 || std::min(dyaw,360-dyaw)>.5) {
                            direct_transform_=false; direct=false;
                            if(log_) log_("HUD: direct actor transform disagreed with the reflected call; using reflected calls");
                        }
                    }
                }
            }
            if(!direct) {
                Call loc(player.pawn,L"K2_GetActorLocation",1); loc.run();
                const auto p=loc.get<FVectorD>(); frame.player_x=p.x; frame.player_y=p.y; frame.player_z=p.z;
                Call rot(player.pawn,L"K2_GetActorRotation",1); rot.run();
                frame.player_yaw=normalize_deg(rot.get<FRotatorD>().yaw);
            }
            if(UObject* movement=object_of(player.pawn,L"CharacterMovement")) {
                const auto v=read<std::array<double,3>>(movement,L"Velocity");
                frame.velocity_x=v[0]; frame.velocity_y=v[1]; frame.velocity_z=v[2];
            }
        }
        if(player.pc) {
            if(UObject* manager=object_of(player.pc,L"PlayerCameraManager")) {
                Call cam(manager,L"GetCameraRotation",1); cam.run();
                frame.camera_yaw=normalize_deg(cam.get<FRotatorD>().yaw);
            }
        }
        if(player.world && (now>=viewport_after_ || viewport_w_<=0)) {
            // The viewport only changes on a window resize; 2 Hz is plenty.
            viewport_after_=now+500;
            Call vp(find_cached(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetViewportSize",2);
            vp.set(L"WorldContextObject",player.world); vp.run();
            const auto size=vp.get<Vec2>(); viewport_w_=size.x; viewport_h_=size.y;
        }
        if(player.world) { frame.viewport_w=viewport_w_; frame.viewport_h=viewport_h_; }
        frame.world_ready=(overlay_.Get() && player.pawn)?1:0;
        frame.in_menu=in_menu?1:0;
        if(minter_) { frame.pawn=player.pawn?minter_(player.pawn):0; frame.controller=player.pc?minter_(player.pc):0; }
    } catch(const std::exception& e) { frame.world_ready=0; if(log_) log_(std::string("HUD update threw: ")+e.what()); }
    catch(...) { frame.world_ready=0; if(log_) log_("HUD update threw: unknown"); }
}
uint64_t HudService::create_layer(uint8_t kind,uint64_t parent,const char* path,size_t len,uintptr_t owner) {
    UObject* tree=tree_.Get(); UObject* canvas=parent_canvas(parent);
    if(!tree || !canvas) return 0;
    UObject* widget=nullptr;
    try {
        if(kind==0) widget=construct(L"/Script/UMG.Image",tree);
        else if(kind==1) { widget=construct(L"/Script/UMG.TextBlock",tree); try { invoke(widget,L"SetJustification",L"InJustification",uint8_t{1}); } catch(...) {} }
        else { if(!path || !len) return 0; widget=construct(wide(std::string(path,len)).c_str(),tree); }
    } catch(const std::exception& e) { if(log_) log_(std::string("HUD layer construct failed: ")+e.what()); return 0; }
    if(!widget) return 0;
    UObject* slot=nullptr;
    try { Call add(canvas,L"AddChildToCanvas",2); add.set(L"content",widget); add.run(); slot=add.get<UObject*>(); }
    catch(const std::exception& e) { if(log_) log_(std::string("HUD layer add failed: ")+e.what()); return 0; }
    if(!slot) return 0;
    invoke(slot,L"SetAutoSize",L"InbAutoSize",false);
    invoke(widget,L"SetVisibility",L"InVisibility",VIS_HITTEST_INVISIBLE);
    const uint64_t id=next_layer_++;
    Layer layer; layer.widget=widget; layer.slot=slot; layer.kind=kind; layer.owner=owner;
    layers_.emplace(id,std::move(layer));
    return id;
}
void HudService::destroy_layer(uint64_t id) {
    auto it=layers_.find(id); if(it==layers_.end()) return;
    if(auto* w=it->second.widget.Get()) { try { invoke(w,L"RemoveFromParent"); } catch(...) {} }
    layers_.erase(it);
}
uint64_t HudService::import_texture(const char* path,size_t len) {
    auto* pc=pc_.Get();
    if(!path || !len || !pc) return 0;
    const auto key=wide(std::string(path,len));
    if(auto it=textures_.find(key); it!=textures_.end()) if(it->second.Get()) return reinterpret_cast<uint64_t>(it->second.Get());
    Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
    import.set(L"WorldContextObject",pc); import.set(L"Filename",FString(key.c_str())); import.run();
    UObject* texture=import.get<UObject*>();
    if(!texture) return 0;
    textures_[key]=texture;
    return reinterpret_cast<uint64_t>(texture);
}
uint64_t HudService::layer_object(uint64_t id) {
    auto* layer=find_layer(id); UObject* w=layer?layer->widget.Get():nullptr;
    return (w && minter_)?minter_(w):0;
}
void HudService::set_brush(uint64_t id,uint64_t texture) {
    auto* layer=find_layer(id); if(!layer || layer->kind!=0) return;
    auto* tex=reinterpret_cast<UObject*>(texture);
    if(!tex || !WeakObject(tex).Get()) return;
    Call brush(layer->widget.Get(),L"SetBrushFromTexture",2); brush.set(L"Texture",tex); brush.set(L"bMatchSize",false); brush.run();
}
void HudService::set_rect(uint64_t id,float x,float y,float w,float h,float ax,float ay,int32_t z) {
    auto* layer=find_layer(id); UObject* slot=layer?layer->slot.Get():nullptr; if(!slot) return;
    invoke(slot,L"SetAnchors",L"InAnchors",Anchors{{0,0},{0,0}});
    invoke(slot,L"SetAlignment",L"InAlignment",Vec2{ax,ay});
    invoke(slot,L"SetPosition",L"InPosition",Vec2{x,y});
    invoke(slot,L"SetSize",L"InSize",Vec2{w,h});
    invoke(slot,L"SetZOrder",L"InZOrder",z);
}
void HudService::set_translation(uint64_t id,float x,float y) {
    auto* layer=find_layer(id); if(!layer) return;
    if(std::abs(layer->tx-x)<1e-3f && std::abs(layer->ty-y)<1e-3f) return;
    layer->tx=x; layer->ty=y; invoke(layer->widget.Get(),L"SetRenderTranslation",L"Translation",Vec2{x,y});
}
void HudService::set_scale(uint64_t id,float sx,float sy) {
    auto* layer=find_layer(id); if(!layer) return;
    if(std::abs(layer->sx-sx)<1e-4f && std::abs(layer->sy-sy)<1e-4f) return;
    layer->sx=sx; layer->sy=sy; invoke(layer->widget.Get(),L"SetRenderScale",L"Scale",Vec2{sx,sy});
}
void HudService::set_angle(uint64_t id,float deg) {
    auto* layer=find_layer(id); if(!layer) return;
    if(std::abs(layer->angle-deg)<0.05f) return;
    layer->angle=deg; invoke(layer->widget.Get(),L"SetRenderTransformAngle",L"Angle",deg);
}
void HudService::set_pivot(uint64_t id,float px,float py) {
    auto* layer=find_layer(id); if(!layer) return;
    if(std::abs(layer->px-px)<1e-3f && std::abs(layer->py-py)<1e-3f) return;
    layer->px=px; layer->py=py; invoke(layer->widget.Get(),L"SetRenderTransformPivot",L"Pivot",Vec2{px,py});
}
void HudService::set_opacity(uint64_t id,float a) {
    auto* layer=find_layer(id); if(!layer) return;
    if(std::abs(layer->opacity-a)<1e-3f) return;
    layer->opacity=a; invoke(layer->widget.Get(),L"SetRenderOpacity",L"InOpacity",a);
}
void HudService::set_color(uint64_t id,float r,float g,float b,float a) {
    auto* layer=find_layer(id); UObject* w=layer?layer->widget.Get():nullptr; if(!w) return;
    if(layer->kind==1) invoke(w,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{{r,g,b,a}});
    else invoke(w,L"SetColorAndOpacity",L"InColorAndOpacity",Color{r,g,b,a});
}
void HudService::set_visible(uint64_t id,int32_t visible) {
    auto* layer=find_layer(id); if(!layer) return;
    const int8_t want=visible?1:0; if(layer->visible==want) return;
    layer->visible=want; invoke(layer->widget.Get(),L"SetVisibility",L"InVisibility",uint8_t(visible?VIS_HITTEST_INVISIBLE:VIS_COLLAPSED));
}
void HudService::set_text(uint64_t id,const char* utf8,size_t len) {
    auto* layer=find_layer(id); UObject* w=layer?layer->widget.Get():nullptr; if(!w || layer->kind!=1) return;
    text_value(w,std::string(utf8?utf8:"",len));
}
void HudService::set_font(uint64_t id,float size) {
    auto* layer=find_layer(id); UObject* w=layer?layer->widget.Get():nullptr; if(!w || layer->kind!=1) return;
    font_size(w,size);
}
void HudService::set_clip(uint64_t id,int32_t clip) { auto* layer=find_layer(id); if(!layer) return; invoke(layer->widget.Get(),L"SetClipping",L"InClipping",uint8_t(clip?1:0)); }
void HudService::set_anchor(uint64_t id,float minx,float miny,float maxx,float maxy) {
    auto* layer=find_layer(id); UObject* slot=layer?layer->slot.Get():nullptr; if(!slot) return;
    invoke(slot,L"SetAnchors",L"InAnchors",Anchors{{minx,miny},{maxx,maxy}});
}
void HudService::minimap_unsupported() { if(!minimap_warned_ && log_) { minimap_warned_=true; log_("hud.minimap_update is not available in standalone CSSX; the call is ignored"); } }
Json HudService::diagnostics() const { return {{"layers",layers_.size()},{"textures",textures_.size()},{"generation",generation_},{"overlay",overlay_.Get()!=nullptr}}; }
namespace {
template<class F> auto guard(F&& fn) noexcept -> decltype(fn()) { using R=decltype(fn()); if(!g_hud) return R{}; try { return fn(); } catch(...) { return R{}; } }
CssxLayer hud_image(void* ctx,CssxLayer p) noexcept { return guard([&]{ return g_hud->create_layer(0,p,nullptr,0,reinterpret_cast<uintptr_t>(ctx)); }); }
CssxLayer hud_text(void* ctx,CssxLayer p) noexcept { return guard([&]{ return g_hud->create_layer(1,p,nullptr,0,reinterpret_cast<uintptr_t>(ctx)); }); }
CssxLayer hud_widget(void* ctx,CssxLayer p,const char* path,size_t len) noexcept { return guard([&]{ return g_hud->create_layer(2,p,path,len,reinterpret_cast<uintptr_t>(ctx)); }); }
void hud_destroy(void*,CssxLayer id) noexcept { guard([&]{ g_hud->destroy_layer(id); return 0; }); }
uint64_t hud_texture(void*,const char* path,size_t len) noexcept { return guard([&]{ return g_hud->import_texture(path,len); }); }
uint64_t hud_layer_object(void*,CssxLayer id) noexcept { return guard([&]{ return g_hud->layer_object(id); }); }
void hud_set_brush(void*,CssxLayer id,uint64_t t) noexcept { guard([&]{ g_hud->set_brush(id,t); return 0; }); }
void hud_set_rect(void*,CssxLayer id,float x,float y,float w,float h,float ax,float ay,int32_t z) noexcept { guard([&]{ g_hud->set_rect(id,x,y,w,h,ax,ay,z); return 0; }); }
void hud_set_translation(void*,CssxLayer id,float x,float y) noexcept { guard([&]{ g_hud->set_translation(id,x,y); return 0; }); }
void hud_set_scale(void*,CssxLayer id,float sx,float sy) noexcept { guard([&]{ g_hud->set_scale(id,sx,sy); return 0; }); }
void hud_set_angle(void*,CssxLayer id,float d) noexcept { guard([&]{ g_hud->set_angle(id,d); return 0; }); }
void hud_set_pivot(void*,CssxLayer id,float px,float py) noexcept { guard([&]{ g_hud->set_pivot(id,px,py); return 0; }); }
void hud_set_opacity(void*,CssxLayer id,float a) noexcept { guard([&]{ g_hud->set_opacity(id,a); return 0; }); }
void hud_set_color(void*,CssxLayer id,float r,float g,float b,float a) noexcept { guard([&]{ g_hud->set_color(id,r,g,b,a); return 0; }); }
void hud_set_visible(void*,CssxLayer id,int32_t v) noexcept { guard([&]{ g_hud->set_visible(id,v); return 0; }); }
void hud_set_text(void*,CssxLayer id,const char* t,size_t len) noexcept { guard([&]{ g_hud->set_text(id,t,len); return 0; }); }
void hud_set_font(void*,CssxLayer id,float s) noexcept { guard([&]{ g_hud->set_font(id,s); return 0; }); }
void hud_set_clip(void*,CssxLayer id,int32_t c) noexcept { guard([&]{ g_hud->set_clip(id,c); return 0; }); }
void hud_set_anchor(void*,CssxLayer id,float a,float b,float c,float d) noexcept { guard([&]{ g_hud->set_anchor(id,a,b,c,d); return 0; }); }
void hud_minimap_update(void*,int32_t,float,float,float,float,double,double,double,uint32_t) noexcept { guard([&]{ g_hud->minimap_unsupported(); return 0; }); }
const CssxHudApi g_vtable{CSSX_ABI,sizeof(CssxHudApi),hud_image,hud_text,hud_widget,hud_destroy,hud_texture,hud_layer_object,
    hud_set_brush,hud_set_rect,hud_set_translation,hud_set_scale,hud_set_angle,hud_set_pivot,hud_set_opacity,hud_set_color,hud_set_visible,
    hud_set_text,hud_set_font,hud_set_clip,hud_set_anchor,hud_minimap_update};
}
const CssxHudApi* HudService::api() const { return &g_vtable; }
}

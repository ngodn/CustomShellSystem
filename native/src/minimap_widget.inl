// Native minimap, ported from Cartographer's Map/NativeWidget.lua Build(). Lives
// in css_core because the SpartaMapWidget must be configured inline (SetTileSet /
// SetPanCenterNormalized on the live object) — routing it through the extension's
// $object handle bridge makes the handle "expire". The extension drives it via
// the hud.minimap.* request ops: build(config) on world change, update(...) per
// frame. Included in engine.cpp after extension_hud.inl (uses Vec2/Color/Call/
// construct/invoke/find/load/read/write_field/object_property/WeakObject).
namespace css {
namespace {
using namespace RC::Unreal;

namespace mm {
constexpr const wchar_t* TILESET   = L"/Game/Sparta/UI/World/Map/Blueprints/DT_MapTiles.DT_MapTiles";
constexpr const wchar_t* TILEMAT   = L"/Game/Sparta/UI/World/Map/Materials/MI_SpartaMapTile.MI_SpartaMapTile";
constexpr const wchar_t* MASK      = L"/Game/Sparta/UI/World/Map/Textures/T_UI_MapGlobalMask.T_UI_MapGlobalMask";
constexpr const wchar_t* CIRCLE    = L"/Game/Sparta/UI/Common/Materials/Master/Mat_UI_Circle_Background_Inst.Mat_UI_Circle_Background_Inst";
constexpr const wchar_t* ARROW_TEX = L"/Game/Sparta/UI/World/Map/Textures/T_UI_Icon_Map_PlayerIndicator.T_UI_Icon_Map_PlayerIndicator";
constexpr const wchar_t* C_USERWIDGET = L"/Script/UMG.UserWidget";
constexpr const wchar_t* C_WIDGETTREE = L"/Script/UMG.WidgetTree";
constexpr const wchar_t* C_CANVAS     = L"/Script/UMG.CanvasPanel";
constexpr const wchar_t* C_IMAGE      = L"/Script/UMG.Image";
constexpr const wchar_t* C_RETAINER   = L"/Script/UMG.RetainerBox";
constexpr const wchar_t* C_MAPWIDGET  = L"/Script/Sparta.SpartaMapWidget";
constexpr const wchar_t* WIDGETLIB    = L"/Script/UMG.Default__WidgetBlueprintLibrary";
constexpr double SPAN = 315552.0, SPAN_M = 3155.52, BASE_ZOOM = 0.25;
// VIS_COLLAPSED / VIS_HITTEST_INVISIBLE come from extension_hud.inl (same TU).

UObject* asset(const wchar_t* path) {
    if(auto* o = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path)) return o;
    return load(narrow(path));
}
// One-arg void call, set positionally (parameter names unknown for native setters).
template<class T> void call1(UObject* o, const wchar_t* fn, const T& a) { Call c(o, fn, 1); c.set_arg(0, a); c.run(); }
// content -> panel, returning the new slot (AddChild / AddChildToCanvas: 1 arg + return).
UObject* add_child(UObject* panel, const wchar_t* fn, UObject* content) { Call c(panel, fn, 2); c.set_arg(0, content); c.run(); return c.get<UObject*>(); }
} // namespace mm

class MinimapWidget {
    WeakObject widget_, tree_, container_, frame_, retainer_, backdrop_, map_widget_, arrow_;
    bool built_ = false, circle_ = true;
    double size_ = 300;
    std::function<void(const std::string&)> log_;
    double l_panx_ = 1e9, l_pany_ = 1e9, l_zoom_ = -1.0;
    float l_mapang_ = 1e9f, l_arrang_ = 1e9f, l_scale_ = -1.f, l_opac_ = -2.f;
    int l_vis_ = -1;
    void note(const std::string& m) { if(log_) log_("minimap " + m); }
public:
    void set_logger(std::function<void(const std::string&)> fn) { log_ = std::move(fn); }
    bool ready() const { return built_ && const_cast<WeakObject&>(widget_).Get() != nullptr; }

    void destroy() {
        if(auto* w = widget_.Get()) { try { invoke(w, L"RemoveFromParent"); } catch(...) {} }
        widget_.Reset(); tree_.Reset(); container_.Reset(); frame_.Reset(); retainer_.Reset();
        backdrop_.Reset(); map_widget_.Reset(); arrow_.Reset();
        built_ = false; l_panx_ = l_pany_ = 1e9; l_zoom_ = -1.0; l_mapang_ = l_arrang_ = 1e9f; l_scale_ = -1.f; l_opac_ = -2.f; l_vis_ = -1;
    }

    // config: {size, offx, offy, zoom_m, orientation(0/1/2), shape(0 square,1 circle)}
    bool build(void* engine, const Json& config) {
        if(ready()) return true;
        destroy();
        using namespace mm;
        try {
            size_ = std::clamp(config.value("size", 200.0), 80.0, 800.0);
            circle_ = config.value("shape", 1) != 0;
            const double offx = config.value("offx", 28.0), offy = config.value("offy", 28.0);
            const double zoom_m = std::max(1.0, config.value("zoom_m", 1000.0));
            const int orientation = config.value("orientation", 1);
            const bool rotating = orientation != 0;

            UObject* controller = nullptr;
            if(auto* vp = read<UObject*>(static_cast<UObject*>(engine), L"GameViewport"))
                if(auto* world = read<UObject*>(vp, L"World")) {
                    Call pc(find(L"/Script/Engine.Default__GameplayStatics"), L"GetPlayerController", 3);
                    pc.set_arg(0, world); pc.set_arg(1, int32_t{0}); pc.run(); controller = pc.get<UObject*>();
                }
            if(!controller) { note("no controller"); return false; }

            UObject* tile_set = asset(TILESET); UObject* tile_mat = asset(TILEMAT);
            UObject* mask = asset(MASK); UObject* arrow_tex = asset(ARROW_TEX);
            UObject* circle_mat = circle_ ? asset(CIRCLE) : nullptr;
            if(!tile_set || !tile_mat || !mask || !arrow_tex || (circle_ && !circle_mat)) { note("assets unavailable"); return false; }

            UObject* uw_class = find(C_USERWIDGET);
            Call create(find(WIDGETLIB), L"Create", 4);
            create.set_arg(0, controller); create.set_arg(1, uw_class); create.set_arg(2, controller); create.run();
            UObject* widget = create.get<UObject*>();
            if(!widget) { note("Create returned null"); return false; }

            UObject* tree = read<UObject*>(widget, L"WidgetTree");
            if(!tree) { tree = construct(C_WIDGETTREE, widget); object_property(widget, L"WidgetTree", tree); }
            UObject* root = construct(C_CANVAS, tree);
            object_property(tree, L"RootWidget", root);

            // Container canvas panel on root for positioning, scaling, and opacity.
            // Avoid nested RetainerBoxes: dynamic SetRetainRendering or nested render
            // targets cause D3D12/Vulkan driver crashes (DXGI_ERROR_DEVICE_REMOVED).
            UObject* container = construct(C_CANVAS, root);
            if(UObject* cslot = add_child(root, L"AddChildToCanvas", container)) {
                call1(cslot, L"SetPosition", Vec2{offx, offy}); call1(cslot, L"SetSize", Vec2{size_, size_});
            }
            call1(container, L"SetRenderTransformPivot", Vec2{0.5, 0.5});
            container_ = container;

            UObject* frame_parent = container;
            if(circle_) {
                UObject* ret = construct(C_RETAINER, container);
                bool eff = false;
                try { call1(ret, L"SetEffectMaterial", circle_mat); eff = true; } catch(...) {}
                if(!eff) { try { object_property(ret, L"EffectMaterial", circle_mat); eff = true; } catch(...) {} }
                try { write_field<FName>(ret, L"TextureParameter", FName(L"Texture", FNAME_Add)); } catch(...) {}
                if(UObject* rslot = add_child(container, L"AddChildToCanvas", ret)) {
                    call1(rslot, L"SetPosition", Vec2{0, 0}); call1(rslot, L"SetSize", Vec2{size_, size_});
                }
                retainer_ = ret; frame_parent = ret;
                try { write_field<int32_t>(ret, L"Phase", int32_t{0}); } catch(...) {}
                try { write_field<int32_t>(ret, L"PhaseCount", int32_t{3}); } catch(...) {}
                try { write_field<bool>(ret, L"RenderOnPhase", true); } catch(...) {}
                try { Call ph(ret, L"SetRenderingPhase", 2); ph.set_arg(0, int32_t{0}); ph.set_arg(1, int32_t{3}); ph.run(); } catch(...) {}
            }

            UObject* frame = construct(C_CANVAS, frame_parent);
            try { write_field<uint8_t>(frame, L"Clipping", 1); } catch(...) {}
            frame_ = frame;
            if(retainer_.Get()) {
                add_child(retainer_.Get(), L"AddChild", frame);
            } else {
                if(UObject* fslot = add_child(container, L"AddChildToCanvas", frame)) {
                    call1(fslot, L"SetPosition", Vec2{0, 0}); call1(fslot, L"SetSize", Vec2{size_, size_});
                }
            }
            call1(frame, L"SetRenderTransformPivot", Vec2{0.5, 0.5});

            // backdrop (centered, full size)
            UObject* backdrop = construct(C_IMAGE, frame);
            try { call1(backdrop, L"SetColorAndOpacity", Color{0.015f, 0.015f, 0.015f, 0.80f}); } catch(...) {}
            if(UObject* bslot = add_child(frame, L"AddChildToCanvas", backdrop)) {
                call1(bslot, L"SetPosition", Vec2{0, 0}); call1(bslot, L"SetSize", Vec2{size_, size_}); call1(bslot, L"SetZOrder", int32_t{0});
            }
            backdrop_ = backdrop;

            // native tile widget: inset by 2px; oversize sqrt2 when a square viewport rotates.
            const double inner = size_ - 4.0;
            const double surf = (rotating && !circle_) ? std::ceil(inner * 1.41421356) : inner;
            UObject* map = construct(C_MAPWIDGET, frame);
            if(UObject* mslot = add_child(frame, L"AddChildToCanvas", map)) {
                call1(mslot, L"SetPosition", Vec2{(size_ - surf) / 2.0, (size_ - surf) / 2.0});
                call1(mslot, L"SetSize", Vec2{surf, surf}); call1(mslot, L"SetZOrder", int32_t{2});
            }
            call1(map, L"SetRenderTransformPivot", Vec2{0.5, 0.5});
            map_widget_ = map;

            // player arrow (centered, pivot 0.5/0.575 like the native WBP_WMI_Player)
            UObject* arrow = construct(C_IMAGE, frame);
            { Call br(arrow, L"SetBrushFromTexture", 2); br.set_arg(0, arrow_tex); br.set_arg(1, false); br.run(); }
            const double asz = std::max(20.0, std::floor(size_ * 0.095 + 0.5));
            try { call1(arrow, L"SetDesiredSizeOverride", Vec2{asz, asz}); } catch(...) {}
            if(UObject* aslot = add_child(frame, L"AddChildToCanvas", arrow)) {
                call1(aslot, L"SetPosition", Vec2{size_ / 2.0 - asz * 0.5, size_ / 2.0 - asz * 0.575});
                call1(aslot, L"SetSize", Vec2{asz, asz}); call1(aslot, L"SetZOrder", int32_t{10});
            }
            call1(arrow, L"SetRenderTransformPivot", Vec2{0.5, 0.575});
            arrow_ = arrow;

            call1(widget, L"SetVisibility", uint8_t(VIS_COLLAPSED));
            call1(widget, L"AddToViewport", int32_t{40});

            // configure the tile widget inline (this is what the handle bridge could not do)
            write_field<int32_t>(map, L"MaxResidentTiles", int32_t{8});
            write_field<int32_t>(map, L"LODBias", int32_t{2});
            try { write_field<bool>(map, L"bAllowMouseInput", false); } catch(...) {}
            write_field<bool>(map, L"bExternalZoomAndPan", true);
            object_property(map, L"TileMaterial", tile_mat);
            call1(map, L"SetTileSet", tile_set);
            call1(map, L"SetGlobalAlphaMask", mask);
            { Call rb(map, L"SetRegionBounds", 2); rb.set_arg(0, Vec2{0.5, 0.5}); rb.set_arg(1, Vec2{1.0, 1.0}); rb.run(); }
            call1(map, L"SetRevealAmount", 1.0f);
            call1(map, L"SetZoom", float(BASE_ZOOM * SPAN_M / zoom_m));
            call1(map, L"SetPanCenterNormalized", Vec2{0.5, 0.5});

            widget_ = widget; tree_ = tree;
            built_ = true;
            note("built size=" + std::to_string(int(size_)) + " circle=" + std::to_string(circle_));
            return true;
        } catch(const std::exception& e) { note(std::string("build threw: ") + e.what()); destroy(); return false; }
    }

    // Direct update: bypasses JSON serialization on hot per-frame paths.
    // flags: bit 0: visible, bit 1: scale, bit 2: opacity, bit 3: arrow_angle, bit 4: map_angle, bit 5: pan, bit 6: zoom
    void update_direct(int vis, float sc, float op, float aa, float ma, double px, double py, double zm, uint32_t flags) {
        if(!ready()) return;
        using namespace mm;
        try {
            UObject* widget = widget_.Get();
            if(!widget) return;
            if((flags & 0x01) && vis != l_vis_) {
                l_vis_ = vis;
                call1(widget, L"SetVisibility", uint8_t(vis ? VIS_HITTEST_INVISIBLE : VIS_COLLAPSED));
            }
            if(!l_vis_) return;
            if(UObject* map = map_widget_.Get()) {
                // Spatial pan thresholding: only invalidate the tile retain cache when moved > ~6 cm
                if((flags & 0x20) && (std::abs(px - l_panx_) > 2e-5 || std::abs(py - l_pany_) > 2e-5)) {
                    l_panx_ = px; l_pany_ = py;
                    call1(map, L"SetPanCenterNormalized", Vec2{px, py});
                }
                if((flags & 0x10) && std::abs(ma - l_mapang_) > 0.1f) {
                    l_mapang_ = ma;
                    call1(map, L"SetRenderTransformAngle", ma);
                }
                if((flags & 0x40) && zm > 0.0 && std::abs(zm - l_zoom_) > 0.5) {
                    l_zoom_ = zm;
                    call1(map, L"SetZoom", float(BASE_ZOOM * SPAN_M / zm));
                }
            }
            if(UObject* arrow = arrow_.Get()) {
                if((flags & 0x08) && std::abs(aa - l_arrang_) > 0.1f) {
                    l_arrang_ = aa;
                    call1(arrow, L"SetRenderTransformAngle", aa);
                }
            }
            if(UObject* container = container_.Get()) {
                if((flags & 0x02) && std::abs(sc - l_scale_) > 1e-3f) {
                    l_scale_ = sc;
                    call1(container, L"SetRenderScale", Vec2{double(sc), double(sc)});
                }
                if((flags & 0x04) && std::abs(op - l_opac_) > 1e-3f) {
                    l_opac_ = op;
                    const float clamped = std::clamp(op, 0.0f, 1.0f);
                    // For circle retainer, Slate pre-multiplies alpha into the brush tint,
                    // squaring effective opacity. Sqrt (power 0.5) linearizes it cleanly
                    // on the container CanvasPanel without any secondary RetainerBox.
                    const float applied = circle_ ? std::sqrt(clamped) : clamped;
                    call1(container, L"SetRenderOpacity", applied);
                }
            }
        } catch(...) {}
    }

    // JSON update thunk (for host requests / Python probes)
    void update(const Json& u) {
        uint32_t flags = 0;
        int vis = l_vis_;
        if(u.contains("visible")) { vis = u["visible"].get<int>(); flags |= 0x01; }
        float sc = l_scale_;
        if(u.contains("scale")) { sc = u["scale"].get<float>(); flags |= 0x02; }
        float op = l_opac_;
        if(u.contains("opacity")) { op = u["opacity"].get<float>(); flags |= 0x04; }
        float aa = l_arrang_;
        if(u.contains("arrow_angle")) { aa = u["arrow_angle"].get<float>(); flags |= 0x08; }
        float ma = l_mapang_;
        if(u.contains("map_angle")) { ma = u["map_angle"].get<float>(); flags |= 0x10; }
        double px = l_panx_, py = l_pany_;
        if(u.contains("pan_x") && u.contains("pan_y")) {
            px = u["pan_x"].get<double>(); py = u["pan_y"].get<double>(); flags |= 0x20;
        }
        double zm = l_zoom_;
        if(u.contains("zoom_m")) { zm = u["zoom_m"].get<double>(); flags |= 0x40; }
        update_direct(vis, sc, op, aa, ma, px, py, zm, flags);
    }
};
MinimapWidget g_minimap;
} // namespace
bool minimap_build(void* engine, const Json& config) { return g_minimap.build(engine, config); }
void minimap_update(const Json& u) { g_minimap.update(u); }
void minimap_update_direct(int vis, float sc, float op, float aa, float ma, double px, double py, double zm, uint32_t flags) {
    g_minimap.update_direct(vis, sc, op, aa, ma, px, py, zm, flags);
}
void minimap_destroy() { g_minimap.destroy(); }
void minimap_set_logger(std::function<void(const std::string&)> fn) { g_minimap.set_logger(std::move(fn)); }
} // namespace css

// CSSX ABI 2 HUD backend. Included inside engine.cpp after the UMG helpers
// (construct/invoke/Call/read/find/text_value/font_size, structs Vec2/Color/
// SlateColor) so it can build widgets directly. The core owns every widget; an
// extension only holds opaque layer ids and pushes per-frame updates through the
// CssxHudApi vtable. Never touch an old world's widgets: on teardown we forget
// them by handle (WeakObject) and rebuild, exactly as the reference Lua mods do.
namespace css {
namespace {
using namespace RC::Unreal;

struct FVectorD { double x, y, z; };
struct FRotatorD { double pitch, yaw, roll; };
struct FAnchorsD { Vec2 minimum, maximum; };

double normalize_deg(double a) {
    a = std::fmod(a, 360.0);
    if(a > 180.0) a -= 360.0;
    if(a <= -180.0) a += 360.0;
    return a;
}

// ESlateVisibility / EWidgetClipping enum values used below.
constexpr uint8_t VIS_COLLAPSED = 1, VIS_HITTEST_INVISIBLE = 3;

bool object_is(UObject* object, const wchar_t* class_path) {
    if(!object) return false;
    auto* cls = static_cast<UClass*>(find(class_path));
    return cls && object->IsA(cls);
}

// Depth-first search for the first CanvasPanel at or under `widget`.
UObject* first_canvas(UObject* widget, int depth) {
    if(!widget || depth > 24) return nullptr;
    if(object_is(widget, L"/Script/UMG.CanvasPanel")) return widget;
    Call count(widget, L"GetChildrenCount", 1); count.run();
    const int n = count.get<int>();
    for(int i = 0; i < n; ++i) {
        Call at(widget, L"GetChildAt", 2); at.set(L"Index", i); at.run();
        if(auto* found = first_canvas(at.get<UObject*>(), depth + 1)) return found;
    }
    return nullptr;
}

// The player HUD, only when it is actually on screen (skips the class-default object).
UObject* resolve_live_hud() {
    auto* hud = UObjectGlobals::FindFirstOf(L"WBP_Player_HUD_C");
    if(!hud) return nullptr;
    Call shown(hud, L"IsInViewport", 1); shown.run();
    return shown.get<bool>() ? hud : nullptr;
}
} // namespace

HudService* g_hud_service = nullptr;

HudService::HudService() { g_hud_service = this; }
HudService::~HudService() { if(g_hud_service == this) g_hud_service = nullptr; }

void HudService::drop_scene() {
    // Forget every world-owned widget by handle. Do not RemoveFromParent: the old
    // world may already be torn down and touching a freed widget faults natively.
    layers_.clear();
    overlay_.Reset(); tree_.Reset(); hud_.Reset();
}

HudService::Layer* HudService::find_layer(uint64_t id) {
    auto it = layers_.find(id);
    if(it == layers_.end()) return nullptr;
    if(!it->second.widget.Get()) { layers_.erase(it); return nullptr; }
    return &it->second;
}

UObject* HudService::parent_canvas(uint64_t parent) {
    if(parent == 0) return overlay_.Get();
    auto* layer = find_layer(parent);
    return layer ? layer->widget.Get() : nullptr;
}

void HudService::release() {
    // release() runs on a core hot-reload, where the game HUD is still alive: our overlay
    // (and the extension widgets inside it) would otherwise linger and stack a ghost every
    // reload. Remove it while the HUD is valid. During a real world teardown the HUD is
    // already gone (WeakObject null) and drop_scene handles it without touching freed
    // widgets. RemoveFromParent on the overlay takes its whole subtree with it.
    if(hud_.Get() && overlay_.Get()) { try { invoke(overlay_.Get(), L"RemoveFromParent"); } catch(...) {} }
    drop_scene(); textures_.clear();
}

void HudService::update(void* engine, CssxFrame& frame) {
    frame = CssxFrame{};
    frame.abi = CSSX_ABI;
    frame.size = sizeof(CssxFrame);
    frame.world_generation = generation_;
    const bool should_log = log_ && (++log_tick_ >= 120);
    if(should_log) log_tick_ = 0;
    std::string dbg;
    try {
        // Resolve the player the same way CSS does every frame (Appearance::player):
        // GameViewport -> World -> GetPlayerCharacter(0). FindFirstOf was unreliable
        // (could hand back a class-default object) and left world_ready stuck at 0.
        UObject* pawn = nullptr; UObject* pc = nullptr; UObject* world = nullptr;
        if(engine) {
            if(UObject* vp = read<UObject*>(static_cast<UObject*>(engine), L"GameViewport"))
                world = read<UObject*>(vp, L"World");
        }
        if(world) {
            Call gp(find(L"/Script/Engine.Default__GameplayStatics"), L"GetPlayerCharacter", 3);
            gp.set(L"WorldContextObject", world); gp.set(L"PlayerIndex", int32_t{0}); gp.run();
            pawn = gp.get<UObject*>();
            if(pawn && WeakObject(pawn).Get() != pawn) pawn = nullptr;
        }
        if(pawn) pc = read<UObject*>(pawn, L"Controller");
        pc_ = pc;
        UObject* hud = resolve_live_hud();
        if(should_log) {
            dbg = "pawn=" + std::to_string(pawn != nullptr) + " pc=" + std::to_string(pc != nullptr)
                + " hud=" + std::to_string(hud != nullptr);
        }

        // Teardown / (re)build detection by identity. A new HUD (or a lost overlay)
        // means a fresh scene: forget the old handles and build a new overlay.
        const bool have_scene = overlay_.Get() && hud_.Get() && hud_.Get() == hud;
        if(!have_scene) {
            drop_scene();
            if(hud) {
                UObject* tree = read<UObject*>(hud, L"WidgetTree");
                UObject* root = tree ? read<UObject*>(tree, L"RootWidget") : nullptr;
                UObject* canvas = first_canvas(root, 0);
                if(should_log) dbg += " tree=" + std::to_string(tree != nullptr) + " canvas=" + std::to_string(canvas != nullptr);
                if(tree && canvas) {
                    UObject* overlay = construct(L"/Script/UMG.CanvasPanel", tree);
                    Call add(canvas, L"AddChildToCanvas", 2); add.set(L"content", overlay); add.run();
                    if(auto* slot = add.get<UObject*>()) {
                        invoke(slot, L"SetAnchors", L"InAnchors", FAnchorsD{{0,0},{1,1}});
                        invoke(slot, L"SetZOrder", L"InZOrder", int32_t{9000});
                        invoke(overlay, L"SetVisibility", L"InVisibility", VIS_HITTEST_INVISIBLE);
                        hud_ = hud; tree_ = tree; overlay_ = overlay;
                        ++generation_;
                        frame.world_generation = generation_;
                        if(should_log) dbg += " built-overlay";
                    }
                }
            }
        }

        if(pawn) {
            Call loc(pawn, L"K2_GetActorLocation", 1); loc.run();
            const auto p = loc.get<FVectorD>();
            frame.player_x = p.x; frame.player_y = p.y; frame.player_z = p.z;
            Call rot(pawn, L"K2_GetActorRotation", 1); rot.run();
            frame.player_yaw = normalize_deg(rot.get<FRotatorD>().yaw);
            if(UObject* movement = read<UObject*>(pawn, L"CharacterMovement")) {
                const auto v = read<std::array<double,3>>(movement, L"Velocity");
                frame.velocity_x = v[0]; frame.velocity_y = v[1]; frame.velocity_z = v[2];
            }
        }
        if(pc) {
            if(UObject* manager = read<UObject*>(pc, L"PlayerCameraManager")) {
                Call cam(manager, L"GetCameraRotation", 1); cam.run();
                frame.camera_yaw = normalize_deg(cam.get<FRotatorD>().yaw);
            }
        }
        if(world) {
            Call vp(find(L"/Script/UMG.Default__WidgetLayoutLibrary"), L"GetViewportSize", 2);
            vp.set(L"WorldContextObject", world); vp.run();
            const auto size = vp.get<Vec2>();
            frame.viewport_w = size.x; frame.viewport_h = size.y;
        }
        frame.world_ready = (overlay_.Get() && pawn) ? 1 : 0;
        // A menu (inventory, map, settings, pause) is open when the game's UI Handler
        // says so. bIsInGameMenu / ActiveMenu / ActiveDisplayMenu are set by the game
        // logic itself, so they are reliable for BOTH controller and mouse (the old
        // HUD-visibility and mouse-cursor checks missed controller-driven menus, which
        // never show the cursor and don't always collapse the HUD). The cursor stays as
        // a last-resort hint.
        frame.in_menu = 0;
        if(pc) {
            try {
                if(UObject* handler = read<UObject*>(pc, L"User Interface Handler Component")) {
                    if(read<bool>(handler, L"bIsInGameMenu")
                       || read<UObject*>(handler, L"ActiveMenu")
                       || read<UObject*>(handler, L"ActiveDisplayMenu")) frame.in_menu = 1;
                }
            } catch(...) {}
            if(!frame.in_menu) { try { if(read<bool>(pc, L"bShowMouseCursor")) frame.in_menu = 1; } catch(...) {} }
        }
        if(should_log) {
            dbg += " overlay=" + std::to_string(overlay_.Get() != nullptr)
                + " vpw=" + std::to_string(int(frame.viewport_w))
                + " ready=" + std::to_string(frame.world_ready);
            log_("HUD " + dbg);
        }
    } catch(const std::exception& e) {
        frame.world_ready = 0; if(log_) log_(std::string("HUD threw: ") + e.what());
    } catch(...) {
        frame.world_ready = 0; if(log_) log_("HUD threw: unknown");
    }
}

uint64_t HudService::create_layer(uint8_t kind, uint64_t parent, const char* path, size_t len, uintptr_t owner) {
    UObject* tree = tree_.Get();
    UObject* canvas = parent_canvas(parent);
    if(!tree || !canvas) { if(log_) log_("create_layer fail tree=" + std::to_string(tree!=nullptr) + " canvas=" + std::to_string(canvas!=nullptr) + " parent=" + std::to_string(parent)); return 0; }
    UObject* widget = nullptr;
    try {
        if(kind == 0) widget = construct(L"/Script/UMG.Image", tree);
        else if(kind == 1) {
            widget = construct(L"/Script/UMG.TextBlock", tree);
            // Center the text within its box so a box centered on a bearing places the
            // text on that bearing (UMG TextBlocks default to left-justified, which left
            // a marker's distance label offset from its icon).
            if(widget) { try { invoke(widget, L"SetJustification", L"InJustification", uint8_t{1}); } catch(...) {} }
        }
        else {
            if(!path || !len) return 0;
            widget = construct(wide(std::string(path, len)).c_str(), tree);
        }
    } catch(const std::exception& e) { if(log_) log_(std::string("create_layer construct threw: ") + e.what()); return 0; }
    if(!widget) { if(log_) log_("create_layer construct null kind=" + std::to_string(kind)); return 0; }
    UObject* slot = nullptr;
    try { Call add(canvas, L"AddChildToCanvas", 2); add.set(L"content", widget); add.run(); slot = add.get<UObject*>(); }
    catch(const std::exception& e) { if(log_) log_(std::string("create_layer add threw: ") + e.what()); return 0; }
    if(!slot) { if(log_) log_("create_layer no slot kind=" + std::to_string(kind)); return 0; }
    invoke(slot, L"SetAutoSize", L"InbAutoSize", false);
    invoke(widget, L"SetVisibility", L"InVisibility", VIS_HITTEST_INVISIBLE);
    const uint64_t id = next_layer_++;
    Layer layer; layer.widget = widget; layer.slot = slot; layer.kind = kind; layer.owner = owner;
    layers_.emplace(id, std::move(layer));
    return id;
}

void HudService::destroy_layer(uint64_t id) {
    auto it = layers_.find(id);
    if(it == layers_.end()) return;
    if(auto* w = it->second.widget.Get()) { try { invoke(w, L"RemoveFromParent"); } catch(...) {} }
    layers_.erase(it);
}

uint64_t HudService::import_texture(const char* path, size_t len) {
    if(!path || !len || !pc_) return 0;
    const auto key = wide(std::string(path, len));
    if(auto it = textures_.find(key); it != textures_.end()) if(it->second.Get()) return reinterpret_cast<uint64_t>(it->second.Get());
    Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"), L"ImportFileAsTexture2D", 3);
    import.set(L"WorldContextObject", static_cast<UObject*>(pc_));
    import.set(L"Filename", FString(key.c_str()));
    import.run();
    UObject* texture = import.get<UObject*>();
    if(!texture) return 0;
    textures_[key] = texture;
    return reinterpret_cast<uint64_t>(texture);
}

uint64_t HudService::layer_object(uint64_t id) {
    auto* layer = find_layer(id);
    UObject* w = layer ? layer->widget.Get() : nullptr;
    if(!w || !minter_) return 0;
    return minter_(w);   // register with the ExtensionBridge, return its $object id
}

void HudService::set_brush(uint64_t id, uint64_t texture) {
    auto* layer = find_layer(id); if(!layer || layer->kind != 0) return;
    auto* tex = reinterpret_cast<UObject*>(texture);
    if(!tex || !WeakObject(tex).Get()) return;
    Call brush(layer->widget.Get(), L"SetBrushFromTexture", 2);
    brush.set(L"Texture", tex); brush.set(L"bMatchSize", false); brush.run();
}

void HudService::set_rect(uint64_t id, float x, float y, float w, float h, float ax, float ay, int32_t z) {
    auto* layer = find_layer(id); UObject* slot = layer ? layer->slot.Get() : nullptr; if(!slot) return;
    invoke(slot, L"SetAnchors", L"InAnchors", FAnchorsD{{0,0},{0,0}});
    invoke(slot, L"SetAlignment", L"InAlignment", Vec2{ax, ay});
    invoke(slot, L"SetPosition", L"InPosition", Vec2{x, y});
    invoke(slot, L"SetSize", L"InSize", Vec2{w, h});
    invoke(slot, L"SetZOrder", L"InZOrder", z);
}

void HudService::set_translation(uint64_t id, float x, float y) {
    auto* layer = find_layer(id); if(!layer) return;
    if(std::abs(layer->tx - x) < 1e-3f && std::abs(layer->ty - y) < 1e-3f) return;
    layer->tx = x; layer->ty = y;
    invoke(layer->widget.Get(), L"SetRenderTranslation", L"Translation", Vec2{x, y});
}

void HudService::set_scale(uint64_t id, float sx, float sy) {
    auto* layer = find_layer(id); if(!layer) return;
    if(std::abs(layer->sx - sx) < 1e-4f && std::abs(layer->sy - sy) < 1e-4f) return;
    layer->sx = sx; layer->sy = sy;
    invoke(layer->widget.Get(), L"SetRenderScale", L"Scale", Vec2{sx, sy});
}

void HudService::set_angle(uint64_t id, float deg) {
    auto* layer = find_layer(id); if(!layer) return;
    if(std::abs(layer->angle - deg) < 0.05f) return;
    layer->angle = deg;
    invoke(layer->widget.Get(), L"SetRenderTransformAngle", L"Angle", deg);
}

void HudService::set_pivot(uint64_t id, float px, float py) {
    auto* layer = find_layer(id); if(!layer) return;
    if(std::abs(layer->px - px) < 1e-3f && std::abs(layer->py - py) < 1e-3f) return;
    layer->px = px; layer->py = py;
    invoke(layer->widget.Get(), L"SetRenderTransformPivot", L"Pivot", Vec2{px, py});
}

void HudService::set_opacity(uint64_t id, float a) {
    auto* layer = find_layer(id); if(!layer) return;
    if(std::abs(layer->opacity - a) < 1e-3f) return;
    layer->opacity = a;
    invoke(layer->widget.Get(), L"SetRenderOpacity", L"InOpacity", a);
}

void HudService::set_color(uint64_t id, float r, float g, float b, float a) {
    auto* layer = find_layer(id); UObject* w = layer ? layer->widget.Get() : nullptr; if(!w) return;
    if(layer->kind == 1) invoke(w, L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{{r,g,b,a}});
    else invoke(w, L"SetColorAndOpacity", L"InColorAndOpacity", Color{r,g,b,a});
}

void HudService::set_visible(uint64_t id, int32_t visible) {
    auto* layer = find_layer(id); if(!layer) return;
    const int8_t want = visible ? 1 : 0;
    if(layer->visible == want) return;
    layer->visible = want;
    invoke(layer->widget.Get(), L"SetVisibility", L"InVisibility", uint8_t(visible ? VIS_HITTEST_INVISIBLE : VIS_COLLAPSED));
}

void HudService::set_text(uint64_t id, const char* utf8, size_t len) {
    auto* layer = find_layer(id); UObject* w = layer ? layer->widget.Get() : nullptr; if(!w || layer->kind != 1) return;
    text_value(w, std::string(utf8 ? utf8 : "", len));
}

void HudService::set_font(uint64_t id, float size) {
    auto* layer = find_layer(id); UObject* w = layer ? layer->widget.Get() : nullptr; if(!w || layer->kind != 1) return;
    font_size(w, size);
}

void HudService::set_clip(uint64_t id, int32_t clip) {
    auto* layer = find_layer(id); if(!layer) return;
    invoke(layer->widget.Get(), L"SetClipping", L"InClipping", uint8_t(clip ? 1 : 0));
}

void HudService::set_anchor(uint64_t id, float minx, float miny, float maxx, float maxy) {
    auto* layer = find_layer(id); UObject* slot = layer ? layer->slot.Get() : nullptr; if(!slot) return;
    invoke(slot, L"SetAnchors", L"InAnchors", FAnchorsD{{minx, miny}, {maxx, maxy}});
}

namespace {
// Vtable thunks. Each swallows its own exceptions so a transient widget error
// never unwinds into the extension's render (which would suspend the extension).
template<class F> auto hud_guard(F&& fn) noexcept -> decltype(fn()) {
    using R = decltype(fn());
    if(!g_hud_service) return R{};
    try { return fn(); } catch(...) { return R{}; }
}
CssxLayer hud_image(void* ctx, CssxLayer p) noexcept { return hud_guard([&]{ return g_hud_service->create_layer(0, p, nullptr, 0, reinterpret_cast<uintptr_t>(ctx)); }); }
CssxLayer hud_text(void* ctx, CssxLayer p) noexcept { return hud_guard([&]{ return g_hud_service->create_layer(1, p, nullptr, 0, reinterpret_cast<uintptr_t>(ctx)); }); }
CssxLayer hud_widget(void* ctx, CssxLayer p, const char* path, size_t len) noexcept { return hud_guard([&]{ return g_hud_service->create_layer(2, p, path, len, reinterpret_cast<uintptr_t>(ctx)); }); }
void hud_destroy(void*, CssxLayer id) noexcept { hud_guard([&]{ g_hud_service->destroy_layer(id); return 0; }); }
uint64_t hud_texture(void*, const char* path, size_t len) noexcept { return hud_guard([&]{ return g_hud_service->import_texture(path, len); }); }
uint64_t hud_layer_object(void*, CssxLayer id) noexcept { return hud_guard([&]{ return g_hud_service->layer_object(id); }); }
void hud_set_brush(void*, CssxLayer id, uint64_t t) noexcept { hud_guard([&]{ g_hud_service->set_brush(id, t); return 0; }); }
void hud_set_rect(void*, CssxLayer id, float x, float y, float w, float h, float ax, float ay, int32_t z) noexcept { hud_guard([&]{ g_hud_service->set_rect(id, x, y, w, h, ax, ay, z); return 0; }); }
void hud_set_translation(void*, CssxLayer id, float x, float y) noexcept { hud_guard([&]{ g_hud_service->set_translation(id, x, y); return 0; }); }
void hud_set_scale(void*, CssxLayer id, float sx, float sy) noexcept { hud_guard([&]{ g_hud_service->set_scale(id, sx, sy); return 0; }); }
void hud_set_angle(void*, CssxLayer id, float d) noexcept { hud_guard([&]{ g_hud_service->set_angle(id, d); return 0; }); }
void hud_set_pivot(void*, CssxLayer id, float px, float py) noexcept { hud_guard([&]{ g_hud_service->set_pivot(id, px, py); return 0; }); }
void hud_set_opacity(void*, CssxLayer id, float a) noexcept { hud_guard([&]{ g_hud_service->set_opacity(id, a); return 0; }); }
void hud_set_color(void*, CssxLayer id, float r, float g, float b, float a) noexcept { hud_guard([&]{ g_hud_service->set_color(id, r, g, b, a); return 0; }); }
void hud_set_visible(void*, CssxLayer id, int32_t v) noexcept { hud_guard([&]{ g_hud_service->set_visible(id, v); return 0; }); }
void hud_set_text(void*, CssxLayer id, const char* t, size_t len) noexcept { hud_guard([&]{ g_hud_service->set_text(id, t, len); return 0; }); }
void hud_set_font(void*, CssxLayer id, float s) noexcept { hud_guard([&]{ g_hud_service->set_font(id, s); return 0; }); }
void hud_set_clip(void*, CssxLayer id, int32_t c) noexcept { hud_guard([&]{ g_hud_service->set_clip(id, c); return 0; }); }
void hud_set_anchor(void*, CssxLayer id, float a, float b, float c, float d) noexcept { hud_guard([&]{ g_hud_service->set_anchor(id, a, b, c, d); return 0; }); }
void hud_minimap_update(void*, int32_t vis, float sc, float op, float aa, float ma, double px, double py, double zm, uint32_t flags) noexcept {
    try { minimap_update_direct(vis, sc, op, aa, ma, px, py, zm, flags); } catch(...) {}
}

const CssxHudApi g_hud_vtable{
    CSSX_ABI, sizeof(CssxHudApi),
    hud_image, hud_text, hud_widget, hud_destroy,
    hud_texture, hud_layer_object,
    hud_set_brush, hud_set_rect, hud_set_translation, hud_set_scale, hud_set_angle,
    hud_set_pivot, hud_set_opacity, hud_set_color, hud_set_visible,
    hud_set_text, hud_set_font, hud_set_clip, hud_set_anchor,
    hud_minimap_update,
};
} // namespace

const void* HudService::api() const { return &g_hud_vtable; }

} // namespace css

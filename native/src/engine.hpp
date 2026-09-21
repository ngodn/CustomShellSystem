#include <optional>
#pragma once
#include <string>
#include <functional>
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <set>
#include <vector>
#include "data.hpp"
#include "body_geometry.hpp"
#include "inventory_motion.hpp"
#include "inventory_backdrop.hpp"
#include "inventory_light.hpp"
#include "inventory_keys.hpp"
#include "inventory_light_keys.hpp"
#include "ground_offset.hpp"
#include "extension_client.hpp"
#include "cssx/hud.h"
#include "extension_search.hpp"
#include "hook_api.hpp"
#include <memory>
#include <Unreal/UObject.hpp>
#include <Unreal/FWeakObjectPtr.hpp>

namespace css {
fs::path engine_content_directory();
Outfit discover_original_shells();
// UE4SS's serial-allocation fallback uses a legacy soft-reference layout.
// Initialize new serials through a reflected frame before constructing a weak handle.
class WeakObject : public RC::Unreal::FWeakObjectPtr {
public:
    WeakObject() = default;
    WeakObject(RC::Unreal::UObject* object);
    WeakObject& operator=(RC::Unreal::UObject* object);
};
class Appearance;
class ExtensionBridge {
    struct HookGroup;
    const CssHookHost* hook_host_=nullptr;
    std::map<RC::Unreal::UFunction*,std::unique_ptr<HookGroup>> hooks_;
    uint64_t next_hook_=1;
    static void hook_callback(void*,void*,void*,void*) noexcept;
    Json hook_request(const Json&);
    std::map<uint64_t,WeakObject> objects_;
    std::map<std::string,WeakObject> defaults_;
    uint64_t next_=1;
    RC::Unreal::UObject* resolve(const Json&);
    Json handle(RC::Unreal::UObject*);
    Json decode(RC::Unreal::FProperty*,void*,unsigned);
    void encode(RC::Unreal::FProperty*,void*,const Json&,unsigned);
public:
    ExtensionBridge();
    ~ExtensionBridge();
    void configure_hooks(void* loader_address) noexcept;
    bool stop_hooks() noexcept;
    Json request(void* engine,Appearance&,const Json&);
    // Register an object and return its $object id (0 for null), so the HUD can
    // hand an extension a handle for a widget it built (e.g. SpartaMapWidget).
    uint64_t track(RC::Unreal::UObject*);
};
class InventoryUI {
#ifdef CSS_INVENTORY_DEV
    WeakObject cinema_camera_, cinema_player_, cinema_pc_, cinema_before_, cinema_hud_;
    uint8_t cinema_hud_visibility_=0;
    std::array<double,4> cinema_from_{}, cinema_to_{};
    uint64_t cinema_start_=0, cinema_duration_=0, cinema_deadline_=0;
    std::array<double,4> capture_from_{}, capture_to_{};
    uint64_t capture_start_=0, capture_duration_=0;
    void cinema_stop();
    void cinema_update(bool focused);
    void cinema_command(RC::Unreal::UObject*,const Json&);
#endif
    fs::path logo_path_, extension_logo_path_;
    ExtensionClient* extensions_=nullptr;
    WeakObject extension_tab_, extension_page_, extension_canvas_;
    bool extension_active_=false;
    std::string extension_id_, extension_text_key_, extension_text_draft_, extension_error_;
    Json extension_model_, extension_library_, extension_confirm_;
    extensions::LibraryPage extension_paging_;
    int extension_section_=0, extension_row_=0;
    std::vector<WeakObject> extension_loading_;
    WeakObject extension_description_;
    std::string extension_description_key_;
    float extension_description_offset_=0;
    bool extension_details_=false, extension_picker_=false;
    extensions::OptionSearch extension_options_;
    WeakObject extension_search_input_, extension_search_results_, extension_search_count_;
    std::string extension_search_query_;
    void build_extension_results();
    int extension_slide_=1;
    uint64_t extension_wheel_after_=0;
    uint64_t extension_revision_=0, extension_check_=0;
    void build_extensions();
    void build_extension_page();
    Json dispatch_extension(const Json&);
    Json dispatch_extension_action(const Json&);
    bool extension_input(const std::string&);
    WeakObject main_, tabs_, switcher_, tab_, page_, controller_;
    WeakObject canvas_, status_, scroll_, name_input_, display_, camera_component_, input_prompt_;
    WeakObject camera_tick_controller_;
    WeakObject camera_actor_, camera_state_, camera_target_before_;
    bool camera_tick_before_=false;
    float camera_fov_before_=0;
    std::array<double,3> camera_actor_location_before_{};
    Json confirm_action_;
    bool native_picker_=false;
    std::string native_picker_title_, native_picker_kind_, native_picker_target_;
    extensions::OptionSearch native_options_;
    WeakObject native_search_input_, native_search_results_, native_search_count_;
    std::string native_search_query_;
    uint64_t native_wheel_after_=0;
    const Catalog* catalog_=nullptr;
    const Appearance* appearance_=nullptr;
    void build_native_picker_results();
    struct Hit { WeakObject widget; Json action; bool down=false; };
    struct Row { WeakObject marker, widget; Json accept, previous, next, secondary, tertiary; };
    // `unit` is what the readout says after the number: "" for a bare value, " Hz" for a
    // frequency, "%" for a ratio shown as a percentage. It has to live here because the
    // drag handler redraws the label and only ever sees the slider.
    struct Slider { WeakObject widget, label, heading; Json action; float previous; bool scalar; std::string unit; };
    struct Binding { std::string action; std::vector<std::string> keys; bool down=false; uint64_t repeat=0; WeakObject input_action; };
    std::vector<Hit> hits_;
    std::vector<Row> rows_;
    std::vector<Slider> sliders_;
    std::vector<Binding> bindings_;
    std::map<std::string,WeakObject> textures_;
    std::map<std::string,std::pair<int,int>> texture_sizes_;
    std::vector<std::pair<WeakObject,std::array<float,4>>> top_padding_;
    std::array<double,2> layout_size_{};
    uint64_t layout_check_=0;
    int section_=0, row_=0, channel_=0, tint_field_index_=0;   // which tint slider Left/Right drives
    bool exact_color_=false;      // CUSTOMIZE: swatch strip, or Red/Green/Blue for the people who want it
    std::string last_message_;
    bool dirty_=true, active_=false, enabled_=true, was_active_=false;
    uint64_t discover_after_=0, last_tick_=0;
    uint64_t transition_started_=0;
    bool enter_transition_=false, closing_=false;
    struct TransitionWidget { WeakObject widget; std::array<double,2> offset; };
    std::vector<TransitionWidget> transition_widgets_;
    // What the last build cost, reported through diagnostics(). page_widgets_ is the
    // page canvas itself, nested_widgets_ is everything on the canvases inside it
    // (list rows, tab labels), which is the half that grows with the catalog.
    int page_widgets_=0, nested_widgets_=0;
    float scroll_offset_=0;
    WeakObject choice_scroll_;
    std::string choice_key_,choice_selected_;
    float choice_offset_=0;
    size_t choice_count_=0;
    double yaw_before_=0, yaw_=0, zoom_=0, frame_=0, pan_=0;
    InventoryMotion motion_;
    WeakObject light_component_;
    InventoryLightOrbit light_orbit_;
    LightVector light_location_before_{}, light_rotation_before_{};
    bool light_edit_=false;
    bool light_available() const;
    void light_start();
    void light_toggle();
    void light_reset();
    void light_move(double horizontal,double vertical);
    void light_stop();
    bool gamepad_=true, mouse_left_=false, mouse_right_=false, drag_pan_=false, drag_rotate_=false;
    std::array<double,2> mouse_before_{};
    float lens_before_=0;
    struct BackdropLayer {
        WeakObject component;
        BackdropVector relative_location{},relative_scale{},world_location{},axis_x{},axis_y{};
        double half_x=0,half_y=0;
    };
    std::vector<BackdropLayer> backdrop_layers_;
    BackdropVector backdrop_forward_{},backdrop_right_{},backdrop_up_{};
    double backdrop_aspect_=0;
    std::array<double,3> location_before_{}, camera_rotation_before_{}, camera_location_before_{}, camera_world_rotation_{}, camera_world_location_{};
    void build(const Catalog&,const State&,Appearance&);
    void bind_inputs();
    void camera_start();
    void camera_stop();
    void camera_tick_restore();
    void camera_bind_state();
    void camera_restore_state();
    void camera_update(double delta,bool invert_x);
    void camera_move(const std::array<double,4>& movement);
    void backdrop_start();
    void backdrop_update();
    void backdrop_stop();
    void animate(uint64_t now);
    void close_menu();
    Json dispatch(Json,const State&);
public:
    void assets(const fs::path& root) {
        logo_path_=root/"assets/inventory-logo-v1.png";
        extension_logo_path_=root/"assets/cssx-logo.png";
    }
    Json command(void* engine, const Json&);
    Json poll(void* engine,const Catalog&,const State&,Appearance&,float delta,bool focused);
    Json diagnostics() const;
    void message(const std::string&);
    void refresh() { dirty_=true; }
    void extensions(ExtensionClient* client) {extensions_=client;}
    void close() {close_menu();}
    bool active() const { return active_; }
    void detach();
};
class AttachmentFollower {
    WeakObject component_, proxy_, mesh_;
    std::string original_;
    bool ready_=false;
    std::set<std::wstring> unsupported_;
    struct Attachment {WeakObject child;};
    std::vector<Attachment> owned_;
public:
    void update(RC::Unreal::UObject* component,const std::string& original);
    void release();
};
// 0.4: per-outfit socket corrections for stowed items (seals sink into wider hips).
class AttachmentOffsets {
    // `location`/`rotation` hold the transform the game last gave the child, so the
    // correction is always measured from the game's own stow pose. `applied` is what
    // CSS last wrote, so a re-stow can be told apart from CSS's own live push.
    struct Tracked {
        WeakObject child; std::wstring socket;
        std::array<double,3> location{}, rotation{}, applied{};
        bool owned=false;
    };
    struct BonePose { std::array<double,3> location{}; double basis[3][3]{}; };
    std::vector<Tracked> tracked_;
    std::map<std::string,AttachmentOffset> offsets_;
    std::map<std::string,BonePose> poses_;
    bool pose(RC::Unreal::UObject* component,const std::string& bone,BonePose& out);
    bool push_for(RC::Unreal::UObject* component,RC::Unreal::UObject* child,const AttachmentOffset& offset,
                  const double socket_basis[3][3],std::array<double,3>& out);
    void apply(RC::Unreal::UObject* component,Tracked& item,const AttachmentOffset& offset,bool live);
public:
    void configure(const std::map<std::string,AttachmentOffset>& offsets);
    void update(RC::Unreal::UObject* component);        // 4 Hz: find and track stowed children
    void push(RC::Unreal::UObject* component);          // every frame: keep them out of the moving body
    void release();
    bool collides() const;
#ifdef CSS_INVENTORY_DEV
    // Rescale the correction on the live outfit, so a lift can be judged on screen
    // without rebuilding a package and restarting the game. Never shipped.
    Json tune(double lift,double clearance,double max_push);
    Json diagnostics() const;
    double last_distance_=0, last_push_=0;
#endif
};
// 1.0: the accessories a variant ships. Each is its own skeletal mesh component posed by
// the body it hangs on, created when the outfit goes on and destroyed when it comes off.
// The body item is not here: that one replaces the character mesh, which is what apply()
// has always done, so a package published before items is a one-item package and never
// reaches this at all.
class WornItems {
    struct Worn { std::string id; WeakObject component; };
    WeakObject body_;
    std::vector<Worn> worn_;
    // outfit:variant:mesh. Anything that changes it rebuilds the set rather than trying
    // to reconcile two lists of components, which is not worth the bookkeeping for at
    // most fifteen accessories.
    std::string identity_;
public:
    // Returns the body material sections the worn items ask to hide.
    std::set<int> update(RC::Unreal::UObject* body,const std::string& identity,const std::vector<Item>& items);
    void release();
    int count() const { return int(worn_.size()); }
    std::vector<std::string> ids() const;
    void sync_morph(const std::string& morph, float weight);
    void sync_morphs(const std::map<std::string, float>& driven_morphs);
};
// 0.4: optional feminine walk (LOCOMOTION tab). Drives the game's own carrier
// blendspace override on the player's animation instance, the way the GenessaWalk
// and ProximaWalk mods do, for whichever shell is worn.
class WalkOverride {
    WeakObject pawn_, anim_, movement_, walk_ability_, walk_bs_, run_bs_, active_;
    uint64_t next_ability_search_=0, last_heal_=0;
    int idle_ticks_=0, off_ticks_=0, slide_ticks_=0;
    uint64_t slide_until_=0;
    bool engaged_=false, run_tweaked_=false;
    uint8_t original_run_axis_=0;
    std::optional<uint64_t> hook_;
    std::atomic<bool> scale_walk_{false};
    std::string reason_;
    fs::path mods_; bool mods_checked_=false, mod_active_=false; uint64_t mods_check_=0;
    bool genessa_active_=false, proxima_active_=false;
    std::string mod_name_;
    RC::Unreal::UObject* blendspace(WeakObject& slot,const wchar_t* path);
    void push_on(RC::Unreal::UObject* target);
    void push_off();
    void hook_speed();
    void unhook_speed();
    void prepare_run(RC::Unreal::UObject* run);
    void restore_run();
public:
    bool walk_mod_active(const fs::path& mods);
    bool walk_mod_active() { return walk_mod_active(mods_); }
    const std::string& walk_mod_name() const { return mod_name_; }
    bool genessa_walk_active() const { return genessa_active_; }
    bool proxima_walk_active() const { return proxima_active_; }
    bool engaged() const { return engaged_; }
    const std::string& reason() const { return reason_; }
    void update(RC::Unreal::UObject* pawn,bool walk_feminine,bool jog_feminine,bool sprint_feminine);
    void release();
};
// CSSX ABI 2: a retained HUD surface for native extensions. The core owns the
// widgets (injected into WBP_Player_HUD); an extension holds only opaque layer
// handles and pushes cheap per-frame updates through the CssxHudApi vtable. All
// calls happen on the game thread from the core's per-frame tick.
class HudService {
    struct Layer {
        WeakObject widget, slot;   // slot is the CanvasPanelSlot (null for the root)
        uint8_t kind=0;            // 0 image, 1 text, 2 generic widget
        uintptr_t owner=0;
        // last values written, so redundant Slate writes are skipped
        float tx=1e30f, ty=1e30f, sx=1e30f, sy=1e30f, px=1e30f, py=1e30f, angle=1e30f, opacity=1e30f;
        int8_t visible=-1;
    };
    std::map<uint64_t,Layer> layers_;
    std::map<std::wstring,WeakObject> textures_;
    WeakObject hud_, tree_, overlay_;   // overlay_ is our root CanvasPanel in the HUD
    uint64_t next_layer_=1;
    uint32_t generation_=0;
    void* pc_=nullptr;                  // resolved player controller for this frame
    std::function<void(const std::string&)> log_;   // optional diagnostic sink
    std::function<uint64_t(RC::Unreal::UObject*)> minter_;   // widget -> $object id
    int log_tick_=0;
    void drop_scene();                  // forget overlay + all layers (world teardown)
    Layer* find_layer(uint64_t id);
    RC::Unreal::UObject* parent_canvas(uint64_t parent);
public:
    HudService();
    ~HudService();
    const void* api() const;            // &CssxHudApi, threaded into the extension host
    void set_logger(std::function<void(const std::string&)> fn) { log_=std::move(fn); }
    void set_object_minter(std::function<uint64_t(RC::Unreal::UObject*)> fn) { minter_=std::move(fn); }
    // Per frame: resolve the HUD, (re)build the overlay, fill `frame`, detect teardown.
    void update(void* engine,CssxFrame& frame);
    void release();                     // on core stop
    // Backends for the vtable (called with an owner token and layer ids):
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
};
// Native minimap (ported from Cartographer's Map/NativeWidget.lua). The
// SpartaMapWidget must be configured inline on the live object, so it lives in
// the engine TU (minimap_widget.inl) and the extension drives it through the
// hud.minimap.* request ops. Defined over a single file-static instance.
bool minimap_build(void* engine, const Json& config);
void minimap_update(const Json& update);
void minimap_update_direct(int vis, float sc, float op, float aa, float ma, double px, double py, double zm, uint32_t flags);
void minimap_destroy();
void minimap_set_logger(std::function<void(const std::string&)> fn);
// Native world-marker provider (Lost Gloom, nearby dungeons, map pings).
Json markers_collect(void* engine, double radius_m);
class Appearance {
    WeakObject component_, applied_;
    WeakObject ground_component_;
    GroundOffset ground_offset_;
    void restore_ground_offset();
    WeakObject observed_pawn_, observed_component_, observed_controller_;
    std::string original_;
    std::vector<std::string> original_materials_;
    std::set<std::string> original_default_materials_;
    std::vector<WeakObject> original_live_materials_;
    std::map<int,std::string> applied_materials_;
    // Two different things hide a body's material sections: a toggle the player flipped,
    // and an item that covers that part of the body. They are tracked apart and reconciled
    // together, because one owner clearing its own set must not put back what the other
    // still wants hidden. `applied_hidden_` is what the component was last told.
    std::set<int> toggle_hidden_, item_hidden_, applied_hidden_;
    // Spring: every field CSS may write on a node, remembered on first touch and keyed by
    // bone, so dropping the control puts the author's own motion back with no mesh reload.
    struct SpringOriginal {
        double stiffness=0, damping=0, max_displacement=0, error_reset=0;
        bool limit=false; std::array<bool,3> translate{}, rotate{};
        bool operator==(const SpringOriginal&) const = default;
    };
    std::map<std::string,SpringOriginal> spring_originals_;
    WeakObject spring_instance_;
    std::map<std::string,DynamicsSettings> dynamics_originals_;
    WeakObject dynamics_instance_;
    std::optional<RigSettings> rig_original_;
    std::optional<RigSettings> rig_applied_;
    WeakObject rig_instance_;
    std::optional<BodyRigSettings> body_rig_original_, body_rig_applied_;
    WeakObject body_rig_instance_;
    WeakObject body_geometry_instance_;
    std::optional<BodyGeometryModel> body_geometry_model_;
    std::optional<BodyGeometry> body_geometry_original_, body_geometry_applied_;
    std::optional<std::array<float,6>> body_geometry_morphs_;
    std::map<std::string,SpringOriginal> menu_spring_originals_;
    std::map<std::string,DynamicsSettings> menu_dynamics_originals_;
    std::optional<RigSettings> menu_rig_original_;
    std::optional<RigSettings> menu_rig_applied_;
    std::optional<BodyRigSettings> menu_body_rig_original_, menu_body_rig_applied_;
    std::optional<BodyGeometry> menu_body_geometry_original_, menu_body_geometry_applied_;
    WeakObject menu_physics_instance_;
    std::optional<bool> menu_post_process_disabled_;
    // Shape: the morph targets CSS drove and their weights, so taking the outfit off puts
    // back only those and leaves anything the game or another mod set alone.
    // ClearMorphTargets is a bigger hammer than this deserves. The weights are kept
    // because the wardrobe preview is a second component that needs the same ones.
    std::map<std::string,float> driven_morphs_;
    int lod_count();
    void show_hidden_sections();
    void restore_springs();
    void restore_dynamics();
    void restore_rig();
    void sync_menu_physics(RC::Unreal::UObject* component);
    void restore_menu_physics();
    void clear_driven_morphs();
    void push_morphs(RC::Unreal::UObject* component);
    std::map<int,WeakObject> control_mids_;
    std::map<std::string,WeakObject> dye_targets_, dye_textures_;
    std::map<std::string,ControlValue> last_values_;
    std::string control_outfit_;
    std::vector<WeakObject> expected_materials_;
    WeakObject menu_component_, menu_applied_;
    std::string menu_original_;
    std::vector<std::string> menu_original_materials_;
    std::vector<WeakObject> menu_original_live_materials_;
    AttachmentFollower attachments_, menu_attachments_;
    // The wardrobe previews a second component, so everything CSS puts on the body has to
    // be put on that one too: its accessories, its hidden sections and its shapes. Keeping
    // a separate WornItems for the preview matches how attachments already work.
    WornItems items_, menu_items_;
    std::vector<Item> current_items_;
    std::string current_items_identity_;
    std::set<int> menu_hidden_sections_;
    void reconcile_sections();
    AttachmentOffsets offsets_;
    std::map<std::string, std::array<double, 3>> formula_offsets_;
    void restore_menu();
    void remember_materials();
    bool materials_match() const;
    bool reuse_materials();
    void detach_residual_controls();
    void reset_controls();
    void sync_body_geometry(RC::Unreal::UObject* component);
    void prepare_deformation_materials();
public:
    std::string shell, pawn_name, current_mesh;
    uint64_t player_revision = 0;
    Json material_debug;
    RC::Unreal::UObject* player(void* engine);
    bool apply(void* engine, const std::string& mesh_path, const std::map<int,std::string>& materials = {});
    bool restore();
    void set_ground_offset(double offset);
    bool active() const;
    bool repair_materials_needed();
    bool repair_mesh_needed() const;
    bool ready_to_apply() const;
    void sync_menu();
    void sync_attachments();
    void sync_seals();   // every frame, unlike sync_attachments: a stride is faster than 4 Hz
#ifdef CSS_INVENTORY_DEV
    Json seal_diagnostics() const;
    Json tune_seals(double lift,double clearance,double max_push) { return offsets_.tune(lift,clearance,max_push); }
#endif
    void set_attachment_offsets(const std::map<std::string,AttachmentOffset>& offsets) { offsets_.configure(offsets); }
    // Put the variant's accessories on the body and hide what they cover. Safe to call
    // repeatedly: it rebuilds only when the outfit or variant changes.
    void sync_items(const Outfit&,const std::string& variant);
    int worn_item_count() const { return items_.count(); }
    WalkOverride walk;
    void sync_walk(bool walk_feminine,bool jog_feminine,bool sprint_feminine) { walk.update(observed_pawn_.Get(),walk_feminine,jog_feminine,sprint_feminine); }
    Json transition_state(void* engine);
#ifdef CSS_TRANSITION_TESTS
    void test_reset_mesh();
    void test_effect(bool begin,bool parameters=false);
    void test_cursor(void* engine, bool visible);
#endif
    void customize(const Outfit&, const std::string& variant, const Customization&);
};
}

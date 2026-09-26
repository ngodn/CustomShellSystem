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
#include <deque>
#include "data.hpp"
#include "body_geometry.hpp"
#include "inventory_motion.hpp"
#include "inventory_backdrop.hpp"
#include "inventory_light.hpp"
#include "inventory_keys.hpp"
#include "inventory_light_keys.hpp"
#include "ground_offset.hpp"
#include "animation_override.hpp"
#include "extension_search.hpp"   // OptionSearch, reused by CSS's own native outfit search
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
// CSS's own reflected access to the live game (player/get/set/call/find). This is not an
// extension host; the shell-revive recovery and CSS's dev probes use it to read and drive the
// engine through JSON ops. It keeps a handle table so a caller can hold an object across calls.
class EngineBridge {
    std::map<uint64_t,WeakObject> objects_;
    std::map<std::string,WeakObject> defaults_;
    uint64_t next_=1;
    RC::Unreal::UObject* resolve(const Json&);
    Json handle(RC::Unreal::UObject*);
    Json decode(RC::Unreal::FProperty*,void*,unsigned);
    void encode(RC::Unreal::FProperty*,void*,const Json&,unsigned);
public:
    Json request(void* engine,Appearance&,const Json&);
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
    fs::path logo_path_;
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
    std::string physics_modal_control_;
    int physics_modal_channel_=0;
    extensions::OptionSearch native_options_;
    WeakObject native_search_input_, native_search_results_, native_search_count_;
    std::string native_search_query_;
    uint64_t native_wheel_after_=0;
    const Catalog* catalog_=nullptr;
    const Appearance* appearance_=nullptr;
    void build_native_picker_results();
    // `parts`: glyphs inside the hit that each do their own thing (W / S on one prompt).
    // `glyph`: the WBP_Prompt that flashes when the hit is clicked, as it does for its key.
    struct Hit { WeakObject widget; Json action; bool down=false; std::vector<std::pair<WeakObject,Json>> parts; WeakObject glyph; };
    // `heading` is the category header right above the row, if any, so moving up onto it
    // brings the header into view too.
    struct Row { WeakObject marker, widget; Json accept, previous, next, secondary, tertiary; WeakObject heading; };
    // `unit` is what the readout says after the number: "" for a bare value, " Hz" for a
    // frequency, "%" for a ratio shown as a percentage. It has to live here because the
    // drag handler redraws the label and only ever sees the slider.
    // `widget` is the native bar the value is read off when dragged, `heading` the whole row
    // (its selected state marks the channel Left/Right adjusts).
    struct Slider { WeakObject widget, label, heading; Json action; float previous; bool scalar; std::string unit;
                    float minimum=0, maximum=1, step=0; };
    struct Binding { std::string action; std::vector<std::string> keys; bool down=false; uint64_t repeat=0; WeakObject input_action; };
    std::vector<Hit> hits_;
    bool mouse_was_down_=false, left_was_down_=false;
    int drag_slider_=-1;          // index into sliders_ while a bar is being dragged
    // ---- beta.5 native page (inventory_native.inl) ----
    // The page is the game's own widgets, created once and kept. A build runs the same
    // immediate-mode code as before, but each call takes the next pooled widget of its kind
    // and changes only what differs, so moving the selection is a few restyles instead of
    // a teardown and rebuild of every widget on the page.
    enum class NativeKind : uint8_t { none, row, header, option, slider, divider, action, swatches, input, paragraph, tab };
    struct NativeItem {
        NativeKind kind=NativeKind::none;
        WeakObject widget, hit, hit_left, hit_right, text_block, value_block, extra;
        std::vector<WeakObject> cells;                 // swatch chips: frame, colour, selection, button
        std::string text, value;                       // what is on screen now
        int selected=-1, badge=-1, shown=-1, enabled=-1, icon_shown=-1;   // icon_shown: 0 none, 1 picture, 2 colour chip, 3 empty slot; +4 in a two-line list
        const void* icon=nullptr; std::array<float,4> chip{}; float fill=-2.f;
        float name_width=-1.f;                         // list rows: the name box, shortened by a state word
        std::string glyph;                             // binding + fallback + device the glyph was set for
    };
    // A stack is a column of fixed cells. Each cell keeps one widget per kind it has ever
    // shown and switches which one is visible, so a layout change (a divider where a row
    // was) never creates, removes or reparents a widget once each shape has been seen.
    // Creating a game widget, or moving one to a new parent, rebuilds its Slate tree and
    // costs about a millisecond; a visibility switch costs almost nothing.
    struct NativeCell { WeakObject holder; std::vector<NativeItem> kinds; int shown=-1; };
    struct NativeStack { WeakObject box; std::deque<NativeCell> cells; size_t used=0; };   // deque: taken items keep their address
    void native_slot(NativeKind kind,RC::Unreal::UObject* slot);
    WeakObject design_, left_root_, right_root_, center_root_, list_scroll_, strip_scroll_, details_, panel_scroll_, panel_size_, status_text_, strip_previous_, strip_next_, strip_previous_glyph_, strip_next_glyph_;
    NativeStack tab_items_, list_, panel_head_, panel_, actions_, footer_, camera_bar_;   // panel_head_: fixed, above the scroll
    double design_w_=0, design_scale_=0;
    int shown_section_=-1, revealed_row_=-1;
    // The details window scroll follows the control Left/Right drives. `panel_context_` names
    // what the window shows (section, row, modal, search), so a new subject starts at its top.
    std::string panel_context_;
    const void* panel_revealed_=nullptr;
    // The window grows with its description and prompts; the scrolling part takes what is left
    // above the status line. Measured the frame after a build, once layout has run.
    bool panel_fit_pending_=false;
    // A scroll to a widget the build just showed waits a frame: until layout has measured
    // the new content, the scroll box clamps to the old content height.
    struct PendingReveal { WeakObject scroll, target; uint8_t destination=0; int frames=0; };
    std::array<PendingReveal,2> pending_reveals_{};   // the list, the details window
    void native_reveal_pending();
    float panel_max_=720.f;
    void native_fit_panel();
    std::string detail_title_, detail_sub_, detail_body_, status_shown_;
    const void* detail_icon_=reinterpret_cast<const void*>(1);
    WeakObject dialog_, dialog_primary_, dialog_secondary_;
    std::vector<WeakObject> frozen_listeners_;
    std::string dialog_shown_;
    int dialog_focus_=0, dialog_focus_shown_=-1;
#ifdef CSS_INVENTORY_DEV
    bool frozen_=false;   // inventory_freeze: no page rebuilds while prototyping
#endif
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
#ifdef CSS_INVENTORY_DEV
    // Builds slower than 1 ms: time, widgets created, and where the page was.
    int created_widgets_=0;
    std::vector<Json> slow_builds_;
#endif
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
    bool native_page(double width,double height);
    void native_forget();
    NativeItem& native_take(NativeStack&,NativeKind);
    void native_finish(NativeStack&);
    void native_glyph(RC::Unreal::UObject* prompt,const std::string& action,uint8_t fallback,uint8_t keyboard=255);
    void native_visible(NativeItem&,bool);
    void native_text(RC::Unreal::UObject* block,std::string& shown,const std::string& value);
    void native_state(NativeItem&,bool selected);
    void native_dialog();
    void native_dialog_close();
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
    std::optional<Json> press(const std::string& key,const State&);
public:
    void assets(const fs::path& root) {
        logo_path_=root/"assets/inventory-logo-v1.png";
    }
    Json command(void* engine, const Json&);
    Json poll(void* engine,const Catalog&,const State&,Appearance&,float delta,bool focused);
    Json diagnostics() const;
    void message(const std::string&);
    void refresh() { dirty_=true; }
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
    RC::Unreal::UObject* proxy() const { return proxy_.Get(); }
};
// 0.4: per-outfit socket corrections for stowed items (seals sink into wider hips).
class AttachmentOffsets {
    // `location`/`rotation` hold the transform the game last gave the child, so the
    // correction is always measured from the game's own stow pose. `applied` is what
    // CSS last wrote, so a re-stow can be told apart from CSS's own live push.
    struct Tracked {
        WeakObject child; RC::Unreal::FName socket; std::string socket_key;   // socket_key = narrow(socket), the offsets_ map key, cached so push() never allocates
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
    void configure(const std::map<std::string,AttachmentOffset>& offsets, bool include_defaults=true);
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
// Shared owner for the built-in feminine idle/walk and mod movement options.
// The game's animation instance evaluates the selected locomotion BlendSpace.
class WalkOverride {
    struct SameWeakObject {
        bool operator()(const WeakObject& a,const WeakObject& b) const {
            return a.ObjectIndex==b.ObjectIndex && a.ObjectSerialNumber==b.ObjectSerialNumber;
        }
    };
    using BlendLease=AnimationOverrideLease<WeakObject,SameWeakObject>;
    BlendLease blend_lease_;
    bool original_blend_root_owned_=false;
    WeakObject pawn_, anim_, movement_, walk_bs_, active_;
    std::array<std::string,3> custom_paths_;
    std::array<WeakObject,3> custom_blends_;
    WeakObject custom_skeleton_;
    uint64_t last_heal_=0;
    int idle_ticks_=0, off_ticks_=0, slide_ticks_=0;
    uint64_t slide_until_=0;
    bool engaged_=false;
    std::optional<uint64_t> hook_;
    // What the walk-speed hook reads. The hook holds its own reference, so a hook still
    // registered when the core is torn down at exit (never on the game thread, so it cannot be
    // unregistered safely there) reads a switched-off flag instead of a freed WalkOverride.
    struct SpeedHook {
        std::atomic<bool> scale{false};
        std::atomic<uint32_t> thread{0};
        std::atomic<const void*> movement{nullptr};   // compared by address only, never dereferenced
    };
    std::shared_ptr<SpeedHook> speed_=std::make_shared<SpeedHook>();
    std::string reason_;
    fs::path mods_; bool mods_checked_=false, mod_active_=false; uint64_t mods_check_=0;
    bool genessa_active_=false, proxima_active_=false;
    std::string mod_name_;
    std::string custom_idle_clip_;
    bool hide_weapons_=false;
    bool custom_idle_engaged_=false;
    WeakObject custom_idle_post_;
    std::vector<WeakObject> hidden_weapons_;
    uint64_t next_footstep_=0;
    bool foot_left_=false;
    std::optional<std::array<double,3>> hair_resting_gravity_;
    bool hair_aerodynamics_active_=false;
    RC::Unreal::UObject* blendspace(WeakObject& slot,const wchar_t* path);
    RC::Unreal::UObject* custom_blendspace(size_t index,RC::Unreal::UObject* skeleton);
    void set_weapon_hidden(RC::Unreal::UObject* pawn,bool hide);
    void push_on(RC::Unreal::UObject* target);
    void push_off();
    void forget_blend_lease();
    void hook_speed();
    void unhook_speed();
public:
    ~WalkOverride() { speed_->scale=false; }
    bool walk_mod_active(const fs::path& mods);
    bool walk_mod_active() { return walk_mod_active(mods_); }
    const std::string& walk_mod_name() const { return mod_name_; }
    bool genessa_walk_active() const { return genessa_active_; }
    bool proxima_walk_active() const { return proxima_active_; }
    bool engaged() const { return engaged_ || custom_idle_engaged_; }
    bool custom_idle_engaged() const { return custom_idle_engaged_; }
    const std::string& reason() const { return reason_; }
    void update(RC::Unreal::UObject* pawn,bool idle_feminine,bool walk_feminine,
                const std::array<std::string,3>& custom_paths,
                const std::string& custom_idle_clip={},
                bool hide_weapons=false);
    void release();
};
// MISC visibility applier. Bounded: it hides only the OWNER ACTORS of accessories found as
// direct children of a mesh, never the body (which is the parent) and never in-hand weapons
// (a socket gate). One instance drives the world pawn, another the wardrobe preview. Method
// bodies live in misc_visibility.inl. See that file and investigation/2026-09-23.
class MiscVisibility {
    struct Item { WeakObject component, owner; std::string category; };
    std::vector<Item> candidates_;   // item mesh components found at the last enumeration (~20 Hz)
    std::vector<Item> hidden_;       // components currently hidden
public:
    // Rebuild the candidate list by walking the containers. The heavy pass (a fully dressed body
    // has hundreds of components), so it runs rate-limited, not every frame.
    void enumerate(const std::vector<RC::Unreal::UObject*>& containers, RC::Unreal::UObject* pawn);
    // Decide and enforce visibility for the cached candidates. Cheap (only the ~10 real items),
    // so it runs EVERY frame: it re-reads each item's socket (a draw/sheathe/use changes it),
    // re-hides anything the game turned back on, and reveals an item the instant it is used - no
    // 50 ms flash when an action ends.
    void evaluate(const std::map<std::string,MiscRule>& rules, bool action_active);
    void restore();
    bool any() const { return !hidden_.empty(); }
};
class Appearance {
    WeakObject component_, applied_;
    WeakObject ground_component_;
    GroundOffset ground_offset_;
    void restore_ground_offset();
    WeakObject observed_pawn_, observed_component_, observed_controller_;
    // player()'s string cache: rebuilt only when the tag, pawn or mesh changes.
    uint64_t shell_tag_ = 0;
    std::string shell_text_, pawn_text_, mesh_text_;
    WeakObject mesh_seen_;
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
    // Transition guard: one dye value CSS wrote, cheap to re-read. A launchpad/Harbinger gate
    // resets material parameters in place - same MID objects, so every pointer check misses it -
    // and if this value has diverged the customization was silently reset and must be re-applied.
    WeakObject color_check_mid_;
    RC::Unreal::FName color_check_param_;
    uint8_t color_check_assoc_=0; int32_t color_check_layer_=0;
    bool color_check_scalar_=false, color_check_valid_=false;
    ControlValue color_check_value_{};
    std::vector<WeakObject> expected_materials_;
    WeakObject menu_component_, menu_applied_;
    std::string menu_original_;
    std::vector<std::string> menu_original_materials_;
    std::vector<WeakObject> menu_original_live_materials_;
    AttachmentFollower attachments_, menu_attachments_;
    // MISC visibility: the active rules (a copy of state.misc_rules), the appliers for the
    // world pawn and the wardrobe preview, and the last sampled locomotion so the world pass
    // only re-hides when the player's state actually changes.
    MiscVisibility misc_, menu_misc_;
    std::map<std::string, MiscRule> misc_rules_;   // only the categories set to something other than Default
    uint64_t misc_signature_ = 0;                  // what misc_layout_changed() last saw attached
    bool misc_rules_changed_ = false;
    WeakObject menu_display_mesh_;
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
    bool repair_materials_needed() const;
    bool customization_reset() const;        // a transition reset CSS's applied customization in place
    bool transition_active() const;   // a teleport/gate/traversal is currently in progress
    bool repair_mesh_needed() const;
    bool ready_to_apply() const;
    std::string ready_to_apply_reason() const;
    void sync_menu();
    void sync_attachments();
    void sync_seals();   // every frame, unlike sync_attachments: a stride is faster than 4 Hz
    // MISC visibility. `set_misc_rules` copies the player's choices in; `sync_misc` re-hides
    // the world pawn's accessories when locomotion changes (called from the tick); the menu
    // pass runs inside sync_menu against the wardrobe preview character.
    // Default means "never touch", so it is not kept: with every category on Default the MISC
    // passes see an empty rule set and cost nothing.
    void set_misc_rules(const std::map<std::string, MiscRule>& rules) {
        misc_rules_.clear();
        for(const auto& [category,rule]:rules) if(rule.mode!="default") misc_rules_.emplace(category,rule);
        misc_rules_changed_=true;   // relist (or restore everything) on the next frame
    }
    bool misc_layout_changed();   // every frame: did anything attach, detach or swap on the bodies?
    void sync_misc();          // on a layout change (and once a second): rebuild the candidate item lists
    void tick_misc();          // every frame: decide + enforce visibility on the cached items
    void restore_misc() { misc_.restore(); menu_misc_.restore(); }
    Json misc_diagnostics() const {
        return {{"rules", misc_rules_.size()}, {"world_hidden", misc_.any()}, {"menu_hidden", menu_misc_.any()}};
    }
    Json misc_report() const;   // per-item live visibility state on the world pawn (dev diagnostic)
#ifdef CSS_INVENTORY_DEV
    Json seal_diagnostics() const;
    Json tune_seals(double lift,double clearance,double max_push) { return offsets_.tune(lift,clearance,max_push); }
#endif
    void set_attachment_offsets(const std::map<std::string,AttachmentOffset>& offsets, bool include_defaults=true) { offsets_.configure(offsets, include_defaults); }
    // Put the variant's accessories on the body and hide what they cover. Safe to call
    // repeatedly: it rebuilds only when the outfit or variant changes.
    void sync_items(const Outfit&,const std::string& variant);
    int worn_item_count() const { return items_.count(); }
    WalkOverride walk;
    void sync_walk(bool idle_feminine,bool walk_feminine,const std::array<std::string,3>& custom_paths,
                   const std::string& custom_idle_clip={},bool hide_weapons=false) {
        walk.update(observed_pawn_.Get(),idle_feminine,walk_feminine,custom_paths,custom_idle_clip,hide_weapons);
    }
    Json transition_state(void* engine);
#ifdef CSS_INVENTORY_DEV
#endif
#ifdef CSS_TRANSITION_TESTS
    void test_reset_mesh();
    void test_effect(bool begin,bool parameters=false);
    void test_cursor(void* engine, bool visible);
#endif
    void customize(const Outfit&, const std::string& variant, const Customization&);
};
}

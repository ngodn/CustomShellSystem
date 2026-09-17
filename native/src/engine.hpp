#include <optional>
#pragma once
#include <string>
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <set>
#include <vector>
#include "data.hpp"
#include "inventory_motion.hpp"
#include "inventory_keys.hpp"
#include "extension_client.hpp"
#include "extension_search.hpp"
#include "hook_api.hpp"
#include <memory>
#include <Unreal/UObject.hpp>
#include <Unreal/FWeakObjectPtr.hpp>

namespace css {
fs::path engine_content_directory();
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
    double yaw_before_=0, yaw_=0, zoom_=0, frame_=0, pan_=0;
    InventoryMotion motion_;
    bool gamepad_=true, mouse_left_=false, mouse_right_=false, drag_pan_=false, drag_rotate_=false;
    std::array<double,2> mouse_before_{};
    float lens_before_=0;
    std::array<double,3> location_before_{}, camera_rotation_before_{}, camera_location_before_{}, camera_world_rotation_{}, camera_world_location_{};
    void build(const Catalog&,const State&,Appearance&);
    void bind_inputs();
    void camera_start();
    void camera_stop();
    void camera_update(double delta,bool invert_x);
    void camera_move(const std::array<double,4>& movement);
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
// 0.4: optional feminine walk (ANIMATION tab). Drives the game's own carrier
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
class Appearance {
    WeakObject component_, applied_;
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
    };
    std::map<std::string,SpringOriginal> spring_originals_;
    WeakObject spring_instance_;
    // Shape: the morph targets CSS drove and their weights, so taking the outfit off puts
    // back only those and leaves anything the game or another mod set alone.
    // ClearMorphTargets is a bigger hammer than this deserves. The weights are kept
    // because the wardrobe preview is a second component that needs the same ones.
    std::map<std::string,float> driven_morphs_;
    int lod_count();
    void show_hidden_sections();
    void restore_springs();
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
    void prepare_deformation_materials();
public:
    std::string shell, pawn_name, current_mesh;
    uint64_t player_revision = 0;
    Json material_debug;
    RC::Unreal::UObject* player(void* engine);
    bool apply(void* engine, const std::string& mesh_path, const std::map<int,std::string>& materials = {});
    bool restore();
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

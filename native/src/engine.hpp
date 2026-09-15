#pragma once
#include <string>
#include <algorithm>
#include <array>
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
    struct Slider { WeakObject widget, label, heading; Json action; float previous; bool scalar; };
    struct Binding { std::string action; std::vector<std::string> keys; bool down=false; uint64_t repeat=0; WeakObject input_action; };
    std::vector<Hit> hits_;
    std::vector<Row> rows_;
    std::vector<Slider> sliders_;
    std::vector<Binding> bindings_;
    std::map<std::string,WeakObject> textures_;
    std::vector<std::pair<WeakObject,std::array<float,4>>> top_padding_;
    std::array<double,2> layout_size_{};
    uint64_t layout_check_=0;
    int section_=0, row_=0, color_channel_=0;
    std::string last_message_;
    bool dirty_=true, active_=false, enabled_=true, was_active_=false;
    uint64_t discover_after_=0, last_tick_=0;
    uint64_t transition_started_=0;
    bool enter_transition_=false, closing_=false;
    struct TransitionWidget { WeakObject widget; std::array<double,2> offset; };
    std::vector<TransitionWidget> transition_widgets_;
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
class Appearance {
    WeakObject component_, applied_;
    WeakObject observed_pawn_, observed_component_, observed_controller_;
    std::string original_;
    std::vector<std::string> original_materials_;
    std::set<std::string> original_default_materials_;
    std::vector<WeakObject> original_live_materials_;
    std::map<int,std::string> applied_materials_;
    std::map<int,WeakObject> color_mids_;
    std::map<std::string,WeakObject> color_targets_, color_textures_;
    std::map<std::string,ColorValue> last_colors_;
    std::string color_outfit_;
    std::vector<WeakObject> expected_materials_;
    WeakObject menu_component_, menu_applied_;
    std::string menu_original_;
    std::vector<std::string> menu_original_materials_;
    std::vector<WeakObject> menu_original_live_materials_;
    void restore_menu();
    void remember_materials();
    bool materials_match() const;
    bool reuse_materials();
    void detach_residual_colors();
    void reset_colors();
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
    Json transition_state(void* engine);
#ifdef CSS_TRANSITION_TESTS
    void test_reset_mesh();
    void test_effect(bool begin,bool parameters=false);
    void test_cursor(void* engine, bool visible);
#endif
    void customize(const Outfit&, const std::string& variant, const Customization&);
};
}

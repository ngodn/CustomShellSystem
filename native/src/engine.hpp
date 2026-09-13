#pragma once
#include <string>
#include <algorithm>
#include <array>
#include <vector>
#include "data.hpp"
#include <Unreal/UObject.hpp>
#include <Unreal/FWeakObjectPtr.hpp>

namespace css {
// UE4SS's serial-allocation fallback uses a legacy soft-reference layout.
// Initialize new serials through a reflected frame before constructing a weak handle.
class WeakObject : public RC::Unreal::FWeakObjectPtr {
public:
    WeakObject() = default;
    WeakObject(RC::Unreal::UObject* object);
    WeakObject& operator=(RC::Unreal::UObject* object);
};
class Appearance {
    WeakObject component_, applied_;
    std::string original_;
    std::vector<std::string> original_materials_;
    std::map<int,std::string> applied_materials_;
    std::map<int,WeakObject> color_mids_;
    std::map<std::string,WeakObject> color_targets_, color_textures_;
    std::map<std::string,ColorValue> last_colors_;
    std::string color_outfit_;
    void reset_colors();
public:
    std::string shell, pawn_name, current_mesh;
    Json material_debug;
    RC::Unreal::UObject* player(void* engine);
    bool apply(void* engine, const std::string& mesh_path, const std::map<int,std::string>& materials = {});
    bool restore();
    bool active() const;
    void customize(const Outfit&, const Customization&);
};
// All widgets and input ownership stay on the game thread. No widget delegates
// point into the reloadable DLL, so closing the view permits core unloading.
class Wardrobe {
    struct Hit { WeakObject widget; Json action; bool down = false; };
    struct Row { Json wear, favorite, previous, next, save; WeakObject marker; };
    WeakObject root_, controller_, pawn_, status_;
    std::vector<Hit> hits_;
    std::vector<Row> rows_;
    struct Slider { WeakObject widget, label; Json action; float previous=0; bool scalar=false; };
    std::vector<Slider> sliders_;
    WeakObject color_title_, color_swatch_;
    std::string color_title_text_;
    int color_part_ = 0;
    bool owns_input_ = false, old_cursor_ = false;
    WeakObject camera_, view_before_;
    bool owns_pause_ = false, full_tick_before_ = false, full_tick_changed_ = false;
    struct HiddenActor { WeakObject actor; bool before; };
    WeakObject preview_, preview_mesh_;
    std::vector<HiddenActor> hidden_;
    double preview_time_ = 0, preview_length_ = 0, preview_sample_at_ = 0;
    std::array<double,3> preview_first_head_{};
    bool preview_sampled_ = false, preview_moving_ = false;
    std::string preview_animation_;
    uint64_t last_input_tick_ = 0;
    double yaw_ = 0, pitch_ = 3, distance_ = 500, height_ = 0, pan_ = 0;
    std::array<double,3> last_center_{};
    double frame_offset_ = .32, default_distance_ = 500;
    bool camera_dirty_ = true, right_mouse_ = false;
    bool invert_x_ = false, invert_y_ = true;
    long mouse_x_ = 0, mouse_y_ = 0;
    int focus_ = 0, pad_index_ = -1;
    uint16_t pad_buttons_ = 0;
    uint64_t next_pad_search_ = 0, repeat_at_ = 0;
    void camera_open(double aspect, double screen_x);
    void camera_close();
    void camera_update();
    void protect();
    void unprotect();
    void preview_open();
    void preview_close();
    void preview_update(double delta);
    void focus(int index);
    int category_ = 0, page_ = 0;
    std::map<std::string, WeakObject> textures_;
public:
    bool opened() const { return root_.Get() != nullptr; }
    void open(void* engine, const Catalog&, const State&, Appearance&, const std::string& message, const fs::path& assets);
    void close();
    Json poll(float delta, bool focused);
    void message(const std::string&);
    void rotate(double degrees, bool front = false);
    Json diagnostics() const;
    Json inspect(RC::Unreal::UObject* player) const;
    void sync_materials();
    void configure(bool invert_x,bool invert_y) { invert_x_=invert_x; invert_y_=invert_y; }
    void filter(int category) { category_ = std::clamp(category,0,3); page_ = 0; }
    void color_part(int delta) { color_part_ += std::clamp(delta,-1,1); }
    void page(int delta) { page_ = std::max(0, page_ + delta); }
};
}

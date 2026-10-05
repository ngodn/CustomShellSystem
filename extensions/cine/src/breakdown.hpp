#pragma once
// Breakdown: shows a character and the frame in layers, the way a VFX breakdown reel does.
//
// Everything is built once and then left to the engine: bone shapes are static-mesh components
// attached to their bones, physics bodies are the engine's own shape components attached the
// same way, wireframe and clay are materials on her mesh, and the frame passes are a
// post-process material on CINE's camera. A layer is shown or hidden with one material
// parameter, and a wipe between two sets of layers runs on the GPU from four numbers written
// once (assets/author_materials.py). So a breakdown costs the mod nothing per frame.
//
// The materials come from CINE's own asset pak (/Game/CSSX/CINE). Without it only the layers
// drawn by engine shape components (collision, capsule) work, and note() says so.
//
// Building is spread over ticks (bounded bridge calls per tick). Game thread only; tick() and
// show() never throw.
#include "shot.hpp"
#include <cssx/client.hpp>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

enum class Layer : uint8_t {
    Clay, Wireframe, Skeleton, VirtualBones, IkBones, Springs, Sockets, Collision, Capsule,
    BaseColor, Normals, Roughness, Metallic, Depth, Occlusion, Lighting,
    Count
};
using LayerSet = uint32_t;
constexpr LayerSet bit(Layer l) { return LayerSet{1} << static_cast<unsigned>(l); }
constexpr LayerSet frame_layers = bit(Layer::BaseColor) | bit(Layer::Normals) | bit(Layer::Roughness) | bit(Layer::Metallic)
                                | bit(Layer::Depth) | bit(Layer::Occlusion) | bit(Layer::Lighting);
constexpr LayerSet bone_layers = bit(Layer::Skeleton) | bit(Layer::VirtualBones) | bit(Layer::IkBones) | bit(Layer::Springs) | bit(Layer::Sockets);
const char* layer_id(Layer);                       // as written in presets: "wireframe", "base_color"...
const char* layer_label(Layer);                    // for the caption: "Wireframe", "Base colour"...
std::optional<Layer> layer_from(const std::string& id);
// Preset form of a set: ["clay", "wireframe"]. Throws std::invalid_argument naming a bad id.
LayerSet layers_from(const Json& array);

// How a bone is drawn, decided from its name alone (pure, tested).
enum class BoneKind { Regular, Virtual, Ik, Spring };
BoneKind classify_bone(const std::string& name, const std::vector<std::string>& spring_bones);
// Default spring bones by name, for outfits that follow the usual naming (chest, glutes, belly).
bool spring_by_name(const std::string& bone);
// "chest", "glutes", "belly" or "" for a spring bone name: used to group the callouts.
std::string spring_group(const std::string& bone);
// Unit quaternion (x, y, z, w) turning +Z onto `direction` by the shortest arc.
struct Quat { double x = 0, y = 0, z = 0, w = 1; };
Quat z_to(Vec3 direction);

// A label for something on screen: text, and the scene component (plus socket or bone) it points at.
struct Callout { std::string text; Json component; std::string socket; };

class Breakdown {
public:
    explicit Breakdown(const cssx::Client& host) : host_(host) {}

    // Start building the layers in `needed` for this pawn. `camera` is CINE's take camera
    // (its CameraComponent), or null: then frame passes are skipped. `spring_bones` come from
    // the outfit when known; empty falls back to the usual names.
    void begin(const Json& pawn, const Json& camera, LayerSet needed, std::vector<std::string> spring_bones);
    bool active() const { return active_; }
    bool ready() const { return active_ && queue_.empty() && !reading_; }
    // 10 Hz: builds a bounded amount, finishes a transition, tears down after finish().
    void tick();
    // Go to this set of layers with a left-to-right wipe of `wipe_seconds` (0 cuts).
    void show(LayerSet layers, double wipe_seconds);
    LayerSet shown() const { return shown_; }
    // Put everything back over the next ticks (normal end of a take)...
    void finish();
    // ...or right now (cancel, stop). Returns what could not be restored.
    std::vector<std::string> end();
    void forget();                                 // world changed: handles are dead
    bool busy() const { return active_ || !teardown_.empty(); }

    const std::string& note() const { return note_; }          // layers skipped, and why
    const std::vector<Callout>& callouts() const { return callouts_; }
    std::string caption() const;                               // "Wireframe + Skeleton"

    static constexpr size_t calls_per_tick = 110;

private:
    struct Piece { Json component; LayerSet layer = 0; };      // one attached component
    struct Bone { std::string name, parent; BoneKind kind = BoneKind::Regular; Vec3 local; };
    struct Work { enum Kind { Stick, Joint, Socket, Body } kind = Stick; size_t index = 0; };

    void resolve_assets();
    void read_skeleton();          // a slice of bones per tick
    void plan();                   // skeleton read: queue the pieces
    void build(const Work& work);
    Json add_component(const Json& cls, const Json& transform);
    Json material(Layer layer);    // the layer's shared instance, created on first use
    Json instance(const char* key, Vec3 colour, double opacity);
    void set_param(const Json& mid, const char* name, double value);
    void wipe_numbers(double from, double to, double seconds);
    void apply_clay(bool on);      // slot materials <-> her own
    void set_overlay(const Json& material);
    void settle();                 // a transition ended: outgoing layers off, switches reset
    void refresh_callouts();
    void add_note(const std::string& text);
    Json call(const Json& target, const std::string& function, const Json& args = Json::object()) const;
    bool valid(const Json& object) const;
    Json load(const std::string& path) const;
    double game_time() const;
    bool spend(size_t calls) { if (budget_ < calls) return false; budget_ -= calls; return true; }

    const cssx::Client& host_;
    bool active_ = false, reading_ = false, pak_ = false, frame_passes_ = false;
    LayerSet needed_ = 0, shown_ = 0, leaving_ = 0;
    size_t budget_ = 0;
    Json pawn_, mesh_, capsule_, camera_, statics_, kismet_materials_;
    Json class_mesh_, class_capsule_, class_sphere_, class_box_;
    Json mids_[static_cast<size_t>(Layer::Count)];
    Json clay_, wire_, pass_[2];           // instances: clay, wireframe, the two frame-pass slots
    int pass_now_ = 0;                     // which pass instance shows the current frame layer
    Json overlay_before_;                  // her mesh's own overlay material
    std::vector<Json> slots_before_;       // her mesh's own slot materials while clay is on
    bool clay_slots_ = false, overlay_ours_ = false, capsule_touched_ = false, capsule_hidden_before_ = true;
    Json capsule_colour_before_;
    std::vector<std::string> spring_names_;
    std::vector<Bone> bones_;
    size_t bones_expected_ = 0;
    std::vector<std::string> sockets_;
    std::vector<Json> bodies_;             // physics body setups
    std::deque<Work> queue_;
    std::vector<Piece> pieces_;
    std::deque<Json> teardown_;
    double transition_end_ = -1;           // game time, or -1 when settled
    double shapes_flip_at_ = -1;           // game time the shape layers swap (middle of a wipe)
    LayerSet shapes_pending_ = 0;
    std::vector<Callout> callouts_;
    std::string note_;
};

} // namespace cine

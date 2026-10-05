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
#include "layers.hpp"
#include <cssx/client.hpp>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

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
    // True when this frame layer was asked for and its pass is on the camera.
    bool has_pass(Layer layer) const { return active_ && frame_passes_ && (needed_ & bit(layer)); }
    // Put everything back over the next ticks (normal end of a take)...
    void finish();
    // ...or right now (cancel, stop). Returns what could not be restored.
    std::vector<std::string> end();
    void forget();                                 // world changed: handles are dead
    bool busy() const { return active_ || !teardown_.empty(); }

    const std::string& note() const { return note_; }          // layers skipped, and why
    // After end(): true when one of her materials could not be put back exactly (the slot was
    // cleared instead). Whoever painted her (the wardrobe mod) should paint her again.
    bool needs_repaint() const { return repaint_; }
    const std::vector<Callout>& callouts() const { return callouts_; }
    std::string caption() const;                               // "Wireframe + Skeleton"

    static constexpr size_t calls_per_tick = 110;

private:
    struct Piece { Json component; LayerSet layer = 0; };      // one attached component
    struct Bone { std::string name, parent; BoneKind kind = BoneKind::Regular; Vec3 local; };
    struct Work { enum Kind { Stick, Joint, Socket, Body, Skin } kind = Stick; size_t index = 0; };

    void resolve_assets();
    void read_skeleton();          // a slice of bones per tick
    void plan();                   // skeleton read: queue the pieces
    void build(const Work& work);
    Json add_component(const Json& cls, const Json& transform);
    Json material(Layer layer);    // the layer's shared instance, created on first use
    Json instance(const char* key, Vec3 colour, double opacity);
    void set_param(const Json& mid, const char* name, double value);
    void wipe_numbers(double from, double to, double seconds);
    void apply_clay(bool on);      // slot materials <-> her own, on every skinned part
    void set_overlay(const Json* material);   // our material on every part, or null: each part's own back
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
    // Every skinned part of the character (body, hair, garments, accessories): clay and
    // wireframe go on all of them. The body mesh is first.
    struct Skin { Json component, overlay_before; std::vector<Json> slots_before; bool read = false; };
    std::vector<Skin> skins_;
    // Her own materials are held here for the whole take. While clay sits in a slot nothing
    // else may reference the material it replaced (an outfit's dynamic instance), and a
    // garbage collection would take it: the take would end with nothing to put back. The
    // locker is a hidden component of ours whose material list keeps every one of them alive.
    Json locker_;
    int locked_ = 0;
    bool repaint_ = false;                 // something of hers could not be put back as it was
    void keep(const Json& material);
    void read_skin(Skin& skin);
    void verify_restored();
    static bool ours(const Json& material);
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

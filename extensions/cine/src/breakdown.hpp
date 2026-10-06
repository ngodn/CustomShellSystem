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
//
// A take that changes outfit (a sequence) teaches the breakdown every look before it builds:
// learn() reads the worn mesh's bones, assemble() then builds one piece per bone of the union,
// and pieces are grouped by which looks have their bone. adopt() after an outfit change shows
// only the groups that exist on the look now worn, so bone shapes never hang off a bone that
// is not there.
//
// A step may stop its wipe part of the way (split): the layers stay left of that line and the
// final picture right of it, until the next step moves the line or changes what is left of it.
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
// `other` is a second place the same label may point at (the other bone of a left and right
// pair): a split breakdown points at the one on the layers' side.
struct Callout { std::string text; Json component; std::string socket; std::string other; };

class Breakdown {
public:
    explicit Breakdown(const cssx::Client& host) : host_(host) {}

    struct Options {
        // The take wears several looks: begin() builds nothing; learn() each look, then assemble().
        bool looks = false;
        // Never put clay in her material slots, only in the overlay slot: the wardrobe mod can
        // then repaint her under a running breakdown. Clay and wireframe cannot show together.
        bool overlay_only = false;
    };
    // Start building the layers in `needed` for this pawn. `camera` is CINE's take camera
    // (its CameraComponent), or null: then frame passes are skipped. `spring_bones` come from
    // the outfit when known; empty falls back to the usual names.
    void begin(const Json& pawn, const Json& camera, LayerSet needed, std::vector<std::string> spring_bones) { begin(pawn, camera, needed, std::move(spring_bones), Options{}); }
    void begin(const Json& pawn, const Json& camera, LayerSet needed, std::vector<std::string> spring_bones, Options options);
    bool active() const { return active_; }
    bool ready() const { return active_ && queue_.empty() && !reading_; }
    // Read the bones of the look she wears now, under this name (a slice per tick).
    void learn(const std::string& look, const std::vector<std::string>& spring_bones);
    bool learned() const { return !reading_; }
    // Every look is learned: queue the pieces.
    void assemble();
    // Before the wardrobe mod changes her outfit: take our materials off her.
    void suspend();
    // After it: find her parts again, show the bone groups this look has, put our materials back.
    void adopt(const std::string& look);
    // 10 Hz: builds a bounded amount, finishes a transition, tears down after finish().
    void tick();
    // Go to this set of layers with a left-to-right wipe of `wipe_seconds` (0 cuts). `stop` is
    // where the wipe line comes to rest, as a fraction of the frame's width: 1 is the whole
    // frame, less holds a split with the final picture right of the line.
    void show(LayerSet layers, double wipe_seconds, double stop = 1.0);
    LayerSet shown() const { return shown_; }
    // Where the split line rests (or is heading), 1 when the layers have the whole frame.
    double edge() const { return transition_end_ >= 0 && edge_mode_ ? edge_to_ : edge_; }
    // How far the depth pass reaches, in cm: white at the lens, black at this distance. Set
    // from the shot (a close-up at 3000 cm of range is a white screen).
    void depth_range(double cm);
    // True when this frame layer was asked for and its pass is on the camera.
    bool has_pass(Layer layer) const { return active_ && frame_passes_ && (needed_ & bit(layer)); }
    // A held split line goes to this place across the frame (a fraction of its width). Called
    // every frame by a take whose split follows her body; it writes only when the line moved.
    void follow(double place);
    // True when every layer of the set can show.
    bool can_show(LayerSet set) const { return active_ && set != 0 && (needed_ & set) == set && (!(set & frame_layers) || frame_passes_); }
    // True while clay sits in her own material slots (never with overlay_only).
    bool in_slots() const { return clay_slots_; }
    // Her parts may have changed (an outfit is landing): put our overlay on any new one.
    void reskin();
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
    struct Piece { Json component; LayerSet layer = 0; uint32_t looks = 0; };   // one attached component, and the looks that have its bone
    struct Bone { std::string name, parent; BoneKind kind = BoneKind::Regular; Vec3 local; uint32_t looks = 0; };
    struct Work { enum Kind { Stick, Joint, Socket, Body, Skin } kind = Stick; size_t index = 0; };

    void resolve_assets();
    void read_skeleton();          // a slice of bones per tick
    void plan();                   // skeleton read: queue the pieces
    void build(const Work& work);
    Json add_component(const Json& cls, const Json& transform);
    Json material(Layer layer, uint32_t looks);   // the shared instance of a layer's bone group, created on first use
    void find_skins();
    const Json& wire_mid();        // the wireframe and clay instances, made on first use and kept alive
    const Json& clay_mid();
    void place_bones(LayerSet layer_bit, const Json& mid, uint32_t looks, int state);
    void edge_show(LayerSet set, double wipe_seconds, double stop);
    // A look that was never learned has what every look has.
    bool worn(uint32_t looks) const { return look_now_ < looks_.size() ? (looks & (uint32_t{1} << look_now_)) != 0 : looks == all_looks_; }
    size_t bones_worn() const;
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
    struct Group { Layer layer; uint32_t looks; Json mid; };
    std::vector<Group> groups_;            // bone shapes share one instance per layer and set of looks
    std::vector<std::string> looks_;       // the looks learned, in order; a piece's bit i is looks_[i]
    size_t look_now_ = 0;
    uint32_t all_looks_ = 1;
    Options options_;
    bool edge_mode_ = false;               // the running transition moves the line only (no outgoing side)
    double edge_ = 1.0, edge_to_ = 1.0;    // where the wipe line rests now, and where the running wipe ends
    Json suspended_;                       // our overlay, taken off for an outfit change
    Json overlay_now_;                     // our material in the overlay slot, when overlay_ours_
    bool suspended_clay_ = false, assembled_ = false;
    size_t read_index_ = 0;                // the next bone index of the look being learned
    int watch_ = 0;
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
    double depth_range_ = 0;
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

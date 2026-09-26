#pragma once
#include <filesystem>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "controls.hpp"
#include "animations.hpp"

namespace css {
inline constexpr const char* original_shells_id = "css.original_shells";
using Json = nlohmann::json;
namespace fs = std::filesystem;
inline std::string path_utf8(const fs::path& path) {
    const auto value=path.generic_u8string();
    return {value.begin(),value.end()};
}
// 0.4: a stowed prop is welded to its socket and never collides with the body, so a
// fuller shell leaves seals buried in the hip and every stride swings the body through
// them. `location`/`rotation` are a fixed correction in socket space; `collision` asks CSS
// to hold the socket a set distance off the body's own physics asset, measured live, which
// is the only thing that tracks the pose.
struct AttachmentCollision {
    double clearance = 0;                // cm to keep between the socket and the body
    double max_push = 0;                 // cm, a hard stop so no pose can fling the prop away
    std::string anchor;                  // bone carrying the fallback direction
    std::array<double,3> direction{};    // outward in anchor space, used when the socket is inside the body
    // The anchor only supplies a direction for the rare frame where the prop is fully
    // inside the body and the engine has none to give, so it is not required.
    bool active() const { return clearance > 0 && max_push > 0; }
    bool operator==(const AttachmentCollision&) const = default;
};
struct AttachmentOffset { std::array<double,3> location{}, rotation{}; AttachmentCollision collision; bool operator==(const AttachmentOffset&) const = default; };   // socket space, cm and degrees
// 1.0: a variant is a set of items rather than a single mesh. Exactly one sits in the
// `body` slot and replaces the character mesh, which is what every package published so
// far does, and the rest are accessories posed by the body. A manifest that names one
// `mesh` reads as a single body item, so nothing published has to change.
enum class ItemSlot {
    Body, Head, Hair, Face, Ears, Neck, Chest, Back, Hands, Waist, Legs, Feet,
    Jewelry, Genitalia, FabricOuter, FabricInner,
    Trinket1, Trinket2, Trinket3, Trinket4
};
const char* item_slot_name(ItemSlot);
bool item_slot_from_name(const std::string&, ItemSlot&);
// 1.0.0-beta: Templates for combinations, palettes, body archetypes, physics, etc.
enum class TemplateKind { Combination, Palette, Archetype, Physics, Hair, Jewelry, Glow, Accessory, Fabric, Anatomy };
struct Template {
    std::string id, name;
    TemplateKind kind = TemplateKind::Combination;
    Json data = Json::object();
};
struct Item {
    std::string id, name, mesh;
    ItemSlot slot = ItemSlot::Body;
    // Layering, low to high. Two items on the same part of the body need an order for
    // the author to say which one sits on top; the cook decides the rest.
    int order = 0;
    std::map<int,std::string> materials;
    // Body material sections this item covers, so a boot can stop a foot poking through.
    std::vector<int> hides_sections;
};
struct Variant {
    std::string id, name, mesh;
    std::map<int,std::string> materials;
    std::optional<ControlSet> controls;
    std::map<std::string,AttachmentOffset> attachments;   // 0.4: per-socket correction for stowed items
    // Always at least one, and items[0] is the body item whose mesh and materials are
    // mirrored by `mesh` and `materials` above.
    std::vector<Item> items;
    double ground_offset_cm=0;
    AnimationSet animations;
};
struct Outfit {
    std::string id, name, author, description, category;
    std::vector<std::string> shells;
    std::vector<Variant> variants;
    bool same_skeleton = false;
    fs::path thumbnail;
    ControlSet controls;
    fs::path resources;
    std::vector<Template> templates;
    AnimationSet animations;
    const ControlSet& controls_for(const std::string& variant) const {
        for(const auto& v:variants) if(v.id==variant && v.controls) return *v.controls;
        return controls;
    }
};
struct Catalog {
    std::vector<Outfit> outfits;
    Json diagnostics = Json::object();
    static Catalog load(const fs::path&,const fs::path& paks={},const fs::path& cache={});
    std::string empty_message() const;
    const Variant* find(const std::string& outfit, const std::string& variant) const;
    bool compatible(const std::string& outfit, const std::string& shell) const;
    std::vector<const Outfit*> display_order(const std::string& equipped,const std::set<std::string>& favorites) const;
    const std::vector<AnimationOption>& animation_options(const std::string& outfit,
        const std::string& variant,AnimationSlot) const;
};
struct Selection { std::string outfit, variant; Customization custom; };
// 1.0.0-alpha.7: MISC visibility. Per category the player picks one of three modes. There is
// no combat/locomotion detection: CSS only ever acts on an item while it rests on its stowed
// or cosmetic socket, so a weapon you draw leaves that socket and shows on its own. That makes
// "only when in use" fall out for free, and "always hidden" is the same plus hiding the drawn
// copy too. Modes: "default" (never touch, the game decides), "hidden" (hide it, even in use),
// "in_use" (hide it while it rests on the body; it shows the moment you draw/use it).
struct MiscRule {
    std::string mode = "default";   // default | hidden | in_use
    bool hides_at_rest() const { return mode=="hidden" || mode=="in_use"; }
    bool hides_when_drawn() const { return mode=="hidden"; }
    bool operator==(const MiscRule&) const = default;
    Json json() const;
    static MiscRule parse(const Json&);
};
inline const std::array<const char*,4>& misc_categories() {
    static const std::array<const char*,4> c{"seal","sidearm","stowed_weapons","accessories"};
    return c;
}
// Every category offers all three modes: some accessories are usable shell tools (Eredrim's
// Diapason fires a shell ability), so none are locked out of "only when in use". A piece that
// never leaves its socket simply behaves the same under "in_use" as under "hidden".
inline bool misc_category_has_in_use(const std::string&) { return true; }
// 0.4: a template keeps the animation settings with the outfit selections.
struct Preset {
    std::map<std::string, Selection> selections;
    std::string walk_animation = "normal";
    AnimationChoices animation_choices;
    std::map<std::string, MiscRule> misc_rules;
};
bool valid_walk_animation(const std::string&);
// 1.0.0-beta: Profile is the full character snapshot across all systems
using Profile = Preset;
struct State {
    bool enabled = false;
    bool auto_apply = true;
    bool invert_orbit_x = false, invert_orbit_y = true;
    std::string walk_animation = "normal";   // "normal" or "feminine" (LOCOMOTION tab). Jog and sprint stay on the game's own animation in 0.4.
    AnimationChoices animation_choices;
    bool harbinger_mirror = true;            // when severed into the Harbinger (Darkform), wear the living shell's current outfit instead of its own saved one.
    bool keep_default_attachments = false;   // MISC safety switch: skip the stowed-weapon collision servos entirely and leave every prop on its native game position.
    bool darkform_mirror_cleaned = false;    // one-time: dropped Harbinger slots the old mirror polluted with a living shell's look.
    std::map<std::string, Selection> selections;
    std::map<std::string, Customization> remembered_custom;
    std::set<std::string> favorites;
    std::map<std::string, MiscRule> misc_rules;   // global MISC visibility, applied to any shell
    std::map<std::string, Preset> presets;
    std::map<std::string, Profile>& profiles() { return presets; }
    const std::map<std::string, Profile>& profiles() const { return presets; }
    static State parse(const Json&);
    Json json() const;
};
State load_state(const fs::path&, bool* recovered = nullptr);
bool set_animation_choice(State&,const Catalog&,const std::string& shell,
    const std::string& outfit,const std::string& variant,AnimationSlot,const std::string& choice);
bool use_feminine_animation(const State&,const std::string& shell,AnimationSlot);
bool set_legacy_walk_choice(State&,const Catalog&,const std::string& shell,const std::string& value);
Json read_json(const fs::path&);
void atomic_json(const fs::path&, const Json&, bool backup = true);
// runtime/*.json is diagnostics for tools and support logs, not saved state: a plain write
// and rename, with no fsync and no read-back, so the game thread never waits on the disk.
void write_runtime_json(const fs::path&, const Json&);
bool valid_id(const std::string&);
bool valid_asset(const std::string&);
}

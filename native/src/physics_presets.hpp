#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "controls.hpp"

// Physics presets for CSS's own motion: the angular body rig, the positional hair rig and
// SpringBone parts. CSS ships a built-in set for each solver; a package can declare more
// on any physics control (Control::presets), listed after the built-ins.
namespace css {
enum class PhysicsRegion { chest, glute, thigh, belly, none };

// Which body region a part moves, from what the manifest declares: a body rig's region
// bones, then a spring's nodes. Only a part that declares neither falls back to the words
// in its id and name.
PhysicsRegion physics_region(const Control&);

// Built-in presets that suit this control (none for AnimDynamics, whose units differ),
// clamped to its ranges, then the ones its package declares.
std::vector<PhysicsPreset> physics_presets(const Control&);

// The value a preset gives this control. Channel 3 (a rig's motion switch, a spring's
// fixed opacity) stays as it is, and so does a spring's travel when it has no clamp.
ControlValue physics_preset_value(const Control&, const PhysicsPreset&, const ControlValue& current);

// A body rig preset's travel in degrees, clamped to the part's range. Empty for a part
// without a travel range, a spring, or a package preset that sets none.
std::optional<float> physics_preset_travel(const Control&, const PhysicsPreset&);

// The preset `value` matches, or "" when the sliders were moved off every preset. With
// `travel` (the part's current travel) a preset that sets travel must match it as well.
std::string matching_physics_preset(const Control&, const ControlValue& value,
                                    std::optional<float> travel = std::nullopt);

// The soft limit band (rig SoftZone) for every body rig part that has a value or a travel
// override: the band of the preset its sliders match, else body_soft_zone_default. A
// built-in template carries its band; a package preset or a hand-tuned part gets the
// default. Derived, not saved: a look keeps its four numbers and travel per part.
std::map<std::string,float> physics_soft_zones(const std::vector<Control>&,
                                               const std::map<std::string,ControlValue>& values,
                                               const std::map<std::string,float>& travel);

// The `physics_preset` request: one part's preset, or with an empty control id every part
// that has a preset of that id. Writes the three channels and, on a part with a travel
// range, its travel. Throws when no part has the preset. `label` gets the preset's name.
Customization apply_physics_preset(const ControlSet&, Customization, const std::string& preset,
                                   const std::string& control, std::string* label = nullptr);

// Ids older saves and commands used for today's built-ins.
std::string physics_preset_id(std::string id);

// Every built-in id, so a package cannot declare one of them.
bool builtin_physics_preset(const std::string& id);
}

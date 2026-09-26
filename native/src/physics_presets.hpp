#pragma once
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

// The preset `value` matches, or "" when the sliders were moved off every preset.
std::string matching_physics_preset(const Control&, const ControlValue& value);

// Ids older saves and commands used for today's built-ins.
std::string physics_preset_id(std::string id);

// Every built-in id, so a package cannot declare one of them.
bool builtin_physics_preset(const std::string& id);
}

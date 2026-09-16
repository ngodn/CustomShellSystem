#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
using ControlValue = std::array<float,4>;
struct ControlBinding {
    int slot = 0;
    std::string parameter;
    int association = 2, layer = -1;
};
// 0.4: every control says which half of the character it belongs to and what kind of
// material it is, so the menu can group them the same way in every package and a group
// tint knows what it may rotate. See docs/control-convention.md. Both are optional and are
// inferred from the id when a package predates the convention.
enum class ControlGroup { Outfit, Body };
// 1.0: colour is one kind of control, not the only one. A package declares what a
// control *is*, and the menu, the saved look and the apply path all follow from that.
// `Color` carries three channels and a dye layer; the rest carry one number.
//   Intensity  a named material scalar the author means as a strength, like eye glow
//   Scalar     any other named material scalar, like gloss or roughness
//   Toggle     material sections shown or hidden, 0 or 1
//   Choice     one of a few textures the package ships, chosen by index
//   Spring     live secondary motion: how fast a part moves and how fast it settles
//   Shape      a morph target the package cooked into its own mesh
enum class ControlKind { Color, Intensity, Scalar, Toggle, Choice, Spring, Shape };
const char* control_kind_name(ControlKind);
// A Choice option: what the player sees, and the cooked texture it binds.
struct ControlOption { std::string name, texture; };
// What a spring node wants, in the two numbers that mean something to a person, and
// what the engine wants, which is neither of them. FAnimNode_SpringBone integrates
// a = K*error - D*velocity at a fixed 1/120 s with no mass term, so the system is
// x'' + D x' + K x = 0: K = (2*pi*f)^2 and D = 4*pi*zeta*f. Doing that conversion here
// means no author ever has to know it. See docs/control-convention.md.
struct SpringTuning { double stiffness = 0, damping = 0; };
SpringTuning spring_tuning(float frequency, float damping_ratio);
struct Control {
    std::string id, name, role;
    ControlGroup group = ControlGroup::Outfit;
    ControlKind kind = ControlKind::Color;
    bool hue_locked = false;      // metal, gems and skin read as a material, not a colour
    bool scalar = false;          // edited as one number rather than a colour: every kind but Color
    std::vector<int> sections;    // Toggle only: the material sections it shows or hides
    std::vector<ControlOption> options;   // Choice only: the textures it picks between
    std::vector<std::string> nodes;       // Spring only: the bones whose spring it tunes
    std::string morph;                    // Shape only: the morph target on the package's own mesh
    ControlValue value{1,1,1,1};
    // Channel 0 uses these. Every control but Spring has only channel 0.
    float minimum = 0, maximum = 1, step = .01f;
    // Spring only: channel 1, the damping ratio. Channel 0 is frequency in Hz.
    float damping_minimum = 0, damping_maximum = 1, damping_step = .01f;
    std::vector<ControlBinding> bindings;
};
// A hue rotation in degrees plus saturation and brightness multipliers, applied to a whole
// group on top of the palette and any per-part override.
struct ColorTint {
    float hue = 0, saturation = 1, brightness = 1;
    bool neutral() const { return hue == 0 && saturation == 1 && brightness == 1; }
    bool operator==(const ColorTint&) const = default;
};
struct DyeSurface {
    std::string id, parameter;
    std::vector<int> slots;
    int resolution = 2048;
    std::map<std::string,std::string> layers;
};
struct Palette {
    std::string id, name;
    std::map<std::string,ControlValue> values;
};
struct ControlSet {
    std::vector<Control> controls;
    std::vector<DyeSurface> surfaces;
    std::vector<Palette> palettes;
    static ControlSet parse(const nlohmann::json&);
    const Control* find(const std::string&) const;
};
struct Customization {
    std::string palette = "original";
    std::map<std::string,ControlValue> values;
    std::map<std::string,ColorTint> tints;   // keyed by group name: "outfit" or "body"
    static Customization parse(const nlohmann::json&);
    nlohmann::json json() const;
    bool operator==(const Customization&) const = default;
};
const char* control_group_name(ControlGroup);
ControlValue apply_tint(const ColorTint&, const ControlValue&, bool hue_locked);
std::map<std::string,ControlValue> control_values(const ControlSet&, const Customization&);
Customization compatible_values(const ControlSet&, const Customization&);
Customization choose_palette(const ControlSet&, const Customization&, const std::string& palette);
bool dye_resource(const std::string&);
float srgb_linear(float);
}

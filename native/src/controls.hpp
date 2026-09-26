#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
using ControlValue = std::array<float,4>;
// A physics preset: the first three channels of a spring, rig or AnimDynamics part.
// Built-ins come from CSS (physics_presets.cpp); a package adds its own per control.
struct PhysicsPreset {
    std::string id, name, description;
    std::array<float,3> channels{};
    bool builtin=false;
};
struct ColorSwatch {
    std::string name;
    ControlValue color;
    bool reset=false;
};
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
// Color has three channels; motion controls have two or three solver channels.
//   Intensity  a named material scalar the author means as a strength, like eye glow
//   Scalar     any other named material scalar, like gloss or roughness
//   Toggle     material sections shown or hidden, 0 or 1
//   Choice     one of a few textures the package ships, chosen by index
//   Spring     live secondary motion: how fast a part moves and how fast it settles
//   Shape      a morph target the package cooked into its own mesh
//   Glow       universal emissive control (RGB, intensity, pulse, combat reactivity)
//   Opacity    alpha transparency scalar (0..1)
//   Dynamics   direct AnimDynamics angular stiffness, damping and gravity
enum class ControlKind { Color, Intensity, Scalar, Toggle, Choice, Spring, Shape, Glow, Opacity, Dynamics, Rig };
const char* control_kind_name(ControlKind);
// A Choice option: what the player sees, and the cooked texture it binds.
struct ControlOption { std::string name, texture; };
// A morph formula: shifts joint center or orientation alongside a morph target
struct MorphFormula {
    std::string target;      // target bone
    std::string type;        // BoneCenterX/Y/Z or OrientationX/Y/Z
    double multiplier = 0.0;
};
// What a spring node wants, in the two numbers that mean something to a person, and
// what the engine wants, which is neither of them. FAnimNode_SpringBone integrates
// a = K*error - D*velocity at a fixed 1/120 s with no mass term, so the system is
// x'' + D x' + K x = 0: K = (2*pi*f)^2 and D = 4*pi*zeta*f. Doing that conversion here
// means no author ever has to know it. See docs/control-convention.md.
struct SpringTuning { double stiffness = 0, damping = 0; };
SpringTuning spring_tuning(float frequency, float damping_ratio);
struct SliderRange { float minimum=0, maximum=1, value=0, step=.01f; };
struct DynamicsControl {
    // Direct AnimDynamics values: angular spring constant, damping override,
    // gravity multiplier. These are not SpringBone frequency/damping-ratio units.
    std::array<SliderRange,3> channels;
};
struct RigControl {
    // CSS ControlRig positional stiffness, exponential damping, gravity scale.
    std::array<SliderRange,3> channels;
    bool body=false;
    std::vector<uint8_t> regions;
};
struct Control {
    std::string id, name, role;
    ControlGroup group = ControlGroup::Outfit;
    ControlKind kind = ControlKind::Color;
    bool hue_locked = false;      // metal, gems and skin read as a material, not a colour
    bool scalar = false;          // numeric controls, excluded from color tinting
    std::vector<int> sections;    // Toggle only: the material sections it shows or hides
    std::vector<int> occludes_sections; // Toggle only: covered sections hidden while it is on
    std::vector<ControlOption> options;   // Choice only: the textures it picks between
    std::vector<std::string> nodes;       // Spring bones or Dynamics chain root bones
    std::optional<DynamicsControl> dynamics;
    std::optional<RigControl> rig;
    std::string morph;                    // Shape only: the morph target on the package's own mesh
    std::vector<MorphFormula> formulas;   // Shape only: joint center/orientation shifts
    float pulse_hz = 0.0f;                // Glow only: breathing frequency in Hz
    bool combat_reactive = false;         // Glow only: reactivity to stamina/swings
    ControlValue value{1,1,1,1};
    // Legacy channel 0 range. Dynamics keeps its three ranges in dynamics.
    float minimum = 0, maximum = 1, step = .01f;
    // Spring only: channel 1, the damping ratio. Channel 0 is frequency in Hz.
    float damping_minimum = 0, damping_maximum = 1, damping_step = .01f;
    // Spring only, optional: channel 2 is a travel clamp (FAnimNode_SpringBone's
    // MaxDisplacement, in cm). With a max_displacement range the spring limits how far
    // the part moves, which is what lets Better Jiggle run a lively low damping without
    // the bone flying off; without one a spring keeps its two channels and no clamp.
    bool spring_clamp = false;
    float displacement_minimum = 0, displacement_maximum = 0, displacement_step = .05f;
    // Spring only, optional axis filters and reset threshold. -1 leaves the blueprint's
    // own flag alone; 0 or 1 forces it. error_reset < 0 leaves the threshold alone.
    std::array<std::int8_t,3> translate{{-1,-1,-1}}, rotate{{-1,-1,-1}};
    double error_reset = -1;
    // Reserved dynamics metadata, not applied by the SpringBone adapter. Preserve
    // absence separately from an explicit zero for the future solver adapter.
    std::optional<float> world_damping;    // 0..1 world motion damping
    std::optional<float> limit_angle;      // 0..180 degrees
    std::optional<float> collision_radius; // 0..100 cm
    std::optional<float> gravity_scale;    // -5..5
    int planar_constraint = 0;         // 0: none, 1: X, 2: Y, 3: Z
    std::vector<ControlBinding> bindings;
    std::vector<ColorSwatch> swatches;
    std::vector<PhysicsPreset> presets;   // spring, rig or dynamics: the package's own presets
};
struct SpringAxes {
    std::array<bool,3> translate{}, rotate{};
    bool operator==(const SpringAxes&) const = default;
};
// Explicit overrides start from the captured author settings. A planar lock then
// disables its normal axis without enabling any other axis.
SpringAxes spring_axes(const Control&, SpringAxes authored);
int control_channel_count(const Control&);
SliderRange control_channel(const Control&, int channel);
struct DynamicsSettings {
    float angular_spring=0, linear_damping=.7f, angular_damping=.7f, gravity=1;
    bool spring_enabled=false, override_linear=false, override_angular=false, gravity_override=false;
    bool operator==(const DynamicsSettings&) const = default;
};
DynamicsSettings dynamics_settings(const Control&, const ControlValue&);
bool dynamics_reset_required(const DynamicsSettings& before, const DynamicsSettings& after);
struct RigSettings {
    float stiffness=150, damping=18;
    std::array<double,3> gravity{};
    bool enabled=true;
    bool operator==(const RigSettings&) const = default;
};
RigSettings rig_settings(const Control&, const ControlValue&);
inline constexpr std::array<const char*,7> body_region_names={"brust001","brust002","butt001","butt002",
    "thigh_twist_02_l","thigh_twist_02_r","belly"};
struct BodyRigSettings {
    std::array<float,7> frequency{}, damping{}, motion{};
    std::array<bool,7> enabled{};
    float global_frequency=2, global_damping=.7f, global_motion=1;
    bool use_regions=false;
    bool operator==(const BodyRigSettings&) const = default;
};
bool body_rig_control(const Control&);
BodyRigSettings body_rig_settings(const std::vector<Control>&, const std::map<std::string,ControlValue>&,
                                  const BodyRigSettings& authored);
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
    // The player's Ground height, in cm (-10..10). Unset: the variant's ground_offset_cm.
    std::optional<double> ground_offset_cm;
    static Customization parse(const nlohmann::json&);
    nlohmann::json json() const;
    bool operator==(const Customization&) const = default;
};
const char* control_group_name(ControlGroup);
ControlValue apply_tint(const ColorTint&, const ControlValue&, bool hue_locked);
std::map<std::string,ControlValue> control_values(const ControlSet&, const Customization&);
std::set<int> hidden_control_sections(const ControlSet&, const std::map<std::string,ControlValue>&);
Customization compatible_values(const ControlSet&, const Customization&);
Customization choose_palette(const ControlSet&, const Customization&, const std::string& palette);
bool dye_resource(const std::string&);
float srgb_linear(float);
}

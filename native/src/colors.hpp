#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
using ColorValue = std::array<float,4>;
struct ColorBinding {
    int slot = 0;
    std::string parameter;
    int association = 2, layer = -1;
};
// 0.4: every control says which half of the character it belongs to and what kind of
// material it is, so the menu can group them the same way in every package and a group
// tint knows what it may rotate. See docs/color-convention.md. Both are optional and are
// inferred from the id when a package predates the convention.
enum class ColorGroup { Outfit, Body };
// 1.0: colour is one kind of control, not the only one. A package declares what a
// control *is*, and the menu, the saved look and the apply path all follow from that.
// `Color` carries three channels and a dye layer; the rest carry one number.
//   Intensity  a named material scalar the author means as a strength, like eye glow
//   Scalar     any other named material scalar, like gloss or roughness
//   Toggle     material sections shown or hidden, 0 or 1
//   Choice     one of a few textures the package ships, chosen by index
enum class ControlKind { Color, Intensity, Scalar, Toggle, Choice };
const char* control_kind_name(ControlKind);
// A Choice option: what the player sees, and the cooked texture it binds.
struct ControlOption { std::string name, texture; };
struct ColorControl {
    std::string id, name, role;
    ColorGroup group = ColorGroup::Outfit;
    ControlKind kind = ControlKind::Color;
    bool hue_locked = false;      // metal, gems and skin read as a material, not a colour
    bool scalar = false;          // edited as one number rather than a colour: every kind but Color
    std::vector<int> sections;    // Toggle only: the material sections it shows or hides
    std::vector<ControlOption> options;   // Choice only: the textures it picks between
    ColorValue value{1,1,1,1};
    float minimum = 0, maximum = 1, step = .01f;
    std::vector<ColorBinding> bindings;
};
// A hue rotation in degrees plus saturation and brightness multipliers, applied to a whole
// group on top of the palette and any per-part override.
struct ColorTint {
    float hue = 0, saturation = 1, brightness = 1;
    bool neutral() const { return hue == 0 && saturation == 1 && brightness == 1; }
    bool operator==(const ColorTint&) const = default;
};
struct ColorSurface {
    std::string id, parameter;
    std::vector<int> slots;
    int resolution = 2048;
    std::map<std::string,std::string> layers;
};
struct ColorPalette {
    std::string id, name;
    std::map<std::string,ColorValue> values;
};
struct ColorOptions {
    std::vector<ColorControl> controls;
    std::vector<ColorSurface> surfaces;
    std::vector<ColorPalette> palettes;
    static ColorOptions parse(const nlohmann::json&);
    const ColorControl* find(const std::string&) const;
};
struct Customization {
    std::string palette = "original";
    std::map<std::string,ColorValue> values;
    std::map<std::string,ColorTint> tints;   // keyed by group name: "outfit" or "body"
    static Customization parse(const nlohmann::json&);
    nlohmann::json json() const;
    bool operator==(const Customization&) const = default;
};
const char* color_group_name(ColorGroup);
ColorValue apply_tint(const ColorTint&, const ColorValue&, bool hue_locked);
std::map<std::string,ColorValue> color_values(const ColorOptions&, const Customization&);
Customization compatible_colors(const ColorOptions&, const Customization&);
Customization choose_palette(const ColorOptions&, const Customization&, const std::string& palette);
bool color_resource(const std::string&);
float srgb_linear(float);
}

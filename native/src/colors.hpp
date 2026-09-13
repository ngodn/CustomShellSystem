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
struct ColorControl {
    std::string id, name;
    bool scalar = false;
    ColorValue value{1,1,1,1};
    float minimum = 0, maximum = 1, step = .01f;
    std::vector<ColorBinding> bindings;
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
    static Customization parse(const nlohmann::json&);
    nlohmann::json json() const;
    bool operator==(const Customization&) const = default;
};
std::map<std::string,ColorValue> color_values(const ColorOptions&, const Customization&);
bool color_resource(const std::string&);
float srgb_linear(float);
}

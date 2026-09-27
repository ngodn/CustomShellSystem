#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace ccs::runtime {
struct MenuLayout {
    static constexpr double design_width = 1920;
    static constexpr double design_height = 1080;
    double scale{}, left{}, top{};

    static std::optional<MenuLayout> fit(double width, double height) {
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return {};
        const auto scale = std::min(width / design_width, height / design_height);
        if (!std::isfinite(scale) || scale <= 0) return {};
        return MenuLayout{scale, (width - design_width * scale) / 2,
            (height - design_height * scale) / 2};
    }
};
}

#include "menu_layout.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

using ccs::runtime::MenuLayout;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double a, double b) { return std::abs(a - b) <= 1e-8 * std::max({1.0, std::abs(a), std::abs(b)}); }
int main() {
    try {
        constexpr std::array<std::array<double, 2>, 9> sizes{{
            {1920,1080}, {1280,720}, {3840,2160}, {2560,1080}, {5120,1440},
            {1920,1200}, {1024,768}, {320,240}, {1080,1920}}};
        for (const auto& size : sizes) {
            const auto frame = MenuLayout::fit(size[0], size[1]);
            check(frame.has_value(), "Usable page rejected");
            const auto right = frame->left + MenuLayout::design_width * frame->scale;
            const auto bottom = frame->top + MenuLayout::design_height * frame->scale;
            check(frame->left >= 0 && frame->top >= 0 && right <= size[0] + 1e-8 && bottom <= size[1] + 1e-8,
                "Design extends outside native page");
            check(close(frame->left, size[0] - right) && close(frame->top, size[1] - bottom), "Design is not centered");
            check(close(right, size[0]) || close(bottom, size[1]), "Fit wastes space on both axes");
            const auto box_width = 200 * frame->scale, box_height = 60 * frame->scale;
            const auto font = 24 * frame->scale;
            check(close(box_width / box_height, 200.0 / 60) && close(box_height / font, 60.0 / 24),
                "Control proportions or font spacing changed with aspect ratio");
        }
        for (const auto bad : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::denorm_min()}) {
            check(!MenuLayout::fit(bad, 1080) && !MenuLayout::fit(1920, bad), "Invalid page accepted");
        }
        std::cout << "Menu layout checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}

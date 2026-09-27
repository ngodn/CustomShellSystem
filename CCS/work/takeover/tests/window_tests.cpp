#include "visible_window.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using ccs::runtime::visible_window;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        for (size_t count = 0; count <= 1024; ++count)
            for (size_t selected = 0; selected <= count; ++selected)
                for (const size_t capacity : {size_t{1}, size_t{8}, size_t{14}}) {
                    const auto window = visible_window(count, selected, capacity);
                    check(window.begin <= window.end && window.end <= count && window.end - window.begin <= capacity,
                          "Library produced an oversized or invalid visible range");
                    if (count) {
                        const auto focus = std::min(selected, count - 1);
                        check(window.begin <= focus && focus < window.end, "Selected row is offscreen");
                        check(window.end - window.begin == std::min(count, capacity), "Visible library has avoidable empty rows");
                    }
                }
        const auto maximum = std::numeric_limits<size_t>::max();
        const auto large = visible_window(maximum, maximum, 14);
        check(large.end == maximum && large.end - large.begin == 14, "Range overflow at a very large library");
        const auto empty = visible_window(100, 50, 0);
        check(empty.begin == 0 && empty.end == 0, "Zero-sized viewport produced rows");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

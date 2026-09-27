#pragma once
#include <algorithm>
#include <cstddef>

namespace ccs::runtime {
struct VisibleWindow { size_t begin{}, end{}; };
inline VisibleWindow visible_window(size_t count, size_t selected, size_t capacity) {
    if (!count || !capacity) return {};
    selected = std::min(selected, count - 1);
    const auto offset = capacity / 2;
    const auto first = std::min(selected > offset ? selected - offset : size_t{0},
                                count > capacity ? count - capacity : size_t{0});
    return {first, first + std::min(capacity, count - first)};
}
}

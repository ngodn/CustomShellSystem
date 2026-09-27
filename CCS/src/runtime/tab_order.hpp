#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace ccs::runtime {
enum class TabRole { Inventory, Tarstones, Map, Css, Ccs, Cssx, Other };

struct TabOrder {
    std::vector<size_t> indices;
    size_t ccs_index{};
};

TabOrder player_menu_order(std::span<const TabRole> roles);
}

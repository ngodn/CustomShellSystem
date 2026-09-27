#include "tab_order.hpp"
#include <array>
#include <stdexcept>

namespace ccs::runtime {
TabOrder player_menu_order(std::span<const TabRole> roles) {
    if (roles.size() < 4 || roles.size() > 64) throw std::runtime_error("Player Menu tab count is unsupported");
    std::array<size_t, 6> found;
    found.fill(roles.size());
    for (size_t i = 0; i < roles.size(); ++i) {
        const auto role = static_cast<size_t>(roles[i]);
        if (role == static_cast<size_t>(TabRole::Other)) continue;
        if (role >= found.size() || found[role] != roles.size()) throw std::runtime_error("Player Menu tab role is ambiguous");
        found[role] = i;
    }
    for (const auto role : {TabRole::Inventory, TabRole::Tarstones, TabRole::Map, TabRole::Ccs})
        if (found[static_cast<size_t>(role)] == roles.size()) throw std::runtime_error("Player Menu required tab is missing");
    TabOrder order;
    order.indices.reserve(roles.size());
    for (size_t role = 0; role < found.size(); ++role) {
        if (found[role] == roles.size()) continue;
        if (role == static_cast<size_t>(TabRole::Ccs)) order.ccs_index = order.indices.size();
        order.indices.push_back(found[role]);
    }
    for (size_t i = 0; i < roles.size(); ++i)
        if (roles[i] == TabRole::Other) order.indices.push_back(i);
    return order;
}
}

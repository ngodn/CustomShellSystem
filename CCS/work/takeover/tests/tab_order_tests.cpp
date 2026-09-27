#include "tab_order.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

using Role = ccs::runtime::TabRole;
void check(bool condition) { if (!condition) throw std::runtime_error("Tab order invariant failed"); }
void permutations(bool css, bool cssx) {
    std::vector<Role> expected{Role::Inventory, Role::Tarstones, Role::Map};
    if (css) expected.push_back(Role::Css);
    expected.push_back(Role::Ccs);
    if (cssx) expected.push_back(Role::Cssx);
    std::vector<size_t> input(expected.size());
    std::iota(input.begin(), input.end(), 0);
    do {
        std::vector<Role> roles;
        for (auto i : input) roles.push_back(expected[i]);
        const auto plan = ccs::runtime::player_menu_order(roles);
        check(plan.indices.size() == expected.size());
        for (size_t i = 0; i < expected.size(); ++i) check(roles[plan.indices[i]] == expected[i]);
        check(plan.ccs_index == (css ? 4u : 3u));
        std::vector<Role> ordered;
        for (auto i : plan.indices) ordered.push_back(roles[i]);
        const auto repeat = ccs::runtime::player_menu_order(ordered);
        for (size_t i = 0; i < repeat.indices.size(); ++i) check(repeat.indices[i] == i);
    } while (std::next_permutation(input.begin(), input.end()));
}
void rejected(std::vector<Role> roles) {
    try { (void)ccs::runtime::player_menu_order(roles); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid tab roles were accepted");
}
int main() {
    permutations(false, false); permutations(true, false);
    permutations(false, true); permutations(true, true);
    const std::vector<Role> foreign{Role::Other, Role::Inventory, Role::Other, Role::Ccs,
        Role::Tarstones, Role::Cssx, Role::Css, Role::Map};
    const auto order = ccs::runtime::player_menu_order(foreign);
    check(order.indices == std::vector<size_t>({1, 4, 7, 6, 3, 5, 0, 2}));
    rejected({Role::Inventory, Role::Tarstones, Role::Map});
    rejected({Role::Inventory, Role::Tarstones, Role::Map, Role::Other});
    rejected({Role::Inventory, Role::Tarstones, Role::Map, Role::Ccs, Role::Ccs});
    rejected({Role::Inventory, Role::Tarstones, Role::Map, Role::Ccs, Role::Css, Role::Css});
    rejected({Role::Inventory, Role::Tarstones, Role::Map, Role::Ccs, static_cast<Role>(255)});
    rejected(std::vector<Role>(65, Role::Other));
}

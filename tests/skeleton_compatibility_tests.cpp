#include "skeleton_compatibility.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    using css::compatible_standard_skeletons;
    const std::string human = "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    const std::string base = "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
    auto check = [](bool value, const char* why) { if (!value) throw std::runtime_error(why); };
    check(compatible_standard_skeletons(human, base), "Audited human-to-base pair rejected");
    check(compatible_standard_skeletons(base, human), "Restoring human from base rejected");
    for (const auto& other : std::vector<std::string>{"", "/Game/Creature.Beast", base + "_Invalid",
                                   "/Game/Other/SKEL_CSS_Base.SKEL_CSS_Base",
                                   "/Game/Other/SKEL_Human_Skeleton.SKEL_Human_Skeleton"}) {
        check(!compatible_standard_skeletons(other, base), "Unknown source rig accepted");
        check(!compatible_standard_skeletons(human, other), "Unknown target rig accepted");
    }
    check(!compatible_standard_skeletons("", ""), "Missing skeletons accepted");
    std::cout << "Skeleton allowlist and lookalike rejection passed\n";
}

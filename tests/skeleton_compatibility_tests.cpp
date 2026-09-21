#include "skeleton_compatibility.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    using css::compatible_standard_skeletons;
    const std::string human = "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    const std::string base = "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
    const std::string b2 = "/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2";
    const std::string standard = "/Game/CSS/Shared/SKEL_Base.SKEL_Base";
    auto check = [](bool value, const char* why) { if (!value) throw std::runtime_error(why); };
    check(compatible_standard_skeletons(human, base), "Audited human-to-base pair rejected");
    check(compatible_standard_skeletons(base, human), "Restoring human from base rejected");
    for (const auto& previous : {human, base}) {
        check(compatible_standard_skeletons(previous, b2), "Audited V44 B2 target rejected");
        check(compatible_standard_skeletons(b2, previous), "Restoring from V44 B2 rejected");
    }
    for (const auto& previous : {human, base, b2}) {
        check(compatible_standard_skeletons(previous, standard), "Audited short-path target rejected");
        check(compatible_standard_skeletons(standard, previous), "Restoring from short-path rig rejected");
    }
    for (const auto& other : std::vector<std::string>{"", "/Game/Creature.Beast", base + "_Invalid",
                                   "/Game/Other/SKEL_CSS_Base.SKEL_CSS_Base",
                                   "/Game/Other/SKEL_Human_Skeleton.SKEL_Human_Skeleton", b2 + "_Invalid",
                                   "/Game/Other/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2",
                                   standard + "_Invalid", "/Game/Other/SKEL_Base.SKEL_Base",
                                   "/Game/CSS/Reference/SKEL_Source.SKEL_Source",
                                   "/Game/CSS/SeduXtress/Reference/SKEL_Hands.SKEL_Hands"}) {
        check(!compatible_standard_skeletons(other, base), "Unknown source rig accepted");
        check(!compatible_standard_skeletons(human, other), "Unknown target rig accepted");
        check(!compatible_standard_skeletons(other, b2), "Unknown source accepted for V44");
        check(!compatible_standard_skeletons(b2, other), "Unknown target accepted from V44");
        check(!compatible_standard_skeletons(other, standard), "Unknown source accepted for short-path rig");
        check(!compatible_standard_skeletons(standard, other), "Unknown target accepted from short-path rig");
    }
    check(!compatible_standard_skeletons(standard, standard), "Identical skeleton treated as a remap");
    check(!compatible_standard_skeletons("", ""), "Missing skeletons accepted");
    std::cout << "Skeleton allowlist and lookalike rejection passed\n";
}

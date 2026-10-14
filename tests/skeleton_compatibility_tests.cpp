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
    const std::string wings = "/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
    const std::string authored = "/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base.SKEL_Base";
    const std::string authored_wings = "/Game/CSS/eins0fx/UnholyGenessa/Skeletons/SKEL_Wings2.SKEL_Wings2";
    const std::string cw = "/Game/CSS/eins0fx/CommanderWhite/Skeletons/SKEL_Strands1.SKEL_Strands1";
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
    for (const auto& previous : {human, base, b2, standard}) {
        check(compatible_standard_skeletons(previous, wings), "Audited Genessa wing rig rejected");
        check(compatible_standard_skeletons(wings, previous), "Restoring from Genessa wing rig rejected");
    }
    for (const auto& other : std::vector<std::string>{"", "/Game/Creature.Beast", base + "_Invalid",
                                   "/Game/Other/SKEL_CSS_Base.SKEL_CSS_Base",
                                   "/Game/Other/SKEL_Human_Skeleton.SKEL_Human_Skeleton", b2 + "_Invalid",
                                   "/Game/Other/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2",
                                   standard + "_Invalid", "/Game/Other/SKEL_Base.SKEL_Base",
                                   "/Game/CSS/Reference/SKEL_Source.SKEL_Source",
                                   "/Game/CSS/SeduXtress/Reference/SKEL_Hands.SKEL_Hands",
                                   wings + "_Invalid", "/Game/Other/SKEL_Wings2.SKEL_Wings2",
                                   "/Game/CSS/UnholyGenessa/SKEL_Wings1.SKEL_Wings1"}) {
        check(!compatible_standard_skeletons(other, base), "Unknown source rig accepted");
        check(!compatible_standard_skeletons(human, other), "Unknown target rig accepted");
        check(!compatible_standard_skeletons(other, b2), "Unknown source accepted for V44");
        check(!compatible_standard_skeletons(b2, other), "Unknown target accepted from V44");
        check(!compatible_standard_skeletons(other, standard), "Unknown source accepted for short-path rig");
        check(!compatible_standard_skeletons(standard, other), "Unknown target accepted from short-path rig");
        check(!compatible_standard_skeletons(other, wings), "Unknown source accepted for wing rig");
        check(!compatible_standard_skeletons(wings, other), "Unknown target accepted from wing rig");
    }
    check(!compatible_standard_skeletons(standard, standard), "Identical skeleton treated as a remap");
    check(!compatible_standard_skeletons(wings, wings), "Identical wing skeleton treated as a remap");
    check(!compatible_standard_skeletons("", ""), "Missing skeletons accepted");
    check(css::compatible_animation_skeletons(standard, standard), "Same-rig movement rejected");
    check(css::compatible_animation_skeletons(wings, wings), "Same wing-rig movement rejected");
    check(css::compatible_animation_skeletons(standard, wings), "Audited base movement rejected on wings");
    check(!css::compatible_animation_skeletons(wings, standard), "Unaudited reverse movement remap accepted");
    for (const auto& other : std::vector<std::string>{"", human, base, b2, wings + "_Invalid",
                                  "/Game/Other/SKEL_Base.SKEL_Base"}) {
        check(!css::compatible_animation_skeletons(other, wings), "Unaudited animation source accepted");
        check(!css::compatible_animation_skeletons(standard, other), "Unaudited animation target accepted");
    }
    check(!css::compatible_animation_skeletons("", ""), "Missing animation skeletons accepted");
    for (const auto& previous : {human, base, b2, standard, wings}) {
        check(compatible_standard_skeletons(previous, authored), "Renamed base appearance rejected");
        check(compatible_standard_skeletons(authored, previous), "Restoring from renamed base rejected");
    }
    check(!compatible_standard_skeletons(authored, authored), "Identical authored rig treated as a remap");
    for (const auto& from : {standard, authored}) {
        for (const auto& to : {standard, authored, wings, authored_wings})
            check(css::compatible_animation_skeletons(from, to), "Verified base alias lost movement support");
        for (const auto& wing : {wings, authored_wings})
            check(!css::compatible_animation_skeletons(wing, from), "Wing-only movement accepted on base rig");
    }
    for (const auto& previous : {human, base, b2, standard, authored, wings}) {
        check(compatible_standard_skeletons(previous, authored_wings), "Migrated wing appearance rejected");
        check(compatible_standard_skeletons(authored_wings, previous), "Restoring from migrated wings rejected");
    }
    check(css::compatible_animation_skeletons(wings, authored_wings), "Legacy wing animation rejected on migrated wings");
    check(css::compatible_animation_skeletons(authored_wings, wings), "Migrated wing animation rejected on legacy wings");
    check(!compatible_standard_skeletons(authored_wings, authored_wings), "Identical migrated wings treated as remap");
    for (const auto& other : std::vector<std::string>{
            "", authored + "_Invalid", "/Game/CSS/another_author/Shared/Skeletons/SKEL_Base.SKEL_Base",
            "/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base.Other",
            "/Game/CSS/eins0fx/Shared/SkeletonsExtra/SKEL_Base.SKEL_Base",
            authored_wings + "_Invalid",
            "/Game/CSS/another_author/UnholyGenessa/Skeletons/SKEL_Wings2.SKEL_Wings2"}) {
        check(!compatible_standard_skeletons(other, authored), "Unaudited source accepted for renamed base");
        check(!compatible_standard_skeletons(authored, other), "Unaudited target accepted from renamed base");
        check(!css::compatible_animation_skeletons(other, authored), "Unaudited movement source accepted");
        check(!css::compatible_animation_skeletons(authored, other), "Unaudited movement target accepted");
        check(!compatible_standard_skeletons(other, authored_wings), "Unaudited source accepted for migrated wings");
        check(!css::compatible_animation_skeletons(other, authored_wings), "Unaudited movement source accepted for migrated wings");
        check(!css::compatible_animation_skeletons(authored_wings, other), "Unaudited wing animation target accepted");
    }
    for (const auto& previous : {human, base, b2, standard, authored, wings, authored_wings}) {
        check(compatible_standard_skeletons(previous, cw), "CW hair appearance rejected");
        check(compatible_standard_skeletons(cw, previous), "Restoring from CW hair rejected");
    }
    check(!compatible_standard_skeletons(cw, cw), "Identical CW skeleton treated as remap");
    check(css::compatible_animation_skeletons(cw, cw), "Same CW skeleton animation rejected");
    for (const auto& source : {standard, authored}) {
        check(css::compatible_animation_skeletons(source, cw), "Base animation rejected on CW hair");
        check(!css::equivalent_animation_skeletons(source, cw), "CW extension incorrectly treated as alias");
        check(!css::compatible_animation_skeletons(cw, source), "Reverse CW animation remap accepted");
    }
    for (const auto& source : {wings, authored_wings, human, base, b2}) {
        check(!css::compatible_animation_skeletons(source, cw), "Unaudited animation remap into CW");
        check(!css::compatible_animation_skeletons(cw, source), "Unaudited animation remap from CW");
    }
    for (const auto& other : std::vector<std::string>{"", cw + "_Invalid",
             "/Game/CSS/another_author/CommanderWhite/Skeletons/SKEL_Strands1.SKEL_Strands1",
             "/Game/CSS/eins0fx/CommanderWhite/Studies/HairStrand1/SKEL_Strands1.SKEL_Strands1",
             "/Game/CSS/eins0fx/CommanderWhite/Skeletons/SKEL_Strands1.Other"}) {
        check(!compatible_standard_skeletons(other, cw), "Unknown source accepted for CW");
        check(!compatible_standard_skeletons(cw, other), "Unknown target accepted from CW");
        check(!css::compatible_animation_skeletons(other, cw), "Unknown animation accepted for CW");
        check(!css::compatible_animation_skeletons(cw, other), "CW animation accepted on unknown rig");
    }
    const std::string belt(css::authored_cw_belt_skeleton);
    for (const auto& previous : {human, base, b2, standard, authored, wings, authored_wings, cw}) {
        check(compatible_standard_skeletons(previous, belt), "CW belt appearance rejected");
        check(compatible_standard_skeletons(belt, previous), "Restoring from CW belt rejected");
    }
    for (const auto& source : {standard, authored, cw}) {
        check(css::compatible_animation_skeletons(source, belt), "Audited prefix animation rejected on belt rig");
        check(!css::compatible_animation_skeletons(belt, source), "Belt-only animation accepted on smaller rig");
    }
    check(css::compatible_animation_skeletons(belt, belt), "Same belt rig animation rejected");
    check(!compatible_standard_skeletons(belt, belt), "Identical belt rig treated as remap");
    for (const auto& other : std::vector<std::string>{"", human, wings, authored_wings, belt + "_Invalid",
            "/Game/CSS/eins0fx/CommanderWhite/Studies/BeltWaist1/SKEL_Waist1.SKEL_Waist1",
            "/Game/CSS/another_author/CommanderWhite/Skeletons/SKEL_Waist1.SKEL_Waist1"}) {
        check(!css::compatible_animation_skeletons(other, belt), "Unaudited animation source accepted for belt");
        check(!css::compatible_animation_skeletons(belt, other), "Belt animation accepted on unaudited target");
    }
    std::cout << "Skeleton allowlist and lookalike rejection passed\n";
}

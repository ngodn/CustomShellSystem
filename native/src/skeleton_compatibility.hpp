#pragma once
#include <string_view>

namespace css {
inline constexpr std::string_view legacy_base_skeleton = "/Game/CSS/Shared/SKEL_Base.SKEL_Base";
inline constexpr std::string_view authored_base_skeleton =
    "/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base.SKEL_Base";
inline constexpr std::string_view legacy_wing_skeleton = "/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
inline constexpr std::string_view authored_wing_skeleton =
    "/Game/CSS/eins0fx/UnholyGenessa/Skeletons/SKEL_Wings2.SKEL_Wings2";
inline constexpr std::string_view authored_cw_hair_skeleton =
    "/Game/CSS/eins0fx/CommanderWhite/Skeletons/SKEL_Strands1.SKEL_Strands1";

inline bool is_audited_base_skeleton(std::string_view path) {
    // The namespace cook preserves every decoded Skeleton field. See
    // docs/development/three-outfit-rig-physics-status.md.
    return path == legacy_base_skeleton || path == authored_base_skeleton;
}

inline bool is_audited_wing_skeleton(std::string_view path) {
    return path == legacy_wing_skeleton || path == authored_wing_skeleton;
}

inline bool equivalent_animation_skeletons(std::string_view source, std::string_view target) {
    return (!source.empty() && source == target) ||
        (is_audited_base_skeleton(source) && is_audited_base_skeleton(target)) ||
        (is_audited_wing_skeleton(source) && is_audited_wing_skeleton(target));
}

inline bool compatible_animation_skeletons(std::string_view source, std::string_view target) {
    // Base motion maps into the audited extensions. Their extra wing/hair
    // tracks must not be accepted in reverse or across the two extensions.
    return equivalent_animation_skeletons(source, target) ||
        (is_audited_base_skeleton(source) &&
         (is_audited_wing_skeleton(target) || target == authored_cw_hair_skeleton));
}

// Different skeleton objects are permitted only for the audited human, CSS
// base, V44 B2, Genessa wing and CW strand references. See
// docs/development/{b2-reference-binding,short-rig-paths,genessa-wing-rig}.md.
// CW preserves the shared raw prefix, sockets and virtual definitions; its
// saved/cooked and component evidence is linked in three-outfit-rig-physics-status.md.
// This allowlist is not a structural validator for arbitrary mod rigs.
inline bool compatible_standard_skeletons(std::string_view before, std::string_view target) {
    constexpr std::string_view human =
        "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    constexpr std::string_view base =
        "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
    constexpr std::string_view b2 =
        "/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2";
    const auto audited = [&](std::string_view path) {
        return path == human || path == base || path == b2 || is_audited_base_skeleton(path) ||
            is_audited_wing_skeleton(path) || path == authored_cw_hair_skeleton;
    };
    return before != target && audited(before) && audited(target);
}
}

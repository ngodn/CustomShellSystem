#pragma once
#include <string_view>

namespace css {
inline constexpr std::string_view legacy_base_skeleton = "/Game/CSS/Shared/SKEL_Base.SKEL_Base";
inline constexpr std::string_view authored_base_skeleton =
    "/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base.SKEL_Base";

inline bool is_audited_base_skeleton(std::string_view path) {
    // The namespace cook preserves every decoded Skeleton field. See
    // docs/development/three-outfit-rig-physics-status.md.
    return path == legacy_base_skeleton || path == authored_base_skeleton;
}

inline bool compatible_animation_skeletons(std::string_view source, std::string_view target) {
    // Only this directed remap has been audited for the appended wing bones.
    // See docs/development/genessa-wing-rig.md.
    constexpr std::string_view wings = "/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
    return (!source.empty() && source == target) ||
        (is_audited_base_skeleton(source) && (is_audited_base_skeleton(target) || target == wings));
}

// Different skeleton objects are permitted only for the audited human, CSS
// base, V44 B2 and Genessa wing references, including the short-path copy. See
// docs/development/{b2-reference-binding,short-rig-paths,genessa-wing-rig}.md.
// This allowlist is not a structural validator for arbitrary mod rigs.
inline bool compatible_standard_skeletons(std::string_view before, std::string_view target) {
    constexpr std::string_view human =
        "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    constexpr std::string_view base =
        "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
    constexpr std::string_view b2 =
        "/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2";
    constexpr std::string_view wings = "/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
    const auto audited = [&](std::string_view path) {
        return path == human || path == base || path == b2 || is_audited_base_skeleton(path) || path == wings;
    };
    return before != target && audited(before) && audited(target);
}
}

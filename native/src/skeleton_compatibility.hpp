#pragma once
#include <string_view>

namespace css {
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
    constexpr std::string_view standard = "/Game/CSS/Shared/SKEL_Base.SKEL_Base";
    constexpr std::string_view wings = "/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2";
    const auto audited = [&](std::string_view path) {
        return path == human || path == base || path == b2 || path == standard || path == wings;
    };
    return before != target && audited(before) && audited(target);
}
}

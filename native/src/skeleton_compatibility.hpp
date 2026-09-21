#pragma once
#include <string_view>

namespace css {
// Different skeleton objects are permitted only for the audited human, CSS
// base and V44 B2 references, including the audited short-path copy. See
// docs/development/b2-reference-binding.md and short-rig-paths.md.
// This allowlist is not a structural validator for arbitrary mod rigs.
inline bool compatible_standard_skeletons(std::string_view before, std::string_view target) {
    constexpr std::string_view human =
        "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    constexpr std::string_view base =
        "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
    constexpr std::string_view b2 =
        "/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2";
    constexpr std::string_view standard = "/Game/CSS/Shared/SKEL_Base.SKEL_Base";
    const auto audited = [&](std::string_view path) {
        return path == human || path == base || path == b2 || path == standard;
    };
    return before != target && audited(before) && audited(target);
}
}

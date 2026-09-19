#pragma once
#include <string_view>

namespace css {
// Different skeleton objects are permitted only for the audited human/base
// pair. This allowlist is not a structural validator for arbitrary mod rigs.
inline bool compatible_standard_skeletons(std::string_view before, std::string_view target) {
    constexpr std::string_view human =
        "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
    constexpr std::string_view base =
        "/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base.SKEL_CSS_Base";
    return (before == human && target == base) || (before == base && target == human);
}
}

#pragma once
#include <optional>
#include <string_view>

namespace css {
struct OriginalShadeDefinition { std::string_view suffix, fallback_label; };
inline std::optional<OriginalShadeDefinition> original_shade_definition(std::string_view tag) {
    if(tag=="Sparta.Item.Skin.Shell.Shared.Alternative") return OriginalShadeDefinition{"AltSkin","Alternative"};
    constexpr std::string_view prefix="Sparta.Item.Skin.Shell.Shared.";
    if(!tag.starts_with(prefix)) return std::nullopt;
    const auto suffix=tag.substr(prefix.size());
    if(suffix.empty() || suffix.size()>64) return std::nullopt;
    for(const char c:suffix)
        if(!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return std::nullopt;
    return OriginalShadeDefinition{suffix,suffix};
}
}

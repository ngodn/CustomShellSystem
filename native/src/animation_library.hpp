#pragma once
#include "animations.hpp"

namespace css {
enum class AnimationPlayback { Loop, Once, Pose };
struct LibraryAnimationAsset {
    std::string asset, skeleton;
};
struct LegacyAnimationAlias {
    AnimationSlot slot;
    std::string id, asset;
};
struct LibraryAnimation {
    std::string id, name, category;
    AnimationPlayback playback = AnimationPlayback::Once;
    std::optional<AnimationSlot> slot;
    bool hide_weapons = false;
    std::vector<LibraryAnimationAsset> assets;
    // Matching an ID alone is insufficient: an outfit may use a specialized clip.
    std::vector<LegacyAnimationAlias> aliases;
};
struct AnimationLibrary {
    std::string id, name, author, version;
    std::vector<LibraryAnimation> entries;
    static AnimationLibrary parse(const nlohmann::json&);
};
}

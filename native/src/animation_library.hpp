#pragma once
#include <cstdint>
#include "animations.hpp"

namespace css {
struct AnimationContainerRevision {
    std::string toc_sha256;
    uint64_t bulk_bytes = 0;
    bool operator==(const AnimationContainerRevision&) const = default;
};
struct PerformanceAdapter {
    std::string source_outfit, source_class, target_class, skeleton;
    AnimationContainerRevision source_revision;
};
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
    std::vector<PerformanceAdapter> adapters;
    static AnimationLibrary parse(const nlohmann::json&);
};
const PerformanceAdapter* resolve_performance_adapter(const std::vector<AnimationLibrary>&,
    const std::string& outfit,const AnimationContainerRevision&,
    const std::string& source_class,const std::string& skeleton);
std::vector<AnimationOption> merge_library_animation_options(const std::vector<AnimationLibrary>&,
    AnimationSlot,const std::vector<AnimationOption>& bundled);
struct AnimationAssetCandidate {
    std::string asset;
    // Empty for bundled fallbacks; their actual skeleton must be read after loading.
    std::string declared_skeleton;
    bool bundled = false;
};
struct AnimationLibraryMatch {
    const LibraryAnimation* entry = nullptr;
    std::vector<AnimationAssetCandidate> candidates;
};
// Metadata resolution at selection/catalog change, not per frame. The caller must
// validate loaded types/skeletons and cache failures before starting playback.
AnimationLibraryMatch resolve_library_animation(const std::vector<AnimationLibrary>&,
    AnimationSlot,const std::string& choice,const std::vector<AnimationOption>& bundled,
    const std::string& target_skeleton,const std::string& weapon_tag = {});
// Explicit performances use canonical library IDs, never outfit aliases or raw paths.
AnimationLibraryMatch resolve_library_performance(const std::vector<AnimationLibrary>&,
    const std::string& choice,const std::string& target_skeleton);
}

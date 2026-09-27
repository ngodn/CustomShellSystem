#include "rig.hpp"

namespace ccs::rig {
using namespace engine;
std::string short_name(const std::string& object_path) {
    const auto dot = object_path.rfind('.');
    return dot == std::string::npos ? object_path : object_path.substr(dot + 1);
}
std::string player_skeleton(const PlayerContext& player) {
    if (!player.pawn) return {};
    auto* mesh = object_of(player.pawn, L"Mesh");
    auto* asset = mesh ? object_of(mesh, L"SkinnedAsset") : nullptr;   // USkinnedMeshComponent::SkinnedAsset (5.1+)
    auto* skeleton = asset ? object_of(asset, L"Skeleton") : nullptr;
    return skeleton ? narrow(skeleton->GetPathName()) : std::string{};
}
bool compatible(const std::string& skeleton_path, const std::string& player_skeleton) {
    if (skeleton_path.empty()) return false;
    if (short_name(skeleton_path) == "SKEL_Human_Skeleton") return true;      // the game's own human rig
    if (skeleton_path.starts_with("/Game/CSS/")) return true;                  // a CSS package's rig, built for the player's body
    return !player_skeleton.empty() && skeleton_path == player_skeleton;       // the body worn right now, whatever put it there
}
}

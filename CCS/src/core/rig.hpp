#pragma once
// Which skeletons a montage may be authored on and still play on the player. The game's bodies
// and every CSS body share the human bone hierarchy: the game's SKEL_Human_Skeleton, the copies
// CSS packages cook under /Game/CSS/ (SKEL_Base, SKEL_CSS_Base, ...) and whatever rig the body
// the player wears right now references. Creature rigs (spider, golem, The Coffin, ...) share
// too few bones and are refused before anything is loaded.
#include "engine.hpp"
#include <string>

namespace ccs::rig {
// Object path of the skeleton behind the player's current mesh, empty when it cannot be read.
std::string player_skeleton(const engine::PlayerContext& player);
// `skeleton_path` is an object path such as /Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton.
bool compatible(const std::string& skeleton_path, const std::string& player_skeleton);
// Whether a pawn on this skeleton can play human-rig montages at all (the game's human rig or a CSS copy).
// The Harbinger form and creature shells are not; swapping their attacks feeds garbage poses to the GPU.
bool humanoid(const std::string& skeleton_path);
// Short asset name of an object path, for messages.
std::string short_name(const std::string& object_path);
}

#pragma once
// Which skeletons a montage may be authored on and still play on the player. The game's bodies
// and every CSS body share the human bone hierarchy: the game's SKEL_Human_Skeleton, the copies
// CSS packages cook under /Game/CSS/ (SKEL_Base, SKEL_CSS_Base, ...) and whatever rig the body
// the player wears right now references. On top of those, the audited boss and enemy rigs listed
// in enemy-catalog.json ("retarget_rigs"): their bones carry the human names, and the engine
// retargets a montage onto the player's skeleton by bone name at runtime (UE 5.2+ has no runtime
// compatibility gate). Root motion is not retargeted, so each listed rig carries a travel scale.
// Creature rigs are refused before anything is loaded (docs/enemy-moves-retargeting.md).
#include "engine.hpp"
#include "ccs_types.hpp"
#include <string>
#include <vector>

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
// The retargeted rigs, set once from the enemy catalog when the core starts.
void set_retarget_rigs(std::vector<RetargetRig> rigs);
// The listed rig a montage skeleton is, or null for the human family and anything unlisted.
const RetargetRig* retargeted(const std::string& skeleton_path);
// How far a montage on `skeleton_path` should carry the player: the rig's travel scale, or 1 when
// the body worn is that rig itself (a CSS boss appearance, whose mesh scale already sizes travel).
double travel_scale(const std::string& skeleton_path, const std::string& player_skeleton);
}

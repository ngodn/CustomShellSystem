#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The game's foot IK (CTRL_Human_FullbodyIK on ABP_Player) solves with the leg lengths authored
// in the rig asset unless its node takes its initial transforms from the worn mesh
// (AnimNode_ControlRig::bSetRefPoseFromSkeleton, "Set Initial Transforms From Mesh"). A body with
// other leg proportions can have its thigh and calf stretched by the solver every step. CSS turns
// the flag on only for outfits listed in catalog/native-foot-ik.json, only where the mismatch
// exists, and only for bodies on a CSS skeleton: stock shells, NPC and enemy looks and
// replacement mods all stay on the game's skeleton and are never touched. The game's own shells
// already differ from the rig by 2 to 6 cm and walk fine, so a difference alone is no reason.
namespace css::native_foot_ik {
inline constexpr std::string_view rig_class=
    "/Game/Sparta/Characters/Shared/Rigs/CTRL_Human_FullbodyIK.CTRL_Human_FullbodyIK_C";
inline constexpr std::string_view game_skeleton=
    "/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton";
// Child bones whose local translation length is the length of the segment above them:
// thigh (calf), calf (foot) and foot (ball), both sides.
inline constexpr std::array<const wchar_t*,6> segment_bones={L"calf_l",L"foot_l",L"ball_l",L"calf_r",L"foot_r",L"ball_r"};
// Half a centimetre: below that the solver's stretch is invisible and the rig stays as authored.
inline constexpr double threshold_cm=.5;

using Lengths=std::array<std::optional<double>,6>;

// Largest segment difference in cm over the segments both sides know, or nothing when no
// segment can be compared (then the rig is left alone).
inline std::optional<double> largest_difference(const Lengths& rig,const Lengths& mesh) {
    std::optional<double> worst;
    for(size_t i=0;i<rig.size();++i) {
        if(!rig[i] || !mesh[i] || !std::isfinite(*rig[i]) || !std::isfinite(*mesh[i])) continue;
        const double difference=std::abs(*rig[i]-*mesh[i]);
        worst=std::max(worst.value_or(0.),difference);
    }
    return worst;
}

// Outfits opt in through catalog/native-foot-ik.json shipped with CSS. Everything not listed keeps
// the game rig exactly as 1.0.2 did, whatever its proportions.
inline std::set<std::string> opt_in(const std::vector<std::string>& ids) {
    if(ids.size()>256) throw std::runtime_error("Too many foot IK outfits");
    std::set<std::string> out;
    for(const auto& id:ids) {
        if(id.empty() || id.size()>96 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=std::string::npos)
            throw std::runtime_error("Invalid foot IK outfit id");
        out.insert(id);
    }
    return out;
}

// Whether the worn body needs the rig to take its lengths from the mesh.
inline bool needs_mesh_lengths(std::string_view skeleton,const Lengths& rig,const Lengths& mesh) {
    if(skeleton.empty() || skeleton==game_skeleton) return false;
    const auto worst=largest_difference(rig,mesh);
    return worst && *worst>threshold_cm;
}
}

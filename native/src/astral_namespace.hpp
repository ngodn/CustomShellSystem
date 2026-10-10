#pragma once
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace css {
inline constexpr std::string_view legacy_astral_pose =
    "/Game/CSS/SharedAssets/Astral/ABP_CopyPose.ABP_CopyPose_C";
inline constexpr std::string_view authored_astral_pose =
    "/Game/CSS/eins0fx/Shared/Astral/ABP_CopyPose.ABP_CopyPose_C";
inline constexpr std::string_view legacy_astral_root = "/Game/CSS/SharedAssets/Astral/";
inline constexpr std::string_view authored_astral_root = "/Game/CSS/eins0fx/Shared/Astral/";

// Path aliases do not bypass the source graph GUID or material-state checks.
inline constexpr std::array<std::pair<std::string_view,std::string_view>,8> astral_root_aliases{{
    {"/Game/CSS/UnholyGenessa/Mat1/M_Trim.M_Trim", "/Game/CSS/eins0fx/UnholyGenessa/Mat1/M_Trim.M_Trim"},
    {"/Game/CSS/UnholyGenessa/Mat10/M_Trim.M_Trim", "/Game/CSS/eins0fx/UnholyGenessa/Mat10/M_Trim.M_Trim"},
    {"/Game/CSS/UnholyGenessa/Mat12/M_Metal.M_Metal", "/Game/CSS/eins0fx/UnholyGenessa/Mat12/M_Metal.M_Metal"},
    {"/Game/CSS/UnholyGenessa/Fabric02/M_Fabric.M_Fabric", "/Game/CSS/eins0fx/UnholyGenessa/Fabric02/M_Fabric.M_Fabric"},
    {"/Game/CSS/UnholyGenessa/Fabric02/M_Silk.M_Silk", "/Game/CSS/eins0fx/UnholyGenessa/Fabric02/M_Silk.M_Silk"},
    {"/Game/CSS/EveHair/M_Hair1.M_Hair1", "/Game/CSS/eins0fx/EveSB/Hair/M_Hair1.M_Hair1"},
    {"/Game/CSS/CommanderWhite/MaterialY/M_Hair.M_Hair", "/Game/CSS/eins0fx/CommanderWhite/MaterialY/M_Hair.M_Hair"},
    {"/Game/CSS/Locomotion/Faces/Optical1/M_Optical.M_Optical", "/Game/CSS/eins0fx/CommanderWhite/Faces/Optical/M_Optical.M_Optical"}
}};

inline bool astral_material_root_matches(std::string_view catalog_root,std::string_view actual) {
    if(catalog_root==actual) return true;
    for(const auto& [legacy,authored]:astral_root_aliases)
        if(catalog_root==legacy && actual==authored) return true;
    return false;
}

inline bool is_astral_pose(std::string_view path) {
    return path==legacy_astral_pose || path==authored_astral_pose;
}

inline bool astral_fallback_texture(std::string_view path) {
    return path.starts_with("/Game/CSS/SharedAssets/") || path.starts_with(authored_astral_root);
}

inline std::string astral_companion_namespace(std::string_view companion,std::string_view pose) {
    if(!is_astral_pose(pose)) return {};
    if(companion.starts_with(authored_astral_root))
        return pose==authored_astral_pose ? std::string(companion) : std::string{};
    if(!companion.starts_with(legacy_astral_root)) return {};
    if(pose==legacy_astral_pose) return std::string(companion);
    return std::string(authored_astral_root)+std::string(companion.substr(legacy_astral_root.size()));
}
}

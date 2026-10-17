#pragma once
#include <string_view>

namespace css {
inline constexpr std::string_view vista_rest_montage =
    "/Game/Sparta/Characters/Shells/_Shared/Animation/Actions/Vista_Sit/"
    "A_Shared_Actions_Vista_Sit_Loop_Montage.A_Shared_Actions_Vista_Sit_Loop_Montage";
constexpr bool is_vista_rest_loop(std::string_view path) {
    return path==vista_rest_montage;
}
inline constexpr const char* vista_feminine_idle =
    "/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Locomotion/"
    "A_CultistSpearLady_Locomotion_Idle_02.A_CultistSpearLady_Locomotion_Idle_02";
}

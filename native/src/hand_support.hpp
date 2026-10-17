#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <string_view>

namespace css::hand_support {
enum class Family { none, eve, unholy, commander };
enum class Variant { none, peony, black_pearl, unholy_eve, unholy_genessa, commander };
struct Binding {
    Family family;
    Variant variant;
    std::string_view mesh, animation, original, candidate;
};
inline constexpr std::string_view eve_animation = "/Game/CSS/eins0fx/EveSB/ABP_Secondary.ABP_Secondary_C";
inline constexpr std::string_view unholy_animation = "/Game/CSS/eins0fx/UnholyGenessa/ABP_Wings2.ABP_Wings2_C";
inline constexpr std::string_view eve_original = "/Game/CSS/eins0fx/EveSB/CR_Hand.CR_Hand_C";
inline constexpr std::string_view unholy_original = "/Game/CSS/eins0fx/UnholyGenessa/CR_Hand.CR_Hand_C";
inline constexpr std::string_view commander_original = "/Game/CSS/eins0fx/CommanderWhite/Rig/CR_BodyB3.CR_BodyB3_C";
inline constexpr std::string_view eve_candidate = "/Game/CSS/Runtime/Hands/CR_EveShoulderSupport.CR_EveShoulderSupport_C";
inline constexpr std::string_view unholy_candidate = "/Game/CSS/Runtime/Hands/CR_UnholyShoulderSupport.CR_UnholyShoulderSupport_C";
inline constexpr std::string_view commander_candidate = "/Game/CSS/Runtime/Hands/CR_CommanderBodyShoulderSupport.CR_CommanderBodyShoulderSupport_C";
inline constexpr Binding bindings[] = {
    {Family::eve, Variant::peony, "/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2.SK_PeonyV2", eve_animation, eve_original, eve_candidate},
    {Family::eve, Variant::black_pearl, "/Game/CSS/eins0fx/EveSB/SK_BlackPearl2.SK_BlackPearl2", eve_animation, eve_original, eve_candidate},
    {Family::unholy, Variant::unholy_eve, "/Game/CSS/eins0fx/UnholyGenessa/SK_EveW3.SK_EveW3", unholy_animation, unholy_original, unholy_candidate},
    {Family::unholy, Variant::unholy_genessa, "/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_GenessaW3_Face.SK_GenessaW3_Face", unholy_animation, unholy_original, unholy_candidate},
    {Family::unholy, Variant::unholy_genessa, "/Game/CSS/eins0fx/UnholyGenessa/SK_GenessaW3.SK_GenessaW3", unholy_animation, unholy_original, unholy_candidate},
    {Family::commander, Variant::commander, "/Game/CSS/eins0fx/CommanderWhite/Faces/SK_Facial20.SK_Facial20",
     "/Game/CSS/eins0fx/CommanderWhite/Rig/ABP_FacialBelt20.ABP_FacialBelt20_C", commander_original, commander_candidate},
    {Family::commander, Variant::commander, "/Game/CSS/eins0fx/CommanderWhite/Meshes/SK_Normal20.SK_Normal20",
     "/Game/CSS/eins0fx/CommanderWhite/Rig/ABP_NormalBelt20.ABP_NormalBelt20_C", commander_original, commander_candidate},
};
inline const Binding* match(std::string_view mesh, std::string_view animation) {
    for (const auto& binding : bindings)
        if (mesh == binding.mesh && animation == binding.animation) return &binding;
    return nullptr;
}
inline constexpr std::array<std::string_view, 19> right_bones = {
    "pinky_metacarpal_r", "pinky_01_r", "pinky_02_r", "pinky_03_r",
    "ring_metacarpal_r", "ring_01_r", "ring_02_r", "ring_03_r",
    "middle_metacarpal_r", "middle_01_r", "middle_02_r", "middle_03_r",
    "index_metacarpal_r", "index_01_r", "index_02_r", "index_03_r",
    "thumb_01_r", "thumb_02_r", "thumb_03_r",
};
inline constexpr std::array<std::string_view, 4> arm_bones = {
    "upperarm_r", "lowerarm_r", "hand_r", "prop_r",
};
enum class Weapon { none, scythe, hadern, needle, hammer, battle_axe };
inline Weapon weapon_class(std::string_view path) {
    constexpr std::string_view paths[] = {
        "/Game/Sparta/Core/Weapons/Player/ClockworkScythe/WP_ClockworkScythe.WP_ClockworkScythe_C",
        "/Game/Sparta/Core/Weapons/Player/HadernsSword/WP_HadernsSword.WP_HadernsSword_C",
        "/Game/Sparta/Core/Weapons/Player/BlackNeedle/WP_BlackNeedle.WP_BlackNeedle_C",
        "/Game/Sparta/Core/Weapons/Player/HeavyHammer/WP_HeavyHammer.WP_HeavyHammer_C",
        "/Game/Sparta/Core/Weapons/Player/BattleAxe/WP_BattleAxe.WP_BattleAxe_C",
    };
    for(size_t i=0;i<std::size(paths);++i) if(path==paths[i]) return static_cast<Weapon>(i+1);
    return Weapon::none;
}
inline double moving_phase(double prop_local_z) {
    if(!std::isfinite(prop_local_z)) return 0.;
    return std::clamp((prop_local_z-.25)/(2.8-.25),0.,1.);
}
struct CarryInput {
    Weapon weapon=Weapon::none;
    bool enabled=false,held=false,grounded=false,action=false;
    double wrist_height=0.,prop_local_z=0.;
    bool supported() const {
        return enabled && weapon!=Weapon::none && held && grounded && !action &&
            std::isfinite(wrist_height) && wrist_height>-24. && wrist_height<5. &&
            std::isfinite(prop_local_z) && prop_local_z>-1. && prop_local_z<6.;
    }
};
class CarryBlend {
    Weapon weapon_=Weapon::none;
    double value_=0.;
public:
    void reset() {weapon_=Weapon::none;value_=0.;}
    double value() const {return value_;}
    double update(const CarryInput& input,double seconds) {
        if(!input.enabled || input.weapon!=weapon_) {value_=0.;weapon_=input.weapon;return value_;}
        if(!std::isfinite(seconds) || seconds<0.) {value_=0.;return value_;}
        const bool carry=input.supported();
        const double step=std::min(seconds,.1)/(carry?.15:.06);
        value_=std::clamp(value_+(carry?step:-step),0.,1.);
        return value_;
    }
};
using Quat=std::array<double,4>;
using Targets=std::array<Quat,19>;
}

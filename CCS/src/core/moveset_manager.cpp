#include "moveset_manager.hpp"
#include <algorithm>

namespace ccs {

MovesetManager::MovesetManager() {
    initialize_database();
}

void MovesetManager::initialize_database() {
    moves_db_.clear();
    tarstones_db_.clear();

    // -------------------------------------------------------------
    // 1. Tarstones Database (Compatible Finisher & Hold Stones)
    // -------------------------------------------------------------
    tarstones_db_.push_back({
        "ID_Melee_LightAttackFinisher_Break",
        "Stillblade's Stone",
        "Grants access to a special Light Combo Finisher.\nDeals +25 Break Damage on combo completion.",
        SlotId::LF,
        3,
        "/Game/Sparta/UI/Tarstones/T_UI_Melee_LightComboBreak.T_UI_Melee_LightComboBreak",
        25.0, 0.0, 0.0
    });

    tarstones_db_.push_back({
        "ID_Melee_LightAttackFinisher_Resolve",
        "Zealot's Stone",
        "Grants access to a special Light Combo Finisher.\nRestores 1.5 Resolve bars on combo completion.",
        SlotId::LF,
        3,
        "/Game/Sparta/UI/Tarstones/T_UI_Tarstone_Melee_ZealotStone.T_UI_Tarstone_Melee_ZealotStone",
        0.0, 0.0, 1.5
    });

    tarstones_db_.push_back({
        "ID_Melee_HeavyAttackFinisher_Critical",
        "Clerik's Stone",
        "Grants access to a special Heavy Combo Finisher.\nGuaranteed Critical Hit and +50 Poise Damage.",
        SlotId::HF,
        3,
        "/Game/Sparta/UI/Tarstones/T_UI_TStone_Melee_CriticalFlow_CritChance.T_UI_TStone_Melee_CriticalFlow_CritChance",
        0.0, 50.0, 0.0
    });

    tarstones_db_.push_back({
        "ID_Melee_HeavyAttackFinisher_Weak",
        "Tyrant's Stone",
        "Grants access to a special Heavy Combo Finisher.\nForces Flyback Knockdown and inflicts Weakness.",
        SlotId::HF,
        3,
        "/Game/Sparta/UI/Tarstones/T_UI_Tarstone_Melee_TyrantStone.T_UI_Tarstone_Melee_TyrantStone",
        0.0, 40.0, 0.0
    });

    tarstones_db_.push_back({
        "ID_Melee_LightHoldAttack",
        "Acolyte's Stone",
        "Consume Resolve to perform a Charged Light Attack.\nCharge damage scales up to 1.75x.",
        SlotId::LC,
        3,
        "/Game/Sparta/UI/Tarstones/T_UI_Tarstone_Melee_ChargedLight.T_UI_Tarstone_Melee_ChargedLight",
        0.0, 20.0, 0.0
    });

    tarstones_db_.push_back({
        "ID_Melee_HeavyHoldAttack",
        "Unwieldly Stone",
        "Consume Resolve to perform a Charged Heavy Attack.\nCharge damage scales up to 2.5x with hyperarmor.",
        SlotId::HC,
        3,
        "/Game/Sparta/UI/Tarstones/T_UI_Tarstone_Melee_ChargedHeavy.T_UI_Tarstone_Melee_ChargedHeavy",
        0.0, 40.0, 0.0
    });

    // -------------------------------------------------------------
    // 2. Player Weapon Moves
    // -------------------------------------------------------------
    // Hadern's Sword
    moves_db_.push_back({
        "HadernsSword_A1", "Iconoclast Slash 1", "Player's Weapon", "HadernsSword", MoveOrigin::PlayerWeapon,
        "HadernsSword_A1",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/A_Shared_HadernSword_A_03_Montage.A_Shared_HadernSword_A_03_Montage",
        "Swift horizontal left slash. Fast opener.", 1.0, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "HadernsSword_A2", "Iconoclast Slash 2", "Player's Weapon", "HadernsSword", MoveOrigin::PlayerWeapon,
        "HadernsSword_A2",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/A_Shared_HadernSword_AA_Montage.A_Shared_HadernSword_AA_Montage",
        "Right-handed follow-through diagonal slash.", 1.0, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "HadernsSword_A3", "Iconoclast Slash 3", "Player's Weapon", "HadernsSword", MoveOrigin::PlayerWeapon,
        "HadernsSword_A3",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/A_Shared_HadernSword_AAA_Montage.A_Shared_HadernSword_AAA_Montage",
        "Downward vertical cleave combo ender.", 1.0, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "HadernsSword_A_Finisher", "Iconoclast Finisher", "Player's Weapon", "HadernsSword", MoveOrigin::TarstoneFinisher,
        "HadernsSword_A_Finisher",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/A_Shared_HadernSword_AAA_Finisher_Montage.A_Shared_HadernSword_AAA_Finisher_Montage",
        "Brutal two-hit spinning finisher.", 1.0, 30.0, 25.0, "Event.Reaction.Hit.Medium", true, false
    });
    moves_db_.push_back({
        "Attack_HadernsSword_A3_Hold", "Iconoclast Charged Light", "Player's Weapon", "HadernsSword", MoveOrigin::HoldAttack,
        "Attack_HadernsSword_A3_Hold",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/Hold/A_Shared_HadernSword_AAA_Hold_Montage.A_Shared_HadernSword_AAA_Hold_Montage",
        "Sustained light charge into crushing plunge.", 1.75, 40.0, 0.0, "Event.Reaction.Hit.Heavy", false, true
    });

    moves_db_.push_back({
        "HadernsSword_B1", "Iconoclast Heavy 1", "Player's Weapon", "HadernsSword", MoveOrigin::PlayerWeapon,
        "HadernsSword_B1",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/Temp/B/A_Shared_HadernSword_B_Montage.A_Shared_HadernSword_B_Montage",
        "Heavy forward thrust with shield break.", 1.5, 40.0, 0.0, "Event.Reaction.Hit.Heavy", false, false
    });
    moves_db_.push_back({
        "HadernsSword_B2", "Iconoclast Heavy 2", "Player's Weapon", "HadernsSword", MoveOrigin::PlayerWeapon,
        "HadernsSword_B2",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/Temp/B/A_Shared_Attacks_HadernSword_BB_02_Montage.A_Shared_Attacks_HadernSword_BB_02_Montage",
        "Upward rising heavy slash.", 1.5, 40.0, 0.0, "Event.Reaction.Hit.Heavy", false, false
    });
    moves_db_.push_back({
        "HadernsSword_B3", "Iconoclast Heavy 3", "Player's Weapon", "HadernsSword", MoveOrigin::PlayerWeapon,
        "HadernsSword_B3",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/Temp/B/A_Shared_HadernSword_BBB_Montage.A_Shared_HadernSword_BBB_Montage",
        "Overhead slam heavy combo climax.", 1.5, 40.0, 0.0, "Event.Reaction.Hit.Heavy", false, false
    });
    moves_db_.push_back({
        "HadernsSword_B_Finisher", "Iconoclast Heavy Finisher", "Player's Weapon", "HadernsSword", MoveOrigin::TarstoneFinisher,
        "HadernsSword_B_Finisher",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/Temp/B/A_Shared_HadernSword_BBB_Finisher_Montage.A_Shared_HadernSword_BBB_Finisher_Montage",
        "Devastating heavy finisher causing guard flyback.", 1.5, 40.0, 50.0, "Event.Reaction.Hit.Flyback", true, false
    });
    moves_db_.push_back({
        "Attack_HadernsSword_B3_Hold", "Iconoclast Charged Heavy", "Player's Weapon", "HadernsSword", MoveOrigin::HoldAttack,
        "Attack_HadernsSword_B3_Hold",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/Hold/A_Shared_HadernSword_BBB_Hold_Montage.A_Shared_HadernSword_BBB_Hold_Montage",
        "Maximum windup charge with unstoppable hyperarmor.", 2.5, 60.0, 0.0, "Event.Reaction.Hit.Flyback", false, true
    });

    // Clockwork Scythe
    moves_db_.push_back({
        "ClockworkScythe_A1", "Scythe Reaping 1", "Player's Weapon", "ClockworkScythe", MoveOrigin::PlayerWeapon,
        "ClockworkScythe_A1",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/Scythe/A_Shared_Attacks_Scythe_A_02_Montage.A_Shared_Attacks_Scythe_A_02_Montage",
        "Wide sweeping curved harvest slash.", 1.0, 25.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "ClockworkScythe_A3_Finisher", "Scythe Saw Finisher", "Player's Weapon", "ClockworkScythe", MoveOrigin::TarstoneFinisher,
        "ClockworkScythe_A3_Finisher",
        "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/Scythe/A_Shared_Attacks_Scythe_AAA_03_Finisher_Montage.A_Shared_Attacks_Scythe_AAA_03_Finisher_Montage",
        "Continuous 4-tick motorized chainsaw shred.", 1.0, 50.0, 30.0, "Event.Reaction.Hit.Medium", true, false
    });

    // -------------------------------------------------------------
    // 3. Humanoid Enemy Moves (Brigand, Cultist, Sicario, Wraith)
    // -------------------------------------------------------------
    // The Brigands
    moves_db_.push_back({
        "BrigBase_Attack_Swipes_2hit", "Brigand Quick Slash", "Brigands", "BrigBase", MoveOrigin::EnemyHumanoid,
        "GA_BrigBase_Attack_Swipes_2hit",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animations/Attacks/AM_BrigBase_Attack_Swipes_2hit.AM_BrigBase_Attack_Swipes_2hit",
        "Aggressive dual forward slashes with quick recovery.", 1.0, 20.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "BrigBase_Attack_Stab", "Brigand Quick Stab", "Brigands", "BrigBase", MoveOrigin::EnemyHumanoid,
        "GA_BrigBase_Attack_Stab",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animations/Attacks/AM_BrigBase_Attack_Stab.AM_BrigBase_Attack_Stab",
        "Snappy linear thrust ideal for combo openers.", 1.0, 25.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "BrigBase_Attack_Juke_Left", "Brigand Juke Slash", "Brigands", "BrigBase", MoveOrigin::EnemyHumanoid,
        "GA_BrigBase_Attack_Juke_Left",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animations/Attacks/AM_BrigBase_Attack_Juke_Left.AM_BrigBase_Attack_Juke_Left",
        "Feint sidestep into counter diagonal slash.", 0.8, 20.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "BrigBase_Attacks_Stabs_3hit", "Brigand Triple Stabs", "Brigands", "BrigBase", MoveOrigin::EnemyHumanoid,
        "GA_BrigBase_Attacks_Stabs_3hit",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animations/Attacks/AM_BrigBase_Attacks_Stabs_3hit.AM_BrigBase_Attacks_Stabs_3hit",
        "Rapid 3-hit spear thrust barrage ending with a lunge.", 1.0, 35.0, 0.0, "Event.Reaction.Hit.Heavy", false, false
    });
    moves_db_.push_back({
        "BrigBase_Attack_Gap_Swing_2hit", "Brigand Leaping Slam", "Brigands", "BrigBase", MoveOrigin::EnemyHumanoid,
        "GA_BrigBase_Attack_Gap_Swing_2hit",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animations/Attacks/AM_BrigBase_Attack_Gap_Swing_2hit.AM_BrigBase_Attack_Gap_Swing_2hit",
        "500cm leaping downward crush with shield break.", 1.25, 35.0, 15.0, "Event.Reaction.Hit.Heavy", true, false
    });
    moves_db_.push_back({
        "BrigBase_Attack_Runup_Stab", "Brigand Run-Up Thrust", "Brigands", "BrigBase", MoveOrigin::EnemyHumanoid,
        "GA_BrigBase_Attack_Runup_Stab",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animations/Attacks/AM_BrigBase_Attack_Runup_Stab.AM_BrigBase_Attack_Runup_Stab",
        "Sprinting charge thrust with long tracking.", 1.2, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, true
    });
    moves_db_.push_back({
        "BrigElite_Attack_Overhead_AAA", "Elite Brigand Triple Slam", "Brigands", "BrigElite", MoveOrigin::EnemyHumanoid,
        "GA_BrigElite_Attack_Overhead_AAA",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigElite/Animation/Attacks/AM_BrigElite_Attack_Overhead_AAA.AM_BrigElite_Attack_Overhead_AAA",
        "Heavy vertical execution slam with high poise break.", 1.3, 45.0, 0.0, "Event.Reaction.Hit.Heavy", false, false
    });
    moves_db_.push_back({
        "BrigElite_Attack_Jump_Overhead", "Elite Jump Overhead", "Brigands", "BrigElite", MoveOrigin::EnemyHumanoid,
        "GA_BrigElite_Attack_Jump_Overhead",
        "/Game/Sparta/Characters/Enemies/Brigands/BrigElite/Animation/Attacks/AM_BrigElite_Attack_Jump_Overhead.AM_BrigElite_Attack_Jump_Overhead",
        "High leap downward splatter causing ground shockwave.", 1.5, 50.0, 20.0, "Event.Reaction.Hit.Heavy", true, true
    });

    // The Cultists
    moves_db_.push_back({
        "CultistSpearLady_5Hit_Attack", "Cultist Acrobatic Dance", "Cultists", "CultistSpearLady", MoveOrigin::EnemyHumanoid,
        "GA_CultistSpearLady_5Hit_Attack",
        "/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Attacks/AM_CultistSpearLady_5Hit_Attack.AM_CultistSpearLady_5Hit_Attack",
        "Acrobatic 5-hit spear dance covering broad frontal arc.", 1.0, 25.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "CultistSpearLady_3Spins_Attack", "Cultist 360 Whirlwind", "Cultists", "CultistSpearLady", MoveOrigin::EnemyHumanoid,
        "GA_CultistSpearLady_3Spins_Attack",
        "/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Attacks/AM_CultistSpearLady_3Spins_Attack.AM_CultistSpearLady_3Spins_Attack",
        "360-degree rotational whirlwind hitting all surrounding foes.", 1.0, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "CultistSpearLady_StableLeap_Attack", "Cultist Aerial Leap", "Cultists", "CultistSpearLady", MoveOrigin::EnemyHumanoid,
        "GA_CultistSpearLady_StableLeap_Attack",
        "/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Attacks/AM_CultistSpearLady_StableLeap_Attack.AM_CultistSpearLady_StableLeap_Attack",
        "Aerial downward spear plunge piercing neutral guard.", 1.35, 45.0, 20.0, "Event.Reaction.Hit.Heavy", true, false
    });
    moves_db_.push_back({
        "CultistSpearLady_Gap_Thrust_Attack", "Cultist Gap Vault Thrust", "Cultists", "CultistSpearLady", MoveOrigin::EnemyHumanoid,
        "GA_CultistSpearLady_Gap_Thrust_Attack",
        "/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Attacks/AM_CultistSpearLady_Gap_Thrust_Attack.AM_CultistSpearLady_Gap_Thrust_Attack",
        "High-velocity linear spear vault with 700cm reach.", 1.0, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, true
    });

    // The Sicario
    moves_db_.push_back({
        "Sicario_Attack_Rush", "Sicario Dash Cross-Slice", "Sicario", "Sicario", MoveOrigin::EnemyHumanoid,
        "GA_Sicario_Attack_Rush",
        "/Game/Sparta/Characters/Enemies/Sicario/Animation/Attacks/AM_Sicario_Attack_Rush.AM_Sicario_Attack_Rush",
        "Lightning-fast sprint into dual-dagger scissor cross.", 1.0, 30.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "Sicario_2Hit_Swing_Attack", "Sicario Twin Slice", "Sicario", "Sicario", MoveOrigin::EnemyHumanoid,
        "GA_Sicario_2Hit_Swing_Attack",
        "/Game/Sparta/Characters/Enemies/Sicario/Animation/Attacks/AM_Sicario_2Hit_Swing_Attack.AM_Sicario_2Hit_Swing_Attack",
        "Close-quarters rapid double slash with bleed application.", 1.0, 25.0, 0.0, "Event.Reaction.Hit.Medium", false, false
    });
    moves_db_.push_back({
        "Sicario_Counter_Attack", "Sicario Vanish Riposte", "Sicario", "Sicario", MoveOrigin::EnemyHumanoid,
        "GA_Sicario_Counter_Attack",
        "/Game/Sparta/Characters/Enemies/Sicario/Animation/Attacks/AM_Sicario_Counter_Attack.AM_Sicario_Counter_Attack",
        "Lethal counter-slash inflicting 1.8x damage and Bleed.", 1.8, 60.0, 10.0, "Event.Reaction.Hit.Heavy", true, false
    });
}

std::vector<MoveDefinition> MovesetManager::get_moves_for_slot(SlotId slot) const {
    std::vector<MoveDefinition> out;
    for (const auto& move : moves_db_) {
        if (slot == SlotId::LF || slot == SlotId::HF) {
            if (move.is_finisher || move.poise_damage >= 35.0) out.push_back(move);
        } else if (slot == SlotId::LC || slot == SlotId::HC) {
            if (move.is_hold || move.damage_multiplier >= 1.2) out.push_back(move);
        } else {
            if (!move.is_finisher && !move.is_hold) out.push_back(move);
        }
    }
    return out;
}

std::vector<TarstoneDefinition> MovesetManager::get_tarstones_for_slot(SlotId slot) const {
    std::vector<TarstoneDefinition> out;
    for (const auto& stone : tarstones_db_) {
        if (stone.compatible_slot == slot) {
            out.push_back(stone);
        }
    }
    return out;
}

const MoveDefinition* MovesetManager::find_move(const std::string& id) const {
    for (const auto& m : moves_db_) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const TarstoneDefinition* MovesetManager::find_tarstone(const std::string& id) const {
    for (const auto& t : tarstones_db_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

void MovesetManager::assign_move_to_slot(SlotId slot, const MoveDefinition& move) {
    auto& binding = active_preset_.slots[static_cast<size_t>(slot)];
    binding.slot = slot;
    binding.source = move.source_name;
    binding.move_id = move.id;
    binding.ability_path = move.ability_path;
    binding.montage_path = move.montage_path;
    binding.origin = move.origin;
}

void MovesetManager::assign_tarstone_to_slot(SlotId slot, const TarstoneDefinition& stone) {
    auto& binding = active_preset_.slots[static_cast<size_t>(slot)];
    binding.slot = slot;
    binding.tarstone_id = stone.id;
    binding.tarstone_name = stone.display_name;
}

void MovesetManager::unequip_slot(SlotId slot) {
    auto& binding = active_preset_.slots[static_cast<size_t>(slot)];
    binding.tarstone_id.clear();
    binding.tarstone_name.clear();
}

void MovesetManager::root_asset(UObject* asset) {
    if (!asset) return;
    if (std::find(rooted_assets_.begin(), rooted_assets_.end(), asset) == rooted_assets_.end()) {
        try {
            engine::Call call(asset, L"AddToRoot", 0);
            call.run();
        } catch (...) {}
        rooted_assets_.push_back(asset);
    }
}

void MovesetManager::unroot_all() {
    for (auto* asset : rooted_assets_) {
        if (asset) {
            try {
                engine::Call call(asset, L"RemoveFromRoot", 0);
                call.run();
            } catch (...) {}
        }
    }
    rooted_assets_.clear();
}

bool MovesetManager::apply_to_player(const engine::PlayerContext& player) {
    if (!player.pawn) return false;

    // Pre-load all configured montages in active preset and pin to root so GC never invalidates them
    for (const auto& binding : active_preset_.slots) {
        if (!binding.montage_path.empty()) {
            try {
                auto* montage = engine::load(binding.montage_path);
                if (montage) root_asset(montage);
            } catch (...) {}
        }
    }

    applied_ = true;
    return true;
}

void MovesetManager::restore_vanilla(const engine::PlayerContext& /*player*/) {
    if (!applied_) return;
    unroot_all();
    vanilla_montages_.clear();
    applied_ = false;
}

bool MovesetManager::on_attack_montage_requested(UObject* /*ability*/, UObject*& /*in_out_montage*/, double& /*in_out_play_rate*/) {
    if (!applied_) return false;
    return false;
}

} // namespace ccs

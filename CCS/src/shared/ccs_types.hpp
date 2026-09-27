#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace ccs {

enum class SlotId : uint8_t {
    L1 = 0,
    L2 = 1,
    L3 = 2,
    LF = 3,
    LC = 4,
    H1 = 5,
    H2 = 6,
    H3 = 7,
    HF = 8,
    HC = 9,
    R = 10,        // ranged: the sidearm's primary fire
    Count = 11
};

inline const char* slot_to_string(SlotId slot) {
    switch (slot) {
        case SlotId::L1: return "L1";
        case SlotId::L2: return "L2";
        case SlotId::L3: return "L3";
        case SlotId::LF: return "LF";
        case SlotId::LC: return "LC";
        case SlotId::H1: return "H1";
        case SlotId::H2: return "H2";
        case SlotId::H3: return "H3";
        case SlotId::HF: return "HF";
        case SlotId::HC: return "HC";
        case SlotId::R: return "R";
        default: return "Unknown";
    }
}

inline std::optional<SlotId> string_to_slot(const std::string& str) {
    if (str == "L1") return SlotId::L1;
    if (str == "L2") return SlotId::L2;
    if (str == "L3") return SlotId::L3;
    if (str == "LF") return SlotId::LF;
    if (str == "LC") return SlotId::LC;
    if (str == "H1") return SlotId::H1;
    if (str == "H2") return SlotId::H2;
    if (str == "H3") return SlotId::H3;
    if (str == "HF") return SlotId::HF;
    if (str == "HC") return SlotId::HC;
    if (str == "R") return SlotId::R;
    return std::nullopt;
}

enum class MoveOrigin : uint8_t {
    PlayerWeapon,
    EnemyHumanoid,
    TarstoneFinisher,
    HoldAttack
};

struct MoveDefinition {
    std::string id;
    std::string display_name;
    std::string category;          // e.g. "Player's Weapon", "Tarstones", "Brigands", "Cultists", "Sicario"
    std::string source_name;       // e.g. "HadernsSword", "CultistSpearLady"
    MoveOrigin origin{MoveOrigin::PlayerWeapon};
    std::string ability_path;
    std::string montage_path;
    std::string description;
    double damage_multiplier{1.0};
    double poise_damage{25.0};
    double break_damage{0.0};
    std::string reaction_tag;
    bool is_finisher{false};
    bool is_hold{false};
    uint16_t compatible_slots{0};
    std::string skeleton;
    bool runtime_verified{false};
    bool payload_known{false};
};

struct TarstoneDefinition {
    std::string id;                // e.g. "ID_Melee_LightAttackFinisher_Break"
    std::string display_name;      // e.g. "Stillblade's Stone"
    std::string description;
    SlotId compatible_slot{SlotId::LF};
    int tier{3};
    std::string icon_path;
    double bonus_break{25.0};
    double bonus_poise{0.0};
    double resolve_gain{0.0};
    std::string stat;
    std::vector<double> levels;
    std::vector<std::string> compatibility_tags;
};

// Per-slot tuning, saved with the settings and inside presets: play-rate, whose hit payload a
// swapped swing carries, and which weapon shows in hand while it plays.
struct SlotTuning {
    double speed{1.0};                 // 0.5 .. 2.0, multiplies the montage play rate
    std::string hit_damage{"move"};    // "move": the replacement's own hit payload; "weapon": the slot's original payload
    std::string weapon{"inventory"};   // "inventory": the equipped weapon stays visible; "move": the move's own weapon shows
    bool operator==(const SlotTuning&) const = default;
};
inline bool valid_tuning(const SlotTuning& t) {
    return t.speed >= 0.5 && t.speed <= 2.0 && (t.hit_damage == "move" || t.hit_damage == "weapon") && (t.weapon == "inventory" || t.weapon == "move");
}

struct SlotBinding {
    SlotId slot{SlotId::L1};
    MoveOrigin origin{MoveOrigin::PlayerWeapon};
    std::string source;            // Weapon name or Enemy Archetype
    std::string move_id;
    std::string ability_path;
    std::string montage_path;
    std::string tarstone_id;       // If finisher/hold tarstone equipped
    std::string tarstone_name;
    SlotTuning tuning;
};

struct PresetData {
    int schema_version{1};
    std::string name{"Default"};
    std::string author{"Community"};
    std::string description{"Custom Combat System moveset preset"};
    std::string base_weapon;
    std::array<SlotBinding, static_cast<size_t>(SlotId::Count)> slots;

    PresetData() {
        for (size_t i = 0; i < slots.size(); ++i) {
            slots[i].slot = static_cast<SlotId>(i);
        }
    }
};

} // namespace ccs

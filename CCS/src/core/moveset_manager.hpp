#pragma once
#include "ccs_types.hpp"
#include "engine.hpp"
#include "catalog.hpp"
#include <vector>
#include <string>
#include <span>

namespace ccs {

using RC::Unreal::UObject;

class MovesetManager {
public:
    MovesetManager() = default;
    ~MovesetManager() = default;
    MovesetManager(const MovesetManager&) = delete;
    MovesetManager& operator=(const MovesetManager&) = delete;


    const std::vector<MoveDefinition>& get_all_moves() const { return catalog_.moves(); }
    const std::vector<TarstoneDefinition>& get_all_tarstones() const { return catalog_.tarstones(); }

    std::span<const MoveDefinition* const> get_moves_for_slot(SlotId slot, bool enemy = false) const;
    std::span<const TarstoneDefinition* const> get_tarstones_for_slot(SlotId slot) const;

    const MoveDefinition* find_move(const std::string& id) const;
    const TarstoneDefinition* find_tarstone(const std::string& id) const;

    // Active preset management
    const PresetData& active_preset() const { return active_preset_; }
    void set_active_preset(PresetData preset) { active_preset_ = std::move(preset); }

    // Slot binding mutators
    void assign_move_to_slot(SlotId slot, const MoveDefinition& move);
    void assign_tarstone_to_slot(SlotId slot, const TarstoneDefinition& stone);
    void unequip_slot(SlotId slot);

    // Apply active bindings to the live player character
    bool apply_to_player(const engine::PlayerContext& player);
    void restore_vanilla(const engine::PlayerContext& player);

    // Live hook interceptor when an attack starts
    bool on_attack_montage_requested(UObject* ability, UObject*& in_out_montage, double& in_out_play_rate);

    void root_asset(UObject* asset);
    void unroot_all();

private:
    runtime::Catalog catalog_;
    std::array<std::vector<const MoveDefinition*>, static_cast<size_t>(SlotId::Count)> player_slots_, enemy_slots_;
    std::array<std::vector<const TarstoneDefinition*>, static_cast<size_t>(SlotId::Count)> stone_slots_;
    PresetData active_preset_;


    // Only roots acquired by CCS are released on the game thread.
    std::vector<engine::WeakObject> rooted_assets_;
};

} // namespace ccs

#pragma once
#include "ccs_types.hpp"
#include "engine.hpp"
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace ccs {

using RC::Unreal::UObject;

class MovesetManager {
public:
    MovesetManager();
    ~MovesetManager() = default;

    void initialize_database();

    const std::vector<MoveDefinition>& get_all_moves() const { return moves_db_; }
    const std::vector<TarstoneDefinition>& get_all_tarstones() const { return tarstones_db_; }

    std::vector<MoveDefinition> get_moves_for_slot(SlotId slot) const;
    std::vector<TarstoneDefinition> get_tarstones_for_slot(SlotId slot) const;

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
    std::vector<MoveDefinition> moves_db_;
    std::vector<TarstoneDefinition> tarstones_db_;
    PresetData active_preset_;
    bool applied_{false};

    // Cached vanilla montage pointers for clean rollback (class name -> weak montage)
    std::map<std::wstring, engine::WeakObject> vanilla_montages_;
    std::vector<UObject*> rooted_assets_;
};

} // namespace ccs

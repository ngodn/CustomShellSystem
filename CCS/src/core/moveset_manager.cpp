#include "moveset_manager.hpp"
#include <algorithm>
#include <stdexcept>

namespace ccs {
std::span<const MoveDefinition* const> MovesetManager::get_moves_for_slot(SlotId slot, bool enemy) const {
    if (slot >= SlotId::Count) return {};
    return (enemy ? enemy_slots_ : player_slots_)[static_cast<size_t>(slot)];
}

std::span<const TarstoneDefinition* const> MovesetManager::get_tarstones_for_slot(SlotId slot) const {
    if (slot >= SlotId::Count) return {};
    return stone_slots_[static_cast<size_t>(slot)];
}

const MoveDefinition* MovesetManager::find_move(const std::string& id) const { return catalog_.find_move(id); }
const TarstoneDefinition* MovesetManager::find_tarstone(const std::string& id) const { return catalog_.find_tarstone(id); }

void MovesetManager::assign_move_to_slot(SlotId slot, const MoveDefinition& move) {
    if (slot >= SlotId::Count) return;
    const auto* entry = catalog_.find_move(move.id);
    if (!entry || !(entry->compatible_slots & (1u << static_cast<unsigned>(slot)))) return;
    auto& binding = active_preset_.slots[static_cast<size_t>(slot)];
    binding.slot = slot;
    binding.source = entry->source_name;
    binding.move_id = entry->id;
    binding.ability_path = entry->ability_path;
    binding.montage_path = entry->montage_path;
    binding.origin = entry->origin;
}
void MovesetManager::assign_tarstone_to_slot(SlotId slot, const TarstoneDefinition& stone) {
    if (slot >= SlotId::Count) return;
    const auto* entry = catalog_.find_tarstone(stone.id);
    if (!entry || entry->compatible_slot != slot) return;
    auto& binding = active_preset_.slots[static_cast<size_t>(slot)];
    binding.slot = slot;
    binding.tarstone_id = entry->id;
    binding.tarstone_name = entry->display_name;
}
void MovesetManager::unequip_slot(SlotId slot) {
    if (slot >= SlotId::Count) return;
    active_preset_.slots[static_cast<size_t>(slot)] = SlotBinding{};
    active_preset_.slots[static_cast<size_t>(slot)].slot = slot;
}
void MovesetManager::root_asset(UObject* asset) {
    if (!asset || asset->IsRootSet()) return;
    rooted_assets_.emplace_back(asset);
    asset->SetRootSet();
}
void MovesetManager::unroot_all() {
    for (auto& weak : rooted_assets_) if (auto* asset = weak.Get()) asset->ClearRootSet();
    rooted_assets_.clear();
}
bool MovesetManager::apply_to_player(const engine::PlayerContext&) {
    // The prototype has no verified combat hook. Never load assets or report an applied override.
    return false;
}
void MovesetManager::restore_vanilla(const engine::PlayerContext&) { unroot_all(); }
bool MovesetManager::on_attack_montage_requested(UObject*, UObject*&, double&) { return false; }
} // namespace ccs

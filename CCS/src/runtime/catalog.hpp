#pragma once
#include "ccs_types.hpp"
#include <filesystem>
#include <unordered_map>

namespace ccs::runtime {
class Catalog {
public:
    bool load(const std::filesystem::path& path);
    // Enemy attack montages verified to target the player's skeleton (data/enemy-catalog.json).
    // Appended to the move list with origin EnemyHumanoid and every slot allowed.
    bool load_enemy(const std::filesystem::path& path);
    // Sidearm fire montages (data/ranged-catalog.json): the ranged slot's player candidates.
    bool load_ranged(const std::filesystem::path& path);
    // Sprint attacks (data/running-catalog.json): the sprint slots' player candidates.
    bool load_running(const std::filesystem::path& path);
    size_t enemy_count() const { return enemy_count_; }
    const std::vector<MoveDefinition>& moves() const { return moves_; }
    const std::vector<TarstoneDefinition>& tarstones() const { return tarstones_; }
    const MoveDefinition* find_move(const std::string& id) const;
    const TarstoneDefinition* find_tarstone(const std::string& id) const;
    const std::string& error() const { return error_; }
private:
    std::vector<MoveDefinition> moves_;
    std::vector<TarstoneDefinition> tarstones_;
    std::unordered_map<std::string, size_t> move_index_, tarstone_index_;
    std::string error_;
    size_t enemy_count_{};
};
}

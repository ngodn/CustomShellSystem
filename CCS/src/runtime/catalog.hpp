#pragma once
#include "ccs_types.hpp"
#include <filesystem>
#include <unordered_map>

namespace ccs::runtime {
class Catalog {
public:
    bool load(const std::filesystem::path& path);
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
};
}

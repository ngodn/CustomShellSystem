#include "catalog.hpp"
#include "common.hpp"
#include <cmath>
#include <stdexcept>

namespace ccs::runtime {
bool Catalog::load(const std::filesystem::path& path) {
    try {
        const auto j = nlohmann::json::parse(read_file_text(path));
        if (j.at("schema_version") != 1) throw std::runtime_error("Catalog schema mismatch");
        const auto source_build = j.at("game_build").get<std::string>();
        if (source_build.empty() || source_build.size() > 256)
            throw std::runtime_error("Catalog extraction provenance invalid");
        Catalog candidate;
        const auto& rows = j.at("moves");
        if (!rows.is_array() || rows.size() > 2048) throw std::runtime_error("Catalog move count invalid");
        for (const auto& row : rows) {
            MoveDefinition move;
            move.id = row.at("id").get<std::string>();
            move.display_name = row.at("display_name").get<std::string>();
            move.source_name = row.at("source_name").get<std::string>();
            move.ability_path = row.at("ability").get<std::string>();
            move.montage_path = row.at("montage").get<std::string>();
            move.skeleton = row.at("skeleton").get<std::string>();
            move.description = "Extracted metadata. Runtime compatibility has not been verified.";
            move.category = "Player's Weapon";
            move.damage_multiplier = 0.0;
            move.poise_damage = 0.0;
            if (move.id.empty() || move.id.size() > 256 || move.montage_path.size() > 4096 ||
                !move.montage_path.starts_with("/Game/") || move.skeleton.empty())
                throw std::runtime_error("Catalog move identity invalid");
            const auto& slots = row.at("slots");
            if (!slots.is_array() || slots.empty()) throw std::runtime_error("Catalog has no slot evidence");
            for (const auto& name : slots) {
                const auto slot = string_to_slot(name.get<std::string>());
                if (!slot) throw std::runtime_error("Catalog slot invalid");
                move.compatible_slots |= static_cast<uint16_t>(1u << static_cast<unsigned>(*slot));
                move.is_finisher |= *slot == SlotId::LF || *slot == SlotId::HF;
                move.is_hold |= *slot == SlotId::LC || *slot == SlotId::HC;
            }
            move.origin = move.is_hold ? MoveOrigin::HoldAttack :
                          move.is_finisher ? MoveOrigin::TarstoneFinisher : MoveOrigin::PlayerWeapon;
            // Cooked payload metadata is retained in JSON. Do not present it as live damage.
            const auto index = candidate.moves_.size();
            if (!candidate.move_index_.emplace(move.id, index).second)
                throw std::runtime_error("Duplicate catalog move id");
            if (move.id.starts_with("GA_Player_") && move.id.ends_with("_C")) {
                const auto alias = move.id.substr(10, move.id.size() - 12);
                if (!candidate.move_index_.emplace(alias, index).second)
                    throw std::runtime_error("Duplicate legacy move alias");
            }
            candidate.moves_.push_back(std::move(move));
        }
        const auto& stones = j.at("tarstone_stats");
        if (!stones.is_array() || stones.size() > 256) throw std::runtime_error("Catalog item count invalid");
        for (const auto& row : stones) {
            TarstoneDefinition stone;
            stone.id = row.at("id").get<std::string>();
            stone.display_name = row.at("display_name").get<std::string>();
            stone.description = row.at("description").get<std::string>();
            stone.icon_path = row.at("icon").get<std::string>();
            stone.stat = row.at("stat").get<std::string>();
            stone.levels = row.at("levels").get<std::vector<double>>();
            stone.compatibility_tags = row.at("compatibility_tags").get<std::vector<std::string>>();
            const auto slot = string_to_slot(row.at("slot").get<std::string>());
            if (!slot || (*slot != SlotId::LF && *slot != SlotId::HF) || stone.levels.empty() || stone.levels.size() > 16)
                throw std::runtime_error("Catalog Tarstone evidence invalid");
            for (double level : stone.levels) if (!std::isfinite(level)) throw std::runtime_error("Catalog stat invalid");
            stone.compatible_slot = *slot;
            stone.tier = static_cast<int>(stone.levels.size());
            stone.bonus_break = 0.0;
            if (!candidate.tarstone_index_.emplace(stone.id, candidate.tarstones_.size()).second)
                throw std::runtime_error("Duplicate catalog item id");
            candidate.tarstones_.push_back(std::move(stone));
        }
        *this = std::move(candidate);
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}
bool Catalog::load_enemy(const std::filesystem::path& path) {
    try {
        const auto j = nlohmann::json::parse(read_file_text(path));
        if (j.at("schema_version") != 1) throw std::runtime_error("Enemy catalog schema mismatch");
        const auto& rows = j.at("moves");
        if (!rows.is_array() || rows.size() > 4096) throw std::runtime_error("Enemy catalog move count invalid");
        std::vector<MoveDefinition> added;
        for (const auto& row : rows) {
            MoveDefinition move;
            move.id = row.at("id").get<std::string>();
            move.display_name = row.at("display_name").get<std::string>();
            move.source_name = row.at("source_name").get<std::string>();
            move.montage_path = row.at("montage").get<std::string>();
            move.skeleton = row.at("skeleton").get<std::string>();
            move.description = row.value("description", std::string{});
            move.category = "Enemy's Weapon";
            move.origin = MoveOrigin::EnemyHumanoid;
            move.compatible_slots = 0x3FF;
            move.payload_known = row.value("hit_windows", 0) > 0;
            if (move.id.empty() || move.id.size() > 256 || move.montage_path.size() > 4096 || !move.montage_path.starts_with("/Game/") ||
                move.skeleton.empty() || move.source_name.empty() || move.source_name.size() > 64 || move.description.size() > 1024)
                throw std::runtime_error("Enemy catalog move identity invalid");
            if (move_index_.contains(move.id)) throw std::runtime_error("Enemy catalog id collides with a player move");
            added.push_back(std::move(move));
        }
        for (auto& move : added) {
            const auto index = moves_.size();
            if (!move_index_.emplace(move.id, index).second) throw std::runtime_error("Duplicate enemy catalog move id");
            moves_.push_back(std::move(move));
        }
        enemy_count_ = added.size();
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}
bool Catalog::load_ranged(const std::filesystem::path& path) {
    try {
        const auto j = nlohmann::json::parse(read_file_text(path));
        if (j.at("schema_version") != 1) throw std::runtime_error("Ranged catalog schema mismatch");
        const auto& rows = j.at("moves");
        if (!rows.is_array() || rows.size() > 256) throw std::runtime_error("Ranged catalog move count invalid");
        std::vector<MoveDefinition> added;
        for (const auto& row : rows) {
            MoveDefinition move;
            move.id = row.at("id").get<std::string>();
            move.display_name = row.at("display_name").get<std::string>();
            move.source_name = row.at("source_name").get<std::string>();
            move.ability_path = row.value("ability", std::string{});
            move.montage_path = row.at("montage").get<std::string>();
            move.skeleton = row.at("skeleton").get<std::string>();
            move.description = row.value("description", std::string{});
            move.category = "Sidearm";
            move.origin = MoveOrigin::PlayerWeapon;
            move.compatible_slots = 1u << unsigned(SlotId::R);
            move.payload_known = true;
            if (move.id.empty() || move.id.size() > 256 || !move.montage_path.starts_with("/Game/") || move.montage_path.size() > 4096 || move.skeleton.empty() || move.description.size() > 1024)
                throw std::runtime_error("Ranged catalog move identity invalid");
            if (move_index_.contains(move.id)) throw std::runtime_error("Ranged catalog id collides");
            added.push_back(std::move(move));
        }
        for (auto& move : added) { const auto index = moves_.size(); move_index_.emplace(move.id, index); moves_.push_back(std::move(move)); }
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}
const MoveDefinition* Catalog::find_move(const std::string& id) const {
    const auto it = move_index_.find(id);
    return it == move_index_.end() ? nullptr : &moves_[it->second];
}
const TarstoneDefinition* Catalog::find_tarstone(const std::string& id) const {
    const auto it = tarstone_index_.find(id);
    return it == tarstone_index_.end() ? nullptr : &tarstones_[it->second];
}
}

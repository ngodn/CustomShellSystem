#pragma once
#include "engine.hpp"
#include <memory>
#include <string_view>
#include <unordered_map>

namespace ccs {
inline constexpr int32_t invalid_ability_spec_handle = -1;
struct LoadedAbilityFacts {
    std::optional<int32_t> instancing_policy;
    std::optional<bool> is_hold;
    std::optional<std::vector<std::string>> asset_tags;
    bool operator==(const LoadedAbilityFacts&) const = default;
};
struct LoadedMove {
    std::string selector_path;
    std::string relationship;
    unsigned position{};
    std::string ability_path;
    std::string montage_path;
    std::string skeleton_path;
    std::string ability_object_path;
    int32_t selector_spec_handle{invalid_ability_spec_handle}, ability_spec_handle{invalid_ability_spec_handle};
    bool selector_instanced{}, ability_instanced{};
    LoadedAbilityFacts selector_facts, ability_facts;
    std::optional<std::vector<std::string>> grant_tags;
};
struct LoadedMovesSnapshot {
    uint64_t revision{};
    uint64_t observed_at_ms{};
    std::string weapon_path;
    std::vector<LoadedMove> candidates;
};

// Read-only observations. Slot eligibility and combat compatibility are separate checks.
class LoadedMoves {
public:
    void tick(const engine::PlayerContext& player, bool active, uint64_t now_ms);
    void reset();
    void suspend(std::string_view state, std::string_view error);
    std::shared_ptr<const LoadedMovesSnapshot> snapshot() const { return published_; }
    const std::string& state() const { return state_; }
    const std::string& error() const { return error_; }
    uint64_t maximum_step_us() const { return maximum_step_us_; }
private:
    bool same_player(const engine::PlayerContext& player, engine::UObject* item, engine::UObject* shell) const;
    void start(const engine::PlayerContext& player, engine::UObject* item, engine::UObject* shell);
    void step(const engine::PlayerContext& player, uint64_t now_ms);
    void collect_selector(engine::UObject* selector, int32_t handle, bool instanced);
    void read_candidate(size_t index, bool verify);
    struct Grant {
        engine::ObjectHandle ability;
        int32_t handle{};
        bool pending_remove{};
        bool selector{};
        std::vector<engine::ObjectHandle> instances;
        std::optional<std::vector<std::string>> tags;
    };
    struct Candidate {
        engine::ObjectHandle selector, ability_class, ability, montage, skeleton;
        unsigned relationship{}, position{};
        int32_t selector_handle{}, ability_handle{};
        bool selector_instanced{}, ability_instanced{};
        std::optional<size_t> grant_index;
        LoadedAbilityFacts selector_facts, ability_facts;
    };
    enum class Phase { CaptureGrants, Instances, Selectors, ReadCandidates, VerifyGrants, VerifyCandidates };
    engine::ObjectHandle world_, pc_, pawn_, asc_, weapon_, shell_;
    std::vector<Grant> grants_;
    std::vector<Grant> previous_grants_;
    std::unordered_multimap<uintptr_t, size_t> grant_classes_;
    std::vector<Candidate> candidates_;
    std::shared_ptr<LoadedMovesSnapshot> building_;
    std::shared_ptr<const LoadedMovesSnapshot> published_;
    size_t index_{}, instance_index_{};
    size_t expected_grants_{};
    Phase phase_{Phase::Instances};
    uint64_t revision_{}, retry_at_{}, started_at_{}, maximum_step_us_{};
    bool active_{};
    std::string state_{"idle"}, error_;
};
}

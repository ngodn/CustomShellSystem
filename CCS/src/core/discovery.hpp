#pragma once
// Live discovery of attack montages through the game's own Asset Registry, so the shipped
// catalogs are only a warm-up: a montage the registry no longer lists is marked missing, one it
// lists that the catalogs do not know appears as a new, unverified candidate. Runs once per core
// start after a player exists, a few hundred registry rows per tick, no per-frame cost after.
#include "engine.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ccs {
class Discovery {
public:
    struct Found { std::string name, path, source; bool player{}; };
    enum class State { Idle, Querying, Scanning, Done, Failed };
    void tick(const engine::PlayerContext& player);
    State state() const { return state_; }
    const std::string& error() const { return error_; }
    bool done() const { return state_ == State::Done; }
    size_t assets_seen() const { return seen_; }
    bool known(const std::string& montage_path) const { return present_.contains(montage_path); }
    const std::vector<Found>& found() const { return found_; }
    // Object paths of every attack montage the registry lists (package + "." + asset).
    const std::unordered_set<std::string>& present() const { return present_; }
private:
    void begin(const engine::PlayerContext& player);
    void scan();
    State state_{State::Idle};
    std::string error_;
    std::unique_ptr<engine::Call> query_;   // keeps the result array alive across ticks
    engine::FProperty* out_{};
    engine::FProperty* package_name_{}; engine::FProperty* asset_name_{}; engine::FProperty* class_path_{};
    int32_t element_{}, next_{}, count_{};
    size_t seen_{};
    std::unordered_set<std::string> present_;
    std::vector<Found> found_;
    uint64_t retry_after_{};
};
}

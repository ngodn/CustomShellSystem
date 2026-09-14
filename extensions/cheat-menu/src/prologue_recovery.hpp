#pragma once
#include <cssx/client.hpp>
#include <array>
#include <set>

namespace cheat {
// The only automatic cleanup allowed by this observer is the measured,
// completed Egg Stranding ability. It never edits tags or progression flags.
class PrologueRecovery {
    cssx::Client host_;
    uint64_t pawn_=0,controller_=0,ability_=0;
    unsigned observations_=0;
    double elapsed_=0;
    bool running_=false;
    std::set<std::pair<uint64_t,uint64_t>> attempted_;
    std::string message_;
    static uint64_t id(const cssx::Json& value);
    cssx::Json inspect(const cssx::Json& pawn);
public:
    static constexpr std::array<const char*,6> tags={
        "State.Block.Weapon.PutInHand","State.Block.Ability.Attack.Selector",
        "State.Block.Ability.Dodge","State.Block.Ability.Aiming",
        "State.Block.Ability.SealAbility","State.Block.Ability.Shell.Unique"};
    explicit PrologueRecovery(const CssxHost* host):host_(host){}
    void start();
    void cancel();
    void tick(double seconds);
    bool running() const {return running_;}
    const std::string& message() const {return message_;}
};
}

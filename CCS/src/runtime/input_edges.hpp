#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ccs::runtime {
enum class MenuAction : uint8_t { Cancel, Accept, Unequip, Details, TabLeft, TabRight, Up, Down, Left, Right, Count };
inline constexpr size_t menu_action_count = static_cast<size_t>(MenuAction::Count);

inline bool typing_key_allowed(MenuAction action, std::string_view key, bool typing) {
    return !typing || key.starts_with("Gamepad_") || (action == MenuAction::Cancel && key == "Escape");
}

class InputEdges {
public:
    using Sample = std::array<bool, menu_action_count>;
    InputEdges() { reset(); }
    void reset() {
        held_.fill(false); suppressed_.fill(true); repeat_after_.fill(0); last_time_ = 0;
    }
    std::optional<MenuAction> update(uint64_t now, const Sample& down) {
        if (now < last_time_) reset();
        last_time_ = now;
        std::optional<MenuAction> chosen;
        for (size_t i = 0; i < menu_action_count; ++i) {
            const auto action = static_cast<MenuAction>(i);
            const bool repeating = action >= MenuAction::Up;
            if (!down[i]) { held_[i] = false; suppressed_[i] = false; continue; }
            if (suppressed_[i]) { held_[i] = true; continue; }
            const bool fresh = !held_[i];
            const bool trigger = fresh || (repeating && now >= repeat_after_[i]);
            if (fresh) repeat_after_[i] = now + 400;
            else if (trigger) repeat_after_[i] = now + 90;
            held_[i] = true;
            if (trigger && !chosen) chosen = action;
        }
        return chosen;
    }
private:
    Sample held_, suppressed_;
    std::array<uint64_t, menu_action_count> repeat_after_{};
    uint64_t last_time_{};
};
}

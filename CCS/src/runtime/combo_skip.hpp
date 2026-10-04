#pragma once
#include <array>
#include <cstddef>

namespace ccs::runtime {
// Which position of an attack selector's combo list plays when the game asks for one the player
// skips. The selector picks ComboAttackList[CurrentComboCount]; a skipped position hands over to
// the next one that plays, wrapping to the first (which never skips). skip[i] covers positions
// 0..2, the steps CCS has slots for; a longer list's later positions always play. usable(i) says
// whether position i holds an attack at all. Returns -1 when the requested position plays.
template<typename Usable>
int combo_skip_target(int requested, int length, const std::array<bool, 3>& skip, Usable&& usable) {
    if (length <= 0 || requested < 0 || requested >= length) return -1;
    auto skipped = [&](int i) { return i > 0 && i < 3 && skip[std::size_t(i)]; };
    if (!skipped(requested)) return -1;
    for (int d = 1; d < length; ++d) {
        const int i = (requested + d) % length;
        if (!skipped(i) && usable(i)) return i;
    }
    return -1;
}
}

#pragma once
#include <string>

namespace ccs {

struct NavAction {
    static constexpr const char* Accept = "Accept";
    static constexpr const char* Cancel = "Cancel";
    static constexpr const char* Unequip = "Unequip";
    static constexpr const char* Details = "Details";
    static constexpr const char* TabLeft = "TabLeft";
    static constexpr const char* TabRight = "TabRight";
    static constexpr const char* Up = "Up";
    static constexpr const char* Down = "Down";
    static constexpr const char* Left = "Left";
    static constexpr const char* Right = "Right";
};

} // namespace ccs

#pragma once
#include <string>

namespace ccs {

// Native Game Navigation Buttons & Virtual Keys
constexpr uint8_t E_BTN_ACCEPT = 3;         // Gamepad A / Enter
constexpr uint8_t E_BTN_CANCEL = 5;         // Gamepad B / Escape
constexpr uint8_t E_BTN_UNEQUIP = 4;        // Gamepad X / Delete
constexpr uint8_t E_BTN_DETAILS = 6;        // Gamepad Y / F
constexpr uint8_t E_BTN_LB = 8;             // Gamepad LB / Z
constexpr uint8_t E_BTN_RB = 9;             // Gamepad RB / X
constexpr uint8_t E_BTN_UP = 13;
constexpr uint8_t E_BTN_DOWN = 14;
constexpr uint8_t E_BTN_LEFT = 15;
constexpr uint8_t E_BTN_RIGHT = 16;

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

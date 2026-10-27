#pragma once
// The game's menu keys, shared by the Player Menu page and Photo Mode tabs: the IA_Menu_*
// actions of the UI handler's input mapping with the keys bound to them (the player's remaps
// included), and a key test. Game thread only.
#include "engine.hpp"
#include <string>
#include <vector>

namespace cssx {
struct MenuKeyBinding {
    std::string action;                 // up, down, left, right, previous_section, next_section, accept, secondary, close
    std::vector<std::string> keys;      // Unreal key names
    RC::Unreal::UObject* input_action=nullptr;
};
std::vector<MenuKeyBinding> read_menu_bindings(RC::Unreal::UObject* pc,RC::Unreal::UObject* handler);
bool key_down(RC::Unreal::UObject* pc,const std::string& key);
RC::Unreal::FName key_name(const std::string& key);   // cached FName of an Unreal key name
}

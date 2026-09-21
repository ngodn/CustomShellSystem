#pragma once
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace css {
// CSS owns these controller shortcuts while its character page is active.
// Keep native keyboard mappings and never steal a remapped Back/navigation key.
template<class Bindings>
void inventory_light_keys(Bindings& bindings,const std::set<std::string>& reserved) {
    for(auto& binding:bindings) {
        if(binding.action!="toggle_light" && binding.action!="tertiary") continue;
        std::erase_if(binding.keys,[](const auto& key){return key.starts_with("Gamepad_");});
        const std::string key=binding.action=="toggle_light"?"Gamepad_FaceButton_Top":"Gamepad_Special_Left";
        if(!reserved.contains(key)) binding.keys.push_back(key);
    }
}
}

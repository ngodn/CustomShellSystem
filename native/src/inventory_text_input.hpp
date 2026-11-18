#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace css {
enum class InventoryInputRole { PickerSearch, AnimationSearch, ProfileName };

class InventoryTextInput {
    std::optional<InventoryInputRole> role_;
    std::string value_;
public:
    bool bind(InventoryInputRole role,std::string_view value,bool focused) {
        const bool role_changed=role_!=role;
        if(!role_changed && (value_==value || (focused && role==InventoryInputRole::ProfileName))) return false;
        role_=role;
        value_=value;
        return role_changed || !focused;
    }
};
}

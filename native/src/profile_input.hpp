#pragma once
#include <string_view>

namespace css {
class ProfileSaveShortcut {
    bool enabled_=false, down_=false;
public:
    static constexpr std::string_view key="Enter";
    bool update(bool enabled,bool down) {
        const bool submit=enabled && enabled_ && down && !down_;
        enabled_=enabled;
        down_=down;
        return submit;
    }
};
}

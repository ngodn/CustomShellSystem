#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace css {
class AnimationTrace {
    void (*sink_)(const char*) noexcept = nullptr;
    size_t remaining_=0;
public:
    void enable(void (*sink)(const char*) noexcept) { sink_=sink;remaining_=96; }
    explicit operator bool() const { return sink_ && remaining_; }
    void write(std::string_view event) {
        if(!*this) return;
        --remaining_;
        sink_(("[DEBUG-idle-load] "+std::string(event)).c_str());
        if(!remaining_) sink_("[DEBUG-idle-load] Capture limit reached");
    }
};
}

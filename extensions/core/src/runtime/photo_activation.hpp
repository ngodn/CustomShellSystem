#pragma once
#include <cstdint>

namespace cssx {
struct PhotoActivation {
    enum Action { wait, try_activate, panel, closed };
    bool live = false;
    uint64_t next_attempt = 0;
    uint64_t active_since = 0;

    Action poll(uint64_t now, bool active) {
        if (active) {
            if (!live) active_since = now;
            live = true;
            return panel;
        }
        if (live) return closed;
        if (now < next_attempt) return wait;
        next_attempt = now + 250;
        return try_activate;
    }
};
}

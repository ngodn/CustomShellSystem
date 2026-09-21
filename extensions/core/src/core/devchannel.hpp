#pragma once
// File request channel for tools and tests. Enabled only when
// Mods/CSSX/dev/enabled.txt exists at core start. Waits on a directory change
// notification (zero-timeout check per tick), reads runtime/request.json when
// its id changes and writes runtime/response.json. No periodic stat calls.
#include "common.hpp"
#include <functional>
#include <windows.h>

namespace cssx {
class DevChannel {
    fs::path root_;
    HANDLE watch_=INVALID_HANDLE_VALUE;
    std::string last_id_;
    bool enabled_=false;
    std::function<Json(const Json&)> handler_;
public:
    DevChannel(fs::path root,std::function<Json(const Json&)> handler);
    ~DevChannel();
    bool enabled() const { return enabled_; }
    // Game thread, once per tick. Returns true when a request was served.
    bool poll();
};
}

#pragma once
// Extension lifecycle: discovery, native/Lua loading, model/event routing,
// coalesced ticks, per-frame render dispatch, suspension and retryable stop.
// Portable: the engine-facing services arrive through HostServices so this
// compiles on Linux for the lifecycle test.
#include "common.hpp"
#include "manifest.hpp"
#include "storage.hpp"
#include "cssx/abi.h"
#include "cssx/hud.h"
#include <functional>
#include <memory>
#include <vector>

namespace cssx {
struct HostServices {
    // Engine/host operation with the calling extension id stamped in "extension".
    std::function<Json(const Json&)> request;
    const CssxHudApi* hud=nullptr;
    // Optional: sink for framework log lines (defaults to the storage log).
    std::function<void(const std::string& level,const std::string& message,const Json& fields)> log;
};
struct ExtensionCost {
    uint64_t tick_calls=0, tick_us=0, render_calls=0, render_us=0, model_calls=0, model_us=0, event_calls=0, event_us=0, requests=0;
    Json json() const;
};
class Runtime {
public:
    struct Entry;
    Runtime(fs::path root,HostServices services);
    ~Runtime();
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
    // UI-facing requests: library, model, event, status. Throws on error.
    Json request(const Json&);
    Json library();                    // {revision, extensions:[...], errors:[...]}
    Json model(const std::string& id);
    void event(const std::string& id,const Json& event);
    void tick(double seconds);
    void render(const CssxFrame& frame);
    bool needs_frame() const;
    bool stop();                        // true when every extension restored its changes
    bool stopped() const { return stopped_; }
    uint64_t revision() const { return revision_; }
    size_t count() const { return entries_.size(); }
    const fs::path& root() const { return root_; }
    Storage& storage() { return storage_; }
    void invalidate(const std::string& id);
    void log(const std::string& id,const std::string& level,const std::string& message,const Json& fields=Json::object());
private:
    fs::path root_;
    HostServices services_;
    Storage storage_;
    std::vector<std::unique_ptr<Entry>> entries_;
    Json errors_=Json::array();
    uint64_t revision_=1;
    bool stopped_=false;
    Entry& find(const std::string& id);
};
}

#pragma once
// CSSX service bus: finds native modules that export cssx_services() (include/cssx/service.h)
// and forwards extension calls to them. Knows no service names. The module scan and the
// "still loaded" check are injected, so the whole bus runs in host tests; the core supplies
// the Windows versions (EnumProcessModules / GetModuleFileNameW).
#include "common.hpp"
#include <cssx/service.h>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cssx {

// One loaded module that exports cssx_services. `handle` is opaque to the bus.
struct ServiceProvider {
    void* handle = nullptr;
    std::string module;            // full path, also the identity across rescans
    CssxServicesFn get = nullptr;
};

struct ServiceEntry {
    std::string name, description;
    uint32_t version = 0;
    CssxServiceCall call = nullptr;
    void* provider = nullptr;
};

inline constexpr uint32_t max_services_per_table = 64;
inline constexpr size_t max_service_reply = 4u << 20;   // 4 MiB; a longer reply is an error

bool valid_service_name(const std::string&);
// Valid entries of a table, in order; invalid entries and repeated names are skipped and
// described in `problems` (when given). A null or malformed table yields nothing.
std::vector<ServiceEntry> read_service_table(const CssxServiceTable*, std::vector<std::string>* problems = nullptr);
// Calls one entry with a JSON request. Returns the reply; throws with the provider's error
// text when it refuses, or a description when the reply is missing, too long or not JSON.
Json call_service(const ServiceEntry&, const Json& request);

class ServiceBus {
public:
    using Scan = std::function<std::vector<ServiceProvider>()>;
    using Alive = std::function<bool(const ServiceProvider&)>;
    using Log = std::function<void(const std::string& level, const std::string& message, const Json& fields)>;
    ServiceBus(Scan scan, Alive alive, Log log = {});

    // [{name, version, description, module}] for every service a live provider offers now.
    Json list(double now_seconds);
    // Forwards `request` to the named service. `min_version` 0 accepts any version.
    Json call(const std::string& name, uint32_t min_version, const Json& request, double now_seconds);
    // Drop every cached provider (the core is stopping or a world teardown wants a clean slate).
    void reset();

    static constexpr double rescan_interval = 1.0;   // seconds between scans on a miss

private:
    void rescan(double now, bool force);
    std::optional<ServiceEntry> lookup(const std::string& name, const ServiceProvider*& from);
    Scan scan_;
    Alive alive_;
    Log log_;
    std::vector<ServiceProvider> providers_;
    double scanned_at_ = -1e9;
    bool scanned_ = false;
    std::map<std::string, std::string> reported_;   // module -> last logged problem summary
};

} // namespace cssx

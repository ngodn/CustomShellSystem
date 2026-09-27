#pragma once
#include "engine.hpp"
#include "probe_schedule.hpp"
#include "writer.hpp"
#include <filesystem>

namespace ccs {
class DiscoveryProbe {
public:
    explicit DiscoveryProbe(const std::filesystem::path& output);
    void toggle();
    void cancel();
    void tick(void* engine);
    const char* state() const;
private:
    bool capture_step(void* engine);
#ifdef CCS_REGISTRY_CONTROLS
    bool registry_step();
    bool log_drained();
    engine::ObjectHandle registry_, known_montage_;
    std::string known_package_, known_path_, negative_package_;
#endif
    void emit(nlohmann::json record);
    void finish();
    runtime::Writer writer_;
    runtime::ProbeSchedule schedule_;
    engine::ObjectHandle pc_, pawn_, asc_;
    std::vector<const void*> grants_;
    size_t index_{0};
    unsigned phase_{0};
    uint64_t retry_at_{0};
    uint64_t session_{0};
    bool reported_{true};
};
}

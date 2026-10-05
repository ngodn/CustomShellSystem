#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;
using PerformanceCall = std::function<Json(const Json&)>;

struct PerformanceChoice {
    std::string id = "off", library;
    double rate = 1., seconds = 0.;
    static PerformanceChoice parse(const Json&);
    Json json() const;
};
struct PerformanceEntry {
    std::string id, name, library, library_name, playback, reason;
    bool available = false;
};
class PerformanceCatalog {
    std::vector<PerformanceEntry> entries_;
    std::string error_ = "Refresh animations after choosing your outfit in CSS.";
public:
    void refresh(const PerformanceCall&);
    const PerformanceEntry* find(const std::string& id) const;
    Json libraries(const PerformanceChoice&) const;
    Json options(const PerformanceChoice&) const;
    std::string library(const PerformanceChoice&) const;
    std::string unavailable(const PerformanceChoice&) const;
    std::string description(const PerformanceChoice&) const;
};

class PerformanceTake {
    uint64_t token_ = 0;
    std::string id_;
public:
    void start(const PerformanceCall&, const PerformanceChoice&);
    // Empty on normal playback/completion; a reason when gameplay interrupted it.
    std::string poll(const PerformanceCall&);
    void stop(const PerformanceCall&);
    bool owned() const { return token_ != 0; }
};
}

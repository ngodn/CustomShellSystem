#include "performance.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <map>
#include <stdexcept>

namespace cine {
namespace {
std::string text(const Json& j, const char* key, size_t limit) {
    const auto s = j.at(key).get<std::string>();
    if (s.empty() || s.size() > limit || s.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid animation catalog text");
    return s;
}
void validate(const PerformanceChoice& c) {
    if (c.id.empty() || c.id.size() > 96 || c.library.size() > 96 || c.id.find('\0') != std::string::npos || c.library.find('\0') != std::string::npos ||
        !std::isfinite(c.rate) || c.rate < .1 || c.rate > 4. || !std::isfinite(c.seconds) || c.seconds < 0. || c.seconds > 86400.)
        throw std::runtime_error("Invalid performance selection, rate or start time");
}
}
PerformanceChoice PerformanceChoice::parse(const Json& j) {
    PerformanceChoice c;
    c.id = j.value("id", std::string("off")); c.library = j.value("library", std::string());
    c.rate = j.value("rate", 1.); c.seconds = j.value("seconds", 0.);
    validate(c); return c;
}
Json PerformanceChoice::json() const { return {{"id", id}, {"library", library}, {"rate", rate}, {"seconds", seconds}}; }

void PerformanceCatalog::refresh(const PerformanceCall& call) {
    entries_.clear();
    try {
        const auto result = call({{"action", "performances"}});
        const auto& entries = result.at("entries");
        if (!entries.is_array() || entries.size() > 4096) throw std::runtime_error("Invalid animation catalog size");
        std::vector<PerformanceEntry> parsed;
        std::set<std::string> ids, libraries;
        std::map<std::string, size_t> counts;
        for (const auto& e : entries) {
            PerformanceEntry entry;
            entry.id = text(e, "id", 96); entry.name = text(e, "name", 160);
            entry.library = text(e, "library", 96); entry.library_name = text(e, "library_name", 160);
            entry.playback = text(e, "playback", 8); entry.available = e.at("available").get<bool>();
            entry.reason = e.value("reason", std::string());
            if (entry.reason.size() > 1024 || (entry.playback != "once" && entry.playback != "loop" && entry.playback != "pose") || !ids.insert(entry.id).second)
                throw std::runtime_error("Invalid or duplicate performance entry");
            libraries.insert(entry.library);
            if (libraries.size() > 64 || ++counts[entry.library] > 256)
                throw std::runtime_error("Animation library exceeds the browser limit");
            parsed.push_back(std::move(entry));
        }
        entries_ = std::move(parsed);
        error_ = entries_.empty() ? "Install CSS Locomotion Shared Assets, then refresh animations." : "";
    } catch (const std::exception& e) { error_ = std::string("Animation library unavailable: ") + e.what(); }
}
const PerformanceEntry* PerformanceCatalog::find(const std::string& id) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& e) { return e.id == id; });
    return it == entries_.end() ? nullptr : &*it;
}
std::string PerformanceCatalog::library(const PerformanceChoice& c) const {
    if (!c.library.empty()) return c.library;
    if (const auto* entry = find(c.id)) return entry->library;
    return entries_.empty() ? "none" : entries_.front().library;
}
Json PerformanceCatalog::libraries(const PerformanceChoice& c) const {
    Json result = Json::array(); std::set<std::string> seen;
    for (const auto& e : entries_) if (seen.insert(e.library).second) result.push_back({{"id", e.library}, {"label", e.library_name}});
    const auto selected = library(c);
    if (!seen.contains(selected)) result.push_back({{"id", selected}, {"label", "No installed library"}});
    return result;
}
Json PerformanceCatalog::options(const PerformanceChoice& c) const {
    Json result = Json::array({{{"id", "off"}, {"label", "Off (use locomotion)"}}});
    const auto selected = library(c); bool found = c.id == "off";
    for (const auto& e : entries_) if (e.library == selected) {
        result.push_back({{"id", e.id}, {"label", e.name + (e.available ? "" : " (unavailable)")}});
        found = found || e.id == c.id;
    }
    if (!found) result.push_back({{"id", c.id}, {"label", "Unavailable: " + c.id}});
    return result;
}
std::string PerformanceCatalog::unavailable(const PerformanceChoice& c) const {
    if (c.id == "off") return {};
    if (!error_.empty()) return error_;
    const auto* entry = find(c.id);
    if (!entry) return "The selected animation is no longer installed. Choose another or Off.";
    return entry->available ? "" : entry->reason.empty() ? "This animation is unavailable for the worn outfit." : entry->reason;
}
std::string PerformanceCatalog::description(const PerformanceChoice& c) const {
    if (const auto why = unavailable(c); !why.empty()) return why;
    if (c.id == "off") return error_.empty() ? "Your normal locomotion stays active." : error_;
    const auto* entry = find(c.id);
    return entry->name + (entry->playback == "loop" ? ": loops until the take ends." : entry->playback == "pose" ? ": holds its final pose until the take ends." : ": plays once, then returns to locomotion.");
}

void PerformanceTake::start(const PerformanceCall& call, const PerformanceChoice& choice) {
    validate(choice);
    if (owned()) throw std::runtime_error("This take already owns a performance");
    if (choice.id == "off") return;
    // Retain only the returned ownership token. Saved settings never contain session state.
    const auto started = call({{"action", "performance"}, {"op", "start"}, {"id", choice.id}, {"rate", choice.rate}, {"seconds", choice.seconds}});
    token_ = started.at("token").get<uint64_t>(); id_ = choice.id;
    if (!token_ || started.at("id") != id_ || !started.at("active").get<bool>()) {
        try { stop(call); } catch (...) {}
        throw std::runtime_error("CSS did not start the selected performance");
    }
}
std::string PerformanceTake::poll(const PerformanceCall& call) {
    if (!owned()) return {};
    const auto state = call({{"action", "performance"}, {"op", "status"}});
    if (state.at("token").get<uint64_t>() != token_ || state.at("id") != id_) {
        token_ = 0; return "Another performance replaced this take's animation.";
    }
    if (!state.at("active").get<bool>()) { token_ = 0; return state.value("reason", std::string()); }
    return {};
}
void PerformanceTake::stop(const PerformanceCall& call) {
    if (!owned()) return;
    const auto state = call({{"action", "performance"}, {"op", "status"}});
    if (state.at("token").get<uint64_t>() == token_ && state.at("id") == id_ && state.at("active").get<bool>())
        call({{"action", "performance"}, {"op", "stop"}, {"token", token_}});
    token_ = 0;
}
}

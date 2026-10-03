#include "services.hpp"
#include <algorithm>
#include <set>

namespace cssx {

bool valid_service_name(const std::string& name) {
    if (name.size() < 3 || name.size() > 64) return false;
    if (name.front() == '.' || name.back() == '.' || name.find('.') == std::string::npos) return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}

namespace {
// Bounded read of a provider string: never walks past 256 bytes of foreign memory.
std::optional<std::string> bounded(const char* text, size_t limit) {
    if (!text) return std::nullopt;
    size_t n = 0;
    while (n <= limit && text[n]) ++n;
    if (n > limit) return std::nullopt;
    return std::string(text, n);
}
}

std::vector<ServiceEntry> read_service_table(const CssxServiceTable* table, std::vector<std::string>* problems) {
    std::vector<ServiceEntry> out;
    auto problem = [&](std::string text) { if (problems) problems->push_back(std::move(text)); };
    if (!table) return out;
    if (table->abi != CSSX_SERVICE_ABI) { problem("service ABI " + std::to_string(table->abi) + " is not supported (CSSX speaks 1)"); return out; }
    if (table->size < sizeof(CssxServiceTable)) { problem("service table is too small"); return out; }
    if (table->count > max_services_per_table) { problem("service table lists more than 64 services"); return out; }
    if (table->count && !table->services) { problem("service table has no entries pointer"); return out; }
    std::set<std::string> seen;
    for (uint32_t i = 0; i < table->count; ++i) {
        const auto& s = table->services[i];
        const auto name = bounded(s.name, 64);
        if (!name || !valid_service_name(*name)) { problem("entry " + std::to_string(i) + " has an invalid name"); continue; }
        if (!s.call) { problem(*name + " has no call function"); continue; }
        if (s.version == 0) { problem(*name + " has version 0"); continue; }
        if (!seen.insert(*name).second) { problem(*name + " is listed twice"); continue; }
        out.push_back({*name, bounded(s.description, 256).value_or(std::string()), s.version, s.call, s.provider});
    }
    return out;
}

Json call_service(const ServiceEntry& entry, const Json& request) {
    struct Reply { std::string text; bool overflow = false, twice = false, received = false; } reply;
    const auto text = request.dump();
    // C boundary: the sink must not throw. A failed allocation reads as an overflow.
    const CssxServiceSink sink = [](void* context, const char* data, size_t size) {
        auto& r = *static_cast<Reply*>(context);
        if (r.received) { r.twice = true; return; }
        r.received = true;
        if (size > max_service_reply || (size && !data)) { r.overflow = true; return; }
        try { r.text.assign(data ? data : "", size); } catch (...) { r.overflow = true; }
    };
    const int ok = entry.call(entry.provider, text.data(), text.size(), sink, &reply);
    if (reply.overflow) throw std::runtime_error(entry.name + " sent a reply larger than 4 MiB");
    if (!reply.received) throw std::runtime_error(entry.name + (ok ? " sent no reply" : " refused the call"));
    auto value = Json::parse(reply.text, nullptr, false);
    if (value.is_discarded()) throw std::runtime_error(entry.name + " sent a reply that is not JSON");
    if (!ok) {
        const auto* error = value.is_object() && value.contains("error") && value["error"].is_string() ? &value["error"] : nullptr;
        throw std::runtime_error(error ? error->get<std::string>() : entry.name + " refused the call");
    }
    return value;
}

ServiceBus::ServiceBus(Scan scan, Alive alive, Log log) : scan_(std::move(scan)), alive_(std::move(alive)), log_(std::move(log)) {}

void ServiceBus::reset() { providers_.clear(); scanned_ = false; scanned_at_ = -1e9; }

void ServiceBus::rescan(double now, bool force) {
    if (!force && scanned_ && now - scanned_at_ < rescan_interval) return;
    scanned_ = true; scanned_at_ = now;
    try { providers_ = scan_(); }
    catch (const std::exception& e) { providers_.clear(); if (log_) log_("warning", "Service scan failed", {{"error", e.what()}}); }
}

std::optional<ServiceEntry> ServiceBus::lookup(const std::string& name, const ServiceProvider*& from) {
    for (const auto& p : providers_) {
        if (!p.get || !alive_(p)) continue;
        std::vector<std::string> problems;
        const auto entries = read_service_table(p.get(), &problems);
        if (!problems.empty() && log_) {
            std::string summary;
            for (const auto& x : problems) summary += x + "; ";
            auto& last = reported_[p.module];
            if (last != summary) { last = summary; log_("warning", "A service provider has problems", {{"module", p.module}, {"problems", problems}}); }
        }
        for (const auto& e : entries) if (e.name == name) { from = &p; return e; }
    }
    return std::nullopt;
}

Json ServiceBus::list(double now) {
    rescan(now, false);
    Json out = Json::array();
    std::set<std::string> seen;
    for (const auto& p : providers_) {
        if (!p.get || !alive_(p)) continue;
        for (const auto& e : read_service_table(p.get())) {
            if (!seen.insert(e.name).second) continue;   // the first live provider wins, as in call()
            out.push_back({{"name", e.name}, {"version", e.version}, {"description", e.description}, {"module", p.module}});
        }
    }
    return out;
}

Json ServiceBus::call(const std::string& name, uint32_t min_version, const Json& request, double now) {
    if (!valid_service_name(name)) throw std::invalid_argument("Invalid service name");
    const ServiceProvider* from = nullptr;
    auto entry = scanned_ ? lookup(name, from) : std::nullopt;
    if (!entry) { rescan(now, false); entry = lookup(name, from); }
    if (!entry) throw std::runtime_error("No service named " + name + " is loaded");
    if (entry->version < min_version)
        throw std::runtime_error(name + " is version " + std::to_string(entry->version) + "; this caller needs " + std::to_string(min_version) + " or newer");
    return call_service(*entry, request);
}

} // namespace cssx

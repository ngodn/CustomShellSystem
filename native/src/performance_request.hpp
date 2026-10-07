#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace css {
struct PerformanceContext {
    uint64_t player=0, appearance=0, catalog=0;
    std::string outfit, variant;
    std::string mesh;
    bool operator==(const PerformanceContext&) const = default;
};
// Ownership is separate from CINE's explicit sessions. A failed automatic start
// is retried only after the selection, mesh, player or catalog changes.
class AutomaticIdlePlayback {
    std::optional<PerformanceContext> context_, failed_context_;
    std::string id_, failed_id_;
    uint64_t token_=0;
public:
    bool active() const { return token_!=0; }
    bool owns(uint64_t token) const { return token_ && token==token_; }
    bool matches(const PerformanceContext& context,const std::string& id) const {
        return context_ && *context_==context && id_==id;
    }
    bool retry_allowed(const PerformanceContext& context,const std::string& id) const {
        return !failed_context_ || *failed_context_!=context || failed_id_!=id;
    }
    void begin(PerformanceContext context,std::string id,uint64_t token) {
        if(!token || id.empty()) throw std::invalid_argument("Invalid automatic idle session");
        context_=std::move(context);id_=std::move(id);token_=token;
        failed_context_.reset();failed_id_.clear();
    }
    void fail(PerformanceContext context,std::string id) {
        clear();failed_context_=std::move(context);failed_id_=std::move(id);
    }
    void clear() { context_.reset();id_.clear();token_=0; }
};
class PendingPerformance {
public:
    enum class State { Empty, Waiting, Ready, Changed, Expired };
private:
    std::optional<PerformanceContext> context_;
    std::string id_;
    uint64_t started_=0;
public:
    void begin(std::string id,PerformanceContext context,uint64_t now) {
        if(id.empty() || id.size()>96 || context.outfit.empty()!=context.variant.empty() ||
           (context.outfit.empty() && context.mesh.empty()))
            throw std::runtime_error("Invalid performance selection");
        id_=std::move(id);context_=std::move(context);started_=now;
    }
    bool active() const { return context_.has_value(); }
    const std::string& id() const { return id_; }
    void clear() { context_.reset();id_.clear(); }
    State poll(const PerformanceContext& current,uint64_t now,bool menu_open) const {
        if(!context_) return State::Empty;
        if(current!=*context_) return State::Changed;
        if(now<started_ || now-started_>=5000) return State::Expired;
        if(menu_open || now-started_<200) return State::Waiting;
        return State::Ready;
    }
};
}

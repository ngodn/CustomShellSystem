#pragma once
#include <algorithm>
#include <cstdint>
#include <string_view>

namespace css {
inline bool is_stock_mesh_reset(std::string_view current, std::string_view applied,
                               std::string_view captured, std::string_view definition) {
    return !current.empty() && !applied.empty() && current!=applied &&
           (current==captured || current==definition);
}
inline bool is_mesh_switch_traversal(std::string_view ability) {
    // GA_ShellTraversal* are persistent detach/recall listeners, including at new game.
    // Their ActiveCount stays nonzero outside traversal and must not block recovery.
    return ability.starts_with("GA_Traversal_");
}
// Time comes from the monotonic host clock, independent of world pause/travel.
class Recovery {
    bool pending_ = false;
    uint64_t retry_at_ = 0;
    unsigned failures_ = 0;
public:
    void observe(bool enabled, bool automatic, bool selected, bool changed, bool stock_reset) {
        if(!enabled || !automatic || !selected) { clear(); return; }
        if(changed) { clear(); pending_=true; }
        else if(stock_reset) pending_=true;
    }
    bool due(uint64_t now) const { return pending_ && now>=retry_at_; }
    bool pending() const { return pending_; }
    void failed(uint64_t now) {
        if(pending_) retry_at_=now+std::min<uint64_t>(8000,500ULL<<std::min(failures_++,4U));
    }
    void clear() { pending_=false; retry_at_=0; failures_=0; }
};
}

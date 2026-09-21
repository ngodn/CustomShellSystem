#pragma once
// The reflection request bridge: opaque object handles, property decode and
// encode, reflected calls, DataTable rows, checked map edits, input reads and
// the managed hook rules. Ported from the validated CSS 0.4.x bridge with two
// changes: handle creation is O(log n) through a reverse index, and hooks go
// to the loader's hook host instead of CSS.
#include "engine.hpp"
#include "core_abi.h"
#include <map>
#include <set>
#include <unordered_map>

namespace cssx {
class Bridge {
public:
    explicit Bridge(const CssxHookHost* hooks);
    ~Bridge();
    Bridge(const Bridge&)=delete;
    Bridge& operator=(const Bridge&)=delete;
    // Engine ops. `player` is the current player context computed by the core
    // this frame. Throws on error.
    Json request(const engine::PlayerContext& player,const Json& request);
    Json handle(engine::UObject* object);          // {$object,name,class} or null
    uint64_t track(engine::UObject* object);       // id only, 0 for null
    engine::UObject* resolve(const Json& value);   // throws when expired
    bool stop_hooks();                             // remove every rule; false while one is busy
    Json hook_stats() const;
    struct HookGroup;
private:
    const CssxHookHost* hooks_;
    std::map<engine::UFunction*,std::unique_ptr<HookGroup>> groups_;
    uint64_t next_hook_=1;
    static void hook_callback(void*,void*,void*,void*);
    Json hook_request(const Json&);
    struct Tracked { engine::WeakObject weak; };
    std::map<uint64_t,Tracked> objects_;
    std::unordered_map<engine::UObject*,uint64_t> reverse_;
    std::map<std::string,engine::WeakObject> defaults_;
    uint64_t next_=1, sweep_=0;
    Json decode(engine::FProperty*,void*,unsigned);
    void encode(engine::FProperty*,void*,const Json&,unsigned);
};
}

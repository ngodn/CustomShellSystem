#include "hook_service.hpp"
#include <windows.h>
#include <atomic>
#include <array>
#include <algorithm>
#include <map>
#include <vector>
#include <stdexcept>
#include <Unreal/UFunction.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/Hooks/Hooks.hpp>

using namespace RC::Unreal;
struct CssxHookService::State {
    struct Slot {
        UFunction* function=nullptr;
        CssxHookCallback callback=nullptr; void* user=nullptr;
        CallbackId native_id=0;
        bool rooted=false, pre=false; unsigned running=0;
    };
    std::shared_ptr<std::recursive_mutex> gate;
    std::atomic<DWORD> thread{0};
    std::atomic<bool> enabled{false};
    std::map<uint64_t,std::shared_ptr<Slot>> slots;
    struct ScriptRegistry {
        Hook::GlobalCallbackId id=0;
        std::map<UFunction*,std::vector<std::shared_ptr<Slot>>> scripts;
        // Lock-free pre-filter for the global script callback: the hooked
        // Blueprint functions, published under the gate, read without it. Every
        // Blueprint call in the game passes through that callback while a
        // Blueprint rule exists, so the common case (not ours) must cost a few
        // loads and no mutex.
        std::array<std::atomic<UFunction*>,128> fast{};
        std::atomic<uint32_t> fast_count{0};
        void publish_fast() {
            uint32_t n=0;
            for(const auto& [function,slots]:scripts) { if(n>=fast.size()) break; fast[n++].store(function,std::memory_order_relaxed); }
            fast_count.store(n,std::memory_order_release);
        }
        bool maybe_hooked(UFunction* node) const noexcept {
            const auto n=fast_count.load(std::memory_order_acquire);
            for(uint32_t i=0;i<n;++i) if(fast[i].load(std::memory_order_relaxed)==node) return true;
            return false;
        }
    };
    ScriptRegistry before,after;
    struct Root { unsigned users=0; bool owned=false; };
    std::map<UFunction*,Root> roots;
    uint64_t next=1;
    bool stopped=false;
    // Accounting for the performance record (QPC ticks).
    std::atomic<uint64_t> calls{0}, ticks{0};
    explicit State(std::shared_ptr<std::recursive_mutex> g):gate(std::move(g)) {}
    static void invoke(State& state,const std::shared_ptr<Slot>& slot,UObject* object,FFrame& frame,void* result) noexcept {
        if(!slot->callback) return;
        LARGE_INTEGER a,b; QueryPerformanceCounter(&a);
        ++slot->running; slot->callback(slot->user,object,&frame,result); --slot->running;
        QueryPerformanceCounter(&b);
        state.calls.fetch_add(1,std::memory_order_relaxed);
        state.ticks.fetch_add(uint64_t(b.QuadPart-a.QuadPart),std::memory_order_relaxed);
    }
};
CssxHookService::CssxHookService(std::shared_ptr<std::recursive_mutex> gate)
    :state_(std::make_shared<State>(std::move(gate))),api_{1,sizeof(CssxHookHost),this,add,remove,stats,add_script_pre} {}
void CssxHookService::game_thread() { state_->thread=GetCurrentThreadId(); }
uint64_t CssxHookService::add(void* context,void* target,CssxHookCallback callback,void* user) {
    return register_hook(context,target,callback,user,false);
}
uint64_t CssxHookService::add_script_pre(void* context,void* target,CssxHookCallback callback,void* user) {
    return register_hook(context,target,callback,user,true);
}
uint64_t CssxHookService::register_hook(void* context,void* target,CssxHookCallback callback,void* user,bool pre) {
    auto state=static_cast<CssxHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        if(state->stopped || state->thread!=GetCurrentThreadId() || !target || !callback || state->slots.size()>=128) return 0;
        auto* function=static_cast<UFunction*>(target);
        if(function->HasAnyFunctionFlags(FUNC_Delegate|FUNC_MulticastDelegate) || (pre && function->HasAnyFunctionFlags(FUNC_Native))) return 0;
        auto slot=std::make_shared<State::Slot>(); slot->pre=pre; slot->function=function; slot->callback=callback; slot->user=user;
        const auto id=state->next++;
        state->slots.emplace(id,slot);
        try {
            auto& root=state->roots[function];
            if(!root.users && !function->IsRootSet()) { function->SetRootSet(); root.owned=true; }
            ++root.users; slot->rooted=true;
            if(function->HasAnyFunctionFlags(FUNC_Native)) {
                slot->native_id=function->RegisterPostHook([state,slot](UnrealScriptFunctionCallableContext& c,void*) {
                    if(!state->enabled || state->thread!=GetCurrentThreadId()) return;
                    std::lock_guard lock(*state->gate);
                    if(!state->stopped) State::invoke(*state,slot,c.Context,c.TheStack,c.RESULT_DECL);
                });
            } else {
                if(!UObject::ProcessLocalScriptFunctionInternal.is_ready()) throw std::runtime_error("Script dispatcher is unavailable");
                auto& registry=pre?state->before:state->after;
                registry.scripts[function].push_back(slot);
                registry.publish_fast();
                if(!registry.id) {
                    auto dispatch=[state,pre](auto&,UObject* object,FFrame& frame,void* result) {
                        if(!state->enabled || state->thread!=GetCurrentThreadId()) return;
                        auto& registry=pre?state->before:state->after;
                        if(!registry.maybe_hooked(frame.Node())) return;
                        std::lock_guard lock(*state->gate);
                        if(state->stopped) return;
                        const auto found=registry.scripts.find(frame.Node());
                        if(found==registry.scripts.end()) return;
                        for(const auto& slot:found->second) State::invoke(*state,slot,object,frame,result);
                    };
                    Hook::FCallbackOptions options{false,false,STR("CSSX"),pre?STR("ScriptEntry"):STR("ScriptDispatch")};
                    registry.id=pre?Hook::RegisterProcessLocalScriptFunctionPreCallback(dispatch,options):
                                    Hook::RegisterProcessLocalScriptFunctionPostCallback(dispatch,options);
                    if(!registry.id) throw std::runtime_error("Script callback registration failed");
                }
            }
            state->enabled=true; return id;
        } catch(...) { remove(context,id); return 0; }
    } catch(...) { return 0; }
}
int CssxHookService::remove(void* context,uint64_t id) {
    auto state=static_cast<CssxHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        auto found=state->slots.find(id); if(found==state->slots.end()) return 1;
        auto slot=found->second;
        if(slot->running) return 0;
        slot->callback=nullptr; slot->user=nullptr;
        auto& registry=slot->pre?state->before:state->after;
        if(slot->native_id) {
            auto& hooks=Internal::GetHookedFunctionsMap(); auto registered=hooks.find(slot->function);
            if(registered!=hooks.end() && registered->second.GetCallbackData(slot->native_id)) {
                slot->function->UnregisterHook(slot->native_id);
                if(registered->second.GetCallbackData(slot->native_id)) return 0;
            }
            slot->native_id=0;
        } else if(auto scripts=registry.scripts.find(slot->function); scripts!=registry.scripts.end()) {
            auto& values=scripts->second;
            values.erase(std::remove(values.begin(),values.end(),slot),values.end());
            if(values.empty()) registry.scripts.erase(scripts);
            registry.publish_fast();
        }
        if(slot->rooted) {
            auto root=state->roots.find(slot->function);
            if(root!=state->roots.end() && !--root->second.users) {
                if(root->second.owned) slot->function->ClearRootSet();
                state->roots.erase(root);
            }
            slot->rooted=false;
        }
        state->slots.erase(found); state->enabled=!state->slots.empty();
        if(registry.scripts.empty() && registry.id) { Hook::UnregisterCallback(registry.id); registry.id=0; }
        return 1;
    } catch(...) { return 0; }
}
void CssxHookService::stats(void* context,uint64_t* calls,uint64_t* ticks,int* script_hook_installed) {
    auto state=static_cast<CssxHookService*>(context)->state_;
    if(calls) *calls=state->calls.load(std::memory_order_relaxed);
    if(ticks) *ticks=state->ticks.load(std::memory_order_relaxed);
    if(script_hook_installed) *script_hook_installed=(state->before.id || state->after.id)?1:0;
}
void CssxHookService::quiesce() {
    std::lock_guard lock(*state_->gate); state_->stopped=true; state_->enabled=false;
    for(auto& [id,slot]:state_->slots) { slot->callback=nullptr; slot->user=nullptr; }
    for(auto* registry:{&state_->before,&state_->after}) { registry->scripts.clear(); registry->publish_fast(); }
}
CssxHookService::~CssxHookService() { quiesce(); }

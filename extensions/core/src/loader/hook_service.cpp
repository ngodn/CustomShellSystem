#include "hook_service.hpp"
#include <windows.h>
#include <atomic>
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
        bool rooted=false; unsigned running=0;
    };
    std::shared_ptr<std::recursive_mutex> gate;
    std::atomic<DWORD> thread{0};
    std::atomic<bool> enabled{false};
    Hook::GlobalCallbackId script_id=0;
    std::map<uint64_t,std::shared_ptr<Slot>> slots;
    std::map<UFunction*,std::vector<std::shared_ptr<Slot>>> scripts;
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
    :state_(std::make_shared<State>(std::move(gate))),api_{1,sizeof(CssxHookHost),this,add,remove,stats} {}
void CssxHookService::game_thread() { state_->thread=GetCurrentThreadId(); }
uint64_t CssxHookService::add(void* context,void* target,CssxHookCallback callback,void* user) {
    auto state=static_cast<CssxHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        if(state->stopped || state->thread!=GetCurrentThreadId() || !target || !callback || state->slots.size()>=128) return 0;
        auto* function=static_cast<UFunction*>(target);
        if(function->HasAnyFunctionFlags(FUNC_Delegate|FUNC_MulticastDelegate)) return 0;
        auto slot=std::make_shared<State::Slot>(); slot->function=function; slot->callback=callback; slot->user=user;
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
                state->scripts[function].push_back(slot);
                if(!state->script_id) {
                    // One global script-function interception for every Blueprint rule.
                    // Installed on the first Blueprint rule and removed with the last,
                    // so it costs nothing while no such rule exists.
                    state->script_id=Hook::RegisterProcessLocalScriptFunctionPostCallback([state](auto&,UObject* object,FFrame& frame,void* result) {
                        if(!state->enabled || state->thread!=GetCurrentThreadId()) return;
                        std::lock_guard lock(*state->gate);
                        if(state->stopped) return;
                        const auto found=state->scripts.find(frame.Node());
                        if(found==state->scripts.end()) return;
                        for(const auto& slot:found->second) State::invoke(*state,slot,object,frame,result);
                    },{false,false,STR("CSSX"),STR("ScriptDispatch")});
                    if(!state->script_id) throw std::runtime_error("Script callback registration failed");
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
        if(slot->native_id) {
            auto& hooks=Internal::GetHookedFunctionsMap(); auto registered=hooks.find(slot->function);
            if(registered!=hooks.end() && registered->second.GetCallbackData(slot->native_id)) {
                slot->function->UnregisterHook(slot->native_id);
                if(registered->second.GetCallbackData(slot->native_id)) return 0;
            }
            slot->native_id=0;
        } else if(auto scripts=state->scripts.find(slot->function); scripts!=state->scripts.end()) {
            auto& values=scripts->second;
            values.erase(std::remove(values.begin(),values.end(),slot),values.end());
            if(values.empty()) state->scripts.erase(scripts);
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
        if(state->scripts.empty() && state->script_id) { Hook::UnregisterCallback(state->script_id); state->script_id=0; }
        return 1;
    } catch(...) { return 0; }
}
void CssxHookService::stats(void* context,uint64_t* calls,uint64_t* ticks,int* script_hook_installed) {
    auto state=static_cast<CssxHookService*>(context)->state_;
    if(calls) *calls=state->calls.load(std::memory_order_relaxed);
    if(ticks) *ticks=state->ticks.load(std::memory_order_relaxed);
    if(script_hook_installed) *script_hook_installed=state->script_id?1:0;
}
void CssxHookService::quiesce() {
    std::lock_guard lock(*state_->gate); state_->stopped=true; state_->enabled=false;
    for(auto& [id,slot]:state_->slots) { slot->callback=nullptr; slot->user=nullptr; }
    state_->scripts.clear();
}
CssxHookService::~CssxHookService() { quiesce(); }

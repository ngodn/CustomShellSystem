#include "hook_host.hpp"
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
namespace {const CssHookHost* active_host=nullptr;}
struct CssHookService::State {
    struct Slot {
        UFunction* function=nullptr;
        CssHookCallback callback=nullptr;void* user=nullptr;
        CallbackId native_id=0;
        bool rooted=false;unsigned running=0;
    };
    std::shared_ptr<std::recursive_mutex> gate;
    std::atomic<DWORD> thread{0};
    std::atomic<bool> enabled{false};
    Hook::GlobalCallbackId script_id=0;
    std::map<uint64_t,std::shared_ptr<Slot>> slots;
    std::map<UFunction*,std::vector<std::shared_ptr<Slot>>> scripts;
    struct Root {unsigned users=0;bool owned=false;};
    std::map<UFunction*,Root> roots;
    uint64_t next=1;
    bool stopped=false;
    explicit State(std::shared_ptr<std::recursive_mutex> g):gate(std::move(g)){}
    static void invoke(const std::shared_ptr<Slot>& slot,UObject* object,FFrame& frame,void* result) noexcept {
        if(!slot->callback) return;
        ++slot->running;slot->callback(slot->user,object,&frame,result);--slot->running;
    }
};
CssHookService::CssHookService(std::shared_ptr<std::recursive_mutex> gate):state_(std::make_shared<State>(std::move(gate))),api_{1,sizeof(CssHookHost),this,add,remove} {active_host=&api_;}
void CssHookService::game_thread() noexcept {state_->thread=GetCurrentThreadId();}
uint64_t CssHookService::add(void* context,void* target,CssHookCallback callback,void* user) noexcept {
    auto state=static_cast<CssHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        if(state->stopped || state->thread!=GetCurrentThreadId() || !target || !callback || state->slots.size()>=128) return 0;
        auto* function=static_cast<UFunction*>(target);
        if(function->HasAnyFunctionFlags(FUNC_Delegate|FUNC_MulticastDelegate)) return 0;
        auto slot=std::make_shared<State::Slot>();slot->function=function;slot->callback=callback;slot->user=user;
        const auto id=state->next++;
        state->slots.emplace(id,slot);
        try {
            auto& root=state->roots[function];
            if(!root.users && !function->IsRootSet()) {function->SetRootSet();root.owned=true;}
            ++root.users;slot->rooted=true;
            if(function->HasAnyFunctionFlags(FUNC_Native)) {
                slot->native_id=function->RegisterPostHook([state,slot](UnrealScriptFunctionCallableContext& c,void*) {
                    if(!state->enabled || state->thread!=GetCurrentThreadId()) return;
                    std::lock_guard lock(*state->gate);
                    if(!state->stopped) State::invoke(slot,c.Context,c.TheStack,c.RESULT_DECL);
                });
            } else {
                if(!UObject::ProcessLocalScriptFunctionInternal.is_ready()) throw std::runtime_error("Script dispatcher is unavailable");
                state->scripts[function].push_back(slot);
                if(!state->script_id) {
                    state->script_id=Hook::RegisterProcessLocalScriptFunctionPostCallback([state](auto&,UObject* object,FFrame& frame,void* result) {
                        if(!state->enabled || state->thread!=GetCurrentThreadId()) return;
                        std::lock_guard lock(*state->gate);
                        if(state->stopped) return;
                        const auto found=state->scripts.find(frame.Node());
                        if(found==state->scripts.end()) return;
                        for(const auto& slot:found->second) State::invoke(slot,object,frame,result);
                    },{false,false,STR("CustomShellSystem"),STR("CSSXScriptDispatch")});
                    if(!state->script_id) throw std::runtime_error("Script callback registration failed");
                }
            }
            state->enabled=true;return id;
        } catch(...) {remove(context,id);return 0;}
    } catch(...) {return 0;}
}
bool CssHookService::remove(void* context,uint64_t id) noexcept {
    auto state=static_cast<CssHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        auto found=state->slots.find(id);if(found==state->slots.end()) return true;
        auto slot=found->second;
        if(slot->running) return false;
        // Late UE4SS snapshots retain only loader-owned closures and this slot.
        // Clear every pointer into reloadable code before unregistering.
        slot->callback=nullptr;slot->user=nullptr;
        if(slot->native_id) {
            auto& hooks=Internal::GetHookedFunctionsMap();auto registered=hooks.find(slot->function);
            if(registered!=hooks.end() && registered->second.GetCallbackData(slot->native_id)) {
                slot->function->UnregisterHook(slot->native_id);
                if(registered->second.GetCallbackData(slot->native_id)) return false;
            }
            slot->native_id=0;
        } else if(auto scripts=state->scripts.find(slot->function);scripts!=state->scripts.end()) {
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
        state->slots.erase(found);state->enabled=!state->slots.empty();
        if(state->scripts.empty() && state->script_id) {Hook::UnregisterCallback(state->script_id);state->script_id=0;}
        return true;
    } catch(...) {return false;}
}
void CssHookService::quiesce() noexcept {
    std::lock_guard lock(*state_->gate);state_->stopped=true;state_->enabled=false;
    // Shutdown can run off the game thread. Clear the callable pointers but
    // leave engine roots and native dispatch bookkeeping to process shutdown.
    for(auto& [id,slot]:state_->slots) {slot->callback=nullptr;slot->user=nullptr;}
    state_->scripts.clear();
}
CssHookService::~CssHookService() {if(active_host==&api_) active_host=nullptr;quiesce();}
extern "C" __declspec(dllexport) const CssHookHost* css_get_hook_host() noexcept {return active_host;}

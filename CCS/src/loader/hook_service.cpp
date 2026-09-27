#include "hook_service.hpp"
#include "hook_target.hpp"
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <map>
#include <limits>
#include <stdexcept>
#include <utility>
#include <Unreal/UFunction.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObjectArray.hpp>

using namespace RC::Unreal;
struct CcsHookService::State {
    struct Slot {
        UFunction* function{};
        int32_t index{-1};
        int32_t serial{};
        FName name;
        CallbackId native_id{};
        bool rooted{};
        ccs::runtime::HookTarget target;
        Slot(UFunction* value, CcsNativePreHook callback, void* user)
            : function(value), index(value->GetInternalIndex()), name(value->GetNamePrivate()), target(callback, user) {
            auto* item = FUObjectArray::IndexToObject(index);
            if (!item || item->GetUObject() != value || !item->IsValid(false))
                throw std::runtime_error("Native hook function is not live");
            serial = item->GetSerialNumber();
            if (serial <= 0) throw std::runtime_error("Native hook function serial is not initialized");
        }
        UFunction* get() const {
            auto* item = FUObjectArray::IndexToObject(index);
            if (!item || item->GetUObject() != function || !item->IsValid(false) ||
                !function->IsRootSet() || function->GetNamePrivate() != name || !function->IsA<UFunction>()) return nullptr;
            return item->GetSerialNumber() == serial ? function : nullptr;
        }
    };
    std::shared_ptr<std::recursive_mutex> gate;
    std::atomic<DWORD> thread{};
    std::atomic<bool> stopped{};
    std::atomic<uint64_t> calls{}, wrong_thread{}, failures{};
    std::map<uint64_t, std::shared_ptr<Slot>> slots;
    uint64_t next{1};
    bool pinned{};
    explicit State(std::shared_ptr<std::recursive_mutex> value) : gate(std::move(value)) {}
};
CcsHookService::CcsHookService(std::shared_ptr<std::recursive_mutex> gate)
    : state_(std::make_shared<State>(std::move(gate))),
      api_{CCS_HOOK_ABI_VERSION, sizeof(CcsHookHost), this, add, remove, statistics, on_game_thread} {}
bool CcsHookService::game_thread() noexcept {
    const auto current = GetCurrentThreadId();
    const auto recorded = state_->thread.load(std::memory_order_acquire);
    if (recorded) return recorded == current;
    DWORD expected = 0;
    state_->thread.compare_exchange_strong(expected, current);
    return state_->thread.load() == current;
}
uint64_t CcsHookService::add(void* context, void* function, CcsNativePreHook callback, void* user) noexcept {
    if (!context) return 0;
    const auto state = static_cast<CcsHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        if (state->stopped || state->thread != GetCurrentThreadId() || !function || !callback ||
            state->slots.size() >= 16 || state->next == std::numeric_limits<uint64_t>::max()) return 0;
        auto* value = static_cast<UFunction*>(function);
        auto* item = FUObjectArray::IndexToObject(value->GetInternalIndex());
        if (!item || item->GetUObject() != value || !item->IsValid(false) || item->GetSerialNumber() <= 0 ||
            !value->IsA<UFunction>() || !value->HasAnyFunctionFlags(FUNC_Native) ||
            value->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate)) return 0;
        for (const auto& [id, slot] : state->slots) if (slot->function == value) return 0;
        if (!state->pinned) {
            HMODULE module{};
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                   reinterpret_cast<LPCWSTR>(&CcsHookService::add), &module)) return 0;
            // Late framework closures must keep executable loader code until process exit.
            state->pinned = true;
        }
        auto slot = std::make_shared<State::Slot>(value, callback, user);
        const auto id = state->next++;
        state->slots.emplace(id, slot);
        try {
            if (!value->IsRootSet()) { value->SetRootSet(); slot->rooted = true; }
            slot->native_id = value->RegisterPreHook([state, slot](UnrealScriptFunctionCallableContext& call, void*) {
                if (state->stopped.load(std::memory_order_acquire)) return;
                if (state->thread.load(std::memory_order_relaxed) != GetCurrentThreadId()) {
                    state->wrong_thread.fetch_add(1, std::memory_order_relaxed); return;
                }
                try {
                    std::lock_guard lock(*state->gate);
                    if (state->stopped || !slot->get()) return;
                    const auto result = slot->target.invoke(call.Context, &call.TheStack, call.RESULT_DECL);
                    if (result != ccs::runtime::HookTarget::Result::Skipped) state->calls.fetch_add(1, std::memory_order_relaxed);
                    if (result == ccs::runtime::HookTarget::Result::Failed) state->failures.fetch_add(1, std::memory_order_relaxed);
                } catch (...) { state->failures.fetch_add(1, std::memory_order_relaxed); }
            });
            if (slot->native_id <= 0) throw std::runtime_error("Native pre-hook registration failed");
            return id;
        } catch (...) { remove(context, id); return 0; }
    } catch (...) { state->failures.fetch_add(1, std::memory_order_relaxed); return 0; }
}
int CcsHookService::remove(void* context, uint64_t id) noexcept {
    if (!context) return 0;
    const auto state = static_cast<CcsHookService*>(context)->state_;
    try {
        std::lock_guard lock(*state->gate);
        if (state->stopped || state->thread != GetCurrentThreadId()) return 0;
        const auto found = state->slots.find(id);
        if (found == state->slots.end()) return 1;
        const auto slot = found->second;
        if (!slot->target.detach()) return 0;
        auto* function = slot->get();
        if (!function) return 0;
        if (slot->native_id > 0) {
            auto& functions = Internal::GetHookedFunctionsMap();
            const auto registered = functions.find(function);
            if (registered != functions.end() && registered->second.GetCallbackData(slot->native_id)) {
                function->UnregisterHook(slot->native_id);
                // The framework defers physical removal while the function is executing.
                if (registered->second.GetCallbackData(slot->native_id)) return 0;
            }
            slot->native_id = 0;
        }
        if (slot->rooted) { function->ClearRootSet(); slot->rooted = false; }
        state->slots.erase(found);
        return 1;
    } catch (...) { state->failures.fetch_add(1, std::memory_order_relaxed); return 0; }
}
int CcsHookService::statistics(void* context, CcsHookStats* output) noexcept {
    if (!context || !output || output->size < sizeof(CcsHookStats)) return 0;
    const auto state = static_cast<CcsHookService*>(context)->state_;
    try {
        std::unique_lock lock(*state->gate, std::try_to_lock);
        if (!lock) return 0;
        CcsHookStats stats{};
        stats.size = sizeof(stats); stats.slots = static_cast<uint32_t>(state->slots.size());
        stats.stopped = state->stopped.load();
        for (const auto& [id, slot] : state->slots)
            stats.running = static_cast<uint32_t>(std::min<uint64_t>(std::numeric_limits<uint32_t>::max(),
                uint64_t(stats.running) + slot->target.running()));
        stats.calls = state->calls.load(); stats.wrong_thread = state->wrong_thread.load(); stats.failures = state->failures.load();
        *output = stats; return 1;
    } catch (...) { return 0; }
}
int CcsHookService::on_game_thread(void* context) noexcept {
    if (!context) return 0;
    const auto state = static_cast<CcsHookService*>(context)->state_;
    return !state->stopped.load() && state->thread.load() == GetCurrentThreadId();
}
bool CcsHookService::quiesce() noexcept {
    state_->stopped.store(true, std::memory_order_release);
    try {
        std::lock_guard lock(*state_->gate);
        bool idle = true;
        for (const auto& [id, slot] : state_->slots) slot->target.clear();
        for (const auto& [id, slot] : state_->slots) if (slot->target.running()) idle = false;
        // Off-thread shutdown leaves engine roots and dispatch bookkeeping to process teardown.
        return idle;
    } catch (...) { return false; }
}
CcsHookService::~CcsHookService() { quiesce(); }

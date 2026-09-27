#include "combat.hpp"
#include "rig.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <Unreal/FFrame.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
using namespace engine;
namespace {
uint64_t now_us() { return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
constexpr size_t max_cached_classes = 512;
// A class name's identity: the FName's comparison index and number, as stored (8 bytes in a
// shipping build). Cached per class so the hook never converts a name to text twice.
static_assert(sizeof(FName) == 8, "FName layout is the 8 byte shipping layout");
uint64_t name_key(const FName& name) { uint64_t key{}; std::memcpy(&key, &name, sizeof(key)); return key; }
bool ends_with(const std::string& s, const char* suffix) { const std::string_view v(suffix); return s.size() >= v.size() && s.compare(s.size() - v.size(), v.size(), v) == 0; }
}
Combat::Combat(Deps deps) : deps_(std::move(deps)) {
    if (!deps_.hooks || deps_.hooks->version != CCS_HOOK_ABI_VERSION || deps_.hooks->size < sizeof(CcsHookHost) ||
        !deps_.hooks->add_native_pre || !deps_.hooks->remove || !deps_.hooks->statistics || !deps_.hooks->on_game_thread)
        throw std::runtime_error("Combat engine needs the loader's native hook host");
}
int Combat::classify(const std::string& full) {
    std::string name = full;
    if (ends_with(name, "_C")) name.resize(name.size() - 2);
    // Sidearm primary fire: GA_<Sidearm>Attack_Primary and the shared ranged attack bases.
    if (name.starts_with("GA_") && !name.starts_with("GA_Player") &&
        (ends_with(name, "Attack_Primary") || ends_with(name, "Attack_Primary_InfiniteAmmo") || name == "GA_SidearmRangedAttackBase" ||
         name == "GA_SidearmRangedBurstAttackBase" || name == "GA_SidearmRangedChargedAttackBase")) return int(SlotId::R);
    if (!full.starts_with("GA_Player")) return -1;
    if (ends_with(name, "_A_Finisher") || ends_with(name, "_A3_Finisher")) return int(SlotId::LF);
    if (ends_with(name, "_B_Finisher") || ends_with(name, "_B3_Finisher")) return int(SlotId::HF);
    if (ends_with(name, "_Hold")) {
        const auto stem = name.substr(0, name.size() - 5);
        if (ends_with(stem, "_A1") || ends_with(stem, "_A2") || ends_with(stem, "_A3")) return int(SlotId::LC);
        if (ends_with(stem, "_B1") || ends_with(stem, "_B2") || ends_with(stem, "_B3")) return int(SlotId::HC);
        return -1;
    }
    if (ends_with(name, "_A1")) return int(SlotId::L1);
    if (ends_with(name, "_A2")) return int(SlotId::L2);
    if (ends_with(name, "_A3")) return int(SlotId::L3);
    if (ends_with(name, "_B1")) return int(SlotId::H1);
    if (ends_with(name, "_B2")) return int(SlotId::H2);
    if (ends_with(name, "_B3")) return int(SlotId::H3);
    return -1;
}
void Combat::set_enabled(bool on) { enabled_ = on; if (!on) active_ = false; }
void Combat::set_rate(double scale) {
    if (!std::isfinite(scale)) return;
    rate_ = std::clamp(scale, 0.25, 4.0);
}
void Combat::set_slot(SlotId slot, std::string move_id) {
    auto& s = slots_[size_t(slot)];
    if (s.move_id == move_id) return;
    release(s);
    s.move_id = std::move(move_id); s.error.clear(); s.hits = 0; s.path.clear();
    if (s.move_id.empty()) { s.pending = false; return; }
    if (s.move_id.starts_with("found:")) { s.path = s.move_id.substr(6); s.pending = s.path.starts_with("/Game/") && s.path.size() < 1024; if (!s.pending) s.error = "Invalid montage path"; return; }
    const auto* move = deps_.catalog ? deps_.catalog->find_move(s.move_id) : nullptr;
    if (!move) { s.error = "Unknown move"; s.pending = false; return; }
    s.path = move->montage_path; s.pending = true;
}
int Combat::assigned() const { int n = 0; for (const auto& s : slots_) if (!s.move_id.empty()) ++n; return n; }
void Combat::release(Slot& slot) {
    if (slot.rooted) { if (auto* montage = slot.montage.get()) montage->ClearRootSet(); slot.rooted = false; }
    slot.montage = {};
}
bool Combat::player_outer(UObject* object) const {
    if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) return false;
    const auto* pawn = pawn_.get(); const auto* asc = asc_.get();
    if (!pawn && !asc) return false;
    for (unsigned depth = 0; object && depth < 8; ++depth, object = object->GetOuterPrivate())
        if (object == pawn || object == asc) return true;
    return false;
}
// One montage per tick, on the game thread, only while a player exists: the load is blocking
// (a few milliseconds from the pak the first time) and the skeleton check needs the pawn's mesh.
void Combat::load_pending(const PlayerContext& player) {
    if (!player.pawn) return;
    for (auto& s : slots_) {
        if (!s.pending) continue;
        s.pending = false;
        try {
            auto* montage = load(s.path);
            if (!montage->IsA(static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage")))) throw std::runtime_error("Not an animation montage");
            // The montage must be authored on a rig the player's body can play: the game's human
            // skeleton, a CSS package's copy of it, or the exact rig of the body worn now (rig.hpp).
            auto* skeleton = object_of(montage, L"Skeleton");
            const auto rig_path = skeleton ? narrow(skeleton->GetPathName()) : std::string{};
            if (!rig::compatible(rig_path, rig::player_skeleton(player)))
                throw std::runtime_error("Montage is not on the player's rig (" + (skeleton ? rig::short_name(rig_path) : std::string("no skeleton")) + ")");
            if (!montage->IsRootSet()) { montage->SetRootSet(); s.rooted = true; }
            s.montage.capture(montage);
            s.error.clear();
            log("CCS slot ready: " + s.move_id + " -> " + narrow(montage->GetNamePrivate().ToString()));
        } catch (const std::exception& e) { s.error = e.what(); release(s); log("CCS slot load failed: " + s.move_id + ": " + e.what()); }
        return;
    }
}
void Combat::ensure_hook() {
    if (token_) return;
    auto* cdo = find(L"/Script/CSAbilityTasks.Default__AbilityTask_PlayMontageAndWaitWithNotifies");
    Call signature(cdo, L"PlayMontageAndWaitWithNotifies", 8);
    auto* function = signature.function();
    if (!function->HasAnyFunctionFlags(FUNC_Native) || function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate)) throw std::runtime_error("Montage task is not a native function");
    auto* ability = signature.param(L"OwningAbility"); auto* montage = signature.param(L"MontageToPlay"); auto* rate = signature.param(L"Rate");
    for (auto* p : {ability, montage, rate})
        if (p->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm) || p->GetOffset_Internal() < 0 || p->GetOffset_Internal() + p->GetElementSize() > function->GetParmsSize())
            throw std::runtime_error("Montage task parameter layout changed");
    for (auto* p : {ability, montage}) if (!p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*)) throw std::runtime_error("Montage task object parameter changed");
    if (!rate->IsA<FNumericProperty>() || rate->GetElementSize() != sizeof(float) || !static_cast<FNumericProperty*>(rate)->IsFloatingPoint()) throw std::runtime_error("Montage task rate parameter changed");
    auto* montage_class = static_cast<FObjectProperty*>(montage)->GetPropertyClass().Get();
    if (!montage_class || narrow(montage_class->GetPathName()) != "/Script/Engine.AnimMontage") throw std::runtime_error("Montage task montage class changed");
    WeakObject serial(function);
    if (serial.Get() != function) throw std::runtime_error("Montage task weak identity is unavailable");
    inputs_ = {ability, montage, rate};
    function_.capture(function);
    token_ = deps_.hooks->add_native_pre(deps_.hooks->context, function, callback, this);
    if (!token_) { inputs_ = {}; function_ = {}; throw std::runtime_error("Native montage pre-hook registration rejected"); }
    log("CCS combat hook installed");
}
void Combat::remove_hook() {
    active_ = false;
    if (!token_) return;
    if (!deps_.hooks->remove(deps_.hooks->context, token_)) { retry_after_ = 0; return; }   // deferred: retried next tick
    token_ = 0; inputs_ = {}; function_ = {};
    log("CCS combat hook removed");
}
bool Combat::stop() {
    enabled_ = false; active_ = false;
    remove_hook();
    if (token_) return false;
    for (auto& s : slots_) release(s);
    skeleton_ = {}; pawn_ = {}; asc_ = {};
    return true;
}
void Combat::tick(const PlayerContext& player, uint64_t now) {
    // Player identity for the callback's owner filter, refreshed twice a second.
    if (now >= player_check_) {
        player_check_ = now + 500;
        if (player.pawn != pawn_.get()) { pawn_ = {}; if (player.pawn) pawn_.capture(player.pawn); skeleton_ = {}; }
        if (player.asc != asc_.get()) { asc_ = {}; if (player.asc) asc_.capture(player.asc); }
    }
    const bool wanted = enabled_ && (assigned() > 0 || std::abs(rate_ - 1.0) > 1e-6);
    if (wanted) {
        load_pending(player);
        if (!token_ && now >= retry_after_) {
            try { ensure_hook(); error_.clear(); }
            catch (const std::exception& e) { error_ = e.what(); retry_after_ = now + 2000; log(std::string("CCS combat hook failed: ") + e.what()); }
        }
        active_ = token_ != 0 && player.pawn != nullptr;
    } else {
        active_ = false;
        if (token_) remove_hook();
    }
}
void Combat::callback(void* user, void*, void* frame, void*) noexcept {
    auto* self = static_cast<Combat*>(user);
    const auto started = now_us();
    try { self->observe(frame); }
    catch (...) { ++self->failures_; }
    const auto took = now_us() - started;
    if (took > self->maximum_us_) self->maximum_us_ = took;
}
void Combat::observe(void* frame_ptr) {
    ++seen_;
    if (!active_ || !function_.alive()) { ++skipped_; return; }
    auto* frame = static_cast<FFrame*>(frame_ptr);
    auto* locals = frame ? frame->Locals() : nullptr;
    if (!locals || frame->Node() != function_.get()) { ++wrong_frame_; return; }
    auto* bytes = static_cast<std::byte*>(static_cast<void*>(locals));
    auto* ability = static_cast<FObjectProperty*>(inputs_[0])->GetObjectPropertyValue(bytes + inputs_[0]->GetOffset_Internal());
    if (!player_outer(ability)) { ++skipped_; return; }
    auto* cls = ability->GetClassPrivate();
    if (!cls) { ++skipped_; return; }
    const auto key = name_key(cls->GetNamePrivate());
    int slot = -1;
    if (auto it = class_slots_.find(key); it != class_slots_.end()) slot = it->second;
    else {
        slot = classify(narrow(cls->GetNamePrivate().ToString()));
        if (class_slots_.size() < max_cached_classes) class_slots_.emplace(key, int8_t(slot));
    }
    if (slot < 0) { ++skipped_; return; }
    auto& s = slots_[size_t(slot)];
    bool changed = false;
    if (auto* replacement = s.montage.get()) {
        UObject* pointer = replacement;
        std::memcpy(bytes + inputs_[1]->GetOffset_Internal(), &pointer, sizeof(pointer));
        ++s.hits; changed = true;
    }
    if (std::abs(rate_ - 1.0) > 1e-6) {
        float rate{}; std::memcpy(&rate, bytes + inputs_[2]->GetOffset_Internal(), sizeof(rate));
        if (std::isfinite(rate) && rate > 0.f) { rate *= float(rate_); std::memcpy(bytes + inputs_[2]->GetOffset_Internal(), &rate, sizeof(rate)); changed = true; }
    }
    if (changed) ++swapped_; else ++skipped_;
}
nlohmann::json Combat::status() const {
    nlohmann::json slots = nlohmann::json::array();
    for (size_t i = 0; i < slots_.size(); ++i) {
        const auto& s = slots_[i];
        slots.push_back({{"slot", slot_to_string(SlotId(i))}, {"move", s.move_id}, {"ready", s.montage.alive()}, {"pending", s.pending}, {"error", s.error}, {"hits", s.hits}});
    }
    CcsHookStats stats{}; stats.size = sizeof(stats);
    nlohmann::json host = {{"available", false}};
    if (deps_.hooks->statistics(deps_.hooks->context, &stats))
        host = {{"available", true}, {"slots", stats.slots}, {"calls", stats.calls}, {"wrong_thread", stats.wrong_thread}, {"failures", stats.failures}};
    return {{"enabled", enabled_}, {"active", active_}, {"hooked", token_ != 0}, {"rate", rate_}, {"seen", seen_}, {"swapped", swapped_},
        {"skipped", skipped_}, {"failures", failures_}, {"wrong_frame", wrong_frame_}, {"maximum_callback_us", maximum_us_},
        {"cached_classes", class_slots_.size()}, {"error", error_}, {"slots", std::move(slots)}, {"host", std::move(host)}};
}
}

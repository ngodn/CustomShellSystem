#include "swap_probe.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <Unreal/FFrame.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
namespace {
using namespace engine;
uint64_t now_us() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
constexpr std::array<const wchar_t*, 7> names{L"OwningAbility", L"TaskInstanceName", L"MontageToPlay", L"Rate",
    L"StartSection", L"bStopWhenAbilityEnds", L"StartTime"};
constexpr size_t max_entries = 64;
constexpr size_t max_config_bytes = 64 * 1024;
void require_object(FProperty* p, const char* what) {
    if (!p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*)) throw std::runtime_error(what);
}
bool same_name(const FName& a, const FName& b) {
    return a.GetComparisonIndex() == b.GetComparisonIndex() && a.GetNumber() == b.GetNumber();
}
std::string text(const FName& name) { return narrow(name.ToString()); }
UObject* optional_object_of(UObject* object, const wchar_t* name) {
    try { return object_of(object, name); } catch (const std::exception&) { return nullptr; }
}
}
SwapProbe::SwapProbe(const CcsHookHost* host, const std::filesystem::path& root)
    : host_(host), root_(root), writer_(root / "logs/swap-calls.jsonl"), session_(now_us()) {
    if (!host_ || host_->version != CCS_HOOK_ABI_VERSION || host_->size < sizeof(CcsHookHost) ||
        !host_->add_native_pre || !host_->remove || !host_->statistics || !host_->on_game_thread)
        throw std::runtime_error("Swap probe needs the native hook host");
}
void SwapProbe::emit(nlohmann::json record) {
    record["session"] = session_; record["run"] = run_; record["time_us"] = now_us();
    if (writer_.drain_state() == runtime::Writer::DrainState::Failed || !writer_.write(record.dump())) {
        output_failed_ = true; throw std::runtime_error("Swap probe output rejected a record");
    }
}
nlohmann::json SwapProbe::host_stats() const {
    CcsHookStats stats{}; stats.size = sizeof(stats);
    if (!host_->statistics(host_->context, &stats)) return {{"available", false}};
    return {{"available", true}, {"slots", stats.slots}, {"running", stats.running}, {"stopped", stats.stopped != 0},
        {"calls", stats.calls}, {"wrong_thread", stats.wrong_thread}, {"failures", stats.failures}};
}
void SwapProbe::toggle() {
    if (output_failed_) { state_ = "failed"; error_ = "Swap probe output is unavailable"; return; }
    if (terminal_pending_) return;
    if (wanted_ || token_) { wanted_ = false; active_ = false; reason_ = "disarmed_by_user"; return; }
    wanted_ = true; requested_ = now_us(); retry_ = 0; ++run_; reported_ = false;
    head_ = count_ = 0; seen_ = swapped_ = skipped_ = failures_ = maximum_us_ = 0; remove_after_ = 0;
    error_.clear(); reason_.clear(); state_ = "arming";
    emit({{"event", "start"}, {"probe", "swap_test_04"}, {"read_only", false}});
}
void SwapProbe::load_config() {
    const auto path = root_ / "swap-test.json";
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) throw std::runtime_error("swap-test.json is missing");
    if (size == 0 || size > max_config_bytes) throw std::runtime_error("swap-test.json size is out of bounds");
    std::ifstream stream(path, std::ios::binary);
    std::string bytes(static_cast<size_t>(size), '\0');
    if (!stream.read(bytes.data(), static_cast<std::streamsize>(size))) throw std::runtime_error("swap-test.json could not be read");
    const auto config = nlohmann::json::parse(bytes, nullptr, false);
    if (!config.is_object() || !config.contains("map") || !config["map"].is_object()) throw std::runtime_error("swap-test.json needs a map object");
    rate_scale_ = 1.0f;
    if (config.contains("rate")) {
        if (!config["rate"].is_number()) throw std::runtime_error("swap-test.json rate must be a number");
        rate_scale_ = config["rate"].get<float>();
        if (!std::isfinite(rate_scale_) || rate_scale_ < 0.25f || rate_scale_ > 4.0f) throw std::runtime_error("swap-test.json rate must be within 0.25 and 4");
    }
    entries_.clear();
    for (const auto& [key, value] : config["map"].items()) {
        if (entries_.size() >= max_entries) throw std::runtime_error("swap-test.json has more than 64 entries");
        if (key.empty() || key.size() > 256 || !value.is_string() || value.get_ref<const std::string&>().empty() ||
            value.get_ref<const std::string&>().size() > 1024)
            throw std::runtime_error("swap-test.json entry is malformed: " + key);
        Entry entry; entry.class_text = key; entry.montage_path = value.get<std::string>();
        entry.class_name = FName(wide(key).c_str(), FNAME_Add);
        entries_.push_back(std::move(entry));
    }
    if (entries_.empty()) throw std::runtime_error("swap-test.json map is empty");
}
void SwapProbe::release_roots() {
    for (auto& entry : entries_) {
        if (entry.rooted) { if (auto* montage = entry.montage.get()) montage->ClearRootSet(); entry.rooted = false; }
        entry.montage = {};
    }
}
void SwapProbe::arm(void* engine_ptr) {
    const auto p = player_context(engine_ptr);
    auto* item = object_of(p.pc, L"ActiveWeaponItemDefinition");
    if (!p.world || !p.pc || !p.pawn || !p.asc || !item) { retry_ = now_us() + 250000; return; }
    load_config();
    auto* task_cdo = find(L"/Script/CSAbilityTasks.Default__AbilityTask_PlayMontageAndWaitWithNotifies");
    Call signature(task_cdo, L"PlayMontageAndWaitWithNotifies", 8);
    auto* function = signature.function();
    if (!function->HasAnyFunctionFlags(FUNC_Native) || function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate))
        throw std::runtime_error("Montage task is not a supported native function");
    for (size_t i = 0; i < inputs_.size(); ++i) {
        auto* field = signature.param(names[i]);
        if (field->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)) throw std::runtime_error("Montage task input direction changed");
        if (field->GetOffset_Internal() < 0 || field->GetOffset_Internal() + field->GetElementSize() > function->GetParmsSize())
            throw std::runtime_error("Montage task input exceeds its frame");
        inputs_[i] = field;
    }
    require_object(inputs_[0], "Montage task ability input layout changed"); require_object(inputs_[2], "Montage task montage input layout changed");
    if (!inputs_[3]->IsA<FNumericProperty>() || inputs_[3]->GetElementSize() != sizeof(float) ||
        !static_cast<FNumericProperty*>(inputs_[3])->IsFloatingPoint()) throw std::runtime_error("Montage task rate layout changed");
    auto* montage_class = static_cast<FObjectProperty*>(inputs_[2])->GetPropertyClass().Get();
    if (!montage_class || narrow(montage_class->GetPathName()) != "/Script/Engine.AnimMontage") throw std::runtime_error("Montage task montage class changed");
    // The replacement must target the skeleton the player's mesh is driven by.
    auto* mesh = object_of(p.pawn, L"Mesh");
    auto* mesh_asset = optional_object_of(mesh, L"SkeletalMeshAsset");
    if (!mesh_asset) mesh_asset = object_of(mesh, L"SkeletalMesh");
    auto* skeleton = object_of(mesh_asset, L"Skeleton");
    if (!skeleton) throw std::runtime_error("Player skeleton is unavailable");
    skeleton_.capture(skeleton);
    auto resolved = nlohmann::json::array();
    try {
        for (auto& entry : entries_) {
            auto* montage = load(entry.montage_path);
            if (!montage->IsA(static_cast<UClass*>(montage_class))) throw std::runtime_error("Not an AnimMontage: " + entry.montage_path);
            if (object_of(montage, L"Skeleton") != skeleton) throw std::runtime_error("Montage skeleton differs from the player mesh: " + entry.montage_path);
            if (!montage->IsRootSet()) { montage->SetRootSet(); entry.rooted = true; }
            entry.montage.capture(montage);
            resolved.push_back({{"class", entry.class_text}, {"montage", narrow(montage->GetPathName())}, {"rooted_by_ccs", entry.rooted}});
        }
    } catch (...) { release_roots(); throw; }
    WeakObject serial(function);
    if (serial.Get() != function) throw std::runtime_error("Montage task weak identity is unavailable");
    function_.capture(function); engine_.capture(static_cast<UObject*>(engine_ptr)); world_.capture(p.world);
    pc_.capture(p.pc); pawn_.capture(p.pawn); asc_.capture(p.asc); item_.capture(item);
    shell_.capture(object_of(p.pc, L"ActiveShellItemDefinition"));
    token_ = host_->add_native_pre(host_->context, function, callback, this);
    if (!token_) { release_roots(); throw std::runtime_error("Native montage pre-hook registration rejected"); }
    active_ = true; state_ = "active";
    emit({{"event", "armed"}, {"function", narrow(function->GetPathName())}, {"rate_scale", rate_scale_},
        {"skeleton", narrow(skeleton->GetPathName())}, {"entries", std::move(resolved)}, {"host_stats", host_stats()},
        {"weapon_item", narrow(item->GetPathName())}});
}
bool SwapProbe::player_current() const {
    auto* pc = pc_.get(); auto* pawn = pawn_.get(); auto* asc = asc_.get(); auto* engine = engine_.get();
    return pc && pawn && asc && engine && world_.alive() && item_.alive() && (!shell_.ptr || shell_.alive()) &&
        cached_object_of(cached_object_of(engine, L"GameViewport"), L"World") == world_.get() &&
        cached_object_of(pc, L"Pawn") == pawn && cached_object_of(pawn, L"AbilitySystemComponent") == asc &&
        cached_object_of(pc, L"ActiveWeaponItemDefinition") == item_.get() &&
        cached_object_of(pc, L"ActiveShellItemDefinition") == shell_.get();
}
bool SwapProbe::player_outer(UObject* object) const {
    if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) return false;
    const auto* pawn = pawn_.get(); const auto* asc = asc_.get();
    for (unsigned depth = 0; object && depth < 8; ++depth, object = object->GetOuterPrivate())
        if (object == pawn || object == asc) return true;
    return false;
}
void SwapProbe::callback(void* user, void*, void* frame, void*) noexcept {
    auto* self = static_cast<SwapProbe*>(user);
    const auto started = now_us();
    try { self->observe(frame); }
    catch (...) { ++self->failures_; self->active_ = false; self->wanted_ = false; }
    self->maximum_us_ = std::max(self->maximum_us_, now_us() - started);
    if (self->maximum_us_ > 2000) { ++self->failures_; self->active_ = false; self->wanted_ = false; }
}
void SwapProbe::observe(void* frame_ptr) {
    ++seen_;
    if (!active_ || !token_) { ++skipped_; return; }
    if (!function_.alive() || !player_current()) { ++failures_; active_ = false; wanted_ = false; return; }
    auto* frame = static_cast<FFrame*>(frame_ptr);
    auto* locals = frame ? frame->Locals() : nullptr;
    if (!locals || frame->Node() != function_.get()) throw std::runtime_error("Montage task frame identity/storage is unavailable");
    const auto data = [&](size_t index) { return static_cast<std::byte*>(static_cast<void*>(locals)) + inputs_[index]->GetOffset_Internal(); };
    auto* ability = static_cast<FObjectProperty*>(inputs_[0])->GetObjectPropertyValue(data(0));
    if (!player_outer(ability)) { ++skipped_; return; }
    auto* ability_class = ability->GetClassPrivate();
    if (!ability_class) { ++skipped_; return; }
    const FName class_name = ability_class->GetNamePrivate();
    Entry* match = nullptr;
    for (auto& entry : entries_) if (same_name(entry.class_name, class_name)) { match = &entry; break; }
    if (!match) { ++skipped_; return; }
    auto* replacement = match->montage.get();
    if (!replacement) throw std::runtime_error("Replacement montage expired: " + match->montage_path);
    auto* original = static_cast<FObjectProperty*>(inputs_[2])->GetObjectPropertyValue(data(2));
    Row row; row.class_name = class_name; row.original = original ? original->GetNamePrivate() : FName();
    row.replacement = replacement->GetNamePrivate();
    std::memcpy(&row.rate_in, data(3), sizeof(float));
    row.rate_out = rate_scale_ == 1.0f ? row.rate_in : row.rate_in * rate_scale_;
    if (!std::isfinite(row.rate_out) || row.rate_out <= 0.0f) row.rate_out = row.rate_in;
    // The thunk's rebuilt frame has no bytecode, so the native reads these locals after this callback returns.
    UObject* pointer = replacement;
    std::memcpy(data(2), &pointer, sizeof(pointer));
    std::memcpy(data(3), &row.rate_out, sizeof(float));
    row.time_us = now_us();
    ++match->hits; ++swapped_;
    if (count_ < rows_.size()) { rows_[(head_ + count_) % rows_.size()] = row; ++count_; }
}
void SwapProbe::drain() {
    for (unsigned i = 0; count_ && i < 4; ++i) {
        const auto& row = rows_[head_];
        emit({{"event", "swap"}, {"observed_us", row.time_us}, {"ability_class", text(row.class_name)},
            {"original", text(row.original)}, {"replacement", text(row.replacement)},
            {"rate_in", row.rate_in}, {"rate_out", row.rate_out}});
        head_ = (head_ + 1) % rows_.size(); --count_;
    }
}
bool SwapProbe::stop() {
    wanted_ = false; active_ = false;
    if (token_) {
        if (!host_->remove(host_->context, token_)) { state_ = "removal_pending"; return false; }
        token_ = 0;
    }
    release_roots();
    inputs_.fill(nullptr); function_ = {}; skeleton_ = {};
    return true;
}
void SwapProbe::tick(void* engine_ptr) {
    if (!wanted_ && !token_ && reported_ && !terminal_pending_) return;
    try {
        const auto output_state = writer_.drain_state();
        if (output_state == runtime::Writer::DrainState::Failed) { output_failed_ = true; error_ = "Swap probe output failed"; }
        if (output_failed_) {
            wanted_ = false; active_ = false;
            if (token_ && now_us() >= remove_after_) { stop(); remove_after_ = now_us() + 250000; }
            state_ = token_ ? "removal_pending" : "failed"; reported_ = true; terminal_pending_ = false; count_ = 0;
            return;
        }
        if (terminal_pending_) {
            if (output_state == runtime::Writer::DrainState::Complete) {
                terminal_pending_ = false; state_ = failures_ || !error_.empty() ? "failed" : "idle";
            }
            return;
        }
        if (wanted_ && !token_ && now_us() >= retry_) {
            if (now_us() - requested_ >= 5000000) throw std::runtime_error("Swap probe player wait timed out");
            arm(engine_ptr);
        }
        if (token_ && wanted_ && !player_current()) { reason_ = "player_generation_changed"; wanted_ = false; active_ = false; }
        if (!wanted_ && token_ && now_us() >= remove_after_) { stop(); remove_after_ = now_us() + 250000; }
        drain();
        if (!wanted_ && !token_ && !count_ && !reported_) {
            if (reason_.empty()) reason_ = failures_ ? "failed" : "disarmed";
            state_ = failures_ || !error_.empty() ? "failed" : "idle";
            auto hits = nlohmann::json::array();
            for (const auto& entry : entries_) hits.push_back({{"class", entry.class_text}, {"hits", entry.hits}});
            emit({{"event", "end"}, {"state", state_}, {"reason", reason_}, {"seen", seen_}, {"swapped", swapped_},
                {"skipped", skipped_}, {"failures", failures_}, {"maximum_callback_us", maximum_us_},
                {"entries", std::move(hits)}, {"host_stats", host_stats()}, {"hook_removed", true}, {"roots_released", true}});
            reported_ = true; terminal_pending_ = true; state_ = "writing";
        }
    } catch (const std::exception& e) {
        error_ = e.what(); ++failures_; wanted_ = false; active_ = false; reason_ = "failed";
        stop(); state_ = token_ ? "removal_pending" : "failed";
        try { emit({{"event", "error"}, {"message", error_}, {"host_stats", host_stats()}}); } catch (...) { reported_ = true; }
    }
}
nlohmann::json SwapProbe::status() const {
    return {{"state", state_}, {"error", error_}, {"hook", token_ != 0}, {"active", active_}, {"seen", seen_},
        {"swapped", swapped_}, {"skipped", skipped_}, {"failures", failures_}, {"maximum_callback_us", maximum_us_},
        {"entries", entries_.size()}, {"host_stats", host_stats()}};
}
}

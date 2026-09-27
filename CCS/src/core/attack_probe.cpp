#include "attack_probe.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
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
void require_object(FProperty* p) {
    if (!p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Attack task object input layout changed");
}
FProperty* checked_field(UStruct* owner, const wchar_t* name) {
    auto* field = owner ? owner->GetPropertyByNameInChain(name) : nullptr;
    if (!field) throw std::runtime_error("Attack event field is unavailable");
    const auto offset = field->GetOffset_Internal(), size = field->GetElementSize();
    if (owner->GetPropertiesSize() <= 0 || owner->GetPropertiesSize() > 65536 ||
        field->GetArrayDim() != 1 || offset < 0 || size <= 0 ||
        offset > owner->GetPropertiesSize() || size > owner->GetPropertiesSize() - offset)
        throw std::runtime_error("Attack event field exceeds its owner");
    return field;
}
UScriptStruct* checked_struct(FProperty* field, const char* path) {
    if (!field->IsA<FStructProperty>()) throw std::runtime_error("Attack event struct kind changed");
    auto* type = static_cast<FStructProperty*>(field)->GetStruct().Get();
    if (!type || type->GetPropertiesSize() <= 0 || type->GetPropertiesSize() > field->GetElementSize() ||
        narrow(type->GetPathName()) != path)
        throw std::runtime_error("Attack event struct layout changed");
    return type;
}
nlohmann::json describe(const ObjectHandle& value) {
    auto* object = value.get();
    return {{"live", object != nullptr}, {"index", value.index}, {"serial", value.serial},
        {"path", object ? narrow(object->GetPathName()) : ""}};
}
}
AttackProbe::AttackProbe(const CcsHookHost* host, const std::filesystem::path& output)
    : host_(host), writer_(output), session_(now_us()) {
    if (!host_ || host_->version != CCS_HOOK_ABI_VERSION || host_->size < sizeof(CcsHookHost) ||
        !host_->add_native_pre || !host_->remove || !host_->on_game_thread)
        throw std::runtime_error("Attack probe needs the native hook host");
}
void AttackProbe::emit(nlohmann::json record) {
    record["session"] = session_; record["run"] = run_; record["time_us"] = now_us();
    if (writer_.drain_state() == runtime::Writer::DrainState::Failed || !writer_.write(record.dump())) {
        output_failed_ = true; throw std::runtime_error("Attack probe output rejected a record");
    }
}
void AttackProbe::toggle() {
    if (output_failed_) { state_ = "failed"; error_ = "Attack probe output is unavailable"; return; }
    if (terminal_pending_) return;
    if (wanted_ || token_) { wanted_ = false; reason_ = "cancelled"; return; }
    wanted_ = true; requested_ = now_us(); retry_ = 0; ++run_; reported_ = false;
    head_ = count_ = 0; seen_ = recorded_ = skipped_ = failures_ = maximum_us_ = 0;
    remove_after_ = 0;
    identity_skipped_ = 0;
    unretained_calls_ = 0;
    error_.clear(); reason_.clear(); state_ = "waiting_for_player";
    emit({{"event", "start"}, {"probe", "attack_call_03"}, {"schema", 3}, {"read_only", true}});
}
AttackProbe::ObjectObservation AttackProbe::observe_object(UObject* object) {
    ObjectObservation result;
    if (!object) return result;
    result.index = object->GetInternalIndex();
    auto* item = FUObjectArray::IndexToObject(result.index);
    if (!item || item->GetUObject() != object || !item->IsValid(false) || item->GetSerialNumber() < 0)
        throw std::runtime_error("Attack observation object is not live");
    result.live_at_callback = true; result.serial = item->GetSerialNumber();
    if (result.retained.capture_existing(object)) result.serial = result.retained.serial;
    while (object && result.name_count < result.names.size()) {
        result.names[result.name_count++] = object->GetNamePrivate(); object = object->GetOuterPrivate();
    }
    result.ancestry_complete = object == nullptr;
    return result;
}
nlohmann::json AttackProbe::describe_observation(const ObjectObservation& value) {
    auto result = describe(value.retained);
    result["index"] = value.index; result["serial"] = value.serial;
    result["live_at_callback"] = value.live_at_callback;
    result["retained_identity"] = value.retained.ptr != nullptr;
    result["ancestry_complete"] = value.ancestry_complete;
    auto names = nlohmann::json::array();
    for (unsigned i = 0; i < value.name_count; ++i) {
        auto name = narrow(value.names[i].ToString());
        if (name.empty() || name.size() > 1024) throw std::runtime_error("Attack observation name exceeds bound");
        names.push_back(std::move(name));
    }
    result["observed_names"] = std::move(names);
    return result;
}
bool AttackProbe::player_current() const {
    auto* pc = pc_.get(); auto* pawn = pawn_.get(); auto* asc = asc_.get(); auto* engine = engine_.get();
    return pc && pawn && asc && engine && world_.alive() && item_.alive() && (!shell_.ptr || shell_.alive()) &&
        cached_object_of(cached_object_of(engine, L"GameViewport"), L"World") == world_.get() &&
        cached_object_of(pc, L"Pawn") == pawn && cached_object_of(pawn, L"AbilitySystemComponent") == asc &&
        cached_object_of(pc, L"ActiveWeaponItemDefinition") == item_.get() &&
        cached_object_of(pc, L"ActiveShellItemDefinition") == shell_.get();
}
bool AttackProbe::player_outer(UObject* object) const {
    if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) return false;
    const auto* pawn = pawn_.get(); const auto* asc = asc_.get();
    for (unsigned depth = 0; object && depth < 8; ++depth, object = object->GetOuterPrivate())
        if (object == pawn || object == asc) return true;
    return false;
}
void AttackProbe::begin(void* engine_ptr) {
    const auto p = player_context(engine_ptr);
    auto* item = object_of(p.pc, L"ActiveWeaponItemDefinition");
    if (!p.world || !p.pc || !p.pawn || !p.asc || !item) { retry_ = now_us() + 250000; return; }
    auto* owner = find(L"/Script/CSAbilityTasks.Default__AbilityTask_PlayMontageAndWaitWithNotifies");
    Call signature(owner, L"PlayMontageAndWaitWithNotifies", 8);
    auto* function = signature.function();
    if (!function->HasAnyFunctionFlags(FUNC_Native) || function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate))
        throw std::runtime_error("Attack task is not a supported native function");
    std::array<bool, 512> occupied{};
    if (function->GetParmsSize() <= 0 || function->GetParmsSize() > occupied.size())
        throw std::runtime_error("Attack task parameter frame exceeds bound");
    for (size_t i = 0; i < inputs_.size(); ++i) {
        auto* field = signature.param(names[i]);
        if (field->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)) throw std::runtime_error("Attack task input direction changed");
        for (int byte = 0; byte < field->GetElementSize(); ++byte) {
            const auto index = static_cast<size_t>(field->GetOffset_Internal() + byte);
            if (index >= occupied.size() || occupied[index]) throw std::runtime_error("Attack task inputs overlap");
            occupied[index] = true;
        }
        inputs_[i] = field;
    }
    require_object(inputs_[0]); require_object(inputs_[2]);
    auto* ability_class = static_cast<FObjectProperty*>(inputs_[0])->GetPropertyClass().Get();
    auto* montage_class = static_cast<FObjectProperty*>(inputs_[2])->GetPropertyClass().Get();
    if (!ability_class || !montage_class || narrow(ability_class->GetPathName()) != "/Script/GameplayAbilities.GameplayAbility" ||
        narrow(montage_class->GetPathName()) != "/Script/Engine.AnimMontage")
        throw std::runtime_error("Attack task declared input classes changed");
    auto* event = checked_field(ability_class, L"CurrentEventData");
    auto* event_type = checked_struct(event, "/Script/GameplayAbilities.GameplayEventData");
    auto* tag = checked_field(event_type, L"EventTag");
    auto* tag_type = checked_struct(tag, "/Script/GameplayTags.GameplayTag");
    auto* tag_name = checked_field(tag_type, L"TagName");
    if (!tag_name->IsA<FNameProperty>() || tag_name->GetElementSize() != sizeof(FName))
        throw std::runtime_error("Attack event tag name layout changed");
    active_property_ = checked_field(ability_class, L"bIsActive");
    if (!active_property_->IsA<FBoolProperty>() || active_property_->GetElementSize() != sizeof(bool))
        throw std::runtime_error("Attack ability active flag layout changed");
    event_offset_ = event->GetOffset_Internal() + tag->GetOffset_Internal() + tag_name->GetOffset_Internal();
    for (auto* type : std::array<UObject*, 3>{ability_class, event_type, tag_type}) {
        WeakObject identity(type);
        if (identity.Get() != type) throw std::runtime_error("Attack event owner weak identity is unavailable");
    }
    event_owners_[0].capture(ability_class); event_owners_[1].capture(event_type); event_owners_[2].capture(tag_type);
    for (const auto index : {1u, 4u})
        if (!inputs_[index]->IsA<FNameProperty>() || inputs_[index]->GetElementSize() != sizeof(FName))
            throw std::runtime_error("Attack task name input layout changed");
    for (const auto index : {3u, 6u})
        if (!inputs_[index]->IsA<FNumericProperty>() || inputs_[index]->GetElementSize() != sizeof(float) ||
            !static_cast<FNumericProperty*>(inputs_[index])->IsFloatingPoint()) throw std::runtime_error("Attack task float input layout changed");
    if (!inputs_[5]->IsA<FBoolProperty>() || inputs_[5]->GetElementSize() != sizeof(bool) ||
        !static_cast<FBoolProperty*>(inputs_[5])->IsNativeBool()) throw std::runtime_error("Attack task bool input layout changed");
    auto* result = signature.param(L"ReturnValue"); require_object(result);
    if (!result->HasAnyPropertyFlags(CPF_ReturnParm)) throw std::runtime_error("Attack task return direction changed");
    for (int byte = 0; byte < result->GetElementSize(); ++byte)
        if (occupied[static_cast<size_t>(result->GetOffset_Internal() + byte)]) throw std::runtime_error("Attack task return overlaps input storage");
    WeakObject serial(function);
    if (serial.Get() != function) throw std::runtime_error("Attack task weak identity is unavailable");
    function_.capture(function); engine_.capture(static_cast<UObject*>(engine_ptr)); world_.capture(p.world);
    pc_.capture(p.pc); pawn_.capture(p.pawn); asc_.capture(p.asc); item_.capture(item);
    shell_.capture(object_of(p.pc, L"ActiveShellItemDefinition"));
    auto layout = nlohmann::json::array();
    for (size_t i = 0; i < inputs_.size(); ++i)
        layout.push_back({{"name", narrow(names[i])}, {"offset", inputs_[i]->GetOffset_Internal()},
            {"size", inputs_[i]->GetElementSize()}});
    emit({{"event", "interface"}, {"native", true}, {"frame_size", function->GetParmsSize()}, {"parameters", 8},
        {"function", narrow(function->GetPathName())}, {"input_layout", std::move(layout)},
        {"ability_class", narrow(ability_class->GetPathName())}, {"montage_class", narrow(montage_class->GetPathName())},
        {"event_context_layout", {{"event_tag_offset", event_offset_},
            {"active_offset", active_property_->GetOffset_Internal()}, {"event_type", narrow(event_type->GetPathName())},
            {"tag_type", narrow(tag_type->GetPathName())}}},
        {"observation_name_limit", 8},
        {"pawn", describe(pawn_)}, {"weapon_item", describe(item_)}});
    token_ = host_->add_native_pre(host_->context, function, callback, this);
    if (!token_) throw std::runtime_error("Native attack pre-hook registration rejected");
    started_ = now_us(); state_ = "observing";
}
void AttackProbe::callback(void* user, void*, void* frame, void*) noexcept {
    auto* self = static_cast<AttackProbe*>(user);
    const auto started = now_us();
    try { self->observe(frame); }
    catch (...) { ++self->failures_; self->wanted_ = false; }
    self->maximum_us_ = std::max(self->maximum_us_, now_us() - started);
    if (self->maximum_us_ > 2000) { ++self->failures_; self->wanted_ = false; }
}
void AttackProbe::observe(void* frame_ptr) {
    ++seen_;
    if (!wanted_ || !token_ || recorded_ >= rows_.size()) { ++skipped_; return; }
    if (!function_.alive() || !player_current()) { ++failures_; wanted_ = false; return; }
    auto* frame = static_cast<FFrame*>(frame_ptr);
    auto* locals = frame ? frame->Locals() : nullptr;
    if (!locals || frame->Node() != function_.get()) throw std::runtime_error("Attack task frame identity/storage is unavailable");
    const auto data = [&](size_t index) { return static_cast<std::byte*>(static_cast<void*>(locals)) + inputs_[index]->GetOffset_Internal(); };
    auto* ability = static_cast<FObjectProperty*>(inputs_[0])->GetObjectPropertyValue(data(0));
    if (!player_outer(ability)) { ++skipped_; return; }
    for (const auto& owner : event_owners_)
        if (!owner.alive()) throw std::runtime_error("Attack event reflected owner expired");
    if (!ability->IsA(static_cast<UClass*>(event_owners_[0].get())))
        throw std::runtime_error("Attack task ability class does not match event storage");
    if (count_ >= rows_.size()) { ++failures_; wanted_ = false; return; }
    Row row;
    row.ability = observe_object(ability); row.ability_class = observe_object(ability->GetClassPrivate());
    const auto* ability_bytes = reinterpret_cast<const std::byte*>(ability);
    std::memcpy(&row.event_tag, ability_bytes + event_offset_, sizeof(FName));
    row.ability_active = static_cast<FBoolProperty*>(active_property_)->GetPropertyValue(
        ability_bytes + active_property_->GetOffset_Internal());
    row.montage = observe_object(static_cast<FObjectProperty*>(inputs_[2])->GetObjectPropertyValue(data(2)));
    std::memcpy(&row.task, data(1), sizeof(FName)); std::memcpy(&row.section, data(4), sizeof(FName));
    std::memcpy(&row.rate, data(3), sizeof(float)); std::memcpy(&row.start_time, data(6), sizeof(float));
    if (!std::isfinite(row.rate) || !std::isfinite(row.start_time)) throw std::runtime_error("Attack task float input is not finite");
    row.stop_with_ability = static_cast<FBoolProperty*>(inputs_[5])->GetPropertyValue(data(5)); row.time_us = now_us();
    if (!row.ability.retained.ptr || !row.montage.retained.ptr) ++unretained_calls_;
    rows_[(head_ + count_) % rows_.size()] = row; ++count_; ++recorded_;
    if (recorded_ == rows_.size()) wanted_ = false;
}
void AttackProbe::drain() {
    for (unsigned i = 0; count_ && i < 4; ++i) {
        const auto& row = rows_[head_];
        emit({{"event", "call"}, {"number", recorded_ - count_}, {"observed_us", row.time_us},
            {"ability", describe_observation(row.ability)}, {"ability_class", describe_observation(row.ability_class)},
            {"montage", describe_observation(row.montage)},
            {"task", narrow(row.task.ToString())}, {"section", narrow(row.section.ToString())},
            {"rate", row.rate}, {"start_time", row.start_time}, {"stop_with_ability", row.stop_with_ability},
            {"current_event_tag", narrow(row.event_tag.ToString())}, {"ability_active", row.ability_active},
            {"player_outer_match", true}, {"combat_verified", false}});
        head_ = (head_ + 1) % rows_.size(); --count_;
    }
}
bool AttackProbe::stop() {
    wanted_ = false;
    if (!token_) return true;
    if (!host_->remove(host_->context, token_)) { state_ = "removal_pending"; return false; }
    token_ = 0; inputs_.fill(nullptr); function_ = {};
    event_owners_.fill({}); active_property_ = nullptr; event_offset_ = 0; return true;
}
void AttackProbe::tick(void* engine_ptr) {
    if (!wanted_ && !token_ && reported_ && !terminal_pending_) return;
    try {
        const auto output_state = writer_.drain_state();
        if (output_state == runtime::Writer::DrainState::Failed) {
            output_failed_ = true; error_ = "Attack probe output failed";
        }
        if (output_failed_) {
            wanted_ = false;
            if (token_ && now_us() >= remove_after_) { stop(); remove_after_ = now_us() + 250000; }
            state_ = token_ ? "removal_pending" : "failed"; reported_ = true; terminal_pending_ = false; count_ = 0;
            return;
        }
        if (terminal_pending_) {
            if (output_state == runtime::Writer::DrainState::Complete) {
                terminal_pending_ = false;
                state_ = failures_ || !error_.empty() ? "failed" : "complete";
            }
            return;
        }
        if (wanted_ && !token_ && now_us() >= retry_) {
            if (now_us() - requested_ >= 5000000) throw std::runtime_error("Attack probe player wait timed out");
            begin(engine_ptr);
        }
        if (token_ && wanted_) {
            const auto current = player_current();
            if (!current || now_us() - started_ >= 30000000) {
                reason_ = current ? "duration" : "player_generation_changed"; wanted_ = false;
            }
        }
        if (!wanted_ && token_ && now_us() >= remove_after_) { stop(); remove_after_ = now_us() + 250000; }
        drain();
        if (!wanted_ && !token_ && !count_ && !reported_) {
            if (reason_.empty()) reason_ = failures_ ? "failed" : recorded_ == rows_.size() ? "call_capacity" : "cancelled";
            state_ = failures_ || !error_.empty() ? "failed" : "complete";
            emit({{"event", "end"}, {"state", state_}, {"reason", reason_}, {"seen", seen_}, {"recorded", recorded_},
                {"skipped", skipped_}, {"failures", failures_}, {"maximum_callback_us", maximum_us_},
                {"identity_skipped", identity_skipped_},
                {"unretained_calls", unretained_calls_},
                {"hook_removed", true}, {"callsite_observed", recorded_ > 0}});
            reported_ = true; terminal_pending_ = true; state_ = "writing";
        }
    } catch (const std::exception& e) {
        error_ = e.what(); ++failures_; wanted_ = false; reason_ = "failed";
        stop(); state_ = token_ ? "removal_pending" : "failed";
        try { emit({{"event", "error"}, {"message", error_}}); } catch (...) { reported_ = true; }
    }
}
nlohmann::json AttackProbe::status() const {
    return {{"state", state_}, {"error", error_}, {"hook", token_ != 0}, {"recorded", recorded_},
        {"seen", seen_}, {"skipped", skipped_}, {"identity_skipped", identity_skipped_},
        {"unretained_calls", unretained_calls_},
        {"failures", failures_}, {"maximum_callback_us", maximum_us_}};
}
}

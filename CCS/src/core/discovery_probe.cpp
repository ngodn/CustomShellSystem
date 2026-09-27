#include "discovery_probe.hpp"
#include <chrono>
#include <cmath>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
namespace {
using namespace engine;
using Json = nlohmann::json;
uint64_t now_us() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
FProperty* property(UStruct* owner, const wchar_t* name) {
    if (!owner) throw std::runtime_error("No property owner");
    auto* p = owner->GetPropertyByNameInChain(name);
    if (!p) return nullptr;
    const auto offset = p->GetOffset_Internal();
    const auto size = p->GetElementSize();
    if (p->GetArrayDim() != 1 || offset < 0 || size <= 0 ||
        offset > owner->GetPropertiesSize() || size > owner->GetPropertiesSize() - offset)
        throw std::runtime_error("Property exceeds owner: " + narrow(name));
    return p;
}
void* address(void* container, FProperty* p) {
    return static_cast<std::byte*>(container) + p->GetOffset_Internal();
}
UObject* read_object(FProperty* p, void* data) {
    if (!p || !p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Object reference layout mismatch");
    return static_cast<FObjectProperty*>(p)->GetObjectPropertyValue(data);
}
Json describe(UObject* object) {
    if (!object) return nullptr;
    ObjectHandle handle;
    handle.capture(object);
    return {{"path", narrow(object->GetPathName())}, {"class", narrow(object->GetClassPrivate()->GetPathName())},
            {"object_index", handle.index}, {"serial", handle.serial}};
}
FArrayProperty* array_property(FProperty* p) {
    if (!p || !p->IsA<FArrayProperty>() || p->GetElementSize() != sizeof(FScriptArray))
        throw std::runtime_error("Array layout mismatch");
    auto* array = static_cast<FArrayProperty*>(p);
    if (!array->GetInner() || array->GetInner()->GetElementSize() <= 0)
        throw std::runtime_error("Array inner layout mismatch");
    if (!!(array->GetArrayFlags() & EArrayPropertyFlags::UsesMemoryImageAllocator))
        throw std::runtime_error("Memory-image array is unsupported by this probe");
    return array;
}
FScriptArrayHelper checked_array(FArrayProperty* property, void* data) {
    if (!data) throw std::runtime_error("Array storage is unavailable");
    auto* array = static_cast<FScriptArray*>(data);
    const auto count = array->NumUnchecked();
    if (count < 0 || array->Max() < count || array->Max() > 65536 || (count && !array->GetData()))
        throw std::runtime_error("Array storage layout mismatch");
    return FScriptArrayHelper(property, data);
}
int bounded_count(FScriptArrayHelper& values, int bound) {
    const auto count = values.NumUnchecked();
    if (count < 0 || count > bound) throw std::runtime_error("Array count exceeds probe bound");
    if (count > 0 && !values.GetRawPtr(0)) throw std::runtime_error("Array data is unavailable");
    return count;
}
#ifdef CCS_REGISTRY_CONTROLS
std::string read_name(UStruct* owner, void* data, const wchar_t* name) {
    auto* p = property(owner, name);
    if (!p || !p->IsA<FNameProperty>() || p->GetElementSize() != sizeof(FName))
        throw std::runtime_error("Name layout mismatch: " + narrow(name));
    FName value;
    std::memcpy(&value, address(data, p), sizeof(value));
    return narrow(value.ToString());
}
Json ability_tags(UObject* ability, const wchar_t* name = L"AbilityTags") {
    auto* p = property(ability->GetClassPrivate(), name);
    if (!p) return {{"available", false}};
    if (!p->IsA<FStructProperty>()) throw std::runtime_error("Ability tags are not a struct");
    auto* type = static_cast<FStructProperty*>(p)->GetStruct().Get();
    if (!type || type->GetPropertiesSize() > p->GetElementSize() ||
        narrow(type->GetPathName()) != "/Script/GameplayTags.GameplayTagContainer")
        throw std::runtime_error("Ability tag container mismatch");
    auto* tags = array_property(property(type, L"GameplayTags"));
    auto* inner = tags->GetInner();
    if (!inner->IsA<FStructProperty>()) throw std::runtime_error("Gameplay tag is not a struct");
    auto* tag = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if (!tag || tag->GetPropertiesSize() > inner->GetElementSize() ||
        narrow(tag->GetPathName()) != "/Script/GameplayTags.GameplayTag")
        throw std::runtime_error("Gameplay tag layout mismatch");
    auto values = checked_array(tags, address(address(ability, p), tags));
    Json names = Json::array();
    const auto count = bounded_count(values, 64);
    for (int i = 0; i < count; ++i) names.push_back(read_name(tag, values.GetRawPtr(i), L"TagName"));
    return {{"available", true}, {"values", std::move(names)}};
}
void require_bool(Call& call, const wchar_t* name) {
    auto* p = call.param(name);
    if (!p->IsA<FBoolProperty>() || p->GetElementSize() != sizeof(bool) ||
        !static_cast<FBoolProperty*>(p)->IsNativeBool())
        throw std::runtime_error("Registry bool layout mismatch: " + narrow(name));
}
Json registry_query(UObject* registry, const std::string& path, bool by_path) {
    Call call(registry, by_path ? L"GetAssetsByPath" : L"GetAssetsByPackageName", 5);
    const auto* input_name = by_path ? L"PackagePath" : L"PackageName";
    auto* input = call.param(input_name);
    if (!input->IsA<FNameProperty>() || input->GetElementSize() != sizeof(FName))
        throw std::runtime_error("Registry package input is not a name");
    require_bool(call, L"bIncludeOnlyOnDiskAssets");
    require_bool(call, L"ReturnValue");
    require_bool(call, by_path ? L"bRecursive" : L"bSkipARFilteredAssets");
    auto* output = array_property(call.param(L"OutAssetData"));
    if (!output->HasAnyPropertyFlags(CPF_OutParm) || !output->GetInner()->IsA<FStructProperty>())
        throw std::runtime_error("Registry output is not a struct array out parameter");
    auto* asset = static_cast<FStructProperty*>(output->GetInner())->GetStruct().Get();
    if (!asset || asset->GetPropertiesSize() > output->GetInner()->GetElementSize() ||
        narrow(asset->GetPathName()) != "/Script/CoreUObject.AssetData")
        throw std::runtime_error("Registry asset-data layout mismatch");
    // Validate required reflected fields before the query, including its nontrivial native output struct.
    for (const auto* name : {L"PackageName", L"PackagePath", L"AssetName"}) {
        auto* p = property(asset, name);
        if (!p || !p->IsA<FNameProperty>() || p->GetElementSize() != sizeof(FName))
            throw std::runtime_error("Registry asset-data name mismatch");
    }
    auto* class_path = property(asset, L"AssetClassPath");
    if (!class_path || !class_path->IsA<FStructProperty>()) throw std::runtime_error("Asset class path unavailable");
    auto* class_type = static_cast<FStructProperty*>(class_path)->GetStruct().Get();
    if (!class_type || class_type->GetPropertiesSize() > class_path->GetElementSize() ||
        narrow(class_type->GetPathName()) != "/Script/CoreUObject.TopLevelAssetPath")
        throw std::runtime_error("Asset class path layout mismatch");
    for (const auto* name : {L"PackageName", L"AssetName"}) {
        auto* p = property(class_type, name);
        if (!p || !p->IsA<FNameProperty>() || p->GetElementSize() != sizeof(FName))
            throw std::runtime_error("Asset class path name mismatch");
    }
    call.set(input_name, FName(wide(path).c_str()));
    call.set(L"bIncludeOnlyOnDiskAssets", true);
    call.set(by_path ? L"bRecursive" : L"bSkipARFilteredAssets", false);
    call.run();
    auto values = checked_array(output, call.data(output));
    const auto count = bounded_count(values, 128);
    Json rows = Json::array();
    for (int i = 0; i < count; ++i) {
        void* data = values.GetRawPtr(i);
        const auto package = read_name(asset, data, L"PackageName");
        const auto name = read_name(asset, data, L"AssetName");
        if (!package.starts_with("/Game/") || package.size() > 4096 || name.empty() || name.size() > 1024)
            throw std::runtime_error("Registry asset identity invalid");
        void* cls = address(data, class_path);
        rows.push_back({{"path", package + "." + name}, {"package", package},
            {"package_path", read_name(asset, data, L"PackagePath")},
            {"class", read_name(class_type, cls, L"PackageName") + "." + read_name(class_type, cls, L"AssetName")}});
    }
    return {{"query", path}, {"method", by_path ? "GetAssetsByPath" : "GetAssetsByPackageName"},
        {"recursive", false}, {"only_on_disk", true}, {"returned", call.get<bool>()},
        {"asset_struct_size", output->GetInner()->GetElementSize()}, {"assets", std::move(rows)}};
}
#endif

Json object_array(UObject* object, const wchar_t* name) {
    auto* p = property(object->GetClassPrivate(), name);
    if (!p) return {{"available", false}};
    auto* array = array_property(p);
    auto* inner = array->GetInner();
    if (!inner->IsA<FObjectProperty>() || inner->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Selector array is not an object/class array");
    auto values = checked_array(array, address(object, p));
    Json rows = Json::array();
    const int count = bounded_count(values, 16);
    for (int i = 0; i < count; ++i) {
        auto* value = read_object(inner, values.GetRawPtr(i));
        auto record = describe(value);
        if (value && value->IsA<UClass>()) {
            // Read the already-existing CDO. Never construct or load one for this probe.
            auto* cdo = static_cast<UClass*>(value)->GetClassDefaultObject().Get();
            if (cdo) {
                ObjectHandle live;
                live.capture(cdo);
#ifdef CCS_REGISTRY_CONTROLS
                record["ability_tags"] = ability_tags(cdo);
                record["activation_owned_tags"] = ability_tags(cdo, L"ActivationOwnedTags");
                if (auto* hold = property(cdo->GetClassPrivate(), L"IsHoldAttack")) {
                    if (!hold->IsA<FBoolProperty>()) throw std::runtime_error("Selector move hold flag is not a bool");
                    record["is_hold_attack"] = static_cast<FBoolProperty*>(hold)->GetPropertyValueInContainer(cdo);
                }
#endif
                if (auto* montage = property(cdo->GetClassPrivate(), L"Montage"))
                    record["montage"] = describe(read_object(montage, address(cdo, montage)));
            }
        }
        rows.push_back({{"index", i}, {"object", std::move(record)}});
    }
    return {{"available", true}, {"values", std::move(rows)}};
}
Json ability_record(UObject* ability) {
    auto record = describe(ability);
    if (!ability) return record;
    auto* owner = ability->GetClassPrivate();
#ifdef CCS_REGISTRY_CONTROLS
    record["ability_tags"] = ability_tags(ability);
    record["activation_owned_tags"] = ability_tags(ability, L"ActivationOwnedTags");
    if (auto* hold = property(owner, L"IsHoldAttack")) {
        if (!hold->IsA<FBoolProperty>()) throw std::runtime_error("Hold attack flag is not a bool");
        record["is_hold_attack"] = static_cast<FBoolProperty*>(hold)->GetPropertyValueInContainer(ability);
    }
#endif
    record["selector"] = Json::object();
    for (const auto* name : {L"ComboAttackList", L"RunningAttackList", L"AdditionalAttacks"})
        record["selector"][narrow(name)] = object_array(ability, name);
    if (auto* p = property(owner, L"Montage")) {
        auto* montage = read_object(p, address(ability, p));
        record["montage"] = describe(montage);
        if (montage) {
            if (auto* skeleton = property(montage->GetClassPrivate(), L"Skeleton"))
                record["skeleton"] = describe(read_object(skeleton, address(montage, skeleton)));
        }
    }
    for (const auto* name : {L"MontagePlayRate", L"InstancingPolicy"}) {
        auto* p = property(owner, name);
        if (!p) continue;
        auto* number = p;
        if (p->IsA<FEnumProperty>()) number = static_cast<FEnumProperty*>(p)->GetUnderlyingProperty();
        if (!number || !number->IsA<FNumericProperty>() || number->GetElementSize() != p->GetElementSize())
            throw std::runtime_error("Numeric ability property layout mismatch");
        auto* numeric = static_cast<FNumericProperty*>(number);
        if (numeric->IsFloatingPoint()) {
            const auto value = numeric->GetFloatingPointPropertyValue(address(ability, p));
            if (!std::isfinite(value)) throw std::runtime_error("Non-finite ability property");
            record[narrow(name)] = value;
        } else record[narrow(name)] = numeric->GetSignedIntPropertyValue(address(ability, p));
    }
    return record;
}
struct Grants {
    FArrayProperty* array;
    void* data;
    UScriptStruct* spec;
    FProperty* ability;
};
Grants granted(UObject* asc) {
    auto* container = property(asc->GetClassPrivate(), L"ActivatableAbilities");
    if (!container || !container->IsA<FStructProperty>()) throw std::runtime_error("Granted ability container unavailable");
    auto* type = static_cast<FStructProperty*>(container)->GetStruct().Get();
    if (!type || type->GetPropertiesSize() > container->GetElementSize()) throw std::runtime_error("Granted container size mismatch");
    auto* items = array_property(property(type, L"Items"));
    auto* inner = items->GetInner();
    if (!inner->IsA<FStructProperty>()) throw std::runtime_error("Granted ability spec is not a struct");
    auto* spec = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if (!spec || spec->GetPropertiesSize() > inner->GetElementSize()) throw std::runtime_error("Granted spec size mismatch");
    auto* ability = property(spec, L"Ability");
    if (!ability || !ability->IsA<FObjectProperty>() || ability->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Granted ability pointer layout mismatch");
    return {items, address(address(asc, container), items), spec, ability};
}
Json function_signature(const wchar_t* owner_path, const wchar_t* name) {
    auto* owner = find_optional(owner_path);
    if (!owner) return {{"available", false}, {"owner", narrow(owner_path)}};
    auto* function = owner->GetFunctionByNameInChain(name);
    if (!function) return {{"available", false}, {"owner", narrow(owner_path)}};
    Json parameters = Json::array();
    unsigned count = 0;
    for (auto* p : function->ForEachProperty()) {
        if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
        if (++count > 64 || p->GetOffset_Internal() < 0 || p->GetElementSize() <= 0 || p->GetArrayDim() != 1 ||
            p->GetOffset_Internal() > function->GetParmsSize() ||
            p->GetElementSize() > function->GetParmsSize() - p->GetOffset_Internal())
            throw std::runtime_error("Function frame layout mismatch");
        parameters.push_back({{"name", narrow(p->GetName())}, {"size", p->GetElementSize()},
                              {"offset", p->GetOffset_Internal()}});
    }
    return {{"available", true}, {"owner", narrow(owner_path)}, {"name", narrow(name)},
            {"frame_size", function->GetParmsSize()}, {"parameter_count", function->GetNumParms()},
            {"parameters", std::move(parameters)}, {"executed", false}};
}
}
DiscoveryProbe::DiscoveryProbe(const std::filesystem::path& output) : writer_(output), session_(now_us()) {}
const char* DiscoveryProbe::state() const {
    using S = runtime::ProbeSchedule::State;
    switch (schedule_.state()) {
        case S::Idle: return "idle";
        case S::Running: return "running";
        case S::Complete: return "complete";
        case S::Cancelled: return "cancelled";
        case S::TimedOut: return "timed_out";
        case S::Failed: return "failed";
    }
    return "failed";
}
void DiscoveryProbe::emit(Json record) {
    record["session"] = session_;
    record["run"] = schedule_.run();
    record["time_us"] = now_us();
    if (!writer_.write(record.dump())) throw std::runtime_error("Probe output queue rejected record");
}
void DiscoveryProbe::toggle() {
    if (schedule_.state() == runtime::ProbeSchedule::State::Running) {
        cancel();
        return;
    }
    schedule_.start(now_us());
    phase_ = 0;
    index_ = 0;
    retry_at_ = 0;
    grants_.clear();
    pc_ = {}; pawn_ = {}; asc_ = {};
    reported_ = false;
#ifdef CCS_REGISTRY_CONTROLS
    registry_ = {}; known_montage_ = {};
    known_path_.clear(); known_package_.clear();
    negative_package_ = "/Game/__CCS_Probe_Nonexistent_" + std::to_string(session_) + "/Missing";
#endif
#ifdef CCS_REGISTRY_CONTROLS
    constexpr auto probe_name = "loaded_combat_02";
#else
    constexpr auto probe_name = "loaded_combat_01";
#endif
    try { emit({{"event", "start"}, {"schema_version", 1}, {"probe", probe_name}, {"read_only", true}}); }
    catch (...) { schedule_.fail(); reported_ = true; throw; }
}
void DiscoveryProbe::cancel() {
    if (schedule_.state() != runtime::ProbeSchedule::State::Running) return;
    schedule_.cancel();
    finish();
}
void DiscoveryProbe::finish() {
    if (reported_) return;
    reported_ = true;
    emit({{"event", "end"}, {"state", state()}, {"steps", schedule_.steps()},
          {"maximum_step_us", schedule_.maximum_us()}, {"abilities_read", index_}, {"abilities_expected", grants_.size()}});
}
bool DiscoveryProbe::capture_step(void* engine_ptr) {
    if (phase_ == 0) {
        const auto player = player_context(engine_ptr);
        if (!player.pc || !player.pawn || !player.asc) {
            retry_at_ = now_us() + 250'000;
            return false;
        }
        pc_.capture(player.pc); pawn_.capture(player.pawn); asc_.capture(player.asc);
        const auto spec = granted(player.asc);
        auto values = checked_array(spec.array, spec.data);
        const auto count = bounded_count(values, 256);
        if (count == 0) { retry_at_ = now_us() + 250'000; return false; }
        grants_.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) grants_.push_back(read_object(spec.ability, address(values.GetRawPtr(i), spec.ability)));
        emit({{"event", "player"}, {"pc", describe(player.pc)}, {"pawn", describe(player.pawn)},
              {"asc", describe(player.asc)}, {"granted_abilities", count}, {"source", "live"}});
        ++phase_;
        return false;
    }
    auto* pc = pc_.get(); auto* pawn = pawn_.get(); auto* asc = asc_.get();
    if (!pc || !pawn || !asc || object_of(pc, L"Pawn") != pawn || object_of(pawn, L"AbilitySystemComponent") != asc)
        throw std::runtime_error("Player generation changed during probe; arm a fresh capture");
    if (phase_ == 1) {
        emit({{"event", "interface"}, {"montage_task", function_signature(
            L"/Script/CSAbilityTasks.Default__AbilityTask_PlayMontageAndWaitWithNotifies", L"PlayMontageAndWaitWithNotifies")}});
        ++phase_;
        return false;
    }
    if (phase_ == 2) {
        emit({{"event", "interface"}, {"asset_registry", function_signature(
            L"/Script/AssetRegistry.AssetRegistry", L"GetAssetsByClass")}});
        ++phase_;
        return false;
    }
#ifdef CCS_REGISTRY_CONTROLS
    if (phase_ >= 4) return registry_step();
#endif
    const auto spec = granted(asc);
    auto values = checked_array(spec.array, spec.data);
    if (bounded_count(values, 256) != static_cast<int>(grants_.size()))
        throw std::runtime_error("Granted ability count changed during probe");
    if (index_ >= grants_.size()) return true;
    auto* data = values.GetRawPtr(static_cast<int>(index_));
    auto* ability = read_object(spec.ability, address(data, spec.ability));
    if (ability != grants_[index_]) throw std::runtime_error("Granted ability order changed during probe");
    Json record{{"event", "ability"}, {"index", index_}, {"ability", ability_record(ability)}};
    for (const auto* name : {L"NonReplicatedInstances", L"ReplicatedInstances"}) {
        auto* p = property(spec.spec, name);
        if (!p) continue;
        auto* array = array_property(p);
        if (!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize() != sizeof(UObject*))
            throw std::runtime_error("Ability instance array layout mismatch");
        auto instances = checked_array(array, address(data, p));
        Json rows = Json::array();
        const int count = bounded_count(instances, 8);
        for (int i = 0; i < count; ++i) rows.push_back(ability_record(read_object(array->GetInner(), instances.GetRawPtr(i))));
        record[narrow(name)] = std::move(rows);
    }
#ifdef CCS_REGISTRY_CONTROLS
    if (!known_montage_.ptr && record["ability"].is_object()) {
        for (const auto& group : record["ability"]["selector"]) {
            for (const auto& item : group.value("values", Json::array())) {
                if (!item["object"].is_object() || !item["object"].contains("montage") || item["object"]["montage"].is_null()) continue;
                known_path_ = item["object"]["montage"]["path"].get<std::string>();
                known_montage_.capture(find_optional(wide(known_path_).c_str()));
                if (!known_montage_.alive()) throw std::runtime_error("Known selector montage is no longer live");
                known_package_ = known_path_.substr(0, known_path_.rfind('.'));
                break;
            }
            if (known_montage_.ptr) break;
        }
    }
#endif
    emit(std::move(record));
    ++index_;
#ifdef CCS_REGISTRY_CONTROLS
    if (index_ == grants_.size()) phase_ = 4;
    return false;
#else
    return index_ == grants_.size();
#endif
}
#ifdef CCS_REGISTRY_CONTROLS
bool DiscoveryProbe::log_drained() {
    const auto state = writer_.drain_state();
    if (state == runtime::Writer::DrainState::Failed) throw std::runtime_error("Probe log write failed before registry call");
    return state == runtime::Writer::DrainState::Complete;
}
bool DiscoveryProbe::registry_step() {
    if (!known_montage_.alive()) throw std::runtime_error("No live selector montage for registry control");
    if (phase_ == 4) {
        emit({{"event", "before_call"}, {"method", "GetAssetRegistry"}});
        ++phase_;
        return false;
    }
    if (!log_drained()) return false;
    if (phase_ == 5) {
        Call call(find(L"/Script/AssetRegistry.Default__AssetRegistryHelpers"), L"GetAssetRegistry", 1);
        auto* result = call.param(L"ReturnValue");
        if (!result->IsA<FInterfaceProperty>() || result->GetElementSize() != sizeof(FScriptInterface))
            throw std::runtime_error("Registry interface return layout mismatch");
        call.run();
        const auto value = call.get<FScriptInterface>();
        registry_.capture(value.ObjectPointer);
        if (!registry_.alive() || !value.InterfacePointer) throw std::runtime_error("Registry interface is unavailable");
        emit({{"event", "registry"}, {"object", describe(registry_.get())}, {"executed", "GetAssetRegistry"}});
        emit({{"event", "before_call"}, {"method", "IsLoadingAssets"}});
        ++phase_;
        return false;
    }
    auto* registry = registry_.get();
    if (!registry) throw std::runtime_error("Registry generation changed");
    if (phase_ == 6) {
        Call call(registry, L"IsLoadingAssets", 1);
        require_bool(call, L"ReturnValue");
        call.run();
        const bool loading = call.get<bool>();
        emit({{"event", "registry_loading"}, {"loading", loading}});
        if (loading) throw std::runtime_error("Registry is still loading; a later capture is required");
        emit({{"event", "before_call"}, {"method", "GetAssetsByPackageName"}, {"query", known_package_}});
        ++phase_;
        return false;
    }
    if (phase_ == 7) {
        auto result = registry_query(registry, known_package_, false);
        bool found = false;
        for (const auto& asset : result["assets"]) found |= asset["path"] == known_path_ && asset["class"] == "/Script/Engine.AnimMontage";
        result["event"] = "registry_query";
        result["control"] = "known_montage";
        result["expected_path"] = known_path_;
        result["passed"] = found && result["returned"].get<bool>();
        emit(result);
        if (!result["passed"].get<bool>()) throw std::runtime_error("Known montage was not found in the registry");
        emit({{"event", "before_call"}, {"method", "GetAssetsByPackageName"}, {"query", negative_package_}});
        ++phase_;
        return false;
    }
    if (phase_ == 8) {
        auto result = registry_query(registry, negative_package_, false);
        result["event"] = "registry_query";
        result["control"] = "nonexistent_package";
        result["passed"] = result["assets"].empty();
        emit(result);
        if (!result["passed"].get<bool>()) throw std::runtime_error("Nonexistent package control unexpectedly returned assets");
        const auto folder = known_package_.substr(0, known_package_.rfind('/'));
        emit({{"event", "before_call"}, {"method", "GetAssetsByPath"}, {"query", folder}});
        ++phase_;
        return false;
    }
    auto result = registry_query(registry, known_package_.substr(0, known_package_.rfind('/')), true);
    bool found = false;
    for (const auto& asset : result["assets"]) found |= asset["path"] == known_path_;
    result["event"] = "registry_query";
    result["control"] = "known_folder";
    result["expected_path"] = known_path_;
    result["passed"] = found && result["returned"].get<bool>();
    emit(result);
    if (!result["passed"].get<bool>()) throw std::runtime_error("Known folder control did not contain the known montage");
    return true;
}
#endif

void DiscoveryProbe::tick(void* engine_ptr) {
    if (schedule_.state() != runtime::ProbeSchedule::State::Running) return;
    const auto start = now_us();
    if (!schedule_.poll(start)) { finish(); return; }
    if (start < retry_at_) return;
    try {
        const bool done = capture_step(engine_ptr);
        schedule_.step(now_us() - start, done);
        if (schedule_.state() != runtime::ProbeSchedule::State::Running) finish();
    } catch (const std::exception& error) {
        schedule_.fail();
        try { emit({{"event", "error"}, {"message", error.what()}}); finish(); } catch (...) {}
    } catch (...) {
        schedule_.fail();
        try { emit({{"event", "error"}, {"message", "Unknown probe failure"}}); finish(); } catch (...) {}
    }
}
}

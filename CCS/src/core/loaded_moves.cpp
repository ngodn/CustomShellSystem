#include "loaded_moves.hpp"
#include <algorithm>
#include <chrono>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <limits>

namespace ccs {
namespace {
using namespace engine;
constexpr std::array relationships{L"ComboAttackList", L"RunningAttackList", L"AdditionalAttacks"};
FProperty* property(UStruct* owner, const wchar_t* name) {
    if (!owner) throw std::runtime_error("Loaded move property owner unavailable");
    auto* p = owner->GetPropertyByNameInChain(name);
    if (!p) return nullptr;
    const auto offset = p->GetOffset_Internal(), size = p->GetElementSize();
    if (p->GetArrayDim() != 1 || offset < 0 || size <= 0 ||
        offset > owner->GetPropertiesSize() || size > owner->GetPropertiesSize() - offset)
        throw std::runtime_error("Loaded move property exceeds owner: " + narrow(name));
    return p;
}
void* address(void* object, FProperty* p) {
    return static_cast<std::byte*>(object) + p->GetOffset_Internal();
}
UObject* object(FProperty* p, void* storage) {
    if (!p || !p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Loaded move reference layout mismatch");
    return static_cast<FObjectProperty*>(p)->GetObjectPropertyValue(storage);
}
UScriptStruct* structure(FProperty* p) {
    if (!p || !p->IsA<FStructProperty>()) throw std::runtime_error("Loaded move struct unavailable");
    auto* type = static_cast<FStructProperty*>(p)->GetStruct().Get();
    if (!type || type->GetPropertiesSize() <= 0 || type->GetPropertiesSize() > p->GetElementSize())
        throw std::runtime_error("Loaded move struct size mismatch");
    return type;
}
FArrayProperty* array(FProperty* p) {
    if (!p || !p->IsA<FArrayProperty>() || p->GetElementSize() != sizeof(FScriptArray))
        throw std::runtime_error("Loaded move array layout mismatch");
    auto* a = static_cast<FArrayProperty*>(p);
    if (!a->GetInner() || a->GetInner()->GetElementSize() <= 0 ||
        a->GetInner()->GetArrayDim() != 1 ||
        !!(a->GetArrayFlags() & EArrayPropertyFlags::UsesMemoryImageAllocator))
        throw std::runtime_error("Loaded move array inner layout mismatch");
    return a;
}
FScriptArrayHelper values(FArrayProperty* p, void* storage, int limit) {
    auto* a = static_cast<FScriptArray*>(storage);
    const auto count = a->NumUnchecked();
    if (count < 0 || count > limit || a->Max() < count || a->Max() > 65536 || (count && !a->GetData()))
        throw std::runtime_error("Loaded move array storage mismatch");
    return FScriptArrayHelper(p, storage);
}
std::optional<std::vector<std::string>> tags_of(FProperty* field, void* storage) {
    if (!field) return std::nullopt;
    auto* container = structure(field);
    if (narrow(container->GetPathName()) != "/Script/GameplayTags.GameplayTagContainer")
        throw std::runtime_error("Loaded move tag container identity changed");
    auto* tags = array(property(container, L"GameplayTags"));
    auto* tag = structure(tags->GetInner());
    if (narrow(tag->GetPathName()) != "/Script/GameplayTags.GameplayTag")
        throw std::runtime_error("Loaded move tag identity changed");
    auto* name = property(tag, L"TagName");
    if (!name || !name->IsA<FNameProperty>() || name->GetElementSize() != sizeof(FName))
        throw std::runtime_error("Loaded move tag name layout mismatch");
    auto list = values(tags, address(storage, tags), 64);
    std::vector<std::string> result; result.reserve(static_cast<size_t>(list.NumUnchecked()));
    for (int i = 0; i < list.NumUnchecked(); ++i) {
        FName value;
        std::memcpy(&value, address(list.GetRawPtr(i), name), sizeof(value));
        auto text = narrow(value.ToString());
        if (text.empty() || text.size() > 1024) throw std::runtime_error("Loaded move tag exceeds bound");
        result.push_back(std::move(text));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
LoadedAbilityFacts facts_of(UObject* ability) {
    LoadedAbilityFacts result;
    auto* owner = ability->GetClassPrivate();
    if (auto* field = property(owner, L"InstancingPolicy")) {
        auto* underlying = field->IsA<FEnumProperty>() ? static_cast<FEnumProperty*>(field)->GetUnderlyingProperty() : field;
        if (!underlying || !underlying->IsA<FNumericProperty>() ||
            underlying->GetElementSize() != field->GetElementSize() || field->GetElementSize() > static_cast<int32_t>(sizeof(int64_t)))
            throw std::runtime_error("Loaded move instancing policy layout mismatch");
        auto* numeric = static_cast<FNumericProperty*>(underlying);
        if (numeric->IsFloatingPoint()) throw std::runtime_error("Loaded move instancing policy is not integral");
        const auto value = numeric->GetSignedIntPropertyValue(address(ability, field));
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max())
            throw std::runtime_error("Loaded move instancing policy exceeds bound");
        result.instancing_policy = static_cast<int32_t>(value);
    }
    if (auto* field = property(owner, L"IsHoldAttack")) {
        if (!field->IsA<FBoolProperty>() || field->GetElementSize() != sizeof(bool))
            throw std::runtime_error("Loaded move hold flag layout mismatch");
        result.is_hold = static_cast<FBoolProperty*>(field)->GetPropertyValue(address(ability, field));
    }
    auto* tags = property(owner, L"AbilityTags");
    result.asset_tags = tags_of(tags, tags ? address(ability, tags) : nullptr);
    return result;
}
struct Grants {
    FArrayProperty* items;
    FProperty* ability;
    FProperty* handle;
    FBoolProperty* pending_remove;
    std::array<FArrayProperty*, 2> instances{};
    FProperty* tags{};
    void* storage;
};
Grants granted(UObject* asc) {
    auto* container = property(asc->GetClassPrivate(), L"ActivatableAbilities");
    auto* type = structure(container);
    auto* items = array(property(type, L"Items"));
    auto* spec = structure(items->GetInner());
    auto* ability = property(spec, L"Ability");
    if (!ability || !ability->IsA<FObjectProperty>() || ability->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Loaded grant ability layout mismatch");
    auto* handle = property(spec, L"Handle");
    auto* handle_type = structure(handle);
    auto* value = property(handle_type, L"Handle");
    if (!value || !value->IsA<FIntProperty>() || value->GetElementSize() != sizeof(int32_t) ||
        handle_type->GetPropertiesSize() != sizeof(int32_t) || value->GetOffset_Internal() != 0 ||
        narrow(handle_type->GetPathName()) != "/Script/GameplayAbilities.GameplayAbilitySpecHandle")
        throw std::runtime_error("Loaded grant handle layout mismatch");
    auto* pending = property(spec, L"PendingRemove");
    if (!pending || !pending->IsA<FBoolProperty>() || pending->GetElementSize() != sizeof(bool))
        throw std::runtime_error("Loaded grant removal flag layout mismatch");
    std::array<FArrayProperty*, 2> instances;
    for (size_t i = 0; const auto* name : {L"NonReplicatedInstances", L"ReplicatedInstances"}) {
        auto* list = array(property(spec, name));
        if (!list->GetInner()->IsA<FObjectProperty>() || list->GetInner()->GetElementSize() != sizeof(UObject*))
            throw std::runtime_error("Loaded grant instance array layout mismatch");
        instances[i++] = list;
    }
    return {items, ability, handle, static_cast<FBoolProperty*>(pending), instances,
        property(spec, L"DynamicAbilityTags"), address(address(asc, container), items)};
}
int32_t handle_of(const Grants& fields, void* spec) {
    int32_t handle{}; std::memcpy(&handle, address(spec, fields.handle), sizeof(handle));
    if (handle == invalid_ability_spec_handle) throw std::runtime_error("Loaded grant handle is not valid");
    return handle;
}
bool pending_remove(const Grants& fields, void* spec) {
    return fields.pending_remove->GetPropertyValue(address(spec, fields.pending_remove));
}
std::vector<ObjectHandle> instances_of(const Grants& fields, void* spec, UObject* ability) {
    std::vector<ObjectHandle> result;
    for (auto* field : fields.instances) {
        auto list = values(field, address(spec, field), 16);
        for (int i = 0; i < list.NumUnchecked(); ++i) {
            auto* value = object(field->GetInner(), list.GetRawPtr(i));
            if (!value) continue;
            if (!ability || value->GetClassPrivate() != ability->GetClassPrivate() ||
                value->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
                throw std::runtime_error("Loaded grant contains an invalid ability instance");
            if (std::any_of(result.begin(), result.end(), [&](const auto& entry) { return entry.get() == value; })) continue;
            ObjectHandle captured; captured.capture(value); result.push_back(captured);
        }
    }
    return result;
}
std::string path(UObject* value) {
    if (!value) return {};
    const auto result = narrow(value->GetPathName());
    if (result.empty() || result.size() > 4096) throw std::runtime_error("Loaded move identity exceeds bound");
    return result;
}
}
void LoadedMoves::collect_selector(UObject* selector, int32_t selector_handle, bool selector_instanced) {
    if (!selector) return;
    for (unsigned relationship = 0; const auto* name : relationships) {
        const auto role = relationship++;
        auto* p = property(selector->GetClassPrivate(), name);
        if (!p) continue;
        auto* a = array(p);
        if (!a->GetInner()->IsA<FObjectProperty>() || a->GetInner()->GetElementSize() != sizeof(UObject*))
            throw std::runtime_error("Loaded selector array is not an object reference array");
        auto list = values(a, address(selector, p), 16);
        for (int i = 0; i < list.NumUnchecked(); ++i) {
            auto* cls = object(a->GetInner(), list.GetRawPtr(i));
            if (!cls) continue;
            if (!cls->IsA<UClass>()) throw std::runtime_error("Loaded selector entry is not an ability class");
            auto* cdo = static_cast<UClass*>(cls)->GetClassDefaultObject().Get();
            if (!cdo) continue;
            const auto append = [&](UObject* ability, int32_t ability_handle, bool instanced, std::optional<size_t> grant_index) {
                if (candidates_.size() >= 256) throw std::runtime_error("Loaded selector candidates exceed bound");
                Candidate candidate;
                candidate.selector.capture(selector); candidate.ability_class.capture(cls); candidate.ability.capture(ability);
                candidate.relationship = role; candidate.position = static_cast<unsigned>(i);
                candidate.selector_handle = selector_handle; candidate.ability_handle = ability_handle;
                candidate.selector_instanced = selector_instanced; candidate.ability_instanced = instanced;
                candidate.grant_index = grant_index;
                candidates_.push_back(std::move(candidate));
            };
            const auto [first, last] = grant_classes_.equal_range(reinterpret_cast<uintptr_t>(cls));
            for (auto entry = first; entry != last; ++entry) {
                const auto& grant = grants_[entry->second];
                if (grant.pending_remove) continue;
                auto* current = grant.ability.get();
                if (!current || current != cdo) throw std::runtime_error("Referenced attack grant changed during discovery");
                if (grant.instances.empty()) append(current, grant.handle, false, entry->second);
                else for (const auto& instance : grant.instances) {
                    auto* live = instance.get();
                    if (!live) throw std::runtime_error("Referenced attack instance expired during discovery");
                    append(live, grant.handle, true, entry->second);
                }
            }
            if (first == last) append(cdo, invalid_ability_spec_handle, false, std::nullopt);
        }
    }
}
void LoadedMoves::read_candidate(size_t index, bool verify) {
    auto& candidate = candidates_[index];
    auto* selector = candidate.selector.get(); auto* ability = candidate.ability.get();
    auto* cls = candidate.ability_class.get();
    if (!selector || !ability || !cls || ability->GetClassPrivate() != cls)
        throw std::runtime_error("Loaded selector or referenced ability expired during discovery");
    auto* list_property = array(property(selector->GetClassPrivate(), relationships[candidate.relationship]));
    if (!list_property->GetInner()->IsA<FObjectProperty>() || list_property->GetInner()->GetElementSize() != sizeof(UObject*))
        throw std::runtime_error("Loaded selector reference array layout changed");
    auto list = values(list_property, address(selector, list_property), 16);
    if (candidate.position >= static_cast<unsigned>(list.NumUnchecked()) ||
        object(list_property->GetInner(), list.GetRawPtr(static_cast<int>(candidate.position))) != cls)
        throw std::runtime_error("Loaded selector references changed during discovery");
    auto* montage_property = property(ability->GetClassPrivate(), L"Montage");
    auto* montage = montage_property ? object(montage_property, address(ability, montage_property)) : nullptr;
    auto* skeleton_property = montage ? property(montage->GetClassPrivate(), L"Skeleton") : nullptr;
    auto* skeleton = skeleton_property ? object(skeleton_property, address(montage, skeleton_property)) : nullptr;
    auto selector_facts = facts_of(selector), ability_facts = facts_of(ability);
    if (verify) {
        if ((candidate.montage.ptr ? !montage || candidate.montage.get() != montage : montage != nullptr) ||
            (candidate.skeleton.ptr ? !skeleton || candidate.skeleton.get() != skeleton : skeleton != nullptr))
            throw std::runtime_error("Observed montage or skeleton changed before publication");
        if (selector_facts != candidate.selector_facts || ability_facts != candidate.ability_facts)
            throw std::runtime_error("Observed ability facts changed before publication");
        return;
    }
    candidate.montage.capture(montage); candidate.skeleton.capture(skeleton);
    candidate.selector_facts = std::move(selector_facts); candidate.ability_facts = std::move(ability_facts);
    if (!montage) return;
    building_->candidates.push_back({path(selector), narrow(relationships[candidate.relationship]), candidate.position,
        path(cls), path(montage), path(skeleton), path(ability), candidate.selector_handle, candidate.ability_handle,
        candidate.selector_instanced, candidate.ability_instanced, candidate.selector_facts, candidate.ability_facts,
        candidate.grant_index ? grants_[*candidate.grant_index].tags : std::nullopt});
}

void LoadedMoves::reset() {
    world_ = {}; pc_ = {}; pawn_ = {}; asc_ = {}; weapon_ = {}; shell_ = {};
    grants_.clear(); previous_grants_.clear(); grant_classes_.clear(); candidates_.clear(); building_.reset(); published_.reset(); index_ = instance_index_ = expected_grants_ = 0;
    phase_ = Phase::Instances;
    retry_at_ = 0; started_at_ = 0; active_ = false;
    state_ = "idle"; error_.clear();
}
void LoadedMoves::suspend(std::string_view state, std::string_view error) {
    if (state_ == state && error_ == error && !published_ && !building_) return;
    reset(); state_.assign(state); error_.assign(error);
}
bool LoadedMoves::same_player(const PlayerContext& p, UObject* item, UObject* shell) const {
    return p.world && p.pc && p.pawn && p.asc && world_.get() == p.world &&
        pc_.get() == p.pc && pawn_.get() == p.pawn && asc_.get() == p.asc &&
        (weapon_.ptr ? weapon_.get() == item : !item) && (shell_.ptr ? shell_.get() == shell : !shell);
}
void LoadedMoves::start(const PlayerContext& p, UObject* item, UObject* shell) {
    world_.capture(p.world); pc_.capture(p.pc); pawn_.capture(p.pawn); asc_.capture(p.asc);
    weapon_.capture(item); shell_.capture(shell);
    const auto spec = granted(p.asc);
    auto list = values(spec.items, spec.storage, 256);
    if (!list.NumUnchecked()) throw std::runtime_error("Player abilities are not available yet");
    grant_classes_.clear(); candidates_.clear();
    expected_grants_ = static_cast<size_t>(list.NumUnchecked());
    if (grants_.size() != expected_grants_) published_.reset();
    previous_grants_ = std::move(grants_); grants_.clear(); grants_.reserve(expected_grants_);
    building_ = std::make_shared<LoadedMovesSnapshot>();
    building_->weapon_path = path(item);
    index_ = instance_index_ = 0; phase_ = Phase::CaptureGrants; state_ = "reading"; error_.clear();
}
void LoadedMoves::step(const PlayerContext& p, uint64_t now) {
    const auto spec = granted(p.asc);
    auto list = values(spec.items, spec.storage, 256);
    if (static_cast<size_t>(list.NumUnchecked()) != expected_grants_)
        throw std::runtime_error("Player grants changed during discovery");
    if (phase_ == Phase::CaptureGrants) {
        if (index_ == expected_grants_) { previous_grants_.clear(); phase_ = Phase::Instances; index_ = 0; return; }
        auto* raw = list.GetRawPtr(static_cast<int>(index_));
        auto* ability = object(spec.ability, address(raw, spec.ability));
        if (ability && !ability->HasAnyFlags(RF_ClassDefaultObject))
            throw std::runtime_error("Loaded grant ability is not its class default object");
        Grant grant;
        grant.ability.capture(ability); grant.handle = handle_of(spec, raw); grant.pending_remove = pending_remove(spec, raw);
        if (index_ < previous_grants_.size()) {
            auto& old = previous_grants_[index_];
            if (old.handle != grant.handle || old.pending_remove != grant.pending_remove ||
                (old.ability.ptr ? old.ability.get() != ability : ability != nullptr)) published_.reset();
            else { grant.instances = std::move(old.instances); grant.tags = std::move(old.tags); }
        }
        if (ability) grant_classes_.emplace(reinterpret_cast<uintptr_t>(ability->GetClassPrivate()), index_);
        grants_.push_back(std::move(grant)); ++index_; return;
    }
    if (phase_ == Phase::Selectors)
        while (index_ < grants_.size() && !grants_[index_].selector) ++index_;
    if (phase_ == Phase::ReadCandidates || phase_ == Phase::VerifyCandidates) {
        if (index_ < candidates_.size()) { read_candidate(index_++, phase_ == Phase::VerifyCandidates); return; }
        if (phase_ == Phase::ReadCandidates) { phase_ = Phase::VerifyGrants; index_ = 0; return; }
        building_->revision = ++revision_; building_->observed_at_ms = now;
        published_ = std::move(building_); state_ = "observed"; return;
    }
    if (index_ < grants_.size()) {
        auto* raw = list.GetRawPtr(static_cast<int>(index_));
        auto* current = object(spec.ability, address(raw, spec.ability));
        auto& captured = grants_[index_];
        if ((captured.ability.ptr ? !current || captured.ability.get() != current : current != nullptr) ||
            captured.handle != handle_of(spec, raw) || captured.pending_remove != pending_remove(spec, raw))
            throw std::runtime_error("Player grant identity changed during discovery");
        if (phase_ == Phase::Instances) {
            auto actual = instances_of(spec, raw, current);
            auto tags = tags_of(spec.tags, spec.tags ? address(raw, spec.tags) : nullptr);
            if (published_ && tags != captured.tags) published_.reset();
            captured.tags = std::move(tags);
            if (published_ && (actual.size() != captured.instances.size() ||
                !std::equal(actual.begin(), actual.end(), captured.instances.begin(), [](const auto& a, const auto& b) {
                    return a.get() == b.get();
                }))) published_.reset();
            captured.instances = std::move(actual);
            if (current) captured.selector = std::any_of(relationships.begin(), relationships.end(), [&](const auto* name) {
                return property(current->GetClassPrivate(), name) != nullptr;
            });
            ++index_;
        } else if (phase_ == Phase::Selectors) {
            if (!captured.pending_remove) {
                if (captured.instances.empty()) collect_selector(current, captured.handle, false);
                else {
                    auto* instance = captured.instances[instance_index_].get();
                    if (!instance) throw std::runtime_error("Selector instance expired during discovery");
                    collect_selector(instance, captured.handle, true);
                }
            }
            if (captured.pending_remove || captured.instances.empty() || ++instance_index_ >= captured.instances.size()) {
                ++index_; instance_index_ = 0;
            }
        } else {
            const auto actual = instances_of(spec, raw, current);
            if (tags_of(spec.tags, spec.tags ? address(raw, spec.tags) : nullptr) != captured.tags)
                throw std::runtime_error("Player grant tags changed before publication");
            if (actual.size() != captured.instances.size()) throw std::runtime_error("Player ability instances changed before publication");
            for (size_t i = 0; i < actual.size(); ++i)
                if (actual[i].get() != captured.instances[i].get()) throw std::runtime_error("Player ability instance identity changed before publication");
            ++index_;
        }
        return;
    }
    if (phase_ == Phase::Instances) { phase_ = Phase::Selectors; index_ = 0; return; }
    if (phase_ == Phase::Selectors) { phase_ = Phase::ReadCandidates; index_ = 0; return; }
    phase_ = Phase::VerifyCandidates; index_ = 0;
}
void LoadedMoves::tick(const PlayerContext& p, bool active, uint64_t now) {
    if (!active) { if (active_) reset(); return; }
    if (!active_) { active_ = true; retry_at_ = 0; }
    const auto begin = std::chrono::steady_clock::now();
    try {
        auto* item = object_of(p.pc, L"ActiveWeaponItemDefinition");
        auto* shell = object_of(p.pc, L"ActiveShellItemDefinition");
        if ((pc_.ptr || pawn_.ptr || asc_.ptr) && !same_player(p, item, shell)) {
            reset(); active_ = true;
        }
        if (!p.world || !p.pc || !p.pawn || !p.asc || !item) {
            published_.reset(); state_ = "waiting_for_player"; return;
        }
        if (state_ == "budget_exceeded") return;
        if (now < retry_at_) return;
        if (!building_) { start(p, item, shell); started_at_ = now; }
        else {
            if (now - started_at_ >= 30000) throw std::runtime_error("Loaded move discovery timed out");
            step(p, now);
        }
        const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - begin).count());
        maximum_step_us_ = std::max(maximum_step_us_, elapsed);
        if (elapsed > 2000) {
            building_.reset(); published_.reset(); grants_.clear(); previous_grants_.clear(); grant_classes_.clear(); candidates_.clear();
            state_ = "budget_exceeded"; error_ = "Loaded move discovery exceeded measured step budget";
            return;
        }
        if (!building_) retry_at_ = now + 5000;
    } catch (const std::exception& e) {
        const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - begin).count());
        maximum_step_us_ = std::max(maximum_step_us_, elapsed);
        building_.reset(); published_.reset(); grants_.clear(); previous_grants_.clear(); grant_classes_.clear(); candidates_.clear();
        state_ = elapsed > 2000 ? "budget_exceeded" : "unavailable";
        error_ = e.what(); retry_at_ = now + 1000;
    }
}
}

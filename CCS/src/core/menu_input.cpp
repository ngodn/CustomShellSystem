#include "menu_input.hpp"
#include <algorithm>
#include <string_view>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
namespace {
using namespace engine;
using Action = runtime::MenuAction;
std::optional<Action> action_of(std::wstring_view name) {
    static constexpr std::array<std::pair<std::wstring_view, Action>, runtime::menu_action_count> actions{{
        {L"IA_Menu_Back", Action::Cancel}, {L"IA_Menu_Confirm_Primary_Press", Action::Accept},
        {L"IA_Menu_Confirm_Secondary_Press", Action::Unequip}, {L"IA_Menu_Inspect", Action::Details},
        {L"IA_Menu_Left_Tertiary", Action::TabLeft}, {L"IA_Menu_Right_Tertiary", Action::TabRight},
        {L"IA_Menu_Up", Action::Up}, {L"IA_Menu_Down", Action::Down},
        {L"IA_Menu_Left_Primary", Action::Left}, {L"IA_Menu_Right_Primary", Action::Right}
    }};
    for (const auto& [source, action] : actions) if (source == name) return action;
    return std::nullopt;
}
UStruct* struct_type(FProperty* property) {
    if (!property || !property->IsA<FStructProperty>() || property->GetArrayDim() != 1)
        throw std::runtime_error("Menu input value is not a scalar struct");
    auto* type = static_cast<FStructProperty*>(property)->GetStruct().Get();
    if (!type || type->GetPropertiesSize() <= 0 || type->GetPropertiesSize() > property->GetElementSize())
        throw std::runtime_error("Menu input struct exceeds its storage");
    return type;
}
FProperty* name_field(UStruct* type) {
    auto* name = field(type, L"KeyName", sizeof(FName));
    if (!name->IsA<FNameProperty>()) throw std::runtime_error("Input key name type changed");
    return name;
}
bool usable(const std::string& key) {
    return !key.empty() && key.size() <= 128 && key != "None" && key != "Gamepad_LeftX" &&
        key != "Gamepad_LeftY" && key != "Gamepad_RightX" && key != "Gamepad_RightY";
}
}

void MenuInput::reset() {
    ++revision_;
    controller_.Reset(); mapping_.Reset(); subsystem_.Reset(); keys_.clear();
    for (auto& route : routes_) route.clear();
    edges_.reset();
    typing_ = false;
}

bool MenuInput::refresh(UObject* controller, UObject* handler) {
    auto* mapping = object_of(handler, L"InputMapping");
    if (!controller || !mapping) throw std::runtime_error("Player Menu input mapping is unavailable");
    auto* mappings = mapping->GetPropertyByNameInChain(L"Mappings");
    if (!mappings || !mappings->IsA<FArrayProperty>()) throw std::runtime_error("Menu mappings are not an array");
    field(mapping, L"Mappings", mappings->GetElementSize());
    auto* array = static_cast<FArrayProperty*>(mappings);
    auto* mapping_type = struct_type(array->GetInner());
    auto* action_field = field(mapping_type, L"Action", sizeof(UObject*));
    if (!action_field->IsA<FObjectProperty>()) throw std::runtime_error("Mapped action is not an object");
    FScriptArrayHelper values(array, reinterpret_cast<const std::byte*>(mapping) + mappings->GetOffset_Internal());
    if (values.Num() < 0 || values.Num() > 256) throw std::runtime_error("Menu mapping exceeds bound");
    Call subsystem(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"), L"GetLocalPlayerSubSystemFromPlayerController", 3);
    subsystem.set(L"PlayerController", controller);
    subsystem.set(L"Class", static_cast<UClass*>(find(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem")));
    subsystem.run();
    auto* input = subsystem.get<UObject*>();
    if (!input) throw std::runtime_error("Enhanced Input subsystem is unavailable");
    std::vector<Key> keys;
    Routes routes;
    std::vector<UObject*> seen;
    std::vector<std::pair<Action, WeakObject>> actions;
    seen.reserve(runtime::menu_action_count);
    for (int i = 0; i < values.Num(); ++i) {
        auto* action = static_cast<FObjectProperty*>(action_field)->GetObjectPropertyValue(values.GetRawPtr(i) + action_field->GetOffset_Internal());
        if (!action || std::find(seen.begin(), seen.end(), action) != seen.end()) continue;
        const auto semantic = action_of(action->GetName());
        if (!semantic) continue;
        if (seen.size() >= runtime::menu_action_count) throw std::runtime_error("Menu actions are ambiguous");
        seen.push_back(action);
        actions.emplace_back(*semantic, WeakObject(action));
    }
    Call key_check(controller, L"IsInputKeyDown", 2);
    if (struct_type(key_check.param(L"Key"))->GetPathName() != L"/Script/InputCore.Key")
        throw std::runtime_error("Input polling key struct type changed");
    for (const auto& [semantic, reference] : actions) {
        auto* action = reference.Get();
        if (!action) throw std::runtime_error("Menu input action expired during binding");
        Call query(input, L"QueryKeysMappedToAction", 2); query.set(L"Action", action); query.run();
        auto* output = query.param(L"ReturnValue");
        if (!output->IsA<FArrayProperty>()) throw std::runtime_error("Mapped keys are not an array");
        auto* key_array = static_cast<FArrayProperty*>(output);
        auto* key_type = struct_type(key_array->GetInner());
        if (key_type->GetPathName() != L"/Script/InputCore.Key")
            throw std::runtime_error("Mapped key struct type changed");
        auto* key_name = name_field(key_type);
        FScriptArrayHelper mapped(key_array, query.data(output));
        if (mapped.Num() < 0 || mapped.Num() > 32) throw std::runtime_error("Mapped key count exceeds bound");
        auto& route = routes[static_cast<size_t>(semantic)];
        for (int k = 0; k < mapped.Num(); ++k) {
            FName name{};
            std::memcpy(&name, mapped.GetRawPtr(k) + key_name->GetOffset_Internal(), sizeof(name));
            const auto label = narrow(name.ToString());
            if (!usable(label)) continue;
            const auto found = std::find_if(keys.begin(), keys.end(), [&](const Key& key) { return key.name == name; });
            size_t index = static_cast<size_t>(found - keys.begin());
            if (found == keys.end()) {
                if (keys.size() >= 64) throw std::runtime_error("Distinct menu key count exceeds bound");
                keys.push_back({name, label});
            }
            if (std::find(route.begin(), route.end(), index) == route.end()) route.push_back(index);
        }
    }
    if (seen.empty()) throw std::runtime_error("No supported native menu actions were found");
    std::vector<size_t> permutation;
    permutation.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) permutation.push_back(i);
    std::sort(permutation.begin(), permutation.end(), [&](size_t a, size_t b) { return keys[a].label < keys[b].label; });
    std::vector<Key> sorted;
    sorted.reserve(keys.size());
    std::array<size_t, 64> translated{};
    for (size_t i = 0; i < permutation.size(); ++i) {
        translated[permutation[i]] = i;
        sorted.push_back(std::move(keys[permutation[i]]));
    }
    keys = std::move(sorted);
    for (auto& route : routes) {
        for (auto& key : route) key = translated[key];
        std::sort(route.begin(), route.end());
    }
    if (controller_.Get() != controller || mapping_.Get() != mapping || subsystem_.Get() != input || keys_ != keys || routes_ != routes) {
        edges_.reset();
        ++revision_;
    }
    controller_ = controller; mapping_ = mapping; subsystem_ = input;
    keys_ = std::move(keys); routes_ = std::move(routes);
    return !keys_.empty();
}

std::optional<Action> MenuInput::poll(uint64_t now, bool typing) {
    auto* controller = controller_.Get();
    if (!controller || !mapping_.Get() || !subsystem_.Get()) { reset(); return std::nullopt; }
    if (typing_ != typing) { edges_.reset(); typing_ = typing; }
    std::array<bool, 64> down{};
    for (size_t i = 0; i < keys_.size(); ++i) {
        Call query(controller, L"IsInputKeyDown", 2);
        auto* key = query.param(L"Key");
        auto* name = name_field(struct_type(key));
        name->CopyCompleteValue(static_cast<std::byte*>(query.data(key)) + name->GetOffset_Internal(), &keys_[i].name);
        query.run(); down[i] = query.get<bool>();
    }
    runtime::InputEdges::Sample actions{};
    for (size_t i = 0; i < routes_.size(); ++i)
        for (const auto key : routes_[i])
            if (runtime::typing_key_allowed(static_cast<Action>(i), keys_[key].label, typing)) actions[i] |= down[key];
    return edges_.update(now, actions);
}

std::string MenuInput::hint(Action action) const {
    if (action >= Action::Count) return "Unmapped";
    const auto& route = routes_[static_cast<size_t>(action)];
    if (route.empty()) return "Unmapped";
    std::string value;
    for (size_t i = 0; i < std::min(route.size(), size_t{2}); ++i) {
        if (i) value += " / ";
        value += keys_[route[i]].label;
    }
    return value;
}
}

#include "engine.hpp"
#include <windows.h>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs::engine {

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), result.data(), size);
    return result;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Invalid Unicode text");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), result.data(), size, nullptr, nullptr);
    return result;
}

UObject* find_optional(const wchar_t* path) {
    return UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path);
}

namespace {
void validate_serial_initializer(Call& call) {
    auto* function = call.function();
    if (!function->HasAllFunctionFlags(FUNC_Native | FUNC_Static) ||
        function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate))
        throw std::runtime_error("Serial initializer is not a native static function");
    auto* input = call.param(L"Object"); auto* output = call.param(L"ReturnValue");
    if (!input->IsA<FObjectProperty>() || input->GetElementSize() != sizeof(UObject*) ||
        input->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm) ||
        !output->IsA<FSoftObjectProperty>() || !output->HasAnyPropertyFlags(CPF_ReturnParm))
        throw std::runtime_error("Serial initializer parameter contract changed");
    auto* input_class = static_cast<FObjectProperty*>(input)->GetPropertyClass().Get();
    if (!input_class || narrow(input_class->GetPathName()) != "/Script/CoreUObject.Object")
        throw std::runtime_error("Serial initializer input class changed");
    const auto input_start = input->GetOffset_Internal(), input_end = input_start + input->GetElementSize();
    const auto output_start = output->GetOffset_Internal(), output_end = output_start + output->GetElementSize();
    if (input_start < output_end && output_start < input_end)
        throw std::runtime_error("Serial initializer parameter storage overlaps");
}
void initialize_serial(UObject* object) {
    auto* item = FUObjectArray::IndexToObject(object->GetInternalIndex());
    if (!item || item->GetUObject() != object || !item->IsValid(false))
        throw std::runtime_error("Object is not live for serial initialization");
    if (item->GetSerialNumber() > 0) return;
    static thread_local bool busy{};
    if (busy) throw std::runtime_error("Object serial initialization reentered");
    busy = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{busy};
    const auto index = object->GetInternalIndex();
    const auto name = object->GetNamePrivate();
    Call call(find(L"/Script/Engine.Default__KismetSystemLibrary"), L"Conv_ObjectToSoftObjectReference", 2,
        Call::Cache::SerialInitializer);
    call.set(L"Object", object); call.run();
    item = FUObjectArray::IndexToObject(index);
    if (!item || item->GetUObject() != object || !item->IsValid(false) || item->GetSerialNumber() <= 0 ||
        object->GetNamePrivate() != name)
        throw std::runtime_error("Engine did not establish the live object serial");
}
}
bool ObjectHandle::capture_existing(UObject* object) {
    *this = {};
    if (!object) return true;
    const auto object_index = object->GetInternalIndex();
    auto* item = FUObjectArray::IndexToObject(object_index);
    if (!item || item->GetUObject() != object || !item->IsValid(false) || item->GetSerialNumber() <= 0) return false;
    ptr = object;
    index = object_index;
    serial = item->GetSerialNumber();
    name = object->GetNamePrivate();
    return true;
}
void ObjectHandle::capture(UObject* object) {
    *this = {};
    if (!object) return;
    initialize_serial(object);
    if (!capture_existing(object)) throw std::runtime_error("Object identity changed during capture");
}
UObject* ObjectHandle::get() const {
    if (!ptr || index < 0 || serial <= 0) return nullptr;
    auto* item = FUObjectArray::IndexToObject(index);
    if (!item || item->GetUObject() != ptr || item->GetSerialNumber() != serial || !item->IsValid(false)) return nullptr;
    auto* object = static_cast<UObject*>(item->GetUObject());
    return object->GetNamePrivate() == name ? object : nullptr;
}

namespace {
struct ReflKey { const void* owner; std::wstring name; };
struct ReflView { const void* owner; std::wstring_view name; };
struct ReflHash {
    using is_transparent = void;
    static size_t mix(const void* o, std::wstring_view n) noexcept {
        size_t h = std::hash<const void*>{}(o) + 0x9e3779b97f4a7c15ull;
        h ^= std::hash<std::wstring_view>{}(n) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    }
    size_t operator()(const ReflKey& k) const noexcept { return mix(k.owner, k.name); }
    size_t operator()(const ReflView& k) const noexcept { return mix(k.owner, k.name); }
};
struct ReflEq {
    using is_transparent = void;
    bool operator()(const ReflKey& a, const ReflKey& b) const noexcept { return a.owner == b.owner && a.name == b.name; }
    bool operator()(const ReflKey& a, const ReflView& b) const noexcept { return a.owner == b.owner && a.name == b.name; }
    bool operator()(const ReflView& a, const ReflKey& b) const noexcept { return a.owner == b.owner && a.name == b.name; }
    bool operator()(const ReflView& a, const ReflView& b) const noexcept { return a.owner == b.owner && a.name == b.name; }
};
using OwnerGuard = ObjectHandle;
struct FieldEntry { OwnerGuard owner; FProperty* property{nullptr}; };
std::unordered_map<ReflKey, FieldEntry, ReflHash, ReflEq> g_field_cache;
struct CallEntry { OwnerGuard owner, function_guard; UFunction* function{nullptr}; std::vector<FProperty*> params; };
std::unordered_map<ReflKey, CallEntry, ReflHash, ReflEq> g_call_cache;
} // namespace

UObject* find_cached(const wchar_t* path) {
    static std::unordered_map<std::wstring, OwnerGuard> cache;
    if (auto it = cache.find(path); it != cache.end() && it->second.alive())
        return static_cast<UObject*>(const_cast<void*>(it->second.ptr));
    auto* object = find(path);
    OwnerGuard guard;
    guard.capture(object);
    cache.insert_or_assign(path, guard);
    return object;
}

static FProperty* resolve_field(UObject* object, const wchar_t* name) {
    if (!object) return nullptr;
    UObject* owner = object->IsA<UStruct>() ? object : object->GetClassPrivate();
    if (owner) {
        if (auto it = g_field_cache.find(ReflView{owner, name}); it != g_field_cache.end() && it->second.owner.alive())
            return it->second.property;
    }
    auto* property = object->GetPropertyByNameInChain(name);
    if (property && owner) {
        FieldEntry e;
        e.owner.capture(owner);
        e.property = property;
        g_field_cache.insert_or_assign(ReflKey{owner, name}, e);
    }
    return property;
}

UObject* find(const wchar_t* path) {
    auto* object = find_optional(path);
    if (!object) throw std::runtime_error("Required reflected object is missing: " + narrow(path));
    return object;
}

FProperty* field(UObject* object, const wchar_t* name, size_t size) {
    if (!object) throw std::runtime_error("No live object for " + narrow(name));
    auto* property = resolve_field(object, name);
    if (!property || property->GetElementSize() != static_cast<int32_t>(size) || property->GetArrayDim() != 1)
        throw std::runtime_error("Reflected property layout does not match: " + narrow(name));
    auto* owner = object->IsA<UStruct>() ? static_cast<UStruct*>(object) : object->GetClassPrivate();
    const auto offset = property->GetOffset_Internal();
    if (!owner || offset < 0 || offset > owner->GetPropertiesSize() ||
        property->GetElementSize() > owner->GetPropertiesSize() - offset)
        throw std::runtime_error("Reflected property exceeds owner: " + narrow(name));
    return property;
}

UObject* object_of(UObject* object, const wchar_t* name) {
    auto* p = resolve_field(object, name);
    if (!p || !p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*)) return nullptr;
    p = field(object, name, sizeof(UObject*));
    return static_cast<FObjectProperty*>(p)->GetObjectPropertyValue(reinterpret_cast<const std::byte*>(object) + p->GetOffset_Internal());
}

UObject* cached_object_of(UObject* object, const wchar_t* name) {
    if (!object) return nullptr;
    auto* owner = object->IsA<UStruct>() ? object : object->GetClassPrivate();
    const auto found = g_field_cache.find(ReflView{owner, name});
    if (found == g_field_cache.end() || !found->second.owner.alive()) return nullptr;
    auto* p = found->second.property;
    if (!p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*) || p->GetArrayDim() != 1) return nullptr;
    const auto offset = p->GetOffset_Internal();
    auto* type = static_cast<UStruct*>(owner);
    if (offset < 0 || offset > type->GetPropertiesSize() || p->GetElementSize() > type->GetPropertiesSize() - offset)
        return nullptr;
    return static_cast<FObjectProperty*>(p)->GetObjectPropertyValue(reinterpret_cast<const std::byte*>(object) + offset);
}

bool bool_of(UObject* object, const wchar_t* name, bool fallback) {
    auto* p = resolve_field(object, name);
    if (!p || !p->IsA<FBoolProperty>()) return fallback;
    return static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(object);
}

WeakObject::WeakObject(UObject* object) { *this = object; }
WeakObject& WeakObject::operator=(UObject* object) {
    if (object) initialize_serial(object);
    FWeakObjectPtr::operator=(object);
    return *this;
}

static CallEntry inspect_call(UObject* object, const wchar_t* name) {
    CallEntry entry;
    entry.function = object->GetFunctionByNameInChain(name);
    if (!entry.function || entry.function->GetParmsSize() > 2048)
        throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
    for (auto* p : entry.function->ForEachProperty()) {
        if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
        if (entry.params.size() >= 64 || p->GetOffset_Internal() < 0 || p->GetElementSize() <= 0 ||
            p->GetArrayDim() != 1 || p->GetMinAlignment() > 16 ||
            p->GetOffset_Internal() > entry.function->GetParmsSize() ||
            p->GetElementSize() > entry.function->GetParmsSize() - p->GetOffset_Internal())
            throw std::runtime_error("Parameter exceeds reflected frame: " + narrow(name));
        entry.params.push_back(p);
    }
    return entry;
}
static const CallEntry& resolve_call(UObject* object, const wchar_t* name) {
    auto* owner = object->GetClassPrivate();
    if (!owner) throw std::runtime_error("Reflected call has no class owner");
    if (auto it = g_call_cache.find(ReflView{owner, name}); it != g_call_cache.end() &&
        it->second.owner.alive() && it->second.function_guard.alive()) return it->second;
    auto entry = inspect_call(object, name);
    entry.owner.capture(owner);
    entry.function_guard.capture(entry.function);
    return g_call_cache.insert_or_assign(ReflKey{owner, name}, std::move(entry)).first->second;
}

Call::Call(UObject* object, const wchar_t* name, unsigned count, Cache cache) : object_(object) {
    if (!object) throw std::runtime_error("No target for reflected call " + narrow(name));
    if (cache == Cache::SerialInitializer) {
        auto* owner = object->GetClassPrivate();
        if (std::wstring_view(name) != L"Conv_ObjectToSoftObjectReference" || !owner ||
            narrow(owner->GetPathName()) != "/Script/Engine.KismetSystemLibrary")
            throw std::runtime_error("Uncached reflected call is reserved for serial initialization");
        auto entry = inspect_call(object, name);
        function_ = entry.function; param_count_ = entry.params.size();
        std::copy(entry.params.begin(), entry.params.end(), params_.begin());
    } else {
        target_identity_.capture(object);
        const auto& entry = resolve_call(object, name);
        function_ = entry.function; function_identity_ = entry.function_guard; param_count_ = entry.params.size();
        std::copy(entry.params.begin(), entry.params.end(), params_.begin());
    }
    if (function_->GetNumParms() != count || param_count_ != count)
        throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
    if (cache == Cache::SerialInitializer) validate_serial_initializer(*this);
    try {
        for (; initialized_ < param_count_; ++initialized_) {
            auto* p = params_[initialized_]; p->InitializeValue(bytes_.data() + p->GetOffset_Internal());
        }
    } catch (...) {
        while (initialized_) { auto* p = params_[--initialized_]; p->DestroyValue(bytes_.data() + p->GetOffset_Internal()); }
        throw;
    }
}

Call::~Call() {
    while (initialized_) { auto* p = params_[--initialized_]; p->DestroyValue(bytes_.data() + p->GetOffset_Internal()); }
}

FProperty* Call::param(const wchar_t* name) {
    for (size_t i = 0; i < param_count_; ++i) if (params_[i]->GetName() == name) return params_[i];
    throw std::runtime_error("Missing parameter: " + narrow(name));
}

void Call::run() {
    if ((target_identity_.ptr && target_identity_.get() != object_) ||
        (function_identity_.ptr && function_identity_.get() != function_))
        throw std::runtime_error("Reflected call identity changed before execution");
    object_->ProcessEvent(function_, bytes_.data());
}

void Call::set_bool(const wchar_t* name, bool value) {
    auto* p = param(name);
    if (!p->IsA<FBoolProperty>() || static_cast<FBoolProperty*>(p)->GetByteOffset() >= p->GetElementSize())
        throw std::runtime_error("Boolean parameter layout mismatch: " + narrow(name));
    static_cast<FBoolProperty*>(p)->SetPropertyValueInContainer(bytes_.data(), value);
}

bool Call::get_bool(const wchar_t* name) {
    auto* p = param(name);
    if (!p->IsA<FBoolProperty>() || static_cast<FBoolProperty*>(p)->GetByteOffset() >= p->GetElementSize())
        throw std::runtime_error("Boolean return layout mismatch: " + narrow(name));
    return static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(bytes_.data());
}

namespace {
std::unordered_map<std::string, OwnerGuard> s_asset_cache;
}

UObject* load(const std::string& path) {
    if (auto it = s_asset_cache.find(path); it != s_asset_cache.end()) {
        if (it->second.alive()) return static_cast<UObject*>(const_cast<void*>(it->second.ptr));
    }
    auto name = wide(path);
    if (auto* object = find_optional(name.c_str())) {
        s_asset_cache[path].capture(object);
        return object;
    }
    auto* kismet = find_cached(L"/Script/Engine.Default__KismetSystemLibrary");
    Call make(kismet, L"MakeSoftObjectPath", 2);
    FString string(name.c_str());
    make.set(L"PathString", string);
    make.run();

    Call convert(kismet, L"Conv_SoftObjPathToSoftObjRef", 2);
    auto* soft_path_param = convert.param(L"SoftObjectPath");
    soft_path_param->CopyCompleteValue(convert.data(soft_path_param), make.data(make.param(L"ReturnValue")));
    convert.run();

    Call loading(kismet, L"LoadAsset_Blocking", 2);
    auto* asset_param = loading.param(L"Asset");
    asset_param->CopyCompleteValue(loading.data(asset_param), convert.data(convert.param(L"ReturnValue")));
    loading.run();

    auto* object = loading.get<UObject*>();
    if (!object) throw std::runtime_error("Asset could not load: " + path);
    s_asset_cache[path].capture(object);
    return object;
}

void invoke(UObject* object, const wchar_t* fn) {
    Call c(object, fn, 0);
    c.run();
}

UObject* construct_class(UClass* cls, UObject* outer) {
    if (!cls) throw std::runtime_error("Widget class is null");
    FStaticConstructObjectParameters params(cls, outer);
    auto* object = UObjectGlobals::StaticConstructObject(params);
    if (!object) throw std::runtime_error("Could not construct object");
    return object;
}

UObject* construct(const wchar_t* type, UObject* outer) {
    return construct_class(static_cast<UClass*>(find_cached(type)), outer);
}

void object_property(UObject* object, const wchar_t* name, UObject* value) {
    auto* p = field(object, name, sizeof(UObject*));
    if (!p->IsA<FObjectProperty>()) throw std::runtime_error("Property is not an object reference");
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(object) + p->GetOffset_Internal(), &value);
}

void copy_property(UObject* to, UObject* from, const wchar_t* name) {
    auto* p = to->GetPropertyByNameInChain(name);
    auto* q = from->GetPropertyByNameInChain(name);
    if (!p || !q || !p->SameType(q)) throw std::runtime_error("Style property mismatch: " + narrow(name));
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(to) + p->GetOffset_Internal(),
                         reinterpret_cast<std::byte*>(from) + q->GetOffset_Internal());
}

void text_property(UObject* object, const wchar_t* name, const std::string& value) {
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_StringToText", 2);
    convert.set(L"InString", FString(wide(value).c_str()));
    convert.run();
    auto* p = object->GetPropertyByNameInChain(name);
    auto* result = convert.param(L"ReturnValue");
    if (!p || !p->SameType(result)) throw std::runtime_error("Text property mismatch: " + narrow(name));
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(object) + p->GetOffset_Internal(), convert.data(result));
}

void text_value(UObject* widget, const std::string& text) {
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_StringToText", 2);
    if (!convert.param(L"InString")->IsA<FStrProperty>() || !convert.param(L"ReturnValue")->IsA<FTextProperty>())
        throw std::runtime_error("Native text conversion types changed");
    FString value(wide(text).c_str());
    convert.set(L"InString", value);
    convert.run();
    Call set(widget, L"SetText", 1);
    auto* param = set.param(L"InText");
    if (!param->SameType(convert.param(L"ReturnValue"))) throw std::runtime_error("Native text setter type changed");
    param->CopyCompleteValue(set.data(param), convert.data(convert.param(L"ReturnValue")));
    set.run();
}

std::string text_of(UObject* widget, int limit) {
    if (!widget || limit < 1 || limit > 4096) throw std::runtime_error("Invalid text input request");
    Call text(widget, L"GetText", 1);
    if (!text.param(L"ReturnValue")->IsA<FTextProperty>()) throw std::runtime_error("Native text getter type changed");
    text.run();
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_TextToString", 2);
    auto* param = convert.param(L"InText");
    if (!param->SameType(text.param(L"ReturnValue"))) throw std::runtime_error("Native text input type changed");
    auto* result = convert.param(L"ReturnValue");
    if (!result->IsA<FStrProperty>() || result->GetElementSize() != sizeof(FString))
        throw std::runtime_error("Native text string layout changed");
    param->CopyCompleteValue(convert.data(param), text.data(text.param(L"ReturnValue")));
    convert.run();
    const auto& value = *static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars = value.GetCharArray();
    if (chars.Num() < 0 || chars.Num() > limit || (chars.Num() && (!chars.GetData() || chars.GetData()[chars.Num() - 1] != L'\0')))
        throw std::runtime_error("Text is too long or invalid");
    return chars.Num() ? narrow(std::wstring(chars.GetData(), static_cast<size_t>(chars.Num() - 1))) : std::string{};
}

std::string text_property_string(UObject* object, const wchar_t* name, int limit) {
    if (!object || limit < 1 || limit > 4096) throw std::runtime_error("Invalid text property request");
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_TextToString", 2);
    auto* input = convert.param(L"InText");
    auto* result = convert.param(L"ReturnValue");
    if (!input->IsA<FTextProperty>() || !result->IsA<FStrProperty>() || result->GetElementSize() != sizeof(FString))
        throw std::runtime_error("Native text conversion types changed");
    auto* property = field(object, name, static_cast<size_t>(input->GetElementSize()));
    if (!property->SameType(input)) throw std::runtime_error("Text property type mismatch");
    input->CopyCompleteValue(convert.data(input), reinterpret_cast<const std::byte*>(object) + property->GetOffset_Internal());
    convert.run();
    const auto& value = *static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars = value.GetCharArray();
    if (chars.Num() < 0 || chars.Num() > limit || (chars.Num() && !chars.GetData()))
        throw std::runtime_error("Text property exceeds bound");
    if (!chars.Num()) return {};
    if (chars.GetData()[chars.Num() - 1] != L'\0') throw std::runtime_error("Text property is not terminated");
    return narrow(std::wstring(chars.GetData(), static_cast<size_t>(chars.Num() - 1)));
}

void font_size(UObject* widget, float size, UObject* font_object) {
    if (!std::isfinite(size) || size <= 0 || size > 512) throw std::runtime_error("Native font size exceeds rendering bound");
    auto* info = find_cached(L"/Script/SlateCore.SlateFontInfo");
    auto* size_field = field(info, L"Size", sizeof(float));
    if (!size_field->IsA<FNumericProperty>() || !static_cast<FNumericProperty*>(size_field)->IsFloatingPoint())
        throw std::runtime_error("Native font size type changed");
    Call set(widget, L"SetFont", 1);
    auto* param = set.param(L"InFontInfo");
    if (!param->IsA<FStructProperty>() || static_cast<FStructProperty*>(param)->GetStruct().Get() != info)
        throw std::runtime_error("Native font struct type changed");
    auto* font = field(widget, L"Font", param->GetElementSize());
    if (!font->SameType(param) || size_field->GetOffset_Internal() + sizeof(float) > static_cast<size_t>(param->GetElementSize()))
        throw std::runtime_error("Font layout mismatch");
    param->CopyCompleteValue(set.data(param), reinterpret_cast<std::byte*>(widget) + font->GetOffset_Internal());
    std::memcpy(static_cast<std::byte*>(set.data(param)) + size_field->GetOffset_Internal(), &size, sizeof(size));
    if (font_object) {
        auto* obj_field = field(info, L"FontObject", sizeof(UObject*));
        if (!obj_field->IsA<FObjectProperty>() || obj_field->GetOffset_Internal() + sizeof(UObject*) > static_cast<size_t>(param->GetElementSize()))
            throw std::runtime_error("Native font object layout changed");
        obj_field->CopyCompleteValue(static_cast<std::byte*>(set.data(param)) + obj_field->GetOffset_Internal(), &font_object);
    }
    set.run();
}

void font_style(UObject* widget, UObject* source, const wchar_t* name, float size) {
    if (!std::isfinite(size) || size <= 0 || size > 512) throw std::runtime_error("Native font size exceeds rendering bound");
    Call set(widget, L"SetFont", 1);
    auto* input = set.param(L"InFontInfo");
    auto* info = find_cached(L"/Script/SlateCore.SlateFontInfo");
    if (!input->IsA<FStructProperty>() || static_cast<FStructProperty*>(input)->GetStruct().Get() != info)
        throw std::runtime_error("Native font struct type changed");
    auto* original = field(source, name, input->GetElementSize());
    if (!original->SameType(input)) throw std::runtime_error("Native font style type changed");
    input->CopyCompleteValue(set.data(input), reinterpret_cast<const std::byte*>(source) + original->GetOffset_Internal());
    auto* size_field = field(info, L"Size", sizeof(float));
    if (!size_field->IsA<FNumericProperty>() || !static_cast<FNumericProperty*>(size_field)->IsFloatingPoint())
        throw std::runtime_error("Native font size type changed");
    if (size_field->GetOffset_Internal() + sizeof(float) > static_cast<size_t>(input->GetElementSize()))
        throw std::runtime_error("Native font size exceeds style");
    std::memcpy(static_cast<std::byte*>(set.data(input)) + size_field->GetOffset_Internal(), &size, sizeof(size));
    set.run();
}

UObject* create_widget(UObject* pc, UClass* type) {
    Call c(find_cached(L"/Script/UMG.Default__WidgetBlueprintLibrary"), L"Create", 4);
    c.set(L"WorldContextObject", pc);
    c.set(L"WidgetType", type);
    c.set(L"OwningPlayer", pc);
    c.run();
    auto* widget = c.get<UObject*>();
    if (!widget) throw std::runtime_error("Widget creation failed");
    return widget;
}

std::vector<UObject*> children(UObject* panel) {
    Call count(panel, L"GetChildrenCount", 1);
    count.run();
    auto n = count.get<int32_t>();
    if (n < 0 || n > 512) throw std::runtime_error("Panel child count exceeds bound");
    std::vector<UObject*> result;
    for (int i = 0; i < n; ++i) {
        Call get(panel, L"GetChildAt", 2);
        get.set(L"Index", i);
        get.run();
        result.push_back(get.get<UObject*>());
    }
    return result;
}

void reorder(UObject* panel, const std::vector<UObject*>& order) {
    if (!panel || order.empty() || order.size() > 64) throw std::runtime_error("Invalid panel ordering request");
    const auto original = children(panel);
    if (original == order) return;
    if (original.size() != order.size()) throw std::runtime_error("Panel ordering must retain every child");
    for (auto* child : order) {
        if (!child || std::count(order.begin(), order.end(), child) != 1 || std::count(original.begin(), original.end(), child) != 1)
            throw std::runtime_error("Panel ordering contains missing or duplicate children");
    }
    struct Layout {
        UObject* child;
        Margin padding;
        std::array<std::byte, 8> size;
        uint8_t horizontal, vertical;
    };
    auto read = []<typename T>(UObject* object, const wchar_t* name) {
        auto* p = field(object, name, sizeof(T));
        T value{};
        std::memcpy(&value, reinterpret_cast<const std::byte*>(object) + p->GetOffset_Internal(), sizeof(T));
        return value;
    };
    std::vector<Layout> layouts;
    layouts.reserve(order.size());
    for (auto* child : original) {
        ObjectHandle live;
        live.capture(child);
        auto* slot = object_of(child, L"Slot");
        if (!slot || slot->GetClassPrivate()->GetName() != L"HorizontalBoxSlot") continue;
        layouts.push_back({child, read.operator()<Margin>(slot, L"Padding"), read.operator()<std::array<std::byte, 8>>(slot, L"Size"),
            read.operator()<uint8_t>(slot, L"HorizontalAlignment"), read.operator()<uint8_t>(slot, L"VerticalAlignment")});
        // Resolve setter contracts before releasing the panel's references.
        Call padding(slot, L"SetPadding", 1); padding.set(L"InPadding", layouts.back().padding);
        Call size(slot, L"SetSize", 1); size.set(L"InSize", layouts.back().size);
        Call horizontal(slot, L"SetHorizontalAlignment", 1); horizontal.set(L"InHorizontalAlignment", layouts.back().horizontal);
        Call vertical(slot, L"SetVerticalAlignment", 1); vertical.set(L"InVerticalAlignment", layouts.back().vertical);
    }
    Call clear(panel, L"ClearChildren", 0);
    Call add_contract(panel, L"AddChild", 2);
    add_contract.set(L"content", order.front());
    struct Roots {
        std::vector<UObject*> objects;
        explicit Roots(const std::vector<UObject*>& children) {
            objects.reserve(children.size());
            for (auto* child : children) if (!child->IsRootSet()) {
                objects.push_back(child);
                child->SetRootSet();
            }
        }
        ~Roots() { for (auto* object : objects) object->ClearRootSet(); }
    } roots(original);
    auto apply = [&](const std::vector<UObject*>& desired) {
        clear.run();
        for (auto* child : desired) {
            Call add(panel, L"AddChild", 2); add.set(L"content", child); add.run();
            if (!add.get<UObject*>()) throw std::runtime_error("Panel rejected a child during ordering");
        }
        for (const auto& layout : layouts) {
            auto* slot = object_of(layout.child, L"Slot");
            if (!slot) throw std::runtime_error("Panel lost a child slot during ordering");
            invoke(slot, L"SetPadding", L"InPadding", layout.padding);
            invoke(slot, L"SetSize", L"InSize", layout.size);
            invoke(slot, L"SetHorizontalAlignment", L"InHorizontalAlignment", layout.horizontal);
            invoke(slot, L"SetVerticalAlignment", L"InVerticalAlignment", layout.vertical);
        }
        if (children(panel) != desired) throw std::runtime_error("Panel ordering did not apply");
    };
    try { apply(order); }
    catch (...) {
        try { apply(original); } catch (...) {}
        throw;
    }
}

void nav_children_refresh(UObject* panel) {
    if (auto* nav = object_of(panel, L"NavigationObject")) invoke(nav, L"GetNavigableChildren");
}

PlayerContext player_context(void* engine) {
    PlayerContext out;
    if (!engine) return out;
    auto* viewport = object_of(static_cast<UObject*>(engine), L"GameViewport");
    auto* world = object_of(viewport, L"World");
    if (!world) return out;
    out.world = world;
    Call pc(find_cached(L"/Script/Engine.Default__GameplayStatics"), L"GetPlayerController", 3);
    pc.set(L"WorldContextObject", world);
    pc.set(L"PlayerIndex", int32_t{0});
    pc.run();
    out.pc = pc.get<UObject*>();
    ObjectHandle controller;
    controller.capture(out.pc);
    out.pc = controller.get();
    out.pawn = object_of(out.pc, L"Pawn");
    out.asc = object_of(out.pawn, L"AbilitySystemComponent");
    return out;
}

} // namespace ccs::engine

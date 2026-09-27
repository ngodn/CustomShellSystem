#include "engine.hpp"
#include <windows.h>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>

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
struct OwnerGuard {
    const void* ptr{nullptr};
    int32_t index{-1};
    int32_t serial{0};
    void capture(UObject* o) {
        ptr = o;
        index = o->GetInternalIndex();
        auto* it = FUObjectArray::IndexToObject(index);
        serial = it ? it->GetSerialNumber() : 0;
    }
    bool alive() const {
        if (!ptr || index < 0) return false;
        auto* it = FUObjectArray::IndexToObject(index);
        return it && it->GetUObject() == ptr && it->GetSerialNumber() == serial;
    }
};
struct FieldEntry { OwnerGuard owner; FProperty* property{nullptr}; };
std::unordered_map<ReflKey, FieldEntry, ReflHash, ReflEq> g_field_cache;
struct CallEntry { OwnerGuard owner; UFunction* function{nullptr}; std::vector<FProperty*> params; };
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
    UObject* owner = object->GetClassPrivate();
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
    return property;
}

UObject* object_of(UObject* object, const wchar_t* name) {
    auto* p = resolve_field(object, name);
    if (!p || !p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*)) return nullptr;
    UObject* val = nullptr;
    std::memcpy(&val, reinterpret_cast<const std::byte*>(object) + p->GetOffset_Internal(), sizeof(UObject*));
    return val;
}

bool bool_of(UObject* object, const wchar_t* name, bool fallback) {
    auto* p = resolve_field(object, name);
    if (!p || !p->IsA<FBoolProperty>()) return fallback;
    return static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(object);
}

WeakObject::WeakObject(UObject* object) { *this = object; }
WeakObject& WeakObject::operator=(UObject* object) {
    if (object) {
        auto* item = FUObjectArray::IndexToObject(object->GetInternalIndex());
        if (!item || item->GetUObject() != object) throw std::runtime_error("Object is absent from the live object array");
        if (!item->GetSerialNumber()) {
            Call serial(find(L"/Script/Engine.Default__KismetSystemLibrary"), L"Conv_ObjectToSoftObjectReference", 2);
            serial.set(L"Object", object);
            serial.run();
            if (!item->GetSerialNumber()) throw std::runtime_error("Engine did not initialize the object serial");
        }
    }
    FWeakObjectPtr::operator=(object);
    return *this;
}

static const CallEntry& resolve_call(UObject* object, const wchar_t* name) {
    UObject* owner = object->GetClassPrivate();
    if (owner) {
        if (auto it = g_call_cache.find(ReflView{owner, name}); it != g_call_cache.end() && it->second.owner.alive())
            return it->second;
    }
    CallEntry entry;
    entry.function = object->GetFunctionByNameInChain(name);
    if (!entry.function || entry.function->GetParmsSize() > 2048)
        throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
    for (auto* p : entry.function->ForEachProperty()) {
        if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
        if (p->GetOffset_Internal() < 0 || p->GetArrayDim() != 1 || p->GetOffset_Internal() + p->GetElementSize() > entry.function->GetParmsSize())
            throw std::runtime_error("Parameter exceeds reflected frame: " + narrow(name));
        entry.params.push_back(p);
    }
    if (!owner) {
        static thread_local CallEntry scratch;
        scratch = std::move(entry);
        return scratch;
    }
    entry.owner.capture(owner);
    return g_call_cache.insert_or_assign(ReflKey{owner, name}, std::move(entry)).first->second;
}

Call::Call(UObject* object, const wchar_t* name, unsigned count) : object_(object) {
    if (!object) throw std::runtime_error("No target for reflected call " + narrow(name));
    const CallEntry& entry = resolve_call(object, name);
    if (entry.function->GetNumParms() != count || entry.params.size() != count)
        throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
    function_ = entry.function;
    params_ = &entry.params;
    for (auto* p : *params_) p->InitializeValue(bytes_.data() + p->GetOffset_Internal());
}

Call::~Call() {
    if (params_) for (auto* p : *params_) p->DestroyValue(bytes_.data() + p->GetOffset_Internal());
}

FProperty* Call::param(const wchar_t* name) {
    for (auto* p : *params_) if (p->GetName() == name) return p;
    throw std::runtime_error("Missing parameter: " + narrow(name));
}

void Call::run() {
    object_->ProcessEvent(function_, bytes_.data());
}

namespace {
std::unordered_map<std::string, WeakObject> s_asset_cache;
}

UObject* load(const std::string& path) {
    if (auto it = s_asset_cache.find(path); it != s_asset_cache.end()) {
        if (auto* cached = it->second.Get()) return cached;
    }
    auto name = wide(path);
    if (auto* object = find_optional(name.c_str())) {
        s_asset_cache[path] = object;
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
    s_asset_cache[path] = object;
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
    FString value(wide(text).c_str());
    convert.set(L"InString", value);
    convert.run();
    Call set(widget, L"SetText", 1);
    auto* param = set.param(L"InText");
    param->CopyCompleteValue(set.data(param), convert.data(convert.param(L"ReturnValue")));
    set.run();
}

std::string text_of(UObject* widget, int limit) {
    if (!widget) return {};
    Call text(widget, L"GetText", 1);
    text.run();
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_TextToString", 2);
    auto* param = convert.param(L"InText");
    param->CopyCompleteValue(convert.data(param), text.data(text.param(L"ReturnValue")));
    convert.run();
    const auto& value = *static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars = value.GetCharArray();
    if (chars.Num() > limit) throw std::runtime_error("Text is too long");
    return chars.Num() ? narrow(std::wstring(chars.GetData())) : std::string{};
}

void font_size(UObject* widget, float size, UObject* font_object) {
    auto* font = widget->GetPropertyByNameInChain(L"Font");
    auto* info = find_cached(L"/Script/SlateCore.SlateFontInfo");
    auto* size_field = field(info, L"Size", sizeof(float));
    Call set(widget, L"SetFont", 1);
    auto* param = set.param(L"InFontInfo");
    if (!font || !font->SameType(param)) throw std::runtime_error("Font layout mismatch");
    param->CopyCompleteValue(set.data(param), reinterpret_cast<std::byte*>(widget) + font->GetOffset_Internal());
    std::memcpy(static_cast<std::byte*>(set.data(param)) + size_field->GetOffset_Internal(), &size, sizeof(size));
    if (font_object) {
        auto* obj_field = field(info, L"FontObject", sizeof(UObject*));
        obj_field->CopyCompleteValue(static_cast<std::byte*>(set.data(param)) + obj_field->GetOffset_Internal(), &font_object);
    }
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
    auto* slots = panel->GetPropertyByNameInChain(L"Slots");
    if (!slots || !slots->IsA<FArrayProperty>()) throw std::runtime_error("Panel has no Slots array");
    auto* helper = reinterpret_cast<FScriptArray*>(reinterpret_cast<std::byte*>(panel) + slots->GetOffset_Internal());
    if (!helper || helper->Num() != static_cast<int32_t>(order.size())) return;
}

void nav_children_refresh(UObject* panel) {
    invoke(panel, L"ForceLayoutPrepass");
}

} // namespace ccs::engine

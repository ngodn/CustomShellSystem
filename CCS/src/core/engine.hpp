#pragma once
#include "ccs_types.hpp"
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <Unreal/UObject.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/FWeakObjectPtr.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/NameTypes.hpp>

namespace ccs::engine {
using namespace RC::Unreal;

struct PlayerContext {
    UObject* pc{nullptr};         // APlayerController*
    UObject* pawn{nullptr};       // ASpartaCharacter*
    UObject* weapon{nullptr};     // ASpartaWeapon*
    UObject* asc{nullptr};        // USpartaAbilitySystemComponent*
};

std::wstring wide(const std::string& str);
std::string narrow(const std::wstring& wide);

UObject* find(const wchar_t* path);
UObject* find_optional(const wchar_t* path);
UObject* load(const std::string& path);

FProperty* field(UObject* object, const wchar_t* name, size_t size);
UObject* object_of(UObject* object, const wchar_t* name);
bool bool_of(UObject* object, const wchar_t* name, bool fallback = false);

class WeakObject : public FWeakObjectPtr {
public:
    WeakObject() = default;
    WeakObject(UObject* object);
    WeakObject& operator=(UObject* object);
    bool same(const WeakObject& other) const {
        return ObjectIndex == other.ObjectIndex && ObjectSerialNumber == other.ObjectSerialNumber;
    }
};

class Call {
    UObject* object_{nullptr};
    UFunction* function_{nullptr};
    alignas(16) std::array<std::byte, 2048> bytes_{};
    const std::vector<FProperty*>* params_{nullptr};

public:
    Call(UObject* object, const wchar_t* name, unsigned count = 0);
    ~Call();
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;

    FProperty* param(const wchar_t* name);
    void* data(FProperty* p) { return bytes_.data() + p->GetOffset_Internal(); }

    template<typename T>
    void set(const wchar_t* name, const T& value) {
        auto* p = param(name);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Parameter size mismatch: " + narrow(name));
        p->CopyCompleteValue(data(p), &value);
    }

    template<typename T>
    T get(const wchar_t* name = L"ReturnValue") {
        auto* p = param(name);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Return size mismatch: " + narrow(name));
        T value{};
        std::memcpy(&value, data(p), sizeof(T));
        return value;
    }

    void run();
    UFunction* function() const { return function_; }
};

struct Vec2 { double x{0}, y{0}; };
struct Color { float r{1}, g{1}, b{1}, a{1}; };
struct SlateColor { Color color; uint8_t rule{0}; uint8_t padding[3]{}; };
struct Margin { float left{0}, top{0}, right{0}, bottom{0}; };

void invoke(UObject* object, const wchar_t* fn);
template<class T>
void invoke(UObject* object, const wchar_t* fn, const wchar_t* param, const T& value) {
    Call c(object, fn, 1);
    c.set(param, value);
    c.run();
}

UObject* construct(const wchar_t* type, UObject* outer);
UObject* construct_class(UClass* type, UObject* outer);
void object_property(UObject* object, const wchar_t* name, UObject* value);
void copy_property(UObject* dest, UObject* src, const wchar_t* name);
void text_property(UObject* object, const wchar_t* name, const std::string& text);
void text_value(UObject* widget, const std::string& text);
std::string text_of(UObject* widget, int limit = 256);
void font_size(UObject* widget, float size, UObject* font_object = nullptr);

UObject* create_widget(UObject* owning_object, UClass* widget_class);
std::vector<UObject*> children(UObject* panel);
void reorder(UObject* panel, const std::vector<UObject*>& order);
void nav_children_refresh(UObject* panel);

} // namespace ccs::engine

#pragma once
#include "ccs_types.hpp"
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <type_traits>
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
    UObject* world{nullptr};
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
PlayerContext player_context(void* engine);

FProperty* field(UObject* object, const wchar_t* name, size_t size);
UObject* object_of(UObject* object, const wchar_t* name);
// Cache misses return null without resolving fields or initializing object serials.
UObject* cached_object_of(UObject* object, const wchar_t* name);
bool bool_of(UObject* object, const wchar_t* name, bool fallback = false);

struct ObjectHandle {
    const void* ptr{nullptr};
    int32_t index{-1};
    int32_t serial{0};
    FName name{};
    // Cold capture may call the engine to initialize a weak serial. Hooks use capture_existing.
    void capture(UObject* object);
    bool capture_existing(UObject* object);
    UObject* get() const;
    bool alive() const { return get() != nullptr; }
};

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
    std::array<FProperty*, 64> params_{};
    size_t param_count_{}, initialized_{};
    ObjectHandle target_identity_, function_identity_;

public:
    enum class Cache { Use, SerialInitializer };
    Call(UObject* object, const wchar_t* name, unsigned count = 0, Cache cache = Cache::Use);
    ~Call();
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;

    FProperty* param(const wchar_t* name);
    void set_bool(const wchar_t* name, bool value);
    bool get_bool(const wchar_t* name);
    void* data(FProperty* p) { return bytes_.data() + p->GetOffset_Internal(); }

    template<typename T>
    void set(const wchar_t* name, const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            set_bool(name, value);
        } else {
            auto* p = param(name);
            if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Parameter size mismatch: " + narrow(name));
            p->CopyCompleteValue(data(p), &value);
        }
    }

    template<typename T>
    T get(const wchar_t* name = L"ReturnValue") {
        static_assert(std::is_trivially_copyable_v<T>, "Read owning reflected values in-place or with CopyCompleteValue");
        if constexpr (std::is_same_v<T, bool>) {
            return get_bool(name);
        } else {
            auto* p = param(name);
            if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Return size mismatch: " + narrow(name));
            T value{};
            std::memcpy(&value, data(p), sizeof(T));
            return value;
        }
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
std::string text_property_string(UObject* object, const wchar_t* name, int limit = 256);
void font_size(UObject* widget, float size, UObject* font_object = nullptr);
void font_style(UObject* widget, UObject* source, const wchar_t* property, float size);

UObject* create_widget(UObject* owning_object, UClass* widget_class);
std::vector<UObject*> children(UObject* panel);
void reorder(UObject* panel, const std::vector<UObject*>& order);
void nav_children_refresh(UObject* panel);

} // namespace ccs::engine

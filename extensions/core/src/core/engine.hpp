#pragma once
// Reflection and UMG helpers for the core. Every function here runs on the
// game thread. Layouts, offsets and parameter frames come from live reflection
// at each call; nothing assumes a header struct layout for game objects.
#include "common.hpp"
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <Unreal/UObject.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/FWeakObjectPtr.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/NameTypes.hpp>

namespace cssx::engine {
using namespace RC::Unreal;

std::wstring wide(const std::string&);
std::string narrow(const std::wstring&);
// Existing object by full path, or throw.
UObject* find(const wchar_t* path);
UObject* find_optional(const wchar_t* path);
// find() for permanent native objects (class defaults, script structs) used on hot paths.
// Revalidated read-only on every use (object-array index + serial, like the field cache);
// never builds a WeakObject, so it cannot repeat the path-cache crash. Game thread only.
UObject* find_cached(const wchar_t* path);
void set_find_cache(bool enabled);   // kept for the core switch log; no cache exists
// Load an asset by path (blocking on first use), cached by weak handle.
UObject* load(const std::string& path);
FProperty* field(UObject* object,const wchar_t* name,size_t size);
template<typename T> T read(UObject* object,const wchar_t* name) {
    auto* p=field(object,name,sizeof(T));
    T value{}; std::memcpy(&value,reinterpret_cast<const std::byte*>(object)+p->GetOffset_Internal(),sizeof(T));
    return value;
}
// Object property read that returns null instead of throwing when the
// property is absent or not an object reference.
UObject* object_of(UObject* object,const wchar_t* name);
bool bool_of(UObject* object,const wchar_t* name,bool fallback=false);

// UE4SS's serial-allocation fallback uses a legacy soft-reference layout.
// Initialise new serials through a reflected frame before constructing a weak handle.
class WeakObject : public FWeakObjectPtr {
public:
    WeakObject()=default;
    WeakObject(UObject* object);
    WeakObject& operator=(UObject* object);
    bool same(const WeakObject& other) const { return ObjectIndex==other.ObjectIndex && ObjectSerialNumber==other.ObjectSerialNumber; }
};

// A reflected function call frame. Parameter offsets come from the UFunction.
class Call {
    UObject* object_;
    UFunction* function_=nullptr;
    alignas(16) std::array<std::byte,2048> bytes_{};
    const std::vector<FProperty*>* params_=nullptr;   // owned by the reflected-call cache, stable across rehash
public:
    Call(UObject* object,const wchar_t* name,unsigned count);
    ~Call();
    Call(const Call&)=delete;
    Call& operator=(const Call&)=delete;
    FProperty* param(const wchar_t* name);
    void* data(FProperty* p) { return bytes_.data()+p->GetOffset_Internal(); }
    template<typename T> void set(const wchar_t* name,const T& value) {
        auto* p=param(name);
        if(p->GetElementSize()!=sizeof(T)) throw std::runtime_error("Parameter size mismatch: "+narrow(name));
        p->CopyCompleteValue(data(p),&value);
    }
    void copy(const wchar_t* name,Call& other,const wchar_t* other_name);
    template<typename T> T get(const wchar_t* name=L"ReturnValue") {
        auto* p=param(name);
        if(p->GetElementSize()!=sizeof(T)) throw std::runtime_error("Return size mismatch: "+narrow(name));
        T value{}; std::memcpy(&value,data(p),sizeof(T)); return value;
    }
    void run();
    UFunction* function() const { return function_; }
};

struct Vec2 { double x,y; };
struct Color { float r,g,b,a; };
struct SlateColor { Color color; uint8_t rule=0; uint8_t padding[3]{}; };
struct Margin { float left,top,right,bottom; };
struct UVRect { float min_x=0,min_y=0,max_x=1,max_y=1; uint8_t valid=1; uint8_t padding[3]{}; };
struct Anchors { Vec2 minimum,maximum; };

void invoke(UObject* object,const wchar_t* fn);
template<class T> void invoke(UObject* object,const wchar_t* fn,const wchar_t* param,const T& value) {
    Call c(object,fn,1); c.set(param,value); c.run();
}
UObject* construct(const wchar_t* type,UObject* outer);
UObject* construct_class(UClass* type,UObject* outer);
void object_property(UObject* object,const wchar_t* name,UObject* value);
template<class T> void member(void* data,size_t bytes,UObject* structure,const wchar_t* name,const T& value) {
    auto* p=field(structure,name,sizeof(T));
    if(p->GetOffset_Internal()<0 || static_cast<size_t>(p->GetOffset_Internal())+sizeof(T)>bytes)
        throw std::runtime_error("Struct member exceeds reflected size: "+narrow(name));
    p->CopyCompleteValue(static_cast<std::byte*>(data)+p->GetOffset_Internal(),&value);
}
template<class T> void raw_value(UObject* object,const wchar_t* name,const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto* p=field(object,name,sizeof(T));
    std::memcpy(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal(),&value,sizeof(T));
}
void text_value(UObject* widget,const std::string& text);
std::string text_of(UObject* widget,int limit=256);
void font_size(UObject* widget,float size,UObject* font_object=nullptr);
void flat_button(UObject* widget,bool active);
UObject* content(UObject* parent,UObject* child);
std::vector<UObject*> children(UObject* panel,int limit=256);
UObject* create_widget(UObject* pc,UClass* type);   // WidgetBlueprintLibrary.Create

// World/player resolution through the engine, never by class search.
struct PlayerContext { UObject* world=nullptr; UObject* pc=nullptr; UObject* pawn=nullptr; };
PlayerContext player_context(void* engine);

// Coordinates in a 1920x1080 reference space scaled to the live viewport.
struct Layout {
    UObject* tree; UObject* canvas; double scale; UObject* serif; UObject* title_font;
    double origin_x=0, origin_y=0;
    struct Placed { UObject* widget; UObject* canvas; double x,y,w,h; };
    std::vector<Placed> placed{};
    std::vector<UObject*> on(UObject* target) const;
    double extent_of(UObject* target) const;
    UObject* place(UObject* widget,double x,double y,double w,double h);   // returns the canvas slot
    UObject* box(double x,double y,double w,double h,Color color);
    UObject* label(const std::string& text,double x,double y,double w,double h,float size,Color color,bool title=false,uint8_t justify=0);
    UObject* button(const std::string& text,double x,double y,double w,double h,bool active=false,bool enabled=true,float size=20,Color ink={1,1,1,1});
    UObject* image(UObject* texture,double x,double y,double w,double h,float opacity=1,UVRect uv={});
};
}

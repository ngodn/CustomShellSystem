#include "engine.hpp"
#include <windows.h>
#include <Xinput.h>
#include "wardrobe_input.hpp"
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace css {
using namespace RC::Unreal;
static std::string slider_text(float value,bool scalar) {
    if(!scalar) return std::to_string(int(std::lround(value*100)));
    std::ostringstream text; text<<std::fixed<<std::setprecision(2)<<value; return text.str();
}
static std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!size) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(size,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),size);
    return result;
}
static std::string narrow(const std::wstring& s) {
    if(s.empty()) return {};
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(!size) throw std::runtime_error("Invalid Unicode text");
    std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),size,nullptr,nullptr);
    return result;
}
static UObject* find(const wchar_t* path) {
    auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path);
    if (!object) throw std::runtime_error("Required reflected object is missing");
    return object;
}
static FProperty* field(UObject* object, const wchar_t* name, size_t size) {
    if (!object) throw std::runtime_error("No live object");
    auto* property = object->GetPropertyByNameInChain(name);
    if (!property || property->GetElementSize() != static_cast<int32_t>(size) || property->GetArrayDim() != 1)
        throw std::runtime_error("Reflected property layout does not match");
    return property;
}
template<typename T> static T read(UObject* object, const wchar_t* name) {
    auto* p = field(object, name, sizeof(T));
    T value{};
    std::memcpy(&value, reinterpret_cast<const std::byte*>(object) + p->GetOffset_Internal(), sizeof(T));
    return value;
}

// Parameter offsets and ownership come from live reflection. The frame never
// assumes the old UE4 FSoftObjectPath layout in the compatibility headers.
class Call {
    UObject* object_;
    UFunction* function_;
    alignas(16) std::array<std::byte, 2048> bytes_{};
    std::vector<FProperty*> properties_;
public:
    Call(UObject* object, const wchar_t* name, unsigned count) : object_(object) {
        if (!object) throw std::runtime_error("No target for reflected call");
        function_ = object->GetFunctionByNameInChain(name);
        if (!function_ || function_->GetNumParms() != count || function_->GetParmsSize() > bytes_.size())
            throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
        for (auto* p : function_->ForEachProperty()) {
            if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
            if (p->GetOffset_Internal() < 0 || p->GetArrayDim() != 1 ||
                p->GetOffset_Internal() + p->GetElementSize() > function_->GetParmsSize())
                throw std::runtime_error("Parameter exceeds reflected frame");
            properties_.push_back(p);
        }
        if (properties_.size() != count) throw std::runtime_error("Parameter enumeration mismatch");
        for (auto* p : properties_) p->InitializeValue(bytes_.data() + p->GetOffset_Internal());
    }
    ~Call() { for (auto* p : properties_) p->DestroyValue(bytes_.data() + p->GetOffset_Internal()); }
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;
    FProperty* param(const wchar_t* name) {
        for (auto* p : properties_) if (p->GetName() == name) return p;
        throw std::runtime_error("Missing parameter: " + narrow(name));
    }
    void* data(FProperty* p) { return bytes_.data() + p->GetOffset_Internal(); }
    template<typename T> void set(const wchar_t* name, const T& value) {
        auto* p = param(name);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Parameter size mismatch");
        p->CopyCompleteValue(data(p), &value);
    }
    void copy(const wchar_t* name, Call& other, const wchar_t* other_name) {
        auto* p = param(name); auto* q = other.param(other_name);
        if (p->GetElementSize() != q->GetElementSize() || !p->SameType(q))
            throw std::runtime_error("Parameter type mismatch");
        p->CopyCompleteValue(data(p), other.data(q));
    }
    template<typename T> T get(const wchar_t* name = L"ReturnValue") {
        auto* p = param(name);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Return size mismatch");
        T value{}; std::memcpy(&value, data(p), sizeof(T)); return value;
    }
    void run() { object_->ProcessEvent(function_, bytes_.data()); }
};
static UObject* load(const std::string& path) {
    auto name = wide(path);
    if (auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, name.c_str())) return object;
    auto* library = find(L"/Script/Engine.Default__KismetSystemLibrary");
    Call make(library, L"MakeSoftObjectPath", 2);
    FString string(name.c_str()); make.set(L"PathString", string); make.run();
    Call convert(library, L"Conv_SoftObjPathToSoftObjRef", 2);
    convert.copy(L"SoftObjectPath", make, L"ReturnValue"); convert.run();
    Call loading(library, L"LoadAsset_Blocking", 2);
    loading.copy(L"Asset", convert, L"ReturnValue"); loading.run();
    auto* object = loading.get<UObject*>();
    if (!object) throw std::runtime_error("Asset could not load: " + path);
    return object;
}
static UObject* mesh_asset(UObject* component) {
    Call call(component, L"GetSkeletalMeshAsset", 1); call.run(); return call.get<UObject*>();
}
static void set_mesh(UObject* component, UObject* mesh) {
    Call call(component, L"SetSkeletalMeshAsset", 1); call.set(L"NewMesh", mesh); call.run();
    if (mesh_asset(component) != mesh) throw std::runtime_error("Mesh read-back did not confirm replacement");
}
static FScriptArrayHelper overrides(UObject* component) {
    auto* p=component->GetPropertyByNameInChain(L"OverrideMaterials");
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Missing material overrides");
    auto* a=static_cast<FArrayProperty*>(p);
    if(a->GetInner()->GetElementSize()!=sizeof(UObject*)) throw std::runtime_error("Material array layout mismatch");
    FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(component)+a->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>128) throw std::runtime_error("Invalid material override count");
    return values;
}
static std::vector<std::string> material_paths(UObject* component) {
    auto values=overrides(component);
    std::vector<std::string> result;
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        auto path=value?narrow(value->GetPathName()):std::string{};
        // Transient effects cannot be restored by asset path after collection.
        // Refuse before mutation instead of pinning an old world through a MID.
        if(value && (!path.starts_with("/Game/") && !path.starts_with("/Engine/")))
            throw std::runtime_error("A temporary material effect is active. Let it finish before changing appearance.");
        result.push_back(std::move(path));
    }
    return result;
}
static void material(UObject* component,int index,UObject* value) {
    Call set(component,L"SetMaterial",2); set.set(L"ElementIndex",index); set.set(L"Material",value); set.run();
}
static void restore_materials(UObject* component,const std::vector<std::string>& paths) {
    std::vector<UObject*> loaded;
    for(const auto& path:paths) loaded.push_back(path.empty()?nullptr:load(path));
    auto count=std::max(static_cast<int>(paths.size()),overrides(component).Num());
    for(int i=0;i<count;++i) material(component,i,i<static_cast<int>(loaded.size())?loaded[i]:nullptr);
    if(material_paths(component)!=paths) throw std::runtime_error("Original material read-back failed");
}
static Json material_snapshot(UObject* component,UObject* mesh) {
    Json result={{"overrides",Json::array()},{"defaults",Json::array()},{"effective",Json::array()}};
    auto* overrides=component->GetPropertyByNameInChain(L"OverrideMaterials");
    if(!overrides || !overrides->IsA<FArrayProperty>()) throw std::runtime_error("Missing material array");
    auto* property=static_cast<FArrayProperty*>(overrides);
    if(property->GetInner()->GetElementSize()!=sizeof(UObject*)) throw std::runtime_error("Material array layout mismatch");
    FScriptArrayHelper array(property,reinterpret_cast<std::byte*>(component)+property->GetOffset_Internal());
    if(array.Num()<0 || array.Num()>512) throw std::runtime_error("Invalid material count");
    for(int i=0;i<array.Num();++i) {
        UObject* value{}; std::memcpy(&value,array.GetRawPtr(i),sizeof(value));
        result["overrides"].push_back(value?narrow(value->GetFullName()):"null");
    }
    Call defaults(mesh,L"GetMaterials",1); defaults.run();
    auto* p=defaults.param(L"ReturnValue");
    if(!p->IsA<FArrayProperty>()) throw std::runtime_error("Default materials are not an array");
    auto* a=static_cast<FArrayProperty*>(p);
    auto* slot=field(find(L"/Script/Engine.SkeletalMaterial"),L"MaterialInterface",sizeof(UObject*));
    if(slot->GetOffset_Internal()+sizeof(UObject*)>static_cast<size_t>(a->GetInner()->GetElementSize())) throw std::runtime_error("Material slot layout mismatch");
    FScriptArrayHelper values(a,defaults.data(p));
    if(values.Num()<0 || values.Num()>128) throw std::runtime_error("Invalid default material count");
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i)+slot->GetOffset_Internal(),sizeof(value));
        result["defaults"].push_back(value?narrow(value->GetFullName()):"null");
        Call actual(component,L"GetMaterial",2); actual.set(L"ElementIndex",i); actual.run();
        auto* current=actual.get<UObject*>();
        result["effective"].push_back(current?narrow(current->GetFullName()):"null");
    }
    return result;
}
UObject* Appearance::player(void* engine) {
    shell.clear(); pawn_name.clear(); current_mesh.clear();
    if (!Version::IsAtLeast(5, 6) || !Version::IsBelow(5, 7)) throw std::runtime_error("CSS adapter requires UE5.6");
    auto* viewport = read<UObject*>(static_cast<UObject*>(engine), L"GameViewport");
    if (!viewport) return nullptr;
    auto* world = read<UObject*>(viewport, L"World");
    if (!world) return nullptr;
    Call player_call(find(L"/Script/Engine.Default__GameplayStatics"), L"GetPlayerCharacter", 3);
    player_call.set(L"WorldContextObject", world); player_call.set(L"PlayerIndex", int32_t{0}); player_call.run();
    auto* pawn = player_call.get<UObject*>();
    if (!pawn || !pawn->GetPropertyByNameInChain(L"CharacterId")) return nullptr;
    // FGameplayTag contains the reflected FName TagName (8 bytes in this build).
    shell = narrow(read<FName>(pawn, L"CharacterId").ToString());
    if (!shell.starts_with("CharacterId.Player.")) { shell.clear(); return nullptr; }
    pawn_name = narrow(pawn->GetFullName());
    auto* component = read<UObject*>(pawn, L"Mesh");
    if (component) if (auto* mesh = mesh_asset(component)) current_mesh = narrow(mesh->GetPathName());
    return pawn;
}
bool Appearance::active() const { return component_.Get() && applied_.Get(); }
bool Appearance::apply(void* engine, const std::string& mesh_path, const std::map<int,std::string>& materials) {
    auto* pawn = player(engine);
    if (!pawn) return false;
    auto* component = read<UObject*>(pawn, L"Mesh");
    if (!component) return false;
    auto* before = mesh_asset(component);
    if (!before) return false;
    if (component_.Get() && component_.Get() != component && !restore()) return false;
    FWeakObjectPtr live_component(component), live_pawn(pawn), previous_mesh(before);
    auto* target = load(mesh_path);
    FWeakObjectPtr live_target(target);
    std::map<int,FWeakObjectPtr> loaded_materials;
    for(const auto& [slot,path]:materials) {
        auto* value=load(path);
        if(!value->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"))))
            throw std::runtime_error("Override asset is not a material");
        loaded_materials.emplace(slot,FWeakObjectPtr(value));
    }
    if(live_target.Get()!=target || std::any_of(loaded_materials.begin(),loaded_materials.end(),[](const auto& pair){return !pair.second.Get();}))
        throw std::runtime_error("Appearance assets changed during loading; request cancelled");
    if (live_component.Get() != component || live_pawn.Get() != pawn || previous_mesh.Get() != before || mesh_asset(component) != before)
        throw std::runtime_error("Player appearance changed during asset loading; request cancelled");
    auto* type = static_cast<UClass*>(find(L"/Script/Engine.SkeletalMesh"));
    if (!target->IsA(type)) throw std::runtime_error("Selected asset is not a skeletal mesh");
    if (read<UObject*>(before, L"Skeleton") != read<UObject*>(target, L"Skeleton"))
        throw std::runtime_error("Different skeleton: appearance change refused");
    if (before == target && applied_materials_==materials) return true;
    if (component_.Get() != component || before != applied_.Get()) {
        auto materials=material_paths(component);
        original_ = narrow(before->GetPathName());
        original_materials_=std::move(materials);
        component_ = component;
    }
    // Record the intended asset before calling the engine so a failed read-back
    // can still be rolled back. No gameplay or animation-class setters are used.
    applied_ = target;
    try {
        reset_colors();
        set_mesh(component, target);
        const int count=overrides(component).Num();
        for(int i=0;i<count;++i) material(component,i,nullptr);
        auto defaults=material_snapshot(component,target).at("defaults");
        for(const auto& [slot,value]:loaded_materials) {
            if(slot<0 || static_cast<size_t>(slot)>=defaults.size()) throw std::runtime_error("Material slot exceeds selected mesh");
            material(component,slot,value.Get());
            defaults[slot]=narrow(value.Get()->GetFullName());
        }
        material_debug=material_snapshot(component,target);
        if(material_debug["effective"]!=defaults)
            throw std::runtime_error("Outfit material read-back failed");
        applied_materials_=materials;
    }
    catch (...) { restore(); throw; }
    current_mesh = narrow(target->GetPathName());
    return true;
}
bool Appearance::restore() {
    auto* component = component_.Get();
    auto* applied = applied_.Get();
    if (component && applied && mesh_asset(component) == applied) {
        auto* original = load(original_);
        set_mesh(component, original);
        restore_materials(component,original_materials_);
        material_debug=material_snapshot(component,original);
    }
    color_mids_.clear(); color_targets_.clear(); color_textures_.clear(); last_colors_.clear(); color_outfit_.clear();
    component_.Reset(); applied_.Reset(); original_.clear(); original_materials_.clear(); applied_materials_.clear();
    return true;
}
}

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace css {
namespace {
struct Vec2 { double x, y; };
struct Color { float r, g, b, a; };
struct SlateColor { Color color; uint8_t rule = 0; uint8_t padding[3]{}; };
struct Margin { float left, top, right, bottom; };
struct UVRect { float min_x=0,min_y=0,max_x=1,max_y=1; uint8_t valid=1; uint8_t padding[3]{}; };
constexpr Color gold{0.69f, 0.49f, 0.25f, 1}, ivory{0.88f, 0.83f, 0.72f, 1};
constexpr Color muted{0.43f, 0.45f, 0.43f, 1};
void invoke(UObject* object, const wchar_t* fn) { Call c(object, fn, 0); c.run(); }
template<class T> void invoke(UObject* object, const wchar_t* fn, const wchar_t* param, const T& value) {
    Call c(object, fn, 1); c.set(param, value); c.run();
}
UObject* construct(const wchar_t* type, UObject* outer) {
    auto* cls = static_cast<UClass*>(find(type));
    FStaticConstructObjectParameters params(cls, outer);
    auto* object = UObjectGlobals::StaticConstructObject(params);
    if (!object) throw std::runtime_error("Could not construct wardrobe widget");
    return object;
}
void object_property(UObject* object, const wchar_t* name, UObject* value) {
    auto* p = field(object, name, sizeof(UObject*));
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(object) + p->GetOffset_Internal(), &value);
}
FBoolProperty* cursor_property(UObject* object) {
    auto* p = object->GetPropertyByNameInChain(L"bShowMouseCursor");
    if (!p || !p->IsA<FBoolProperty>()) throw std::runtime_error("Controller cursor property mismatch");
    return static_cast<FBoolProperty*>(p);
}
void text_value(UObject* widget, const std::string& text) {
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_StringToText", 2);
    FString value(wide(text).c_str()); convert.set(L"InString", value); convert.run();
    Call set(widget, L"SetText", 1); set.copy(L"InText", convert, L"ReturnValue"); set.run();
}
template<class T> void member(void* data, size_t bytes, UObject* structure, const wchar_t* name, const T& value) {
    auto* p=field(structure,name,sizeof(T));
    if(p->GetOffset_Internal()<0 || static_cast<size_t>(p->GetOffset_Internal())+sizeof(T)>bytes)
        throw std::runtime_error("Widget style member exceeds reflected structure");
    p->CopyCompleteValue(static_cast<std::byte*>(data)+p->GetOffset_Internal(),&value);
}
void font_size(UObject* widget, float size, UObject* font_object=nullptr) {
    auto* font = widget->GetPropertyByNameInChain(L"Font");
    auto* size_field = field(find(L"/Script/SlateCore.SlateFontInfo"), L"Size", sizeof(float));
    Call set(widget, L"SetFont", 1);
    auto* param = set.param(L"InFontInfo");
    if (!font || !font->SameType(param) || size_field->GetOffset_Internal() < 0 || static_cast<size_t>(size_field->GetOffset_Internal()) + sizeof(float) > static_cast<size_t>(param->GetElementSize()))
        throw std::runtime_error("Wardrobe font layout mismatch");
    param->CopyCompleteValue(set.data(param), reinterpret_cast<std::byte*>(widget) + font->GetOffset_Internal());
    std::memcpy(static_cast<std::byte*>(set.data(param)) + size_field->GetOffset_Internal(), &size, sizeof(size));
    auto* info=find(L"/Script/SlateCore.SlateFontInfo");
    if(font_object) member(set.data(param),param->GetElementSize(),info,L"FontObject",font_object);
    member(set.data(param),param->GetElementSize(),info,L"TypefaceFontName",FName(L"Regular"));
    set.run();
}
void flat_button(UObject* widget,bool active) {
    Call set(widget,L"SetStyle",1);
    auto* param=set.param(L"InStyle");
    auto* source=widget->GetPropertyByNameInChain(L"WidgetStyle");
    if(!source || !source->SameType(param)) throw std::runtime_error("Button style layout mismatch");
    param->CopyCompleteValue(set.data(param),reinterpret_cast<std::byte*>(widget)+source->GetOffset_Internal());
    auto* style=find(L"/Script/SlateCore.ButtonStyle"); auto* brush=find(L"/Script/SlateCore.SlateBrush");
    for(const auto* name:{L"Normal",L"Hovered",L"Pressed",L"Disabled"}) {
        auto* p=style->GetPropertyByNameInChain(name);
        if(!p || p->GetOffset_Internal()<0 || p->GetOffset_Internal()+p->GetElementSize()>param->GetElementSize()) throw std::runtime_error("Invalid button brush");
        auto* data=static_cast<std::byte*>(set.data(param))+p->GetOffset_Internal();
        Color tint{0,0,0,0};
        if(std::wstring_view(name)==L"Hovered") tint={0.18f,0.135f,0.075f,0.55f};
        else if(std::wstring_view(name)==L"Pressed") tint={0.30f,0.225f,0.12f,0.7f};
        else if(active) tint={0.10f,0.075f,0.035f,0.25f};
        member(data,p->GetElementSize(),brush,L"DrawAs",uint8_t{3});
        member(data,p->GetElementSize(),brush,L"TintColor",SlateColor{tint});
        member(data,p->GetElementSize(),brush,L"ResourceObject",static_cast<UObject*>(nullptr));
        member(data,p->GetElementSize(),brush,L"Margin",Margin{});
    }
    set.run();
}
UObject* content(UObject* parent, UObject* child) {
    Call c(parent, L"SetContent", 2); c.set(L"content", child); c.run(); return c.get<UObject*>();
}
struct Layout {
    UObject* tree; UObject* canvas; double scale;
    UObject* serif;
    void place(UObject* widget, double x, double y, double width, double height) {
        Call add(canvas, L"AddChildToCanvas", 2); add.set(L"content", widget); add.run();
        auto* slot = add.get<UObject*>();
        invoke(slot, L"SetPosition", L"InPosition", Vec2{x*scale, y*scale});
        invoke(slot, L"SetSize", L"InSize", Vec2{width*scale, height*scale});
    }
    UObject* box(double x, double y, double w, double h, Color color) {
        auto* widget = construct(L"/Script/UMG.Border", tree);
        invoke(widget, L"SetBrushColor", L"InBrushColor", color);
        invoke(widget, L"SetVisibility", L"InVisibility", uint8_t{4}); // SelfHitTestInvisible.
        place(widget,x,y,w,h); return widget;
    }
    void sigil(double x, double y, double size, Color color) {
        // Original shell mark, built from native shapes with no texture uploads.
        box(x+size*.47,y+size*.13,size*.06,size*.76,color);
        for (int side : {-1,1}) {
            auto* stroke=box(x+size*(side<0?.19:.75),y+size*.2,size*.055,size*.63,color);
            invoke(stroke,L"SetRenderTransformAngle",L"Angle",static_cast<float>(side*22));
            auto* crown=box(x+size*(side<0?.28:.65),y+size*.04,size*.05,size*.38,color);
            invoke(crown,L"SetRenderTransformAngle",L"Angle",static_cast<float>(side*-38));
        }
    }
    void star(double x,double y,double radius,Color color) {
        for(int i=0;i<10;++i) {
            double a=(i*36-90)*3.141592653589793/180,b=((i+1)*36-90)*3.141592653589793/180;
            double r1=i%2?radius*.43:radius,r2=i%2?radius:radius*.43;
            double x1=x+std::cos(a)*r1,y1=y+std::sin(a)*r1,x2=x+std::cos(b)*r2,y2=y+std::sin(b)*r2;
            double length=std::hypot(x2-x1,y2-y1);
            auto* line=box((x1+x2-length)/2,(y1+y2)/2-1,length,2,color);
            invoke(line,L"SetRenderTransformAngle",L"Angle",static_cast<float>(std::atan2(y2-y1,x2-x1)*180/3.141592653589793));
        }
    }
    UObject* label(const std::string& text, double x, double y, double w, double h, float size, Color color=ivory) {
        auto* widget = construct(L"/Script/UMG.TextBlock", tree);
        text_value(widget,text); font_size(widget, size*static_cast<float>(scale),size>=22?serif:nullptr);
        invoke(widget,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{color});
        invoke(widget,L"SetAutoWrapText",L"InAutoTextWrap",true);
        invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3}); // HitTestInvisible.
        place(widget,x,y,w,h); return widget;
    }
    UObject* button(const std::string& text,double x,double y,double w,double h,bool active=false,bool enabled=true,float size=20) {
        auto* widget=construct(L"/Script/UMG.Button",tree);
        flat_button(widget,active);
        auto* focusable=widget->GetPropertyByNameInChain(L"IsFocusable");
        if(!focusable || !focusable->IsA<FBoolProperty>()) throw std::runtime_error("Button focus property mismatch");
        static_cast<FBoolProperty*>(focusable)->SetPropertyValueInContainer(widget,false);
        auto* label=construct(L"/Script/UMG.TextBlock",tree);
        text_value(label,text); font_size(label,size*static_cast<float>(scale),size>=22?serif:nullptr);
        invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{enabled ? (active ? gold : ivory) : muted});
        invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
        content(widget,label);
        invoke(widget,L"SetIsEnabled",L"bInIsEnabled",enabled);
        place(widget,x,y,w,h); return widget;
    }
    UObject* image(UObject* texture,double x,double y,double w,double h,float opacity=1,UVRect uv={}) {
        auto* widget=construct(L"/Script/UMG.Image",tree);
        Call brush(widget,L"SetBrushFromTexture",2); brush.set(L"Texture",texture); brush.set(L"bMatchSize",false); brush.run();
        Call set(widget,L"SetBrush",1); auto* p=set.param(L"InBrush");
        auto* source=widget->GetPropertyByNameInChain(L"Brush");
        if(!source || !source->SameType(p)) throw std::runtime_error("Image brush layout mismatch");
        p->CopyCompleteValue(set.data(p),reinterpret_cast<std::byte*>(widget)+source->GetOffset_Internal());
        member(set.data(p),p->GetElementSize(),find(L"/Script/SlateCore.SlateBrush"),L"UVRegion",uv);
        set.run();
        invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3});
        invoke(widget,L"SetRenderOpacity",L"InOpacity",opacity);
        place(widget,x,y,w,h); return widget;
    }
};
}
void Wardrobe::open(void* engine,const Catalog& catalog,const State& state,Appearance& appearance,const std::string& message_text,const fs::path& assets) {
    auto* pawn=appearance.player(engine);
    if (!pawn) throw std::runtime_error("Load a shell in the game world to open the wardrobe.");
    auto* pc=read<UObject*>(pawn,L"Controller");
    if (!pc) throw std::runtime_error("No local controller is ready.");
    const bool refresh=opened() && camera_.Get() && pawn_.Get()==pawn && controller_.Get()==pc;
    if(refresh) {
        invoke(root_.Get(),L"RemoveFromParent"); root_.Reset(); status_.Reset(); hits_.clear(); rows_.clear(); sliders_.clear(); color_title_.Reset(); color_swatch_.Reset();
    } else close();
    auto* cursor=cursor_property(pc);
    if(!refresh) {
        old_cursor_=cursor->GetPropertyValueInContainer(pc);
        if (old_cursor_) throw std::runtime_error("Close the current game menu before opening the wardrobe.");
    }
    controller_=pc; pawn_=pawn;
    try {
        auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
        auto* root=construct(L"/Script/UMG.UserWidget",pc); root_=root;
        invoke(root,L"SetOwningPlayer",L"LocalPlayerController",pc);
        auto* tree=construct(L"/Script/UMG.WidgetTree",root); object_property(root,L"WidgetTree",tree);
        auto* canvas=construct(L"/Script/UMG.CanvasPanel",tree); object_property(tree,L"RootWidget",canvas);
        // Viewport ownership keeps the tree alive while artwork is imported.
        invoke(root,L"AddToViewport",L"ZOrder",int32_t{9000});
        auto* library=find(L"/Script/UMG.Default__WidgetLayoutLibrary");
        Call dimensions(library,L"GetViewportSize",2); dimensions.set(L"WorldContextObject",pawn); dimensions.run();
        const auto viewport=dimensions.get<Vec2>();
        Call scaling(library,L"GetViewportScale",2); scaling.set(L"WorldContextObject",pawn); scaling.run();
        const float dpi=scaling.get<float>();
        if (viewport.x<320 || viewport.y<240 || dpi<=0) throw std::runtime_error("Viewport dimensions are not ready.");
        Layout ui{tree,canvas,viewport.y/1080.0/dpi,serif};
        const double width=viewport.x/viewport.y*1080.0;
        const double x=width-580;
        auto artwork=[&](const fs::path& name) {
            auto path=name.is_absolute()?name:assets/name;
            auto& cached=textures_[narrow(path.wstring())];
            if(auto* texture=cached.Get()) return texture;
            if(!fs::exists(path)) throw std::runtime_error("Wardrobe artwork is missing: "+narrow(path.wstring()));
            Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
            import.set(L"WorldContextObject",pawn); import.set(L"Filename",FString(path.c_str())); import.run();
            auto* texture=import.get<UObject*>();
            if(!texture) throw std::runtime_error("Could not import wardrobe artwork");
            cached=texture; return texture;
        };
        std::vector<const Outfit*> outfits;
        for(const auto& outfit:catalog.outfits)
            if(category_!=1 || state.favorites.contains(outfit.id)) outfits.push_back(&outfit);
        constexpr int per_page=5;
        const int pages=std::max(1,(static_cast<int>(outfits.size())+per_page-1)/per_page);
        page_=std::min(page_,pages-1);
        const int visible=std::min(per_page,static_cast<int>(outfits.size())-page_*per_page);
        const double top=46;
        const double list_y=top+280;
        const double list_height=category_==3?410:category_==2?330:56+std::max(110*visible,90);
        const double footer=list_y+list_height+14;
        const double bottom=footer+120;
        auto* background=artwork("wardrobe-v1.png");
        // Sample the header at its original aspect, then stretch only the quiet
        // middle texture. A small collection does not need a full-height panel.
        ui.image(background,x,top,552,178,.97f,UVRect{0,0,1,.21f});
        ui.image(background,x,top+178,552,bottom-top-196,.97f,UVRect{0,.21f,1,.97f});
        ui.image(background,x,bottom-18,552,18,.97f,UVRect{0,.97f,1,1});
        auto centered=[&](const std::string& text,double y,float size,Color color=ivory) {
            auto* label=ui.label(text,x+38,y,476,size*1.6,size,color);
            invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
            return label;
        };
        auto bind=[&](UObject* widget,Json action) { hits_.push_back({FWeakObjectPtr(widget),std::move(action),false}); };
        auto row=[&](double y,double h,Json wear,Json favorite=Json{},Json prev=Json{},Json next=Json{},Json save=Json{}) {
            auto* marker=ui.box(x+24,y+8,2,h-16,Color{0,0,0,0});
            rows_.push_back({std::move(wear),std::move(favorite),std::move(prev),std::move(next),std::move(save),FWeakObjectPtr(marker)});
        };
        const char* categories[]={"All appearances","Favorites","Saved looks","Colors"};
        centered(categories[category_],top+191,27);
        centered("LB / RB   Change section",top+235,18,muted);
        for(int tab=0;tab<4;++tab) {
            double y=top+188+tab*90;
            ui.box(x-89,y,76,80,Color{.014f,.016f,.016f,.93f});
            if(category_==tab) ui.box(x-14,y+10,1,60,gold);
            bind(ui.button("",x-87,y,72,80,category_==tab),{{"action","filter"},{"category",tab}});
            Color color=category_==tab?gold:ivory;
            if(tab==0) ui.sigil(x-70,y+7,38,color);
            else if(tab==1) ui.star(x-51,y+28,17,color);
            else if(tab==2) for(int i=0;i<3;++i) { ui.box(x-67,y+15+i*10,5,4,color); ui.box(x-57,y+15+i*10,23,2,color); }
            else { ui.box(x-69,y+15,14,25,Color{.32f,.08f,.10f,1}); ui.box(x-50,y+15,14,25,Color{.07f,.12f,.27f,1}); ui.box(x-31,y+15,10,25,gold); }
            auto* label=ui.label(tab==0?"ALL":tab==1?"FAV":tab==2?"LOOKS":"COLOR",x-86,y+57,70,23,15,color);
            invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
        }
        double preview=(x-89)/2-218;
        ui.box(preview-14,937,464,103,Color{.009f,.012f,.013f,.78f});
        bind(ui.button("<",preview,946,60,43,false,true,24),{{"action","rotate"},{"degrees",-30}});
        bind(ui.button("Reset view",preview+68,946,300,43,false,true,24),{{"action","front"}});
        bind(ui.button(">",preview+376,946,60,43,false,true,24),{{"action","rotate"},{"degrees",30}});
        auto* hint=ui.label("RS orbit   /   LS zoom + frame",preview,1003,436,27,18,ivory);
        invoke(hint,L"SetJustification",L"InJustification",uint8_t{1});
        auto selection=state.selections.find(appearance.shell);
        if(category_==3) {
            const Outfit* outfit=nullptr;
            if(selection!=state.selections.end()) for(const auto& o:catalog.outfits) if(o.id==selection->second.outfit) outfit=&o;
            if(!outfit || outfit->colors.controls.empty()) centered("Wear an appearance with color options.",list_y+50,22,muted);
            else {
                const auto& options=outfit->colors; const auto& custom=selection->second.colors;
                auto values=color_values(options,custom);
                size_t palette=0;
                for(size_t i=0;i<options.palettes.size();++i) if(options.palettes[i].id==custom.palette) palette=i+1;
                auto palette_action=[&](size_t i) { return Json{{"action","palette"},{"palette",i?options.palettes[i-1].id:"original"}}; };
                size_t count=options.palettes.size()+1;
                auto prev=palette_action((palette+count-1)%count),next=palette_action((palette+1)%count);
                ui.label(outfit->name,x+46,list_y-12,450,30,17,muted);
                bind(ui.button("<",x+40,list_y+25,40,42),prev);
                std::string title=palette?options.palettes[palette-1].name:"Original colors";
                if(!custom.values.empty()) title+=" *";
                color_title_=centered(title,list_y+29,24,gold);
                color_title_text_=title;
                bind(ui.button(">",x+476,list_y+25,40,42),next);
                row(list_y+22,48,{}, {},prev,next);
                for(size_t i=0;i<count;++i) {
                    double width=462.0/count;
                    auto* button=ui.button(i?options.palettes[i-1].name.substr(0,options.palettes[i-1].name.find(' ')):"Original",x+45+i*width,list_y+75,width-6,33,palette==i,true,16);
                    bind(button,palette_action(i));
                }
                color_part_=(color_part_%static_cast<int>(options.controls.size())+static_cast<int>(options.controls.size()))%static_cast<int>(options.controls.size());
                const auto& control=options.controls[color_part_];
                auto value=values.contains(control.id)?values.at(control.id):control.value;
                Json previous={{"action","color_part"},{"delta",-1}},following={{"action","color_part"},{"delta",1}};
                bind(ui.button("<",x+40,list_y+125,40,43),previous);
                centered(control.name,list_y+131,25);
                bind(ui.button(">",x+476,list_y+125,40,43),following);
                row(list_y+125,43,{}, {},previous,following);
                if(!control.scalar) color_swatch_=ui.box(x+479,list_y+174,24,8,Color{srgb_linear(value[0]),srgb_linear(value[1]),srgb_linear(value[2]),1});
                const char* channels[]={"Red","Green","Blue"};
                for(int channel=0;channel<(control.scalar?1:3);++channel) {
                    double y=list_y+190+channel*49;
                    ui.label(control.scalar?"Intensity":channels[channel],x+47,y+4,89,30,17,muted);
                    auto* slider=construct(L"/Script/UMG.Slider",tree);
                    invoke(slider,L"SetMinValue",L"InValue",control.minimum); invoke(slider,L"SetMaxValue",L"InValue",control.maximum);
                    invoke(slider,L"SetStepSize",L"InValue",control.step); invoke(slider,L"SetValue",L"InValue",value[channel]);
                    invoke(slider,L"SetSliderBarColor",L"InValue",Color{.07f,.065f,.055f,1});
                    invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                    ui.place(slider,x+143,y,293,33);
                    auto* label=ui.label(slider_text(value[channel],control.scalar),x+451,y+4,59,30,17,ivory);
                    Json action={{"action","color"},{"control",control.id},{"channel",channel},{"refresh",false}};
                    sliders_.push_back({FWeakObjectPtr(slider),FWeakObjectPtr(label),action,value[channel],control.scalar});
                    action["refresh"]=true; action["delta"]=-1; auto left=action; action["delta"]=1;
                    row(y,37,Json{{"action","reset_color"},{"control",control.id}}, {},left,action);
                }
                Json reset={{"action","reset_color"},{"control",control.id}},original={{"action","palette"},{"palette","original"}};
                bind(ui.button("Reset part",x+41,list_y+350,220,40,false,true,19),reset);
                bind(ui.button("Reset all colors",x+274,list_y+350,240,40,false,true,19),original);
                row(list_y+350,40,reset,{}, {},{},original);
            }
        } else if(category_==2) {
            for(int slot=0;slot<3;++slot) {
                auto name="look."+std::to_string(slot+1);
                bool exists=state.presets.contains(name);
                double y=list_y+slot*110;
                ui.box(x+32,y,488,101,Color{.023f,.025f,.024f,.78f});
                ui.label("0"+std::to_string(slot+1),x+48,y+22,64,50,30,gold);
                ui.label(exists?"Saved appearance":"Empty look",x+130,y+8,354,40,24,exists?ivory:muted);
                Json wear=exists?Json{{"action","load_look"},{"name",name}}:Json{};
                Json save={{"action","save_look"},{"name",name}};
                bind(ui.button("A  Wear",x+116,y+52,160,42,false,exists),wear);
                bind(ui.button(exists?"X  Replace":"X  Save",x+286,y+52,218,42),save);
                row(y,101,wear,{}, {},{},save);
            }
        } else {
            Json original={{"action","restore"}};
            bind(ui.button("Original appearance",x+34,list_y,484,48,selection==state.selections.end(),true,25),original);
            row(list_y,48,original);
            for(int n=0;n<visible;++n) {
                const auto& outfit=*outfits[page_*per_page+n];
                bool compatible=catalog.compatible(outfit.id,appearance.shell);
                bool chosen=selection!=state.selections.end() && selection->second.outfit==outfit.id;
                size_t variant=0;
                if(chosen) for(size_t i=0;i<outfit.variants.size();++i) if(outfit.variants[i].id==selection->second.variant) variant=i;
                double y=list_y+56+n*110;
                ui.box(x+32,y,488,102,chosen?Color{.052f,.041f,.023f,.83f}:Color{.019f,.022f,.022f,.76f});
                ui.box(x+32,y+101,488,1,chosen?gold:Color{.12f,.105f,.08f,.55f});
                if(!outfit.thumbnail.empty()) ui.image(artwork(outfit.thumbnail),x+43,y+13,76,76);
                else ui.sigil(x+60,y+29,36,gold);
                const auto& title=outfit.name;
                Json wear={{"action","select"},{"outfit",outfit.id},{"variant",outfit.variants[variant].id}};
                Json favorite={{"action","favorite"},{"outfit",outfit.id}};
                auto prev_index=(variant+outfit.variants.size()-1)%outfit.variants.size();
                auto next_index=(variant+1)%outfit.variants.size();
                Json previous={{"action","select"},{"outfit",outfit.id},{"variant",outfit.variants[prev_index].id}};
                Json next={{"action","select"},{"outfit",outfit.id},{"variant",outfit.variants[next_index].id}};
                bind(ui.button(title,x+127,y+4,327,46,false,compatible,25),wear);
                bind(ui.button("",x+461,y+5,49,45),favorite);
                ui.star(x+485,y+27,13,state.favorites.contains(outfit.id)?gold:muted);
                if(compatible && outfit.variants.size()>1) {
                    bind(ui.button("<",x+130,y+51,34,35),previous);
                    auto* label=ui.label(outfit.variants[variant].name,x+170,y+56,285,32,19,chosen?gold:ivory);
                    invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
                    bind(ui.button(">",x+472,y+51,34,35),next);
                    double segment=310.0/outfit.variants.size();
                    for(size_t i=0;i<outfit.variants.size();++i) ui.box(x+165+i*segment,y+91,segment-6,2,chosen&&i==variant?gold:Color{.16f,.16f,.14f,.65f});
                } else if(compatible) {
                    auto* label=ui.label(outfit.author.empty()?"Appearance":"By "+outfit.author,x+135,y+57,361,32,17,muted);
                    invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
                } else ui.label("Unavailable for this form",x+135,y+58,361,32,19,muted);
                row(y,102,compatible?wear:Json{},favorite,compatible&&outfit.variants.size()>1?previous:Json{},compatible&&outfit.variants.size()>1?next:Json{});
            }
            if(outfits.empty()) centered("Star an appearance to keep it here.",list_y+81,22,muted);
            if(pages>1) {
                bind(ui.button("<",x+34,top+235,42,32),{{"action","page"},{"delta",-1}});
                bind(ui.button(">",x+476,top+235,42,32),{{"action","page"},{"delta",1}});
            }
        }
        ui.box(x+38,footer,476,1,Color{.32f,.24f,.13f,.7f});
        status_=centered(message_text,footer+12,19);
        centered(category_==3?"D-pad adjust   /   A reset selected part":"A wear   /   Y favorite   /   D-pad browse",footer+43,18,muted);
        bind(ui.button("B / Esc   Close",x+70,footer+72,412,35,false,true,23),{{"action","close"}});
        // Own the restoration obligation before the first input change.
        owns_input_=true; cursor->SetPropertyValueInContainer(pc,true);
        Call mode(find(L"/Script/UMG.Default__WidgetBlueprintLibrary"),L"SetInputMode_UIOnlyEx",4);
        mode.set(L"PlayerController",pc); mode.set(L"InWidgetToFocus",root);
        mode.set(L"InMouseLockMode",uint8_t{0}); mode.set(L"bFlushInput",true); mode.run();
        if(!refresh) {
            camera_open(viewport.x/viewport.y, (width-674)/(2*width)+.02);
            preview_open();
            protect();
        } else {
            // Keep the live camera and pause ownership. Recenter only if framing changed.
            rotate(0,true);
            auto* source=read<UObject*>(pawn,L"Mesh");
            if(!preview_mesh_.Get() || mesh_asset(preview_mesh_.Get())!=mesh_asset(source)) {
                auto phase=preview_time_;
                preview_close(); preview_open();
                preview_time_=std::fmod(phase,preview_length_); preview_update(0);
            }
            sync_materials();
        }
        invoke(root,L"RemoveFromParent");
        invoke(root,L"AddToViewport",L"ZOrder",int32_t{9000});
        invoke(root,L"SetVisibility",L"InVisibility",uint8_t{0});
        focus(focus_);
    } catch (...) { close(); throw; }
}
namespace {
struct Vec3 { double x{}, y{}, z{}; };
struct alignas(16) Transform {
    std::array<double,4> rotation{0,0,0,1};
    alignas(16) Vec3 location;
    alignas(16) Vec3 scale{1,1,1};
};
static_assert(sizeof(Transform)==96);
constexpr double pi=3.14159265358979323846;
UObject* view_target(UObject* pc) {
    auto* manager=read<UObject*>(pc,L"PlayerCameraManager");
    if(manager && manager->GetPropertyByNameInChain(L"ActiveCameraActor")) {
        auto* active=read<UObject*>(manager,L"ActiveCameraActor");
        if(active && FWeakObjectPtr(active).Get()) return active;
    }
    Call c(pc,L"GetViewTarget",1); c.run(); return c.get<UObject*>();
}
void set_view(UObject* pc,UObject* target) {
    Call c(pc,L"SetViewTargetWithBlend",5);
    c.set(L"NewViewTarget",target); c.set(L"BlendTime",0.0f); c.set(L"BlendFunc",uint8_t{0});
    c.set(L"BlendExp",0.0f); c.set(L"bLockOutgoing",false); c.run();
}
}
void Wardrobe::camera_open(double aspect,double screen_x) {
    auto* pc=controller_.Get(); auto* pawn=pawn_.Get();
    if(!pc || !pawn) throw std::runtime_error("Player changed before camera setup");
    view_before_=view_target(pc);
    auto* library=find(L"/Script/Engine.Default__GameplayStatics");
    Transform transform;
    Call location(pawn,L"K2_GetActorLocation",1); location.run(); transform.location=location.get<Vec3>();
    Call spawn(library,L"BeginDeferredActorSpawnFromClass",7);
    spawn.set(L"WorldContextObject",pawn); spawn.set(L"ActorClass",static_cast<UClass*>(find(L"/Script/Engine.CameraActor")));
    spawn.set(L"SpawnTransform",transform); spawn.set(L"CollisionHandlingOverride",uint8_t{1});
    spawn.set(L"Owner",pawn); spawn.set(L"TransformScaleMethod",uint8_t{0}); spawn.run();
    auto* actor=spawn.get<UObject*>();
    if(!actor) throw std::runtime_error("Could not create the wardrobe camera");
    camera_=actor;
    Call finish(library,L"FinishSpawningActor",4);
    finish.set(L"Actor",actor); finish.set(L"SpawnTransform",transform); finish.set(L"TransformScaleMethod",uint8_t{0}); finish.run();
    if(finish.get<UObject*>()!=actor) throw std::runtime_error("Wardrobe camera spawn was not confirmed");
    auto* component=read<UObject*>(actor,L"CameraComponent");
    invoke(component,L"SetFieldOfView",L"InFieldOfView",42.0f);
    auto* aspect_flag=component->GetPropertyByNameInChain(L"bConstrainAspectRatio");
    if(!aspect_flag || !aspect_flag->IsA<FBoolProperty>()) throw std::runtime_error("Camera aspect property mismatch");
    static_cast<FBoolProperty*>(aspect_flag)->SetPropertyValueInContainer(component,false);
    auto* capsule=read<UObject*>(pawn,L"CapsuleComponent");
    float half_height=90;
    if(capsule) { Call height(capsule,L"GetScaledCapsuleHalfHeight",1); height.run(); half_height=height.get<float>(); }
    default_distance_=std::clamp(half_height*aspect/(std::tan(21*pi/180)*.70),280.0,750.0);
    distance_=default_distance_; height_=0; pan_=0; pitch_=3;
    frame_offset_=std::clamp((.5-screen_x)*2.0,0.0,.55);
    Call facing(pawn,L"K2_GetActorRotation",1); facing.run(); yaw_=facing.get<std::array<double,3>>()[1];
    camera_dirty_=true; camera_update();
    set_view(pc,actor);
    if(view_target(pc)!=actor) throw std::runtime_error("The game did not accept the wardrobe camera");
}
void Wardrobe::camera_update() {
    auto* actor=camera_.Get(); auto* pawn=pawn_.Get();
    if(!actor || !pawn) return;
    Call position(pawn,L"K2_GetActorLocation",1); position.run(); auto center=position.get<Vec3>();
    std::array<double,3> current{center.x,center.y,center.z};
    if(!camera_dirty_ && current==last_center_) return;
    last_center_=current;
    center.z+=height_;
    double yaw=yaw_*pi/180,pitch=pitch_*pi/180;
    Vec3 location{center.x+std::cos(yaw)*std::cos(pitch)*distance_,
                  center.y+std::sin(yaw)*std::cos(pitch)*distance_,center.z+std::sin(pitch)*distance_};
    double offset=std::atan((frame_offset_+pan_)*std::tan(21*pi/180))*180/pi;
    std::array<double,3> rotation{-pitch_,std::remainder(yaw_+180+offset,360.0),0};
    Call move(actor,L"K2_SetActorLocationAndRotation",6);
    move.set(L"NewLocation",location); move.set(L"NewRotation",rotation); move.set(L"bSweep",false); move.set(L"bTeleport",true); move.run();
    if(!move.get<bool>()) throw std::runtime_error("Wardrobe camera movement failed");
    camera_dirty_=false;
}
void Wardrobe::camera_close() {
    auto* actor=camera_.Get(); auto* pc=controller_.Get();
    if(actor && pc && view_target(pc)==actor) {
        auto* previous=view_before_.Get();
        if(!previous) previous=pawn_.Get();
        if(previous) set_view(pc,previous);
    }
    if(actor) invoke(actor,L"K2_DestroyActor");
    camera_.Reset(); view_before_.Reset();
}
namespace {
bool paused(UObject* context) {
    Call c(find(L"/Script/Engine.Default__GameplayStatics"),L"IsGamePaused",2);
    c.set(L"WorldContextObject",context); c.run(); return c.get<bool>();
}
void pause_world(UObject* context,bool value) {
    Call c(find(L"/Script/Engine.Default__GameplayStatics"),L"SetGamePaused",3);
    c.set(L"WorldContextObject",context); c.set(L"bPaused",value); c.run();
    if(!c.get<bool>() || paused(context)!=value) throw std::runtime_error("Could not change the wardrobe pause state");
}
FBoolProperty* full_tick(UObject* pc) {
    auto* p=pc->GetPropertyByNameInChain(L"bShouldPerformFullTickWhenPaused");
    if(!p || !p->IsA<FBoolProperty>()) throw std::runtime_error("Paused camera tick property mismatch");
    return static_cast<FBoolProperty*>(p);
}
}
void Wardrobe::protect() {
    auto* pc=controller_.Get();
    if(!pc) throw std::runtime_error("No controller for protected preview");
    auto* tick=full_tick(pc);
    full_tick_before_=tick->GetPropertyValueInContainer(pc);
    full_tick_changed_=true; tick->SetPropertyValueInContainer(pc,true);
    if(!paused(pc)) { owns_pause_=true; pause_world(pc,true); }
}
void Wardrobe::unprotect() {
    auto* pc=controller_.Get();
    if(pc && owns_pause_) pause_world(pc,false);
    owns_pause_=false;
    if(pc && full_tick_changed_) full_tick(pc)->SetPropertyValueInContainer(pc,full_tick_before_);
    full_tick_changed_=false;
}
namespace {
FBoolProperty* boolean_field(UObject* object,const wchar_t* name) {
    auto* p=object->GetPropertyByNameInChain(name);
    if(!p || !p->IsA<FBoolProperty>()) throw std::runtime_error("Missing boolean: "+narrow(name));
    return static_cast<FBoolProperty*>(p);
}
unsigned char* preview_cloth_tick(UObject* component) {
    // UE's component pause flag does not include its separate ClothTickFunction.
    // Identify that unreflected member by the verified game vtable and owner,
    // bounded by reflected neighbors. Refuse a different binary layout.
    auto* module=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    constexpr uintptr_t vtable_rva=0x9203478,diagnostic_rva=0x436c010,label_rva=0x93e3d58;
    constexpr std::array<unsigned char,14> signature{0x40,0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0x49,0x28,0x48,0x8b,0xda,0x48};
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) throw std::runtime_error("Unknown game image for preview cloth");
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(module+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage<label_rva+24 ||
       std::memcmp(module+diagnostic_rva,signature.data(),signature.size())!=0 ||
       std::memcmp(module+label_rva,L"[ClothTick]",22)!=0)
        throw std::runtime_error("Preview cloth adapter requires the verified game build");
    auto begin=field(component,L"ClothingSimulationFactory",sizeof(UObject*))->GetOffset_Internal()+static_cast<int>(sizeof(UObject*));
    auto end=field(component,L"TeleportDistanceThreshold",sizeof(float))->GetOffset_Internal();
    auto* flag=boolean_field(find(L"/Script/Engine.TickFunction"),L"bTickEvenWhenPaused");
    if(end<=begin || end-begin>512 || flag->GetOffset_Internal()!=0xa) throw std::runtime_error("Preview cloth tick layout mismatch");
    unsigned char* tick=nullptr;
    for(size_t offset=begin;offset+0x30<=static_cast<size_t>(end);offset+=8) {
        uintptr_t vtable{};UObject* owner{};
        auto* candidate=reinterpret_cast<unsigned char*>(component)+offset;
        std::memcpy(&vtable,candidate,sizeof(vtable));std::memcpy(&owner,candidate+0x28,sizeof(owner));
        // The target is assigned on first tick registration. A fresh preview
        // still has a null target; an unrelated non-null target is refused.
        if(vtable==reinterpret_cast<uintptr_t>(module)+vtable_rva && (!owner || owner==component)) {
            if(tick) throw std::runtime_error("Ambiguous preview cloth tick");
            tick=candidate;
        }
    }
    if(!tick) throw std::runtime_error("Preview cloth tick was not identified");
    return tick;
}
void enable_preview_cloth(UObject* component) {
    auto* tick=preview_cloth_tick(component);
    auto* flag=boolean_field(find(L"/Script/Engine.TickFunction"),L"bTickEvenWhenPaused");
    flag->SetPropertyValueInContainer(tick,true);
    if(!flag->GetPropertyValueInContainer(tick)) throw std::runtime_error("Preview cloth pause flag did not change");
}
}
void Wardrobe::preview_open() {
    auto* pawn=pawn_.Get();
    if(!pawn) throw std::runtime_error("No player for animated preview");
    auto* source=read<UObject*>(pawn,L"Mesh");
    auto* mesh=mesh_asset(source);
    if(!mesh) throw std::runtime_error("No appearance for animated preview");
    FWeakObjectPtr source_before(source),mesh_before(mesh);
    const std::string idle="A_Shared_Idle_L";
    preview_animation_="/Game/Sparta/Characters/Shells/_Shared/Animation/Locomotion/Idles/"+idle+"."+idle;
    auto* animation=load(preview_animation_);
    pawn=pawn_.Get(); source=source_before.Get(); mesh=mesh_before.Get();
    if(!pawn || !source || !mesh || read<UObject*>(pawn,L"Mesh")!=source || mesh_asset(source)!=mesh)
        throw std::runtime_error("Player changed while loading preview animation");
    if(!animation->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimSequence"))))
        throw std::runtime_error("Preview idle is not an animation sequence");
    auto* skeleton=read<UObject*>(mesh,L"Skeleton");
    if(!skeleton || read<UObject*>(animation,L"Skeleton")!=skeleton || read<uint8_t>(animation,L"AdditiveAnimType")!=0)
        throw std::runtime_error("Preview requires a full-body idle on the appearance's skeleton");
    Call length(animation,L"GetPlayLength",1); length.run(); preview_length_=length.get<float>();
    if(!std::isfinite(preview_length_) || preview_length_<.1) throw std::runtime_error("Preview idle has no animation duration");
    Call transform(source,L"K2_GetComponentToWorld",1); transform.run();
    auto* library=find(L"/Script/Engine.Default__GameplayStatics");
    Call spawn(library,L"BeginDeferredActorSpawnFromClass",7);
    spawn.set(L"WorldContextObject",pawn); spawn.set(L"ActorClass",static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshActor")));
    spawn.copy(L"SpawnTransform",transform,L"ReturnValue"); spawn.set(L"CollisionHandlingOverride",uint8_t{1});
    spawn.set(L"Owner",pawn); spawn.set(L"TransformScaleMethod",uint8_t{0}); spawn.run();
    auto* actor=spawn.get<UObject*>();
    if(!actor) throw std::runtime_error("Could not create the animated preview");
    preview_=actor;
    invoke(actor,L"SetActorEnableCollision",L"bNewActorEnableCollision",false);
    boolean_field(actor,L"bShouldDoAnimNotifies")->SetPropertyValueInContainer(actor,false);
    Call finish(library,L"FinishSpawningActor",4);
    finish.set(L"Actor",actor); finish.copy(L"SpawnTransform",transform,L"ReturnValue");
    finish.set(L"TransformScaleMethod",uint8_t{0}); finish.run();
    if(finish.get<UObject*>()!=actor) throw std::runtime_error("Preview spawn was not confirmed");
    auto* component=read<UObject*>(actor,L"SkeletalMeshComponent"); preview_mesh_=component;
    set_mesh(component,mesh);
    // Match the selected appearance, including the player's original material overrides.
    auto materials=overrides(source);
    for(int i=0;i<materials.Num();++i) {
        UObject* material{}; std::memcpy(&material,materials.GetRawPtr(i),sizeof(material));
        Call set(component,L"SetMaterial",2); set.set(L"ElementIndex",i); set.set(L"Material",material); set.run();
    }
    invoke(component,L"SetCollisionEnabled",L"NewType",uint8_t{0});
    invoke(component,L"SetTickableWhenPaused",L"bTickableWhenPaused",true);
    invoke(component,L"SetComponentTickEnabled",L"bEnabled",true);
    Call play(component,L"PlayAnimation",2); play.set(L"NewAnimToPlay",animation); play.set(L"bLooping",true); play.run();
    // CSS owns time and suppresses notifies. Only this disposable visual copy changes animation mode.
    invoke(component,L"SetPlayRate",L"Rate",0.0f);
    enable_preview_cloth(component);
    preview_time_=0; preview_sample_at_=0; preview_sampled_=false; preview_moving_=false;
    preview_update(0);
    Call attachments(pawn,L"GetAttachedActors",3);
    attachments.set(L"bResetArray",true); attachments.set(L"bRecursivelyIncludeAttachedActors",true); attachments.run();
    auto* p=attachments.param(L"OutActors");
    if(!p->IsA<FArrayProperty>()) throw std::runtime_error("Attached actor array mismatch");
    auto* array=static_cast<FArrayProperty*>(p);
    if(array->GetInner()->GetElementSize()!=sizeof(UObject*)) throw std::runtime_error("Attached actor layout mismatch");
    FScriptArrayHelper actors(array,attachments.data(p));
    if(actors.Num()>64) throw std::runtime_error("Unexpected number of player attachments");
    auto hide=[&](UObject* target) {
        if(!target || target==actor) return;
        hidden_.push_back({FWeakObjectPtr(target),boolean_field(target,L"bHidden")->GetPropertyValueInContainer(target)});
        invoke(target,L"SetActorHiddenInGame",L"bNewHidden",true);
    };
    hide(pawn);
    for(int i=0;i<actors.Num();++i) { UObject* child{}; std::memcpy(&child,actors.GetRawPtr(i),sizeof(child)); hide(child); }
}
void Wardrobe::preview_update(double delta) {
    auto* component=preview_mesh_.Get();
    if(!component || preview_length_<=0) return;
    preview_time_=std::fmod(preview_time_+delta,preview_length_);
    Call position(component,L"SetPosition",2);
    position.set(L"InPos",static_cast<float>(preview_time_)); position.set(L"bFireNotifies",false); position.run();
    // A bounded read-back checks evaluated bones, not just the animation clock.
    preview_sample_at_+=delta;
    if(!preview_moving_ && preview_sample_at_>=.25) {
        preview_sample_at_=0;
        Call socket(component,L"GetSocketLocation",2); socket.set(L"InSocketName",FName(L"head")); socket.run();
        auto head=socket.get<std::array<double,3>>();
        if(preview_sampled_) {
            double movement=0;
            for(size_t i=0;i<head.size();++i) movement+=std::pow(head[i]-preview_first_head_[i],2);
            preview_moving_=movement>.0001 && movement<400;
        } else { preview_first_head_=head; preview_sampled_=true; }
    }
}
void Wardrobe::preview_close() {
    for(auto& saved:hidden_) if(auto* actor=saved.actor.Get()) invoke(actor,L"SetActorHiddenInGame",L"bNewHidden",saved.before);
    hidden_.clear();
    if(auto* actor=preview_.Get()) invoke(actor,L"K2_DestroyActor");
    preview_.Reset(); preview_mesh_.Reset(); preview_length_=0; preview_animation_.clear(); preview_moving_=false;
}
void Wardrobe::close() {
    if(auto* root=root_.Get()) invoke(root,L"RemoveFromParent");
    root_.Reset(); status_.Reset(); hits_.clear(); rows_.clear(); sliders_.clear(); color_title_.Reset(); color_swatch_.Reset();
    // Restore gameplay input even if a camera cleanup operation fails.
    std::exception_ptr error;
    try { preview_close(); } catch(...) { error=std::current_exception(); }
    try { camera_close(); } catch(...) { if(!error) error=std::current_exception(); }
    try { unprotect(); } catch(...) { if(!error) error=std::current_exception(); }
    if(owns_input_) {
        if(auto* pc=controller_.Get()) {
            Call mode(find(L"/Script/UMG.Default__WidgetBlueprintLibrary"),L"SetInputMode_GameOnly",2);
            mode.set(L"PlayerController",pc); mode.set(L"bFlushInput",true); mode.run();
            cursor_property(pc)->SetPropertyValueInContainer(pc,old_cursor_);
        }
        owns_input_=false;
    }
    pawn_.Reset(); controller_.Reset(); right_mouse_=false;
    if(error) std::rethrow_exception(error);
}
void Wardrobe::focus(int index) {
    if(rows_.empty()) return;
    focus_=std::clamp(index,0,static_cast<int>(rows_.size())-1);
    for(size_t i=0;i<rows_.size();++i) if(auto* marker=rows_[i].marker.Get())
        invoke(marker,L"SetBrushColor",L"InBrushColor",static_cast<int>(i)==focus_?ivory:Color{0,0,0,0});
}
Json Wardrobe::poll(float delta,bool focused) {
    XINPUT_STATE pad{};
    auto now=GetTickCount64();
    bool connected=false;
    if(pad_index_>=0) connected=XInputGetState(static_cast<DWORD>(pad_index_),&pad)==ERROR_SUCCESS;
    if(!connected && now>=next_pad_search_) {
        next_pad_search_=now+1000; pad_index_=-1;
        for(DWORD i=0;i<4;++i) if(XInputGetState(i,&pad)==ERROR_SUCCESS) { pad_index_=static_cast<int>(i); connected=true; break; }
    }
    uint16_t buttons=focused&&connected?pad.Gamepad.wButtons:0;
    uint16_t pressed=buttons&~pad_buttons_;
    bool chord=(buttons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y);
    bool was_chord=(pad_buttons_&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_Y);
    pad_buttons_=buttons;
    if(chord && !was_chord) return {{"action",opened()?"close":"open"}};
    if(!opened()) { if(owns_input_) close(); return {}; }
    auto* pawn=pawn_.Get(); auto* pc=controller_.Get();
    if(!pawn || !pc || !camera_.Get() || read<UObject*>(pawn,L"Controller")!=pc || read<UObject*>(pc,L"Pawn")!=pawn) { close(); return {}; }
    if(!focused) { last_input_tick_=now; return {}; }
    if(pressed&XINPUT_GAMEPAD_B) return {{"action","close"}};
    if(pressed&XINPUT_GAMEPAD_LEFT_SHOULDER) return {{"action","filter"},{"category",(category_+3)%4}};
    if(pressed&XINPUT_GAMEPAD_RIGHT_SHOULDER) return {{"action","filter"},{"category",(category_+1)%4}};
    const uint16_t directions=XINPUT_GAMEPAD_DPAD_UP|XINPUT_GAMEPAD_DPAD_DOWN|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT;
    uint16_t navigate=pressed&directions;
    if(navigate) repeat_at_=now+350;
    else if((buttons&directions) && now>=repeat_at_) { navigate=buttons&directions; repeat_at_=now+120; }
    if(!rows_.empty()) {
        if(navigate&XINPUT_GAMEPAD_DPAD_UP) focus((focus_+static_cast<int>(rows_.size())-1)%static_cast<int>(rows_.size()));
        if(navigate&XINPUT_GAMEPAD_DPAD_DOWN) focus((focus_+1)%static_cast<int>(rows_.size()));
        const auto& row=rows_[focus_];
        if((navigate&XINPUT_GAMEPAD_DPAD_LEFT) && !row.previous.is_null()) return row.previous;
        if((navigate&XINPUT_GAMEPAD_DPAD_RIGHT) && !row.next.is_null()) return row.next;
        if((pressed&XINPUT_GAMEPAD_A) && !row.wear.is_null()) return row.wear;
        if((pressed&XINPUT_GAMEPAD_Y) && !row.favorite.is_null()) return row.favorite;
        if(pressed&XINPUT_GAMEPAD_X) { if(!row.save.is_null()) return row.save; else return {{"action","filter"},{"category",2}}; }
    }
    if(pressed&XINPUT_GAMEPAD_RIGHT_THUMB) rotate(0,true);
    double dt=last_input_tick_?std::min((now-last_input_tick_)/1000.0,.05):frame_delta(delta);
    last_input_tick_=now;
    preview_update(dt);
    auto orbit=focused&&connected?stick(pad.Gamepad.sThumbRX,pad.Gamepad.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE):Stick{};
    auto dolly=focused&&connected?stick(pad.Gamepad.sThumbLX,pad.Gamepad.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE):Stick{};
    auto down=[](int key) { return (GetAsyncKeyState(key)&0x8000)!=0; };
    if(down('W')) dolly.y+=1;
    if(down('S')) dolly.y-=1;
    double elevation=(down('E')?1.0:0.0)-(down('Q')?1.0:0.0);
    if(connected) elevation+=(static_cast<double>(pad.Gamepad.bRightTrigger)-pad.Gamepad.bLeftTrigger)/255;
    if(orbit.x || orbit.y || dolly.x || dolly.y || elevation) {
        yaw_=std::remainder(yaw_+orbit.x*(invert_x_?-1:1)*110*dt,360.0);
        pitch_=std::clamp(pitch_+orbit.y*(invert_y_?-1:1)*65*dt,-25.0,40.0);
        distance_=std::clamp(distance_-dolly.y*250*dt,130.0,default_distance_*1.6);
        pan_=std::clamp(pan_+dolly.x*.45*dt,-.25,.25);
        height_=std::clamp(height_+elevation*80*dt,-80.0,110.0);
        camera_dirty_=true;
    }
    POINT mouse{}; GetCursorPos(&mouse);
    bool right=down(VK_RBUTTON);
    if(right && right_mouse_) {
        yaw_=std::remainder(yaw_+(mouse.x-mouse_x_)*.20,360.0);
        pitch_=std::clamp(pitch_-(mouse.y-mouse_y_)*.16,-25.0,40.0);
        camera_dirty_=true;
    }
    mouse_x_=mouse.x; mouse_y_=mouse.y; right_mouse_=right;
    camera_update();
    for(auto& slider:sliders_) if(auto* widget=slider.widget.Get()) {
        Call current(widget,L"GetValue",1); current.run(); auto value=current.get<float>();
        if(std::abs(value-slider.previous)>.00001f) {
            slider.previous=value;
            if(auto* label=slider.label.Get()) text_value(label,slider_text(value,slider.scalar));
            if(auto* title=color_title_.Get();title && !color_title_text_.ends_with(" *")) { color_title_text_+=" *"; text_value(title,color_title_text_); }
            if(auto* swatch=color_swatch_.Get();swatch && sliders_.size()==3) {
                Color color{srgb_linear(sliders_[0].previous),srgb_linear(sliders_[1].previous),srgb_linear(sliders_[2].previous),1};
                invoke(swatch,L"SetBrushColor",L"InBrushColor",color);
            }
            auto action=slider.action; action["value"]=value; return action;
        }
    }
    for(auto& hit:hits_) if(auto* widget=hit.widget.Get()) {
        Call pressed_call(widget,L"IsPressed",1); pressed_call.run();
        bool held=pressed_call.get<bool>();
        bool clicked=held && !hit.down;
        hit.down=held;
        if(clicked) return hit.action;
    }
    return {};
}
void Wardrobe::message(const std::string& value) { if(auto* widget=status_.Get()) text_value(widget,value); }
void Wardrobe::rotate(double degrees,bool front) {
    if(!opened() || !pawn_.Get()) return;
    if(front) {
        Call actor(pawn_.Get(),L"K2_GetActorRotation",1); actor.run(); auto yaw=actor.get<std::array<double,3>>()[1];
        if(std::abs(std::remainder(yaw_-yaw,360.0))<.001 && std::abs(distance_-default_distance_)<.001 &&
           std::abs(pitch_-3)<.001 && std::abs(height_)<.001 && std::abs(pan_)<.001) return;
        yaw_=yaw;
        distance_=default_distance_; pitch_=3; height_=0; pan_=0;
    } else yaw_=std::remainder(yaw_+std::clamp(degrees,-90.0,90.0),360.0);
    camera_dirty_=true; camera_update();
}
Json Wardrobe::diagnostics() const {
    Json result={{"controller_connected",pad_index_>=0},{"input_owned",owns_input_},{"camera_active",camera_.Get()!=nullptr}};
    result["preview_active"]=preview_.Get()!=nullptr;
    result["preview_bones_moving"]=preview_moving_;
    if(preview_.Get()) result["preview_animation"]=preview_animation_;
    if(auto* root=root_.Get()) {
        Call shown(root,L"IsInViewport",1); shown.run(); result["in_viewport"]=shown.get<bool>();
        Call visible(root,L"GetVisibility",1); visible.run(); result["visibility"]=visible.get<uint8_t>();
    }
    if(auto* pc=controller_.Get()) {
        result["cursor_visible"]=cursor_property(pc)->GetPropertyValueInContainer(pc);
        result["paused"]=paused(pc);
        result["camera_accepted"]=camera_.Get() && view_target(pc)==camera_.Get();
        Call time(find(L"/Script/Engine.Default__GameplayStatics"),L"GetTimeSeconds",2);
        time.set(L"WorldContextObject",pc); time.run(); result["world_time"]=time.get<double>();
    }
    return result;
}
Json Wardrobe::inspect(UObject* player) const {
    Json result=diagnostics();
    result["framing"]={yaw_,pitch_,distance_,height_,pan_};
    if(auto* actor=camera_.Get()) result["camera"]=narrow(actor->GetPathName());
    if(auto* actor=preview_.Get()) result["preview"]=narrow(actor->GetPathName());
    if(auto* component=preview_mesh_.Get()) {
        result["preview_time"]=preview_time_;
        auto* flag=boolean_field(find(L"/Script/Engine.TickFunction"),L"bTickEvenWhenPaused");
        result["preview_cloth_tick_when_paused"]=flag->GetPropertyValueInContainer(preview_cloth_tick(component));
        Call socket(component,L"GetSocketLocation",2); socket.set(L"InSocketName",FName(L"head")); socket.run();
        result["preview_head"]=socket.get<std::array<double,3>>();
    }
    if(auto* pawn=player) {
        auto* flag=boolean_field(find(L"/Script/Engine.TickFunction"),L"bTickEvenWhenPaused");
        result["gameplay_cloth_tick_when_paused"]=flag->GetPropertyValueInContainer(preview_cloth_tick(read<UObject*>(pawn,L"Mesh")));
        result["player_hidden"]=boolean_field(pawn,L"bHidden")->GetPropertyValueInContainer(pawn);
        result["paused"]=paused(pawn);
        auto* pc=read<UObject*>(pawn,L"Controller");
        if(pc) {
            result["cursor_visible"]=cursor_property(pc)->GetPropertyValueInContainer(pc);
            result["controller_full_tick_when_paused"]=full_tick(pc)->GetPropertyValueInContainer(pc);
        }
        Call animation(read<UObject*>(pawn,L"Mesh"),L"GetAnimInstance",1); animation.run();
        auto* instance=animation.get<UObject*>();
        result["gameplay_animation"]=instance?narrow(instance->GetPathName()):"none";
        result["player"]=narrow(pawn->GetPathName());
    }
    return result;
}
}

namespace css {
namespace {
void update_dye_mips(UObject* target) {
    // The Canvas path does not regenerate lower mip levels. Resolve the same
    // engine method called by the verified CreateRenderTarget2D native wrapper.
    // Only engine code is queued on the render thread, never a CSS callback.
    auto* module=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    constexpr uintptr_t wrapper=0x3f28b70, callsite=0x3f28e5a, update=0x44c85d0;
    constexpr std::array<unsigned char,32> prologue{0x40,0x53,0x57,0x48,0x81,0xec,0xa8,0,0,0,0x48,0x8b,0x05,0x9f,0xd4,0xbd,0x06,0x48,0x33,0xc4,0x48,0x89,0x84,0x24,0x80,0,0,0,0x0f,0xb6,0xda,0x48};
    constexpr std::array<unsigned char,10> caller{0xb2,0x01,0x48,0x8b,0xcb,0xe8,0x6c,0xf7,0x59,0};
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) throw std::runtime_error("Unknown game image for dye mipmaps");
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(module+dos->e_lfanew);
    auto* function=find(L"/Script/Engine.Default__KismetRenderingLibrary")->GetFunctionByNameInChain(L"CreateRenderTarget2D");
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage<update+prologue.size() || !function ||
       reinterpret_cast<const unsigned char*>(function->GetFuncPtr())!=module+wrapper ||
       std::memcmp(module+callsite,caller.data(),caller.size()) || std::memcmp(module+update,prologue.data(),prologue.size()))
        throw std::runtime_error("Dye mipmaps require the verified Mortal Shell II build");
    if(!target->IsA(static_cast<UClass*>(find(L"/Script/Engine.TextureRenderTarget2D")))) throw std::runtime_error("Invalid dye render target");
    reinterpret_cast<void(*)(UObject*,bool)>(const_cast<unsigned char*>(module)+update)(target,false);
}
}
void Appearance::reset_colors() {
    if(auto* component=component_.Get()) for(const auto& [slot,weak]:color_mids_) {
        if(auto* mid=weak.Get()) {
            Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",slot); current.run();
            if(current.get<UObject*>()==mid) material(component,slot,applied_materials_.contains(slot)?read<UObject*>(mid,L"Parent"):nullptr);
        }
    }
    color_mids_.clear(); color_targets_.clear(); color_textures_.clear(); last_colors_.clear(); color_outfit_.clear();
}
void Appearance::customize(const Outfit& outfit,const Customization& custom) {
    auto values=color_values(outfit.colors,custom);
    auto* component=component_.Get();
    if(!component || !applied_.Get() || mesh_asset(component)!=applied_.Get()) throw std::runtime_error("Appearance changed before colors could apply");
    if(color_outfit_!=outfit.id) reset_colors();
    // Dropping a control restores its authored value, including layered parameters.
    // Rebuild from the original material rather than guessing a layer's default.
    if(std::any_of(last_colors_.begin(),last_colors_.end(),[&](const auto& p){return !values.contains(p.first);})) reset_colors();
    if(values.empty()) { reset_colors(); material_debug=material_snapshot(component,applied_.Get()); material_debug["colors"]=Json::object(); material_debug["dye_targets"]=0; return; }
    if(values==last_colors_) return;
    auto mid_for=[&](int index) {
        Call count(component,L"GetNumMaterials",1); count.run();
        if(index<0 || index>=count.get<int>()) throw std::runtime_error("Color slot is absent on this appearance");
        auto& weak=color_mids_[index];
        if(auto* mid=weak.Get()) return mid;
        Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",index); current.run();
        auto* parent=current.get<UObject*>();
        if(!parent || parent->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInstanceDynamic"))))
            throw std::runtime_error("Wait for the temporary material effect to finish before coloring");
        Call make(component,L"CreateDynamicMaterialInstance",4); make.set(L"ElementIndex",index); make.set(L"SourceMaterial",parent); make.run();
        auto* mid=make.get<UObject*>(); if(!mid) throw std::runtime_error("Could not create the color material");
        weak=mid; return mid;
    };
    try {
        auto* library=find(L"/Script/Engine.Default__KismetRenderingLibrary");
        for(const auto& surface:outfit.colors.surfaces) {
            bool active=false,changed=false;
            for(const auto& [id,file]:surface.layers) {
                active|=values.contains(id);
                changed|=values.contains(id)!=last_colors_.contains(id) || (values.contains(id) && last_colors_.contains(id) && values.at(id)!=last_colors_.at(id));
            }
            if(!changed) continue;
            auto parameter=FName(wide(surface.parameter).c_str(),FNAME_Add);
            auto* first=mid_for(surface.slots.front());
            Call base(read<UObject*>(first,L"Parent"),L"K2_GetTextureParameterValue",2); base.set(L"ParameterName",parameter); base.run();
            auto* original=base.get<UObject*>();
            if(!original) throw std::runtime_error("The selected material has no dyeable base texture");
            UObject* target=original;
            if(active) {
                auto& weak=color_targets_[surface.id]; target=weak.Get();
                if(!target) {
                    Call create(library,L"CreateRenderTarget2D",8); create.set(L"WorldContextObject",component);
                    create.set(L"Width",surface.resolution); create.set(L"Height",surface.resolution); create.set(L"Format",uint8_t{3});
                    create.set(L"bAutoGenerateMipMaps",true); create.run(); target=create.get<UObject*>();
                    if(!target) throw std::runtime_error("Could not create the dye texture"); weak=target;
                }
                // Keep the render target referenced by the component before importing layer textures.
                for(int slot:surface.slots) {
                    Call set(mid_for(slot),L"SetTextureParameterValue",2); set.set(L"ParameterName",parameter); set.set(L"Value",target); set.run();
                }
                std::vector<std::pair<FWeakObjectPtr,ColorValue>> layers;
                for(const auto& [id,file]:surface.layers) if(values.contains(id)) {
                    auto& texture=color_textures_[file];
                    if(!texture.Get()) {
                        auto path=outfit.resources/file;
                        if(!fs::is_regular_file(path)) throw std::runtime_error("The outfit's color mask is missing");
                        Call import(library,L"ImportFileAsTexture2D",3); import.set(L"WorldContextObject",component); import.set(L"Filename",FString(path.c_str())); import.run();
                        auto* loaded=import.get<UObject*>(); if(!loaded) throw std::runtime_error("Could not load the outfit's color mask"); texture=loaded;
                    }
                    auto color=values.at(id); for(int i=0;i<3;++i) color[i]=srgb_linear(color[i]);
                    layers.emplace_back(texture,color);
                }
                for(const auto& [texture,color]:layers) if(!texture.Get()) throw std::runtime_error("Color textures changed while loading");
                Call begin(library,L"BeginDrawCanvasToRenderTarget",5); begin.set(L"WorldContextObject",component); begin.set(L"TextureRenderTarget",target); begin.run();
                auto end=[&] { Call finish(library,L"EndDrawCanvasToRenderTarget",2); finish.set(L"WorldContextObject",component); finish.copy(L"Context",begin,L"Context"); finish.run(); };
                try {
                    auto* canvas=begin.get<UObject*>(L"Canvas");
                    auto draw=[&](UObject* texture,const ColorValue& color,uint8_t blend) {
                        Call call(canvas,L"K2_DrawTexture",9); call.set(L"RenderTexture",texture);
                        call.set(L"ScreenSize",Vec2{double(surface.resolution),double(surface.resolution)}); call.set(L"CoordinateSize",Vec2{1,1});
                        call.set(L"RenderColor",color); call.set(L"BlendMode",blend); call.run();
                    };
                    draw(original,{1,1,1,1},0);
                    for(const auto& [texture,color]:layers) draw(texture.Get(),color,2);
                } catch(...) { end(); throw; }
                end();
                update_dye_mips(target);
            }
            for(int slot:surface.slots) {
                auto* mid=mid_for(slot);
                Call set(mid,L"SetTextureParameterValue",2); set.set(L"ParameterName",parameter); set.set(L"Value",target); set.run();
                Call readback(mid,L"K2_GetTextureParameterValue",2); readback.set(L"ParameterName",parameter); readback.run();
                if(readback.get<UObject*>()!=target) throw std::runtime_error("Dye texture read-back failed");
            }
        }
        for(const auto& control:outfit.colors.controls) {
            bool active=values.contains(control.id),previous=last_colors_.contains(control.id);
            if(!active) continue;
            if(active && previous && values.at(control.id)==last_colors_.at(control.id)) continue;
            for(const auto& binding:control.bindings) {
                auto* mid=mid_for(binding.slot);
                auto color=active?values.at(control.id):control.value;
                auto parameter=FName(wide(binding.parameter).c_str(),FNAME_Add);
                // Explicit layer associations use the engine's reflected FMaterialParameterInfo.
                Call set(mid,control.scalar?L"SetScalarParameterValueByInfo":L"SetVectorParameterValueByInfo",2);
                auto* p=set.param(L"ParameterInfo"); auto* info=find(L"/Script/Engine.MaterialParameterInfo");
                member(set.data(p),p->GetElementSize(),info,L"Name",parameter);
                member(set.data(p),p->GetElementSize(),info,L"Association",uint8_t(binding.association));
                member(set.data(p),p->GetElementSize(),info,L"Index",binding.layer);
                if(!control.scalar) for(int i=0;i<3;++i) color[i]=srgb_linear(color[i]);
                if(control.scalar) set.set(L"Value",color[0]); else set.set(L"Value",color);
                set.run();
                Call readback(mid,control.scalar?L"K2_GetScalarParameterValueByInfo":L"K2_GetVectorParameterValueByInfo",2);
                readback.copy(L"ParameterInfo",set,L"ParameterInfo"); readback.run();
                if(control.scalar ? std::abs(readback.get<float>()-color[0])>.00001f : readback.get<ColorValue>()!=color)
                    throw std::runtime_error("Color parameter read-back failed");
            }
        }
        last_colors_=std::move(values); color_outfit_=outfit.id;
        material_debug=material_snapshot(component,applied_.Get());
        material_debug["colors"]=last_colors_;
        material_debug["dye_targets"]=color_targets_.size();
    } catch(...) { reset_colors(); throw; }
}
void Wardrobe::sync_materials() {
    auto* preview=preview_mesh_.Get(); auto* pawn=pawn_.Get();
    if(!preview || !pawn) return;
    auto* source=read<UObject*>(pawn,L"Mesh");
    if(mesh_asset(preview)!=mesh_asset(source)) return;
    auto values=overrides(source);
    int count=std::max(values.Num(),overrides(preview).Num());
    for(int i=0;i<count;++i) {
        UObject* value{}; if(i<values.Num()) std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        material(preview,i,value);
    }
}
}

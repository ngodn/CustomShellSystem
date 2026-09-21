#include "engine.hpp"
#include <windows.h>
#include <unordered_map>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>

namespace cssx::engine {
std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!size) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(size,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),size);
    return result;
}
std::string narrow(const std::wstring& s) {
    if(s.empty()) return {};
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(!size) throw std::runtime_error("Invalid Unicode text");
    std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),size,nullptr,nullptr);
    return result;
}
// No path cache here: a cached weak reference to a reflected path crashed the
// game within a second (2026-09-22 live bisect). StaticFindObject per call it is.
UObject* find_optional(const wchar_t* path) { return UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path); }
void set_find_cache(bool) {}
UObject* find(const wchar_t* path) {
    auto* object=find_optional(path);
    if(!object) throw std::runtime_error("Required reflected object is missing: "+narrow(path));
    return object;
}
FProperty* field(UObject* object,const wchar_t* name,size_t size) {
    if(!object) throw std::runtime_error("No live object for "+narrow(name));
    auto* property=object->GetPropertyByNameInChain(name);
    if(!property || property->GetElementSize()!=static_cast<int32_t>(size) || property->GetArrayDim()!=1)
        throw std::runtime_error("Reflected property layout does not match: "+narrow(name));
    return property;
}
UObject* object_of(UObject* object,const wchar_t* name) {
    if(!object) return nullptr;
    auto* p=object->GetPropertyByNameInChain(name);
    if(!p || !p->IsA<FObjectProperty>() || p->GetElementSize()!=sizeof(UObject*)) return nullptr;
    return read<UObject*>(object,name);
}
bool bool_of(UObject* object,const wchar_t* name,bool fallback) {
    auto* p=object?object->GetPropertyByNameInChain(name):nullptr;
    if(!p || !p->IsA<FBoolProperty>()) return fallback;
    return static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(object);
}
WeakObject::WeakObject(UObject* object) { *this=object; }
WeakObject& WeakObject::operator=(UObject* object) {
    if(object) {
        auto* item=FUObjectArray::IndexToObject(object->GetInternalIndex());
        if(!item || item->GetUObject()!=object) throw std::runtime_error("Object is absent from the live object array");
        if(!item->GetSerialNumber()) {
            Call serial(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"Conv_ObjectToSoftObjectReference",2);
            serial.set(L"Object",object); serial.run();
            if(!item->GetSerialNumber()) throw std::runtime_error("Engine did not initialize the object serial");
        }
    }
    FWeakObjectPtr::operator=(object);
    return *this;
}
Call::Call(UObject* object,const wchar_t* name,unsigned count):object_(object) {
    if(!object) throw std::runtime_error("No target for reflected call "+narrow(name));
    function_=object->GetFunctionByNameInChain(name);
    if(!function_ || function_->GetNumParms()!=count || function_->GetParmsSize()>bytes_.size())
        throw std::runtime_error("Reflected function signature mismatch: "+narrow(name));
    for(auto* p:function_->ForEachProperty()) {
        if(!p->HasAnyPropertyFlags(CPF_Parm)) continue;
        if(p->GetOffset_Internal()<0 || p->GetArrayDim()!=1 || p->GetOffset_Internal()+p->GetElementSize()>function_->GetParmsSize())
            throw std::runtime_error("Parameter exceeds reflected frame: "+narrow(name));
        properties_.push_back(p);
    }
    if(properties_.size()!=count) throw std::runtime_error("Parameter enumeration mismatch: "+narrow(name));
    for(auto* p:properties_) p->InitializeValue(bytes_.data()+p->GetOffset_Internal());
}
Call::~Call() { for(auto* p:properties_) p->DestroyValue(bytes_.data()+p->GetOffset_Internal()); }
FProperty* Call::param(const wchar_t* name) {
    for(auto* p:properties_) if(p->GetName()==name) return p;
    throw std::runtime_error("Missing parameter: "+narrow(name));
}
void Call::copy(const wchar_t* name,Call& other,const wchar_t* other_name) {
    auto* p=param(name); auto* q=other.param(other_name);
    if(p->GetElementSize()!=q->GetElementSize() || !p->SameType(q)) throw std::runtime_error("Parameter type mismatch: "+narrow(name));
    p->CopyCompleteValue(data(p),other.data(q));
}
void Call::run() { object_->ProcessEvent(function_,bytes_.data()); }

namespace { std::unordered_map<std::string,WeakObject> s_asset_cache; }
UObject* load(const std::string& path) {
    if(auto it=s_asset_cache.find(path);it!=s_asset_cache.end()) if(auto* cached=it->second.Get()) return cached;
    auto name=wide(path);
    if(auto* object=find_optional(name.c_str())) { s_asset_cache[path]=object; return object; }
    auto* kismet=find(L"/Script/Engine.Default__KismetSystemLibrary");
    Call make(kismet,L"MakeSoftObjectPath",2);
    FString string(name.c_str()); make.set(L"PathString",string); make.run();
    Call convert(kismet,L"Conv_SoftObjPathToSoftObjRef",2);
    convert.copy(L"SoftObjectPath",make,L"ReturnValue"); convert.run();
    Call loading(kismet,L"LoadAsset_Blocking",2);
    loading.copy(L"Asset",convert,L"ReturnValue"); loading.run();
    auto* object=loading.get<UObject*>();
    if(!object) throw std::runtime_error("Asset could not load: "+path);
    s_asset_cache[path]=object;
    return object;
}
void invoke(UObject* object,const wchar_t* fn) { Call c(object,fn,0); c.run(); }
UObject* construct_class(UClass* cls,UObject* outer) {
    if(!cls) throw std::runtime_error("Widget class is null");
    FStaticConstructObjectParameters params(cls,outer);
    auto* object=UObjectGlobals::StaticConstructObject(params);
    if(!object) throw std::runtime_error("Could not construct object");
    return object;
}
UObject* construct(const wchar_t* type,UObject* outer) { return construct_class(static_cast<UClass*>(find(type)),outer); }
void object_property(UObject* object,const wchar_t* name,UObject* value) {
    auto* p=field(object,name,sizeof(UObject*));
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal(),&value);
}
void text_value(UObject* widget,const std::string& text) {
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
    FString value(wide(text).c_str()); convert.set(L"InString",value); convert.run();
    Call set(widget,L"SetText",1); set.copy(L"InText",convert,L"ReturnValue"); set.run();
}
std::string text_of(UObject* widget,int limit) {
    if(!widget) return {};
    Call text(widget,L"GetText",1); text.run();
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
    convert.copy(L"InText",text,L"ReturnValue"); convert.run();
    const auto& value=*static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars=value.GetCharArray();
    if(chars.Num()>limit) throw std::runtime_error("Text is too long");
    return chars.Num()?narrow(std::wstring(chars.GetData())):std::string{};
}
void font_size(UObject* widget,float size,UObject* font_object) {
    auto* font=widget->GetPropertyByNameInChain(L"Font");
    auto* info=find(L"/Script/SlateCore.SlateFontInfo");
    auto* size_field=field(info,L"Size",sizeof(float));
    Call set(widget,L"SetFont",1);
    auto* param=set.param(L"InFontInfo");
    if(!font || !font->SameType(param) || size_field->GetOffset_Internal()<0 || static_cast<size_t>(size_field->GetOffset_Internal())+sizeof(float)>static_cast<size_t>(param->GetElementSize()))
        throw std::runtime_error("Font layout mismatch");
    param->CopyCompleteValue(set.data(param),reinterpret_cast<std::byte*>(widget)+font->GetOffset_Internal());
    std::memcpy(static_cast<std::byte*>(set.data(param))+size_field->GetOffset_Internal(),&size,sizeof(size));
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
UObject* content(UObject* parent,UObject* child) { Call c(parent,L"SetContent",2); c.set(L"content",child); c.run(); return c.get<UObject*>(); }
std::vector<UObject*> children(UObject* panel,int limit) {
    Call count(panel,L"GetChildrenCount",1); count.run(); auto n=count.get<int32_t>();
    if(n<0 || n>limit) throw std::runtime_error("Child count out of bounds");
    std::vector<UObject*> result;
    for(int i=0;i<n;++i) { Call get(panel,L"GetChildAt",2); get.set(L"Index",i); get.run(); result.push_back(get.get<UObject*>()); }
    return result;
}
UObject* create_widget(UObject* pc,UClass* type) {
    Call c(find(L"/Script/UMG.Default__WidgetBlueprintLibrary"),L"Create",4);
    c.set(L"WorldContextObject",pc); c.set(L"WidgetType",type); c.set(L"OwningPlayer",pc); c.run();
    auto* widget=c.get<UObject*>();
    if(!widget) throw std::runtime_error("Widget creation failed");
    return widget;
}
PlayerContext player_context(void* engine) {
    PlayerContext out;
    if(!engine) return out;
    auto* viewport=object_of(static_cast<UObject*>(engine),L"GameViewport");
    if(!viewport) return out;
    out.world=object_of(viewport,L"World");
    if(!out.world) return out;
    Call pc(find(L"/Script/Engine.Default__GameplayStatics"),L"GetPlayerController",3);
    pc.set(L"WorldContextObject",out.world); pc.set(L"PlayerIndex",int32_t{0}); pc.run();
    out.pc=pc.get<UObject*>();
    if(out.pc && WeakObject(out.pc).Get()!=out.pc) out.pc=nullptr;
    if(out.pc) out.pawn=object_of(out.pc,L"Pawn");
    return out;
}
std::vector<UObject*> Layout::on(UObject* target) const { std::vector<UObject*> r; for(const auto& p:placed) if(p.canvas==target) r.push_back(p.widget); return r; }
double Layout::extent_of(UObject* target) const { double bottom=0; for(const auto& p:placed) if(p.canvas==target) bottom=std::max(bottom,p.y+p.h); return bottom; }
UObject* Layout::place(UObject* widget,double x,double y,double w,double h) {
    Call add(canvas,L"AddChildToCanvas",2); add.set(L"content",widget); add.run();
    auto* slot=add.get<UObject*>();
    invoke(slot,L"SetPosition",L"InPosition",Vec2{(x-origin_x)*scale,(y-origin_y)*scale});
    invoke(slot,L"SetSize",L"InSize",Vec2{w*scale,h*scale});
    placed.push_back({widget,canvas,x-origin_x,y-origin_y,w,h});
    return slot;
}
UObject* Layout::box(double x,double y,double w,double h,Color color) {
    auto* widget=construct(L"/Script/UMG.Border",tree);
    invoke(widget,L"SetBrushColor",L"InBrushColor",color);
    invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{4});   // SelfHitTestInvisible
    place(widget,x,y,w,h); return widget;
}
UObject* Layout::label(const std::string& text,double x,double y,double w,double h,float size,Color color,bool title,uint8_t justify) {
    auto* widget=construct(L"/Script/UMG.TextBlock",tree);
    text_value(widget,text); font_size(widget,size*static_cast<float>(scale),title?title_font:serif);
    invoke(widget,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{color});
    invoke(widget,L"SetAutoWrapText",L"InAutoTextWrap",h>size*2.2);
    invoke(widget,L"SetJustification",L"InJustification",justify);
    invoke(widget,L"SetClipping",L"InClipping",uint8_t{1});
    invoke(widget,L"SetTextOverflowPolicy",L"InOverflowPolicy",uint8_t{1});
    invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3});   // HitTestInvisible
    place(widget,x,y,w,h); return widget;
}
UObject* Layout::button(const std::string& text,double x,double y,double w,double h,bool active,bool enabled,float size,Color ink) {
    auto* widget=construct(L"/Script/UMG.Button",tree);
    flat_button(widget,active);
    auto* focusable=widget->GetPropertyByNameInChain(L"IsFocusable");
    if(!focusable || !focusable->IsA<FBoolProperty>()) throw std::runtime_error("Button focus property mismatch");
    static_cast<FBoolProperty*>(focusable)->SetPropertyValueInContainer(widget,false);
    invoke(widget,L"SetBackgroundColor",L"InBackgroundColor",Color{1,1,1,1});
    auto* label=construct(L"/Script/UMG.TextBlock",tree);
    text_value(label,text); font_size(label,size*static_cast<float>(scale),serif);
    invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{ink});
    invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
    invoke(label,L"SetClipping",L"InClipping",uint8_t{1});
    invoke(label,L"SetTextOverflowPolicy",L"InOverflowPolicy",uint8_t{1});
    content(widget,label);
    invoke(widget,L"SetIsEnabled",L"bInIsEnabled",enabled);
    place(widget,x,y,w,h); return widget;
}
UObject* Layout::image(UObject* texture,double x,double y,double w,double h,float opacity,UVRect uv) {
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
}

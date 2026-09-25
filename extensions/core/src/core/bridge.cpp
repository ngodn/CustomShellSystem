#include "bridge.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FNumericProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FMapProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>

namespace cssx {
using namespace engine;
namespace {
FProperty* property_in(UStruct* type,const std::wstring& name) {
    // GetPropertyByNameInChain compares only the FName base index; Blueprint
    // fields such as SpringBone_1 and SpringBone_2 need the number too.
    const FName wanted(name.c_str());
    unsigned depth=0;
    for(;type;type=type->GetSuperStruct()) {
        if(++depth>64) throw std::runtime_error("Class hierarchy exceeds bound");
        for(auto* field:type->ForEachProperty()) if(field->GetFName()==wanted) return field;
    }
    return nullptr;
}
struct MapView {
    FProperty* key;FProperty* value;FScriptMap* map;const FScriptMapLayout& layout;
    int count,end;
    MapView(FMapProperty* p,void* data):key(p->GetKeyProp()),value(p->GetValueProp()),map(static_cast<FScriptMap*>(data)),layout(p->GetMapLayout()) {
        if(p->GetArrayDim()!=1 || p->GetElementSize()!=sizeof(FScriptMap) || !key || !value ||
           key->GetSize()<=0 || value->GetSize()<=0 || layout.ValueOffset<key->GetSize() ||
           layout.SetLayout.Size<=0 || layout.SetLayout.Size>65536 || layout.ValueOffset>layout.SetLayout.Size-value->GetSize())
            throw std::runtime_error("Map layout is unsupported");
        count=map->Num();end=map->GetMaxIndex();
        if(count<0 || end<count || end>4096) throw std::runtime_error("Map exceeds bound");
    }
    std::byte* pair(int index) {return static_cast<std::byte*>(map->GetData(index,layout));}
};
struct Value {
    FProperty* property;std::vector<std::max_align_t> storage;
    explicit Value(FProperty* p):property(p) {
        const auto size=p->GetSize(),alignment=p->GetMinAlignment();
        if(size<=0 || size>65536 || alignment<=0 || alignment>int(alignof(std::max_align_t)) || (alignment&(alignment-1)))
            throw std::runtime_error("Property exceeds write buffer bounds");
        storage.resize((size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        property->InitializeValue(storage.data());
    }
    ~Value(){property->DestroyValue(storage.data());}
    Value(const Value&)=delete;
    void* data(){return storage.data();}
};
bool player_matches(UObject* pawn,UObject* controller) {
    return pawn && controller && object_of(pawn,L"Controller")==controller && object_of(controller,L"Pawn")==pawn;
}
bool seal_matches(UObject* controller,const std::wstring& seal) {
    if(seal.empty()) return true;
    auto* handle=controller->GetPropertyByNameInChain(L"ActiveSealItemHandle");
    if(!handle || handle->GetArrayDim()!=1) return false;
    auto* base=reinterpret_cast<std::byte*>(controller)+handle->GetOffset_Internal();
    FProperty* item=nullptr;
    if(handle->IsA<FObjectProperty>()) {
        auto* instance=static_cast<FObjectProperty*>(handle)->GetObjectPropertyValue(base);
        if(!instance) return false;
        item=instance->GetPropertyByNameInChain(L"ItemDef");base=reinterpret_cast<std::byte*>(instance);
    } else if(handle->IsA<FStructProperty>()) {
        auto* type=static_cast<FStructProperty*>(handle)->GetStruct().Get();
        item=type?type->GetPropertyByNameInChain(L"ItemDef"):nullptr;
        if(item && (item->GetOffset_Internal()<0 || item->GetOffset_Internal()+item->GetSize()>handle->GetElementSize())) return false;
    }
    if(!item || !item->IsA<FObjectProperty>() || item->GetArrayDim()!=1) return false;
    auto* object=static_cast<FObjectProperty*>(item)->GetObjectPropertyValue(base+item->GetOffset_Internal());
    return object && (object->GetName()==seal || object->GetClassPrivate()->GetName()==seal);
}
}
struct Bridge::HookGroup {
    struct Rule {
        WeakObject target,pawn,controller;
        FBoolProperty* result=nullptr;
        FObjectProperty* owner=nullptr;
        bool value=false,enabled=true,busy=false,failed=false;
        std::wstring after,seal;
        std::string extension;
        uint64_t calls=0;
    };
    uint64_t token=0;
    std::map<uint64_t,Rule> rules;
};
Bridge::Bridge(const CssxHookHost* hooks):hooks_(hooks && hooks->abi==1 && hooks->size>=sizeof(CssxHookHost) && hooks->add && hooks->remove?hooks:nullptr) {}
Bridge::~Bridge()=default;
Json Bridge::handle(UObject* object) {
    if(!object) return nullptr;
    // Sweep dead handles occasionally so the reverse index cannot hand back an
    // id whose object was replaced at the same address.
    if(++sweep_%256==0) for(auto it=objects_.begin();it!=objects_.end();) {
        if(!it->second.weak.Get()) { for(auto r=reverse_.begin();r!=reverse_.end();++r) if(r->second==it->first) { reverse_.erase(r); break; } it=objects_.erase(it); }
        else ++it;
    }
    if(auto found=reverse_.find(object);found!=reverse_.end()) {
        auto it=objects_.find(found->second);
        if(it!=objects_.end() && it->second.weak.Get()==object)
            return {{"$object",found->second},{"name",narrow(object->GetFullName())},{"class",narrow(object->GetClassPrivate()->GetFullName())}};
        if(it!=objects_.end()) objects_.erase(it);
        reverse_.erase(found);
    }
    if(objects_.size()>=8192) throw std::runtime_error("Object handle limit reached");
    const auto id=next_++;
    objects_.emplace(id,Tracked{WeakObject(object)});
    reverse_[object]=id;
    return {{"$object",id},{"name",narrow(object->GetFullName())},{"class",narrow(object->GetClassPrivate()->GetFullName())}};
}
uint64_t Bridge::track(UObject* object) { auto h=handle(object); return h.is_object()?h.value("$object",uint64_t{0}):0; }
UObject* Bridge::resolve(const Json& value) {
    if(value.is_null()) return nullptr;
    if(!value.is_object() || !value.contains("$object")) throw std::runtime_error("Expected an object handle");
    auto it=objects_.find(value.at("$object").get<uint64_t>());
    auto* result=it==objects_.end()?nullptr:it->second.weak.Get();
    if(!result) throw std::runtime_error("Object handle expired");
    return result;
}
Json Bridge::decode(FProperty* p,void* data,unsigned depth) {
    if(depth>12 || p->GetArrayDim()!=1 || p->GetElementSize()<0) throw std::runtime_error("Unsupported property shape");
    if(p->IsA<FEnumProperty>()) {
        auto* underlying=static_cast<FEnumProperty*>(p)->GetUnderlyingProperty();
        if(!underlying || underlying->GetElementSize()!=p->GetElementSize()) throw std::runtime_error("Enum storage mismatch");
        return decode(underlying,data,depth+1);
    }
    if(p->IsA<FBoolProperty>()) return static_cast<FBoolProperty*>(p)->GetPropertyValue(data);
    if(p->IsA<FNumericProperty>()) {
        auto* number=static_cast<FNumericProperty*>(p);
        if(number->IsFloatingPoint()) return number->GetFloatingPointPropertyValue(data);
        return number->GetSignedIntPropertyValue(data);
    }
    if(p->IsA<FObjectProperty>()) return handle(static_cast<FObjectProperty*>(p)->GetObjectPropertyValue(data));
    if(p->IsA<FNameProperty>()) return narrow(static_cast<FName*>(data)->ToString());
    if(p->IsA<FStrProperty>()) {
        const auto& chars=static_cast<FString*>(data)->GetCharArray();
        if(chars.Num()<0 || chars.Num()>65536) throw std::runtime_error("FString exceeds bound");
        return chars.Num()?narrow(std::wstring(chars.GetData(),chars.Num()-1)):std::string{};
    }
    if(p->IsA<FTextProperty>()) {
        Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
        auto* input=convert.param(L"InText");
        if(!input->SameType(p) || input->GetElementSize()!=p->GetElementSize()) throw std::runtime_error("Localized text layout is unsupported");
        input->CopyCompleteValue(convert.data(input),data);convert.run();
        auto* result=convert.param(L"ReturnValue");return decode(result,convert.data(result),depth+1);
    }
    if(p->IsA<FStructProperty>()) {
        auto* type=static_cast<FStructProperty*>(p)->GetStruct().Get();Json result=Json::object();
        size_t count=0;
        for(auto* field:type->ForEachProperty()) {
            if(++count>256 || field->GetOffset_Internal()<0 || field->GetOffset_Internal()+field->GetSize()>p->GetElementSize()) throw std::runtime_error("Struct field exceeds bounds");
            try {result[narrow(field->GetName())]=decode(field,static_cast<std::byte*>(data)+field->GetOffset_Internal(),depth+1);}
            catch(const std::exception&) {result[narrow(field->GetName())]={{"$unsupported",true}};}
        }
        return result;
    }
    if(p->IsA<FArrayProperty>()) {
        auto* a=static_cast<FArrayProperty*>(p);FScriptArrayHelper array(a,data);
        if(array.Num()<0 || array.Num()>4096) throw std::runtime_error("Array exceeds bound");
        Json result=Json::array();for(int i=0;i<array.Num();++i) result.push_back(decode(a->GetInner(),array.GetRawPtr(i),depth+1));return result;
    }
    if(p->IsA<FMapProperty>()) {
        MapView view(static_cast<FMapProperty*>(p),data);
        Json entries=Json::array();
        for(int index=0;index<view.end;++index) if(view.map->IsValidIndex(index)) {
            auto* pair=view.pair(index);
            entries.push_back({{"key",decode(view.key,pair,depth+1)},{"value",decode(view.value,pair+view.layout.ValueOffset,depth+1)}});
        }
        if(entries.size()!=size_t(view.count)) throw std::runtime_error("Map count changed during read");
        return {{"$map",std::move(entries)}};
    }
    throw std::runtime_error("Unsupported reflected property type");
}
void Bridge::encode(FProperty* p,void* data,const Json& value,unsigned depth) {
    if(depth>12 || p->GetArrayDim()!=1) throw std::runtime_error("Unsupported property shape");
    if(value.is_object() && value.contains("$table_field")) {
        const auto& source=value.at("$table_field");
        auto* object=resolve(source.at("table"));
        if(!object || !object->IsA<UDataTable>()) throw std::runtime_error("Typed source is not a DataTable");
        auto* table=static_cast<UDataTable*>(object);auto* type=table->GetRowStruct().Get();
        if(!type) throw std::runtime_error("Table row type is unavailable");
        const auto row_name=source.at("row").get<std::string>(),field_name=source.at("field").get<std::string>();
        for(const auto* name:{&row_name,&field_name}) if(name->empty() || name->size()>256 || name->find('\0')!=std::string::npos) throw std::runtime_error("Table row or field name is invalid");
        const auto row=wide(row_name),field=wide(field_name);
        auto* property=property_in(type,field);
        if(!property || property->GetOffset_Internal()<0 || property->GetOffset_Internal()+property->GetSize()>type->GetPropertiesSize() || !p->SameType(property))
            throw std::runtime_error("Table field type does not match the parameter");
        const auto& rows=table->GetRowMap();if(rows.Num()<0 || rows.Num()>4096) throw std::runtime_error("Table exceeds bound");
        auto* found=rows.Find(FName(row.c_str()));if(!found || !*found) throw std::runtime_error("Table row disappeared");
        p->CopyCompleteValue(data,*found+property->GetOffset_Internal());return;
    }
    if(p->IsA<FEnumProperty>()) {
        auto* underlying=static_cast<FEnumProperty*>(p)->GetUnderlyingProperty();
        if(!underlying || underlying->GetElementSize()!=p->GetElementSize()) throw std::runtime_error("Enum storage mismatch");
        encode(underlying,data,value,depth+1);return;
    }
    if(p->IsA<FBoolProperty>()) {static_cast<FBoolProperty*>(p)->SetPropertyValue(data,value.get<bool>());return;}
    if(p->IsA<FNumericProperty>()) {
        auto* number=static_cast<FNumericProperty*>(p);
        if(!value.is_number()) throw std::runtime_error("Numeric input required");
        if(number->IsFloatingPoint()) {auto v=value.get<double>();if(!std::isfinite(v)) throw std::runtime_error("Numeric value is not finite");number->SetFloatingPointPropertyValue(data,v);}
        else {
            if(!value.is_number_integer()) throw std::runtime_error("Integer input required");
            const auto size=p->GetElementSize();
            if(size!=1 && size!=2 && size!=4 && size!=8) throw std::runtime_error("Unsupported integer width");
            const bool is_unsigned=p->IsA<FByteProperty>() || p->IsA<FUInt16Property>() || p->IsA<FUInt32Property>() || p->IsA<FUInt64Property>();
            if(is_unsigned) {
                if(!value.is_number_unsigned() && value.get<int64_t>()<0) throw std::runtime_error("Unsigned input is negative");
                auto v=value.get<uint64_t>();if(size<8 && v>=(uint64_t{1}<<(size*8))) throw std::runtime_error("Integer exceeds field range");
                number->SetIntPropertyValue(data,static_cast<uint64>(v));
            } else {
                if(value.is_number_unsigned() && value.get<uint64_t>()>uint64_t(INT64_MAX)) throw std::runtime_error("Integer exceeds field range");
                auto v=value.get<int64_t>();if(size<8 && (v<-(int64_t{1}<<(size*8-1)) || v>=(int64_t{1}<<(size*8-1)))) throw std::runtime_error("Integer exceeds field range");
                number->SetIntPropertyValue(data,static_cast<int64>(v));
            }
        }
        return;
    }
    if(p->IsA<FObjectProperty>()) {
        auto* property=static_cast<FObjectProperty*>(p);auto* object=resolve(value);
        if(object && !object->IsA(property->GetPropertyClass().Get())) throw std::runtime_error("Object class does not match the property");
        if(p->GetElementSize()!=sizeof(object)) throw std::runtime_error("Object property size mismatch");
        p->CopyCompleteValue(data,&object);return;
    }
    if(p->IsA<FNameProperty>()) {auto text=wide(value.get<std::string>());FName name(text.c_str());p->CopyCompleteValue(data,&name);return;}
    if(p->IsA<FStrProperty>()) {auto text=wide(value.get<std::string>());FString s(text.c_str());p->CopyCompleteValue(data,&s);return;}
    if(p->IsA<FTextProperty>()) {
        // Build an FText from a plain string through the same Kismet path decode uses in
        // reverse. The result is a runtime (non-localized) text, which is what a mod that
        // sets UI copy wants.
        Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
        auto* input=convert.param(L"InString");
        if(!input->IsA<FStrProperty>()) throw std::runtime_error("Localized text conversion is unsupported");
        auto text=wide(value.get<std::string>());FString s(text.c_str());input->CopyCompleteValue(convert.data(input),&s);
        convert.run();
        auto* result=convert.param(L"ReturnValue");
        if(!result->SameType(p) || result->GetElementSize()!=p->GetElementSize()) throw std::runtime_error("Localized text layout is unsupported");
        p->CopyCompleteValue(data,convert.data(result));return;
    }
    if(p->IsA<FArrayProperty>()) {
        // Element-wise write into the existing array without changing its length
        // (growing needs the allocator's ResizeAllocation, which the pinned UE4SS
        // import library does not export). Same count only.
        if(!value.is_array()) throw std::runtime_error("Array input required");
        auto* a=static_cast<FArrayProperty*>(p);FScriptArrayHelper array(a,data);
        if(int(value.size())!=array.Num()) throw std::runtime_error("Array length is fixed; write the same number of elements");
        for(size_t i=0;i<value.size();++i) encode(a->GetInner(),array.GetRawPtr(static_cast<int32>(i)),value[i],depth+1);
        return;
    }
    if(p->IsA<FStructProperty>()) {
        if(!value.is_object()) throw std::runtime_error("Struct requires an object");
        auto* type=static_cast<FStructProperty*>(p)->GetStruct().Get();
        for(auto it=value.begin();it!=value.end();++it) {
            auto key=wide(it.key());auto* field=property_in(type,key);
            if(!field || field->GetOffset_Internal()<0 || field->GetOffset_Internal()+field->GetSize()>p->GetElementSize()) throw std::runtime_error("Struct member mismatch: "+it.key());
            encode(field,static_cast<std::byte*>(data)+field->GetOffset_Internal(),it.value(),depth+1);
        }
        return;
    }
    throw std::runtime_error("Cannot write this reflected property type");
}
Json Bridge::request(const PlayerContext& player,const Json& request) {
    const auto op=request.at("op").get<std::string>();
    if(op=="valid") {
        const auto& target=request.at("target");if(target.is_null()) return false;
        if(!target.is_object() || !target.contains("$object")) throw std::runtime_error("Expected an object handle");
        const auto found=objects_.find(target.at("$object").get<uint64_t>());
        return found!=objects_.end() && found->second.weak.Get()!=nullptr;
    }
    if(op.starts_with("hooks.")) return hook_request(request);
    if(op=="player") {
        std::string shell;
        if(player.pawn && player.pawn->GetPropertyByNameInChain(L"CharacterId")) {
            try { shell=narrow(read<FName>(player.pawn,L"CharacterId").ToString()); } catch(...) {}
        }
        return {{"pawn",handle(player.pawn)},{"controller",handle(player.pc)},{"world",handle(player.world)},{"shell",shell}};
    }
    if(op=="class_default") {
        const auto name=request.at("class").get<std::string>();
        if(name.empty() || name.size()>96) throw std::runtime_error("Invalid default class name");
        if(auto it=defaults_.find(name);it!=defaults_.end()) {
            // Class default objects of native classes live for the process; the
            // weak pointer path can report them dead, so validate by object-array slot.
            auto* raw=it->second.raw; auto* item=raw?FUObjectArray::IndexToObject(it->second.index):nullptr;
            if(item && item->GetUObject()==raw) return handle(raw);
            defaults_.erase(it);
        }
        auto key=FName(wide(name).c_str());UObject* match=nullptr;bool duplicate=false;
        UObjectGlobals::ForEachUObject([&](UObject* object,int32,int32) {
            if(object && object->HasAnyFlags(RF_ClassDefaultObject) && object->GetClassPrivate()->GetFName()==key) {
                if(match) {duplicate=true;return RC::LoopAction::Break;}
                match=object;
            }
            return RC::LoopAction::Continue;
        });
        if(duplicate) throw std::runtime_error("Ambiguous default class name; use a full object path");
        if(match) {if(defaults_.size()>=128) throw std::runtime_error("Default-object cache limit reached");defaults_[name]=CachedDefault{match,match->GetInternalIndex()};}
        return handle(match);
    }
    if(op=="find" || op=="load") {
        auto path=request.at("path").get<std::string>();if(path.size()>2048) throw std::runtime_error("Object path exceeds bound");
        if(op=="load") return handle(load(path));
        auto text=wide(path);return handle(find_optional(text.c_str()));
    }
    auto* object=resolve(request.at("target"));if(!object) throw std::runtime_error("Target is null");
    if(op=="input.keys") {
        const auto& keys=request.at("keys");
        if(!keys.is_array() || keys.size()>64) throw std::runtime_error("Input batch exceeds 64 keys");
        std::vector<std::string> names;std::set<std::string> unique;
        for(const auto& key:keys) {
            const auto name=key.get<std::string>();
            if(name.empty() || name.size()>64 || !std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c) || c=='_';}) || !unique.insert(name).second)
                throw std::runtime_error("Invalid or duplicate input key");
            names.push_back(name);
        }
        Json result=Json::object();
        for(const auto& name:names) {
            Call call(object,L"IsInputKeyDown",2);auto* p=call.param(L"Key");
            member(call.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(wide(name).c_str()));
            call.run();result[name]=call.get<bool>();
        }
        return result;
    }
    if(op=="table.rows") {
        if(!object->IsA<UDataTable>()) throw std::runtime_error("Target is not a DataTable");
        const auto& rows=static_cast<UDataTable*>(object)->GetRowMap();
        if(rows.Num()<0 || rows.Num()>4096) throw std::runtime_error("Table exceeds bound");
        Json names=Json::array();for(const auto& entry:rows) names.push_back(narrow(entry.Key.ToString()));
        std::sort(names.begin(),names.end());return names;
    }
    if(op=="get" || op=="set" || op=="map.update") {
        const auto name=wide(request.at("property").get<std::string>());
        auto* type=object->IsA<UStruct>()?static_cast<UStruct*>(object):object->GetClassPrivate();
        auto* p=property_in(type,name);if(!p || p->GetOffset_Internal()<0) throw std::runtime_error("Property is missing: "+request.at("property").get<std::string>());
        auto* data=reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal();
        if(op=="get" && request.value("count",false)) {
            // Shallow: the element count of an array, map or struct-with-Items
            // without decoding it. Used by extensions to detect changes cheaply.
            if(p->IsA<FArrayProperty>()) { FScriptArrayHelper array(static_cast<FArrayProperty*>(p),data); return {{"count",array.Num()}}; }
            if(p->IsA<FMapProperty>()) { MapView view(static_cast<FMapProperty*>(p),data); return {{"count",view.count}}; }
            if(p->IsA<FStructProperty>()) {
                auto* type=static_cast<FStructProperty*>(p)->GetStruct().Get();
                if(auto* items=type?property_in(type,L"Items"):nullptr; items && items->IsA<FArrayProperty>()) { FScriptArrayHelper array(static_cast<FArrayProperty*>(items),data+items->GetOffset_Internal()); return {{"count",array.Num()}}; }
            }
            throw std::runtime_error("count is only available for arrays, maps and structs with an Items array");
        }
        if(op=="map.update") {
            if(object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) throw std::runtime_error("Default-object writes are refused");
            if(!p->IsA<FMapProperty>()) throw std::runtime_error("Property is not a map");
            MapView view(static_cast<FMapProperty*>(p),data);
            std::byte* selected=nullptr;
            for(int i=0;i<view.end;++i) if(view.map->IsValidIndex(i)) {
                auto* pair=view.pair(i);
                if(decode(view.key,pair,0)==request.at("key")) {
                    if(selected) throw std::runtime_error("Map key is ambiguous");
                    selected=pair+view.layout.ValueOffset;
                }
            }
            if(!selected) throw std::runtime_error("Map key no longer exists");
            if(decode(view.value,selected,0)!=request.at("expected")) throw std::runtime_error("Map value changed; refresh before editing");
            if(!view.value->HasAnyPropertyFlags(CPF_IsPlainOldData)) {   // in place, same reason as `set`
                encode(view.value,selected,request.at("value"),0);
                const auto result=decode(view.value,selected,0);
                if(result.dump().size()>1024*1024) throw std::runtime_error("Map result exceeds 1 MiB");
                return result;
            }
            Value next(view.value);view.value->CopyCompleteValue(next.data(),selected);
            encode(view.value,next.data(),request.at("value"),0);
            const auto result=decode(view.value,next.data(),0);
            if(result.dump().size()>1024*1024) throw std::runtime_error("Map result exceeds 1 MiB");
            view.value->CopyCompleteValue(selected,next.data());return result;
        }
        if(op=="set") {
            if(object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) throw std::runtime_error("Default-object writes are refused");
            // Plain data: stage the write in a temporary so a rejected value
            // leaves the property untouched. Anything else (structs holding
            // maps, arrays, strings) is written in place: round-tripping such a
            // value through a temporary copy freed the struct's own storage in
            // game (Movement data, 2026-09-22) and the next read crashed.
            if(p->HasAnyPropertyFlags(CPF_IsPlainOldData)) {
                Value next(p);p->CopyCompleteValue(next.data(),data);
                encode(p,next.data(),request.at("value"),0);
                const auto result=decode(p,next.data(),0);
                if(result.dump().size()>1024*1024) throw std::runtime_error("Property result exceeds 1 MiB");
                p->CopyCompleteValue(data,next.data());return result;
            }
            encode(p,data,request.at("value"),0);
            const auto result=decode(p,data,0);
            if(result.dump().size()>1024*1024) throw std::runtime_error("Property result exceeds 1 MiB");
            return result;
        }
        return decode(p,data,0);
    }
    if(op=="properties") {
        Json result=Json::array();std::set<std::string> seen;unsigned depth=0;
        for(UStruct* type=object->GetClassPrivate();type;type=type->GetSuperStruct()) {
            if(++depth>64) throw std::runtime_error("Class hierarchy exceeds bound");
            for(auto* p:type->ForEachProperty()) {
                auto name=narrow(p->GetName());if(!seen.insert(name).second) continue;
                if(result.size()>=2048) throw std::runtime_error("Property metadata exceeds bound");
                result.push_back({{"name",name},{"size",p->GetElementSize()},{"array_dim",p->GetArrayDim()}});
            }
            if(!request.value("inherited",false)) break;
        }
        return result;
    }
    if(op=="functions") {
        Json result=Json::array();std::set<std::string> seen;unsigned depth=0;
        const auto filter=request.value("contains",std::string());
        for(UStruct* type=object->GetClassPrivate();type;type=type->GetSuperStruct()) {
            if(++depth>64) throw std::runtime_error("Class hierarchy exceeds bound");
            for(auto* fn:type->ForEachFunction()) {
                auto name=narrow(fn->GetName());if(!seen.insert(name).second) continue;
                if(!filter.empty() && name.find(filter)==std::string::npos) continue;
                if(result.size()>=4096) throw std::runtime_error("Function metadata exceeds bound");
                result.push_back(name);
            }
            if(!request.value("inherited",true)) break;
        }
        return result;
    }
    if(op=="call" || op=="describe") {
        const auto name=wide(request.at("function").get<std::string>());auto* fn=object->GetFunctionByNameInChain(name.c_str());
        if(!fn) throw std::runtime_error("Function is missing: "+request.at("function").get<std::string>());
        Call call(object,name.c_str(),fn->GetNumParms());
        auto args=request.value("args",Json::object());if(!args.is_object() && !args.is_array()) throw std::runtime_error("Arguments must be named or positional");
        Json result=Json::object();std::set<std::string> used;size_t index=0;
        for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm)) {
            auto key=narrow(p->GetName());
            if(op=="describe") {
                result[key]={{"size",p->GetElementSize()},{"return",p->HasAnyPropertyFlags(CPF_ReturnParm)},{"out",p->HasAnyPropertyFlags(CPF_OutParm)}};
                UEnum* enumeration=nullptr;
                if(p->IsA<FByteProperty>()) enumeration=static_cast<FByteProperty*>(p)->GetEnum().Get();
                else if(p->IsA<FEnumProperty>()) enumeration=static_cast<FEnumProperty*>(p)->GetEnum().Get();
                if(enumeration) {
                    const auto count=enumeration->NumEnums();
                    if(count<0 || count>256) throw std::runtime_error("Enum metadata exceeds bound");
                    Json entries=Json::array();
                    for(int i=0;i<count;++i) {auto entry=enumeration->GetEnumNameByIndex(i);entries.push_back({{"name",narrow(entry.Key.ToString())},{"value",entry.Value}});}
                    result[key]["enum"]=std::move(entries);
                }
                continue;
            }
            if(p->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
            if(args.is_array() && index<args.size()) {encode(p,call.data(p),args[index++],0);used.insert(key);}
            else if(args.is_object() && args.contains(key)) {encode(p,call.data(p),args[key],0);used.insert(key);}
            else if(!p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) throw std::runtime_error("Missing argument: "+key);
        }
        if(op=="describe") return result;
        if(used.size()!=args.size()) throw std::runtime_error("Unknown function argument");
        const auto outputs=request.value("outputs",Json());
        if(!outputs.is_null()) {
            if(!outputs.is_array() || outputs.size()>64) throw std::runtime_error("Invalid output selection");
            std::set<std::string> selected;
            for(const auto& output:outputs) {
                const auto oname=output.get<std::string>();bool found=false;
                for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm) && p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm) && narrow(p->GetName())==oname) found=true;
                if(!found || !selected.insert(oname).second) throw std::runtime_error("Unknown or duplicate output");
            }
        }
        call.run();
        for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm) && p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) {
            const auto oname=narrow(p->GetName());
            if(outputs.is_null() || std::find(outputs.begin(),outputs.end(),Json(oname))!=outputs.end()) result[oname]=decode(p,call.data(p),0);
        }
        return result;
    }
    throw std::runtime_error("Unknown engine operation: "+op);
}
void Bridge::hook_callback(void* user,void* context,void* frame,void* result) {
    auto& group=*static_cast<HookGroup*>(user);
    for(auto& [id,rule]:group.rules) {
        if(!rule.enabled || rule.busy || rule.target.Get()!=context) continue;
        try {
            auto* pawn=rule.pawn.Get();auto* controller=rule.controller.Get();
            if(!player_matches(pawn,controller) || !seal_matches(controller,rule.seal)) continue;
            if(rule.owner) {
                auto* locals=frame?static_cast<FFrame*>(frame)->Locals():nullptr;
                if(!locals || rule.owner->GetObjectPropertyValue(reinterpret_cast<std::byte*>(locals)+rule.owner->GetOffset_Internal())!=pawn) continue;
            }
            rule.busy=true;
            if(rule.result) {
                if(!result) throw std::runtime_error("Hook result is absent");
                rule.result->SetPropertyValue(result,rule.value);
            } else {
                Call after(static_cast<UObject*>(context),rule.after.c_str(),1);
                after.set(L"Owner",pawn);after.run();
            }
            ++rule.calls;rule.busy=false;
        } catch(...) {rule.busy=false;rule.enabled=false;rule.failed=true;}
    }
}
bool Bridge::stop_hooks() {
    bool clean=true;
    for(auto it=groups_.begin();it!=groups_.end();) {
        for(auto& [id,rule]:it->second->rules) rule.enabled=false;
        if(hooks_ && hooks_->remove(hooks_->context,it->second->token)) it=groups_.erase(it);
        else {clean=false;++it;}
    }
    return clean;
}
Json Bridge::hook_stats() const {
    Json result={{"available",hooks_!=nullptr},{"functions",groups_.size()}};
    size_t rules=0;for(const auto& [fn,group]:groups_) rules+=group->rules.size();
    result["rules"]=rules;
    if(hooks_ && hooks_->stats) { uint64_t calls=0,ticks=0;int script=0;hooks_->stats(hooks_->context,&calls,&ticks,&script);result["calls"]=calls;result["ticks"]=ticks;result["script_hook_installed"]=script!=0; }
    return result;
}
Json Bridge::hook_request(const Json& request) {
    const auto op=request.at("op").get<std::string>();
    const auto owner=request.value("extension",std::string("cssx.development"));
    if(op=="hooks.status") {
        Json result={{"available",hooks_!=nullptr},{"rules",Json::array()}};
        for(const auto& [fn,group]:groups_) for(const auto& [id,rule]:group->rules) if(rule.extension==owner)
            result["rules"].push_back({{"id",id},{"enabled",rule.enabled},{"failed",rule.failed},{"calls",rule.calls},{"live",rule.target.Get()!=nullptr}});
        return result;
    }
    if(!hooks_) throw std::runtime_error("The CSSX loader's hook service is unavailable; reinstall the complete CSSX package and restart.");
    if(op=="hooks.clear") {
        std::vector<uint64_t> ids;
        for(const auto& [fn,group]:groups_) for(const auto& [id,rule]:group->rules) if(rule.extension==owner) ids.push_back(id);
        for(auto id:ids) hook_request({{"op","hooks.remove"},{"extension",owner},{"id",id}});
        return true;
    }
    if(op=="hooks.remove") {
        const auto id=request.at("id").get<uint64_t>();
        for(auto it=groups_.begin();it!=groups_.end();++it) {
            auto& group=*it->second;auto found=group.rules.find(id);if(found==group.rules.end()) continue;
            if(found->second.extension!=owner) throw std::runtime_error("Hook belongs to another extension.");
            if(found->second.busy) throw std::runtime_error("Hook is executing; retry cleanup on the next tick.");
            found->second.enabled=false;
            if(group.rules.size()==1) {
                if(!hooks_->remove(hooks_->context,group.token)) throw std::runtime_error("Hook cleanup is pending; retry before unloading.");
                groups_.erase(it);
            } else group.rules.erase(found);
            return true;
        }
        return true;
    }
    if(op!="hooks.add") throw std::runtime_error("Unknown hook operation");
    auto* target=resolve(request.at("target"));auto* pawn=resolve(request.at("pawn"));auto* controller=resolve(request.at("controller"));
    if(!target || target->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) throw std::runtime_error("Hooks require a live instance, not a default object.");
    if(!player_matches(pawn,controller)) throw std::runtime_error("Hook player ownership changed.");
    size_t count=0;for(const auto& [fn,group]:groups_) count+=group->rules.size();
    if(count>=1024) throw std::runtime_error("Hook rule limit reached.");
    auto name=wide(request.at("function").get<std::string>());
    if(name.empty() || name.size()>128 || name.find(L'\0')!=std::wstring::npos) throw std::runtime_error("Invalid hook function name");
    auto* fn=target->GetFunctionByNameInChain(name.c_str());
    if(!fn || fn->HasAnyFunctionFlags(FUNC_Delegate|FUNC_MulticastDelegate)) throw std::runtime_error("Hook function is missing or unsupported.");
    Call signature(target,name.c_str(),fn->GetNumParms());
    HookGroup::Rule rule;rule.target=target;rule.pawn=pawn;rule.controller=controller;rule.extension=owner;
    rule.seal=wide(request.value("seal",std::string{}));
    if(rule.seal.size()>128 || rule.seal.find(L'\0')!=std::wstring::npos || !seal_matches(controller,rule.seal)) throw std::runtime_error("Equip the matching seal before enabling this cheat.");
    const auto mode=request.at("mode").get<std::string>();
    if(mode=="bool") {
        auto* p=signature.param(L"ReturnValue");
        if(!p->IsA<FBoolProperty>() || !p->HasAnyPropertyFlags(CPF_ReturnParm) || p->GetElementSize()!=1) throw std::runtime_error("Hook requires a reflected boolean return value.");
        rule.result=static_cast<FBoolProperty*>(p);rule.value=request.at("value").get<bool>();
    } else if(mode=="after") {
        auto* owner_param=signature.param(L"Owner");
        if(!owner_param->IsA<FObjectProperty>() || owner_param->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) throw std::runtime_error("Hook source requires an Owner parameter.");
        rule.owner=static_cast<FObjectProperty*>(owner_param);
        rule.after=wide(request.at("after").get<std::string>());
        if(rule.after.empty() || rule.after.size()>128 || rule.after==name || rule.after.find(L'\0')!=std::wstring::npos) throw std::runtime_error("Invalid hook follow-up function");
        Call after(target,rule.after.c_str(),1);auto* p=after.param(L"Owner");
        if(!p->IsA<FObjectProperty>() || p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm) || p->GetElementSize()!=sizeof(UObject*) || !pawn->IsA(static_cast<FObjectProperty*>(p)->GetPropertyClass().Get()))
            throw std::runtime_error("Hook follow-up requires one matching Owner parameter.");
    } else throw std::runtime_error("Unsupported hook mode");
    auto it=groups_.find(fn);
    if(it==groups_.end()) {
        if(groups_.size()>=128) throw std::runtime_error("Hooked-function limit reached.");
        it=groups_.emplace(fn,std::make_unique<HookGroup>()).first;
        it->second->token=hooks_->add(hooks_->context,fn,hook_callback,it->second.get());
        if(!it->second->token) {groups_.erase(it);throw std::runtime_error("The CSSX loader could not register the hook.");}
    }
    const auto id=next_hook_++;
    try {it->second->rules.emplace(id,std::move(rule));}
    catch(...) { if(it->second->rules.empty() && hooks_->remove(hooks_->context,it->second->token)) groups_.erase(it); throw; }
    return id;
}
}

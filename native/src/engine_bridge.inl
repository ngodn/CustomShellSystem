// Included after engine.cpp's reflected frame and lifetime helpers.
namespace css {
namespace {
FProperty* bridge_property(UStruct* type,const std::wstring& name) {
    // UE4SS's GetPropertyByNameInChain compares only FName's base index.
    // Blueprint fields such as SpringBone_1 and SpringBone_2 need the number too.
    const FName wanted(name.c_str());
    unsigned depth=0;
    for(;type;type=type->GetSuperStruct()) {
        if(++depth>64) throw std::runtime_error("CSSX class hierarchy exceeds bound");
        for(auto* field:type->ForEachProperty())
            if(field->GetFName()==wanted) return field;
    }
    return nullptr;
}
struct BridgeMapView {
    FProperty* key;FProperty* value;FScriptMap* map;const FScriptMapLayout& layout;
    int count,end;
    BridgeMapView(FMapProperty* p,void* data):key(p->GetKeyProp()),value(p->GetValueProp()),
        map(static_cast<FScriptMap*>(data)),layout(p->GetMapLayout()) {
        if(p->GetArrayDim()!=1 || p->GetElementSize()!=sizeof(FScriptMap) || !key || !value ||
           key->GetSize()<=0 || value->GetSize()<=0 || layout.ValueOffset<key->GetSize() ||
           layout.SetLayout.Size<=0 || layout.SetLayout.Size>65536 ||
           layout.ValueOffset>layout.SetLayout.Size-value->GetSize())
            throw std::runtime_error("CSSX map layout is unsupported");
        count=map->Num();end=map->GetMaxIndex();
        if(count<0 || end<count || end>4096) throw std::runtime_error("CSSX map exceeds bound");
    }
    std::byte* pair(int index) {return static_cast<std::byte*>(map->GetData(index,layout));}
};
struct BridgeValue {
    FProperty* property;std::vector<std::max_align_t> storage;
    explicit BridgeValue(FProperty* p):property(p) {
        const auto size=p->GetSize(),alignment=p->GetMinAlignment();
        if(size<=0 || size>65536 || alignment<=0 || alignment>int(alignof(std::max_align_t)) || (alignment&(alignment-1)))
            throw std::runtime_error("CSSX property exceeds write buffer bounds");
        storage.resize((size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        property->InitializeValue(storage.data());
    }
    ~BridgeValue(){property->DestroyValue(storage.data());}
    BridgeValue(const BridgeValue&)=delete;
    void* data(){return storage.data();}
};
}
Json EngineBridge::handle(UObject* object) {
    if(!object) return nullptr;
    for(auto it=objects_.begin();it!=objects_.end();) {
        auto* live=it->second.Get();if(!live) it=objects_.erase(it);
        else {if(live==object) return {{"$object",it->first},{"name",narrow(object->GetFullName())},{"class",narrow(object->GetClassPrivate()->GetFullName())}};++it;}
    }
    if(objects_.size()>=8192) throw std::runtime_error("CSSX object handle limit reached");
    const auto id=next_++;objects_.emplace(id,WeakObject(object));
    return {{"$object",id},{"name",narrow(object->GetFullName())},{"class",narrow(object->GetClassPrivate()->GetFullName())}};
}
UObject* EngineBridge::resolve(const Json& value) {
    if(value.is_null()) return nullptr;
    if(!value.is_object() || !value.contains("$object")) throw std::runtime_error("Expected CSSX object handle");
    auto it=objects_.find(value.at("$object").get<uint64_t>());
    auto* result=it==objects_.end()?nullptr:it->second.Get();
    if(!result) throw std::runtime_error("CSSX object expired");return result;
}
Json EngineBridge::decode(FProperty* p,void* data,unsigned depth) {
    if(depth>12 || p->GetArrayDim()!=1 || p->GetElementSize()<0) throw std::runtime_error("Unsupported CSSX property shape");
    if(p->IsA<FEnumProperty>()) {
        auto* underlying=static_cast<FEnumProperty*>(p)->GetUnderlyingProperty();
        if(!underlying || underlying->GetElementSize()!=p->GetElementSize()) throw std::runtime_error("CSSX enum storage mismatch");
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
        if(chars.Num()<0 || chars.Num()>65536) throw std::runtime_error("CSSX FString exceeds bound");
        return chars.Num()?narrow(std::wstring(chars.GetData(),chars.Num()-1)):std::string{};
    }
    if(p->IsA<FTextProperty>()) {
        Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
        auto* input=convert.param(L"InText");
        if(!input->SameType(p) || input->GetElementSize()!=p->GetElementSize())
            throw std::runtime_error("CSSX localized text layout is unsupported");
        input->CopyCompleteValue(convert.data(input),data);convert.run();
        auto* result=convert.param(L"ReturnValue");return decode(result,convert.data(result),depth+1);
    }
    if(p->IsA<FStructProperty>()) {
        auto* type=static_cast<FStructProperty*>(p)->GetStruct().Get();Json result=Json::object();
        size_t count=0;
        for(auto* field:type->ForEachProperty()) {
            if(++count>256 || field->GetOffset_Internal()<0 || field->GetOffset_Internal()+field->GetSize()>p->GetElementSize()) throw std::runtime_error("CSSX struct field exceeds bounds");
            try {result[narrow(field->GetName())]=decode(field,static_cast<std::byte*>(data)+field->GetOffset_Internal(),depth+1);}
            catch(const std::exception&) {result[narrow(field->GetName())]={{"$unsupported",true}};}
        }
        return result;
    }
    if(p->IsA<FArrayProperty>()) {
        auto* a=static_cast<FArrayProperty*>(p);FScriptArrayHelper array(a,data);
        if(array.Num()<0 || array.Num()>4096) throw std::runtime_error("CSSX array exceeds bound");
        Json result=Json::array();for(int i=0;i<array.Num();++i) result.push_back(decode(a->GetInner(),array.GetRawPtr(i),depth+1));return result;
    }
    if(p->IsA<FMapProperty>()) {
        BridgeMapView view(static_cast<FMapProperty*>(p),data);
        Json entries=Json::array();
        for(int index=0;index<view.end;++index) if(view.map->IsValidIndex(index)) {
            auto* pair=view.pair(index);
            entries.push_back({{"key",decode(view.key,pair,depth+1)},
                               {"value",decode(view.value,pair+view.layout.ValueOffset,depth+1)}});
        }
        if(entries.size()!=size_t(view.count)) throw std::runtime_error("CSSX map count changed during read");
        return {{"$map",std::move(entries)}};
    }
    throw std::runtime_error("CSSX does not support this reflected property type");
}
void EngineBridge::encode(FProperty* p,void* data,const Json& value,unsigned depth) {
    if(depth>12 || p->GetArrayDim()!=1) throw std::runtime_error("Unsupported CSSX property shape");
    if(value.is_object() && value.contains("$table_field")) {
        const auto& source=value.at("$table_field");
        auto* object=resolve(source.at("table"));
        if(!object || !object->IsA<UDataTable>()) throw std::runtime_error("CSSX typed source is not a DataTable");
        auto* table=static_cast<UDataTable*>(object);auto* type=table->GetRowStruct().Get();
        if(!type) throw std::runtime_error("CSSX table row type is unavailable");
        const auto row_name=source.at("row").get<std::string>(),field_name=source.at("field").get<std::string>();
        for(const auto* name:{&row_name,&field_name}) if(name->empty() || name->size()>256 || name->find('\0')!=std::string::npos)
            throw std::runtime_error("CSSX table row or field name is invalid");
        const auto row=wide(row_name),field=wide(field_name);
        auto* property=bridge_property(type,field);
        if(!property || property->GetOffset_Internal()<0 || property->GetOffset_Internal()+property->GetSize()>type->GetPropertiesSize() || !p->SameType(property))
            throw std::runtime_error("CSSX table field type does not match the parameter");
        const auto& rows=table->GetRowMap();if(rows.Num()<0 || rows.Num()>4096) throw std::runtime_error("CSSX table exceeds bound");
        auto* found=rows.Find(FName(row.c_str()));if(!found || !*found) throw std::runtime_error("CSSX table row disappeared");
        // Copy through the real reflected property, including UE5 soft classes.
        // Never reinterpret a resolved UClass pointer as a TSoftClassPtr.
        p->CopyCompleteValue(data,*found+property->GetOffset_Internal());return;
    }
    if(p->IsA<FEnumProperty>()) {
        auto* underlying=static_cast<FEnumProperty*>(p)->GetUnderlyingProperty();
        if(!underlying || underlying->GetElementSize()!=p->GetElementSize()) throw std::runtime_error("CSSX enum storage mismatch");
        encode(underlying,data,value,depth+1);return;
    }
    if(p->IsA<FBoolProperty>()) {static_cast<FBoolProperty*>(p)->SetPropertyValue(data,value.get<bool>());return;}
    if(p->IsA<FNumericProperty>()) {
        auto* number=static_cast<FNumericProperty*>(p);
        if(!value.is_number()) throw std::runtime_error("CSSX numeric input required");
        if(number->IsFloatingPoint()) {auto v=value.get<double>();if(!std::isfinite(v)) throw std::runtime_error("CSSX numeric value is not finite");number->SetFloatingPointPropertyValue(data,v);}
        else {
            if(!value.is_number_integer()) throw std::runtime_error("CSSX integer input required");
            const auto size=p->GetElementSize();
            if(size!=1 && size!=2 && size!=4 && size!=8) throw std::runtime_error("Unsupported integer width");
            const bool is_unsigned=p->IsA<FByteProperty>() || p->IsA<FUInt16Property>() || p->IsA<FUInt32Property>() || p->IsA<FUInt64Property>();
            if(is_unsigned) {
                if(!value.is_number_unsigned() && value.get<int64_t>()<0) throw std::runtime_error("CSSX unsigned input is negative");
                auto v=value.get<uint64_t>();if(size<8 && v>=(uint64_t{1}<<(size*8))) throw std::runtime_error("CSSX integer exceeds field range");
                number->SetIntPropertyValue(data,static_cast<uint64>(v));
            } else {
                if(value.is_number_unsigned() && value.get<uint64_t>()>uint64_t(INT64_MAX)) throw std::runtime_error("CSSX integer exceeds field range");
                auto v=value.get<int64_t>();if(size<8 && (v<-(int64_t{1}<<(size*8-1)) || v>=(int64_t{1}<<(size*8-1)))) throw std::runtime_error("CSSX integer exceeds field range");
                number->SetIntPropertyValue(data,static_cast<int64>(v));
            }
        }
        return;
    }
    if(p->IsA<FObjectProperty>()) {
        auto* property=static_cast<FObjectProperty*>(p);auto* object=resolve(value);
        if(object && !object->IsA(property->GetPropertyClass().Get())) throw std::runtime_error("CSSX object class does not match the property");
        if(p->GetElementSize()!=sizeof(object)) throw std::runtime_error("CSSX object property size mismatch");
        // The setter wrapper is unavailable in this UE4SS build. Use the same
        // reflected copy path as Call::set for an ordinary object property.
        p->CopyCompleteValue(data,&object);return;
    }
    if(p->IsA<FNameProperty>()) {auto text=wide(value.get<std::string>());FName name(text.c_str());p->CopyCompleteValue(data,&name);return;}
    if(p->IsA<FStrProperty>()) {auto text=wide(value.get<std::string>());FString s(text.c_str());p->CopyCompleteValue(data,&s);return;}
    if(p->IsA<FTextProperty>()) {
        // FText is built by the engine from a string, then copied into place.
        auto text=wide(value.get<std::string>());
        Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
        convert.set(L"InString",FString(text.c_str())); convert.run();
        auto* result=convert.param(L"ReturnValue");
        if(!result->SameType(p)) throw std::runtime_error("CSSX text property type mismatch");
        p->CopyCompleteValue(data,convert.data(result));return;
    }
    if(p->IsA<FArrayProperty>()) {
        if(!value.is_array() || value.size()>4096) throw std::runtime_error("CSSX array requires an array of at most 4096 items");
        // FScriptArray is {data, num, max}. Its resize helpers do not link against this UE4SS
        // build, so the storage is resized through the engine's own allocator (GMalloc).
        struct RawArray { void* data; int32 num; int32 max; };
        auto* a=static_cast<FArrayProperty*>(p); auto* inner=a->GetInner();
        auto* raw=static_cast<RawArray*>(data);
        const auto size=static_cast<size_t>(inner->GetSize());
        if(raw->num<0 || raw->num>raw->max || size==0) throw std::runtime_error("CSSX array header is invalid");
        for(int32 i=0;i<raw->num;++i) inner->DestroyValue(static_cast<std::byte*>(raw->data)+i*size);
        const auto count=static_cast<int32>(value.size());
        raw->data=count?(*GMalloc)->Realloc(raw->data,count*size,static_cast<uint32>(inner->GetMinAlignment())):((*GMalloc)->Free(raw->data),nullptr);
        if(count && !raw->data) throw std::runtime_error("CSSX array allocation failed");
        raw->num=raw->max=count;
        for(int32 i=0;i<count;++i) {
            auto* item=static_cast<std::byte*>(raw->data)+i*size;
            std::memset(item,0,size); inner->InitializeValue(item);
            encode(inner,item,value[i],depth+1);
        }
        return;
    }
    if(p->IsA<FStructProperty>()) {
        if(!value.is_object()) throw std::runtime_error("CSSX struct requires an object");
        auto* type=static_cast<FStructProperty*>(p)->GetStruct().Get();
        for(auto it=value.begin();it!=value.end();++it) {
            auto key=wide(it.key());auto* field=bridge_property(type,key);
            if(!field || field->GetOffset_Internal()<0 || field->GetOffset_Internal()+field->GetSize()>p->GetElementSize()) throw std::runtime_error("CSSX struct member mismatch: "+it.key());
            encode(field,static_cast<std::byte*>(data)+field->GetOffset_Internal(),it.value(),depth+1);
        }
        return;
    }
    throw std::runtime_error("CSSX cannot write this reflected property type");
}
Json EngineBridge::request(void* engine,Appearance& appearance,const Json& request) {
    const auto op=request.at("op").get<std::string>();
    if(op=="valid") {
        const auto& target=request.at("target");if(target.is_null()) return false;
        if(!target.is_object() || !target.contains("$object")) throw std::runtime_error("Expected CSSX object handle");
        const auto found=objects_.find(target.at("$object").get<uint64_t>());
        return found!=objects_.end() && found->second.Get()!=nullptr;
    }
#ifdef CSS_INVENTORY_DEV
    // Startup probes need a world context before a playable character exists.
    if(op=="engine") return handle(static_cast<UObject*>(engine));
#endif
    if(op=="player") {
        auto* pawn=appearance.player(engine);return {{"pawn",handle(pawn)},{"controller",handle(pawn?read<UObject*>(pawn,L"Controller"):nullptr)},{"shell",appearance.shell},{"revision",appearance.player_revision}};
    }
    if(op=="class_default") {
        const auto name=request.at("class").get<std::string>();
        if(name.empty() || name.size()>96) throw std::runtime_error("Invalid CSSX default class name");
        if(auto it=defaults_.find(name);it!=defaults_.end()) if(auto* object=it->second.Get()) return handle(object);
        auto key=FName(wide(name).c_str());UObject* match=nullptr;bool duplicate=false;
        UObjectGlobals::ForEachUObject([&](UObject* object,int32,int32) {
            if(object && object->HasAnyFlags(RF_ClassDefaultObject) && object->GetClassPrivate()->GetFName()==key) {
                if(match) {duplicate=true;return RC::LoopAction::Break;}
                match=object;
            }
            return RC::LoopAction::Continue;
        });
        if(duplicate) throw std::runtime_error("Ambiguous CSSX default class name; use a full object path");
        if(match) {if(defaults_.size()>=128) throw std::runtime_error("CSSX default-object cache limit reached");defaults_[name]=match;}
        return handle(match);
    }
    if(op=="find" || op=="load") {
        auto path=request.at("path").get<std::string>();if(path.size()>2048) throw std::runtime_error("CSSX object path exceeds bound");
        if(op=="load") return handle(load(path));
        auto text=wide(path);return handle(UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,text.c_str()));
    }
#ifdef CSS_INVENTORY_DEV
    // Dev prototyping: build a plain UMG widget (panel, image, text) inside a widget tree.
    if(op=="construct") {
        const auto path=request.at("class").get<std::string>();
        if(!path.starts_with("/Script/UMG.")) throw std::runtime_error("Only /Script/UMG widget classes can be constructed");
        auto* outer=resolve(request.at("outer"));if(!outer) throw std::runtime_error("Construct needs a live outer");
        return handle(construct(wide(path).c_str(),outer));
    }
#endif
    auto* object=resolve(request.at("target"));if(!object) throw std::runtime_error("CSSX target is null");
    if(op=="animation.reset_dynamics") {
        if(!object->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimInstance"))))
            throw std::runtime_error("Dynamics reset target must be an animation instance");
        reset_dynamics(object);
        return true;
    }
    if(op=="input.keys") {
        const auto& keys=request.at("keys");
        if(!keys.is_array() || keys.size()>64) throw std::runtime_error("CSSX input batch exceeds 64 keys");
        std::vector<std::string> names;std::set<std::string> unique;
        for(const auto& key:keys) {
            const auto name=key.get<std::string>();
            if(name.empty() || name.size()>64 || !std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c) || c=='_';}) || !unique.insert(name).second)
                throw std::runtime_error("Invalid or duplicate CSSX input key");
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
        if(!object->IsA<UDataTable>()) throw std::runtime_error("CSSX target is not a DataTable");
        auto* table=static_cast<UDataTable*>(object);const auto& rows=table->GetRowMap();
        if(rows.Num()<0 || rows.Num()>4096) throw std::runtime_error("CSSX table exceeds bound");
        Json names=Json::array();for(const auto& entry:rows) names.push_back(narrow(entry.Key.ToString()));
        std::sort(names.begin(),names.end());return names;
    }
    if(op=="get" || op=="set" || op=="map.update") {
        const auto name=wide(request.at("property").get<std::string>());
        auto* type=object->IsA<UStruct>()?static_cast<UStruct*>(object):object->GetClassPrivate();
        auto* p=bridge_property(type,name);if(!p || p->GetOffset_Internal()<0) throw std::runtime_error("CSSX property is missing");
        auto* data=reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal();
        if(op=="map.update") {
            if(object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) throw std::runtime_error("CSSX refuses default-object writes");
            if(!p->IsA<FMapProperty>()) throw std::runtime_error("CSSX property is not a map");
            BridgeMapView view(static_cast<FMapProperty*>(p),data);
            std::byte* selected=nullptr;
            // Keys must be supplied exactly as returned by get. Keys and map
            // membership are never modified, so no rehash or insertion occurs.
            for(int i=0;i<view.end;++i) if(view.map->IsValidIndex(i)) {
                auto* pair=view.pair(i);
                if(decode(view.key,pair,0)==request.at("key")) {
                    if(selected) throw std::runtime_error("CSSX map key is ambiguous");
                    selected=pair+view.layout.ValueOffset;
                }
            }
            if(!selected) throw std::runtime_error("CSSX map key no longer exists");
            if(decode(view.value,selected,0)!=request.at("expected")) throw std::runtime_error("CSSX map value changed; refresh before editing");
            BridgeValue next(view.value);view.value->CopyCompleteValue(next.data(),selected);
            encode(view.value,next.data(),request.at("value"),0);
            const auto result=decode(view.value,next.data(),0);
            if(result.dump().size()>1024*1024) throw std::runtime_error("CSSX map result exceeds 1 MiB");
            // Everything, including decoding the result, succeeds before the
            // write. A rejected request cannot leave a partially edited value.
            view.value->CopyCompleteValue(selected,next.data());return result;
        }
        if(op=="set") {
            if(object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) throw std::runtime_error("CSSX refuses default-object writes");
            BridgeValue next(p);p->CopyCompleteValue(next.data(),data);
            encode(p,next.data(),request.at("value"),0);
            const auto result=decode(p,next.data(),0);
            if(result.dump().size()>1024*1024) throw std::runtime_error("CSSX property result exceeds 1 MiB");
            p->CopyCompleteValue(data,next.data());return result;
        }
        return decode(p,data,0);
    }
    if(op=="properties") {
        Json result=Json::array();std::set<std::string> seen;unsigned depth=0;
        for(UStruct* type=object->GetClassPrivate();type;type=type->GetSuperStruct()) {
            if(++depth>64) throw std::runtime_error("CSSX class hierarchy exceeds bound");
            for(auto* p:type->ForEachProperty()) {
                auto name=narrow(p->GetName());if(!seen.insert(name).second) continue;
                if(result.size()>=2048) throw std::runtime_error("CSSX property metadata exceeds bound");
                result.push_back({{"name",name},{"size",p->GetElementSize()},{"array_dim",p->GetArrayDim()}});
            }
            if(!request.value("inherited",false)) break;
        }
        return result;
    }
    if(op=="functions") {
        Json result=Json::array();std::set<std::string> seen;unsigned depth=0;
        for(UStruct* type=object->IsA<UStruct>()?static_cast<UStruct*>(object):object->GetClassPrivate();type;type=type->GetSuperStruct()) {
            if(++depth>64) throw std::runtime_error("CSSX class hierarchy exceeds bound");
            for(auto* fn:type->ForEachFunction()) {
                auto name=narrow(fn->GetName());if(!seen.insert(name).second) continue;
                if(result.size()>=2048) throw std::runtime_error("CSSX function metadata exceeds bound");
                result.push_back(name);
            }
            if(!request.value("inherited",false)) break;
        }
        return result;
    }
    if(op=="call" || op=="describe") {
        const auto name=wide(request.at("function").get<std::string>());auto* fn=object->GetFunctionByNameInChain(name.c_str());
        if(!fn) throw std::runtime_error("CSSX function is missing: "+request.at("function").get<std::string>());
        Call call(object,name.c_str(),fn->GetNumParms());
        auto args=request.value("args",Json::object());if(!args.is_object() && !args.is_array()) throw std::runtime_error("CSSX arguments must be named or positional");
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
                    if(count<0 || count>256) throw std::runtime_error("CSSX enum metadata exceeds bound");
                    Json entries=Json::array();
                    for(int i=0;i<count;++i) {auto entry=enumeration->GetEnumNameByIndex(i);entries.push_back({{"name",narrow(entry.Key.ToString())},{"value",entry.Value}});}
                    result[key]["enum"]=std::move(entries);
                }
                if(p->IsA<FStructProperty>()) {
                    auto* st=static_cast<FStructProperty*>(p)->GetStruct().Get();
                    if(st) {
                        result[key]["struct"]=narrow(st->GetName());
                        Json fields=Json::array();
                        for(auto* field:st->ForEachProperty()) {
                            fields.push_back({{"name",narrow(field->GetName())},{"size",field->GetElementSize()}});
                        }
                        result[key]["fields"]=std::move(fields);
                    }
                }
                continue;
            }
            if(p->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
            if(args.is_array() && index<args.size()) {encode(p,call.data(p),args[index++],0);used.insert(key);}
            else if(args.is_object() && args.contains(key)) {encode(p,call.data(p),args[key],0);used.insert(key);}
            else if(!p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) throw std::runtime_error("CSSX missing argument: "+key);
        }
        if(op=="describe") return result;
        if(used.size()!=args.size()) throw std::runtime_error("CSSX unknown function argument");
        // Some functions return opaque soft references as output parameters.
        // Validate an explicit output selection before invoking the function,
        // so unused output decoding cannot turn a successful mutation into an error.
        const auto outputs=request.value("outputs",Json());
        if(!outputs.is_null()) {
            if(!outputs.is_array() || outputs.size()>64) throw std::runtime_error("Invalid CSSX output selection");
            std::set<std::string> selected;
            for(const auto& output:outputs) {
                const auto name=output.get<std::string>();bool found=false;
                for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm) && p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm) && narrow(p->GetName())==name) found=true;
                if(!found || !selected.insert(name).second) throw std::runtime_error("Unknown or duplicate CSSX output");
            }
        }
        call.run();
        for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm) && p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) {
            const auto name=narrow(p->GetName());
            if(outputs.is_null() || std::find(outputs.begin(),outputs.end(),Json(name))!=outputs.end()) result[name]=decode(p,call.data(p),0);
        }
        return result;
    }
    throw std::runtime_error("Unknown CSSX engine operation");
}
}

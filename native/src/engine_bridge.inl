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
#ifdef CSS_INVENTORY_DEV
#include "astral_observer.inl"
#include "astral_materials_probe.inl"
#include "astral_bindings_probe.inl"
#include "astral_source_probe.inl"
#include "astral_adapters_probe.inl"
#include "astral_visual_probe.inl"
#endif
// The diagnostic changes one reflected scalar, never copies an AnimGraph node.
#ifdef CSS_INVENTORY_DEV
namespace {
std::pair<FNumericProperty*,void*> control_rig_alpha(UObject* anim) {
    if(narrow(anim->GetClassPrivate()->GetPathName())!="/Game/Sparta/Characters/Humans/Player/Animations/ABP_Player.ABP_Player_C")
        throw std::runtime_error("Rig trial requires the verified ABP_Player class");
    Call barrier(anim,L"GetCurveValue",2);
    barrier.set(L"CurveName",FName(L"None"));barrier.run();
    (void)barrier.get<float>();
    auto* node=bridge_property(anim->GetClassPrivate(),L"AnimGraphNode_ControlRig");
    if(!node || !node->IsA<FStructProperty>() || node->GetArrayDim()!=1 || node->GetOffset_Internal()<0 ||
       node->GetOffset_Internal()+node->GetSize()>anim->GetClassPrivate()->GetPropertiesSize())
        throw std::runtime_error("ControlRig node layout mismatch");
    auto* type=static_cast<FStructProperty*>(node)->GetStruct().Get();
    if(!type || type->GetFName()!=FName(L"AnimNode_ControlRig")) throw std::runtime_error("Unexpected ControlRig node type");
    auto* scale_bias=bridge_property(type,L"AlphaScaleBias");
    if(!scale_bias || !scale_bias->IsA<FStructProperty>() || scale_bias->GetArrayDim()!=1 ||
       scale_bias->GetOffset_Internal()<0 || scale_bias->GetOffset_Internal()+scale_bias->GetSize()>node->GetElementSize())
        throw std::runtime_error("ControlRig scale/bias layout mismatch");
    auto* scale_type=static_cast<FStructProperty*>(scale_bias)->GetStruct().Get();
    if(!scale_type || scale_type->GetFName()!=FName(L"InputScaleBias")) throw std::runtime_error("Unexpected scale/bias type");
    auto* field=bridge_property(scale_type,L"Scale");
    if(!field || !field->IsA<FNumericProperty>() || field->GetElementSize()!=sizeof(float) ||
       field->GetArrayDim()!=1 || field->GetOffset_Internal()<0 || field->GetOffset_Internal()+field->GetSize()>scale_bias->GetElementSize())
        throw std::runtime_error("ControlRig alpha layout mismatch");
    auto* scalar=static_cast<FNumericProperty*>(field);
    if(!scalar->IsFloatingPoint()) throw std::runtime_error("ControlRig alpha is not floating point");
    return {scalar,reinterpret_cast<std::byte*>(anim)+node->GetOffset_Internal()+scale_bias->GetOffset_Internal()+field->GetOffset_Internal()};
}
}
void EngineBridge::restore_rig_trial() {
    if(!rig_trial_deadline_) return;
    if(auto* anim=rig_trial_anim_.Get()) {
        auto [property,data]=control_rig_alpha(anim);
        const auto current=property->GetFloatingPointPropertyValue(data);
        if(current==rig_trial_written_) {
            property->SetFloatingPointPropertyValue(data,rig_trial_original_);
            if(property->GetFloatingPointPropertyValue(data)!=rig_trial_original_) throw std::runtime_error("Rig trial restore failed");
            rig_trial_result_="restored";
        } else rig_trial_result_="relinquished after another write";
    } else rig_trial_result_="owner expired";
    rig_trial_anim_={};rig_trial_deadline_=0;
}
void EngineBridge::finish_rig_trial(bool force) {
    if(rig_trial_deadline_ && (force || GetTickCount64()>=rig_trial_deadline_)) restore_rig_trial();
}
Json EngineBridge::control_rig_trial(void* engine,Appearance& appearance,const Json& query) {
    const auto action=query.at("action").get<std::string>();
    if(action=="restore") restore_rig_trial();
    else if(action=="begin") {
        if(rig_trial_deadline_) throw std::runtime_error("Restore the active rig trial first");
        auto* pawn=appearance.player(engine);
        if(!pawn) throw std::runtime_error("No player for rig trial");
        auto* mesh=read<UObject*>(pawn,L"Mesh");Call instance(mesh,L"GetAnimInstance",1);instance.run();
        auto* anim=instance.get<UObject*>();
        if(!anim || anim!=resolve(query.at("target"))) throw std::runtime_error("Rig trial target is not the current player AnimInstance");
        const auto alpha=query.at("alpha").get<float>();
        const auto seconds=query.at("seconds").get<unsigned>();
        if(!std::isfinite(alpha) || alpha<0 || alpha>1 || seconds<1 || seconds>120) throw std::runtime_error("Rig trial exceeds bounds");
        auto [property,data]=control_rig_alpha(anim);
        const auto graph=request(engine,appearance,{{"op","get"},{"target",query.at("target")},{"property","AnimGraphNode_ControlRig"}});
        if(graph.at("AlphaInputType")!=0 || graph.at("AlphaScaleBias").at("Bias")!=0)
            throw std::runtime_error("Rig trial requires float alpha with zero bias");
        const auto original=property->GetFloatingPointPropertyValue(data);
        if(!std::isfinite(original) || original<0 || original>1) throw std::runtime_error("Unexpected original rig alpha");
        rig_trial_anim_=anim;rig_trial_original_=static_cast<float>(original);rig_trial_written_=alpha;
        rig_trial_deadline_=GetTickCount64()+seconds*1000;rig_trial_result_="active";
        property->SetFloatingPointPropertyValue(data,alpha);
    } else if(action!="status") throw std::runtime_error("Unknown rig trial action");
    return {{"active",rig_trial_deadline_!=0},{"field","AlphaScaleBias.Scale"},{"result",rig_trial_result_},{"original",rig_trial_original_},{"written",rig_trial_written_},{"deadline_ms",rig_trial_deadline_}};
}
#endif
Json EngineBridge::request(void* engine,Appearance& appearance,const Json& request) {
    const auto op=request.at("op").get<std::string>();
#ifdef CSS_INVENTORY_DEV
    if(op=="animation.control_rig_trial") return control_rig_trial(engine,appearance,request);
    if(op=="animation.class_inventory") {
        auto* animation_type=static_cast<UClass*>(find(L"/Script/Engine.AnimInstance"));
        auto* component_type=static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"));
        Json rows=Json::array();
        size_t visited=0;
        bool truncated=false;
        const auto started=std::chrono::steady_clock::now();
        UObjectGlobals::ForEachUObject([&](UObject* object,int32,int32) {
            ++visited;
            if(!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_BeginDestroyed|RF_FinishDestroyed)) ||
               object->IsUnreachable() || !object->IsA(animation_type)) return RC::LoopAction::Continue;
            auto* component=object->GetOuterPrivate();
            if(!component || !component->IsA(component_type) || component->IsUnreachable() ||
               component->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))) return RC::LoopAction::Continue;
            const bool main=read<UObject*>(component,L"AnimScriptInstance")==object;
            const bool post=read<UObject*>(component,L"PostProcessAnimInstance")==object;
            if(!main && !post) return RC::LoopAction::Continue;
            if(rows.size()>=1024) {truncated=true;return RC::LoopAction::Break;}
            auto* type=object->GetClassPrivate();
            rows.push_back({{"instance",asset_evidence(object)},{"component",asset_evidence(component)},
                {"class",asset_evidence(type)},{"instance_address",reinterpret_cast<uintptr_t>(object)},
                {"component_address",reinterpret_cast<uintptr_t>(component)},
                {"class_address",reinterpret_cast<uintptr_t>(type)},
                {"class_root_set",type->IsRootSet()},
                {"role",main?"main":"post_process"},
                {"sparse_data_present",type->GetSparseClassData()!=nullptr}});
            return RC::LoopAction::Continue;
        });
        return {{"instances",rows},{"truncated",truncated},{"visited",visited},
            {"elapsed_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()}};
    }
    if(op=="object.state") {
        auto* object=resolve(request.at("target"));
        if(!object) throw std::runtime_error("Object state requires a live target");
        Json result={{"identity",asset_evidence(object)},{"class",handle(object->GetClassPrivate())}};
        if(object->IsA<UClass>()) {
            auto* type=static_cast<UClass*>(object);
            result["sparse_data_present"]=type->GetSparseClassData()!=nullptr;
            result["sparse_data_struct"]=handle(type->GetSparseClassDataStruct().Get());
        }
        return result;
    }
#endif
    if(op=="valid") {
        const auto& target=request.at("target");if(target.is_null()) return false;
        if(!target.is_object() || !target.contains("$object")) throw std::runtime_error("Expected CSSX object handle");
        const auto found=objects_.find(target.at("$object").get<uint64_t>());
        return found!=objects_.end() && found->second.Get()!=nullptr;
    }
#ifdef CSS_INVENTORY_DEV
    if(op=="astral.observe") return observe_astral(engine,request.value("poses",false));
    if(op=="astral.source") { appearance.player(engine); return observe_astral_source(appearance); }
    if(op=="astral.adapters.probe") return probe_astral_adapters(engine,appearance);
    if(op=="astral.materials.probe") return probe_astral_materials(engine,request);
    if(op=="astral.bindings.probe") return probe_astral_bindings(engine,request);
    if(op=="astral.visual.probe") return probe_astral_visual(engine,appearance,request);
    if(op=="astral.clear") { astral_lifecycle_.clear(); return true; }
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
#ifdef CSS_INVENTORY_DEV
    if(op=="get.member") {
        const auto& path=request.at("path");
        if(!path.is_array() || path.empty() || path.size()>12)
            throw std::runtime_error("Member path must contain 1 to 12 fields");
        UStruct* type=object->GetClassPrivate();
        auto* data=reinterpret_cast<std::byte*>(object);
        int size=type->GetPropertiesSize();
        for(size_t index=0;index<path.size();++index) {
            const auto name=path.at(index).get<std::string>();
            if(name.empty() || name.size()>128 || name.find('\0')!=std::string::npos)
                throw std::runtime_error("Invalid member path field");
            auto* field=bridge_property(type,wide(name));
            if(!field || field->GetArrayDim()!=1 || field->GetElementSize()<=0 ||
               field->GetElementSize()>size || field->GetOffset_Internal()<0 ||
               field->GetOffset_Internal()>size-field->GetElementSize())
                throw std::runtime_error("Member path field is missing or exceeds bounds: "+name);
            data+=field->GetOffset_Internal();
            if(index+1==path.size()) return decode(field,data,0);
            if(!field->IsA<FStructProperty>())
                throw std::runtime_error("Member path can only descend into structs: "+name);
            type=static_cast<FStructProperty*>(field)->GetStruct().Get();
            size=field->GetElementSize();
            if(!type || type->GetStructureSize()>size)
                throw std::runtime_error("Member path struct exceeds bounds: "+name);
        }
    }
#endif
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

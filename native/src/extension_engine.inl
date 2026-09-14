// Included after engine.cpp's reflected frame and lifetime helpers.
namespace css {
Json ExtensionBridge::handle(UObject* object) {
    if(!object) return nullptr;
    for(auto it=objects_.begin();it!=objects_.end();) {
        auto* live=it->second.Get();if(!live) it=objects_.erase(it);
        else {if(live==object) return {{"$object",it->first},{"name",narrow(object->GetFullName())},{"class",narrow(object->GetClassPrivate()->GetFullName())}};++it;}
    }
    if(objects_.size()>=8192) throw std::runtime_error("CSSX object handle limit reached");
    const auto id=next_++;objects_.emplace(id,WeakObject(object));
    return {{"$object",id},{"name",narrow(object->GetFullName())},{"class",narrow(object->GetClassPrivate()->GetFullName())}};
}
UObject* ExtensionBridge::resolve(const Json& value) {
    if(value.is_null()) return nullptr;
    if(!value.is_object() || !value.contains("$object")) throw std::runtime_error("Expected CSSX object handle");
    auto it=objects_.find(value.at("$object").get<uint64_t>());
    auto* result=it==objects_.end()?nullptr:it->second.Get();
    if(!result) throw std::runtime_error("CSSX object expired");return result;
}
Json ExtensionBridge::decode(FProperty* p,void* data,unsigned depth) {
    if(depth>12 || p->GetArrayDim()!=1 || p->GetElementSize()<0) throw std::runtime_error("Unsupported CSSX property shape");
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
    throw std::runtime_error("CSSX does not support this reflected property type");
}
void ExtensionBridge::encode(FProperty* p,void* data,const Json& value,unsigned depth) {
    if(depth>12 || p->GetArrayDim()!=1) throw std::runtime_error("Unsupported CSSX property shape");
    if(p->IsA<FBoolProperty>()) {static_cast<FBoolProperty*>(p)->SetPropertyValue(data,value.get<bool>());return;}
    if(p->IsA<FNumericProperty>()) {
        auto* number=static_cast<FNumericProperty*>(p);
        if(!value.is_number()) throw std::runtime_error("CSSX numeric input required");
        if(number->IsFloatingPoint()) {auto v=value.get<double>();if(!std::isfinite(v)) throw std::runtime_error("CSSX numeric value is not finite");number->SetFloatingPointPropertyValue(data,v);}
        else {auto v=value.get<int64_t>();const auto size=p->GetElementSize();if(size<8 && (v<-(int64_t{1}<<(size*8-1)) || v>=(int64_t{1}<<(size*8)))) throw std::runtime_error("CSSX integer exceeds field range");number->SetIntPropertyValue(data,static_cast<int64>(v));}
        return;
    }
    if(p->IsA<FObjectProperty>()) {static_cast<FObjectProperty*>(p)->SetObjectPropertyValue(data,resolve(value));return;}
    if(p->IsA<FNameProperty>()) {auto text=wide(value.get<std::string>());FName name(text.c_str());p->CopyCompleteValue(data,&name);return;}
    if(p->IsA<FStrProperty>()) {auto text=wide(value.get<std::string>());FString s(text.c_str());p->CopyCompleteValue(data,&s);return;}
    if(p->IsA<FStructProperty>()) {
        if(!value.is_object()) throw std::runtime_error("CSSX struct requires an object");
        auto* type=static_cast<FStructProperty*>(p)->GetStruct().Get();
        for(auto it=value.begin();it!=value.end();++it) {
            auto key=wide(it.key());auto* field=type->GetPropertyByNameInChain(key.c_str());
            if(!field || field->GetOffset_Internal()<0 || field->GetOffset_Internal()+field->GetSize()>p->GetElementSize()) throw std::runtime_error("CSSX struct member mismatch: "+it.key());
            encode(field,static_cast<std::byte*>(data)+field->GetOffset_Internal(),it.value(),depth+1);
        }
        return;
    }
    throw std::runtime_error("CSSX cannot write this reflected property type");
}
Json ExtensionBridge::request(void* engine,Appearance& appearance,const Json& request) {
    const auto op=request.at("op").get<std::string>();
    if(op=="player") {
        auto* pawn=appearance.player(engine);return {{"pawn",handle(pawn)},{"controller",handle(pawn?read<UObject*>(pawn,L"Controller"):nullptr)},{"shell",appearance.shell},{"revision",appearance.player_revision}};
    }
    if(op=="find" || op=="load") {
        auto path=request.at("path").get<std::string>();if(path.size()>2048) throw std::runtime_error("CSSX object path exceeds bound");
        if(op=="load") return handle(load(path));
        auto text=wide(path);return handle(UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,text.c_str()));
    }
    auto* object=resolve(request.at("target"));if(!object) throw std::runtime_error("CSSX target is null");
    if(op=="get" || op=="set") {
        const auto name=wide(request.at("property").get<std::string>());
        auto* p=object->GetPropertyByNameInChain(name.c_str());if(!p || p->GetOffset_Internal()<0) throw std::runtime_error("CSSX property is missing");
        auto* data=reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal();
        if(op=="set") {
            if(object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject))) throw std::runtime_error("CSSX refuses default-object writes");
            encode(p,data,request.at("value"),0);
        }
        return decode(p,data,0);
    }
    if(op=="call" || op=="describe") {
        const auto name=wide(request.at("function").get<std::string>());auto* fn=object->GetFunctionByNameInChain(name.c_str());
        if(!fn) throw std::runtime_error("CSSX function is missing: "+request.at("function").get<std::string>());
        Call call(object,name.c_str(),fn->GetNumParms());
        auto args=request.value("args",Json::object());if(!args.is_object()) throw std::runtime_error("CSSX arguments must be named");
        Json result=Json::object();std::set<std::string> used;
        for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm)) {
            auto key=narrow(p->GetName());
            if(op=="describe") {result[key]={{"size",p->GetElementSize()},{"return",p->HasAnyPropertyFlags(CPF_ReturnParm)},{"out",p->HasAnyPropertyFlags(CPF_OutParm)}};continue;}
            if(args.contains(key)) {encode(p,call.data(p),args[key],0);used.insert(key);}
            else if(!p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) throw std::runtime_error("CSSX missing argument: "+key);
        }
        if(op=="describe") return result;
        if(used.size()!=args.size()) throw std::runtime_error("CSSX unknown function argument");
        call.run();
        for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm) && p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm)) result[narrow(p->GetName())]=decode(p,call.data(p),0);
        return result;
    }
    throw std::runtime_error("Unknown CSSX engine operation");
}
}

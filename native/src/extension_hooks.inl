// Included in engine.cpp after the reflected Call helper.
namespace css {
struct ExtensionBridge::HookGroup {
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
ExtensionBridge::ExtensionBridge()=default;
ExtensionBridge::~ExtensionBridge()=default;
void ExtensionBridge::configure_hooks(void* address) noexcept {
    HMODULE loader=nullptr;
    if(!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     reinterpret_cast<LPCWSTR>(address),&loader)) return;
    auto get=reinterpret_cast<CssGetHookHost>(GetProcAddress(loader,"css_get_hook_host"));
    const auto* service=get?get():nullptr;
    if(service && service->abi==1 && service->size>=sizeof(CssHookHost) && service->add && service->remove) hook_host_=service;
}
namespace {
bool hook_player_matches(UObject* pawn,UObject* controller) {
    return pawn && controller && read<UObject*>(pawn,L"Controller")==controller && read<UObject*>(controller,L"Pawn")==pawn;
}
bool hook_seal_matches(UObject* controller,const std::wstring& seal) {
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
    auto* data=base+item->GetOffset_Internal();
    auto* object=static_cast<FObjectProperty*>(item)->GetObjectPropertyValue(data);
    // ItemDef is a class reference in this game. Accept its exact class name,
    // or an instance of that class, never a substring of another seal's name.
    return object && (object->GetName()==seal || object->GetClassPrivate()->GetName()==seal);
}
}
void ExtensionBridge::hook_callback(void* user,void* context,void* frame,void* result) noexcept {
    auto& group=*static_cast<HookGroup*>(user);
    for(auto& [id,rule]:group.rules) {
        if(!rule.enabled || rule.busy || rule.target.Get()!=context) continue;
        try {
            auto* pawn=rule.pawn.Get();auto* controller=rule.controller.Get();
            if(!hook_player_matches(pawn,controller) || !hook_seal_matches(controller,rule.seal)) continue;
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
bool ExtensionBridge::stop_hooks() noexcept {
    bool clean=true;
    for(auto it=hooks_.begin();it!=hooks_.end();) {
        for(auto& [id,rule]:it->second->rules) rule.enabled=false;
        if(hook_host_ && hook_host_->remove(hook_host_->context,it->second->token)) it=hooks_.erase(it);
        else {clean=false;++it;}
    }
    return clean;
}
Json ExtensionBridge::hook_request(const Json& request) {
    const auto op=request.at("op").get<std::string>();
    const auto owner=request.value("extension",std::string("css.development"));
    if(op=="hooks.status") {
        Json result={{"available",hook_host_!=nullptr},{"rules",Json::array()}};
        for(const auto& [fn,group]:hooks_) for(const auto& [id,rule]:group->rules) if(rule.extension==owner)
            result["rules"].push_back({{"id",id},{"enabled",rule.enabled},{"failed",rule.failed},{"calls",rule.calls},
                                       {"live",rule.target.Get()!=nullptr}});
        return result;
    }
    if(!hook_host_) throw std::runtime_error("CSSX combat hooks need the updated CSS loader. Install the complete CSS package and restart the game.");
    if(op=="hooks.clear") {
        std::vector<uint64_t> ids;
        for(const auto& [fn,group]:hooks_) for(const auto& [id,rule]:group->rules) if(rule.extension==owner) ids.push_back(id);
        for(auto id:ids) hook_request({{"op","hooks.remove"},{"extension",owner},{"id",id}});
        return true;
    }
    if(op=="hooks.remove") {
        const auto id=request.at("id").get<uint64_t>();
        for(auto it=hooks_.begin();it!=hooks_.end();++it) {
            auto& group=*it->second;auto found=group.rules.find(id);if(found==group.rules.end()) continue;
            if(found->second.extension!=owner) throw std::runtime_error("Hook belongs to another extension.");
            if(found->second.busy) throw std::runtime_error("Hook is executing; retry cleanup on the next tick.");
            found->second.enabled=false;
            if(group.rules.size()==1) {
                if(!hook_host_->remove(hook_host_->context,group.token)) throw std::runtime_error("Hook cleanup is pending; retry before unloading.");
                hooks_.erase(it);
            } else group.rules.erase(found);
            return true;
        }
        return true;
    }
    if(op!="hooks.add") throw std::runtime_error("Unknown CSSX hook operation");
    auto* target=resolve(request.at("target"));auto* pawn=resolve(request.at("pawn"));auto* controller=resolve(request.at("controller"));
    if(!target || target->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Hooks require a live instance, not a default object.");
    if(!hook_player_matches(pawn,controller)) throw std::runtime_error("Hook player ownership changed.");
    size_t count=0;for(const auto& [fn,group]:hooks_) count+=group->rules.size();
    if(count>=1024) throw std::runtime_error("CSSX hook rule limit reached.");
    auto name=wide(request.at("function").get<std::string>());
    if(name.empty() || name.size()>128 || name.find(L'\0')!=std::wstring::npos) throw std::runtime_error("Invalid hook function name");
    auto* fn=target->GetFunctionByNameInChain(name.c_str());
    if(!fn || fn->HasAnyFunctionFlags(FUNC_Delegate|FUNC_MulticastDelegate)) throw std::runtime_error("Hook function is missing or unsupported.");
    Call signature(target,name.c_str(),fn->GetNumParms());
    HookGroup::Rule rule;rule.target=target;rule.pawn=pawn;rule.controller=controller;rule.extension=owner;
    rule.seal=wide(request.value("seal",std::string{}));
    if(rule.seal.size()>128 || rule.seal.find(L'\0')!=std::wstring::npos || !hook_seal_matches(controller,rule.seal))
        throw std::runtime_error("Equip the matching seal before enabling this cheat.");
    const auto mode=request.at("mode").get<std::string>();
    if(mode=="bool") {
        auto* p=signature.param(L"ReturnValue");
        if(!p->IsA<FBoolProperty>() || !p->HasAnyPropertyFlags(CPF_ReturnParm) || p->GetElementSize()!=1)
            throw std::runtime_error("Hook requires a reflected boolean return value.");
        rule.result=static_cast<FBoolProperty*>(p);rule.value=request.at("value").get<bool>();
    } else if(mode=="after") {
        auto* owner=signature.param(L"Owner");
        if(!owner->IsA<FObjectProperty>() || owner->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm))
            throw std::runtime_error("Hook source requires an Owner parameter.");
        rule.owner=static_cast<FObjectProperty*>(owner);
        rule.after=wide(request.at("after").get<std::string>());
        if(rule.after.empty() || rule.after.size()>128 || rule.after==name || rule.after.find(L'\0')!=std::wstring::npos)
            throw std::runtime_error("Invalid hook follow-up function");
        Call after(target,rule.after.c_str(),1);auto* p=after.param(L"Owner");
        if(!p->IsA<FObjectProperty>() || p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm) || p->GetElementSize()!=sizeof(UObject*) ||
           !pawn->IsA(static_cast<FObjectProperty*>(p)->GetPropertyClass().Get()))
            throw std::runtime_error("Hook follow-up requires one matching Owner parameter.");
    } else throw std::runtime_error("Unsupported hook mode");
    auto it=hooks_.find(fn);
    if(it==hooks_.end()) {
        if(hooks_.size()>=128) throw std::runtime_error("CSSX hooked-function limit reached.");
        auto group=std::make_unique<HookGroup>();
        it=hooks_.emplace(fn,std::move(group)).first;
        it->second->token=hook_host_->add(hook_host_->context,fn,hook_callback,it->second.get());
        if(!it->second->token) {hooks_.erase(it);throw std::runtime_error("CSS loader could not register the hook.");}
    }
    const auto id=next_hook_++;
    try {it->second->rules.emplace(id,std::move(rule));}
    catch(...) {
        if(it->second->rules.empty() && hook_host_->remove(hook_host_->context,it->second->token)) hooks_.erase(it);
        throw;
    }
    return id;
}
}

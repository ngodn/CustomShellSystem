// Included after the reflected-call helpers in engine.cpp.
void GenessaRevival::predicate(void* user,void* context,void*,void* result) noexcept {
    auto& self=*static_cast<GenessaRevival*>(user);
    if(!self.armed_ || !result || !self.result_) return;
    auto* ability=static_cast<UObject*>(context);
    auto* pawn=self.pawn_.Get();
    auto* type=self.ability_class_.Get();
    if(!pawn || !type || !ability || ability->GetClassPrivate()!=type) return;
    try {
        Call owner(ability,L"GetSpartaCharacterFromActorInfo",1);owner.run();
        if(owner.get<UObject*>()!=pawn) return;
        auto* property=static_cast<FBoolProperty*>(self.result_);
        if(!property->GetPropertyValue(result)) {
            property->SetPropertyValue(result,true);
            ++self.bypasses_;
        }
    } catch(...) {}
}

void GenessaRevival::release() {
    armed_=false;
    if(hook_ && (!host_ || !host_->remove(host_->context,hook_)))
        throw std::runtime_error("Genessa revival hook could not be released");
    hook_=0;host_=nullptr;result_=nullptr;
    pawn_=nullptr;ability_class_=nullptr;
}

unsigned GenessaRevival::revive(UObject* pawn,const CssHookHost* host) {
    release();
    if(!pawn || !host || host->abi!=1 || host->size<sizeof(CssHookHost) || !host->add || !host->remove)
        throw std::runtime_error("Faithful override requires the matching CSS loader hook service");
    auto* ability=find(L"/Game/Sparta/Core/Player/Ability/ShellRevive/GA_ShellRevive_Genessa.Default__GA_ShellRevive_Genessa_C");
    const auto& entry=resolve_call(ability,L"K2_CanActivateAbility");
    if(entry.function->GetPathName()!=L"/Game/Sparta/Core/Player/Ability/ShellRevive/GA_ShellReviveBase.GA_ShellReviveBase_C:K2_CanActivateAbility")
        throw std::runtime_error("Genessa revival predicate has changed; override was not applied");
    Call predicate_frame(ability,L"K2_CanActivateAbility",4);
    auto* result=predicate_frame.param(L"ReturnValue");
    if(!result->IsA<FBoolProperty>() || !result->HasAnyPropertyFlags(CPF_ReturnParm) ||
       result->GetArrayDim()!=1 || result->GetElementSize()!=sizeof(bool) ||
       !static_cast<FBoolProperty*>(result)->IsNativeBool())
        throw std::runtime_error("Genessa revival return type is unsupported");
    Call revive(find(L"/Game/Sparta/Core/Player/BPFL_Player.Default__BPFL_Player_C"),L"ForceReviveShell",2);
    revive.set(L"Silent",true);revive.set(L"__WorldContext",pawn);
    pawn_=pawn;ability_class_=ability->GetClassPrivate();result_=result;
    host_=host;bypasses_=0;
    hook_=host_->add(host_->context,entry.function,predicate,this);
    if(!hook_) {release();throw std::runtime_error("Genessa revival hook is unavailable");}
    try {
        // The exception lasts only for this call. Alteration keeps ownership of its effects.
        armed_=true;
        revive.run();
        release();
    } catch(...) {release();throw;}
    if(!bypasses_) throw std::runtime_error("Faithful override did not reach the expected revival check");
    return bypasses_;
}

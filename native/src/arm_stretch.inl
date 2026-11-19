namespace {
FProperty* arm_member(UStruct* type,const wchar_t* name) {
    auto* p=type ? type->GetPropertyByNameInChain(name) : nullptr;
    if(!p || p->GetArrayDim()!=1 || p->GetElementSize()<=0 || p->GetOffset_Internal()<0 ||
       p->GetOffset_Internal()>type->GetPropertiesSize()-p->GetElementSize())
        throw std::runtime_error("Player arm IK member layout changed: "+narrow(name));
    return p;
}
}

void PlayerArmStretch::resolve(UObject* instance) {
    auto* cls=instance->GetClassPrivate();
    if(cls->GetPathName()!=L"/Game/Sparta/Characters/Humans/Player/Animations/ABP_Player.ABP_Player_C")
        throw std::runtime_error("Player mesh has a different animation class");
    auto* type=static_cast<UScriptStruct*>(find(L"/Script/AnimGraphRuntime.AnimNode_TwoBoneIK"));
    auto* allow=arm_member(type,L"bAllowStretching");
    auto* maximum=arm_member(type,L"MaxStretchScale");
    auto* bone=arm_member(type,L"IKBone");
    if(!allow->IsA<FBoolProperty>() || !maximum->IsA<FDoubleProperty>() || maximum->GetElementSize()!=sizeof(double) ||
       !bone->IsA<FStructProperty>())
        throw std::runtime_error("Player arm IK settings have different types");
    auto* name=arm_member(static_cast<FStructProperty*>(bone)->GetStruct().Get(),L"BoneName");
    if(!name->IsA<FNameProperty>() || name->GetElementSize()!=sizeof(FName) ||
       name->GetOffset_Internal()>bone->GetElementSize()-name->GetElementSize())
        throw std::runtime_error("Player arm IK bone reference layout changed");
    std::array<int32_t,2> offsets{};
    for(size_t i=0;i<arm_ik_nodes.size();++i) {
        auto* p=idle_graph_property<FProperty>(cls->ForEachProperty(),arm_ik_nodes[i],
            [](FProperty* prop){return prop->GetName();});
        if(!p || !p->IsA<FStructProperty>() || static_cast<FStructProperty*>(p)->GetStruct().Get()!=type ||
           p->GetArrayDim()!=1 || p->GetElementSize()<type->GetPropertiesSize() || p->GetOffset_Internal()<0 ||
           p->GetOffset_Internal()>cls->GetPropertiesSize()-p->GetElementSize())
            throw std::runtime_error("Player arm IK node layout changed");
        offsets[i]=p->GetOffset_Internal();
        FName end;
        std::memcpy(&end,reinterpret_cast<std::byte*>(instance)+offsets[i]+bone->GetOffset_Internal()+
                    name->GetOffset_Internal(),sizeof(end));
        if(end!=FName(i==0 ? L"hand_l" : L"hand_r"))
            throw std::runtime_error("Player arm IK node drives a different bone");
    }
    offsets_=offsets;
    allow_=allow;
    maximum_=maximum;
}
ArmStretchPair PlayerArmStretch::read_values(UObject* instance) const {
    ArmStretchPair values{};
    for(size_t i=0;i<values.size();++i) {
        auto* node=reinterpret_cast<std::byte*>(instance)+offsets_[i];
        values[i].allow=static_cast<FBoolProperty*>(allow_)->GetPropertyValueInContainer(node);
        std::memcpy(&values[i].maximum,node+maximum_->GetOffset_Internal(),sizeof(double));
    }
    return values;
}
void PlayerArmStretch::write_values(UObject* instance,const ArmStretchPair& values) const {
    for(size_t i=0;i<values.size();++i) {
        auto* node=reinterpret_cast<std::byte*>(instance)+offsets_[i];
        static_cast<FBoolProperty*>(allow_)->SetPropertyValueInContainer(node,values[i].allow);
        std::memcpy(node+maximum_->GetOffset_Internal(),&values[i].maximum,sizeof(double));
    }
}
void PlayerArmStretch::release() {
    if(auto* instance=instance_.Get(); instance && lease_.bound()) {
        join_player_animation(instance);
        const auto current=read_values(instance);
        const auto restore=lease_.restoration(current);
        if(restore!=current) write_values(instance,restore);
    }
    lease_.reset();
    instance_.Reset();
}
void PlayerArmStretch::sync(UObject* instance) {
    if(!instance || instance_.Get()!=instance) release();
    if(!instance) return;
    // Both stretch inputs are constant in the shipped graph. Only the IK alphas have copy
    // handlers. Skip the worker join on steady frames; join and reread before every write.
    if(lease_.bound() && read_values(instance)==locked_arm_pair) return;
    join_player_animation(instance);
    if(!lease_.bound()) {
        resolve(instance);
        instance_=instance;
    }
    const auto current=read_values(instance);
    lease_.lock(current);
    if(current!=locked_arm_pair) write_values(instance,locked_arm_pair);
}
Json PlayerArmStretch::inspect(UObject* instance) const {
    Json result={{"applied",false}};
    if(!instance) return result;
    result["instance"]=narrow(instance->GetPathName());
    try {
        join_player_animation(instance);
        const bool owned=instance==instance_.Get() && lease_.bound();
        PlayerArmStretch probe;
        if(!owned) probe.resolve(instance);
        const auto current=(owned ? *this : probe).read_values(instance);
        result["applied"]=owned && current==locked_arm_pair;
        for(size_t i=0;i<current.size();++i)
            result[i==0?"left":"right"]={{"allow_stretching",current[i].allow},{"max_stretch_scale",current[i].maximum}};
    } catch(const std::exception& error) { result["readback_error"]=error.what(); }
    return result;
}
UObject* Appearance::arm_animation() const {
    auto* pawn=shell.empty() ? nullptr : observed_pawn_.Get();
    auto* controller=pawn ? observed_controller_.Get() : nullptr;
    auto* mesh=controller && read<UObject*>(controller,L"Pawn")==pawn ? read<UObject*>(pawn,L"Mesh") : nullptr;
    return mesh ? read<UObject*>(mesh,L"AnimScriptInstance") : nullptr;
}
void Appearance::sync_arm_stretch(bool locked) {
    arm_stretch_.sync(locked ? arm_animation() : nullptr);
}

// Included inside engine.cpp's anonymous namespace. Node views are used only
// during this call; persistent state contains weak instance identity and values.
struct DynamicsNode {
    std::byte* data=nullptr;
    std::array<int32_t,4> offsets{};
    std::array<FBoolProperty*,4> flags{};
    DynamicsSettings capture() const {
        std::array<float,4> v;
        for(size_t i=0;i<v.size();++i) {
            std::memcpy(&v[i],data+offsets[i],sizeof(float));
            if(!std::isfinite(v[i])) throw std::runtime_error("Non-finite authored dynamics value");
        }
        return {v[0],v[1],v[2],v[3],flags[0]->GetPropertyValueInContainer(data),
            flags[1]->GetPropertyValueInContainer(data),flags[2]->GetPropertyValueInContainer(data),
            flags[3]->GetPropertyValueInContainer(data)};
    }
    void apply(const DynamicsSettings& value) const {
        const float v[]={value.angular_spring,value.linear_damping,value.angular_damping,value.gravity};
        const bool f[]={value.spring_enabled,value.override_linear,value.override_angular,value.gravity_override};
        for(size_t i=0;i<offsets.size();++i) {
            std::memcpy(data+offsets[i],&v[i],sizeof(float));
            flags[i]->SetPropertyValueInContainer(data,f[i]);
        }
        if(capture()!=value) throw std::runtime_error("Dynamics setting read-back failed");
    }
};

std::map<std::string,DynamicsNode> dynamics_nodes(UObject* anim) {
    std::map<std::string,DynamicsNode> result;
    if(!anim) return result;
    auto* node_type=static_cast<UScriptStruct*>(find(L"/Script/AnimGraphRuntime.AnimNode_AnimDynamics"));
    auto* bone_type=static_cast<UScriptStruct*>(find(L"/Script/Engine.BoneReference"));
    auto bounded=[](FProperty* p,int32_t size) {
        return p && p->GetArrayDim()==1 && p->GetOffset_Internal()>=0 &&
               p->GetElementSize()>0 && p->GetOffset_Internal()<=size-p->GetElementSize();
    };
    auto* bound=node_type->GetPropertyByNameInChain(L"BoundBone");
    auto* name=field(bone_type,L"BoneName",sizeof(FName));
    if(!bounded(bound,node_type->GetPropertiesSize()) || !bound->IsA<FStructProperty>() ||
       static_cast<FStructProperty*>(bound)->GetStruct().Get()!=bone_type ||
       !bounded(name,bound->GetElementSize()) || !name->IsA<FNameProperty>())
        throw std::runtime_error("Dynamics bound-bone layout mismatch");
    DynamicsNode prototype;
    const wchar_t* numbers[]={L"AngularSpringConstant",L"LinearDampingOverride",L"AngularDampingOverride",L"GravityScale"};
    const wchar_t* flags[]={L"bAngularSpring",L"bOverrideLinearDamping",L"bOverrideAngularDamping",L"bUseGravityOverride"};
    for(size_t i=0;i<prototype.offsets.size();++i) {
        auto* p=field(node_type,numbers[i],sizeof(float));
        auto* flag=node_type->GetPropertyByNameInChain(flags[i]);
        if(!bounded(p,node_type->GetPropertiesSize()) || !p->IsA<FFloatProperty>() ||
           !bounded(flag,node_type->GetPropertiesSize()) || !flag->IsA<FBoolProperty>())
            throw std::runtime_error("Dynamics setting layout mismatch");
        prototype.offsets[i]=p->GetOffset_Internal();
        prototype.flags[i]=static_cast<FBoolProperty*>(flag);
    }
    auto* type=anim->GetClassPrivate();
    if(!type) throw std::runtime_error("Dynamics instance has no class");
    for(auto* p:type->ForEachProperty()) {
        if(!p->IsA<FStructProperty>() || static_cast<FStructProperty*>(p)->GetStruct().Get()!=node_type) continue;
        if(!bounded(p,type->GetPropertiesSize()) || p->GetElementSize()!=node_type->GetPropertiesSize())
            throw std::runtime_error("Dynamics instance layout mismatch");
        auto node=prototype;
        node.data=reinterpret_cast<std::byte*>(anim)+p->GetOffset_Internal();
        FName root;
        std::memcpy(&root,node.data+bound->GetOffset_Internal()+name->GetOffset_Internal(),sizeof(root));
        const auto key=narrow(root.ToString());
        if(key.empty() || key=="None") continue;
        if(result.size()>=32 || !result.emplace(key,node).second)
            throw std::runtime_error("Dynamics roots are ambiguous or exceed the node limit");
    }
    return result;
}

void reset_dynamics(UObject* instance,bool invoke=true) {
    Call reset(instance,L"ResetDynamics",1);
    auto* parameter=reset.param(L"InTeleportType");
    UEnum* enumeration=nullptr;
    if(parameter->IsA<FByteProperty>()) enumeration=static_cast<FByteProperty*>(parameter)->GetEnum().Get();
    else if(parameter->IsA<FEnumProperty>()) enumeration=static_cast<FEnumProperty*>(parameter)->GetEnum().Get();
    if(!enumeration || parameter->GetElementSize()!=sizeof(uint8_t) || enumeration->NumEnums()>16)
        throw std::runtime_error("Dynamics reset enum layout mismatch");
    std::optional<uint8_t> reset_value;
    for(int32_t i=0;i<enumeration->NumEnums();++i) {
        const auto entry=enumeration->GetEnumNameByIndex(i);
        const auto name=narrow(entry.Key.ToString());
        if(name=="ResetPhysics" || name=="ETeleportType::ResetPhysics") {
            if(entry.Value<0 || entry.Value>255 || reset_value) throw std::runtime_error("Invalid dynamics reset enum value");
            reset_value=static_cast<uint8_t>(entry.Value);
        }
    }
    if(!reset_value) throw std::runtime_error("Dynamics reset does not expose ResetPhysics");
    reset.set(L"InTeleportType",*reset_value);
    if(invoke) reset.run();
}

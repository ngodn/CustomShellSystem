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

FProperty* control_rig_reset_epoch(UObject* instance,const wchar_t* field_name=L"CSSResetEpoch") {
    // This optional AnimBP input opts into the CSS ControlRig reset contract.
    // Resolve only on a reset request, never by scanning live objects per frame.
    auto* owner=instance->GetClassPrivate();
    if(!owner) throw std::runtime_error("Dynamics instance has no class");
    const FName wanted(field_name);
    unsigned depth=0,count=0;
    for(UStruct* type=owner;type;type=type->GetSuperStruct()) {
        if(++depth>64) throw std::runtime_error("Dynamics class hierarchy exceeds bound");
        for(auto* p:type->ForEachProperty()) {
            if(++count>4096) throw std::runtime_error("Dynamics property count exceeds bound");
            // Include the FName number. CSSResetEpoch_0 is a different field.
            if(p->GetFName()!=wanted) continue;
            if(!p->IsA<FIntProperty>() || p->GetArrayDim()!=1 || p->GetElementSize()!=sizeof(int32_t) ||
               p->GetOffset_Internal()<0 || p->GetOffset_Internal()>owner->GetPropertiesSize()-int32_t(sizeof(int32_t)))
                throw std::runtime_error("CSS ControlRig reset epoch layout mismatch");
            return p;
        }
    }
    return nullptr;
}

void advance_rig_epoch(UObject* instance,FProperty* epoch) {
    auto* data=reinterpret_cast<std::byte*>(instance)+epoch->GetOffset_Internal();
    int32_t previous{};
    std::memcpy(&previous,data,sizeof(previous));
    const int32_t next=previous==std::numeric_limits<int32_t>::max()?0:previous+1;
    epoch->CopyCompleteValue(data,&next);
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
    auto* epoch=control_rig_reset_epoch(instance);
    auto* body_epoch=control_rig_reset_epoch(instance,L"CSSBodyResetEpoch");
    if(!invoke) return;
    if(instance->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Dynamics reset requires an instance");
    if(epoch) advance_rig_epoch(instance,epoch);
    if(body_epoch) advance_rig_epoch(instance,body_epoch);
    reset.run();
}

// Resolve the authored AnimBP inputs on demand. No graph-node pointers or
// reflected property handles survive this call's owning instance.
struct RigInputs {
    UObject* instance;
    FFloatProperty* stiffness;
    FFloatProperty* damping;
    FStructProperty* gravity;
    FBoolProperty* enabled;
    RigSettings capture() const {
        RigSettings value;
        std::memcpy(&value.stiffness,reinterpret_cast<const std::byte*>(instance)+stiffness->GetOffset_Internal(),sizeof(float));
        std::memcpy(&value.damping,reinterpret_cast<const std::byte*>(instance)+damping->GetOffset_Internal(),sizeof(float));
        std::memcpy(value.gravity.data(),reinterpret_cast<const std::byte*>(instance)+gravity->GetOffset_Internal(),sizeof(value.gravity));
        value.enabled=enabled->GetPropertyValueInContainer(instance);
        if(!std::isfinite(value.stiffness) || !std::isfinite(value.damping) ||
           !std::all_of(value.gravity.begin(),value.gravity.end(),[](double v){return std::isfinite(v);}))
            throw std::runtime_error("Non-finite authored rig inputs");
        return value;
    }
    void apply(const RigSettings& value) const {
        stiffness->CopyCompleteValue(reinterpret_cast<std::byte*>(instance)+stiffness->GetOffset_Internal(),&value.stiffness);
        damping->CopyCompleteValue(reinterpret_cast<std::byte*>(instance)+damping->GetOffset_Internal(),&value.damping);
        gravity->CopyCompleteValue(reinterpret_cast<std::byte*>(instance)+gravity->GetOffset_Internal(),value.gravity.data());
        enabled->SetPropertyValueInContainer(instance,value.enabled);
        if(capture()!=value) throw std::runtime_error("Rig input read-back failed");
    }
};

RigInputs rig_inputs(UObject* instance) {
    if(!instance || !instance->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimInstance"))) ||
       instance->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Rig inputs require an animation instance");
    auto* owner=instance->GetClassPrivate();
    if(!owner || !narrow(owner->GetPathName()).starts_with("/Game/CSS/"))
        throw std::runtime_error("Rig inputs require an authored CSS animation class");
    std::array<FProperty*,4> fields{};
    const std::array<FName,4> names={FName(L"CSSStiffness"),FName(L"CSSDamping"),FName(L"CSSGravity"),FName(L"CSSEnabled")};
    unsigned depth=0,count=0;
    for(UStruct* type=owner;type;type=type->GetSuperStruct()) {
        if(++depth>64) throw std::runtime_error("Rig class hierarchy exceeds bound");
        for(auto* p:type->ForEachProperty()) {
            if(++count>4096) throw std::runtime_error("Rig property count exceeds bound");
            for(size_t i=0;i<names.size();++i) if(p->GetFName()==names[i]) {
                if(fields[i] && fields[i]!=p) throw std::runtime_error("Ambiguous rig input");
                fields[i]=p;
            }
        }
    }
    for(auto* p:fields)
        if(!p || p->GetArrayDim()!=1 || p->GetElementSize()<=0 || p->GetOffset_Internal()<0 ||
           p->GetOffset_Internal()>owner->GetPropertiesSize()-p->GetElementSize())
            throw std::runtime_error("Rig input layout mismatch");
    if(!fields[0]->IsA<FFloatProperty>() || fields[0]->GetElementSize()!=sizeof(float) ||
       !fields[1]->IsA<FFloatProperty>() || fields[1]->GetElementSize()!=sizeof(float) ||
       !fields[2]->IsA<FStructProperty>() || fields[2]->GetElementSize()!=sizeof(std::array<double,3>) ||
       static_cast<FStructProperty*>(fields[2])->GetStruct().Get()!=find(L"/Script/CoreUObject.Vector") ||
       !fields[3]->IsA<FBoolProperty>() || fields[3]->GetElementSize()!=sizeof(uint8_t) || !control_rig_reset_epoch(instance))
        throw std::runtime_error("Rig input types do not match the CSS contract");
    return {instance,static_cast<FFloatProperty*>(fields[0]),static_cast<FFloatProperty*>(fields[1]),
        static_cast<FStructProperty*>(fields[2]),static_cast<FBoolProperty*>(fields[3])};
}

struct BodyRigInputs {
    UObject* instance;
    std::array<FArrayProperty*,4> arrays;
    FBoolProperty* use_regions;
    std::array<FFloatProperty*,3> globals;
    FProperty* epoch;
    static bool same_owned(const BodyRigSettings& a,const BodyRigSettings& b) {
        return a.frequency==b.frequency && a.damping==b.damping && a.motion==b.motion &&
               a.enabled==b.enabled && a.use_regions==b.use_regions;
    }
    BodyRigSettings capture() const {
        BodyRigSettings value;
        const std::array<std::array<float,7>*,3> numbers={&value.frequency,&value.damping,&value.motion};
        for(size_t field=0;field<arrays.size();++field) {
            auto* property=arrays[field];
            FScriptArrayHelper values(property,reinterpret_cast<std::byte*>(instance)+property->GetOffset_Internal());
            if(values.Num()!=7) throw std::runtime_error("Body rig requires seven region values");
            if(!values.GetRawPtr(0)) throw std::runtime_error("Body region array has no storage");
            for(int32_t i=0;i<7;++i) {
                if(field<3) {
                    float v{};std::memcpy(&v,values.GetRawPtr(i),sizeof(v));
                    if(!std::isfinite(v)) throw std::runtime_error("Non-finite body region input");
                    (*numbers[field])[size_t(i)]=v;
                } else value.enabled[size_t(i)]=static_cast<FBoolProperty*>(property->GetInner())->GetPropertyValue(values.GetRawPtr(i));
            }
        }
        const std::array<float*,3> fields={&value.global_frequency,&value.global_damping,&value.global_motion};
        for(size_t i=0;i<3;++i) {
            std::memcpy(fields[i],reinterpret_cast<std::byte*>(instance)+globals[i]->GetOffset_Internal(),sizeof(float));
            if(!std::isfinite(*fields[i])) throw std::runtime_error("Non-finite global body input");
        }
        value.use_regions=use_regions->GetPropertyValueInContainer(instance);
        return value;
    }
    void apply(const BodyRigSettings& value) const {
        if(same_owned(capture(),value)) return;
        const std::array<const std::array<float,7>*,3> numbers={&value.frequency,&value.damping,&value.motion};
        for(auto* values:numbers) for(float v:*values)
            if(!std::isfinite(v)) throw std::runtime_error("Non-finite body setting");
        for(size_t field=0;field<arrays.size();++field) {
            auto* property=arrays[field];
            FScriptArrayHelper values(property,reinterpret_cast<std::byte*>(instance)+property->GetOffset_Internal());
            for(int32_t i=0;i<7;++i) {
                if(field<3) property->GetInner()->CopyCompleteValue(values.GetRawPtr(i),&(*numbers[field])[size_t(i)]);
                else static_cast<FBoolProperty*>(property->GetInner())->SetPropertyValue(values.GetRawPtr(i),value.enabled[size_t(i)]);
            }
        }
        use_regions->SetPropertyValueInContainer(instance,value.use_regions);
        if(!same_owned(capture(),value)) throw std::runtime_error("Body rig input read-back failed");
        advance_rig_epoch(instance,epoch);
    }
};

BodyRigInputs body_rig_inputs(UObject* instance) {
    if(!instance || !instance->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimInstance"))) ||
       instance->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Body rig inputs require an animation instance");
    auto* owner=instance->GetClassPrivate();
    if(!owner || !narrow(owner->GetPathName()).starts_with("/Game/CSS/"))
        throw std::runtime_error("Body rig inputs require an authored CSS animation class");
    const std::array<FName,8> names={FName(L"CSSBodyRegionFrequencies"),FName(L"CSSBodyRegionDampingRatios"),
        FName(L"CSSBodyRegionMotionAmounts"),FName(L"CSSBodyRegionEnabled"),FName(L"CSSBodyUseRegionSettings"),
        FName(L"CSSBodyFrequency"),FName(L"CSSBodyDampingRatio"),FName(L"CSSBodyMotionAmount")};
    std::array<FProperty*,8> fields{};
    unsigned depth=0,count=0;
    for(UStruct* type=owner;type;type=type->GetSuperStruct()) {
        if(++depth>64) throw std::runtime_error("Body rig hierarchy exceeds bound");
        for(auto* property:type->ForEachProperty()) {
            if(++count>4096) throw std::runtime_error("Body rig property count exceeds bound");
            for(size_t i=0;i<names.size();++i) if(property->GetFName()==names[i]) {
                if(fields[i] && fields[i]!=property) throw std::runtime_error("Ambiguous body rig input");
                fields[i]=property;
            }
        }
    }
    for(auto* p:fields)
        if(!p || p->GetArrayDim()!=1 || p->GetElementSize()<=0 || p->GetOffset_Internal()<0 ||
           p->GetOffset_Internal()>owner->GetPropertiesSize()-p->GetElementSize())
            throw std::runtime_error("Body rig input layout mismatch");
    std::array<FArrayProperty*,4> arrays{};
    for(size_t i=0;i<4;++i) {
        if(!fields[i]->IsA<FArrayProperty>() || fields[i]->GetElementSize()!=sizeof(TArray<float>))
            throw std::runtime_error("Body rig requires array inputs");
        arrays[i]=static_cast<FArrayProperty*>(fields[i]);
        auto* inner=arrays[i]->GetInner();
        if(!inner || inner->GetArrayDim()!=1 || inner->GetOffset_Internal()!=0 ||
           (i<3 ? !inner->IsA<FFloatProperty>() || inner->GetElementSize()!=sizeof(float)
                : !inner->IsA<FBoolProperty>() || inner->GetElementSize()!=sizeof(bool)))
            throw std::runtime_error("Body rig array element mismatch");
    }
    if(!fields[4]->IsA<FBoolProperty>() || fields[4]->GetElementSize()!=sizeof(bool))
        throw std::runtime_error("Body region switch layout mismatch");
    std::array<FFloatProperty*,3> globals{};
    for(size_t i=0;i<3;++i) {
        if(!fields[i+5]->IsA<FFloatProperty>() || fields[i+5]->GetElementSize()!=sizeof(float))
            throw std::runtime_error("Global body input layout mismatch");
        globals[i]=static_cast<FFloatProperty*>(fields[i+5]);
    }
    auto* epoch=control_rig_reset_epoch(instance,L"CSSBodyResetEpoch");
    if(!epoch) throw std::runtime_error("Body rig requires an independent reset epoch");
    return {instance,arrays,static_cast<FBoolProperty*>(fields[4]),globals,epoch};
}

#include "engine_body_geometry.inl"

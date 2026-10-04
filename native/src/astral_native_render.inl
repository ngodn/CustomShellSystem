// Keep the hidden native combat mesh evaluating for its visible pose follower.
namespace {
uint8_t astral_required_tick(UObject* component) {
    auto* property=field(component,L"VisibilityBasedAnimTickOption",sizeof(uint8_t));
    UEnum* enumeration=nullptr;
    if(property->IsA<FEnumProperty>()) enumeration=static_cast<FEnumProperty*>(property)->GetEnum().Get();
    else if(property->IsA<FByteProperty>()) enumeration=static_cast<FByteProperty*>(property)->GetEnum().Get();
    if(!enumeration || enumeration->NumEnums()<1 || enumeration->NumEnums()>16)
        throw std::runtime_error("Astral native tick enum layout mismatch");
    std::optional<uint8_t> result;
    for(int32_t i=0;i<enumeration->NumEnums();++i) {
        const auto entry=enumeration->GetEnumNameByIndex(i);
        const auto name=narrow(entry.Key.ToString());
        if(name=="AlwaysTickPoseAndRefreshBones" ||
           name=="EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones") {
            if(result || entry.Value<0 || entry.Value>255)
                throw std::runtime_error("Astral native tick enum value is invalid");
            result=uint8_t(entry.Value);
        }
    }
    if(!result) throw std::runtime_error("Astral native pose refresh option is absent");
    return *result;
}
}
void AstralNativeRenderLease::acquire(UObject* component) {
    if(component_.ObjectIndex>=0) throw std::runtime_error("Restore the prior Astral native render lease first");
    if(!component || WeakObject(component).Get()!=component ||
       !component->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"))))
        throw std::runtime_error("Astral native render lease requires a skeletal component");
    auto* owner=astral_binding_owner(component);
    auto* mesh=mesh_asset(component);
    auto* instance=astral_anim_instance(component);
    Call enabled(component,L"IsComponentTickEnabled",1);enabled.run();
    if(!owner || !mesh || !instance || !enabled.get<bool>())
        throw std::runtime_error("Astral native combat pose is not active");
    required_tick_=astral_required_tick(component);
    tick_=read<uint8_t>(component,L"VisibilityBasedAnimTickOption");
    visible_=astral_source_flag(component,L"bVisible");
    optimized_=astral_source_flag(component,L"bEnableUpdateRateOptimizations");
    component_=component;owner_=owner;mesh_=mesh;instance_=instance;
    try {
        tick_written_=true;write_field(component,L"VisibilityBasedAnimTickOption",required_tick_);
        optimization_written_=true;astral_visual_flag(component,L"bEnableUpdateRateOptimizations",false);
        visibility_written_=true;astral_visual_visibility(component,false);
        if(!intact()) throw std::runtime_error("Astral native render lease read-back failed");
    } catch(...) { restore();throw; }
}
bool AstralNativeRenderLease::intact() const {
    auto* component=component_.Get();
    return component && owner_.Get() && mesh_.Get() && instance_.Get() &&
        astral_binding_owner(component)==owner_.Get() && mesh_asset(component)==mesh_.Get() &&
        astral_anim_instance(component)==instance_.Get() &&
        !astral_source_flag(component,L"bVisible") &&
        !astral_source_flag(component,L"bEnableUpdateRateOptimizations") &&
        read<uint8_t>(component,L"VisibilityBasedAnimTickOption")==required_tick_;
}
bool AstralNativeRenderLease::restore() noexcept {
    try {
        if(auto* component=component_.Get()) {
            if(astral_binding_owner(component)!=owner_.Get()) return false;
            // The flags belong to the component, including across a native
            // mesh reset. Restore only values still equal to our writes.
            if(visibility_written_ && !astral_source_flag(component,L"bVisible"))
                astral_visual_visibility(component,visible_);
            if(optimization_written_ && !astral_source_flag(component,L"bEnableUpdateRateOptimizations"))
                astral_visual_flag(component,L"bEnableUpdateRateOptimizations",optimized_);
            if(tick_written_ && read<uint8_t>(component,L"VisibilityBasedAnimTickOption")==required_tick_)
                write_field(component,L"VisibilityBasedAnimTickOption",tick_);
        }
    } catch(...) { return false; }
    component_=WeakObject{};owner_=WeakObject{};mesh_=WeakObject{};instance_=WeakObject{};
    visibility_written_=optimization_written_=tick_written_=false;
    return true;
}

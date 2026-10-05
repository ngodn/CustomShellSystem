static float facial_weight(UObject* component,const std::string& name) {
    Call get(component,L"GetMorphTarget",2);
    get.set(L"MorphTargetName",FName(wide(name).c_str(),FNAME_Add)); get.run();
    const auto value=get.get<float>();
    if(!std::isfinite(value)) throw std::runtime_error("Invalid live facial weight");
    return value;
}
static void facial_weight(UObject* component,const std::string& name,float value) {
    Call set(component,L"SetMorphTarget",3);
    set.set(L"MorphTargetName",FName(wide(name).c_str(),FNAME_Add));
    set.set(L"Value",value);set.set(L"bRemoveZeroWeight",value==0.f);set.run();
}
void Appearance::restore_facial_binding(FacialBinding& binding) {
    auto* component=binding.component.Get();
    if(component && binding.mesh.Get() && mesh_asset(component)==binding.mesh.Get())
        binding.weights.restore([&](const auto& name){return facial_weight(component,name);},
        [&](const auto& name,float weight){facial_weight(component,name,weight);});
    binding={};
}
void Appearance::write_facial_binding(FacialBinding& binding,UObject* component,
                                     const std::map<std::string,float>& values) {
    auto* mesh=component?mesh_asset(component):nullptr;
    if(!mesh || mesh!=applied_.Get()) return;
    if(binding.component.Get()==component && binding.mesh.Get()==mesh && binding.weights.matches(values)) return;
    if(binding.component.Get()!=component || binding.mesh.Get()!=mesh) {
        restore_facial_binding(binding);
        binding.component=component;binding.mesh=mesh;
    }
    for(const auto& [name,weight]:values) {
        if(!std::isfinite(weight) || std::abs(weight)>4.f || !mesh_has_morph(mesh,name))
            throw std::runtime_error("Facial shape does not match the active mesh");
    }
    binding.weights.apply(values,[&](const auto& name){return facial_weight(component,name);},
        [&](const auto& name,float weight){facial_weight(component,name,weight);});
}
void Appearance::restore_expression() {
    restore_facial_binding(facial_preview_);
    restore_facial_binding(facial_world_);
    facial_weights_.clear();facial_applied_.clear();facial_requested_.clear();facial_reason_.clear();
}
void Appearance::push_facial_morphs(UObject* component) {
    if(facial_weights_.empty() || !component) return;
    if(component==component_.Get()) write_facial_binding(facial_world_,component,facial_weights_);
    else if(component==menu_component_.Get()) write_facial_binding(facial_preview_,component,facial_weights_);
}
void Appearance::set_expression(const FacialPlan& plan) {
    facial_requested_=plan.expression;
    facial_reason_=plan.reason;
    auto* component=component_.Get();
    const bool supported=component && applied_.Get() && mesh_asset(component)==applied_.Get() &&
        narrow(applied_.Get()->GetPathName())==plan.mesh && !plan.weights.empty();
    if(!supported) {
        restore_facial_binding(facial_preview_);restore_facial_binding(facial_world_);
        facial_weights_.clear();facial_applied_.clear();
        if(!plan.expression.empty() && facial_reason_.empty()) {
            const auto error=facial_failures_.find(plan.mesh);
            facial_reason_=error==facial_failures_.end()?"Facial mesh is unavailable":error->second;
        }
        return;
    }
    write_facial_binding(facial_world_,component,plan.weights);
    facial_weights_=plan.weights;facial_applied_=plan.expression;
    push_facial_morphs(menu_component_.Get());
}
Json Appearance::expression_status() const {
    return {{"requested",facial_requested_},{"applied",facial_applied_},{"reason",facial_reason_}};
}

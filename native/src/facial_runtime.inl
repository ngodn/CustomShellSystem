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
    restore_facial_performance();
    restore_facial_binding(facial_preview_);
    restore_facial_binding(facial_world_);
    facial_weights_.clear();facial_applied_.clear();facial_requested_.clear();facial_reason_.clear();
}
void Appearance::push_facial_morphs(UObject* component) {
    if(facial_weights_.empty() || !component) return;
    if(facial_motion_ && component==component_.Get()) return;
    if(component==component_.Get()) write_facial_binding(facial_world_,component,facial_weights_);
    else if(component==menu_component_.Get()) write_facial_binding(facial_preview_,component,facial_weights_);
}
void Appearance::set_expression(const FacialPlan& plan) {
    restore_facial_performance();
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
    return {{"requested",facial_requested_},{"applied",facial_applied_},{"reason",facial_reason_},
        {"performance",facial_motion_.has_value()}};
}
void Appearance::begin_facial_performance(const FacialPerformance& face) {
    restore_facial_performance();
    auto* component=component_.Get();auto* mesh=applied_.Get();
    if(!active() || !mesh || facial_weights_.empty())
        throw std::runtime_error("The performance facial mesh is unavailable");
    std::map<std::string,float> baseline;
    for(const auto& [name,value]:facial_weights_) {
        if(!mesh_has_morph(mesh,name)) throw std::runtime_error("The facial mesh is missing a performance shape");
        baseline.emplace(name,facial_weight(component,name));
    }
    facial_motion_.emplace(face,baseline);
    facial_motion_binding_.component=component;facial_motion_binding_.mesh=mesh;
}
void Appearance::sample_facial_performance(double seconds,double weight) {
    if(!facial_motion_) return;
    auto* component=facial_motion_binding_.component.Get();
    if(!component || component!=component_.Get() || !facial_motion_binding_.mesh.Get() ||
       mesh_asset(component)!=facial_motion_binding_.mesh.Get())
        throw std::runtime_error("The performance facial binding changed");
    const auto& values=facial_motion_->sample(seconds,weight);
    facial_motion_binding_.weights.apply(values,
        [&](const auto& name){return facial_weight(component,name);},
        [&](const auto& name,float value){facial_weight(component,name,value);});
}
void Appearance::restore_facial_performance() {
    restore_facial_binding(facial_motion_binding_);
    facial_motion_.reset();
}
Json Appearance::start_performance(const Catalog& catalog,const std::string& outfit,
    const std::string& id,double rate,double position,const FacialPerformance* face,PerformanceUse use) {
    if(performance.active()) throw std::runtime_error("Stop the current performance first");
    if(const auto why=performance_unavailable(catalog,outfit);!why.empty()) throw std::runtime_error(why);
    if(!face) return performance.start(observed_pawn_.Get(),catalog,outfit,id,rate,position,0.,{},{},use);
    if(face->clip!=id) throw std::runtime_error("Facial performance identity differs from the body clip");
    performance.interrupt("Starting facial performance");
    begin_facial_performance(*face);
    try {
        return performance.start(observed_pawn_.Get(),catalog,outfit,id,rate,position,face->duration,
            [this](double seconds,double weight){sample_facial_performance(seconds,weight);},
            [this]{restore_facial_performance();},use);
    } catch(...) {restore_facial_performance();throw;}
}

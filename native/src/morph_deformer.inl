namespace {
UObject* morph_deformer_instance(UObject* component) {
    if(!component) return nullptr;
    Call get(component,L"GetMeshDeformerInstance",1);get.run();
    auto* instance=get.get<UObject*>();
    if(!instance) return nullptr;
    auto* type=static_cast<UClass*>(find_optional(L"/Script/OptimusCore.OptimusDeformerInstance"));
    return type && instance->IsA(type)?instance:nullptr;
}
bool write_deformer_morph(UObject* instance,const std::string& morph,float value) {
    // Find only: arbitrary source facial curves must not grow Unreal's name pool.
    const FName name(wide("CSS_Morph_"+morph).c_str(),FNAME_Find);
    if(name==FName{}) return false;
    Call set(instance,L"SetFloatVariable",3);
    set.set(L"InVariableName",name);set.set(L"InValue",double(value));set.run();
    return set.get<bool>();
}
}
void MorphDeformerBinding::forget() {
    component_.Reset();mesh_.Reset();instance_.Reset();weights_.forget();
}
void MorphDeformerBinding::release() {
    auto* component=component_.Get();
    auto* instance=instance_.Get();
    if(component && instance && mesh_asset(component)==mesh_.Get() &&
       morph_deformer_instance(component)==instance)
        weights_.sync({},[&](const auto& name,float value){return write_deformer_morph(instance,name,value);});
    forget();
}
void MorphDeformerBinding::sync(UObject* component,const std::map<std::string,float>& values) {
    if(values.empty()) { release();return; }
    if(!component) { forget();return; }
    auto* mesh=mesh_asset(component);
    auto* instance=morph_deformer_instance(component);
    if(!instance) { forget();return; }
    if(component_.Get()!=component || mesh_.Get()!=mesh || instance_.Get()!=instance) {
        forget();component_=component;mesh_=mesh;instance_=instance;
    }
    weights_.sync(values,[&](const auto& name,float value){return write_deformer_morph(instance,name,value);});
}

// Development-only: an unregistered component exercises bindings and rollback.
Json EngineBridge::probe_astral_bindings(void* engine,const Json& request) {
    keep_alive_attach(engine);
    auto* owner=resolve(request.at("owner"));
    auto* mesh=resolve(request.at("mesh"));
    auto* parent=resolve(request.at("material"));
    if(!owner || !owner->IsA(static_cast<UClass*>(find(L"/Script/Engine.Actor"))) ||
       !mesh || !mesh->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMesh"))) ||
       !parent || !parent->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"))))
        throw std::runtime_error("Binding probe requires an actor, mesh and material");
    const auto count=authored_overlays(mesh).size();
    if(!count || count>64) throw std::runtime_error("Binding probe mesh exceeds slot bound");
    const auto retained_before=keep_alive::entries.size();
    AssetLoadRoots roots;
    for(auto* value:{owner,mesh,parent}) astral_retain(roots,value);
    struct ComponentScope {
        WeakObject component,owner;
        bool destroy() noexcept {
            auto* value=component.Get();
            if(!value) return true;
            try {
                Call call(value,L"K2_DestroyComponent",1);call.set(L"Object",owner.Get());call.run();
                component=WeakObject{};return true;
            } catch(...) { return false; }
        }
        ~ComponentScope() { destroy(); }
    } temporary;
    temporary.owner=owner;
    Call transform(find(L"/Script/Engine.Default__KismetMathLibrary"),L"MakeTransform",4);
    transform.set(L"Location",std::array<double,3>{});transform.set(L"Rotation",std::array<double,3>{});
    transform.set(L"Scale",std::array<double,3>{1,1,1});transform.run();
    Call add(owner,L"AddComponentByClass",5);
    add.set(L"Class",find(L"/Script/Engine.SkeletalMeshComponent"));
    add.set(L"bManualAttachment",true);add.set(L"bDeferredFinish",true);
    auto* output=transform.param(L"ReturnValue");auto* input=add.param(L"RelativeTransform");
    if(!input->SameType(output) || input->GetElementSize()!=output->GetElementSize())
        throw std::runtime_error("Binding probe transform layout mismatch");
    input->CopyCompleteValue(add.data(input),transform.data(output));add.run();
    auto* component=add.get<UObject*>();
    if(!component) throw std::runtime_error("Binding probe component creation failed");
    temporary.component=component;
    astral_retain(roots,component);
    Call asset(component,L"SetSkeletalMeshAsset",1);asset.set(L"NewMesh",mesh);asset.run();
    Call collision(component,L"SetCollisionEnabled",1);collision.set(L"NewType",uint8_t{0});collision.run();
    Call tick(component,L"SetComponentTickEnabled",1);tick.set(L"bEnabled",false);tick.run();
    Call visible(component,L"SetVisibility",2);visible.set(L"bNewVisibility",false);
    visible.set(L"bPropagateToChildren",false);visible.run();
    // FinishAddComponent is deliberately omitted: no scene registration,
    // animation tick or collision body is required to test material ownership.
    Call make(find(L"/Script/Engine.Default__KismetMaterialLibrary"),L"CreateDynamicMaterialInstance",5);
    make.set(L"WorldContextObject",owner);make.set(L"Parent",parent);make.run();
    auto* copy=make.get<UObject*>();
    if(!copy || !dynamic_material(copy)) throw std::runtime_error("Binding probe private MID failed");
    astral_retain(roots,copy);
    for(size_t i=0;i<count;++i) material(component,int(i),parent);
    astral_global_overlay(component,parent);
    resize_overlay_slots(component,int(count));
    auto initial=overlay_slots(component);
    for(size_t i=0;i<count;++i) std::memcpy(initial.GetRawPtr(int(i)),&parent,sizeof(parent));
    AstralMaterialBinding binding{component,mesh,std::vector<UObject*>(count,copy),std::vector<UObject*>(count,copy)};
    AstralMaterialBindings bindings;
    std::array<AstralMaterialBinding,2> duplicate{binding,binding};
    bool rejected=false;
    try { bindings.bind(owner,duplicate); }
    catch(const std::exception&) { rejected=true; }
    if(!rejected || bindings.intact() || read<UObject*>(component,L"OverlayMaterial")!=parent)
        throw std::runtime_error("Binding probe duplicate input was not rejected before writes");
    bindings.bind(owner,{&binding,1});
    if(!bindings.intact()) throw std::runtime_error("Binding probe read-back failed");
    if(!bindings.restore() || bindings.intact()) throw std::runtime_error("Binding probe restore failed");
    auto check_original=[&] {
        const auto actual=material_objects(component);auto overlays=overlay_slots(component);
        if(actual.size()<count || overlays.Num()!=int(count) || read<UObject*>(component,L"OverlayMaterial")!=parent)
            throw std::runtime_error("Binding probe original arrays differ");
        for(size_t i=0;i<count;++i) if(actual[i].Get()!=parent || overlay_at(overlays,int(i))!=parent)
            throw std::runtime_error("Binding probe original material differs");
    };
    check_original();
    bindings.bind(owner,{&binding,1});
    // Simulate native RepriseInit taking a material slot and overlay back.
    material(component,0,parent);
    astral_global_overlay(component,parent);
    if(bindings.intact()) throw std::runtime_error("Binding probe missed native reinitialization");
    if(!bindings.restore()) throw std::runtime_error("Binding probe native takeover cleanup failed");
    check_original();
    bindings.bind(owner,{&binding,1});
    auto overlays=overlay_slots(component);
    std::memcpy(overlays.GetRawPtr(0),&parent,sizeof(parent));
    if(bindings.intact()) throw std::runtime_error("Binding probe missed per-slot overlay replacement");
    if(!bindings.restore() || overlay_at(overlays,0)!=parent)
        throw std::runtime_error("Binding probe overwrote another overlay writer");
    bindings.bind(owner,{&binding,1});
    if(!temporary.destroy()) throw std::runtime_error("Binding probe component cleanup failed");
    if(!bindings.restore() || !bindings.restore())
        throw std::runtime_error("Binding probe destroyed-component restoration failed");
    roots.release();
    if(keep_alive::entries.size()!=retained_before)
        throw std::runtime_error("Binding probe retained objects did not return to baseline");
    return {{"passed",true},{"slots",count},{"duplicate_input_rejected",true},
        {"native_reinitialization_preserved",true},{"overlay_replacement_preserved",true},
        {"destroyed_component_restored",true},
        {"retained_objects_restored",true},{"existing_components_changed",false},
        {"scope","Binding and restoration on one unregistered diagnostic component. No summon rendering or ability validation."}};
}

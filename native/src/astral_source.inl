// Included after the reflected mesh, material and post-process setting readers.
namespace {
bool astral_source_flag(UObject* object,const wchar_t* name) {
    auto* property=optional_field(object,name);
    if(!property || !property->IsA<FBoolProperty>() || property->GetArrayDim()!=1 ||
       property->GetOffset_Internal()<0)
        throw std::runtime_error("Astral source flag layout mismatch");
    return static_cast<FBoolProperty*>(property)->GetPropertyValueInContainer(object);
}
UObject* astral_leader(UObject* component) {
    auto* property=field(component,L"LeaderPoseComponent",sizeof(FWeakObjectPtr));
    if(!property->IsA<FWeakObjectProperty>()) throw std::runtime_error("Astral leader property mismatch");
    return read<FWeakObjectPtr>(component,L"LeaderPoseComponent").Get();
}
std::vector<UObject*> astral_mesh_materials(UObject* mesh) {
    Call get(mesh,L"GetMaterials",1);get.run();
    auto* property=get.param(L"ReturnValue");
    if(!property->IsA<FArrayProperty>()) throw std::runtime_error("Astral source material array mismatch");
    auto* array=static_cast<FArrayProperty*>(property);
    auto* type=find(L"/Script/Engine.SkeletalMaterial");
    auto* inner=array->GetInner();
    if(!inner || !inner->IsA<FStructProperty>() ||
       static_cast<FStructProperty*>(inner)->GetStruct().Get()!=type)
        throw std::runtime_error("Astral source material element mismatch");
    auto* material=field(type,L"MaterialInterface",sizeof(UObject*));
    if(!material->IsA<FObjectProperty>() || material->GetOffset_Internal()<0 ||
       material->GetOffset_Internal()>inner->GetElementSize()-int(sizeof(UObject*)))
        throw std::runtime_error("Astral source material field mismatch");
    FScriptArrayHelper slots(array,get.data(property));
    if(slots.Num()<1 || slots.Num()>128) throw std::runtime_error("Astral source material count exceeds bound");
    std::vector<UObject*> result;
    result.reserve(size_t(slots.Num()));
    for(int i=0;i<slots.Num();++i)
        result.push_back(static_cast<FObjectProperty*>(material)->GetObjectPropertyValue(
            slots.GetRawPtr(i)+material->GetOffset_Internal()));
    return result;
}
std::map<std::string,float> astral_source_morphs(UObject* component,UObject* mesh) {
    auto* property=optional_field(mesh,L"MorphTargets");
    if(!property || !property->IsA<FArrayProperty>()) throw std::runtime_error("Astral source morph array mismatch");
    auto* array=static_cast<FArrayProperty*>(property);
    if(!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Astral source morph element mismatch");
    FScriptArrayHelper targets(array,reinterpret_cast<std::byte*>(mesh)+property->GetOffset_Internal());
    if(targets.Num()<0 || targets.Num()>4096) throw std::runtime_error("Astral source morph count exceeds bound");
    std::map<std::string,float> result;
    for(int i=0;i<targets.Num();++i) {
        auto* target=static_cast<FObjectProperty*>(array->GetInner())->GetObjectPropertyValue(targets.GetRawPtr(i));
        if(!target || WeakObject(target).Get()!=target) throw std::runtime_error("Astral source morph expired");
        const auto name=target->GetFName();
        Call get(component,L"GetMorphTarget",2);get.set(L"MorphTargetName",name);get.run();
        const float weight=get.get<float>();
        if(!std::isfinite(weight)) throw std::runtime_error("Astral source morph is not finite");
        if(weight!=0.f && !result.emplace(narrow(name.ToString()),weight).second)
            throw std::runtime_error("Astral source has duplicate morph names");
    }
    return result;
}
AstralPhysicsSource astral_source_physics(UObject* component) {
    AstralPhysicsSource result;
    Call disabled(component,L"GetDisablePostProcessBlueprint",1);disabled.run();
    result.post_process_disabled=disabled.get<bool>();
    result.cloth_disabled=astral_source_flag(component,L"bDisableClothSimulation");
    result.rigid_body_disabled=astral_source_flag(component,L"bDisableRigidBodyAnimNode");
    auto* instance=post_process_instance(component);
    if(!instance) return result;
    result.animation_class=instance->GetClassPrivate();
    for(const auto& [name,node]:spring_nodes(instance)) {
        const auto settings=capture_spring<SpringSettings>(node);
        for(const auto value:{settings.stiffness,settings.damping,settings.max_displacement,settings.error_reset})
            if(!std::isfinite(value)) throw std::runtime_error("Astral source spring is not finite");
        result.springs.emplace(name,settings);
    }
    for(const auto& [name,node]:dynamics_nodes(instance)) result.dynamics.emplace(name,node.capture());
    if(has_rig_inputs(instance)) result.rig=rig_inputs(instance).capture();
    if(has_body_rig_inputs(instance)) result.body_rig=body_rig_inputs(instance).capture();
    if(body_geometry_property(instance,L"CSSBodyGeometryJson")) result.geometry=body_geometry_inputs(instance).capture();
    return result;
}
}
std::unique_ptr<AstralAppearanceSource> Appearance::astral_source() const {
#ifdef CSS_INVENTORY_DEV
    AstralTiming timing(AstralPhase::source);
#endif
    auto* pawn=observed_pawn_.Get();
    auto* body=observed_component_.Get();
    if(!pawn || !body || read<UObject*>(pawn,L"Mesh")!=body ||
       !astral_shell(narrow(read<FName>(pawn,L"CharacterId").ToString()))) return {};
    const bool managed=active();
    // A transition has reset the body but CSS still intends to restore it.
    // Do not turn that transient vanilla mesh into the requested custom look.
    if(component_.Get()==body && applied_.Get() && !managed) return {};
    auto source=std::make_unique<AstralAppearanceSource>();
    source->pawn=pawn;source->player_revision=player_revision;source->appearance_revision=appearance_revision;
    const auto items=managed?items_.components():std::vector<std::pair<std::string,WeakObject>>{};
    if(items.size()>15) throw std::runtime_error("Astral source item count exceeds bound");
    auto capture=[&](UObject* component,const std::string& item,bool primary) {
        if(!component || WeakObject(component).Get()!=component)
            throw std::runtime_error("Astral source component expired");
        Call owner(component,L"GetOwner",1);owner.run();
        if(owner.get<UObject*>()!=pawn) throw std::runtime_error("Astral source component has another owner");
        auto* mesh=mesh_asset(component);
        if(!mesh || WeakObject(mesh).Get()!=mesh) throw std::runtime_error("Astral source mesh expired");
        astral_retain(source->retained,mesh);
        AstralComponentSource row;
        row.item=item;row.component=component;row.mesh=mesh;
        auto* leader=astral_leader(component);
        if(primary && leader) throw std::runtime_error("Astral source body uses an external pose leader");
        if(!primary && (leader!=body || read<UObject*>(component,L"AttachParent")!=body ||
           read<FName>(component,L"AttachSocketName")!=FName(L"None")))
            throw std::runtime_error("Astral item does not follow the captured body");
        row.leader_pose=!primary;
        row.visible=astral_source_flag(component,L"bVisible");
        row.hidden_in_game=astral_source_flag(component,L"bHiddenInGame");
        row.location=read<std::array<double,3>>(component,L"RelativeLocation");
        row.rotation=read<std::array<double,3>>(component,L"RelativeRotation");
        row.scale=read<std::array<double,3>>(component,L"RelativeScale3D");
        for(const auto& vector:{row.location,row.rotation,row.scale}) for(double value:vector)
            if(!std::isfinite(value)) throw std::runtime_error("Astral source transform is not finite");
        const auto defaults=astral_mesh_materials(mesh);
        const auto default_overlays=authored_overlays(mesh);
        auto live_overlays=overlay_slots(component);
        auto* global_overlay=read<UObject*>(component,L"OverlayMaterial");
        auto retain_material=[&](UObject* value) {
            if(value) {
                if(WeakObject(value).Get()!=value || !value->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"))))
                    throw std::runtime_error("Astral source material expired or invalid");
                astral_retain(source->retained,value);
            }
            return WeakObject(value);
        };
        for(size_t i=0;i<defaults.size();++i) {
            UObject* value=nullptr;
            if(primary && managed) {
                value=i<expected_materials_.size()?expected_materials_[i].Get():nullptr;
                if(!value && i<expected_materials_.size() && expected_materials_[i].ObjectIndex>=0)
                    throw std::runtime_error("Astral customized source material expired");
                if(!value) value=defaults[i];
            } else {
                Call get(component,L"GetMaterial",2);get.set(L"ElementIndex",int32_t(i));get.run();
                value=get.get<UObject*>();
            }
            row.materials.push_back(retain_material(value));
            auto* overlay=primary && managed?overlay_controls_.appearance_material(component,int(i)):nullptr;
            if(!overlay) {
                overlay=overlay_at(live_overlays,int(i));
                if(!overlay || overlay==global_overlay) overlay=i<default_overlays.size()?default_overlays[i]:nullptr;
            }
            row.overlays.push_back(retain_material(overlay));
        }
        Call lods(component,L"GetNumLODs",1);lods.run();
        const auto count=lods.get<int32_t>();
        if(count<1 || count>16) throw std::runtime_error("Astral source LOD count exceeds bound");
        row.hidden_by_lod.resize(size_t(count));
        for(int lod=0;lod<count;++lod) for(int slot=0;slot<int(defaults.size());++slot) {
            Call shown(component,L"IsMaterialSectionShown",3);
            shown.set(L"MaterialID",int32_t(slot));shown.set(L"LODIndex",int32_t(lod));shown.run();
            if(!shown.get<bool>()) row.hidden_by_lod[size_t(lod)].insert(slot);
        }
        row.morphs=astral_source_morphs(component,mesh);
        row.physics=astral_source_physics(component);
        if(auto* type=row.physics.animation_class.Get()) astral_retain(source->retained,type);
        if(row.component.Get()!=component || mesh_asset(component)!=mesh)
            throw std::runtime_error("Astral source changed while capturing");
        source->components.push_back(std::move(row));
    };
    capture(body,"",true);
    for(const auto& [item,component]:items) capture(component.Get(),item,false);
    if(source->pawn.Get()!=pawn || observed_pawn_.Get()!=pawn || observed_component_.Get()!=body ||
       read<UObject*>(pawn,L"Mesh")!=body || source->player_revision!=player_revision ||
       source->appearance_revision!=appearance_revision)
        throw std::runtime_error("Astral player changed while capturing");
    return source;
}

// Included after the source capture and post-process setting accessors.
namespace {
uint8_t astral_required_tick(UObject* component);
UObject* astral_pose_source(UObject* component) {
    std::array<UObject*,8> visited{};
    auto* type=static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"));
    for(size_t depth=0;component && depth<visited.size();++depth) {
        if(WeakObject(component).Get()!=component || !component->IsA(type) ||
           std::find(visited.begin(),visited.begin()+depth,component)!=visited.begin()+depth)
            throw std::runtime_error("Invalid Astral pose leader chain");
        visited[depth]=component;
        auto* leader=astral_leader(component);
        if(!leader) return component;
        component=leader;
    }
    throw std::runtime_error("Astral pose leader chain exceeds bound");
}
void astral_set_initial_pose_source(UObject* instance,UObject* source) {
    // Called immediately after registration, before this new component's first
    // tick. InitAnim's registration refresh is synchronous (no tick function).
    // Never rewrite the node on a visual that has already ticked.
    auto view=AstralStructView::object(instance).child(L"AnimGraphNode_CopyPoseFromMesh");
    if(view.type->GetPathName()!=L"/Script/AnimGraphRuntime.AnimNode_CopyPoseFromMesh")
        throw std::runtime_error("Astral copy node type differs from shared template");
    auto* field=view.property(L"SourceMeshComponent");
    if(!field->IsA<FWeakObjectProperty>() || field->GetElementSize()!=sizeof(FWeakObjectPtr))
        throw std::runtime_error("Astral copy source property differs from shared template");
    const WeakObject weak(source);
    auto* address=const_cast<std::byte*>(view.data)+field->GetOffset_Internal();
    std::memcpy(address,static_cast<const FWeakObjectPtr*>(&weak),sizeof(FWeakObjectPtr));
    FWeakObjectPtr readback;std::memcpy(&readback,address,sizeof(readback));
    if(readback.Get()!=source) throw std::runtime_error("Astral copy source read-back failed");
}
void astral_visual_flag(UObject* object,const wchar_t* name,bool value) {
    auto* property=optional_field(object,name);
    if(!property || !property->IsA<FBoolProperty>() || property->GetArrayDim()!=1 ||
       property->GetOffset_Internal()<0)
        throw std::runtime_error("Astral visual flag layout mismatch");
    static_cast<FBoolProperty*>(property)->SetPropertyValueInContainer(object,value);
    if(astral_source_flag(object,name)!=value)
        throw std::runtime_error("Astral visual flag read-back failed");
}
void astral_visual_visibility(UObject* component,bool visible) {
    Call set(component,L"SetVisibility",2);
    set.set(L"bNewVisibility",visible);set.set(L"bPropagateToChildren",false);set.run();
    if(astral_source_flag(component,L"bVisible")!=visible)
        throw std::runtime_error("Astral visual visibility read-back failed");
}
UObject* astral_anim_instance(UObject* component) {
    Call get(component,L"GetAnimInstance",1);get.run();return get.get<UObject*>();
}
void astral_visual_physics(UObject* component,const AstralPhysicsSource& source) {
    auto* instance=post_process_instance(component);
    auto* expected=source.animation_class.Get();
    if(expected && (!instance || instance->GetClassPrivate()!=expected))
        throw std::runtime_error("Astral visual post-process class differs from source");
    const bool has_settings=!source.springs.empty() || !source.dynamics.empty() ||
        source.rig || source.body_rig || source.geometry;
    if(!instance) {
        if(has_settings) throw std::runtime_error("Astral visual post-process settings lack an instance");
        return;
    }
    const auto springs=spring_nodes(instance);
    const auto dynamics=dynamics_nodes(instance);
    for(const auto& [name,value]:source.springs) {
        const auto node=springs.find(name);
        if(node==springs.end()) throw std::runtime_error("Astral visual spring is missing: "+name);
        apply_spring(node->second,value);
    }
    for(const auto& [name,value]:source.dynamics) {
        const auto node=dynamics.find(name);
        if(node==dynamics.end()) throw std::runtime_error("Astral visual dynamics chain is missing: "+name);
        node->second.apply(value);
        if(node->second.capture()!=value) throw std::runtime_error("Astral visual dynamics read-back failed");
    }
    if(source.rig) {
        const auto inputs=rig_inputs(instance);inputs.apply(*source.rig);
        if(inputs.capture()!=*source.rig) throw std::runtime_error("Astral visual rig read-back failed");
    }
    if(source.body_rig) {
        const auto inputs=body_rig_inputs(instance);inputs.apply(*source.body_rig);
        if(inputs.capture()!=*source.body_rig) throw std::runtime_error("Astral visual body rig read-back failed");
    }
    if(source.geometry) {
        const auto inputs=body_geometry_inputs(instance);inputs.apply(*source.geometry);
        if(inputs.capture()!=*source.geometry) throw std::runtime_error("Astral visual geometry read-back failed");
    }
    reset_dynamics(instance);
}
}

void AstralVisualMesh::prepare(UObject* parent,const AstralComponentSource& source,
                               UObject* pose_class,const AstralVisualTransform& relative) {
    if(component_.ObjectIndex>=0) throw std::runtime_error("Release the previous Astral visual first");
    auto* mesh=source.mesh.Get();
    auto* component_type=static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"));
    if(!parent || WeakObject(parent).Get()!=parent || !parent->IsA(component_type) || !mesh ||
       !mesh->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMesh"))) ||
       !pose_class || WeakObject(pose_class).Get()!=pose_class ||
       pose_class->GetPathName()!=L"/Game/CSS/SharedAssets/Astral/ABP_CopyPose.ABP_CopyPose_C" ||
       !pose_class->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimBlueprintGeneratedClass"))))
        throw std::runtime_error("Invalid Astral visual source or shared pose template");
    if(!is_compatible_skeleton(mesh_asset(parent),mesh))
        throw std::runtime_error("Astral visual skeleton pair is not audited");
    for(const auto& vector:{relative.location,relative.rotation,relative.scale})
        for(const auto value:vector) if(!std::isfinite(value))
            throw std::runtime_error("Astral visual transform is not finite");
    for(const auto scale:relative.scale) if(scale<=0. || scale>100.)
        throw std::runtime_error("Astral visual scale is outside supported bounds");
    if(source.hidden_by_lod.empty() || source.hidden_by_lod.size()>16 ||
       source.materials.empty() || source.materials.size()>128 || source.morphs.size()>4096)
        throw std::runtime_error("Astral visual source exceeds bounds");
    for(const auto& lod:source.hidden_by_lod) for(int slot:lod)
        if(slot<0 || size_t(slot)>=source.materials.size())
            throw std::runtime_error("Astral visual hidden slot is invalid");
    for(const auto& [name,weight]:source.morphs)
        if(name.empty() || !std::isfinite(weight) || !mesh_has_morph(mesh,name))
            throw std::runtime_error("Astral visual morph is invalid");
    auto* owner=astral_binding_owner(parent);
    auto* parent_instance=astral_anim_instance(parent);
    if(!owner || !parent_instance) throw std::runtime_error("Astral visual source has no owner or animation");
    if(source.physics.animation_class.ObjectIndex>=0 && !source.physics.animation_class.Get())
        throw std::runtime_error("Astral visual source post-process class expired");
    AssetLoadRoots roots;
    astral_retain(roots,mesh);astral_retain(roots,pose_class);
    if(auto* type=source.physics.animation_class.Get()) astral_retain(roots,type);
    owner_=owner;parent_=parent;parent_mesh_=mesh_asset(parent);parent_instance_=parent_instance;
    mesh_=mesh;pose_class_=pose_class;
    retained_.take(roots);
    source_visible_=source.visible && !source.hidden_in_game;
    leader_pose_=source.leader_pose;
    auto* pose_source=leader_pose_?parent:astral_pose_source(parent);
    if(!is_compatible_skeleton(mesh_asset(pose_source),mesh))
        throw std::runtime_error("Astral effective pose skeleton pair is not audited");
    pose_source_=pose_source;pose_source_mesh_=mesh_asset(pose_source);
    try {
        Call transform(find(L"/Script/Engine.Default__KismetMathLibrary"),L"MakeTransform",4);
        transform.set(L"Location",relative.location);transform.set(L"Rotation",relative.rotation);
        transform.set(L"Scale",relative.scale);transform.run();
        auto copy_transform=[&](Call& to) {
            auto* result=transform.param(L"ReturnValue");auto* input=to.param(L"RelativeTransform");
            if(!input->SameType(result) || input->GetElementSize()!=result->GetElementSize())
                throw std::runtime_error("Astral visual transform layout mismatch");
            input->CopyCompleteValue(to.data(input),transform.data(result));
        };
        Call add(owner,L"AddComponentByClass",5);
        add.set(L"Class",component_type);add.set(L"bManualAttachment",true);add.set(L"bDeferredFinish",true);
        copy_transform(add);add.run();
        auto* visual=add.get<UObject*>();
        if(!visual) throw std::runtime_error("Astral visual creation failed");
        component_=visual;
        astral_visual_visibility(visual,false);
        write_field(visual,L"VisibilityBasedAnimTickOption",astral_required_tick(visual));
        astral_visual_flag(visual,L"bEnableUpdateRateOptimizations",false);
        Call collision(visual,L"SetCollisionEnabled",1);collision.set(L"NewType",uint8_t{0});collision.run();
        Call asset(visual,L"SetSkeletalMeshAsset",1);asset.set(L"NewMesh",mesh);asset.run();
        Call post(visual,L"SetOverridePostProcessAnimBP",2);
        post.set(L"InPostProcessAnimBlueprint",source.physics.animation_class.Get());
        post.set(L"ReinitAnimInstances",false);post.run();
        Call disable(visual,L"SetDisablePostProcessBlueprint",1);
        disable.set(L"bInDisablePostProcess",source.physics.post_process_disabled);disable.run();
        astral_visual_flag(visual,L"bDisableClothSimulation",source.physics.cloth_disabled);
        Call rigid(visual,L"SetAllowRigidBodyAnimNode",2);
        rigid.set(L"bInAllow",!source.physics.rigid_body_disabled);rigid.set(L"bReinitAnim",false);rigid.run();
        // Copy Pose resolves its source during initialization. Establish the
        // parent before registration, and never set a leader on this component.
        Call attach(visual,L"K2_AttachToComponent",7);
        attach.set(L"Parent",parent);attach.set(L"SocketName",FName(L"None"));
        attach.set(L"LocationRule",uint8_t{0});attach.set(L"RotationRule",uint8_t{0});attach.set(L"ScaleRule",uint8_t{0});
        attach.set(L"bWeldSimulatedBodies",false);attach.run();
        if(!attach.get<bool>()) throw std::runtime_error("Astral visual attachment failed");
        if(leader_pose_) {
            Call leader(visual,L"SetLeaderPoseComponent",3);
            leader.set(L"NewLeaderBoneComponent",parent);leader.set(L"bForceUpdate",true);
            leader.set(L"bInFollowerShouldTickPose",false);leader.run();
        } else {
            Call animation(visual,L"SetAnimInstanceClass",1);animation.set(L"NewClass",pose_class);animation.run();
        }
        Call prerequisite(visual,L"AddTickPrerequisiteComponent",1);
        prerequisite.set(L"PrerequisiteComponent",parent);prerequisite.run();
        if(pose_source!=parent) {
            Call source_tick(visual,L"AddTickPrerequisiteComponent",1);
            source_tick.set(L"PrerequisiteComponent",pose_source);source_tick.run();
        }
        Call finish(owner,L"FinishAddComponent",3);
        finish.set(L"Component",visual);finish.set(L"bManualAttachment",true);copy_transform(finish);finish.run();
        if(!leader_pose_) {
            auto* instance=astral_anim_instance(visual);
            if(!instance) throw std::runtime_error("Astral visual has no copy-pose instance");
            astral_set_initial_pose_source(instance,pose_source);
        }
        if(leader_pose_) {
            Call tick(visual,L"SetComponentTickEnabled",1);tick.set(L"bEnabled",false);tick.run();
        }
        if(!intact()) throw std::runtime_error("Astral visual registration or source read-back failed");
        Call lods(visual,L"GetNumLODs",1);lods.run();
        if(lods.get<int32_t>()!=int32_t(source.hidden_by_lod.size()))
            throw std::runtime_error("Astral visual LOD count differs from source");
        for(size_t lod=0;lod<source.hidden_by_lod.size();++lod) for(int slot:source.hidden_by_lod[lod]) {
            Call hide(visual,L"ShowMaterialSection",4);
            hide.set(L"MaterialID",int32_t(slot));hide.set(L"SectionIndex",int32_t{-1});
            hide.set(L"bShow",false);hide.set(L"LODIndex",int32_t(lod));hide.run();
            Call shown(visual,L"IsMaterialSectionShown",3);
            shown.set(L"MaterialID",int32_t(slot));shown.set(L"LODIndex",int32_t(lod));shown.run();
            if(shown.get<bool>()) throw std::runtime_error("Astral visual hidden section read-back failed");
        }
        for(const auto& [name,weight]:source.morphs) {
            Call morph(visual,L"SetMorphTarget",3);
            morph.set(L"MorphTargetName",FName(wide(name).c_str(),FNAME_Add));morph.set(L"Value",weight);
            morph.set(L"bRemoveZeroWeight",false);morph.run();
            Call get(visual,L"GetMorphTarget",2);
            get.set(L"MorphTargetName",FName(wide(name).c_str(),FNAME_Add));get.run();
            if(get.get<float>()!=weight) throw std::runtime_error("Astral visual morph read-back failed");
        }
        if(!leader_pose_) astral_visual_physics(visual,source.physics);
        Call reset_cloth(visual,L"ForceClothNextUpdateTeleportAndReset",0);reset_cloth.run();
        Call collision_state(visual,L"GetCollisionEnabled",1);collision_state.run();
        if(collision_state.get<uint8_t>()!=0 || astral_source_flag(visual,L"bVisible") ||
           astral_source_flag(visual,L"bDisableClothSimulation")!=source.physics.cloth_disabled ||
           astral_source_flag(visual,L"bDisableRigidBodyAnimNode")!=source.physics.rigid_body_disabled || !intact())
            throw std::runtime_error("Astral visual final state differs from requested state");
    } catch(...) { release();throw; }
}

bool AstralVisualMesh::intact() const {
    auto* parent=parent_.Get();auto* visual=component_.Get();auto* owner=owner_.Get();
    if(!parent || !visual || !owner || !mesh_.Get() || !pose_class_.Get() ||
       !parent_mesh_.Get() || !parent_instance_.Get()) return false;
    auto* visual_instance=leader_pose_?nullptr:astral_anim_instance(visual);
    return astral_binding_owner(parent)==owner && astral_binding_owner(visual)==owner &&
        mesh_asset(parent)==parent_mesh_.Get() && astral_anim_instance(parent)==parent_instance_.Get() &&
        mesh_asset(visual)==mesh_.Get() && read<UObject*>(visual,L"AttachParent")==parent &&
        (leader_pose_?astral_leader(visual)==parent:
            visual_instance && visual_instance->GetClassPrivate()==pose_class_.Get());
}
bool AstralVisualMesh::pose_source_intact() const {
    auto* parent=parent_.Get();auto* source=pose_source_.Get();
    if(!parent || !source || !pose_source_mesh_.Get()) return false;
    return (leader_pose_?parent:astral_pose_source(parent))==source &&
        mesh_asset(source)==pose_source_mesh_.Get();
}
void AstralVisualMesh::show(bool enabled) {
    if(!intact()) throw std::runtime_error("Astral visual source changed before visibility update");
    astral_visual_visibility(component_.Get(),enabled && source_visible_);
}
bool AstralVisualMesh::release() noexcept {
    try {
        if(auto* visual=component_.Get();astral_object_valid(visual)) {
            if(astral_binding_owner(visual)!=owner_.Get()) return false;
            astral_visual_visibility(visual,false);
            Call destroy(visual,L"K2_DestroyComponent",1);destroy.set(L"Object",owner_.Get());destroy.run();
            // The shipped weak-pointer bridge can still resolve a component
            // that Unreal has marked as garbage, until GC removes its entry.
            if(astral_object_valid(component_.Get())) return false;
        }
    } catch(...) { return false; }
    component_=WeakObject{};owner_=WeakObject{};parent_=WeakObject{};
    parent_mesh_=WeakObject{};parent_instance_=WeakObject{};mesh_=WeakObject{};pose_class_=WeakObject{};
    pose_source_=WeakObject{};pose_source_mesh_=WeakObject{};
    source_visible_=leader_pose_=false;retained_.release();return true;
}

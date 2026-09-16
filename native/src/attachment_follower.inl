// Included inside css after reflected calls, asset loads and material helpers.
// Missing cosmetic bones use the original mesh's follower-pose fallback. The
// component is hidden, non-colliding and does not run its own animation graph.
static std::vector<WeakObject> attached_children(UObject* component) {
    Call call(component,L"GetChildrenComponents",2);call.set(L"bIncludeAllDescendants",false);call.run();
    auto* property=call.param(L"Children");
    if(!property->IsA<FArrayProperty>()) throw std::runtime_error("Attachment children layout mismatch");
    auto* array=static_cast<FArrayProperty*>(property);
    if(!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Attachment child is not an object");
    FScriptArrayHelper values(array,call.data(property));
    if(values.Num()<0 || values.Num()>128) throw std::runtime_error("Too many character attachments");
    std::vector<WeakObject> result;
    for(int i=0;i<values.Num();++i) {UObject* child{};std::memcpy(&child,values.GetRawPtr(i),sizeof(child));result.emplace_back(child);}
    return result;
}
static UObject* attach_parent(UObject* component) {
    Call call(component,L"GetAttachParent",1);call.run();return call.get<UObject*>();
}
static FName attach_socket(UObject* component) {
    Call call(component,L"GetAttachSocketName",1);call.run();return call.get<FName>();
}
static bool socket_bone_present(UObject* component,FName socket) {
    Call bone(component,L"GetSocketBoneName",2);bone.set(L"InSocketName",socket);bone.run();
    Call index(component,L"GetBoneIndex",2);index.set(L"BoneName",bone.get<FName>());index.run();
    return index.get<int32_t>()>=0;
}
static void attach_relative(UObject* child,UObject* parent,FName socket) {
    Call attach(child,L"K2_AttachToComponent",7);
    attach.set(L"Parent",parent);attach.set(L"SocketName",socket);
    attach.set(L"LocationRule",uint8_t{0});attach.set(L"RotationRule",uint8_t{0});attach.set(L"ScaleRule",uint8_t{0});
    attach.set(L"bWeldSimulatedBodies",false);attach.run();
    if(!attach.get<bool>() || attach_parent(child)!=parent || attach_socket(child)!=socket)
        throw std::runtime_error("Attachment transfer did not complete");
}
void AttachmentFollower::release() {
    auto* proxy=proxy_.Get();auto* component=component_.Get();
    if(proxy) {
        // Restore only children still on our proxy. Drawing or switching an item
        // can already have returned it to a game-owned hand socket.
        for(const auto& entry:owned_) if(auto* child=entry.child.Get();child && attach_parent(child)==proxy) {
            if(component) attach_relative(child,component,attach_socket(child));
            else {Call detach(child,L"K2_DetachFromComponent",4);detach.set(L"LocationRule",uint8_t{1});detach.set(L"RotationRule",uint8_t{1});detach.set(L"ScaleRule",uint8_t{1});detach.set(L"bCallModify",false);detach.run();}
        }
        // An unrelated mod may have attached something to our helper. Preserve
        // it instead of destroying its parent during unload.
        if(!attached_children(proxy).empty()) throw std::runtime_error("An unowned attachment still uses the CSS helper");
        Call owner(proxy,L"GetOwner",1);owner.run();
        Call destroy(proxy,L"K2_DestroyComponent",1);destroy.set(L"Object",owner.get<UObject*>());destroy.run();
    }
    owned_.clear();unsupported_.clear();proxy_.Reset();component_.Reset();mesh_.Reset();original_.clear();ready_=false;
}
void AttachmentFollower::update(UObject* component,const std::string& original) {
    if(!component || original.empty()) {release();return;}
    auto* visible=mesh_asset(component);
    if(component_.Get()!=component || mesh_.Get()!=visible || original_!=original || (proxy_.Get() && !ready_)) {
        release();component_=component;mesh_=visible;original_=original;
    }
    // Drop records the game has taken back. Future stow events are discovered
    // from the visible component's direct children, without an actor scan.
    std::erase_if(owned_,[&](const auto& entry){auto* child=entry.child.Get();return !child || attach_parent(child)!=proxy_.Get();});
    for(const auto& weak:attached_children(component)) {
        auto* child=weak.Get();if(!child || child==proxy_.Get()) continue;
        // Keep audio, particles, cameras and other gameplay components on their
        // original parent. Only rendered mesh accessories use this fallback.
        if(!child->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"))) &&
           !child->IsA(static_cast<UClass*>(find(L"/Script/Engine.StaticMeshComponent")))) continue;
        const auto socket=attach_socket(child);const auto name=socket.ToString();
        if(name==L"None" || unsupported_.contains(name) || socket_bone_present(component,socket)) continue;
        if(!proxy_.Get()) {
            AssetLoadRoots roots;auto* source=load(original);roots.keep(source);
            component=component_.Get();child=weak.Get();
            if(!component || !child || mesh_asset(component)!=mesh_.Get() || attach_parent(child)!=component || attach_socket(child)!=socket) return;
            Call owner(component,L"GetOwner",1);owner.run();auto* actor=owner.get<UObject*>();
            Call transform(find(L"/Script/Engine.Default__KismetMathLibrary"),L"MakeTransform",4);
            transform.set(L"Location",std::array<double,3>{});transform.set(L"Rotation",std::array<double,3>{});
            transform.set(L"Scale",std::array<double,3>{1,1,1});transform.run();
            auto* result=transform.param(L"ReturnValue");
            auto copy_transform=[&](Call& target) {
                auto* input=target.param(L"RelativeTransform");
                if(!input->SameType(result) || input->GetElementSize()!=result->GetElementSize())
                    throw std::runtime_error("Attachment transform layout mismatch");
                input->CopyCompleteValue(target.data(input),transform.data(result));
            };
            Call add(actor,L"AddComponentByClass",5);
            add.set(L"Class",find(L"/Script/Engine.SkeletalMeshComponent"));add.set(L"bManualAttachment",true);add.set(L"bDeferredFinish",true);
            copy_transform(add);add.run();
            auto* proxy=add.get<UObject*>();if(!proxy) throw std::runtime_error("Attachment helper could not be created");proxy_=proxy;
            // Ownership is recorded before any remaining calls, so an exception
            // leaves the component available for normal cleanup/retry.
            Call hidden(proxy,L"SetHiddenInGame",2);hidden.set(L"NewHidden",true);hidden.set(L"bPropagateToChildren",false);hidden.run();
            Call shadow(proxy,L"SetCastShadow",1);shadow.set(L"NewCastShadow",false);shadow.run();
            Call collision(proxy,L"SetCollisionEnabled",1);collision.set(L"NewType",uint8_t{0});collision.run();
            Call tick(proxy,L"SetComponentTickEnabled",1);tick.set(L"bEnabled",false);tick.run();
            Call mesh(proxy,L"SetSkeletalMeshAsset",1);mesh.set(L"NewMesh",source);mesh.run();
            Call leader(proxy,L"SetLeaderPoseComponent",3);leader.set(L"NewLeaderBoneComponent",component);leader.set(L"bForceUpdate",true);leader.set(L"bInFollowerShouldTickPose",false);leader.run();
            Call finish(actor,L"FinishAddComponent",3);finish.set(L"Component",proxy);finish.set(L"bManualAttachment",true);
            copy_transform(finish);finish.run();
            Call stop_tick(proxy,L"SetComponentTickEnabled",1);stop_tick.set(L"bEnabled",false);stop_tick.run();
            attach_relative(proxy,component,FName(L"None"));
            ready_=true;
        }
        auto* proxy=proxy_.Get();
        if(!socket_bone_present(proxy,socket)) {unsupported_.insert(name);continue;}
        if(owned_.size()>=128) throw std::runtime_error("Too many repaired attachments");
        // Record before mutation so cleanup can recover a failed read-back.
        owned_.push_back({weak});attach_relative(child,proxy,socket);
    }
}
void Appearance::sync_attachments() {
    attachments_.update(active()?component_.Get():nullptr,original_);
    auto* menu=menu_component_.Get();
    menu_attachments_.update(menu && mesh_asset(menu)==menu_applied_.Get()?menu:nullptr,menu_original_);
}

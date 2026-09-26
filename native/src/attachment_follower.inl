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
// USceneComponent's AttachParent and AttachSocketName are reflected properties, and the
// Get* functions only return them, so they are read in place: the MISC and seal passes ask
// this for every tracked item every frame.
static UObject* attach_parent(UObject* component) { return read<UObject*>(component,L"AttachParent"); }
static FName attach_socket(UObject* component) { return read<FName>(component,L"AttachSocketName"); }
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
// 0.4: per-outfit socket offsets. The game keeps re-attaching stowed items with its
// own relative transform (draw/stow), so the correction is re-applied whenever the
// child is back at a known base transform, and removed again on restore.
static std::array<double,3> relative_location(UObject* child) { return read<std::array<double,3>>(child,L"RelativeLocation"); }
static std::array<double,3> relative_rotation(UObject* child) { return read<std::array<double,3>>(child,L"RelativeRotation"); }
static bool close_to(const std::array<double,3>& a,const std::array<double,3>& b,double tolerance) {
    for(int i=0;i<3;++i) if(std::abs(a[i]-b[i])>tolerance) return false;
    return true;
}
static void set_relative(UObject* child,const std::array<double,3>& location,const std::array<double,3>& rotation) {
    Call call(child,L"K2_SetRelativeLocationAndRotation",5);
    call.set(L"NewLocation",location); call.set(L"NewRotation",rotation);
    call.set(L"bSweep",false); call.set(L"bTeleport",true); call.run();
}
// A stowed prop is welded to its socket, so nothing stops the body walking through it.
// These two helpers expand an FRotator the way Unreal's own FRotationMatrix does; the
// obvious Rz*Ry*Rx expansion mirrors pitch and roll and puts the prop at the wrong angle.
static void unreal_basis(const std::array<double,3>& rotator,double rows[3][3]) {
    constexpr double radians=3.14159265358979323846/180.;
    const double p=rotator[0]*radians,y=rotator[1]*radians,r=rotator[2]*radians;
    const double sp=std::sin(p),sy=std::sin(y),sr=std::sin(r);
    const double cp=std::cos(p),cy=std::cos(y),cr=std::cos(r);
    rows[0][0]=cp*cy;             rows[0][1]=cp*sy;             rows[0][2]=sp;
    rows[1][0]=sr*sp*cy-cr*sy;    rows[1][1]=sr*sp*sy+cr*cy;    rows[1][2]=-sr*cp;
    rows[2][0]=-(cr*sp*cy+sr*sy); rows[2][1]=cy*sr-cr*sp*sy;    rows[2][2]=cr*cp;
}
static std::array<double,3> to_world(const double rows[3][3],const std::array<double,3>& v) {
    return {v[0]*rows[0][0]+v[1]*rows[1][0]+v[2]*rows[2][0],
            v[0]*rows[0][1]+v[1]*rows[1][1]+v[2]*rows[2][1],
            v[0]*rows[0][2]+v[1]*rows[1][2]+v[2]*rows[2][2]};
}
static std::array<double,3> to_local(const double rows[3][3],const std::array<double,3>& v) {
    return {v[0]*rows[0][0]+v[1]*rows[0][1]+v[2]*rows[0][2],
            v[0]*rows[1][0]+v[1]*rows[1][1]+v[2]*rows[1][2],
            v[0]*rows[2][0]+v[1]*rows[2][1]+v[2]*rows[2][2]};
}
// Bone names as FNames, made once: the seal pass asks for the same few every frame.
static FName bone_name(const std::string& bone) {
    static std::unordered_map<std::string,FName> names;
    if(auto it=names.find(bone);it!=names.end()) return it->second;
    return names.emplace(bone,FName(wide(bone).c_str())).first->second;
}
bool AttachmentOffsets::pose(UObject* component,const std::string& bone,BonePose& out) {
    if(auto cached=poses_.find(bone);cached!=poses_.end()) { out=cached->second; return true; }
    const FName name=bone_name(bone);
    BonePose result;
    Call location(component,L"GetSocketLocation",2); location.set(L"InSocketName",name); location.run();
    result.location=location.get<std::array<double,3>>();
    Call rotation(component,L"GetSocketRotation",2); rotation.set(L"InSocketName",name); rotation.run();
    unreal_basis(rotation.get<std::array<double,3>>(),result.basis);
    poses_.emplace(bone,result); out=result; return true;
}
// Hold the prop a set distance off the body, measured live.
//
// The body's own physics asset is the only description of its shape that follows the
// pose, and the engine will measure against it: GetClosestPointOnCollision returns the
// distance from a point to the nearest body, and the point it found. Nothing here is
// measured offline, which matters: the skeleton's bind pose is nothing like any pose the
// game actually plays, so a distance taken from it means nothing at runtime.
//
// The measurement is taken at the prop's own position, not at the socket, and the
// correction is allowed to pull in as well as push out. That makes it a servo: whatever
// transform the game stows the prop with, and whatever the fixed offset is, it settles at
// `clearance` off the body and stays there through the whole stride.
//
// A zero distance means the prop is inside the body, where the engine has no direction to
// offer. That is what the recorded direction is for; it rides a bone, so it still turns
// with the hips.
bool AttachmentOffsets::push_for(UObject* component,UObject* child,const AttachmentOffset& offset,
                                 const double socket_basis[3][3],std::array<double,3>& out,Tracked* item) {
    const auto& collision=offset.collision;
    if(!collision.active()) return false;
    Call where(child,L"K2_GetComponentLocation",1); where.run();
    const auto point=where.get<std::array<double,3>>();
    // GetClosestPointOnCollision with no bone name measures against the mesh's ROOT body only
    // (UE 5.6.1 USkeletalMeshComponent::GetBodyInstance: NAME_None is the root body), the
    // pelvis on these rigs. A prop stowed on the back was therefore measured against the
    // pelvis, read as far away, and never pushed off a bigger body. Measure against the body
    // of the bone it rides (its anchor) and the root, and keep the nearer.
    double distance=-1; std::array<double,3> body_point{}; std::string body_used;
    auto measure=[&](const FName& bone,const char* label) {
        Call closest(component,L"GetClosestPointOnCollision",4);
        closest.set(L"Point",point); closest.set(L"BoneName",bone); closest.run();
        const double found=closest.get<float>();
        if(item && label==std::string_view("root")) item->distance_root=found;
        if(found<0 || (distance>=0 && found>=distance)) return;
        distance=found; body_point=closest.get<std::array<double,3>>(L"OutPointOnBody"); body_used=label;
    };
    if(!collision.anchor.empty()) measure(bone_name(collision.anchor),collision.anchor.c_str());
    measure(bone_name("None"),"root");
#ifdef CSS_INVENTORY_DEV
    last_distance_=distance;
#endif
    if(item) { item->distance=distance; item->body=body_used; }
    if(distance<0) return false;             // the mesh has no collision to measure against
    std::array<double,3> direction{};
    if(distance>1e-3) {
        for(int i=0;i<3;++i) direction[i]=(point[i]-body_point[i])/distance;
    } else {
        if(collision.anchor.empty()) return false;
        BonePose anchor; if(!pose(component,collision.anchor,anchor)) return false;
        direction=to_world(anchor.basis,collision.direction);
    }
    // 1.0.x: one-sided servo. The correction may only hold a prop AWAY from the body, never pull it
    // inward: a stowed weapon the game already hangs at or beyond `clearance` keeps its native
    // position, and only one sitting closer (about to clip a larger custom body) is pushed out. The
    // old two-sided form settled every prop to exactly `clearance`, which moved the sidearm off its
    // default spot even on a stock-sized body. Floor at zero to drop the pull-in.
    const double push=std::clamp(collision.clearance-distance,0.0,collision.max_push);
    if(item) item->push=push;
#ifdef CSS_INVENTORY_DEV
    last_push_=push;
#endif
    out=to_local(socket_basis,{direction[0]*push,direction[1]*push,direction[2]*push});
    return true;
}
void AttachmentOffsets::apply(UObject* component,Tracked& item,const AttachmentOffset& offset,bool live) {
    auto* child=item.child.Get(); if(!child) return;
    auto current_location=relative_location(child),current_rotation=relative_rotation(child);
    // Tell a re-stow apart from CSS's own correction: anything other than what CSS last
    // wrote is a new base the game chose, and the correction restarts from there.
    if(!item.owned || !close_to(current_location,item.applied,.01)) {
        item.location=current_location; item.rotation=current_rotation;
    }
    std::array<double,3> extra{};
    if(live && offset.collision.active()) {
        Call socket_rotation(component,L"GetSocketRotation",2); socket_rotation.set(L"InSocketName",item.socket); socket_rotation.run();
        double socket_basis[3][3];
        unreal_basis(socket_rotation.get<std::array<double,3>>(),socket_basis);
        // The prop is already sitting at last frame's correction, so the measurement
        // includes it; carrying it forward is what makes this settle instead of oscillate.
        if(push_for(component,child,offset,socket_basis,extra,&item)) {
            for(int i=0;i<3;++i) extra[i]+=current_location[i]-item.location[i]-offset.location[i];
            double extra_len = std::sqrt(extra[0]*extra[0] + extra[1]*extra[1] + extra[2]*extra[2]);
            if(extra_len > offset.collision.max_push && extra_len > 1e-4) {
                double scale = offset.collision.max_push / extra_len;
                for(int i=0;i<3;++i) extra[i] *= scale;
            }
        } else extra={};
    }
    std::array<double,3> target_location{},target_rotation{};
    for(int i=0;i<3;++i) {
        target_location[i]=item.location[i]+offset.location[i]+extra[i];
        target_rotation[i]=item.rotation[i]+offset.rotation[i];
    }
    if(item.owned && close_to(current_location,target_location,.05) && close_to(current_rotation,target_rotation,.01)) return;
    set_relative(child,target_location,target_rotation);
    item.applied=target_location; item.owned=true;
}
static std::map<std::string,AttachmentOffset> default_attachment_offsets() {
    std::map<std::string,AttachmentOffset> d;
    auto add_back = [&](const char* socket, double clearance, double max_push, const char* anchor = "spine_03") {
        AttachmentOffset off;
        off.collision.anchor = anchor;
        off.collision.clearance = clearance;
        off.collision.max_push = max_push;
        off.collision.direction = {0.0, -1.0, 0.0};
        d[socket] = off;
    };
    auto add_hip_r = [&](const char* socket, double clearance, double max_push) {
        AttachmentOffset off;
        off.location = {0.31, 0.2644, 2.9722};
        off.collision.anchor = "pelvis";
        off.collision.clearance = clearance;
        off.collision.max_push = max_push;
        off.collision.direction = {0.32561, -0.81726, 0.47546};
        d[socket] = off;
    };
    auto add_hip_l = [&](const char* socket, double clearance, double max_push) {
        AttachmentOffset off;
        off.location = {-0.31, 0.2644, 2.9722};
        off.collision.anchor = "pelvis";
        off.collision.clearance = clearance;
        off.collision.max_push = max_push;
        off.collision.direction = {-0.32561, -0.81726, 0.47546};
        d[socket] = off;
    };

    // Stowed back weapons & heavy firearms / bows / melee
    add_back("Socket_NailShotgun_Stowed", 2.5, 35.0);
    add_back("Socket_Ballistazooka_Stowed", 3.0, 35.0);
    add_back("Socket_CrossBow_Stowed", 2.5, 35.0);
    add_back("Socket_CrossBow_Shoot_Weap_Stowed", 2.5, 35.0);
    add_back("Socket_Trebuchaxe_Stowed", 2.5, 35.0);
    add_back("Socket_ParasiteGun_Stowed", 2.5, 35.0);
    add_back("Socket_CursedChild_Stowed", 2.5, 35.0);
    add_back("Socket_MachineGun_Stowed", 2.5, 35.0);
    add_back("Socket_Lute_Simple_Stowed", 3.0, 35.0);
    add_back("Socket_MartyrBlade_Stowed", 3.0, 35.0);
    add_back("Socket_AxatanaAxe_Stowed_01", 3.0, 35.0);
    add_back("Socket_Sarcophagus_Sword_Stowed", 3.0, 35.0);
    add_back("Socket_Prop_Stowed_01", 3.0, 35.0);
    add_back("Socket_Prop_Stowed_02", 3.0, 35.0);
    add_back("Socket_Prop_Stowed_03", 3.0, 35.0, "spine_02");

    // Tarnished Seal / Slayer Seal & Stowed waist props
    add_hip_r("Socket_Prop_Stowed_InfiniteSeal_Right", 3.0, 15.0);
    add_hip_r("Socket_Prop_Stowed_InfiniteSeal", 3.0, 15.0);
    add_hip_r("Socket_Prop_Stowed_SlayerSeal_Right", 3.0, 15.0);
    add_hip_r("Socket_Prop_Stowed_ShellItem_01", 3.0, 15.0);

    // Daggers & Sidearms
    AttachmentOffset tiel_dagger;
    tiel_dagger.collision.anchor = "pelvis";
    tiel_dagger.collision.clearance = 2.5;
    tiel_dagger.collision.max_push = 15.0;
    tiel_dagger.collision.direction = {-0.7169, 0.6162, 0.3261};
    d["Socket_Prop_Tiel_Dagger"] = tiel_dagger;

    add_hip_r("Socket_KatanaR_Stowed_01", 2.5, 15.0);
    add_hip_l("Socket_KatanaL_Stowed_01", 2.5, 15.0);
    add_hip_l("Socket_Prop_Stowed_LeftWeapon_01", 2.5, 15.0);

    return d;
}
void AttachmentOffsets::configure(const std::map<std::string,AttachmentOffset>& offsets, bool include_defaults) {
    // The MISC "keep default position" switch drops the built-in servos: with no defaults and no
    // per-variant offsets, offsets_ ends empty, update() no-ops and release() returns every tracked
    // prop to its native game transform.
    std::map<std::string,AttachmentOffset> combined = include_defaults ? default_attachment_offsets() : std::map<std::string,AttachmentOffset>{};
    for(const auto& [socket, offset] : offsets) {
        combined[socket] = offset;
    }
    if(offsets_ == combined) return;
    release();
    offsets_ = std::move(combined);
}
bool AttachmentOffsets::collides() const {
    for(const auto& [socket,offset]:offsets_) if(offset.collision.active()) return true;
    return false;
}
void AttachmentOffsets::release() {
    for(auto& item:tracked_) if(auto* child=item.child.Get();child && item.owned) {
        try { if(attach_socket(child)==item.socket) set_relative(child,item.location,item.rotation); } catch(...) {}
    }
    tracked_.clear(); poses_.clear(); offsets_.clear();
}
void AttachmentOffsets::update(UObject* component) {
    if(!component) { release(); return; }
    if(offsets_.empty() && tracked_.empty()) return;
    // Every driver bone has to exist on the worn mesh before any of this can be trusted:
    // a missing bone would make GetSocketLocation fall back to the component origin and
    // the push would be nonsense.
    auto children=attached_children(component);
    std::erase_if(tracked_,[&](auto& item){
        auto* child=item.child.Get();
        return !child || std::none_of(children.begin(),children.end(),[&](const auto& w){return w.Get()==child;}) || attach_socket(child)!=item.socket;
    });
    poses_.clear();
    for(const auto& weak:children) {
        auto* child=weak.Get(); if(!child) continue;
        if(!child->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"))) &&
           !child->IsA(static_cast<UClass*>(find(L"/Script/Engine.StaticMeshComponent")))) continue;
        const auto socket=attach_socket(child);
        auto key=narrow(socket.ToString());
        const auto entry=offsets_.find(key); if(entry==offsets_.end()) continue;
        auto tracked=std::find_if(tracked_.begin(),tracked_.end(),[&](const auto& item){return item.child.Get()==child;});
        if(tracked==tracked_.end()) {
            if(tracked_.size()>=64) throw std::runtime_error("Too many corrected attachments");
            tracked_.push_back({weak,socket,std::move(key),relative_location(child),relative_rotation(child),{},false,-1,0,-1,{}});
            tracked=std::prev(tracked_.end());
        }
        apply(component,*tracked,entry->second,true);
    }
}
#ifdef CSS_INVENTORY_DEV
Json AttachmentOffsets::diagnostics() const {
    Json sockets=Json::array();
    for(const auto& [socket,offset]:offsets_)
        sockets.push_back({{"socket",socket},{"clearance",offset.collision.clearance},{"active",offset.collision.active()}});
    Json tracked=Json::array();
    for(const auto& item:tracked_) tracked.push_back({{"socket",item.socket_key},{"owned",item.owned},{"base",item.location},
                                                     {"distance",item.distance},{"distance_root",item.distance_root},{"push",item.push},{"body",item.body}});
    return {{"distance_to_body",last_distance_},{"push",last_push_},
            {"configured",std::move(sockets)},{"tracked",std::move(tracked)}};
}
#endif
void AttachmentOffsets::push(UObject* component) {
    if(!component || tracked_.empty()) return;
    poses_.clear();
    for(auto& item:tracked_) {
        auto* child=item.child.Get(); if(!child) continue;
        const auto entry=offsets_.find(item.socket_key);
        if(entry==offsets_.end() || !entry->second.collision.active()) continue;
        // Some animations borrow a stowed item: a parry reaches for the seal and the game
        // re-attaches it to a hand until the move ends. While it is somewhere else it is
        // not ours to correct, and the discovery pass only reruns four times a second.
        if(attach_socket(child)!=item.socket) { item.owned=false; continue; }
        apply(component,item,entry->second,true);
    }
}
void Appearance::sync_seals() {
    offsets_.push(active()?component_.Get():nullptr);
}
#ifdef CSS_INVENTORY_DEV
Json AttachmentOffsets::tune(double lift,double clearance,double max_push) {
    auto length=[](const std::array<double,3>& v){ return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); };
    Json result=Json::object();
    for(auto& [socket,offset]:offsets_) {
        const double current=length(offset.location);
        if(lift>=0 && current>1e-6) for(auto& value:offset.location) value*=lift/current;
        else if(lift==0) offset.location={};
        if(clearance>=0) offset.collision.clearance=clearance;
        if(max_push>=0) offset.collision.max_push=max_push;
        result[socket]={{"lift",length(offset.location)},{"clearance",offset.collision.clearance},{"max_push",offset.collision.max_push}};
    }
    // Nothing else to do: apply() already rewrites whatever no longer matches the new
    // offset. Clearing `owned` here would make it re-read the base from a transform CSS
    // had itself moved, folding the old offset into the base a little more each time.
    return result;
}
Json Appearance::seal_diagnostics() const { return offsets_.diagnostics(); }
#endif
void Appearance::sync_attachments() {
    attachments_.update(active()?component_.Get():nullptr,original_);
    offsets_.update(active()?component_.Get():nullptr);
    auto* menu=menu_component_.Get();
    menu_attachments_.update(menu && mesh_asset(menu)==menu_applied_.Get()?menu:nullptr,menu_original_);
}

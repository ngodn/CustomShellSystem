// Included by engine.cpp after hand_support.inl (uses hand_runtime::barrier and node_type).
namespace foot_ik_runtime {
FProperty* field(UStruct* owner,const wchar_t* name) {
    auto* property=optional_field(owner,name);
    if(!property || property->GetArrayDim()!=1 || property->GetOffset_Internal()<0 ||
       property->GetOffset_Internal()+property->GetElementSize()>owner->GetPropertiesSize())
        throw std::runtime_error("Foot IK field layout mismatch: "+narrow(name));
    return property;
}
FBoolProperty* flag_field(UScriptStruct* type) {
    auto* property=field(type,L"bSetRefPoseFromSkeleton");
    if(!property->IsA<FBoolProperty>()) throw std::runtime_error("Foot IK mesh-pose flag is not a bool");
    return static_cast<FBoolProperty*>(property);
}
UObject* object(void* owner,FProperty* property) {
    if(!property->IsA<FObjectProperty>() || property->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Foot IK object field mismatch");
    UObject* value=nullptr;
    std::memcpy(&value,static_cast<std::byte*>(owner)+property->GetOffset_Internal(),sizeof value);
    return value;
}
// Length of FTransform::Translation in a reflected transform value.
double translation_length(UScriptStruct* transform,const std::byte* data) {
    auto* translation=field(transform,L"Translation");
    if(!translation->IsA<FStructProperty>()) throw std::runtime_error("Foot IK transform layout mismatch");
    auto* vector=static_cast<FStructProperty*>(translation)->GetStruct().Get();
    double sum=0;
    for(const wchar_t* axis:{L"X",L"Y",L"Z"}) {
        auto* member=field(vector,axis);
        if(member->GetElementSize()!=sizeof(double)) throw std::runtime_error("Foot IK vector layout mismatch");
        double value;std::memcpy(&value,data+translation->GetOffset_Internal()+member->GetOffset_Internal(),sizeof value);
        sum+=value*value;
    }
    return std::sqrt(sum);
}
UObject* hierarchy(UObject* rig) {
    Call get(rig,L"GetHierarchy",1);get.run();
    auto* value=get.get<UObject*>();
    if(!value) throw std::runtime_error("Foot IK rig has no hierarchy");
    return value;
}
std::optional<double> mesh_length(UObject* component,const wchar_t* bone) {
    Call index(component,L"GetBoneIndex",2);index.set(L"BoneName",FName(bone,FNAME_Add));index.run();
    const auto bone_index=index.get<int32_t>();
    if(bone_index<0) return std::nullopt;
    Call position(component,L"GetRefPosePosition",2);position.set(L"BoneIndex",bone_index);position.run();
    const auto value=position.get<std::array<double,3>>();
    const double length=std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
    return std::isfinite(length)?std::optional<double>(length):std::nullopt;
}
}

void NativeFootIk::restore_rig() {
    auto* rig=rig_.Get();
    if(!rig || snapshot_.empty()) return;
    auto* bones=foot_ik_runtime::hierarchy(rig);
    for(const auto& [key,transform]:snapshot_) {
        Call set(bones,L"SetLocalTransform",6);
        auto* key_param=set.param(L"InKey");auto* transform_param=set.param(L"InTransform");
        if(size_t(key_param->GetElementSize())!=key.size() || size_t(transform_param->GetElementSize())!=transform.size())
            throw std::runtime_error("Foot IK restore layout mismatch");
        key_param->CopyCompleteValue(set.data(key_param),key.data());
        transform_param->CopyCompleteValue(set.data(transform_param),transform.data());
        set.set(L"bInitial",true);set.set(L"bAffectChildren",false);
        set.set(L"bSetupUndo",false);set.set(L"bPrintPythonCommands",false);set.run();
    }
    // The engine cleared the rig's reset-before-construction switch when it took the mesh pose,
    // so construction keeps the initial transforms written back above.
    Call construct(rig,L"RequestConstruction",0);construct.run();
}
void NativeFootIk::release() {
    try {
        if(applied_) {
            auto* instance=instance_.Get();
            if(instance && node_offset_>=0) {
                hand_runtime::barrier(instance);
                auto* node=reinterpret_cast<std::byte*>(instance)+node_offset_;
                foot_ik_runtime::flag_field(hand_runtime::node_type())->SetPropertyValueInContainer(node,original_);
                restore_rig();
            }
            // A destroyed instance needs nothing: its replacement starts from the class defaults.
        }
    } catch(...) {
        applied_=false;instance_=nullptr;rig_=nullptr;mesh_=nullptr;node_offset_=-1;snapshot_.clear();
        state_="restore failed";throw;
    }
    applied_=false;instance_=nullptr;rig_=nullptr;mesh_=nullptr;node_offset_=-1;snapshot_.clear();
    rig_lengths_={};difference_.reset();state_="inactive";
}
void NativeFootIk::sync(UObject* component,bool worn) {
    auto* instance=worn && component?read<UObject*>(component,L"AnimScriptInstance"):nullptr;
    if(!instance) {
        if(applied_ || instance_.Get()) release();
        return;
    }
    auto* type=hand_runtime::node_type();
    if(instance_.Get()!=instance) {
        release();
        auto* class_field=foot_ik_runtime::field(type,L"ControlRigClass");
        int32_t found=-1;
        for(auto* property:instance->GetClassPrivate()->ForEachProperty()) {
            if(!property->IsA<FStructProperty>() || static_cast<FStructProperty*>(property)->GetStruct().Get()!=type) continue;
            if(property->GetArrayDim()!=1 || property->GetElementSize()!=type->GetPropertiesSize() || property->GetOffset_Internal()<0 ||
               property->GetOffset_Internal()+property->GetElementSize()>instance->GetClassPrivate()->GetPropertiesSize())
                throw std::runtime_error("Foot IK node exceeds animation instance");
            auto* rig_class=foot_ik_runtime::object(static_cast<std::byte*>(static_cast<void*>(instance))+property->GetOffset_Internal(),class_field);
            if(!rig_class || narrow(rig_class->GetPathName())!=native_foot_ik::rig_class) continue;
            if(found>=0) throw std::runtime_error("Ambiguous native foot IK node");
            found=property->GetOffset_Internal();
        }
        instance_=instance;node_offset_=found;
        state_=found<0?"no native foot IK on this animation":"waiting for native foot IK";
    }
    if(node_offset_<0) return;
    auto* node=reinterpret_cast<std::byte*>(instance)+node_offset_;
    auto* rig=foot_ik_runtime::object(node,foot_ik_runtime::field(type,L"ControlRig"));
    if(!rig) {state_="waiting for native foot IK";return;}
    auto* mesh=mesh_asset(component);
    if(rig_.Get()==rig && mesh_.Get()==mesh) return;   // every frame after the first: two compares
    hand_runtime::barrier(instance);
    if(rig_.Get()!=rig) {
        // Capture the rig as authored. A rig recreated while the flag is set starts from its
        // defaults again, so its capture is just as clean.
        snapshot_.clear();rig_lengths_={};
        auto* bones=foot_ik_runtime::hierarchy(rig);
        // UFUNCTION name; "GetBones" is only its script name.
        Call list(bones,L"GetBoneKeys",2);list.set(L"bTraverse",true);list.run();
        auto* result=list.param(L"ReturnValue");
        if(!result->IsA<FArrayProperty>()) throw std::runtime_error("Foot IK bone list is not an array");
        auto* inner=static_cast<FArrayProperty*>(result)->GetInner();
        if(!inner || !inner->IsA<FStructProperty>()) throw std::runtime_error("Foot IK bone key layout mismatch");
        auto* name_field=foot_ik_runtime::field(static_cast<FStructProperty*>(inner)->GetStruct().Get(),L"Name");
        if(!name_field->IsA<FNameProperty>()) throw std::runtime_error("Foot IK bone key name mismatch");
        FScriptArrayHelper keys(static_cast<FArrayProperty*>(result),list.data(result));
        if(keys.Num()<=0 || keys.Num()>2048) throw std::runtime_error("Foot IK bone count out of range");
        for(int32_t i=0;i<keys.Num();++i) {
            const auto* key=reinterpret_cast<const std::byte*>(keys.GetRawPtr(i));
            Call get(bones,L"GetLocalTransform",3);
            auto* key_param=get.param(L"InKey");
            if(key_param->GetElementSize()!=inner->GetElementSize() || !key_param->SameType(inner))
                throw std::runtime_error("Foot IK bone key type mismatch");
            key_param->CopyCompleteValue(get.data(key_param),key);
            get.set(L"bInitial",true);get.run();
            auto* value=get.param(L"ReturnValue");
            if(!value->IsA<FStructProperty>()) throw std::runtime_error("Foot IK transform is not a struct");
            const auto* bytes=static_cast<const std::byte*>(get.data(value));
            snapshot_.emplace_back(std::vector<std::byte>(key,key+inner->GetElementSize()),
                                   std::vector<std::byte>(bytes,bytes+value->GetElementSize()));
            FName name;std::memcpy(&name,key+name_field->GetOffset_Internal(),sizeof name);
            for(size_t s=0;s<native_foot_ik::segment_bones.size();++s)
                if(name==FName(native_foot_ik::segment_bones[s],FNAME_Add))
                    rig_lengths_[s]=foot_ik_runtime::translation_length(static_cast<FStructProperty*>(value)->GetStruct().Get(),bytes);
        }
        rig_=rig;
    }
    native_foot_ik::Lengths body{};
    for(size_t s=0;s<native_foot_ik::segment_bones.size();++s)
        body[s]=foot_ik_runtime::mesh_length(component,native_foot_ik::segment_bones[s]);
    auto* skeleton=mesh?read<UObject*>(mesh,L"Skeleton"):nullptr;
    const auto skeleton_path=skeleton?narrow(skeleton->GetPathName()):std::string{};
    difference_=native_foot_ik::largest_difference(rig_lengths_,body);
    const bool want=native_foot_ik::needs_mesh_lengths(skeleton_path,rig_lengths_,body);
    auto* flag=foot_ik_runtime::flag_field(type);
    if(want && !applied_) {
        original_=flag->GetPropertyValueInContainer(node);
        if(!original_) {flag->SetPropertyValueInContainer(node,true);applied_=true;}
    } else if(!want && applied_) {
        flag->SetPropertyValueInContainer(node,original_);
        restore_rig();applied_=false;
    }
    // While applied, a new mesh is picked up by the engine itself: its pose hash includes the mesh.
    mesh_=mesh;
    state_=applied_?"leg lengths from the worn body":want?"game rig already uses mesh lengths":"rig as authored";
}
Json NativeFootIk::status() const {
    Json out{{"state",state_},{"applied",applied_},{"captured_bones",snapshot_.size()}};
    if(difference_) out["largest_leg_difference_cm"]=*difference_;
    return out;
}
void Appearance::sync_native_foot_ik(bool eligible) {
    const bool worn=eligible && active();
    native_foot_ik_.sync(worn?component_.Get():nullptr,worn);
}

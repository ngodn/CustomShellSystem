// Included by engine.cpp after the post-process instance helpers.
namespace hand_runtime {
void barrier(UObject* instance) {
    // GetProxyOnAnyThread waits for this component's outstanding animation task.
    Call call(instance,L"GetCurveValue",2);
    call.set(L"CurveName",FName(L"None"));call.run();
    (void)call.get<float>();
}
FProperty* checked(UStruct* owner,const wchar_t* name) {
    auto* property=optional_field(owner,name);
    if(!property || property->GetArrayDim()!=1 || property->GetOffset_Internal()<0 ||
       property->GetOffset_Internal()+property->GetElementSize()>owner->GetPropertiesSize())
        throw std::runtime_error("Hand rig field layout mismatch: "+narrow(name));
    return property;
}
void* address(void* owner,FProperty* property) {
    return static_cast<std::byte*>(owner)+property->GetOffset_Internal();
}
FProperty* object_field(UStruct* owner,const wchar_t* name) {
    auto* property=checked(owner,name);
    if(!property->IsA<FObjectProperty>() || property->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Hand rig object field mismatch: "+narrow(name));
    return property;
}
UObject* object(void* owner,FProperty* property) {
    UObject* value=nullptr;std::memcpy(&value,address(owner,property),sizeof value);return value;
}
void set_object(void* owner,FProperty* property,UObject* value) {
    property->CopyCompleteValue(address(owner,property),&value);
}
void enable(UObject* rig,bool enabled,float carry=0.f) {
    auto* flag=checked(rig->GetClassPrivate(),L"ShoulderEnabled");
    auto* arm=checked(rig->GetClassPrivate(),L"ShoulderArmEnabled");
    auto* alpha=checked(rig->GetClassPrivate(),L"ShoulderCarryAlpha");
    if(!flag->IsA<FBoolProperty>() || !arm->IsA<FBoolProperty>() || !alpha->IsA<FNumericProperty>() ||
       alpha->GetElementSize()!=sizeof(float) || !static_cast<FNumericProperty*>(alpha)->IsFloatingPoint())
        throw std::runtime_error("Hand rig controls have incompatible types");
    alpha->CopyCompleteValue(address(rig,alpha),&carry);
    static_cast<FBoolProperty*>(arm)->SetPropertyValueInContainer(rig,enabled && carry>0.f);
    static_cast<FBoolProperty*>(flag)->SetPropertyValueInContainer(rig,enabled);
}
void targets(UObject* rig,const hand_support::Profile& profile) {
    auto* field=checked(rig->GetClassPrivate(),L"ShoulderCarryTargets");
    if(!field->IsA<FArrayProperty>()) throw std::runtime_error("Hand support targets are not an array");
    auto* array=static_cast<FArrayProperty*>(field);auto* inner=array->GetInner();
    if(!inner || !inner->IsA<FStructProperty>() || inner->GetArrayDim()!=1 || inner->GetElementSize()!=sizeof(hand_support::Quat))
        throw std::runtime_error("Hand support quaternion layout mismatch");
    auto* type=static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if(!type || type->GetFName()!=FName(L"Quat")) throw std::runtime_error("Unexpected hand support target type");
    const wchar_t* coordinates[]={L"X",L"Y",L"Z",L"W"};
    for(int i=0;i<4;++i) {
        auto* member=checked(type,coordinates[i]);
        if(member->GetElementSize()!=sizeof(double) || member->GetOffset_Internal()!=i*int(sizeof(double)) ||
           !member->IsA<FNumericProperty>() || !static_cast<FNumericProperty*>(member)->IsFloatingPoint())
            throw std::runtime_error("Hand support quaternion member mismatch");
    }
    FScriptArrayHelper values(array,address(rig,field));
    if(values.Num()!=19) throw std::runtime_error("Hand support target count changed");
    for(int i=0;i<19;++i) {
        const auto& q=profile.standing[i];
        std::memcpy(values.GetRawPtr(i),q.data(),sizeof q);
    }
}
double input_number(UObject* rig,const wchar_t* name) {
    auto* field=checked(rig->GetClassPrivate(),name);
    if(!field->IsA<FNumericProperty>() || field->GetElementSize()!=sizeof(float) ||
       !static_cast<FNumericProperty*>(field)->IsFloatingPoint())
        throw std::runtime_error("Hand rig input number layout mismatch");
    float value;std::memcpy(&value,address(rig,field),sizeof value);return value;
}
void grip_reference(UObject* rig,const hand_support::Grip& grip) {
    auto* field=checked(rig->GetClassPrivate(),L"ShoulderGrip");
    if(!field->IsA<FStructProperty>()) throw std::runtime_error("Hand grip is not a transform");
    auto* type=static_cast<FStructProperty*>(field)->GetStruct().Get();
    if(!type || type->GetFName()!=FName(L"Transform") || field->GetElementSize()!=type->GetPropertiesSize())
        throw std::runtime_error("Hand grip transform layout mismatch");
    auto write=[&](const wchar_t* name,const wchar_t* expected,const double* data,int count) {
        auto* member=checked(type,name);
        if(!member->IsA<FStructProperty>() || member->GetElementSize()!=count*int(sizeof(double)))
            throw std::runtime_error("Hand grip member layout mismatch");
        auto* member_type=static_cast<FStructProperty*>(member)->GetStruct().Get();
        if(!member_type || member_type->GetFName()!=FName(expected))
            throw std::runtime_error("Hand grip member type mismatch");
        const wchar_t* coordinates[]={L"X",L"Y",L"Z",L"W"};
        for(int i=0;i<count;++i) {
            auto* coordinate=checked(member_type,coordinates[i]);
            if(coordinate->GetElementSize()!=sizeof(double) || coordinate->GetOffset_Internal()!=i*int(sizeof(double)) ||
               !coordinate->IsA<FNumericProperty>() || !static_cast<FNumericProperty*>(coordinate)->IsFloatingPoint())
                throw std::runtime_error("Hand grip coordinate layout mismatch");
        }
        std::memcpy(address(address(rig,field),member),data,count*sizeof(double));
    };
    write(L"Rotation",L"Quat",grip.rotation.data(),4);
    write(L"Translation",L"Vector",grip.translation.data(),3);
    constexpr double unit_scale[]={1.,1.,1.};write(L"Scale3D",L"Vector",unit_scale,3);
}
std::pair<hand_support::CarryInput,UObject*> sample(UObject* component,UObject* pawn,UObject* rig,bool enabled,const hand_support::Catalog& catalog) {
    hand_support::CarryInput input;input.enabled=enabled;
    if(!enabled || !pawn || read<UObject*>(pawn,L"Mesh")!=component) return {input,nullptr};
    auto* weapons=read<UObject*>(pawn,L"WeaponsComponent");
    if(!weapons) return {input,nullptr};
    Call in_hand(weapons,L"GetWeaponInHand",1);in_hand.run();auto* weapon=in_hand.get<UObject*>();
    if(!weapon) return {input,nullptr};
    input.weapon=catalog.weapon_class(narrow(weapon->GetClassPrivate()->GetPathName()));
    if(input.weapon.empty()) return {input,weapon};
    auto* root=read<UObject*>(weapon,L"RootComponent");
    auto* hidden=checked(weapon->GetClassPrivate(),L"bHidden");
    if(!hidden->IsA<FBoolProperty>()) throw std::runtime_error("Weapon visibility layout mismatch");
    input.held=root && attach_parent(root)==component && attach_socket(root)==FName(L"Socket_Prop_R") &&
        !static_cast<FBoolProperty*>(hidden)->GetPropertyValueInContainer(weapon);
    auto* movement=read<UObject*>(pawn,L"CharacterMovement");
    auto* animation=read<UObject*>(component,L"AnimScriptInstance");
    if(!input.held || !movement || !animation) return {input,weapon};
    Call grounded(movement,L"IsMovingOnGround",1);grounded.run();input.grounded=grounded.get<bool>();
    Call montage(animation,L"IsAnyMontagePlaying",1);montage.run();input.action=montage.get<bool>();
    input.action=input.action || sidearm_in_hand(pawn,true) || gameplay_input_blocked(pawn);
    // The rig records its incoming pose before the arm solve. Reading the
    // corrected component here would feed the grip compensation back into its gate.
    input.wrist_height=input_number(rig,L"ShoulderInputWristHeight");
    input.prop_local_z=input_number(rig,L"ShoulderInputPropZ");
    auto* valid=checked(rig->GetClassPrivate(),L"ShoulderArmValid");
    if(!valid->IsA<FBoolProperty>()) throw std::runtime_error("Hand reachability flag layout mismatch");
    input.held=input.held && static_cast<FBoolProperty*>(valid)->GetPropertyValueInContainer(rig);
    return {input,weapon};
}
UScriptStruct* node_type() {
    return static_cast<UScriptStruct*>(find(L"/Script/ControlRig.AnimNode_ControlRig"));
}
using Inputs=std::vector<std::pair<FProperty*,FProperty*>>;
Inputs input_bindings(void* node,UScriptStruct* type,UObject* instance,UObject* rig) {
    auto names=[&](const wchar_t* name) {
        auto* field=checked(type,name);
        if(!field->IsA<FArrayProperty>()) throw std::runtime_error("Hand rig input names are not an array");
        auto* array=static_cast<FArrayProperty*>(field);
        auto* inner=array->GetInner();
        if(!inner || !inner->IsA<FNameProperty>() || inner->GetElementSize()!=sizeof(FName))
            throw std::runtime_error("Hand rig input name layout mismatch");
        FScriptArrayHelper values(array,address(node,field));
        if(values.Num()<0 || values.Num()>128) throw std::runtime_error("Hand rig input count exceeds bound");
        std::vector<FName> result;
        for(int i=0;i<values.Num();++i) {
            FName value;std::memcpy(&value,values.GetRawPtr(i),sizeof value);result.push_back(value);
        }
        return result;
    };
    const auto sources=names(L"SourcePropertyNames"),destinations=names(L"DestPropertyNames");
    if(sources.size()!=destinations.size()) throw std::runtime_error("Hand rig input mappings differ in length");
    Inputs result;
    for(size_t i=0;i<sources.size();++i) {
        const auto source_name=sources[i].ToString(),destination_name=destinations[i].ToString();
        if(destination_name.starts_with(L"Shoulder")) throw std::runtime_error("Original graph binds a shoulder control");
        auto* source=checked(instance->GetClassPrivate(),source_name.c_str());
        auto* destination=checked(rig->GetClassPrivate(),destination_name.c_str());
        if(!source->SameType(destination) || source->GetElementSize()!=destination->GetElementSize())
            throw std::runtime_error("Hand rig input types differ");
        result.emplace_back(source,destination);
    }
    return result;
}
void copy_inputs(UObject* instance,UObject* rig,const Inputs& inputs) {
    for(const auto& [source,destination]:inputs) {
        if(source->IsA<FArrayProperty>()) {
            FScriptArrayHelper values(static_cast<FArrayProperty*>(source),address(instance,source));
            if(values.Num()<0 || values.Num()>4096) throw std::runtime_error("Hand rig input array exceeds bound");
        }
        if(source->IsA<FStrProperty>()) {
            const auto& value=*static_cast<FString*>(address(instance,source));
            if(value.GetCharArray().Num()<0 || value.GetCharArray().Num()>262145)
                throw std::runtime_error("Hand rig input string exceeds bound");
        }
    }
    // UE 5.6 keeps its cached pin writers bound to the old rig after a class
    // switch. Forward only the compiled input pins, never simulation state.
    for(const auto& [source,destination]:inputs) {
        auto* from=address(instance,source);auto* to=address(rig,destination);
        if(source->IsA<FBoolProperty>())
            static_cast<FBoolProperty*>(destination)->SetPropertyValue(to,
                static_cast<FBoolProperty*>(source)->GetPropertyValue(from));
        else destination->CopyCompleteValue(to,from);
    }
}
void expand_output(void* node,UScriptStruct* type,const std::vector<std::string>& required) {
    auto* property=checked(type,L"OutputBonesToTransfer");
    if(!property->IsA<FArrayProperty>()) throw std::runtime_error("Hand output filter is not an array");
    auto* array=static_cast<FArrayProperty*>(property);
    auto* inner=array->GetInner();
    if(!inner || !inner->IsA<FStructProperty>() || inner->GetArrayDim()!=1)
        throw std::runtime_error("Hand output bone layout mismatch");
    auto* bone=static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if(!bone || bone->GetFName()!=FName(L"BoneReference") || inner->GetElementSize()!=bone->GetPropertiesSize())
        throw std::runtime_error("Hand output filter contains unexpected elements");
    auto* name=checked(bone,L"BoneName");
    if(!name->IsA<FNameProperty>() || name->GetElementSize()!=sizeof(FName))
        throw std::runtime_error("Hand output bone name mismatch");
    FScriptArrayHelper values(array,address(node,property));
    if(values.Num()<0 || values.Num()>512) throw std::runtime_error("Hand output filter exceeds bound");
    // An empty filter already transfers every bone.
    if(values.Num()==0) return;
    for(const auto& bone_name:required) {
        const FName wanted(wide(bone_name).c_str());
        bool found=false;
        for(int32_t i=0;i<values.Num();++i) {
            FName existing;std::memcpy(&existing,address(values.GetRawPtr(i),name),sizeof existing);
            if(existing==wanted) {found=true;break;}
        }
        if(!found) {
            // The pinned UE4SS DLL does not export the array helper's grow path.
            struct RawArray {void* data;int32_t num,max;};
            static_assert(sizeof(RawArray)==16);
            if(property->GetElementSize()!=sizeof(RawArray)) throw std::runtime_error("Hand output array header mismatch");
            auto& raw=*static_cast<RawArray*>(address(node,property));
            if(raw.num<0 || raw.num>raw.max || raw.max>4096 || (raw.max && !raw.data))
                throw std::runtime_error("Hand output array storage mismatch");
            const auto size=static_cast<size_t>(inner->GetElementSize());
            const int32_t index=raw.num;
            if(raw.num==raw.max) {
                auto* storage=(*GMalloc)->Realloc(raw.data,(raw.num+1)*size,static_cast<uint32>(inner->GetMinAlignment()));
                if(!storage) throw std::runtime_error("Hand output allocation failed");
                raw.data=storage;raw.max=raw.num+1;
            }
            auto* item=static_cast<std::byte*>(raw.data)+index*size;
            std::memset(item,0,size);inner->InitializeValue(item);++raw.num;
            name->CopyCompleteValue(address(item,name),&wanted);
        }
    }
}
}

void HandSupport::release() {
    if(auto* instance=instance_.Get()) {
        hand_runtime::barrier(instance);
        if(auto* rig=rig_.Get()) hand_runtime::enable(rig,false);
    }
    // Keep the compatible class on a surviving instance. Disabling its appended
    // stage preserves the original graph without restarting CW's body simulation.
    rig_=nullptr;instance_=nullptr;component_=nullptr;candidate_=nullptr;unmatched_=nullptr;inputs_.clear();
    carry_weapon_=nullptr;carry_.reset();last_tick_=0;moving_phase_=0.;
    roots_.release();node_offset_=-1;enabled_=false;state_="inactive";
}
std::string HandSupport::configure(const fs::path& path) {
    release();catalog_.reset();catalog_error_.clear();
    try {catalog_=hand_support::Catalog::load(path);}
    catch(const std::exception& error) {catalog_error_=error.what();}
    return catalog_error_;
}
void HandSupport::sync(UObject* component,bool enabled,UObject* pawn) {
    if(!catalog_) {state_="catalog unavailable";return;}
    const auto& catalog=*catalog_;
    if(busy_) return;
    busy_=true;
    struct Reset {bool& flag;~Reset(){flag=false;}} reset{busy_};
    auto* instance=post_process_instance(component);
    auto* mesh=component?mesh_asset(component):nullptr;
    const auto* binding=mesh && instance?catalog.match(narrow(mesh->GetPathName()),narrow(instance->GetClassPrivate()->GetPathName())):nullptr;
    if(component_.Get()!=component || instance_.Get()!=instance || !binding) release();
    if(!binding) return;
    if(!enabled && !candidate_.Get()) {state_="default game";return;}
    // A graph that was searched once and holds no bound rig stays that way for this instance.
    if(unmatched_.Get()==instance) return;
    auto* type=hand_runtime::node_type();
    auto* rig_field=hand_runtime::object_field(type,L"ControlRig");
    auto* class_field=hand_runtime::object_field(type,L"ControlRigClass");
    auto* default_field=hand_runtime::object_field(type,L"DefaultControlRigClass");
    if(node_offset_<0) {
        // Read-only search: node rig pointers are set when the graph initialises, never by
        // the evaluation task, so no barrier is needed until something is written.
        int32_t selected=-1;
        bool initialised=false;
        for(auto* property:instance->GetClassPrivate()->ForEachProperty()) {
            if(!property->IsA<FStructProperty>() || static_cast<FStructProperty*>(property)->GetStruct().Get()!=type) continue;
            if(property->GetArrayDim()!=1 || property->GetElementSize()!=type->GetPropertiesSize() || property->GetOffset_Internal()<0 ||
               property->GetOffset_Internal()+property->GetElementSize()>instance->GetClassPrivate()->GetPropertiesSize())
                throw std::runtime_error("Hand rig node exceeds animation instance");
            auto* node=hand_runtime::address(instance,property);
            auto* rig=hand_runtime::object(node,rig_field);
            if(!rig) continue;
            initialised=true;
            const auto path=narrow(rig->GetClassPrivate()->GetPathName());
            if(path!=binding->original && path!=binding->candidate) continue;
            if(selected>=0) throw std::runtime_error("Ambiguous hand rig node");
            selected=property->GetOffset_Internal();
        }
        if(selected<0) {
            // Rigs exist but none is the catalog's original: the package changed its graph.
            if(initialised) {component_=component;instance_=instance;unmatched_=instance;state_="original rig not in graph";}
            else state_="waiting for original rig";
            return;
        }
        WeakObject pending_component(component),pending_instance(instance),pending_mesh(mesh);
        AssetLoadRoots loading;
        auto* candidate=load(std::string(binding->candidate));loading.keep(candidate);
        if(!candidate->IsA<UClass>() || narrow(candidate->GetPathName())!=binding->candidate)
            throw std::runtime_error("Hand support asset is not the expected class");
        if(pending_component.Get()!=component || pending_instance.Get()!=instance || pending_mesh.Get()!=mesh ||
           mesh_asset(component)!=mesh || post_process_instance(component)!=instance)
            throw std::runtime_error("Hand rig owner changed during loading");
        hand_runtime::barrier(instance);
        auto* node=reinterpret_cast<std::byte*>(instance)+selected;
        auto* original=hand_runtime::object(node,rig_field);
        if(!original || (narrow(original->GetClassPrivate()->GetPathName())!=binding->original &&
                         narrow(original->GetClassPrivate()->GetPathName())!=binding->candidate))
            throw std::runtime_error("Hand rig changed during loading");
        hand_runtime::expand_output(node,type,binding->output_bones);
        if(!hand_runtime::object(node,default_field))
            hand_runtime::set_object(node,default_field,hand_runtime::object(node,class_field));
        hand_runtime::set_object(node,class_field,candidate);
        component_=component;instance_=instance;candidate_=candidate;node_offset_=selected;roots_.take(loading);
        state_="waiting for compatible rig";
        return;
    }
    auto* node=reinterpret_cast<std::byte*>(instance)+node_offset_;
    auto* rig=hand_runtime::object(node,rig_field);
    if(!rig || rig->GetClassPrivate()!=candidate_.Get()) {
        if(hand_runtime::object(node,class_field)!=candidate_.Get())
            throw std::runtime_error("Engine rejected or replaced the hand support rig");
        state_="waiting for compatible rig";return;
    }
    {
        // The one wait per tick: the rig's inputs and targets are written below.
        const auto start=std::chrono::steady_clock::now();
        hand_runtime::barrier(instance);
        const double waited=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        barrier_max_us_=std::max(barrier_max_us_,waited);
        barrier_avg_us_=barrier_samples_?barrier_avg_us_+(waited-barrier_avg_us_)/std::min<double>(barrier_samples_+1,600.):waited;
        ++barrier_samples_;
    }
    hand_runtime::enable(rig,false);
    enabled_=false;
    try {
        if(rig_.Get()!=rig) inputs_=hand_runtime::input_bindings(node,type,instance,rig);
        hand_runtime::copy_inputs(instance,rig,inputs_);
        rig_=rig;
        auto [input,weapon]=hand_runtime::sample(component,pawn,rig,enabled,catalog);
        if(carry_weapon_.Get()!=weapon) {carry_.reset();carry_weapon_=weapon;}
        const auto now=GetTickCount64();
        const double seconds=last_tick_ && now>=last_tick_?double(now-last_tick_)/1000.:0.;last_tick_=now;
        const auto* profile=catalog.profile(binding->finger_profile,input.weapon);
        const auto* grip=catalog.grip(binding->grip_profile,input.weapon);
        input.enabled=input.enabled && profile && grip;
        const auto alpha=carry_.update(input,seconds,catalog.carry());
        if(input.supported(catalog.carry())) moving_phase_=hand_support::moving_phase(input.prop_local_z,catalog.carry());
        if(profile && grip) {
            hand_runtime::targets(rig,*profile);
            hand_runtime::grip_reference(rig,*grip);
        }
        hand_runtime::enable(rig,enabled,profile && grip?static_cast<float>(alpha):0.f);
        enabled_=enabled;
        state_=!enabled?"default game":alpha>0.?"shoulder support":"articulation active";
    } catch(...) {carry_.reset();state_="deferred";throw;}
}
Json HandSupport::status() const {
    return Json{{"catalog_loaded",catalog_.has_value()},{"catalog_error",catalog_error_},{"state",state_},{"enabled",enabled_},{"carry_alpha",carry_.value()},{"moving_phase",moving_phase_},
                {"rig",rig_.Get()?narrow(rig_.Get()->GetPathName()):""},
                {"barrier_avg_us",barrier_avg_us_},{"barrier_max_us",barrier_max_us_},{"barrier_samples",barrier_samples_}};
}
void Appearance::sync_hand_support() {
    // Imported CSS clips already carry their authored finger poses. Calibration
    // here is for the game's animation input, including its combat montages.
    const bool authored=performance.active() || walk.custom_idle_engaged() ||
        (walk.engaged() && walk.reason().starts_with("custom "));
    hand_support_.sync(active()?component_.Get():nullptr,fit_defaults_ && !authored,observed_pawn_.Get());
}

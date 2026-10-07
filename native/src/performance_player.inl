// Included in css after the gameplay transition and reflected component helpers.
namespace {
struct PerformanceFields {
    UObject* instance;
    FObjectProperty* sequence;
    FBoolProperty* enabled;
    FFloatProperty* time;
    FFloatProperty* weight;
    explicit PerformanceFields(UObject* object):instance(object) {
        if(!object) throw std::runtime_error("Performance layer is unavailable");
        auto property=[&](const wchar_t* name) {
            auto* p=object->GetPropertyByNameInChain(name);
            if(!p || p->GetArrayDim()!=1 || p->GetElementSize()<=0 || p->GetOffset_Internal()<0 ||
               p->GetOffset_Internal()>object->GetClassPrivate()->GetPropertiesSize()-p->GetElementSize())
                throw std::runtime_error("Performance layer layout is unsupported");
            return p;
        };
        auto* s=property(L"CSSPerformanceSequence");
        auto* e=property(L"CSSPerformanceEnabled");
        auto* t=property(L"CSSPerformanceTime");
        auto* b=property(L"CSSPerformanceWeight");
        if(!s->IsA<FObjectProperty>() || s->GetElementSize()!=sizeof(UObject*) ||
           !e->IsA<FBoolProperty>() || !t->IsA<FFloatProperty>() || !b->IsA<FFloatProperty>())
            throw std::runtime_error("Performance layer property types differ");
        sequence=static_cast<FObjectProperty*>(s);enabled=static_cast<FBoolProperty*>(e);
        time=static_cast<FFloatProperty*>(t);weight=static_cast<FFloatProperty*>(b);
        if(sequence->GetPropertyClass().Get()!=find(L"/Script/Engine.AnimSequence"))
            throw std::runtime_error("Performance sequence property has a different class");
    }
    void* data(FProperty* p) const {return reinterpret_cast<std::byte*>(instance)+p->GetOffset_Internal();}
    UObject* clip() const {return sequence->GetObjectPropertyValue(data(sequence));}
    bool on() const {return enabled->GetPropertyValue(data(enabled));}
    void enable(bool value) const {enabled->SetPropertyValue(data(enabled),value);}
    // UE4SS's object setter wrapper is unavailable on the pinned UE 5.6 runtime.
    void set_clip(UObject* value) const {sequence->CopyCompleteValue(data(sequence),&value);}
    float seconds() const {return time->GetPropertyValue(data(time));}
    float alpha() const {return weight->GetPropertyValue(data(weight));}
    void set_time(float value) const {time->SetPropertyValue(data(time),value);}
    void set_weight(float value) const {weight->SetPropertyValue(data(weight),value);}
};
bool performance_flag(UObject* object,const wchar_t* name) {
    auto* p=object?object->GetPropertyByNameInChain(name):nullptr;
    if(!p || !p->IsA<FBoolProperty>() || p->GetArrayDim()!=1 || p->GetOffset_Internal()<0 ||
       p->GetOffset_Internal()>object->GetClassPrivate()->GetPropertiesSize()-p->GetElementSize())
        throw std::runtime_error("Performance visibility/state property is unavailable");
    return static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(object);
}
bool performance_native_component(UObject* component) {
    auto* mesh=component?mesh_asset(component):nullptr;
    auto* skeleton=mesh?read<UObject*>(mesh,L"Skeleton"):nullptr;
    auto* animation=component?read<UObject*>(component,L"AnimScriptInstance"):nullptr;
    if(!skeleton || !animation || skeleton->GetPathName()!=
        L"/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton") return false;
    auto* player_class=find(L"/Game/Sparta/Characters/Humans/Player/Animations/ABP_Player.ABP_Player_C");
    return player_class && animation->IsA(static_cast<UClass*>(player_class));
}
std::string performance_gameplay_block(UObject* pawn,UObject* owned_montage=nullptr) {
    if(!pawn) return "Player is unavailable";
    auto* controller=read<UObject*>(pawn,L"Controller");
    if(!controller || read<UObject*>(controller,L"Pawn")!=pawn) return "Player control changed";
    if(is_quest_or_teleport_active(controller) || is_traversal_ability_active(pawn)) return "Travel or traversal is active";
    if(gameplay_input_blocked(pawn)) return "A gameplay action is blocking input";
    for(const auto* name:{L"IsMoveInputIgnored",L"IsLookInputIgnored",L"IsInGameMenu"}) {
        Call blocked(controller,name,1);blocked.run();
        if(blocked.get<bool>()) return "Close the player menu and return to gameplay";
    }
    auto* movement=read<UObject*>(pawn,L"CharacterMovement");
    if(!movement) return "Player movement is unavailable";
    Call grounded(movement,L"IsMovingOnGround",1);grounded.run();
    if(!grounded.get<bool>()) return "Player is not grounded";
    const auto velocity=read<std::array<double,3>>(movement,L"Velocity");
    if(!std::isfinite(velocity[0]) || !std::isfinite(velocity[1]) || !std::isfinite(velocity[2]) ||
       std::hypot(velocity[0],velocity[1],velocity[2])>1.) return "Player is moving";
    Call acceleration(movement,L"GetCurrentAcceleration",1);acceleration.run();
    const auto input=acceleration.get<std::array<double,3>>();
    if(!std::isfinite(input[0]) || !std::isfinite(input[1]) || !std::isfinite(input[2]) ||
       std::hypot(input[0],input[1],input[2])>0.) return "Movement input is active";
    auto* component=read<UObject*>(pawn,L"Mesh");
    auto* animation=component?read<UObject*>(component,L"AnimScriptInstance"):nullptr;
    if(!animation) return "Player animation is unavailable";
    Call montage(animation,L"GetCurrentActiveMontage",1);montage.run();
    if(auto* current=montage.get<UObject*>();current && current!=owned_montage)
        return "Gameplay montage is active";
    if(!owned_montage) {
        Call playing(animation,L"IsAnyMontagePlaying",1);playing.run();
        if(playing.get<bool>()) return "Wait for the gameplay montage to finish";
    }
    if(sidearm_in_hand(pawn,true)) return "Sidearm is in use";
    return {};
}
const PerformanceAdapter* performance_adapter(UObject* component,const Catalog& catalog,
    const std::string& outfit,bool bound_layer=false) {
    const auto revision=catalog.outfit_revisions.find(outfit);
    if(revision==catalog.outfit_revisions.end()) return nullptr;
    auto* post=read<UObject*>(component,L"PostProcessAnimInstance");
    auto* mesh=mesh_asset(component);
    auto* skeleton=mesh?read<UObject*>(mesh,L"Skeleton"):nullptr;
    if(!post || !skeleton) return nullptr;
    const auto resolve=bound_layer?resolve_performance_layer:resolve_performance_adapter;
    return resolve(catalog.animation_libraries,outfit,revision->second,
        narrow(post->GetClassPrivate()->GetPathName()),narrow(skeleton->GetPathName()));
}
void performance_controls(UObject* source,UObject* target,bool copy) {
    if(!source || !target) throw std::runtime_error("Performance controls have no owner");
    size_t count=0;
    for(auto* property:source->GetClassPrivate()->ForEachProperty()) {
        const auto name=property->GetName();
        if(!name.starts_with(L"CSS") || name.starts_with(L"CSSPerformance")) continue;
        auto* destination=target->GetPropertyByNameInChain(name.c_str());
        const auto valid=[](UObject* object,FProperty* field) {
            return field && field->GetArrayDim()==1 && field->GetElementSize()>0 &&
                field->GetOffset_Internal()>=0 &&
                field->GetOffset_Internal()<=object->GetClassPrivate()->GetPropertiesSize()-field->GetElementSize();
        };
        if(++count>128 || !valid(source,property) || !valid(target,destination) ||
           !property->SameType(destination) || property->GetElementSize()!=destination->GetElementSize())
            throw std::runtime_error("Performance adapter changes a CSS control layout");
        auto* from=reinterpret_cast<std::byte*>(source)+property->GetOffset_Internal();
        auto* to=reinterpret_cast<std::byte*>(target)+destination->GetOffset_Internal();
        if(property->IsA<FArrayProperty>()) {
            FScriptArrayHelper values(static_cast<FArrayProperty*>(property),from);
            if(values.Num()<0 || values.Num()>4096) throw std::runtime_error("Performance control array exceeds bound");
        }
        if(property->IsA<FStrProperty>()) {
            const auto& value=*reinterpret_cast<FString*>(from);
            if(value.GetCharArray().Num()<0 || value.GetCharArray().Num()>262145)
                throw std::runtime_error("Performance control string exceeds bound");
        }
        if(!copy) continue;
        if(property->IsA<FBoolProperty>()) {
            static_cast<FBoolProperty*>(destination)->SetPropertyValue(to,
                static_cast<FBoolProperty*>(property)->GetPropertyValue(from));
        } else destination->CopyCompleteValue(to,from);
    }
    // Fixed-physics graphs can have no CSS controls. Binding validates the exact
    // catalog revision/class pair and checks control layouts in both directions.
}
void performance_override(UObject* component,UObject* type) {
    Call set(component,L"SetOverridePostProcessAnimBP",2);
    set.set(L"InPostProcessAnimBlueprint",type);set.set(L"ReinitAnimInstances",true);set.run();
}
void performance_montage_blend(Call& call,const wchar_t* parameter) {
    auto* outer=call.param(parameter);
    if(!outer->IsA<FStructProperty>()) throw std::runtime_error("Montage blend settings are not a struct");
    auto member=[](FProperty* parent,const wchar_t* name) {
        auto* type=static_cast<FStructProperty*>(parent)->GetStruct().Get();
        auto* p=type?optional_field(type,name):nullptr;
        if(!p || p->GetArrayDim()!=1 || p->GetOffset_Internal()<0 || p->GetElementSize()<=0 ||
           p->GetOffset_Internal()>parent->GetElementSize()-p->GetElementSize())
            throw std::runtime_error("Montage blend layout is unsupported");
        return p;
    };
    auto* blend=member(outer,L"Blend");
    if(!blend->IsA<FStructProperty>()) throw std::runtime_error("Montage alpha blend is not a struct");
    auto* data=static_cast<std::byte*>(call.data(outer))+blend->GetOffset_Internal();
    auto* time=member(blend,L"BlendTime");
    if(!time->IsA<FFloatProperty>() || time->GetElementSize()!=sizeof(float))
        throw std::runtime_error("Montage blend time is unsupported");
    const float duration=static_cast<float>(PerformanceClock::blend_seconds);
    time->CopyCompleteValue(data+time->GetOffset_Internal(),&duration);
    auto* option=member(blend,L"BlendOption");
    auto* enumeration=option->IsA<FEnumProperty>()?static_cast<FEnumProperty*>(option)->GetEnum().Get():
        option->IsA<FByteProperty>()?static_cast<FByteProperty*>(option)->GetEnum().Get():nullptr;
    if(!enumeration || option->GetElementSize()!=1 || enumeration->NumEnums()>32)
        throw std::runtime_error("Montage blend curve is unsupported");
    for(int32_t i=0;i<enumeration->NumEnums();++i) {
        const auto item=enumeration->GetEnumNameByIndex(i);
        const auto name=item.Key.ToString();
        if(name!=L"HermiteCubic" && name!=L"EAlphaBlendOption::HermiteCubic") continue;
        if(item.Value<0 || item.Value>255) break;
        const auto value=static_cast<uint8_t>(item.Value);
        option->CopyCompleteValue(data+option->GetOffset_Internal(),&value);
        return;
    }
    throw std::runtime_error("Montage has no smooth-step blend curve");
}
}
std::string PerformancePlayer::unavailable(UObject* pawn,const Catalog& catalog,const std::string& outfit) {
    const auto blocked=performance_gameplay_block(pawn);
    if(!blocked.empty()) return blocked;
    auto* component=read<UObject*>(pawn,L"Mesh");
    if(performance_native_component(component)) return {};
    Call disabled(component,L"GetDisablePostProcessBlueprint",1);disabled.run();
    if(disabled.get<bool>()) return "The outfit post-process layer is disabled";
    return unsupported(pawn,catalog,outfit);
}
bool PerformancePlayer::waiting_for_menu(UObject* pawn) {
    auto* controller=pawn?read<UObject*>(pawn,L"Controller"):nullptr;
    if(!controller) throw std::runtime_error("Player controller is unavailable");
    for(const auto* name:{L"IsMoveInputIgnored",L"IsLookInputIgnored",L"IsInGameMenu"}) {
        Call blocked(controller,name,1);blocked.run();
        if(blocked.get<bool>()) return true;
    }
    return false;
}
std::string PerformancePlayer::unsupported(UObject* pawn,const Catalog& catalog,const std::string& outfit) {
    if(!pawn) return "Player is unavailable";
    auto* component=read<UObject*>(pawn,L"Mesh");
    if(!component) return "Player mesh is unavailable";
    if(performance_native_component(component)) return {};
    auto* post=component?read<UObject*>(component,L"PostProcessAnimInstance"):nullptr;
    if(!performance_adapter(component,catalog,outfit,true)) {
        if(!performance_adapter(component,catalog,outfit)) return "This outfit version has no verified performance adapter";
        return {};
    }
    PerformanceFields fields(post);
    if(fields.on() || fields.clip() || fields.alpha()!=0.f) return "The performance layer is already in use";
    return {};
}
static UObject* performance_class_skeleton(UObject* generated_class) {
    // TargetSkeleton belongs to the class object, not to its animation instances.
    auto* type=generated_class?generated_class->GetClassPrivate():nullptr;
    auto* property=type?type->GetPropertyByNameInChain(L"TargetSkeleton"):nullptr;
    if(!property || !property->IsA<FObjectProperty>() || property->GetArrayDim()!=1 ||
       property->GetElementSize()!=sizeof(UObject*) || property->GetOffset_Internal()<0 ||
       property->GetOffset_Internal()>type->GetPropertiesSize()-property->GetElementSize())
        throw std::runtime_error("Animation class skeleton property layout mismatch");
    return static_cast<FObjectProperty*>(property)->GetObjectPropertyValue(
        reinterpret_cast<std::byte*>(generated_class)+property->GetOffset_Internal());
}
void PerformancePlayer::bind_layer(UObject* pawn,const Catalog& catalog,const std::string& outfit) {
    auto* component=read<UObject*>(pawn,L"Mesh");
    auto* source=read<UObject*>(component,L"PostProcessAnimInstance");
    if(performance_adapter(component,catalog,outfit,true)) return;
    const auto* adapter=performance_adapter(component,catalog,outfit);
    if(!adapter) throw std::runtime_error("No verified performance adapter");
    WeakObject held_pawn(pawn),held_component(component),held_mesh(mesh_asset(component)),held_source(source);
    auto* original=read<UObject*>(component,L"OverridePostProcessAnimBP");
    WeakObject held_override(original);
    auto* main=read<UObject*>(component,L"AnimScriptInstance");WeakObject held_main(main);
    AssetLoadRoots roots;
    roots.keep(source);if(original) roots.keep(original);
    auto* target=load(adapter->target_class);roots.keep(target);
    if(held_component.Get()!=component || held_source.Get()!=source || held_mesh.Get()!=mesh_asset(component) ||
       read<UObject*>(pawn,L"Mesh")!=component || read<UObject*>(component,L"PostProcessAnimInstance")!=source ||
       read<UObject*>(component,L"OverridePostProcessAnimBP")!=original ||
       read<UObject*>(component,L"AnimScriptInstance")!=main)
        throw std::runtime_error("Outfit changed while loading the performance adapter");
    if(!target || !target->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimBlueprintGeneratedClass"))) ||
       narrow(target->GetPathName())!=adapter->target_class ||
       performance_class_skeleton(target)!=read<UObject*>(held_mesh.Get(),L"Skeleton"))
        throw std::runtime_error("Loaded performance adapter has a different class or skeleton");
    auto* defaults=static_cast<UClass*>(target)->GetClassDefaultObject().Get();
    performance_controls(source,defaults,false);
    performance_controls(defaults,source,false);
    PerformanceFields initial(defaults);
    if(initial.on() || initial.clip() || initial.seconds()!=0.f || initial.alpha()!=0.f)
        throw std::runtime_error("Performance adapter defaults are not inactive");
    auto physics=astral_source_physics(component);
    if(const auto why=performance_gameplay_block(pawn);!why.empty()) throw std::runtime_error(why);
    auto layers=std::make_shared<mesh_animation_layers::Snapshot>(component);
    WeakObject held_target(target);
    // Register rollback before the reflected setter can replace the instance.
    retained_.take(roots);
    restore_layer_=[held_pawn,held_component,held_mesh,held_source,held_override,held_target,physics,
                    restored_post=WeakObject{},restored_layers=std::shared_ptr<mesh_animation_layers::Snapshot>{}]() mutable {
        auto* body=held_component.Get();auto* expected=held_target.Get();
        if(!body || !expected || mesh_asset(body)!=held_mesh.Get()) return true;
        const bool replacing=read<UObject*>(body,L"OverridePostProcessAnimBP")==expected;
        if(!replacing && (!restored_post.Get() ||
            read<UObject*>(body,L"OverridePostProcessAnimBP")!=held_override.Get() ||
            read<UObject*>(body,L"PostProcessAnimInstance")!=restored_post.Get())) return true;
        auto* source=held_source.Get();
        if(!source) throw std::runtime_error("Original performance controls expired");
        if(replacing) {
            // This engine setter also reinitializes the main instance, even when
            // its pointer is unchanged. Never run it over a native action.
            auto* main=read<UObject*>(body,L"AnimScriptInstance");
            if(main) {
                Call playing(main,L"IsAnyMontagePlaying",1);playing.run();
                if(playing.get<bool>()) return false;
            }
            if(auto* pawn=held_pawn.Get()) {
                if(read<UObject*>(pawn,L"Mesh")!=body) return true;
                if(!performance_gameplay_block(pawn).empty()) return false;
            } else return true;
            // Weapons and customization can change while restoration is deferred.
            restored_layers=std::make_shared<mesh_animation_layers::Snapshot>(body);
            auto* current=read<UObject*>(body,L"PostProcessAnimInstance");
            performance_controls(current,source,true);
            performance_override(body,held_override.Get());
            restored_post=read<UObject*>(body,L"PostProcessAnimInstance");
        }
        auto* restored=read<UObject*>(body,L"PostProcessAnimInstance");
        if(!restored || restored->GetClassPrivate()!=source->GetClassPrivate())
            throw std::runtime_error("Original outfit graph did not restore");
        if(restored_layers) restored_layers->restore(body);
        performance_controls(source,restored,true);
        astral_visual_physics(body,physics);
        return true;
    };
    try {
        performance_override(component,target);
        layers->restore(component);
        auto* post=read<UObject*>(component,L"PostProcessAnimInstance");
        if(!post || post->GetClassPrivate()!=target || read<UObject*>(component,L"AnimScriptInstance")!=held_main.Get())
            throw std::runtime_error("Performance binding changed the native animation instance");
        performance_controls(source,post,true);
        physics.animation_class=target;
        astral_visual_physics(component,physics);
    } catch(...) {
        release();throw;
    }
}
void PerformancePlayer::bind_native_montage() {
    auto* animation=animation_.Get();auto* sequence=sequence_.Get();
    if(!animation || !sequence || !native_montage_)
        throw std::runtime_error("Native performance has no owner");
    auto* notifies=sequence->GetPropertyByNameInChain(L"Notifies");
    if(!notifies || !notifies->IsA<FArrayProperty>() || notifies->GetArrayDim()!=1 ||
       notifies->GetOffset_Internal()<0 || notifies->GetElementSize()<=0 ||
       notifies->GetOffset_Internal()>sequence->GetClassPrivate()->GetPropertiesSize()-notifies->GetElementSize())
        throw std::runtime_error("Native performance notify layout is unavailable");
    FScriptArrayHelper events(static_cast<FArrayProperty*>(notifies),
        reinterpret_cast<std::byte*>(sequence)+notifies->GetOffset_Internal());
    if(events.Num()!=0) throw std::runtime_error("Native performances cannot contain gameplay notifies");
    Call create(find(L"/Script/Engine.Default__AnimMontage"),
        L"CreateSlotAnimationAsDynamicMontage_WithBlendSettings",8);
    create.set(L"Asset",sequence);
    create.set(L"SlotNodeName",FName(L"DefaultSlot"));
    performance_montage_blend(create,L"BlendInSettings");
    performance_montage_blend(create,L"BlendOutSettings");
    create.set(L"InPlayRate",1.f);create.set(L"LoopCount",int32_t{1});
    create.set(L"InBlendOutTriggerTime",-1.f);create.run();
    auto* montage=create.get<UObject*>();
    if(!montage || !montage->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimMontage"))) ||
       read<UObject*>(montage,L"Skeleton")!=read<UObject*>(sequence,L"Skeleton"))
        throw std::runtime_error("Could not create the native performance montage");
    montage_=montage;retained_.keep(montage);
    auto* auto_out=field(montage,L"bEnableAutoBlendOut",sizeof(bool));
    if(!auto_out->IsA<FBoolProperty>()) throw std::runtime_error("Montage automatic blend property changed");
    static_cast<FBoolProperty*>(auto_out)->SetPropertyValueInContainer(montage,false);
    // The shared clock supplies position. This transient montage owns only its
    // blend and slot; it neither advances twice nor stops gameplay montages.
    Call play(animation,L"Montage_Play",6);
    play.set(L"MontageToPlay",montage);play.set(L"InPlayRate",0.f);
    play.set(L"ReturnValueType",uint8_t{0});
    play.set(L"InTimeToStartMontageAt",static_cast<float>(session_.clock().position()));
    play.set(L"bStopAllMontages",false);play.run();
    if(!std::isfinite(play.get<float>()) || play.get<float>()<=0.f)
        throw std::runtime_error("Native performance montage did not start");
}
bool PerformancePlayer::intact() const {
    auto* pawn=pawn_.Get();auto* component=component_.Get();auto* post=post_.Get();
    auto* sequence=sequence_.Get();
    const bool bound=pawn && component && sequence && mesh_.Get() && animation_.Get() && read<UObject*>(pawn,L"Mesh")==component &&
        mesh_asset(component)==mesh_.Get() && read<UObject*>(component,L"PostProcessAnimInstance")==post &&
        read<UObject*>(component,L"AnimScriptInstance")==animation_.Get() &&
        read<FName>(pawn,L"CharacterId")==character_;
    if(!bound) return false;
    if(!native_montage_) return post && PerformanceFields(post).clip()==sequence;
    auto* montage=montage_.Get();
    if(!montage) return false;
    if(montage_blending_out_) return true;
    Call active(animation_.Get(),L"Montage_IsActive",2);
    active.set(L"Montage",montage);active.run();
    return active.get<bool>();
}
void PerformancePlayer::write_pose() {
    if(native_montage_) {
        auto* montage=montage_.Get();auto* animation=animation_.Get();
        if(!montage || !animation) throw std::runtime_error("Native performance owner expired");
        if(session_.clock().phase()==PerformancePhase::BlendingOut) {
            if(!montage_blending_out_) {
                Call stop(animation,L"Montage_Stop",2);
                stop.set(L"InBlendOutTime",static_cast<float>(PerformanceClock::blend_seconds));
                stop.set(L"Montage",montage);stop.run();montage_blending_out_=true;
            }
        } else {
            Call position(animation,L"Montage_SetPosition",2);
            position.set(L"Montage",montage);
            position.set(L"NewPosition",static_cast<float>(session_.clock().position()));position.run();
        }
        return;
    }
    PerformanceFields fields(post_.Get());
    fields.set_time(static_cast<float>(session_.clock().position()));
    fields.set_weight(static_cast<float>(session_.clock().weight()));
    fields.enable(session_.active());
    if(sample_face_) sample_face_(session_.clock().position(),session_.clock().weight());
}
void PerformancePlayer::release() {
    const bool was_native=native_montage_;
    if(native_montage_) {
        if(auto* animation=animation_.Get();animation && montage_.Get()) {
            Call stop(animation,L"Montage_Stop",2);
            stop.set(L"InBlendOutTime",0.f);stop.set(L"Montage",montage_.Get());stop.run();
        }
        montage_.Reset();
    }
    // Clear only our sequence. A replacement instance or a later owner is untouched.
    if(auto* post=post_.Get();post && sequence_.Get() && !was_native) {
        PerformanceFields fields(post);
        if(fields.clip()==sequence_.Get()) {
            fields.set_weight(0.f);
            fields.enable(false);fields.set_clip(nullptr);
            fields.set_time(original_time_);
        }
    }
    if(restore_face_) {restore_face_();restore_face_={};}
    sample_face_={};
    for(auto it=hidden_.begin();it!=hidden_.end();) {
        if(auto* object=it->object.Get();object && performance_flag(object,it->actor?L"bHidden":L"bHiddenInGame"))
            hide_game_object(object,false);
        it=hidden_.erase(it);
    }
    if(restore_layer_) {
        if(!restore_layer_()) {restore_retry_after_=GetTickCount64()+100;return;}
        restore_layer_={};restore_retry_after_=0;
    }
    retained_.release();
    native_montage_=false;montage_blending_out_=false;
    pawn_.Reset();component_.Reset();mesh_.Reset();post_.Reset();animation_.Reset();sequence_.Reset();
    character_=FName{};
}
void PerformancePlayer::interrupt(const std::string& reason) {
    session_.interrupt();reason_=reason;release();
}
Json PerformancePlayer::start(UObject* pawn,const Catalog& catalog,const std::string& outfit,
                              const std::string& id,double rate,double position,double facial_duration,
                              std::function<void(double,double)> sample_face,std::function<void()> restore_face,
                              PerformanceUse use) {
    if(session_.active()) throw std::runtime_error("Stop the current performance first");
    release();
    if(restore_layer_) throw std::runtime_error("Wait for the gameplay action to finish");
    if(const auto why=unavailable(pawn,catalog,outfit);!why.empty()) throw std::runtime_error(why);
    auto* component=read<UObject*>(pawn,L"Mesh");auto* mesh=mesh_asset(component);
    auto* skeleton=mesh?read<UObject*>(mesh,L"Skeleton"):nullptr;
    if(!skeleton) throw std::runtime_error("Player skeleton is unavailable");
    const auto target=narrow(skeleton->GetPathName());
    const auto match=resolve_library_performance(catalog.animation_libraries,id,target);
    if(!match.entry) throw std::runtime_error("Performance is not installed");
    if(match.candidates.empty()) throw std::runtime_error("Performance has no compatible skeleton");
    WeakObject held_pawn(pawn),held_component(component),held_mesh(mesh);
    auto* post=read<UObject*>(component,L"PostProcessAnimInstance");WeakObject held_post(post);
    auto* main=read<UObject*>(component,L"AnimScriptInstance");WeakObject held_main(main);
    const auto character=read<FName>(pawn,L"CharacterId");
    std::string failure;
    for(const auto& candidate:match.candidates) {
        try {
            AssetLoadRoots roots;
            auto* sequence=load(candidate.asset);roots.keep(sequence);
            if(held_pawn.Get()!=pawn || held_component.Get()!=component || held_mesh.Get()!=mesh || held_post.Get()!=post ||
               !main || held_main.Get()!=main || read<UObject*>(component,L"AnimScriptInstance")!=main ||
               read<UObject*>(pawn,L"Mesh")!=component || mesh_asset(component)!=mesh ||
               read<UObject*>(component,L"PostProcessAnimInstance")!=post ||
               read<FName>(pawn,L"CharacterId")!=character)
                throw std::runtime_error("Player changed while loading the performance");
            auto* type=static_cast<UClass*>(find(L"/Script/Engine.AnimSequence"));
            if(!sequence || !sequence->IsA(type) || read<uint8_t>(sequence,L"AdditiveAnimType")!=0 ||
               performance_flag(sequence,L"bEnableRootMotion"))
                throw std::runtime_error("Performance must be non-additive without root-motion extraction");
            auto* source=read<UObject*>(sequence,L"Skeleton");
            if(!source || narrow(source->GetPathName())!=candidate.declared_skeleton ||
               !compatible_animation_skeletons(candidate.declared_skeleton,target))
                throw std::runtime_error("Loaded performance skeleton differs from its declaration");
            if(const auto why=unavailable(pawn,catalog,outfit);!why.empty()) throw std::runtime_error(why);
            Call length(sequence,L"GetPlayLength",1);length.run();
            if(facial_duration && (!std::isfinite(facial_duration) ||
                std::abs(facial_duration-double(length.get<float>()))>1.e-4))
                throw std::runtime_error("Facial and body performance durations differ");
            PerformanceSession next=session_;
            next.begin(id,match.entry->playback,length.get<float>(),rate,position,use);
            const bool native=performance_native_component(component);
            if(native && (facial_duration || sample_face || restore_face))
                throw std::runtime_error("Native performance facial playback is not mapped");
            if(!native) {
                bind_layer(pawn,catalog,outfit);
                post=read<UObject*>(component,L"PostProcessAnimInstance");
                original_time_=PerformanceFields(post).seconds();
                if(!std::isfinite(original_time_))
                    throw std::runtime_error("Invalid performance layer defaults");
            }
            pawn_=pawn;component_=component;mesh_=mesh;post_=post;
            animation_=read<UObject*>(component,L"AnimScriptInstance");sequence_=sequence;character_=character;
            retained_.take(roots);session_=std::move(next);reason_.clear();
            sample_face_=std::move(sample_face);restore_face_=std::move(restore_face);
            native_montage_=native;
            if(native) bind_native_montage();
            else PerformanceFields(post).set_clip(sequence);
            if(match.entry->hide_weapons) {
                for(const auto& child_weak:attached_children(component)) {
                    auto* child=child_weak.Get();if(!child || !is_hand_weapon_socket(attach_socket(child))) continue;
                    Call owner(child,L"GetOwner",1);owner.run();
                    auto* object=owner.get<UObject*>();const bool actor=object && object!=pawn;
                    if(!actor) object=child;
                    if(performance_flag(object,actor?L"bHidden":L"bHiddenInGame")) continue;
                    hidden_.push_back({WeakObject(object),actor});hide_game_object(object,true);
                }
            }
            write_pose();return status();
        } catch(const std::exception& error) {
            failure=error.what();
            if(session_.active()) {interrupt(failure);throw;}
            if(restore_layer_) {release();throw;}
        }
    }
    throw std::runtime_error(failure.empty()?"Performance could not load":failure);
}
void PerformancePlayer::tick(UObject* pawn) {
    if(!session_.active()) {
        if(GetTickCount64()>=restore_retry_after_ &&
           (post_.Get() || native_montage_ || !hidden_.empty() || restore_layer_ || restore_face_)) release();
        return;
    }
    if(pawn!=pawn_.Get() || !intact()) {interrupt("Player or animation binding changed");return;}
    if(const auto why=performance_gameplay_block(pawn,montage_.Get());!why.empty()) {interrupt(why);return;}
    if(!native_montage_) {
        Call disabled(component_.Get(),L"GetDisablePostProcessBlueprint",1);disabled.run();
        if(disabled.get<bool>()) {interrupt("Post-process animation was disabled");return;}
    }
    // EngineTick's delta is not world time. The same dilation used by character
    // components must drive explicit animation sampling.
    Call delta(find(L"/Script/Engine.Default__GameplayStatics"),L"GetWorldDeltaSeconds",2);
    delta.set(L"WorldContextObject",pawn);delta.run();
    const double elapsed=delta.get<double>()*read<float>(pawn,L"CustomTimeDilation");
    session_.advance(elapsed);
    if(!session_.active()) {release();return;}
    write_pose();
}
Json PerformancePlayer::command(const Json& value) {
    const auto op=value.at("op").get<std::string>();
    if(op=="status") return status();
    const auto token=value.at("token").get<uint64_t>();
    if(op=="stop") session_.stop(token);
    else if(op=="seek") session_.seek(token,value.at("seconds").get<double>());
    else if(op=="rate") session_.rate(token,value.at("rate").get<double>());
    else if(op=="pause") session_.pause(token,value.at("paused").get<bool>());
    else throw std::runtime_error("Unknown performance command");
    if(!intact()) {interrupt("Performance binding changed");throw std::runtime_error(reason_);}
    write_pose();return status();
}
Json PerformancePlayer::status() const {
    const auto& clock=session_.clock();
    const char* phase="stopped";
    switch(clock.phase()) {
    case PerformancePhase::Playing:phase="playing";break;
    case PerformancePhase::Holding:phase="holding";break;
    case PerformancePhase::BlendingOut:phase="blending_out";break;
    case PerformancePhase::Stopped:break;
    }
    return {{"token",session_.token()},{"id",session_.clip()},{"phase",phase},
        {"active",session_.active()},{"seconds",clock.position()},{"duration",clock.duration()},
        {"rate",clock.rate()},{"weight",clock.weight()},{"paused",clock.paused()},{"reason",reason_}};
}

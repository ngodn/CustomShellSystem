// Included inside css after attachment_follower.inl.
// The player's ActiveBlendSpace/UseActiveBlendspace pair owns base locomotion.
// Built-in idle/walk borrow the Cultist Spear Lady's 85 cm/s cycle. Mod gaits
// use their authored direction/speed BlendSpaces without editing shared assets
// or changing movement speed. The lease restores the pair CSS replaced.
namespace {
constexpr const wchar_t* WALK_BLENDSPACE=L"/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Locomotion/BS_CultistSpearLady.BS_CultistSpearLady";
constexpr const wchar_t* WALK_SPEED_FUNCTION=L"/Script/Sparta.SpartaCharacterMovementComponent:SetMaxSpeedAdjustment";
// The game jogs at 540 and sprints at 800 (its own locomotion blendspace samples).
constexpr double WALK_IDLE_MAX=25., WALK_IDLE_OFF=70., WALK_SLIDE_MAX=130.;
constexpr int WALK_IDLE_SETTLE=3, WALK_OFF_DWELL=3;
constexpr uint64_t WALK_SLIDE_COOLDOWN=1500, WALK_MODS_RECHECK=5000;
template<typename T> void write_field(UObject* object,const wchar_t* name,const T& value) {
    auto* p=field(object,name,sizeof(T));
    std::memcpy(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal(),&value,sizeof(T));
}
bool has_field(UObject* object,const wchar_t* name,size_t size) {
    if(!object) return false;
    auto* p=object->GetPropertyByNameInChain(name);
    return p && p->GetElementSize()==static_cast<int32_t>(size) && p->GetArrayDim()==1;
}
}
bool WalkOverride::walk_mod_active(const fs::path& mods) {
    const auto now=GetTickCount64();
    if(!mods.empty() && mods!=mods_) { mods_=mods; mods_checked_=false; }
    if(mods_checked_ && now<mods_check_) return mod_active_;
    mods_checked_=true; mods_check_=now+WALK_MODS_RECHECK;
    mod_active_=false; mod_name_.clear(); genessa_active_=proxima_active_=false;
    std::error_code ec;
    if(mods_.empty()) return false;

    std::set<std::string> disabled_mods, enabled_mods;
    if(fs::is_regular_file(mods_/L"mods.txt",ec)) {
        if(std::ifstream in(mods_/L"mods.txt"); in.is_open()) {
            std::string line;
            while(std::getline(in,line)) {
                auto hash=line.find(';'); if(hash!=std::string::npos) line=line.substr(0,hash);
                auto colon=line.find(':');
                if(colon!=std::string::npos) {
                    auto name=line.substr(0,colon);
                    while(!name.empty() && (name.back()==' ' || name.back()=='\t' || name.back()=='\r')) name.pop_back();
                    while(!name.empty() && (name.front()==' ' || name.front()=='\t')) name.erase(name.begin());
                    for(char& c:name) c=char(std::tolower(static_cast<unsigned char>(c)));
                    auto val=line.substr(colon+1);
                    while(!val.empty() && (val.front()==' ' || val.front()=='\t')) val.erase(val.begin());
                    if(!val.empty() && val[0]=='0') disabled_mods.insert(name);
                    else if(!val.empty() && val[0]=='1') enabled_mods.insert(name);
                }
            }
        }
    }

    auto detect=[&](const wchar_t* wname,const char* sname) {
        std::string lower(sname);
        for(char& c:lower) c=char(std::tolower(static_cast<unsigned char>(c)));
        if(disabled_mods.contains(lower)) return false;
        if(enabled_mods.contains(lower)) return true;
        return fs::is_regular_file(mods_/wname/L"enabled.txt",ec);
    };

    genessa_active_ = detect(L"GenessaWalk","GenessaWalk");
    proxima_active_ = detect(L"ProximaWalk","ProximaWalk");
    if(genessa_active_ && proxima_active_) mod_name_ = "GenessaWalk & ProximaWalk";
    else if(genessa_active_) mod_name_ = "GenessaWalk";
    else if(proxima_active_) mod_name_ = "ProximaWalk";
    mod_active_ = !mod_name_.empty();
    return mod_active_;
}
UObject* WalkOverride::blendspace(WeakObject& slot,const wchar_t* path) {
    if(auto* found=slot.Get()) return found;
    auto* asset=UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path);
    if(!asset) asset=load(narrow(path));
    if(!asset || !asset->IsA(static_cast<UClass*>(find(L"/Script/Engine.BlendSpace")))) throw std::runtime_error("Locomotion blendspace is unavailable");
    slot=asset; return asset;
}
UObject* WalkOverride::custom_blendspace(size_t index,UObject* skeleton) {
    if(index>=custom_paths_.size() || custom_paths_[index].empty() || !skeleton)
        throw std::runtime_error("Custom movement option is incomplete");
    if(custom_skeleton_.Get()!=skeleton) {custom_blends_={};custom_skeleton_=skeleton;}
    if(auto* cached=custom_blends_[index].Get()) return cached;
    auto* asset=load(custom_paths_[index]);
    if(!asset || asset->GetClassPrivate()!=find(L"/Script/Engine.BlendSpace") ||
       read<UObject*>(asset,L"Skeleton")!=skeleton)
        throw std::runtime_error("Custom movement requires a 2D BlendSpace on this mesh's skeleton");
    auto bounded=[](FProperty* p,int32_t size) {
        return p && p->GetArrayDim()==1 && p->GetOffset_Internal()>=0 && p->GetElementSize()>0 &&
               p->GetOffset_Internal()<=size-p->GetElementSize();
    };
    auto* axes=asset->GetPropertyByNameInChain(L"BlendParameters");
    if(!axes || !axes->IsA<FStructProperty>() || axes->GetArrayDim()!=3 || axes->GetElementSize()<=0)
        throw std::runtime_error("Custom movement axes are unavailable");
    auto* axis_type=static_cast<FStructProperty*>(axes)->GetStruct().Get();
    auto* minimum=axis_type?axis_type->GetPropertyByNameInChain(L"Min"):nullptr;
    auto* maximum=axis_type?axis_type->GetPropertyByNameInChain(L"Max"):nullptr;
    if(!bounded(minimum,axes->GetElementSize()) || !bounded(maximum,axes->GetElementSize()) ||
       !minimum->IsA<FFloatProperty>() || !maximum->IsA<FFloatProperty>())
        throw std::runtime_error("Custom movement axis layout mismatch");
    for(int i=0;i<2;++i) {
        auto* base=reinterpret_cast<std::byte*>(asset)+axes->GetOffset_Internal()+i*axes->GetElementSize();
        float lo{},hi{};std::memcpy(&lo,base+minimum->GetOffset_Internal(),sizeof(lo));std::memcpy(&hi,base+maximum->GetOffset_Internal(),sizeof(hi));
        if(!std::isfinite(lo) || !std::isfinite(hi) || (i==0 && (lo!=-180.f || hi!=180.f)) || (i==1 && (lo!=0.f || hi<=0.f)))
            throw std::runtime_error("Custom movement expects X direction -180..180 and Y speed from zero");
    }
    auto* samples=asset->GetPropertyByNameInChain(L"SampleData");
    if(!samples || !samples->IsA<FArrayProperty>()) throw std::runtime_error("Custom movement sample array missing");
    auto* array=static_cast<FArrayProperty*>(samples);
    if(!array->GetInner()->IsA<FStructProperty>()) throw std::runtime_error("Custom movement samples are not structs");
    auto* type=static_cast<FStructProperty*>(array->GetInner())->GetStruct().Get();
    auto* animation=type?type->GetPropertyByNameInChain(L"Animation"):nullptr;
    auto* rate=type?type->GetPropertyByNameInChain(L"RateScale"):nullptr;
    const auto size=array->GetInner()->GetElementSize();
    if(!bounded(animation,size) || !animation->IsA<FObjectProperty>() || !bounded(rate,size) || !rate->IsA<FFloatProperty>())
        throw std::runtime_error("Custom movement sample layout mismatch");
    FScriptArrayHelper values(array,reinterpret_cast<std::byte*>(asset)+samples->GetOffset_Internal());
    if(values.Num()<1 || values.Num()>256) throw std::runtime_error("Custom movement sample count exceeds bound");
    for(int i=0;i<values.Num();++i) {
        auto* sample=values.GetRawPtr(i);
        if(!sample) throw std::runtime_error("Custom movement sample storage missing");
        auto* sequence=static_cast<FObjectProperty*>(animation)->GetObjectPropertyValue(sample+animation->GetOffset_Internal());
        float play_rate{};std::memcpy(&play_rate,sample+rate->GetOffset_Internal(),sizeof(play_rate));
        if(!sequence || !sequence->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimSequence"))) ||
           read<UObject*>(sequence,L"Skeleton")!=skeleton || read<uint8_t>(sequence,L"AdditiveAnimType")!=0 ||
           !std::isfinite(play_rate) || play_rate<=0)
            throw std::runtime_error("Custom movement has an incompatible, additive or invalid-rate sample");
        auto* root_motion=sequence->GetPropertyByNameInChain(L"bEnableRootMotion");
        if(!root_motion || !root_motion->IsA<FBoolProperty>() ||
           static_cast<FBoolProperty*>(root_motion)->GetPropertyValueInContainer(sequence))
            throw std::runtime_error("Custom movement samples must be in-place without root motion");
    }
    custom_blends_[index]=asset;return asset;
}
void WalkOverride::hook_speed() {
    if(hook_) return;
    auto* function=static_cast<UFunction*>(find(WALK_SPEED_FUNCTION));
    if(!function || !function->IsA<UFunction>()) throw std::runtime_error("Walk speed function is missing");
    FProperty* speed=nullptr;
    for(auto* p:function->ForEachProperty()) if(p->GetName()==L"Speed" && p->HasAnyPropertyFlags(CPF_Parm)) speed=p;
    if(!speed || speed->GetElementSize()!=sizeof(float) || speed->GetOffset_Internal()<0 ||
       speed->GetOffset_Internal()+int32_t(sizeof(float))>function->GetParmsSize())
        throw std::runtime_error("Walk speed parameter layout mismatch");
    const auto offset=speed->GetOffset_Internal();
    hook_=function->RegisterPreHook([this,offset](UnrealScriptFunctionCallableContext& context,void*) {
        if(!scale_walk_.load(std::memory_order_relaxed)) return;
        // The hook is global; only the observed player's movement component may
        // borrow this pace. Do not resolve weak engine objects on another thread.
        const bool game_thread=GetCurrentThreadId()==speed_thread_.load(std::memory_order_relaxed);
        if(!game_thread) return;
        const bool player_component=context.Context && context.Context==movement_.Get();
        if(!player_component) return;
        auto* locals=context.TheStack.Locals(); if(!locals) return;
        auto* value=reinterpret_cast<float*>(locals+offset);
        *value=feminine_walk_speed(*value,true,player_component,game_thread);
    });
    if(!*hook_) { hook_.reset(); throw std::runtime_error("Walk speed hook was refused"); }
}
void WalkOverride::unhook_speed() {
    scale_walk_=false;
    if(!hook_) return;
    auto* function=static_cast<UFunction*>(find(WALK_SPEED_FUNCTION));
    if(!function) throw std::runtime_error("Cannot remove walk speed hook: function unavailable");
    function->UnregisterHook(*hook_);
    hook_.reset();
}
void WalkOverride::push_on(UObject* target) {
    auto* anim=anim_.Get();
    if(!anim || !target) throw std::runtime_error("Walk animation targets vanished");
    // Resolve both properties and all weak handles before the first field write.
    field(anim,L"ActiveBlendSpace",sizeof(UObject*));field(anim,L"UseActiveBlendspace",sizeof(bool));
    const BlendLease::Value current{WeakObject(read<UObject*>(anim,L"ActiveBlendSpace")),read<bool>(anim,L"UseActiveBlendspace")};
    const BlendLease::Value next{WeakObject(target),true};
    if(!blend_lease_.owns(current)) {
        forget_blend_lease();
        if(auto* previous=current.object.Get();previous && !previous->IsRootSet()) {
            previous->SetRootSet();original_blend_root_owned_=true;
        }
    }
    blend_lease_.claim(current,next);
    write_field<UObject*>(anim,L"ActiveBlendSpace",target);
    write_field<bool>(anim,L"UseActiveBlendspace",true);
    active_=target; engaged_=true;
    if(!read<bool>(anim,L"UseActiveBlendspace") || read<UObject*>(anim,L"ActiveBlendSpace")!=target)
        throw std::runtime_error("Walk animation readback did not match");
}
void WalkOverride::forget_blend_lease() {
    if(original_blend_root_owned_ && blend_lease_.original())
        if(auto* original=blend_lease_.original()->object.Get()) original->ClearRootSet();
    original_blend_root_owned_=false;blend_lease_.reset();
}
void WalkOverride::push_off() {
    if(auto* anim=anim_.Get()) {
        const BlendLease::Value current{WeakObject(read<UObject*>(anim,L"ActiveBlendSpace")),read<bool>(anim,L"UseActiveBlendspace")};
        if(const auto before=blend_lease_.restoration(current)) {
            auto* original=before->object.Get();
            const bool enabled=before->enabled && original;
            field(anim,L"ActiveBlendSpace",sizeof(UObject*));field(anim,L"UseActiveBlendspace",sizeof(bool));
            write_field<UObject*>(anim,L"ActiveBlendSpace",original);
            write_field<bool>(anim,L"UseActiveBlendspace",enabled);
            if(read<UObject*>(anim,L"ActiveBlendSpace")!=original || read<bool>(anim,L"UseActiveBlendspace")!=enabled)
                throw std::runtime_error("Walk animation restoration did not match");
        }
    }
    forget_blend_lease();
    engaged_=false; active_.Reset(); reason_.clear();
}
static void hide_game_object(UObject* obj, bool hide) {
    if(!obj) return;
    try {
        auto* fn_act = obj->GetFunctionByNameInChain(L"SetActorHiddenInGame");
        if(fn_act && fn_act->GetNumParms() == 1) {
            Call set_act(obj, L"SetActorHiddenInGame", 1);
            set_act.set(L"bNewHidden", hide);
            set_act.run();
            return;
        }
    } catch(...) {}
    try {
        auto* fn_comp = obj->GetFunctionByNameInChain(L"SetHiddenInGame");
        if(fn_comp) {
            if(fn_comp->GetNumParms() == 2) {
                Call set_comp(obj, L"SetHiddenInGame", 2);
                set_comp.set(L"NewHidden", hide);
                set_comp.set(L"bPropagateToChildren", false);
                set_comp.run();
                return;
            } else if(fn_comp->GetNumParms() == 1) {
                Call set_comp(obj, L"SetHiddenInGame", 1);
                set_comp.set(L"NewHidden", hide);
                set_comp.run();
                return;
            }
        }
    } catch(...) {}
}
static bool is_hand_weapon_socket(const FName& socket_name) {
    std::string lower = narrow(socket_name.ToString());
    for(char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    if(lower.empty() || lower == "none" || lower == "root") return false;
    // Exclude stowed equipment, body sockets, accessories
    if(lower.find("stowed") != std::string::npos ||
       lower.find("crown") != std::string::npos ||
       lower.find("flower") != std::string::npos ||
       lower.find("head") != std::string::npos ||
       lower.find("spine") != std::string::npos ||
       lower.find("pelvis") != std::string::npos ||
       lower.find("foot") != std::string::npos ||
       lower.find("ball") != std::string::npos ||
       lower.find("thigh") != std::string::npos ||
       lower.find("calf") != std::string::npos ||
       lower.find("arm") != std::string::npos ||
       lower.find("clavicle") != std::string::npos ||
       lower.find("neck") != std::string::npos) return false;
    
    // Check if attached to hand or weapon or prop sockets
    if(lower.find("hand") != std::string::npos ||
       lower.find("weapon") != std::string::npos ||
       lower.find("prop") != std::string::npos ||
       lower.find("katana") != std::string::npos ||
       lower.find("axatana") != std::string::npos ||
       lower.find("dagger") != std::string::npos ||
       lower.find("shield") != std::string::npos ||
       lower.find("crossbow") != std::string::npos) {
        return true;
    }
    return false;
}
void WalkOverride::set_weapon_hidden(UObject* pawn,bool hide) {
    if(!pawn) return;
    try {
        auto* weapons=read<UObject*>(pawn,L"WeaponsComponent");
        if(hide) {
            if(weapons) {
                try {
                    Call get_in_hand(weapons,L"GetWeaponInHand",1);
                    get_in_hand.run();
                    if(auto* weapon=get_in_hand.get<UObject*>()) {
                        hide_game_object(weapon, true);
                        hidden_weapons_.emplace_back(weapon);
                        // Check if the weapon actor itself has attached offhand or sub-components
                        try {
                            Call get_root(weapon, L"K2_GetRootComponent", 1);
                            get_root.run();
                            if(auto* root = get_root.get<UObject*>()) {
                                for(auto& wchild : attached_children(root)) {
                                    if(auto* c = wchild.Get()) {
                                        hide_game_object(c, true);
                                        hidden_weapons_.emplace_back(c);
                                    }
                                }
                            }
                        } catch(...) {}
                    }
                } catch(...) {}
            }
            if(auto* mesh=read<UObject*>(pawn,L"Mesh")) {
                for(auto& child_weak:attached_children(mesh)) {
                    auto* child=child_weak.Get();
                    if(!child) continue;
                    FName socket=attach_socket(child);
                    if(!is_hand_weapon_socket(socket)) continue;
                    try {
                        Call owner_call(child,L"GetOwner",1); owner_call.run();
                        auto* owner=owner_call.get<UObject*>();
                        if(owner && owner!=pawn) {
                            hide_game_object(owner, true);
                            hidden_weapons_.emplace_back(owner);
                        }
                    } catch(...) {}
                    hide_game_object(child, true);
                    hidden_weapons_.emplace_back(child);
                }
            }
        } else {
            for(auto& weak:hidden_weapons_) {
                if(auto* obj=weak.Get()) {
                    hide_game_object(obj, false);
                }
            }
            hidden_weapons_.clear();
            if(weapons) {
                try {
                    Call get_in_hand(weapons,L"GetWeaponInHand",1);
                    get_in_hand.run();
                    if(auto* weapon=get_in_hand.get<UObject*>()) {
                        hide_game_object(weapon, false);
                    }
                } catch(...) {}
            }
        }
    } catch(...) {}
}
void WalkOverride::release() {
    scale_walk_=false;
    push_off();
    if(custom_idle_engaged_ || !hidden_weapons_.empty()) {
        try {
            if(auto* post=custom_idle_post_.Get()) {
                if(has_field(post,L"CSSIdleEnabled",sizeof(bool)))
                    write_field<bool>(post,L"CSSIdleEnabled",false);
            }
            if(pawn_.Get()) set_weapon_hidden(pawn_.Get(),false);
        } catch(...) {}
        custom_idle_engaged_=false;
        custom_idle_post_.Reset();
    }
    // Keep the hook handle on failure. Core stop must refuse unload while a
    // callback can still enter this DLL.
    unhook_speed();
    pawn_.Reset(); anim_.Reset(); movement_.Reset(); walk_bs_.Reset();
    custom_paths_={};custom_blends_={};custom_skeleton_.Reset();
    custom_idle_clip_.clear(); hide_weapons_=false;
    idle_ticks_=off_ticks_=slide_ticks_=0; slide_until_=0; last_heal_=0;
}
void WalkOverride::update(UObject* pawn,bool idle_feminine,bool walk_feminine,
    const std::array<std::string,3>& custom_paths,
    const std::string& custom_idle_clip,
    bool hide_weapons) {
    const auto now=GetTickCount64();
    speed_thread_=GetCurrentThreadId();
    const bool custom=std::any_of(custom_paths.begin(),custom_paths.end(),[](const auto& path){return !path.empty();});
    const bool has_custom_idle=!custom_idle_clip.empty();
    if(!idle_feminine && !walk_feminine && !custom && !has_custom_idle) {
        if(engaged_ || blend_lease_.engaged() || hook_ || custom_idle_engaged_) release();
        return;
    }
    if(!pawn) { release(); return; }
    if(pawn_.Get()!=pawn) {
        release();
        pawn_=pawn; anim_.Reset(); movement_.Reset();
    }
    if(custom_paths_!=custom_paths || custom_idle_clip_!=custom_idle_clip || hide_weapons_!=hide_weapons) {
        push_off();
        if(custom_idle_engaged_) {
            if(auto* post=custom_idle_post_.Get()) {
                if(has_field(post,L"CSSIdleEnabled",sizeof(bool)))
                    write_field<bool>(post,L"CSSIdleEnabled",false);
            }
            if(hide_weapons_) set_weapon_hidden(pawn,false);
            custom_idle_engaged_=false;
        }
        custom_blends_={};custom_skeleton_.Reset();
        custom_paths_=custom_paths;
        custom_idle_clip_=custom_idle_clip;
        hide_weapons_=hide_weapons;
    }
    auto* mesh=read<UObject*>(pawn,L"Mesh");
    if(!mesh) {release();return;}
    Call instance(mesh,L"GetAnimInstance",1);instance.run();
    auto* anim=instance.get<UObject*>();
    if(anim_.Get()!=anim) {
        push_off();
        if(!anim || !has_field(anim,L"UseActiveBlendspace",sizeof(bool)) || !has_field(anim,L"ActiveBlendSpace",sizeof(UObject*)))
            throw std::runtime_error("Player animation instance has no blendspace override");
        anim_=anim;
    }
    if(!anim) {release();return;}
    movement_=read<UObject*>(pawn,L"CharacterMovement");
    auto* controller=read<UObject*>(pawn,L"Controller");
    if(!controller || read<UObject*>(controller,L"Pawn")!=pawn || !movement_.Get()) {
        release();return;
    }
    // The graph's similarly named flag is false while standing. Query the
    // player's movement component so grounded idle can actually activate.
    Call grounded(movement_.Get(),L"IsMovingOnGround",1);grounded.run();
    if(!grounded.get<bool>()) {release();return;}
    for(const auto* name:{L"IsMoveInputIgnored",L"IsLookInputIgnored",L"IsInGameMenu"}) {
        Call blocked(controller,name,1);blocked.run();
        if(blocked.get<bool>()) {release();return;}
    }
    Call montage(anim,L"GetCurrentActiveMontage",1);montage.run();
    if(montage.get<UObject*>()) {release();return;}
    UObject* walk_bs=(walk_feminine || idle_feminine)?blendspace(walk_bs_,WALK_BLENDSPACE):nullptr;
    // Asset loads may collect objects. Revalidate ownership before dereferencing
    // any instance captured before the load.
    if(pawn_.Get()!=pawn || anim_.Get()!=anim) {release();return;}
    auto* current_mesh=read<UObject*>(pawn,L"Mesh");
    if(!current_mesh) {release();return;}
    Call current_instance(current_mesh,L"GetAnimInstance",1);current_instance.run();
    if(current_instance.get<UObject*>()!=anim) {release();return;}
    if(walk_feminine) { hook_speed(); scale_walk_=true; } else { scale_walk_=false; if(hook_) unhook_speed(); }
    auto* movement=movement_.Get();
    double speed=0; bool speed_known=false;
    if(movement) { auto v=read<std::array<double,3>>(movement,L"Velocity"); speed=std::hypot(v[0],v[1]); speed_known=std::isfinite(speed); }
    std::optional<bool> walking,sprint_requested;
    // These flags belong to this player's current linked layer. Never find the
    // first global walk ability, which could belong to another actor.
    if(auto* cls=UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,L"/Game/Sparta/Characters/Humans/Player/Animations/ABPL_Locomotion_MotionMatching.ABPL_Locomotion_MotionMatching_C")) {
        Call layer(anim,L"GetLinkedAnimLayerInstanceByClass",3);
        layer.set(L"InClass",cls);layer.set(L"bCheckForChildClass",false);layer.run();
        if(auto* linked=layer.get<UObject*>();linked && has_field(linked,L"IsWalking",sizeof(bool)) && has_field(linked,L"IsSprinting",sizeof(bool))) {
            walking=read<bool>(linked,L"IsWalking");sprint_requested=read<bool>(linked,L"IsSprinting");
        }
    }
    if(!speed_known) {release();return;}
    const auto gait=animation_gait(speed,walking,sprint_requested,custom);
    if(gait==AnimationGait::None) {release();return;}
    const bool walk_now=gait==AnimationGait::Walk;
    const bool sprinting=gait==AnimationGait::Sprint;
    const bool jogging=gait==AnimationGait::Jog;
    const double idle_cut=(engaged_ && reason_=="idle")?WALK_IDLE_OFF:WALK_IDLE_MAX;
    const bool standing=!walk_now && !jogging && !sprinting && speed_known && speed<idle_cut;
    idle_ticks_=standing?idle_ticks_+1:0;
    const bool settled=standing && (engaged_ || idle_ticks_>=WALK_IDLE_SETTLE);
    UObject* want=nullptr; bool hard_off=false; std::string reason;
    if(settled && has_custom_idle) {
        auto* post=read<UObject*>(current_mesh,L"PostProcessAnimInstance");
        if(!post) {
            Call get_post(current_mesh,L"GetPostProcessInstance",1);
            get_post.run();
            post=get_post.get<UObject*>();
        }
        if(post && has_field(post,L"CSSIdleEnabled",sizeof(bool)) && has_field(post,L"CSSIdleSequence",sizeof(UObject*))) {
            auto* clip=load(custom_idle_clip_);
            if(clip) {
                if(!custom_idle_engaged_ || custom_idle_post_.Get()!=post ||
                   read<UObject*>(post,L"CSSIdleSequence")!=clip || !read<bool>(post,L"CSSIdleEnabled")) {
                    write_field<UObject*>(post,L"CSSIdleSequence",clip);
                    write_field<bool>(post,L"CSSIdleEnabled",true);
                    custom_idle_engaged_=true;
                    custom_idle_post_=post;
                    if(hide_weapons_) set_weapon_hidden(pawn,true);
                }
                reason="custom idle";
                hard_off=true;
            }
        }
    }
    else if(!standing || !settled) {
        if(custom_idle_engaged_) {
            if(auto* post=custom_idle_post_.Get()) {
                if(has_field(post,L"CSSIdleEnabled",sizeof(bool)))
                    write_field<bool>(post,L"CSSIdleEnabled",false);
            }
            if(hide_weapons_) set_weapon_hidden(pawn,false);
            custom_idle_engaged_=false;
        }
    }
    std::optional<size_t> custom_index;
    if(sprinting && !custom_paths_[2].empty()) custom_index=2;
    else if(jogging && !custom_paths_[1].empty()) custom_index=1;
    else if(walk_now && !custom_paths_[0].empty()) custom_index=0;
    if(custom_index) {
        auto* body=mesh_asset(current_mesh);
        auto* skeleton=body?read<UObject*>(body,L"Skeleton"):nullptr;
        if(!skeleton) throw std::runtime_error("Custom movement needs a live mesh skeleton");
        want=custom_blendspace(*custom_index,skeleton);
        reason=std::array<const char*,3>{"custom walk","custom jog","custom sprint"}[*custom_index];
        if(pawn_.Get()!=pawn || anim_.Get()!=anim) {release();return;}
        auto* loaded_mesh=read<UObject*>(pawn,L"Mesh");
        if(!loaded_mesh || mesh_asset(loaded_mesh)!=body) {release();return;}
        Call loaded_instance(loaded_mesh,L"GetAnimInstance",1);loaded_instance.run();
        if(loaded_instance.get<UObject*>()!=anim) {release();return;}
    }
    else if(walk_now && walk_feminine && walk_bs) { want=walk_bs; reason="walk"; }
    else if(settled && !has_custom_idle && idle_feminine && walk_bs) { want=walk_bs; reason="idle"; }
    else if((standing && !idle_feminine) || (walk_now && !walk_feminine)) hard_off=true;
    // A custom gait must not linger after its category changes or becomes Default.
    if(!want && reason_.starts_with("custom ")) hard_off=true;
    // Slide guard: engaged for walking but still travelling at the stock speed means the
    // scaling has not landed; fall back until the next gait change re-issues the speed.
    if(want && reason=="walk" && speed_known && speed>WALK_SLIDE_MAX) {
        if(++slide_ticks_>=2) { want=nullptr; hard_off=true; slide_until_=now+WALK_SLIDE_COOLDOWN; }
    } else slide_ticks_=0;
    if(reason=="walk" && now<slide_until_) { want=nullptr; hard_off=true; }
    if(want) off_ticks_=0;
    else if(hard_off) off_ticks_=WALK_OFF_DWELL;
    else if(++off_ticks_<WALK_OFF_DWELL && engaged_ && active_.Get() && (!speed_known || speed<=WALK_SLIDE_MAX)) { want=active_.Get(); reason=reason_.empty()?"hold":reason_; }
    if(want) {
        // Re-assert on every mismatch, not on a timer: a walk mod ticking against the same
        // two fields must not silently replace the option the player selected.
        if(!engaged_ || active_.Get()!=want || !read<bool>(anim,L"UseActiveBlendspace") || read<UObject*>(anim,L"ActiveBlendSpace")!=want) { push_on(want); last_heal_=now; }
        reason_=reason;
    } else if(engaged_) push_off();

    // Footstep audio and VFX cadence pulse during custom locomotion override
    if(engaged_ && speed_known && speed >= 25.0) {
        if(now >= next_footstep_) {
            float interval = std::clamp(540.0f - (float(speed) * 0.35f), 260.0f, 520.0f);
            next_footstep_ = now + static_cast<uint64_t>(interval);
            foot_left_ = !foot_left_;
            try {
                if(pawn->GetFunctionByNameInChain(L"HandleFootDown")) {
                    Call step(pawn, L"HandleFootDown", 2);
                    step.set(L"FloorLineTraceLength", 150.0f);
                    auto* prop = step.param(L"FootstepData");
                    if(prop && prop->IsA<FStructProperty>()) {
                        auto* st = static_cast<FStructProperty*>(prop)->GetStruct().Get();
                        void* base = step.data(prop);
                        if(st) {
                            if(auto* tag_prop = st->GetPropertyByNameInChain(L"Tag")) {
                                FName tag(foot_left_ ? L"Foley.BoneLocation.Foot.Foot_L" : L"Foley.BoneLocation.Foot.Foot_R");
                                std::memcpy(static_cast<std::byte*>(base) + tag_prop->GetOffset_Internal(), &tag, sizeof(FName));
                            }
                            std::array<double, 3> foot_loc{};
                            bool has_loc = false;
                            if(auto* mesh = read<UObject*>(pawn, L"Mesh")) {
                                if(mesh->GetFunctionByNameInChain(L"GetSocketLocation")) {
                                    Call sock(mesh, L"GetSocketLocation", 2);
                                    sock.set(L"InSocketName", FName(foot_left_ ? L"ball_l" : L"ball_r"));
                                    sock.run();
                                    foot_loc = sock.get<std::array<double, 3>>(L"ReturnValue");
                                    has_loc = foot_loc[0] != 0.0 || foot_loc[1] != 0.0 || foot_loc[2] != 0.0;
                                }
                            }
                            if(!has_loc) {
                                Call loc_call(pawn, L"K2_GetActorLocation", 1);
                                loc_call.run();
                                foot_loc = loc_call.get<std::array<double, 3>>(L"ReturnValue");
                                foot_loc[2] -= 85.0;
                            }
                            if(auto* loc_prop = st->GetPropertyByNameInChain(L"Location")) {
                                std::memcpy(static_cast<std::byte*>(base) + loc_prop->GetOffset_Internal(), &foot_loc, sizeof(foot_loc));
                            }
                            if(auto* dist_prop = st->GetPropertyByNameInChain(L"LastFootDownDistance")) {
                                float dist = 50.0f;
                                std::memcpy(static_cast<std::byte*>(base) + dist_prop->GetOffset_Internal(), &dist, sizeof(float));
                            }
                        }
                    }
                    step.run();
                }
            } catch(...) {}
        }
    } else {
        next_footstep_ = now + 150;
    }
}

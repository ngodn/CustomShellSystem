void Appearance::release_menu_idle() {
    menu_idle_output_.release();
    if(auto* anim=menu_idle_anim_.Get(); anim && menu_idle_montage_.Get()) {
        Call stop(anim,L"Montage_Stop",2);
        stop.set(L"Montage",menu_idle_montage_.Get());stop.set(L"InBlendOutTime",0.f);stop.run();
    }
    menu_idle_montage_.Reset();
    menu_idle_clip_.Reset();
    restore_facial_binding(menu_idle_face_binding_);
    menu_idle_face_.reset();
    menu_idle_roots_.release();
}
void Appearance::stop_menu_idle() {
    if(!menu_idle_.requested()) return;
    menu_idle_.reset([this]{release_menu_idle();});
    menu_idle_component_.Reset();menu_idle_mesh_.Reset();
    menu_idle_anim_.Reset();menu_idle_post_.Reset();
}
void Appearance::sync_menu_idle(const Catalog& catalog,const InventoryIdleChoice& choice) {
    auto* component=menu_component_.Get();
    auto* mesh=menu_applied_.Get();
    if(choice.outfit.empty()) {
        auto* pawn=observed_pawn_.Get();
        auto* display=pawn ? menu_character(pawn) : nullptr;
        component=display && display!=pawn && read<FName>(display,L"CharacterId")==read<FName>(pawn,L"CharacterId")
            ? read<UObject*>(display,L"Mesh") : nullptr;
        auto* world=observed_component_.Get();
        mesh=world && world!=component ? mesh_asset(world) : nullptr;
    } else if(mesh!=applied_.Get()) mesh=nullptr;
    if(!component || !mesh || mesh_asset(component)!=mesh) { stop_menu_idle();return; }
    auto* anim=read<UObject*>(component,L"AnimScriptInstance");
    auto* post=read<UObject*>(component,L"PostProcessAnimInstance");
    bool same=menu_idle_component_.Get()==component && menu_idle_mesh_.Get()==mesh &&
              menu_idle_anim_.Get()==anim && menu_idle_post_.Get()==post;
    if(same && anim && menu_idle_.playing()) {
        Call active(anim,L"Montage_IsActive",2);
        active.set(L"Montage",menu_idle_montage_.Get());active.run();
        same=active.get<bool>();
        if(same) menu_idle_output_.bind(anim,IdleOutput::slot);
    }
    menu_idle_.sync(choice,same,GetTickCount64(),[this]{release_menu_idle();},[&]() -> double {
        menu_idle_component_=component;menu_idle_mesh_=mesh;menu_idle_anim_=anim;menu_idle_post_=post;
        return start_menu_idle(catalog,choice);
    });
}
double Appearance::start_menu_idle(const Catalog& catalog,const InventoryIdleChoice& choice) {
    auto* component=menu_idle_component_.Get();
    auto* mesh=menu_idle_mesh_.Get();
    auto* anim=menu_idle_anim_.Get();
    auto* post=menu_idle_post_.Get();
    if(!anim || !anim->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimInstance"))))
        throw std::runtime_error("Inventory display has no animation instance");
    const bool feminine=choice.idle==feminine_animation_id;
    const auto bundled=resolve_animation(catalog.animation_options(choice.outfit,choice.variant,AnimationSlot::Idle),
                                         AnimationSlot::Idle,choice.idle);
    const auto shared=resolve_shared_animation(catalog,choice.catalog_generation,choice.outfit,choice.variant,
                                               AnimationSlot::Idle,choice.idle);
    if(shared.matched && shared.asset.empty()) {
        if(!shared_animation_available(AnimationSlot::Idle,choice.idle).has_value()) return 0.;
        throw std::runtime_error(shared.reason);
    }
    std::string path=bundled.asset;
    if(shared.matched) path=shared.asset;
    if(feminine) path=vista_feminine_idle;
    if(path.empty()) throw std::runtime_error("Idle sequence is unavailable");
    UObject* clip;
    try {clip=load(path);} catch(const AssetNotReady&) {return 0.;}
    menu_idle_roots_.keep(clip);
    if(menu_idle_component_.Get()!=component || menu_idle_mesh_.Get()!=mesh || menu_idle_anim_.Get()!=anim ||
       menu_idle_post_.Get()!=post || mesh_asset(component)!=mesh || read<UObject*>(component,L"AnimScriptInstance")!=anim ||
       read<UObject*>(component,L"PostProcessAnimInstance")!=post) return 0.;
    auto* player=observed_pawn_.Get();
    if(!player) return 0.;
    auto* display=menu_character(player);
    if(!display || display==player || read<UObject*>(display,L"Mesh")!=component ||
       read<FName>(display,L"CharacterId")!=read<FName>(player,L"CharacterId")) return 0.;
    const bool idle_layer=post && has_field(post,L"CSSIdleSequence",sizeof(UObject*)) &&
                                 has_field(post,L"CSSIdleEnabled",sizeof(bool));
    if(!clip || !clip->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimSequence"))) ||
       read<uint8_t>(clip,L"AdditiveAnimType")!=0) throw std::runtime_error("Idle requires a full-body sequence");
    auto* source=read<UObject*>(clip,L"Skeleton");auto* target=read<UObject*>(mesh,L"Skeleton");
    require_inventory_idle_rig(feminine,idle_layer,!shared.idle_blend_space.empty(),
        source?narrow(source->GetPathName()):"",target?narrow(target->GetPathName()):"");
    Call length(clip,L"GetPlayLength",1);length.run();
    const double duration=length.get<float>();
    if(!std::isfinite(duration) || duration<=0.) throw std::runtime_error("Idle sequence has no playable frames");
    menu_idle_clip_=clip;
    const auto match=catalog.facial_match(choice.outfit,choice.variant);
    if(match.rig && match.adapter && !facial_weights_.empty() && narrow(mesh->GetPathName())==match.adapter->target_mesh &&
       (performance_adapter(component,catalog,choice.outfit) || performance_adapter(component,catalog,choice.outfit,true))) {
        const auto id=bundled.option && !bundled.option->library_identity.empty()?bundled.option->library_identity:
                      bundled.option?bundled.option->id:choice.idle;
        if(const auto* face=match.rig->find_performance(id); face && std::abs(face->duration-duration)<=1.e-4) {
            menu_idle_face_.emplace(*face,facial_weights_);
            menu_idle_face_binding_.component=component;menu_idle_face_binding_.mesh=mesh;
        }
    }
    auto* montage=create_positioned_montage(clip,0.f);
    menu_idle_roots_.keep(montage);menu_idle_montage_=montage;
    play_positioned_montage(anim,montage,0.f);
    menu_idle_output_.bind(anim,IdleOutput::slot);
    return duration;
}
void Appearance::tick_menu_idle(uint64_t now) {
    if(!menu_idle_.playing()) return;
    auto* component=menu_idle_component_.Get();auto* anim=menu_idle_anim_.Get();auto* montage=menu_idle_montage_.Get();
    if(!component || !anim || !montage || !menu_idle_mesh_.Get() ||
       mesh_asset(component)!=menu_idle_mesh_.Get() || read<UObject*>(component,L"AnimScriptInstance")!=anim) {
        stop_menu_idle();return;
    }
    const auto time=menu_idle_.position(now);
    Call seek(anim,L"Montage_SetPosition",2);
    seek.set(L"Montage",montage);seek.set(L"NewPosition",static_cast<float>(time));seek.run();
    if(menu_idle_face_) {
        menu_idle_face_binding_.weights.apply(menu_idle_face_->sample(time,1.),
            [&](const auto& name){return facial_weight(component,name);},
            [&](const auto& name,float value){facial_weight(component,name,value);},false);
    }
}
Json Appearance::inspect_menu_idle() const {
    auto* anim=menu_idle_anim_.Get();
    auto* montage=menu_idle_montage_.Get();
    auto* clip=menu_idle_clip_.Get();
    if(!anim || !montage || !clip) return {{"playing",false}};
    Call active(anim,L"Montage_IsActive",2);
    active.set(L"Montage",montage);active.run();
    Call position(anim,L"Montage_GetPosition",2);
    position.set(L"Montage",montage);position.run();
    Json result={{"playing",active.get<bool>()},{"direct_slot",menu_idle_output_.linked()},{"clip",narrow(clip->GetPathName())},
                 {"instance",narrow(anim->GetPathName())},{"seconds",position.get<float>()}};
    if(auto* component=menu_idle_face_binding_.component.Get(); component && menu_idle_face_ &&
       mesh_asset(component)==menu_idle_face_binding_.mesh.Get())
        for(const auto& [name,value]:facial_weights_) result["face_weights"][name]=facial_weight(component,name);
    return result;
}

// Development-only, synchronous hidden-component construction and cleanup.
Json EngineBridge::probe_astral_visual(void* engine,Appearance& appearance,const Json& request) {
    keep_alive_attach(engine);
    appearance.player(engine);
    const auto retained_before=keep_alive::entries.size();
    auto source=appearance.astral_source();
    if(!source || source->components.empty()) throw std::runtime_error("No settled Genessa appearance to copy");
    auto* parent=resolve(request.at("parent"));
    auto* pose_class=resolve(request.at("pose_class"));
    if(!parent || !parent->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"))))
        throw std::runtime_error("Visual probe requires a skeletal component parent");
    const WeakObject original_mesh(mesh_asset(parent)),original_instance(astral_anim_instance(parent));
    const auto original_visible=astral_source_flag(parent,L"bVisible");
    const auto original_hidden=astral_source_flag(parent,L"bHiddenInGame");
    const auto original_tick=read<uint8_t>(parent,L"VisibilityBasedAnimTickOption");
    const auto original_optimized=astral_source_flag(parent,L"bEnableUpdateRateOptimizations");
    Call collision(parent,L"GetCollisionEnabled",1);collision.run();
    const auto original_collision=collision.get<uint8_t>();
    auto parent_unchanged=[&] {
        Call get(parent,L"GetCollisionEnabled",1);get.run();
        return mesh_asset(parent)==original_mesh.Get() && astral_anim_instance(parent)==original_instance.Get() &&
            astral_source_flag(parent,L"bVisible")==original_visible &&
            astral_source_flag(parent,L"bHiddenInGame")==original_hidden &&
            read<uint8_t>(parent,L"VisibilityBasedAnimTickOption")==original_tick &&
            astral_source_flag(parent,L"bEnableUpdateRateOptimizations")==original_optimized &&
            get.get<uint8_t>()==original_collision;
    };
    AstralVisualMesh visual;
    visual.prepare(parent,source->components.front(),pose_class,AstralVisualTransform{});
    auto* component=visual.component();
    if(!component || !visual.intact() || astral_source_flag(component,L"bVisible") || !parent_unchanged())
        throw std::runtime_error("Visual probe changed its parent or exposed the component");
    visual.show(false);
    AstralNativeRenderLease native;
    native.acquire(parent);
    if(!native.intact() || !visual.intact())
        throw std::runtime_error("Visual probe native render lease failed");
    if(!native.restore() || native.intact() || !parent_unchanged() || !native.restore())
        throw std::runtime_error("Visual probe native render restoration failed");
    const auto morphs=source->components.front().morphs.size();
    const auto lods=source->components.front().hidden_by_lod.size();
    if(!visual.release() || visual.component() || visual.intact() || !parent_unchanged())
        throw std::runtime_error("Visual probe cleanup or parent preservation failed");
    if(!visual.release()) throw std::runtime_error("Visual probe repeat cleanup failed");
    // Form changes can destroy the native pose source before its lease is
    // restored. Use our own hidden component to exercise that ordering.
    AstralVisualMesh expiring_parent;
    expiring_parent.prepare(parent,source->components.front(),pose_class,AstralVisualTransform{});
    AstralNativeRenderLease expired_native;
    expired_native.acquire(expiring_parent.component());
    if(!expiring_parent.release() || !expired_native.restore() || !expired_native.restore() || !parent_unchanged())
        throw std::runtime_error("Visual probe destroyed-parent cleanup failed");
    source.reset();
    if(keep_alive::entries.size()!=retained_before)
        throw std::runtime_error("Visual probe retained objects did not return to baseline");
    return {{"passed",true},{"morphs",morphs},{"lods",lods},{"parent_preserved",true},{"native_render_restored",true},
        {"destroyed_parent_restored",true},
        {"retained_objects_restored",true},{"rendered",false},
        {"scope","Synchronous hidden visual construction and cleanup. No summon, animation-frame or rendering validation."}};
}

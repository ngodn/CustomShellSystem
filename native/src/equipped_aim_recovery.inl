// SwitchCharacterMesh saves only montage state. A game-owned mesh replacement
// can lose the equipped aim layer before CSS's own mesh-swap snapshot runs.
bool Appearance::repair_equipped_aim() {
    auto* pawn=observed_pawn_.Get();
    auto* component=observed_component_.Get();
    auto* controller=observed_controller_.Get();
    if(!pawn || !component || !controller || read<UObject*>(controller,L"Pawn")!=pawn ||
       read<UObject*>(pawn,L"Mesh")!=component) return false;
    Call animation(component,L"GetAnimInstance",1); animation.run();
    auto* instance=animation.get<UObject*>();
    if(!instance) return false;
    auto* default_class=static_cast<UClass*>(find(
        L"/Game/Sparta/Core/Animations/Layers/ABPL_Aim_Default.ABPL_Aim_Default_C"));
    Call current(component,L"GetLinkedAnimLayerInstanceByClass",2);
    current.set(L"InClass",default_class); current.run();
    auto* default_instance=current.get<UObject*>();
    if(!default_instance) return false; // A weapon or another system already owns aim.

    auto* weapons=read<UObject*>(pawn,L"WeaponsComponent");
    if(!weapons) return false;
    Call equipped(weapons,L"GetWeaponInSlot",2);
    equipped.set(L"WeaponSlot",FName(L"Weapon.Slot.Sidearm")); equipped.run();
    auto* weapon=equipped.get<UObject*>();
    if(!weapon) return false;
    if(aim_checked_instance_.Get()==instance && aim_checked_weapon_.Get()==weapon &&
       aim_checked_default_.Get()==default_instance) return false;
    if(!ready_to_apply()) return false;

    auto* property=optional_field(weapon,L"OnEquip_AnimationLayers");
    if(!property || !property->IsA<FArrayProperty>()) return false;
    auto* array=static_cast<FArrayProperty*>(property);
    auto* inner=array->GetInner();
    if(property->GetArrayDim()!=1 || !inner || !inner->IsA<FSoftClassProperty>())
        throw std::runtime_error("Equipped animation layer references have an unsupported layout");
    FScriptArrayHelper refs(array,reinterpret_cast<std::byte*>(weapon)+property->GetOffset_Internal());
    if(refs.Num()<0 || refs.Num()>16) throw std::runtime_error("Equipped animation layer list exceeds bound");
    auto* library=find(L"/Script/Engine.Default__KismetSystemLibrary");
    // Copy paths before any load can trigger GC or change the equipped actor.
    std::vector<std::string> paths;
    for(int i=0;i<refs.Num();++i) {
        Call path(library,L"Conv_SoftClassReferenceToString",2);
        original_copy(path,L"SoftClassReference",inner,refs.GetRawPtr(i)); path.run();
        auto value=original_string(path);
        if(!value.empty() && value!="None") paths.push_back(std::move(value));
    }
    WeakObject live_pawn(pawn), live_controller(controller), live_component(component),
        live_instance(instance), live_weapons(weapons), live_weapon(weapon);
    AssetLoadRoots roots; roots.keep(default_class);
    UClass* desired=nullptr;
    for(const auto& path:paths) {
        auto* loaded=load(path); roots.keep(loaded);
        if(!loaded || !loaded->IsA(static_cast<UClass*>(find(L"/Script/CoreUObject.Class"))))
            throw std::runtime_error("Equipped animation layer is not a class");
        auto* type=static_cast<UClass*>(loaded);
        // All shipped sidearm aim layers derive from this default. Never replay
        // equip events or restore unrelated traversal/locomotion layers.
        if(type!=default_class && type->IsChildOf(default_class)) {
            if(desired && desired!=type) throw std::runtime_error("Equipped sidearm has ambiguous aim layers");
            desired=type;
        }
    }
    if(live_pawn.Get()!=pawn || live_controller.Get()!=controller || live_component.Get()!=component ||
       live_instance.Get()!=instance || live_weapons.Get()!=weapons || live_weapon.Get()!=weapon ||
       observed_pawn_.Get()!=pawn || observed_component_.Get()!=component ||
       read<UObject*>(controller,L"Pawn")!=pawn || read<UObject*>(pawn,L"Mesh")!=component ||
       read<UObject*>(pawn,L"WeaponsComponent")!=weapons)
        return false;
    animation.run(); equipped.run(); current.run();
    if(animation.get<UObject*>()!=instance || equipped.get<UObject*>()!=weapon ||
       current.get<UObject*>()!=default_instance || !ready_to_apply()) return false;
    if(desired) {
        Call link(component,L"LinkAnimClassLayers",1); link.set(L"InClass",desired); link.run();
        Call readback(component,L"GetLinkedAnimLayerInstanceByClass",2);
        readback.set(L"InClass",desired); readback.run();
        current.run();
        if(!readback.get<UObject*>() || current.get<UObject*>())
            throw std::runtime_error("Equipped sidearm aim layer restoration failed read-back");
    }
    if(!desired) {
        aim_checked_instance_=instance; aim_checked_weapon_=weapon; aim_checked_default_=default_instance;
    }
    return desired!=nullptr;
}

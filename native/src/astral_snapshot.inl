// Player-owned spawner inspection. No world-wide actor scan.
namespace {
AstralIdentity astral_identity(UObject* object) {
    if(!object) return {};
    WeakObject weak(object);
    if(weak.Get()!=object) return {};
    return {weak.ObjectIndex,weak.ObjectSerialNumber};
}
UObject* astral_object(UObject* object,const wchar_t* name) {
    auto* property=field(object,name,sizeof(UObject*));
    if(!property->IsA<FObjectProperty>()) throw std::runtime_error("Astral object property mismatch");
    auto* value=static_cast<FObjectProperty*>(property)->GetObjectPropertyValue(
        reinterpret_cast<std::byte*>(object)+property->GetOffset_Internal());
    return astral_identity(value).valid()?value:nullptr;
}
bool astral_bool(UObject* object,const wchar_t* name) {
    auto* property=optional_field(object,name);
    if(!property || !property->IsA<FBoolProperty>() || property->GetArrayDim()!=1 || property->GetOffset_Internal()<0)
        throw std::runtime_error("Astral boolean property mismatch");
    return static_cast<FBoolProperty*>(property)->GetPropertyValue(
        reinterpret_cast<std::byte*>(object)+property->GetOffset_Internal());
}
UObject* astral_owner(UObject* object) {
    Call call(object,L"GetOwner",1); call.run();
    auto* owner=call.get<UObject*>();
    return astral_identity(owner).valid()?owner:nullptr;
}
UObject* astral_component(UObject* actor,UObject* type) {
    if(!type || !type->IsA<UClass>()) return nullptr;
    Call call(actor,L"GetComponentByClass",2); call.set(L"ComponentClass",type); call.run();
    auto* component=call.get<UObject*>();
    if(!astral_identity(component).valid() || !component->IsA(static_cast<UClass*>(type))) return nullptr;
    return astral_owner(component)==actor?component:nullptr;
}
UObject* astral_resolve(AstralIdentity identity) {
    if(!identity.valid()) return nullptr;
    FWeakObjectPtr weak;weak.ObjectIndex=identity.index;weak.ObjectSerialNumber=identity.serial;
    return weak.Get();
}
struct AstralSnapshot {
    AstralContext context;
    std::array<AstralObservation,AstralLifecycle::capacity> rows{};
    size_t count=0;
    const char* status="observed";
    std::span<const AstralObservation> view() const { return {rows.data(),count}; }
};
AstralSnapshot astral_snapshot(void* engine) {
    auto unavailable=[](const char* reason) { AstralSnapshot result;result.status=reason;return result; };
    if(!engine) return unavailable("no_engine");
    auto* viewport=astral_object(static_cast<UObject*>(engine),L"GameViewport");
    auto* world=viewport?astral_object(viewport,L"World"):nullptr;
    if(!world) return unavailable("no_world");
    Call player(find(L"/Script/Engine.Default__GameplayStatics"),L"GetPlayerCharacter",3);
    player.set(L"WorldContextObject",world); player.set(L"PlayerIndex",int32_t{0}); player.run();
    auto* pawn=player.get<UObject*>();
    if(!astral_identity(pawn).valid() || !optional_field(pawn,L"CharacterId") ||
       !astral_shell(narrow(read<FName>(pawn,L"CharacterId").ToString()))) return unavailable("not_genessa");
    auto* spawner_type=find_optional(L"/Game/Sparta/Core/Components/BPC_AstralAISpawner.BPC_AstralAISpawner_C");
    auto* astral_type=find_optional(L"/Game/Sparta/Core/AI/Components/BPC_AstralAI.BPC_AstralAI_C");
    if(!spawner_type || !astral_type) return unavailable("classes_not_loaded");
    auto* spawner=astral_component(pawn,spawner_type);
    if(!spawner || astral_object(spawner,L"OwnerSpartaCharacter")!=pawn) return unavailable("no_owned_spawner");
    auto* property=optional_field(spawner,L"AllCharacters");
    if(!property || !property->IsA<FArrayProperty>() || property->GetArrayDim()!=1 ||
       property->GetOffset_Internal()<0 || property->GetElementSize()!=sizeof(FScriptArray))
        throw std::runtime_error("Astral character array mismatch");
    auto* array_property=static_cast<FArrayProperty*>(property);
    auto* inner=array_property->GetInner();
    if(!inner || !inner->IsA<FObjectProperty>() || inner->GetArrayDim()!=1 || inner->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Astral character element mismatch");
    FScriptArrayHelper actors(array_property,reinterpret_cast<std::byte*>(spawner)+property->GetOffset_Internal());
    if(actors.Num()<0 || actors.Num()>int(AstralLifecycle::capacity))
        throw std::runtime_error("Astral character count exceeds bound");
    AstralSnapshot result;
    result.context={astral_identity(world),astral_identity(pawn),astral_identity(spawner)};
    for(int i=0;i<actors.Num();++i) {
        auto* actor=static_cast<FObjectProperty*>(inner)->GetObjectPropertyValue(actors.GetRawPtr(i));
        const auto identity=astral_identity(actor);
        if(!identity.valid() || actor==pawn || astral_owner(actor)!=pawn) continue;
        auto* component=astral_component(actor,astral_type);
        if(!component || astral_object(component,L"OwnerSpartaCharacter")!=actor) continue;
        auto* id_property=field(component,L"MyID",sizeof(FName));
        if(!id_property->IsA<FNameProperty>()) throw std::runtime_error("Astral ID property mismatch");
        const auto id=narrow(read<FName>(component,L"MyID").ToString());
        const auto kind=astral_kind(id);
        if(kind==AstralKind::unknown) continue;
        auto* mesh=astral_object(component,L"MySkeletalMesh");
        if(mesh && astral_owner(mesh)!=actor) continue;
        auto* mid=astral_object(component,L"MID_Astral");
        auto& row=result.rows[result.count++];
        row={identity,astral_identity(pawn),astral_identity(component),astral_identity(mesh),astral_identity(mid),kind,
            astral_bool(component,L"bInitialized"),astral_bool(component,L"bCharacterEnabled"),
            astral_bool(actor,L"bHidden"),astral_bool(component,L"bIsCached")};
    }
    return result;
}
}

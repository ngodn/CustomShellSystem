// Included by engine.cpp; also compiled by the asset-loading regression fixture.
static UObject* ready_asset(UObject* object,const std::string& path) {
    if(!object) return nullptr;
    if(object->IsUnreachable() || object->HasAnyFlags(
           static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))
        throw std::runtime_error("Asset is being released: "+path);
    // Same pending-load flags checked by UE 5.6.1 StaticLoadObjectInternal.
    // A soft reference can resolve an existing object before post-load completes.
    if(object->HasAnyFlags(static_cast<EObjectFlags>(
           RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WillBeLoaded)))
        throw AssetNotReady("Asset is still loading: "+path+" "+asset_evidence(object));
    return object;
}
static UObject* load(const std::string& path,bool required=true) {
    if (auto it = s_asset_cache.find(path); it != s_asset_cache.end())
        if (auto* cached = it->second.get()) return ready_asset(cached,path);
    auto name = wide(path);
    if (auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, name.c_str())) {
        ready_asset(object,path);
        s_asset_cache[path].capture(object);
        return object;
    }
    static UObject* s_kismet = nullptr;
    if (!s_kismet) s_kismet = find(L"/Script/Engine.Default__KismetSystemLibrary");
    Call make(s_kismet, L"MakeSoftObjectPath", 2);
    FString string(name.c_str()); make.set(L"PathString", string); make.run();
    Call convert(s_kismet, L"Conv_SoftObjPathToSoftObjRef", 2);
    convert.copy(L"SoftObjectPath", make, L"ReturnValue"); convert.run();
    Call loading(s_kismet, L"LoadAsset_Blocking", 2);
    loading.copy(L"Asset", convert, L"ReturnValue"); loading.run();
    auto* object = loading.get<UObject*>();
    if (!object) {
        if(required) throw std::runtime_error("Asset could not load: " + path);
        return nullptr;
    }
    ready_asset(object,path);
    s_asset_cache[path].capture(object);
    return object;
}

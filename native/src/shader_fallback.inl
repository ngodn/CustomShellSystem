// Included in engine.cpp after dynamic_material(). Rebuilds outfit materials that borrow game
// shaders as dynamic instances of their donor on a game build CSS has not verified. See
// shader_fallback.hpp for the why. Everything here runs on the game thread during an apply or a
// customize, never per frame, and does nothing at all while the fallback is inactive.
namespace {
struct ShaderFallbackOwned { WeakObject mid, source; };
struct ShaderFallbackGlobal {
    shader_fallback::Catalog catalog;
    shader_fallback::Mode mode=shader_fallback::Mode::automatic;
    bool loaded=false, active=false;
    std::string catalog_error, verified_build;
    shader_fallback::PakTables tables;
    // Every MID this feature created, with the outfit material it stands in for. Paths and
    // rollback read the source, so a fallback MID never looks like a temporary game effect.
    std::vector<ShaderFallbackOwned> owned;
    // Log lines for the core to write, and what was already reported, so each material is
    // described once per session instead of on every apply.
    std::vector<std::string> pending;
    std::set<std::string> reported;
    unsigned rebuilt=0, skipped=0;
};
ShaderFallbackGlobal& shader_fallback_state() { static ShaderFallbackGlobal state; return state; }

void shader_fallback_note(std::string key,std::string line) {
    auto& state=shader_fallback_state();
    if(state.reported.size()>=4096 || !state.reported.insert(std::move(key)).second) return;
    if(state.pending.size()<512) state.pending.push_back(std::move(line));
}

UObject* shader_fallback_source(UObject* value) {
    if(!value) return nullptr;
    for(const auto& entry:shader_fallback_state().owned)
        if(entry.mid.Get()==value) return entry.source.Get();
    return nullptr;
}

void shader_fallback_remember(UObject* mid,UObject* source) {
    auto& owned=shader_fallback_state().owned;
    std::erase_if(owned,[](const auto& entry){ return !entry.mid.Get() || !entry.source.Get(); });
    owned.push_back({WeakObject(mid),WeakObject(source)});
}

// The donor must compile to the same permutation as our material on this game build, not only
// on the build the catalog was made on. Compare the reflected fields that select it.
std::string shader_fallback_mismatch(UObject* source,UObject* donor) {
    auto* type=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstance"));
    if(!type || !source->IsA(type) || !donor->IsA(type)) return "not a material instance";
    static const wchar_t* const required[]={L"Parent",L"BasePropertyOverrides",L"StaticParametersRuntime",L"bHasStaticPermutationResource"};
    static const wchar_t* const optional[]={L"bOverrideSubsurfaceProfile",L"SubsurfaceProfile",L"SubsurfaceProfiles",
                                            L"bOverrideSpecularProfile",L"SpecularProfiles"};
    for(auto* name:required) {
        auto* p=type->GetPropertyByNameInChain(name);
        if(!p) return "material layout unknown ("+narrow(name)+")";
        if(!p->Identical_InContainer(source,donor)) return narrow(name)+" differs";
    }
    for(auto* name:optional)
        if(auto* p=type->GetPropertyByNameInChain(name); p && !p->Identical_InContainer(source,donor))
            return narrow(name)+" differs";
    return {};
}

// The listed material whose shaders `source` draws with: itself, or the nearest listed ancestor
// when every instance in between has no permutation (and so no shaders) of its own.
UObject* shader_fallback_anchor(UObject* source,const std::string*& donor_path) {
    auto& state=shader_fallback_state();
    auto* instance=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstance"));
    auto* permutation=instance?instance->GetPropertyByNameInChain(L"bHasStaticPermutationResource"):nullptr;
    if(!permutation || !permutation->IsA<FBoolProperty>()) return nullptr;
    auto* anchor=source;
    for(int depth=0;anchor && depth<8;++depth) {
        if(dynamic_material(anchor) || !anchor->IsA(instance)) return nullptr;
        if((donor_path=state.catalog.donor(narrow(anchor->GetPathName())))) return anchor;
        if(static_cast<FBoolProperty*>(permutation)->GetPropertyValueInContainer(anchor)) return nullptr;
        anchor=read<UObject*>(anchor,L"Parent");
    }
    return nullptr;
}

// Returns the new MID on the slot, or null when this material keeps its own shaders.
UObject* shader_fallback_mid(UObject* component,int slot,UObject* source) {
    auto& state=shader_fallback_state();
    if(!state.active || !component || !source || dynamic_material(source)) return nullptr;
    const std::string* donor_path=nullptr;
    auto* anchor=shader_fallback_anchor(source,donor_path);
    if(!anchor || !donor_path) return nullptr;
    const auto path=narrow(source->GetPathName());
    auto skip=[&](const std::string& why) -> UObject* {
        ++state.skipped;
        shader_fallback_note("skip:"+path,"Shader fallback skipped "+path+": "+why);
        return nullptr;
    };
    UObject* donor=nullptr;
    try { donor=load(*donor_path,false); }
    catch(const AssetNotReady&) { throw; }
    catch(const std::exception& error) { return skip("donor "+*donor_path+" could not load ("+error.what()+")"); }
    if(!donor) return skip("donor "+*donor_path+" is not in this game version");
    if(const auto why=shader_fallback_mismatch(anchor,donor); !why.empty())
        return skip("donor "+*donor_path+" is a different permutation on this game version: "+why);
    Call make(component,L"CreateDynamicMaterialInstance",4);
    make.set(L"ElementIndex",slot); make.set(L"SourceMaterial",donor); make.run();
    auto* mid=make.get<UObject*>();
    if(!mid || !dynamic_material(mid) || read<UObject*>(mid,L"Parent")!=donor) return skip("the engine did not create the replacement material");
    // Every scalar, vector and texture value along our material's whole parent chain, merged
    // the way the renderer sees them (UMaterialInstance::CopyMaterialUniformParametersInternal).
    Call copy(mid,L"K2_CopyMaterialInstanceParameters",2);
    copy.set(L"Source",source); copy.set(L"bQuickParametersOnly",true); copy.run();
    Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",slot); current.run();
    if(current.get<UObject*>()!=mid) return skip("the slot did not keep the replacement material");
    shader_fallback_remember(mid,source);
    ++state.rebuilt;
    shader_fallback_note("use:"+path,"Shader fallback: "+path+" now draws with the game's "+*donor_path);
    return mid;
}

// Item components belong to CSS and are destroyed with the look, so they need no rollback.
void shader_fallback_component(UObject* component) {
    if(!shader_fallback_state().active || !component) return;
    Call count(component,L"GetNumMaterials",1); count.run();
    const auto slots=count.get<int>();
    if(slots<0 || slots>256) throw std::runtime_error("Item material count exceeds the fallback limit");
    for(int slot=0;slot<slots;++slot) {
        Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",slot); current.run();
        if(auto* value=current.get<UObject*>(); value && !dynamic_material(value)) shader_fallback_mid(component,slot,value);
    }
}

std::string shader_fallback_steam_build(const fs::path& paks) {
    // Content/Paks -> MortalShell2 -> Sparta -> common -> steamapps
    std::error_code error;
    const auto manifest=paks.parent_path().parent_path().parent_path().parent_path().parent_path()/"appmanifest_2584270.acf";
    if(!fs::is_regular_file(manifest,error)) return {};
    std::ifstream input(manifest);
    std::string line;
    while(std::getline(input,line)) {
        const auto key=line.find("\"buildid\"");
        if(key==std::string::npos) continue;
        const auto open=line.find('"',key+9), close=open==std::string::npos?open:line.find('"',open+1);
        if(open==std::string::npos || close==std::string::npos || close-open>32) return {};
        return line.substr(open+1,close-open-1);
    }
    return {};
}
}

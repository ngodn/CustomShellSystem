#include "extension_controls.hpp"
#include "engine.hpp"
#include <windows.h>
#include "startup.hpp"
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>

namespace css {
using namespace RC::Unreal;
static std::string slider_text(float value,bool scalar) {
    if(!scalar) return std::to_string(int(std::lround(value*100)));
    std::ostringstream text; text<<std::fixed<<std::setprecision(2)<<value; return text.str();
}
static std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!size) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(size,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),size);
    return result;
}
static std::string narrow(const std::wstring& s) {
    if(s.empty()) return {};
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(!size) throw std::runtime_error("Invalid Unicode text");
    std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),size,nullptr,nullptr);
    return result;
}
static UObject* find(const wchar_t* path) {
    auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path);
    if (!object) throw std::runtime_error("Required reflected object is missing");
    return object;
}
static FProperty* field(UObject* object, const wchar_t* name, size_t size) {
    if (!object) throw std::runtime_error("No live object");
    auto* property = object->GetPropertyByNameInChain(name);
    if (!property || property->GetElementSize() != static_cast<int32_t>(size) || property->GetArrayDim() != 1)
        throw std::runtime_error("Reflected property layout does not match: "+narrow(name));
    return property;
}
template<typename T> static T read(UObject* object, const wchar_t* name) {
    auto* p = field(object, name, sizeof(T));
    T value{};
    std::memcpy(&value, reinterpret_cast<const std::byte*>(object) + p->GetOffset_Internal(), sizeof(T));
    return value;
}

// Parameter offsets and ownership come from live reflection. The frame never
// assumes the old UE4 FSoftObjectPath layout in the compatibility headers.
class Call {
    UObject* object_;
    UFunction* function_;
    alignas(16) std::array<std::byte, 2048> bytes_{};
    std::vector<FProperty*> properties_;
public:
    Call(UObject* object, const wchar_t* name, unsigned count) : object_(object) {
        if (!object) throw std::runtime_error("No target for reflected call");
        function_ = object->GetFunctionByNameInChain(name);
        if (!function_ || function_->GetNumParms() != count || function_->GetParmsSize() > bytes_.size())
            throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
        for (auto* p : function_->ForEachProperty()) {
            if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
            if (p->GetOffset_Internal() < 0 || p->GetArrayDim() != 1 ||
                p->GetOffset_Internal() + p->GetElementSize() > function_->GetParmsSize())
                throw std::runtime_error("Parameter exceeds reflected frame");
            properties_.push_back(p);
        }
        if (properties_.size() != count) throw std::runtime_error("Parameter enumeration mismatch");
        for (auto* p : properties_) p->InitializeValue(bytes_.data() + p->GetOffset_Internal());
    }
    ~Call() { for (auto* p : properties_) p->DestroyValue(bytes_.data() + p->GetOffset_Internal()); }
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;
    FProperty* param(const wchar_t* name) {
        for (auto* p : properties_) if (p->GetName() == name) return p;
        throw std::runtime_error("Missing parameter: " + narrow(name));
    }
    void* data(FProperty* p) { return bytes_.data() + p->GetOffset_Internal(); }
    template<typename T> void set(const wchar_t* name, const T& value) {
        auto* p = param(name);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Parameter size mismatch");
        p->CopyCompleteValue(data(p), &value);
    }
    void copy(const wchar_t* name, Call& other, const wchar_t* other_name) {
        auto* p = param(name); auto* q = other.param(other_name);
        if (p->GetElementSize() != q->GetElementSize() || !p->SameType(q))
            throw std::runtime_error("Parameter type mismatch");
        p->CopyCompleteValue(data(p), other.data(q));
    }
    template<typename T> T get(const wchar_t* name = L"ReturnValue") {
        auto* p = param(name);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Return size mismatch");
        T value{}; std::memcpy(&value, data(p), sizeof(T)); return value;
    }
    void run() { object_->ProcessEvent(function_, bytes_.data()); }
};
fs::path engine_content_directory() {
    auto* library=find(L"/Script/Engine.Default__BlueprintPathsLibrary");
    Call content(library,L"ProjectContentDir",1); content.run();
    Call full(library,L"ConvertRelativePathToFull",3);
    full.copy(L"InPath",content,L"ReturnValue"); full.set(L"InBasePath",FString(L"")); full.run();
    const auto& value=*static_cast<FString*>(full.data(full.param(L"ReturnValue")));
    const auto& chars=value.GetCharArray();
    if(chars.Num()<=1 || chars.Num()>32768 || chars[chars.Num()-1]!=0)
        throw std::runtime_error("Engine content path is invalid");
    return fs::path(std::wstring(chars.GetData(),chars.Num()-1)).lexically_normal();
}
WeakObject::WeakObject(UObject* object) { *this=object; }
WeakObject& WeakObject::operator=(UObject* object) {
    if(object) {
        auto* item=FUObjectArray::IndexToObject(object->GetInternalIndex());
        if(!item || item->GetUObject()!=object) throw std::runtime_error("Object is absent from the live object array");
        if(!item->GetSerialNumber()) {
            Call serial(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"Conv_ObjectToSoftObjectReference",2);
            serial.set(L"Object",object); serial.run();
            if(!item->GetSerialNumber()) throw std::runtime_error("Engine did not initialize the object serial");
        }
    }
    RC::Unreal::FWeakObjectPtr::operator=(object);
    return *this;
}
static UObject* load(const std::string& path) {
    auto name = wide(path);
    if (auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, name.c_str())) return object;
    auto* library = find(L"/Script/Engine.Default__KismetSystemLibrary");
    Call make(library, L"MakeSoftObjectPath", 2);
    FString string(name.c_str()); make.set(L"PathString", string); make.run();
    Call convert(library, L"Conv_SoftObjPathToSoftObjRef", 2);
    convert.copy(L"SoftObjectPath", make, L"ReturnValue"); convert.run();
    Call loading(library, L"LoadAsset_Blocking", 2);
    loading.copy(L"Asset", convert, L"ReturnValue"); loading.run();
    auto* object = loading.get<UObject*>();
    if (!object) throw std::runtime_error("Asset could not load: " + path);
    return object;
}
// Blocking asset loads can run garbage collection between successive imports.
// Keep only this operation's assets rooted until component references own them.
class AssetLoadRoots {
    std::vector<WeakObject> owned_;
public:
    void keep(UObject* object) {
        if(object->IsRootSet()) return;
        owned_.emplace_back(object);
        object->SetRootSet();
    }
    ~AssetLoadRoots() noexcept {
        for(auto& weak:owned_) if(auto* object=weak.Get()) object->ClearRootSet();
    }
};
static UObject* mesh_asset(UObject* component) {
    Call call(component, L"GetSkeletalMeshAsset", 1); call.run(); return call.get<UObject*>();
}
static void set_mesh(UObject* component, UObject* mesh) {
    Call call(component, L"SetSkeletalMeshAsset", 1); call.set(L"NewMesh", mesh); call.run();
    if (mesh_asset(component) != mesh) throw std::runtime_error("Mesh read-back did not confirm replacement");
}
static FScriptArrayHelper overrides(UObject* component) {
    auto* p=component->GetPropertyByNameInChain(L"OverrideMaterials");
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Missing material overrides");
    auto* a=static_cast<FArrayProperty*>(p);
    if(a->GetInner()->GetElementSize()!=sizeof(UObject*)) throw std::runtime_error("Material array layout mismatch");
    FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(component)+a->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>128) throw std::runtime_error("Invalid material override count");
    return values;
}
static bool dynamic_material(UObject* value) {
    return value && value->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInstanceDynamic")));
}
static bool material_has_overrides(UObject* value) {
    // Read the reflected arrays, including parameter kinds added by newer
    // builds. Only unwrap instances with no parameter or profile overrides.
    auto* type=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstance"));unsigned arrays=0;
    for(auto* p:type->ForEachProperty()) {
        const auto name=narrow(p->GetName());
        if(name.starts_with("bOverride") || name=="bHasStaticPermutationResource") {
            if(!p->IsA<FBoolProperty>()) throw std::runtime_error("Unknown material override flag layout");
            if(static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(value)) return true;
        }
        if(name=="BasePropertyOverrides" || name=="NaniteOverrideMaterial") {
            if(!p->IsA<FStructProperty>()) throw std::runtime_error("Unknown material override structure");
            // Nanite's enable flag defaults to true even with no material.
            // Compare complete reflected values, including soft references.
            auto* defaults=find(L"/Script/Engine.Default__MaterialInstanceDynamic");
            if(!p->Identical_InContainer(value,defaults)) return true;
        }
        if(!name.ends_with("ParameterValues") && name!="UserSceneTextureOverrides") continue;
        if(!p->IsA<FArrayProperty>() || p->GetArrayDim()!=1) throw std::runtime_error("Unknown material parameter layout");
        auto* a=static_cast<FArrayProperty*>(p);FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(value)+p->GetOffset_Internal());
        if(values.Num()<0 || values.Num()>4096) throw std::runtime_error("Material parameter count exceeds bounds");
        ++arrays;if(values.Num()) return true;
    }
    if(arrays<3) throw std::runtime_error("Material parameter metadata is unavailable");
    return false;
}
static UObject* material_asset(UObject* value) {
    std::set<UObject*> seen;
    while(dynamic_material(value)) {
        if(seen.size()>=16 || !seen.insert(value).second) throw std::runtime_error("Invalid dynamic material parent chain");
        if(material_has_overrides(value)) throw std::runtime_error("A material effect still owns parameter overrides. Let it finish before changing appearance.");
        value=read<UObject*>(value,L"Parent");
        if(!value) throw std::runtime_error("Dynamic material has no asset parent");
    }
    return value;
}
static std::vector<WeakObject> material_objects(UObject* component) {
    auto values=overrides(component);std::vector<WeakObject> result;
    for(int i=0;i<values.Num();++i) {UObject* value{};std::memcpy(&value,values.GetRawPtr(i),sizeof(value));result.emplace_back(value);}
    return result;
}
static std::vector<std::string> material_paths(UObject* component) {
    auto values=overrides(component);
    std::vector<std::string> result;
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        auto* asset=material_asset(value);
        auto path=asset?narrow(asset->GetPathName()):std::string{};
        // Keep a stable fallback for collected MIDs. Live rollback uses weak
        // references and never roots a material that belongs to an old world.
        if(value && (path.find(':')!=std::string::npos || path.starts_with("/Engine/Transient") || (!path.starts_with("/Game/") && !path.starts_with("/Engine/"))))
            throw std::runtime_error("A temporary material effect is active. Let it finish before changing appearance.");
        result.push_back(std::move(path));
    }
    return result;
}
static void material(UObject* component,int index,UObject* value) {
    Call set(component,L"SetMaterial",2); set.set(L"ElementIndex",index); set.set(L"Material",value); set.run();
}
static void restore_materials(UObject* component,const std::vector<std::string>& paths,const std::vector<WeakObject>& previous={}) {
    WeakObject live(component);
    std::vector<WeakObject> loaded;
    AssetLoadRoots roots;
    for(size_t i=0;i<paths.size();++i) {
        auto* value=i<previous.size()?previous[i].Get():nullptr;
        if(!value && !paths[i].empty()) {value=load(paths[i]);roots.keep(value);}
        loaded.emplace_back(value);
    }
    component=live.Get();
    if(!component) return; // Its world was released while assets loaded.
    for(size_t i=0;i<paths.size();++i)
        if(!paths[i].empty() && !loaded[i].Get()) throw std::runtime_error("Restoration material expired while loading");
    auto count=std::max(static_cast<int>(paths.size()),overrides(component).Num());
    for(int i=0;i<count;++i) material(component,i,i<static_cast<int>(loaded.size())?loaded[i].Get():nullptr);
    auto actual=material_objects(component);
    for(size_t i=0;i<std::max(actual.size(),loaded.size());++i)
        if((i<actual.size()?actual[i].Get():nullptr)!=(i<loaded.size()?loaded[i].Get():nullptr))
            throw std::runtime_error("Original material read-back failed");
}
static Json material_snapshot(UObject* component,UObject* mesh) {
    Json result={{"overrides",Json::array()},{"defaults",Json::array()},{"effective",Json::array()}};
    auto* overrides=component->GetPropertyByNameInChain(L"OverrideMaterials");
    if(!overrides || !overrides->IsA<FArrayProperty>()) throw std::runtime_error("Missing material array");
    auto* property=static_cast<FArrayProperty*>(overrides);
    if(property->GetInner()->GetElementSize()!=sizeof(UObject*)) throw std::runtime_error("Material array layout mismatch");
    FScriptArrayHelper array(property,reinterpret_cast<std::byte*>(component)+property->GetOffset_Internal());
    if(array.Num()<0 || array.Num()>512) throw std::runtime_error("Invalid material count");
    for(int i=0;i<array.Num();++i) {
        UObject* value{}; std::memcpy(&value,array.GetRawPtr(i),sizeof(value));
        result["overrides"].push_back(value?narrow(value->GetFullName()):"null");
    }
    Call defaults(mesh,L"GetMaterials",1); defaults.run();
    auto* p=defaults.param(L"ReturnValue");
    if(!p->IsA<FArrayProperty>()) throw std::runtime_error("Default materials are not an array");
    auto* a=static_cast<FArrayProperty*>(p);
    auto* slot=field(find(L"/Script/Engine.SkeletalMaterial"),L"MaterialInterface",sizeof(UObject*));
    if(slot->GetOffset_Internal()+sizeof(UObject*)>static_cast<size_t>(a->GetInner()->GetElementSize())) throw std::runtime_error("Material slot layout mismatch");
    FScriptArrayHelper values(a,defaults.data(p));
    if(values.Num()<0 || values.Num()>128) throw std::runtime_error("Invalid default material count");
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i)+slot->GetOffset_Internal(),sizeof(value));
        result["defaults"].push_back(value?narrow(value->GetFullName()):"null");
        Call actual(component,L"GetMaterial",2); actual.set(L"ElementIndex",i); actual.run();
        auto* current=actual.get<UObject*>();
        result["effective"].push_back(current?narrow(current->GetFullName()):"null");
    }
    return result;
}
static UObject* menu_character(UObject* player) {
    if(!player) return nullptr;
    auto* pc=read<UObject*>(player,L"Controller");
    if(!pc || !pc->GetPropertyByNameInChain(L"User Interface Handler Component")) return nullptr;
    auto* handler=read<UObject*>(pc,L"User Interface Handler Component");
    if(!handler) return nullptr;
    auto* menu=read<UObject*>(handler,L"ActiveDisplayMenu");
    if(!menu) return nullptr;
    Call character(menu,L"GetDisplayMenuCharacter",1); character.run();
    return character.get<UObject*>();
}
UObject* Appearance::player(void* engine) {
    shell.clear(); pawn_name.clear(); current_mesh.clear();
    if (!Version::IsAtLeast(5, 6) || !Version::IsBelow(5, 7)) throw std::runtime_error("CSS adapter requires UE5.6");
    auto* viewport = read<UObject*>(static_cast<UObject*>(engine), L"GameViewport");
    if (!viewport) return nullptr;
    auto* world = read<UObject*>(viewport, L"World");
    if (!world) return nullptr;
    Call player_call(find(L"/Script/Engine.Default__GameplayStatics"), L"GetPlayerCharacter", 3);
    player_call.set(L"WorldContextObject", world); player_call.set(L"PlayerIndex", int32_t{0}); player_call.run();
    auto* pawn = player_call.get<UObject*>();
    if (!pawn || WeakObject(pawn).Get()!=pawn || !pawn->GetPropertyByNameInChain(L"CharacterId")) return nullptr;
    // FGameplayTag contains the reflected FName TagName (8 bytes in this build).
    shell = narrow(read<FName>(pawn, L"CharacterId").ToString());
    if (!shell.starts_with("CharacterId.Player.")) { shell.clear(); return nullptr; }
    pawn_name = narrow(pawn->GetFullName());
    auto* component = read<UObject*>(pawn, L"Mesh");
    auto* controller=read<UObject*>(pawn,L"Controller");
    if(observed_pawn_.Get()!=pawn || observed_component_.Get()!=component || observed_controller_.Get()!=controller) {
        ++player_revision; observed_pawn_=pawn; observed_component_=component; observed_controller_=controller;
    }
    if (component) {
        if(component==component_.Get()) detach_residual_colors();
        if(auto* mesh = mesh_asset(component)) current_mesh = narrow(mesh->GetPathName());
    }
    return pawn;
}
void Appearance::restore_menu() {
    auto component=menu_component_, applied=menu_applied_;
    auto original=std::exchange(menu_original_,{});
    auto materials=std::exchange(menu_original_materials_,{});
    auto live_materials=std::exchange(menu_original_live_materials_,{});
    menu_component_.Reset(); menu_applied_.Reset();
    if(auto* target=component.Get(); target && applied.Get() && mesh_asset(target)==applied.Get() && !original.empty()) {
        auto* mesh=load(original);
        target=component.Get();
        if(!target || mesh_asset(target)!=applied.Get()) return;
        set_mesh(target,mesh); restore_materials(target,materials,live_materials);
    }
}
void Appearance::sync_menu() {
    auto* source=component_.Get();
    if(!source || source!=observed_component_.Get() || mesh_asset(source)!=applied_.Get()) { restore_menu(); return; }
    Call owner(source,L"GetOwner",1); owner.run();
    auto* player=owner.get<UObject*>();
    auto* display=menu_character(player);
    // Other shell-selection previews belong to the game until that shell is worn.
    if(!display || display==player || read<FName>(display,L"CharacterId")!=read<FName>(player,L"CharacterId")) { restore_menu(); return; }
    auto* target=read<UObject*>(display,L"Mesh");
    if(!target) { restore_menu(); return; }
    auto* before=mesh_asset(target); auto* desired=applied_.Get();
    if(!before || !desired || read<UObject*>(before,L"Skeleton")!=read<UObject*>(desired,L"Skeleton")) { restore_menu(); return; }
    if(menu_component_.Get()!=target || before!=menu_applied_.Get()) {
        WeakObject live_target(target), live_before(before), live_desired(desired);
        restore_menu();
        if(live_target.Get()!=target || live_before.Get()!=before || live_desired.Get()!=desired || mesh_asset(target)!=before) return;
        menu_original_=before==desired?original_:narrow(before->GetPathName());
        menu_original_materials_=before==desired?original_materials_:material_paths(target);
        menu_original_live_materials_=before==desired?original_live_materials_:material_objects(target);
        menu_component_=target;
    }
    menu_applied_=desired;
    if(before!=desired) set_mesh(target,desired);
    auto values=overrides(source), previous=overrides(target);
    for(int i=0;i<std::max(values.Num(),previous.Num());++i) {
        UObject* value{}; UObject* current{};
        if(i<values.Num()) std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        if(i<previous.Num()) std::memcpy(&current,previous.GetRawPtr(i),sizeof(current));
        if(current!=value) material(target,i,value);
    }
}
void Appearance::remember_materials() {
    expected_materials_.clear();
    auto values=overrides(component_.Get());
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        expected_materials_.emplace_back(value);
    }
}
void Appearance::detach_residual_colors() {
    if(color_mids_.empty()) return;
    auto* component=component_.Get();
    if(!component || !applied_.Get() || mesh_asset(component)==applied_.Get()) return;
    // Changing gameplay shells can leave trailing OverrideMaterials entries
    // from the previous, larger mesh. Remove only our exact MID objects, never
    // game-created effects or another mod's replacement. Keep the weak cache so
    // a temporary stock-mesh reset can still reuse live dye resources.
    auto values=overrides(component);
    for(const auto& [slot,weak]:color_mids_) {
        if(slot<0 || slot>=values.Num()) continue;
        UObject* actual{}; std::memcpy(&actual,values.GetRawPtr(slot),sizeof(actual));
        if(auto* owned=weak.Get();owned && actual==owned) material(component,slot,nullptr);
    }
}
bool Appearance::materials_match() const {
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=applied_.Get()) return false;
    auto values=overrides(component);
    for(size_t i=0;i<std::max(static_cast<size_t>(values.Num()),expected_materials_.size());++i) {
        UObject* actual{}; if(i<static_cast<size_t>(values.Num())) std::memcpy(&actual,values.GetRawPtr(static_cast<int>(i)),sizeof(actual));
        auto* expected=i<expected_materials_.size()?expected_materials_[i].Get():nullptr;
        if(i<expected_materials_.size() && expected_materials_[i].ObjectSerialNumber && !expected) return false;
        if(actual!=expected) return false;
    }
    return true;
}
bool Appearance::repair_materials_needed() {
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=applied_.Get() || materials_match()) { return false; }
    // A completed shell effect restores stock asset materials. Repair that
    // specific transition at the end of this frame, leaving active MIDs and
    // unfamiliar effect materials under the game's control.
    auto values=overrides(component);
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        auto* expected=i<static_cast<int>(expected_materials_.size())?expected_materials_[i].Get():nullptr;
        if(value && value!=expected) {
            UObject* asset{};
            try {asset=material_asset(value);} catch(const std::runtime_error&) {return false;}
            auto path=narrow(asset->GetPathName());
            if(!original_default_materials_.contains(path) && std::find(original_materials_.begin(),original_materials_.end(),path)==original_materials_.end()) return false;
        }
    }
    return true;
}
bool Appearance::reuse_materials() {
    // A completed effect can replace OverrideMaterials without changing the
    // outfit. Reattach the existing MIDs and their dye targets instead of
    // importing masks and rendering every color surface again on the game thread.
    // Weak references never retain a previous world; collected resources take
    // the normal rebuild path. Check every reference before changing any slot.
    if(!repair_materials_needed()) return false;
    for(const auto& weak:expected_materials_)
        if(weak.ObjectSerialNumber && !weak.Get()) return false;
    for(const auto& [slot,weak]:color_mids_) {
        if(!weak.Get() || slot<0 || static_cast<size_t>(slot)>=expected_materials_.size() ||
           weak.Get()!=expected_materials_[slot].Get()) return false;
    }
    for(const auto& [id,weak]:color_targets_) if(!weak.Get()) return false;
    auto* component=component_.Get();
    const auto count=std::max(static_cast<size_t>(overrides(component).Num()),expected_materials_.size());
    for(size_t i=0;i<count;++i)
        material(component,static_cast<int>(i),i<expected_materials_.size()?expected_materials_[i].Get():nullptr);
    if(!materials_match()) throw std::runtime_error("Recovered color material read-back failed");
    material_debug=material_snapshot(component,applied_.Get());
    material_debug["colors"]=last_colors_;
    material_debug["dye_targets"]=color_targets_.size();
    return true;
}
Json Appearance::transition_state(void* engine) {
    auto* pawn=player(engine);
    Json result={{"player_ready",pawn!=nullptr},{"mesh",current_mesh},{"shell",shell}};
    if(!pawn) return result;
    auto* pc=read<UObject*>(pawn,L"Controller");
    result["controller_ready"]=pc && read<UObject*>(pc,L"Pawn")==pawn;
    if(!pc) return result;
    result["controller"]=narrow(pc->GetPathName());
    for(auto name:{L"IsMoveInputIgnored",L"IsLookInputIgnored",L"IsInGameMenu"}) {
        Call call(pc,name,1); call.run(); result[narrow(name)]=call.get<bool>();
    }
    auto* handler=read<UObject*>(pc,L"User Interface Handler Component");
    if(handler) {
        result["ui_pause_count"]=read<int32_t>(handler,L"PauseGameCounter");
        for(auto name:{L"ActiveMenu",L"ActiveSubMenu",L"ActiveDisplayMenu",L"CurrentTransitionWidget"}) {
            auto* value=read<UObject*>(handler,name);
            result[narrow(name)]=value?Json(narrow(value->GetPathName())):Json(nullptr);
        }
    }
    return result;
}
#ifdef CSS_TRANSITION_TESTS
void Appearance::test_cursor(void* engine,bool visible) {
    auto* pawn=player(engine); if(!pawn) throw std::runtime_error("No test player");
    auto* pc=read<UObject*>(pawn,L"Controller");
    auto* p=pc->GetPropertyByNameInChain(L"bShowMouseCursor");
    if(!p || !p->IsA<FBoolProperty>()) throw std::runtime_error("Cursor test layout mismatch");
    static_cast<FBoolProperty*>(p)->SetPropertyValueInContainer(pc,visible);
}
void Appearance::test_effect(bool begin,bool parameters) {
    if(begin) test_reset_mesh();
    auto* component=component_.Get();
    if(!component || narrow(mesh_asset(component)->GetPathName())!=original_) throw std::runtime_error("Test effect requires original mesh");
    if(begin) {
        Call effect(component,L"CreateDynamicMaterialInstance",4);
        effect.set(L"ElementIndex",int32_t{0}); effect.run();
        auto* mid=effect.get<UObject*>();
        if(!mid) throw std::runtime_error("Test material creation failed");
        if(parameters) {
            Call set(mid,L"SetScalarParameterValue",2);set.set(L"ParameterName",FName(L"CSS_TransitionProbe",FNAME_Add));set.set(L"Value",.37f);set.run();
        }
    } else restore_materials(component,original_materials_);
}
void Appearance::test_reset_mesh() {
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=applied_.Get()) throw std::runtime_error("Test requires an applied outfit");
    auto* original=load(original_);
    set_mesh(component,original);
    restore_materials(component,original_materials_);
}
#endif
bool Appearance::repair_mesh_needed() const {
    auto* component=component_.Get();
    if(!component || component!=observed_component_.Get()) return false;
    auto* mesh=mesh_asset(component);
    // Only reclaim the stock mesh captured for this component. An unfamiliar
    // replacement can belong to another mod or an unfinished transformation.
    return mesh && mesh!=applied_.Get() && narrow(mesh->GetPathName())==original_;
}
bool Appearance::ready_to_apply() const {
    auto* pawn=observed_pawn_.Get(); auto* component=observed_component_.Get(); auto* pc=observed_controller_.Get();
    if(!pawn || !component || !pc || read<UObject*>(pc,L"Pawn")!=pawn || !mesh_asset(component)) return false;
    Call move(pc,L"IsMoveInputIgnored",1); move.run();
    Call look(pc,L"IsLookInputIgnored",1); look.run();
    if(move.get<bool>() || look.get<bool>()) return false;
    auto* handler=read<UObject*>(pc,L"User Interface Handler Component");
    if(!handler) return false;
    auto* transition=read<UObject*>(handler,L"CurrentTransitionWidget");
    if(transition) { Call shown(transition,L"IsInViewport",1); shown.run(); if(shown.get<bool>()) return false; }
    Call animation(component,L"GetAnimInstance",1); animation.run();
    if(auto* anim=animation.get<UObject*>()) {
        Call montage(anim,L"GetCurrentActiveMontage",1); montage.run();
        if(montage.get<UObject*>()) return false;
    }
    // Retained player/controller changes can keep our own color MIDs.
    if(component==component_.Get() && mesh_asset(component)==applied_.Get()) return materials_match();
    // World-owned dynamic effects have no stable asset path for rollback.
    try { material_paths(component); } catch(const std::runtime_error&) { return false; }
    return true;
}
bool Appearance::active() const { auto* c=component_.Get(); return c && c==observed_component_.Get() && applied_.Get() && mesh_asset(c)==applied_.Get(); }
bool Appearance::apply(void* engine, const std::string& mesh_path, const std::map<int,std::string>& materials) {
    auto* pawn = player(engine);
    if (!pawn) return false;
    auto* component = read<UObject*>(pawn, L"Mesh");
    if (!component) return false;
    auto* before = mesh_asset(component);
    if (!before) return false;
    if (component_.Get() && component_.Get() != component && !restore()) return false;
    WeakObject live_component(component), live_pawn(pawn), previous_mesh(before);
    AssetLoadRoots loading_roots;
    auto* target = load(mesh_path); loading_roots.keep(target);
    WeakObject live_target(target);
    std::map<int,WeakObject> loaded_materials;
    for(const auto& [slot,path]:materials) {
        auto* value=load(path); loading_roots.keep(value);
        if(!value->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"))))
            throw std::runtime_error("Override asset is not a material");
        loaded_materials.emplace(slot,WeakObject(value));
    }
    if(live_target.Get()!=target || std::any_of(loaded_materials.begin(),loaded_materials.end(),[](const auto& pair){return !pair.second.Get();}))
        throw std::runtime_error("Appearance assets changed during loading; request cancelled");
    if (live_component.Get() != component || live_pawn.Get() != pawn || previous_mesh.Get() != before || mesh_asset(component) != before)
        throw std::runtime_error("Player appearance changed during asset loading; request cancelled");
    auto* type = static_cast<UClass*>(find(L"/Script/Engine.SkeletalMesh"));
    if (!target->IsA(type)) throw std::runtime_error("Selected asset is not a skeletal mesh");
    if (read<UObject*>(before, L"Skeleton") != read<UObject*>(target, L"Skeleton"))
        throw std::runtime_error("Different skeleton: appearance change refused");
    if (before == target && applied_materials_==materials && materials_match()) return true;
    if (before == target && applied_materials_==materials && reuse_materials()) return true;
    const bool returning_to_outfit=applied_.Get()==target && applied_materials_==materials && repair_mesh_needed();
    if (component_.Get() != component || before != applied_.Get()) {
        auto materials=material_paths(component);
        original_ = narrow(before->GetPathName());
        original_materials_=std::move(materials);
        original_live_materials_=material_objects(component);
        original_default_materials_.clear();
        const auto baseline=material_snapshot(component,before);
        for(const auto& name:baseline.at("defaults")) {
            const auto text=name.get<std::string>();const auto space=text.find(' ');
            if(space!=std::string::npos) original_default_materials_.insert(text.substr(space+1));
        }
        component_ = component;
    }
    // Record the intended asset before calling the engine so a failed read-back
    // can still be rolled back. No gameplay or animation-class setters are used.
    applied_ = target;
    try {
        // The same pawn can briefly return to its stock mesh during travel.
        // Once recovery is allowed, preserve valid dye resources here too.
        if(returning_to_outfit) {
            set_mesh(component,target);
            if(materials_match() || reuse_materials()) {
                current_mesh=narrow(target->GetPathName());
                return true;
            }
        }
        reset_colors();
        if(before!=target && !returning_to_outfit) set_mesh(component, target);
        const int count=overrides(component).Num();
        for(int i=0;i<count;++i) material(component,i,nullptr);
        auto defaults=material_snapshot(component,target).at("defaults");
        for(const auto& [slot,value]:loaded_materials) {
            if(slot<0 || static_cast<size_t>(slot)>=defaults.size()) throw std::runtime_error("Material slot exceeds selected mesh");
            material(component,slot,value.Get());
            defaults[slot]=narrow(value.Get()->GetFullName());
        }
        material_debug=material_snapshot(component,target);
        if(material_debug["effective"]!=defaults)
            throw std::runtime_error("Outfit material read-back failed");
        applied_materials_=materials;
        remember_materials();
    }
    catch (...) { restore(); throw; }
    current_mesh = narrow(target->GetPathName());
    return true;
}
bool Appearance::restore() {
    restore_menu();
    detach_residual_colors();
    auto* component = component_.Get();
    auto* applied = applied_.Get();
    if (component && applied && mesh_asset(component) == applied) {
        auto* original = load(original_);
        component=component_.Get();
        if(component && mesh_asset(component)==applied_.Get()) {
            set_mesh(component, original);
            restore_materials(component,original_materials_,original_live_materials_);
            if(component_.Get()==component) material_debug=material_snapshot(component,original);
        }
    }
    color_mids_.clear(); color_targets_.clear(); color_textures_.clear(); last_colors_.clear(); color_outfit_.clear();
    expected_materials_.clear();
    component_.Reset(); applied_.Reset(); original_.clear(); original_materials_.clear(); original_default_materials_.clear(); original_live_materials_.clear(); applied_materials_.clear();
    return true;
}
}

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>

namespace css {
namespace {
struct Vec2 { double x, y; };
struct Color { float r, g, b, a; };
struct SlateColor { Color color; uint8_t rule = 0; uint8_t padding[3]{}; };
struct Margin { float left, top, right, bottom; };
struct UVRect { float min_x=0,min_y=0,max_x=1,max_y=1; uint8_t valid=1; uint8_t padding[3]{}; };
constexpr Color gold{0.69f, 0.49f, 0.25f, 1}, ivory{0.88f, 0.83f, 0.72f, 1};
constexpr Color muted{0.43f, 0.45f, 0.43f, 1};
void invoke(UObject* object, const wchar_t* fn) { Call c(object, fn, 0); c.run(); }
template<class T> void invoke(UObject* object, const wchar_t* fn, const wchar_t* param, const T& value) {
    Call c(object, fn, 1); c.set(param, value); c.run();
}
UObject* construct(const wchar_t* type, UObject* outer) {
    auto* cls = static_cast<UClass*>(find(type));
    FStaticConstructObjectParameters params(cls, outer);
    auto* object = UObjectGlobals::StaticConstructObject(params);
    if (!object) throw std::runtime_error("Could not construct wardrobe widget");
    return object;
}
void object_property(UObject* object, const wchar_t* name, UObject* value) {
    auto* p = field(object, name, sizeof(UObject*));
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(object) + p->GetOffset_Internal(), &value);
}

void text_value(UObject* widget, const std::string& text) {
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"), L"Conv_StringToText", 2);
    FString value(wide(text).c_str()); convert.set(L"InString", value); convert.run();
    Call set(widget, L"SetText", 1); set.copy(L"InText", convert, L"ReturnValue"); set.run();
}
template<class T> void member(void* data, size_t bytes, UObject* structure, const wchar_t* name, const T& value) {
    auto* p=field(structure,name,sizeof(T));
    if(p->GetOffset_Internal()<0 || static_cast<size_t>(p->GetOffset_Internal())+sizeof(T)>bytes)
        throw std::runtime_error("Widget style member exceeds reflected structure");
    p->CopyCompleteValue(static_cast<std::byte*>(data)+p->GetOffset_Internal(),&value);
}
void font_size(UObject* widget, float size, UObject* font_object=nullptr) {
    auto* font = widget->GetPropertyByNameInChain(L"Font");
    auto* size_field = field(find(L"/Script/SlateCore.SlateFontInfo"), L"Size", sizeof(float));
    Call set(widget, L"SetFont", 1);
    auto* param = set.param(L"InFontInfo");
    if (!font || !font->SameType(param) || size_field->GetOffset_Internal() < 0 || static_cast<size_t>(size_field->GetOffset_Internal()) + sizeof(float) > static_cast<size_t>(param->GetElementSize()))
        throw std::runtime_error("Wardrobe font layout mismatch");
    param->CopyCompleteValue(set.data(param), reinterpret_cast<std::byte*>(widget) + font->GetOffset_Internal());
    std::memcpy(static_cast<std::byte*>(set.data(param)) + size_field->GetOffset_Internal(), &size, sizeof(size));
    auto* info=find(L"/Script/SlateCore.SlateFontInfo");
    if(font_object) member(set.data(param),param->GetElementSize(),info,L"FontObject",font_object);
    member(set.data(param),param->GetElementSize(),info,L"TypefaceFontName",FName(L"Regular"));
    set.run();
}
void flat_button(UObject* widget,bool active) {
    Call set(widget,L"SetStyle",1);
    auto* param=set.param(L"InStyle");
    auto* source=widget->GetPropertyByNameInChain(L"WidgetStyle");
    if(!source || !source->SameType(param)) throw std::runtime_error("Button style layout mismatch");
    param->CopyCompleteValue(set.data(param),reinterpret_cast<std::byte*>(widget)+source->GetOffset_Internal());
    auto* style=find(L"/Script/SlateCore.ButtonStyle"); auto* brush=find(L"/Script/SlateCore.SlateBrush");
    for(const auto* name:{L"Normal",L"Hovered",L"Pressed",L"Disabled"}) {
        auto* p=style->GetPropertyByNameInChain(name);
        if(!p || p->GetOffset_Internal()<0 || p->GetOffset_Internal()+p->GetElementSize()>param->GetElementSize()) throw std::runtime_error("Invalid button brush");
        auto* data=static_cast<std::byte*>(set.data(param))+p->GetOffset_Internal();
        Color tint{0,0,0,0};
        if(std::wstring_view(name)==L"Hovered") tint={0.18f,0.135f,0.075f,0.55f};
        else if(std::wstring_view(name)==L"Pressed") tint={0.30f,0.225f,0.12f,0.7f};
        else if(active) tint={0.10f,0.075f,0.035f,0.25f};
        member(data,p->GetElementSize(),brush,L"DrawAs",uint8_t{3});
        member(data,p->GetElementSize(),brush,L"TintColor",SlateColor{tint});
        member(data,p->GetElementSize(),brush,L"ResourceObject",static_cast<UObject*>(nullptr));
        member(data,p->GetElementSize(),brush,L"Margin",Margin{});
    }
    set.run();
}
UObject* content(UObject* parent, UObject* child) {
    Call c(parent, L"SetContent", 2); c.set(L"content", child); c.run(); return c.get<UObject*>();
}
struct Layout {
    UObject* tree; UObject* canvas; double scale;
    UObject* serif;
    double origin_x = 0, origin_y = 0;
    void place(UObject* widget, double x, double y, double width, double height) {
        Call add(canvas, L"AddChildToCanvas", 2); add.set(L"content", widget); add.run();
        auto* slot = add.get<UObject*>();
        invoke(slot, L"SetPosition", L"InPosition", Vec2{(x-origin_x)*scale, (y-origin_y)*scale});
        invoke(slot, L"SetSize", L"InSize", Vec2{width*scale, height*scale});
    }
    UObject* box(double x, double y, double w, double h, Color color) {
        auto* widget = construct(L"/Script/UMG.Border", tree);
        invoke(widget, L"SetBrushColor", L"InBrushColor", color);
        invoke(widget, L"SetVisibility", L"InVisibility", uint8_t{4}); // SelfHitTestInvisible.
        place(widget,x,y,w,h); return widget;
    }
    void sigil(double x, double y, double size, Color color) {
        // Original shell mark, built from native shapes with no texture uploads.
        box(x+size*.47,y+size*.13,size*.06,size*.76,color);
        for (int side : {-1,1}) {
            auto* stroke=box(x+size*(side<0?.19:.75),y+size*.2,size*.055,size*.63,color);
            invoke(stroke,L"SetRenderTransformAngle",L"Angle",static_cast<float>(side*22));
            auto* crown=box(x+size*(side<0?.28:.65),y+size*.04,size*.05,size*.38,color);
            invoke(crown,L"SetRenderTransformAngle",L"Angle",static_cast<float>(side*-38));
        }
    }
    void star(double x,double y,double radius,Color color) {
        for(int i=0;i<10;++i) {
            double a=(i*36-90)*3.141592653589793/180,b=((i+1)*36-90)*3.141592653589793/180;
            double r1=i%2?radius*.43:radius,r2=i%2?radius:radius*.43;
            double x1=x+std::cos(a)*r1,y1=y+std::sin(a)*r1,x2=x+std::cos(b)*r2,y2=y+std::sin(b)*r2;
            double length=std::hypot(x2-x1,y2-y1);
            auto* line=box((x1+x2-length)/2,(y1+y2)/2-1,length,2,color);
            invoke(line,L"SetRenderTransformAngle",L"Angle",static_cast<float>(std::atan2(y2-y1,x2-x1)*180/3.141592653589793));
        }
    }
    UObject* label(const std::string& text, double x, double y, double w, double h, float size, Color color=ivory) {
        auto* widget = construct(L"/Script/UMG.TextBlock", tree);
        text_value(widget,text); font_size(widget, size*static_cast<float>(scale),size>=22?serif:nullptr);
        invoke(widget,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{color});
        invoke(widget,L"SetAutoWrapText",L"InAutoTextWrap",true);
        invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3}); // HitTestInvisible.
        place(widget,x,y,w,h); return widget;
    }
    UObject* button(const std::string& text,double x,double y,double w,double h,bool active=false,bool enabled=true,float size=20) {
        auto* widget=construct(L"/Script/UMG.Button",tree);
        flat_button(widget,active);
        auto* focusable=widget->GetPropertyByNameInChain(L"IsFocusable");
        if(!focusable || !focusable->IsA<FBoolProperty>()) throw std::runtime_error("Button focus property mismatch");
        static_cast<FBoolProperty*>(focusable)->SetPropertyValueInContainer(widget,false);
        auto* label=construct(L"/Script/UMG.TextBlock",tree);
        text_value(label,text); font_size(label,size*static_cast<float>(scale),size>=22?serif:nullptr);
        invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{enabled ? (active ? gold : ivory) : muted});
        invoke(label,L"SetJustification",L"InJustification",uint8_t{1});
        content(widget,label);
        invoke(widget,L"SetIsEnabled",L"bInIsEnabled",enabled);
        place(widget,x,y,w,h); return widget;
    }
    UObject* image(UObject* texture,double x,double y,double w,double h,float opacity=1,UVRect uv={}) {
        auto* widget=construct(L"/Script/UMG.Image",tree);
        Call brush(widget,L"SetBrushFromTexture",2); brush.set(L"Texture",texture); brush.set(L"bMatchSize",false); brush.run();
        Call set(widget,L"SetBrush",1); auto* p=set.param(L"InBrush");
        auto* source=widget->GetPropertyByNameInChain(L"Brush");
        if(!source || !source->SameType(p)) throw std::runtime_error("Image brush layout mismatch");
        p->CopyCompleteValue(set.data(p),reinterpret_cast<std::byte*>(widget)+source->GetOffset_Internal());
        member(set.data(p),p->GetElementSize(),find(L"/Script/SlateCore.SlateBrush"),L"UVRegion",uv);
        set.run();
        invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3});
        invoke(widget,L"SetRenderOpacity",L"InOpacity",opacity);
        place(widget,x,y,w,h); return widget;
    }
};
}
namespace {
constexpr double pi=3.14159265358979323846;
#ifdef CSS_INVENTORY_DEV
UObject* view_target(UObject* pc) {
    auto* manager=read<UObject*>(pc,L"PlayerCameraManager");
    if(manager && manager->GetPropertyByNameInChain(L"ActiveCameraActor")) {
        auto* active=read<UObject*>(manager,L"ActiveCameraActor");
        if(active && WeakObject(active).Get()) return active;
    }
    Call c(pc,L"GetViewTarget",1); c.run(); return c.get<UObject*>();
}
#endif
}

}

namespace css {
namespace {
void update_dye_mips(UObject* target) {
    // The Canvas path does not regenerate lower mip levels. Resolve the same
    // engine method called by the verified CreateRenderTarget2D native wrapper.
    // Only engine code is queued on the render thread, never a CSS callback.
    auto* module=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    struct Adapter {
        uintptr_t wrapper,callsite,update;
        std::array<unsigned char,10> caller;
        std::array<unsigned char,32> prologue;
    };
    // Both paths were traced from CreateRenderTarget2D through its direct
    // call and checked against the render-resource enqueue implementation.
    // Keep the old build supported. Never fall back to a nearby address.
    constexpr Adapter adapters[]{
        {0x3f28b70,0x3f28e5a,0x44c85d0,
         {0xb2,0x01,0x48,0x8b,0xcb,0xe8,0x6c,0xf7,0x59,0},
         {0x40,0x53,0x57,0x48,0x81,0xec,0xa8,0,0,0,0x48,0x8b,0x05,0x9f,0xd4,0xbd,0x06,0x48,0x33,0xc4,0x48,0x89,0x84,0x24,0x80,0,0,0,0x0f,0xb6,0xda,0x48}},
        // Steam build 25265616, September 15 hotfix.
        {0x3f28ba0,0x3f28e8a,0x44c8700,
         {0xb2,0x01,0x48,0x8b,0xcb,0xe8,0x6c,0xf8,0x59,0},
         {0x40,0x53,0x57,0x48,0x81,0xec,0xa8,0,0,0,0x48,0x8b,0x05,0xaf,0xb3,0xbd,0x06,0x48,0x33,0xc4,0x48,0x89,0x84,0x24,0x80,0,0,0,0x0f,0xb6,0xda,0x48}}
    };
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) throw std::runtime_error("Unknown game image for dye mipmaps");
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(module+dos->e_lfanew);
    auto* function=find(L"/Script/Engine.Default__KismetRenderingLibrary")->GetFunctionByNameInChain(L"CreateRenderTarget2D");
    const Adapter* match=nullptr;
    if(nt->Signature==IMAGE_NT_SIGNATURE && function) for(const auto& adapter:adapters) {
        if(nt->OptionalHeader.SizeOfImage<adapter.update+adapter.prologue.size() ||
           nt->OptionalHeader.SizeOfImage<adapter.callsite+adapter.caller.size() ||
           reinterpret_cast<const unsigned char*>(function->GetFuncPtr())!=module+adapter.wrapper) continue;
        if(!std::memcmp(module+adapter.callsite,adapter.caller.data(),adapter.caller.size()) &&
           !std::memcmp(module+adapter.update,adapter.prologue.data(),adapter.prologue.size())) {match=&adapter;break;}
    }
    if(!match) throw std::runtime_error("Dye mipmaps require a verified Mortal Shell II build");
    if(!target->IsA(static_cast<UClass*>(find(L"/Script/Engine.TextureRenderTarget2D")))) throw std::runtime_error("Invalid dye render target");
    reinterpret_cast<void(*)(UObject*,bool)>(const_cast<unsigned char*>(module)+match->update)(target,false);
}
}
void Appearance::reset_colors() {
    if(auto* component=component_.Get()) for(const auto& [slot,weak]:color_mids_) {
        if(auto* mid=weak.Get()) {
            Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",slot); current.run();
            if(current.get<UObject*>()==mid) material(component,slot,applied_materials_.contains(slot)?read<UObject*>(mid,L"Parent"):nullptr);
        }
    }
    color_mids_.clear(); color_targets_.clear(); color_textures_.clear(); last_colors_.clear(); color_outfit_.clear();
}
void Appearance::prepare_deformation_materials() {
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=applied_.Get()) return;
    // M_Uber interprets native dash data as DarkBro vertex stretching. Imported
    // outfits do not share the stock mesh's displacement layout. Disable that
    // branch on our instances, retaining the separate dash overlay and shell
    // explosion parameter. Reuse the same owned MIDs for dyes and recovery.
    const FName parameter(L"DarkBro WPO Stretch Scale",FNAME_Add);
    Call count(component,L"GetNumMaterials",1);count.run();
    const auto slots=count.get<int>();
    if(slots<0 || slots>256) throw std::runtime_error("Appearance material count exceeds deformation limit");
    auto* dynamic=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstanceDynamic"));
    auto* instance=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstance"));
    for(int slot=0;slot<slots;++slot) {
        Call current(component,L"GetMaterial",2);current.set(L"ElementIndex",slot);current.run();
        auto* parent=current.get<UObject*>();if(!parent) continue;
        // Standalone materials such as Beaute's cloth driver have no instance
        // parameter API. Leave those surfaces intact.
        if(!parent->IsA(instance)) continue;
        Call value(parent,L"K2_GetScalarParameterValue",2);value.set(L"ParameterName",parameter);value.run();
        const auto scale=value.get<float>();
        if(!std::isfinite(scale) || scale==0.f) continue;
        auto owned=color_mids_.find(slot);
        UObject* mid=owned==color_mids_.end()?nullptr:owned->second.Get();
        if(parent!=mid) {
            // A gameplay effect or another mod owns unfamiliar dynamic parents.
            if(parent->IsA(dynamic)) continue;
            Call make(component,L"CreateDynamicMaterialInstance",4);
            make.set(L"ElementIndex",slot);make.set(L"SourceMaterial",parent);make.run();
            mid=make.get<UObject*>();
            if(!mid) throw std::runtime_error("Could not create the deformation compatibility material");
            color_mids_[slot]=mid;
        }
        Call set(mid,L"SetScalarParameterValue",2);set.set(L"ParameterName",parameter);set.set(L"Value",0.f);set.run();
        Call readback(mid,L"K2_GetScalarParameterValue",2);readback.set(L"ParameterName",parameter);readback.run();
        if(readback.get<float>()!=0.f) throw std::runtime_error("Deformation parameter read-back failed");
    }
}
void Appearance::customize(const Outfit& outfit,const std::string& variant,const Customization& custom) {
    const auto& options=outfit.colors_for(variant);
    auto values=color_values(options,custom);
    auto* component=component_.Get();
    if(!component || !applied_.Get() || mesh_asset(component)!=applied_.Get()) throw std::runtime_error("Appearance changed before colors could apply");
    const auto color_identity=outfit.id+":"+variant;
    if(color_outfit_!=color_identity) reset_colors();
    // Dropping a control restores its authored value, including layered parameters.
    // Rebuild from the original material rather than guessing a layer's default.
    if(std::any_of(last_colors_.begin(),last_colors_.end(),[&](const auto& p){return !values.contains(p.first);})) reset_colors();
    prepare_deformation_materials();
    if(values.empty()) { color_outfit_=color_identity; material_debug=material_snapshot(component,applied_.Get()); material_debug["colors"]=Json::object(); material_debug["dye_targets"]=0; remember_materials(); return; }
    if(values==last_colors_) return;
    auto mid_for=[&](int index) {
        Call count(component,L"GetNumMaterials",1); count.run();
        if(index<0 || index>=count.get<int>()) throw std::runtime_error("Color slot is absent on this appearance");
        auto& weak=color_mids_[index];
        if(auto* mid=weak.Get()) return mid;
        Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",index); current.run();
        auto* parent=current.get<UObject*>();
        if(!parent || parent->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInstanceDynamic"))))
            throw std::runtime_error("Wait for the temporary material effect to finish before coloring");
        Call make(component,L"CreateDynamicMaterialInstance",4); make.set(L"ElementIndex",index); make.set(L"SourceMaterial",parent); make.run();
        auto* mid=make.get<UObject*>(); if(!mid) throw std::runtime_error("Could not create the color material");
        weak=mid; return mid;
    };
    try {
        auto* library=find(L"/Script/Engine.Default__KismetRenderingLibrary");
        for(const auto& surface:options.surfaces) {
            bool active=false,changed=false;
            for(const auto& [id,file]:surface.layers) {
                active|=values.contains(id);
                changed|=values.contains(id)!=last_colors_.contains(id) || (values.contains(id) && last_colors_.contains(id) && values.at(id)!=last_colors_.at(id));
            }
            if(!changed) continue;
            auto parameter=FName(wide(surface.parameter).c_str(),FNAME_Add);
            auto* first=mid_for(surface.slots.front());
            Call base(read<UObject*>(first,L"Parent"),L"K2_GetTextureParameterValue",2); base.set(L"ParameterName",parameter); base.run();
            auto* original=base.get<UObject*>();
            if(!original) throw std::runtime_error("The selected material has no dyeable base texture");
            UObject* target=original;
            if(active) {
                auto& weak=color_targets_[surface.id]; target=weak.Get();
                if(!target) {
                    Call create(library,L"CreateRenderTarget2D",8); create.set(L"WorldContextObject",component);
                    create.set(L"Width",surface.resolution); create.set(L"Height",surface.resolution); create.set(L"Format",uint8_t{3});
                    create.set(L"bAutoGenerateMipMaps",true); create.run(); target=create.get<UObject*>();
                    if(!target) throw std::runtime_error("Could not create the dye texture"); weak=target;
                }
                // Keep the render target referenced by the component before importing layer textures.
                for(int slot:surface.slots) {
                    Call set(mid_for(slot),L"SetTextureParameterValue",2); set.set(L"ParameterName",parameter); set.set(L"Value",target); set.run();
                }
                std::vector<std::pair<WeakObject,ColorValue>> layers;
                for(const auto& [id,file]:surface.layers) if(values.contains(id)) {
                    auto& texture=color_textures_[file];
                    if(!texture.Get()) {
                        auto path=outfit.resources/file;
                        if(!fs::is_regular_file(path)) throw std::runtime_error("The outfit's color mask is missing");
                        Call import(library,L"ImportFileAsTexture2D",3); import.set(L"WorldContextObject",component); import.set(L"Filename",FString(path.c_str())); import.run();
                        auto* loaded=import.get<UObject*>(); if(!loaded) throw std::runtime_error("Could not load the outfit's color mask"); texture=loaded;
                    }
                    auto color=values.at(id); for(int i=0;i<3;++i) color[i]=srgb_linear(color[i]);
                    layers.emplace_back(texture,color);
                }
                for(const auto& [texture,color]:layers) if(!texture.Get()) throw std::runtime_error("Color textures changed while loading");
                Call begin(library,L"BeginDrawCanvasToRenderTarget",5); begin.set(L"WorldContextObject",component); begin.set(L"TextureRenderTarget",target); begin.run();
                auto end=[&] { Call finish(library,L"EndDrawCanvasToRenderTarget",2); finish.set(L"WorldContextObject",component); finish.copy(L"Context",begin,L"Context"); finish.run(); };
                try {
                    auto* canvas=begin.get<UObject*>(L"Canvas");
                    auto draw=[&](UObject* texture,const ColorValue& color,uint8_t blend) {
                        Call call(canvas,L"K2_DrawTexture",9); call.set(L"RenderTexture",texture);
                        call.set(L"ScreenSize",Vec2{double(surface.resolution),double(surface.resolution)}); call.set(L"CoordinateSize",Vec2{1,1});
                        call.set(L"RenderColor",color); call.set(L"BlendMode",blend); call.run();
                    };
                    draw(original,{1,1,1,1},0);
                    for(const auto& [texture,color]:layers) draw(texture.Get(),color,2);
                } catch(...) { end(); throw; }
                end();
                update_dye_mips(target);
            }
            for(int slot:surface.slots) {
                auto* mid=mid_for(slot);
                Call set(mid,L"SetTextureParameterValue",2); set.set(L"ParameterName",parameter); set.set(L"Value",target); set.run();
                Call readback(mid,L"K2_GetTextureParameterValue",2); readback.set(L"ParameterName",parameter); readback.run();
                if(readback.get<UObject*>()!=target) throw std::runtime_error("Dye texture read-back failed");
            }
        }
        for(const auto& control:options.controls) {
            bool active=values.contains(control.id),previous=last_colors_.contains(control.id);
            if(!active) continue;
            if(active && previous && values.at(control.id)==last_colors_.at(control.id)) continue;
            for(const auto& binding:control.bindings) {
                auto* mid=mid_for(binding.slot);
                auto color=active?values.at(control.id):control.value;
                auto parameter=FName(wide(binding.parameter).c_str(),FNAME_Add);
                // Explicit layer associations use the engine's reflected FMaterialParameterInfo.
                Call set(mid,control.scalar?L"SetScalarParameterValueByInfo":L"SetVectorParameterValueByInfo",2);
                auto* p=set.param(L"ParameterInfo"); auto* info=find(L"/Script/Engine.MaterialParameterInfo");
                member(set.data(p),p->GetElementSize(),info,L"Name",parameter);
                member(set.data(p),p->GetElementSize(),info,L"Association",uint8_t(binding.association));
                member(set.data(p),p->GetElementSize(),info,L"Index",binding.layer);
                if(!control.scalar) for(int i=0;i<3;++i) color[i]=srgb_linear(color[i]);
                if(control.scalar) set.set(L"Value",color[0]); else set.set(L"Value",color);
                set.run();
                Call readback(mid,control.scalar?L"K2_GetScalarParameterValueByInfo":L"K2_GetVectorParameterValueByInfo",2);
                readback.copy(L"ParameterInfo",set,L"ParameterInfo"); readback.run();
                if(control.scalar ? std::abs(readback.get<float>()-color[0])>.00001f : readback.get<ColorValue>()!=color)
                    throw std::runtime_error("Color parameter read-back failed");
            }
        }
        prepare_deformation_materials();
        last_colors_=std::move(values); color_outfit_=color_identity;
        material_debug=material_snapshot(component,applied_.Get());
        material_debug["colors"]=last_colors_;
        material_debug["dye_targets"]=color_targets_.size();
        remember_materials();
    } catch(...) { reset_colors(); throw; }
}

}

#include "inventory_ui.inl"

#include "inventory_view.inl"

#include "extension_hooks.inl"
#include "extension_engine.inl"

#include "extension_kit.inl"
#include "extension_view.inl"

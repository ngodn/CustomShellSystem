#include "engine.hpp"
#include <windows.h>
#include "startup.hpp"
#include "skeleton_compatibility.hpp"
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
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
    // Kept raw on purpose: WeakObject's constructor calls find() to initialize an object's
    // serial number, so find() must not itself build a WeakObject or the two would recurse.
    auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path);
    if (!object) throw std::runtime_error("Required reflected object is missing");
    return object;
}

// Reflection handle cache. Resolving a UFunction or FProperty by name walks the class
// chain and hashes an FName at each level, and building a call's parameter list is another
// allocation. In the per-frame seal and MISC passes that is dozens of chain walks and small
// heap allocations every frame for a fixed set of names. The resolved handles are invariant
// for as long as their owning object (the class, or the found object) stays alive, so we
// resolve once and keep them, keyed by (owner, name). A WeakObject on the owner is the
// validity gate: FWeakObjectPtr carries a serial number, so if the class is garbage
// collected, or its slot is reused by a different class on a level load, Get() returns null
// and we re-resolve. Every call happens on the game thread (engine.hpp), so no locking.
// Lookups take a wstring_view and never allocate; only a miss allocates the stored key once.
namespace refl {
struct Key { const void* owner; std::wstring name; };
struct View { const void* owner; std::wstring_view name; };
struct Hash {
    using is_transparent = void;
    static size_t mix(const void* owner, std::wstring_view name) noexcept {
        size_t h = std::hash<const void*>{}(owner) + 0x9e3779b97f4a7c15ull;
        h ^= std::hash<std::wstring_view>{}(name) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    }
    size_t operator()(const Key& k) const noexcept { return mix(k.owner, k.name); }
    size_t operator()(const View& k) const noexcept { return mix(k.owner, k.name); }
};
struct Eq {
    using is_transparent = void;
    bool operator()(const Key& a, const Key& b) const noexcept { return a.owner==b.owner && a.name==b.name; }
    bool operator()(const Key& a, const View& b) const noexcept { return a.owner==b.owner && a.name==b.name; }
    bool operator()(const View& a, const Key& b) const noexcept { return a.owner==b.owner && a.name==b.name; }
    bool operator()(const View& a, const View& b) const noexcept { return a.owner==b.owner && a.name==b.name; }
};
struct WHash { using is_transparent = void; size_t operator()(std::wstring_view s) const noexcept { return std::hash<std::wstring_view>{}(s); } };
struct WEq { using is_transparent = void; bool operator()(std::wstring_view a, std::wstring_view b) const noexcept { return a==b; } };
// Read-only liveness check for a cached owner (a UClass). It mirrors WeakObject's array
// check but never forces serial initialization: doing so would issue a reflected Call and
// recurse back through this very cache before the entry is stored. Pointer identity catches
// a freed or replaced slot; the serial, when the class has one, catches slot reuse.
struct OwnerGuard {
    const void* ptr = nullptr; int32_t index = -1; int32_t serial = 0;
    void capture(UObject* object) {
        ptr = object; index = object->GetInternalIndex();
        auto* item = FUObjectArray::IndexToObject(index);
        serial = item ? item->GetSerialNumber() : 0;
    }
    bool alive() const {
        if (!ptr || index < 0) return false;
        auto* item = FUObjectArray::IndexToObject(index);
        return item && item->GetUObject() == ptr && item->GetSerialNumber() == serial;
    }
};
}

static UObject* find_optional(const wchar_t*);

// Non-throwing, cached StaticFindObject for fixed objects (class-default objects, engine
// classes). Safe against reuse because the stored WeakObject re-resolves once the found
// object dies. Distinct from find(): WeakObject's own serial init routes through raw find(),
// so this never sits on that path and cannot recurse.
static std::unordered_map<std::wstring, WeakObject, refl::WHash, refl::WEq> s_object_cache;
static UObject* find_optional(const wchar_t* path) {
    if (auto it = s_object_cache.find(std::wstring_view{path}); it != s_object_cache.end()) {
        if (auto* cached = it->second.Get()) return cached;
    }
    auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, path);
    if (object) s_object_cache.insert_or_assign(std::wstring{path}, WeakObject(object));
    return object;
}

struct FieldEntry { refl::OwnerGuard owner; FProperty* property = nullptr; };
static std::unordered_map<refl::Key, FieldEntry, refl::Hash, refl::Eq> s_field_cache;
static FProperty* field(UObject* object, const wchar_t* name, size_t size) {
    if (!object) throw std::runtime_error("No live object");
    UObject* owner = object->GetClassPrivate();
    FProperty* property = nullptr;
    if (owner) {
        if (auto it = s_field_cache.find(refl::View{owner, name}); it != s_field_cache.end() && it->second.owner.alive())
            property = it->second.property;
    }
    if (!property) {
        property = object->GetPropertyByNameInChain(name);
        if (property && owner) {
            FieldEntry entry; entry.owner.capture(owner); entry.property = property;
            s_field_cache.insert_or_assign(refl::Key{owner, name}, entry);
        }
    }
    // The size/dim check stays per call: it validates the caller's T against this property.
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
// The resolved function and its parameter list are invariant for a class, so they are
// cached per (class, name) and validated read-only (OwnerGuard). resolve_call issues no
// reflected Call of its own, so it cannot re-enter the cache. The parameter vector lives in
// the cache and a Call only points at it, so constructing a Call allocates nothing.
struct CallEntry { refl::OwnerGuard owner; UFunction* function = nullptr; std::vector<FProperty*> params; };
static std::unordered_map<refl::Key, CallEntry, refl::Hash, refl::Eq> s_call_cache;
static const CallEntry& resolve_call(UObject* object, const wchar_t* name) {
    UObject* owner = object->GetClassPrivate();
    if (owner) {
        if (auto it = s_call_cache.find(refl::View{owner, name}); it != s_call_cache.end() && it->second.owner.alive())
            return it->second;
    }
    CallEntry entry;
    entry.function = object->GetFunctionByNameInChain(name);
    if (!entry.function || entry.function->GetParmsSize() > 2048)
        throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
    for (auto* p : entry.function->ForEachProperty()) {
        if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
        if (p->GetOffset_Internal() < 0 || p->GetArrayDim() != 1 ||
            p->GetOffset_Internal() + p->GetElementSize() > entry.function->GetParmsSize())
            throw std::runtime_error("Parameter exceeds reflected frame");
        entry.params.push_back(p);
    }
    // Should not happen for a real UObject; without a class we cannot key, so return an
    // uncached scratch entry so the caller still works (unbatched, this frame only).
    if (!owner) { static thread_local CallEntry scratch; scratch = std::move(entry); return scratch; }
    entry.owner.capture(owner);
    // unordered_map keeps element references stable across rehash, so a live Call's pointer
    // into params stays valid; we never erase entries.
    return s_call_cache.insert_or_assign(refl::Key{owner, name}, std::move(entry)).first->second;
}
class Call {
    UObject* object_;
    UFunction* function_ = nullptr;
    alignas(16) std::array<std::byte, 2048> bytes_{};
    const std::vector<FProperty*>* params_ = nullptr;   // owned by s_call_cache, stable across rehash
public:
    Call(UObject* object, const wchar_t* name, unsigned count) : object_(object) {
        if (!object) throw std::runtime_error("No target for reflected call");
        const CallEntry& entry = resolve_call(object, name);
        if (entry.function->GetNumParms() != count || entry.params.size() != count)
            throw std::runtime_error("Reflected function signature mismatch: " + narrow(name));
        function_ = entry.function;
        params_ = &entry.params;
        for (auto* p : *params_) p->InitializeValue(bytes_.data() + p->GetOffset_Internal());
    }
    ~Call() { if (params_) for (auto* p : *params_) p->DestroyValue(bytes_.data() + p->GetOffset_Internal()); }
    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;
    FProperty* param(const wchar_t* name) {
        for (auto* p : *params_) if (p->GetName() == name) return p;
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
    // Positional argument access, for functions whose parameter names we do not
    // know (native setters the Lua reference calls positionally).
    FProperty* arg(size_t index) {
        size_t i = 0;
        for (auto* p : *params_) if (!p->HasAnyPropertyFlags(CPF_ReturnParm)) { if (i == index) return p; ++i; }
        throw std::runtime_error("Reflected argument index out of range");
    }
    template<typename T> void set_arg(size_t index, const T& value) {
        auto* p = arg(index);
        if (p->GetElementSize() != sizeof(T)) throw std::runtime_error("Argument size mismatch");
        p->CopyCompleteValue(data(p), &value);
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
static std::unordered_map<std::string, WeakObject> s_asset_cache;
static UObject* load(const std::string& path) {
    if (auto it = s_asset_cache.find(path); it != s_asset_cache.end()) {
        if (auto* cached = it->second.Get()) return cached;
    }
    auto name = wide(path);
    if (auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, name.c_str())) {
        s_asset_cache[path] = object;
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
    if (!object) throw std::runtime_error("Asset could not load: " + path);
    s_asset_cache[path] = object;
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
#include "original_shells.inl"
static UObject* mesh_asset(UObject* component) {
    Call call(component, L"GetSkeletalMeshAsset", 1); call.run(); return call.get<UObject*>();
}
static bool is_compatible_skeleton(UObject* before_mesh, UObject* target_mesh) {
    if (!before_mesh || !target_mesh) return false;
    auto* before_skel = read<UObject*>(before_mesh, L"Skeleton");
    auto* target_skel = read<UObject*>(target_mesh, L"Skeleton");
    if (!before_skel || !target_skel) return false;
    if (before_skel == target_skel) return true;
    return compatible_standard_skeletons(narrow(before_skel->GetPathName()),
                                         narrow(target_skel->GetPathName()));
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
        value=read<UObject*>(value,L"Parent");
        if(!value) throw std::runtime_error("Dynamic material has no asset parent");
    }
    return value;
}
static UObject* material_base_asset(UObject* value) {
    std::set<UObject*> seen;
    while(dynamic_material(value)) {
        if(seen.size()>=16 || !seen.insert(value).second) return nullptr;
        value=read<UObject*>(value,L"Parent");
        if(!value) return nullptr;
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
#include "attachment_follower.inl"
#include "walk_override.inl"
#include "misc_visibility.inl"
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
        if(component==component_.Get()) detach_residual_controls();
        if(auto* mesh = mesh_asset(component)) current_mesh = narrow(mesh->GetPathName());
    }
    return pawn;
}
void Appearance::restore_menu() {
    restore_menu_physics();
    menu_misc_.restore();   // show any accessories MISC hid on the wardrobe preview
    menu_attachments_.release();
    menu_items_.release();
    if(auto* preview=menu_component_.Get()) for(int section:menu_hidden_sections_)
        for(int lod=0;lod<lod_count();++lod) {
            Call set(preview,L"ShowMaterialSection",4);
            set.set(L"MaterialID",int32_t(section)); set.set(L"SectionIndex",int32_t{-1});
            set.set(L"bShow",true); set.set(L"LODIndex",int32_t(lod)); set.run();
        }
    menu_hidden_sections_.clear();
    if(auto* preview=menu_component_.Get()) for(const auto& [morph,weight]:driven_morphs_) {
        Call set(preview,L"SetMorphTarget",3);
        set.set(L"MorphTargetName",FName(wide(morph).c_str(),FNAME_Add));
        set.set(L"Value",0.f); set.set(L"bRemoveZeroWeight",true); set.run();
    }
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
    if(!before || !desired || !is_compatible_skeleton(before, desired)) { restore_menu(); return; }
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
    push_morphs(target);
    sync_menu_physics(target);
    if(!current_items_.empty()) menu_items_.update(target,current_items_identity_,current_items_);
    // A section an item covers, or a toggle switched off, has to be hidden on the preview
    // too, or the wardrobe shows a part the body is not wearing.
    for(int section:applied_hidden_) {
        if(!menu_hidden_sections_.insert(section).second) continue;
        for(int lod=0;lod<lod_count();++lod) {
            Call set(target,L"ShowMaterialSection",4);
            set.set(L"MaterialID",int32_t(section)); set.set(L"SectionIndex",int32_t{-1});
            set.set(L"bShow",false); set.set(L"LODIndex",int32_t(lod)); set.run();
        }
    }
    for(auto it=menu_hidden_sections_.begin();it!=menu_hidden_sections_.end();) {
        if(applied_hidden_.contains(*it)) { ++it; continue; }
        for(int lod=0;lod<lod_count();++lod) {
            Call set(target,L"ShowMaterialSection",4);
            set.set(L"MaterialID",int32_t(*it)); set.set(L"SectionIndex",int32_t{-1});
            set.set(L"bShow",true); set.set(L"LODIndex",int32_t(lod)); set.run();
        }
        it=menu_hidden_sections_.erase(it);
    }
    // MISC on the wardrobe preview is driven from sync_misc (which resolves the display
    // character itself), so it also works on a default shell where this function returns early.
}
void Appearance::remember_materials() {
    expected_materials_.clear();
    auto values=overrides(component_.Get());
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        expected_materials_.emplace_back(value);
    }
}
void Appearance::detach_residual_controls() {
    if(control_mids_.empty()) return;
    auto* component=component_.Get();
    if(!component || !applied_.Get() || mesh_asset(component)==applied_.Get()) return;
    // Changing gameplay shells can leave trailing OverrideMaterials entries
    // from the previous, larger mesh. Remove only our exact MID objects, never
    // game-created effects or another mod's replacement. Keep the weak cache so
    // a temporary stock-mesh reset can still reuse live dye resources.
    auto values=overrides(component);
    for(const auto& [slot,weak]:control_mids_) {
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
static bool is_quest_or_teleport_active(UObject* pc);
static bool is_traversal_ability_active(UObject* pawn);

bool Appearance::repair_materials_needed() const {
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=applied_.Get() || materials_match()) { return false; }
    auto* pc=observed_controller_.Get();
    if(is_quest_or_teleport_active(pc)) return false;
    auto* pawn=observed_pawn_.Get();
    if(is_traversal_ability_active(pawn)) return false;
    // A completed shell effect or fast-travel resets stock shell materials onto the mesh.
    // Repair that transition, leaving only active transient MIDs with parameter overrides
    // (such as Harden or active elemental debuffs) under the game's control until finished.
    auto values=overrides(component);
    for(int i=0;i<values.Num();++i) {
        UObject* value{}; std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        auto* expected=i<static_cast<int>(expected_materials_.size())?expected_materials_[i].Get():nullptr;
        if(value && value!=expected) {
            if(dynamic_material(value)) {
                auto* asset=material_base_asset(value);
                std::string path=asset?narrow(asset->GetPathName()):std::string{};
                bool is_stock_shell=original_default_materials_.contains(path) ||
                                    std::find(original_materials_.begin(),original_materials_.end(),path)!=original_materials_.end() ||
                                    path.find("/Characters/Shells/")!=std::string::npos ||
                                    path.find("/Sparta/MasterMaterials/")!=std::string::npos;
                if(!is_stock_shell && material_has_overrides(value)) {
                    // Active non-shell dynamic effect with parameter overrides.
                    return false;
                }
            }
        }
    }
    return true;
}
bool Appearance::reuse_materials() {
    // A completed effect can replace OverrideMaterials without changing the
    // outfit. Reattach the existing MIDs and their dye targets instead of
    // importing masks and rendering every dye surface again on the game thread.
    // Weak references never retain a previous world; collected resources take
    // the normal rebuild path. Check every reference before changing any slot.
    if(!repair_materials_needed()) return false;
    for(const auto& weak:expected_materials_)
        if(weak.ObjectSerialNumber && !weak.Get()) return false;
    for(const auto& [slot,weak]:control_mids_) {
        if(!weak.Get() || slot<0 || static_cast<size_t>(slot)>=expected_materials_.size() ||
           weak.Get()!=expected_materials_[slot].Get()) return false;
    }
    for(const auto& [id,weak]:dye_targets_) if(!weak.Get()) return false;
    auto* component=component_.Get();
    const auto count=std::max(static_cast<size_t>(overrides(component).Num()),expected_materials_.size());
    for(size_t i=0;i<count;++i)
        material(component,static_cast<int>(i),i<expected_materials_.size()?expected_materials_[i].Get():nullptr);
    if(!materials_match()) throw std::runtime_error("Recovered material read-back failed");
    material_debug=material_snapshot(component,applied_.Get());
    material_debug["controls"]=last_values_;
    material_debug["dye_targets"]=dye_targets_.size();
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
        Call count(component,L"GetNumMaterials",1);count.run();
        const auto slots=count.get<int32_t>();
        if(slots<=0 || slots>128) throw std::runtime_error("Test mesh has invalid material slots");
        int32_t slot=-1;UObject* source=nullptr;
        for(int32_t i=0;i<slots;++i) {
            Call current(component,L"GetMaterial",2);current.set(L"ElementIndex",i);current.run();
            if((source=current.get<UObject*>())) {slot=i;break;}
        }
        if(slot<0) throw std::runtime_error("Test mesh has no material for an effect");
        Call effect(component,L"CreateDynamicMaterialInstance",4);
        effect.set(L"ElementIndex",slot);effect.set(L"SourceMaterial",source);effect.run();
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
static bool is_blocking_camera_state(UObject* state) {
    if(!state) return false;
    auto* klass = state->GetClassPrivate();
    if(!klass) return false;
    std::string name = narrow(klass->GetName());
    if(name.find("Menu") != std::string::npos) return false;
    if(name.find("BoneGate") != std::string::npos ||
       name.find("GateCleansed") != std::string::npos ||
       name.find("Beacon") != std::string::npos ||
       name.find("Traversal") != std::string::npos ||
       name.find("ShellMemory") != std::string::npos ||
       name.find("Teleport") != std::string::npos ||
       name.find("FastTravel") != std::string::npos ||
       name.find("Skydive") != std::string::npos ||
       name.find("Spline") != std::string::npos ||
       name.find("Tarforge") != std::string::npos ||
       name.find("TarredCorpse") != std::string::npos ||
       name.find("Cinematic") != std::string::npos ||
       name.find("Cutscene") != std::string::npos ||
       name.find("Sequence") != std::string::npos) {
        return true;
    }
    return false;
}

static bool is_quest_or_teleport_active(UObject* pc) {
    if(!pc) return false;
    auto* temp_shell_prop = pc->GetPropertyByNameInChain(L"TemporaryShellItemDefinition");
    if(temp_shell_prop && temp_shell_prop->GetElementSize() == sizeof(UObject*)) {
        UObject* temp_shell = nullptr;
        std::memcpy(&temp_shell, reinterpret_cast<const std::byte*>(pc) + temp_shell_prop->GetOffset_Internal(), sizeof(UObject*));
        if(temp_shell) return true;
    }
    auto* temp_weap_prop = pc->GetPropertyByNameInChain(L"TemporaryWeaponItemDefinition");
    if(temp_weap_prop && temp_weap_prop->GetElementSize() == sizeof(UObject*)) {
        UObject* temp_weap = nullptr;
        std::memcpy(&temp_weap, reinterpret_cast<const std::byte*>(pc) + temp_weap_prop->GetOffset_Internal(), sizeof(UObject*));
        if(temp_weap) return true;
    }
    auto* tp_mgr_prop = pc->GetPropertyByNameInChain(L"Teleport Manager");
    if(tp_mgr_prop && tp_mgr_prop->GetElementSize() == sizeof(UObject*)) {
        UObject* tp_mgr = nullptr;
        std::memcpy(&tp_mgr, reinterpret_cast<const std::byte*>(pc) + tp_mgr_prop->GetOffset_Internal(), sizeof(UObject*));
        if(tp_mgr) {
            // CameraState is the teleport warp camera: set only while a gate/beacon warp
            // is actually playing, which is what we must not fight. CurrentDungeonEvent is
            // NOT checked: it stays set the whole time you are inside a dungeon (alongside
            // bIsInDungeon), so treating it as "teleport active" blocked every appearance
            // apply for the entire dungeon, on all shells and on the severed Harbinger. The
            // real cutscene cameras below (PlayerCameraManager.ActiveCameraInstance) cover
            // the bond-quest and warp cases without that persistent over-block.
            auto* tp_cam_prop = tp_mgr->GetPropertyByNameInChain(L"CameraState");
            if(tp_cam_prop && tp_cam_prop->GetElementSize() == sizeof(UObject*)) {
                UObject* tp_cam = nullptr;
                std::memcpy(&tp_cam, reinterpret_cast<const std::byte*>(tp_mgr) + tp_cam_prop->GetOffset_Internal(), sizeof(UObject*));
                if(tp_cam) return true;
            }
        }
    }
    auto* sm_comp_prop = pc->GetPropertyByNameInChain(L"Shell Memory Handler Component");
    if(sm_comp_prop && sm_comp_prop->GetElementSize() == sizeof(UObject*)) {
        UObject* sm_comp = nullptr;
        std::memcpy(&sm_comp, reinterpret_cast<const std::byte*>(pc) + sm_comp_prop->GetOffset_Internal(), sizeof(UObject*));
        if(sm_comp) {
            auto* actor_prop = sm_comp->GetPropertyByNameInChain(L"HandlerActor");
            if(actor_prop && actor_prop->GetElementSize() == sizeof(UObject*)) {
                UObject* actor = nullptr;
                std::memcpy(&actor, reinterpret_cast<const std::byte*>(sm_comp) + actor_prop->GetOffset_Internal(), sizeof(UObject*));
                if(actor) {
                    auto* mem_prop = actor->GetPropertyByNameInChain(L"SpawnedShellMemory");
                    if(mem_prop && mem_prop->GetElementSize() == sizeof(UObject*)) {
                        UObject* mem = nullptr;
                        std::memcpy(&mem, reinterpret_cast<const std::byte*>(actor) + mem_prop->GetOffset_Internal(), sizeof(UObject*));
                        if(mem) return true;
                    }
                }
            }
        }
    }
    auto* cam_mgr_prop = pc->GetPropertyByNameInChain(L"PlayerCameraManager");
    if(cam_mgr_prop && cam_mgr_prop->GetElementSize() == sizeof(UObject*)) {
        UObject* cam_mgr = nullptr;
        std::memcpy(&cam_mgr, reinterpret_cast<const std::byte*>(pc) + cam_mgr_prop->GetOffset_Internal(), sizeof(UObject*));
        if(cam_mgr) {
            auto* active_inst_prop = cam_mgr->GetPropertyByNameInChain(L"ActiveCameraInstance");
            if(active_inst_prop && active_inst_prop->GetElementSize() == sizeof(UObject*)) {
                UObject* active_inst = nullptr;
                std::memcpy(&active_inst, reinterpret_cast<const std::byte*>(cam_mgr) + active_inst_prop->GetOffset_Internal(), sizeof(UObject*));
                if(active_inst) {
                    auto* state_prop = active_inst->GetPropertyByNameInChain(L"CameraState");
                    if(state_prop && state_prop->GetElementSize() == sizeof(UObject*)) {
                        UObject* active_state = nullptr;
                        std::memcpy(&active_state, reinterpret_cast<const std::byte*>(active_inst) + state_prop->GetOffset_Internal(), sizeof(UObject*));
                        if(is_blocking_camera_state(active_state)) return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool is_traversal_ability_active(UObject* pawn) {
    if(!pawn) return false;
    auto* asc_prop = pawn->GetPropertyByNameInChain(L"AbilitySystemComponent");
    if(!asc_prop || asc_prop->GetElementSize() != sizeof(UObject*)) return false;
    UObject* asc = nullptr;
    std::memcpy(&asc, reinterpret_cast<const std::byte*>(pawn) + asc_prop->GetOffset_Internal(), sizeof(UObject*));
    if(!asc) return false;

    auto* act_prop = asc->GetPropertyByNameInChain(L"ActivatableAbilities");
    if(!act_prop || !act_prop->IsA<FStructProperty>()) return false;
    auto* sp = static_cast<FStructProperty*>(act_prop);
    auto* struct_type = sp->GetStruct().Get();
    if(!struct_type) return false;
    auto* items_field = struct_type->GetPropertyByNameInChain(L"Items");
    if(!items_field || !items_field->IsA<FArrayProperty>()) return false;
    auto* arr = static_cast<FArrayProperty*>(items_field);
    auto* inner = arr->GetInner();
    if(!inner || !inner->IsA<FStructProperty>()) return false;
    auto* spec_struct = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if(!spec_struct) return false;

    auto* ability_prop = spec_struct->GetPropertyByNameInChain(L"Ability");
    auto* active_prop = spec_struct->GetPropertyByNameInChain(L"ActiveCount");
    if(!ability_prop || !active_prop) return false;

    const size_t ab_off = ability_prop->GetOffset_Internal();
    const size_t act_off = active_prop->GetOffset_Internal();
    const void* container = reinterpret_cast<const std::byte*>(asc) + act_prop->GetOffset_Internal();
    FScriptArrayHelper helper(arr, arr->ContainerPtrToValuePtr<void>(container));

    for(int32_t i = 0; i < helper.Num(); ++i) {
        const uint8* spec_ptr = helper.GetRawPtr(i);
        uint8_t active_count = *(spec_ptr + act_off);
        if(active_count > 0) {
            auto* ability = *reinterpret_cast<UObject* const*>(spec_ptr + ab_off);
            if(ability && ability->GetClassPrivate()) {
                std::string ab_name = narrow(ability->GetClassPrivate()->GetName());
                if(ab_name.rfind("GA_Traversal_", 0) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool Appearance::repair_mesh_needed() const {
    auto* component=component_.Get();
    if(!component || component!=observed_component_.Get()) return false;
    auto* pc=observed_controller_.Get();
    if(is_quest_or_teleport_active(pc)) return false;
    auto* pawn=observed_pawn_.Get();
    if(is_traversal_ability_active(pawn)) return false;
    auto* mesh=mesh_asset(component);
    // Only reclaim the stock mesh captured for this component. An unfamiliar
    // replacement can belong to another mod or an unfinished transformation.
    return mesh && mesh!=applied_.Get() && narrow(mesh->GetPathName())==original_;
}
std::string Appearance::ready_to_apply_reason() const {
    auto* pawn=observed_pawn_.Get(); auto* component=observed_component_.Get(); auto* pc=observed_controller_.Get();
    if(!pawn) return "no observed pawn";
    if(!component) return "no observed component";
    if(!pc) return "no observed controller";
    if(read<UObject*>(pc,L"Pawn")!=pawn) return "controller pawn != pawn";
    if(!mesh_asset(component)) return "no mesh asset on component";
    bool in_menu = false;
    auto* handler=read<UObject*>(pc,L"User Interface Handler Component");
    if(handler) {
        try {
            if(read<bool>(handler, L"bIsInGameMenu")
               || read<UObject*>(handler, L"ActiveMenu")
               || read<UObject*>(handler, L"ActiveDisplayMenu")) {
                in_menu = true;
            }
        } catch(...) {}
    }
    if(!in_menu) {
        try {
            Call call(pc, L"IsInGameMenu", 1); call.run();
            if(call.get<bool>()) in_menu = true;
        } catch(...) {}
    }
    if(!in_menu) {
        try { if(read<bool>(pc, L"bShowMouseCursor")) in_menu = true; } catch(...) {}
    }

    if(!in_menu) {
        Call move(pc,L"IsMoveInputIgnored",1); move.run();
        Call look(pc,L"IsLookInputIgnored",1); look.run();
        if(move.get<bool>()) return "move input ignored";
        if(look.get<bool>()) return "look input ignored";
    }
    if(is_quest_or_teleport_active(pc)) return "quest or teleport active";
    if(is_traversal_ability_active(pawn)) return "traversal ability active";
    if(!handler) return "no UI handler component";
    auto* transition=read<UObject*>(handler,L"CurrentTransitionWidget");
    if(transition) { Call shown(transition,L"IsInViewport",1); shown.run(); if(shown.get<bool>()) return "transition widget in viewport"; }
    if(!in_menu) {
        Call animation(component,L"GetAnimInstance",1); animation.run();
        if(auto* anim=animation.get<UObject*>()) {
            Call montage(anim,L"GetCurrentActiveMontage",1); montage.run();
            if(montage.get<UObject*>()) return "anim montage active";
        }
    }
    if(component==component_.Get() && mesh_asset(component)==applied_.Get()) {
        if(!materials_match() && !repair_materials_needed()) return "retained mesh materials mismatch and no repair needed";
    }
    if(repair_mesh_needed()) return "";
    try { material_paths(component); } catch(const std::runtime_error& e) { return std::string("material_paths: ") + e.what(); }
    return "";
}
bool Appearance::ready_to_apply() const {
    return ready_to_apply_reason().empty();
}
bool Appearance::active() const { auto* c=component_.Get(); return c && c==observed_component_.Get() && applied_.Get() && mesh_asset(c)==applied_.Get(); }
#ifdef CSS_INVENTORY_DEV
#endif
bool Appearance::apply(void* engine, const std::string& mesh_path, const std::map<int,std::string>& materials) {
    if(!ready_to_apply()) return false;
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
    if (!is_compatible_skeleton(before, target))
        throw std::runtime_error("Different skeleton: appearance change refused");
    if (before == target && applied_materials_==materials && materials_match()) return true;
    if (before == target && applied_materials_==materials && reuse_materials()) return true;
    const bool returning_to_outfit=applied_.Get()==target && applied_materials_==materials && repair_mesh_needed();
    if (!returning_to_outfit && (component_.Get() != component || before != applied_.Get())) {
        auto materials=material_paths(component);
        attachments_.release();
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
            applied_hidden_.clear();   // a fresh mesh shows every section; see note below
            ground_offset_={}; ground_component_.Reset();   // and resets RelativeLocation; drop the stale heel baseline
            if(materials_match() || reuse_materials()) {
                current_mesh=narrow(target->GetPathName());
                return true;
            }
        }
        reset_controls();
        // SetSkeletalMeshAsset brings up a mesh with every material section shown, so the
        // cached hidden set no longer matches the component. Clearing it makes the follow-up
        // reconcile_sections() (in customize()) re-hide from scratch instead of short-circuiting
        // on a stale want==applied_hidden_. This is why a launchpad/gate that swapped the pawn
        // to Harbinger and back used to drop the outfit's cut sections. Event-only, no frame cost.
        if(before!=target && !returning_to_outfit) { set_mesh(component, target); applied_hidden_.clear(); ground_offset_={}; ground_component_.Reset(); }
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
void Appearance::restore_ground_offset() {
    if(auto* component=ground_component_.Get()) {
        auto location=read<std::array<double,3>>(component,L"RelativeLocation");
        auto next=ground_offset_;
        if(auto original=next.restore(location[2])) {
            location[2]=*original;
            Call move(component,L"K2_SetRelativeLocation",4);
            move.set(L"NewLocation",location);move.set(L"bSweep",false);move.set(L"bTeleport",true);move.run();
            auto readback=read<std::array<double,3>>(component,L"RelativeLocation");
            if(std::abs(readback[2]-location[2])>0.05)
                throw std::runtime_error("World mesh height restoration failed");
        }
        ground_offset_=next;
    }
    ground_component_.Reset();ground_offset_={};
}
void Appearance::set_ground_offset(double offset) {
    if(!GroundOffset::valid(offset)) throw std::runtime_error("Invalid ground offset");
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=applied_.Get()) return;
    if(ground_component_.Get()!=component || offset==0) restore_ground_offset();
    if(offset==0) return;
    auto location=read<std::array<double,3>>(component,L"RelativeLocation");
    ground_component_=component;
    const auto target=ground_offset_.set(location[2],offset);
    if(target==location[2]) return;
    location[2]=target;
    Call move(component,L"K2_SetRelativeLocation",4);
    move.set(L"NewLocation",location);move.set(L"bSweep",false);move.set(L"bTeleport",true);move.run();
    auto readback=read<std::array<double,3>>(component,L"RelativeLocation");
    if(std::abs(readback[2]-location[2])>0.05)
        throw std::runtime_error("World mesh height read-back failed");
}
bool Appearance::restore() {
    restore_ground_offset();
    restore_springs();
    restore_dynamics();
    restore_rig();
    offsets_.release();
    attachments_.release();
    items_.release();
    current_items_.clear(); current_items_identity_.clear();
    // The mesh is going back to the game's own, so nothing CSS hid on it may survive.
    toggle_hidden_.clear(); item_hidden_.clear(); reconcile_sections();
    restore_menu();
    detach_residual_controls();
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
    control_mids_.clear(); dye_targets_.clear(); dye_textures_.clear(); last_values_.clear(); control_outfit_.clear();
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
    // AddChildToCanvas is only ever called here, so this is the complete record of what
    // a build put on screen, in the order it went on, with the position already in page
    // units. Anything that needs to walk the finished page reads this instead of asking
    // the engine to enumerate the canvas: that enumeration is bounded, and a panel wide
    // enough to pass the bound used to take the whole tab down with it.
    struct Placed { UObject* widget; UObject* canvas; double x, y, w, h; };
    std::vector<Placed> placed{};
    std::vector<UObject*> on(UObject* target) const {
        std::vector<UObject*> result;
        for(const auto& p:placed) if(p.canvas==target) result.push_back(p.widget);
        return result;
    }
    // How far down a canvas its contents actually reach. A scrolling list is sized from
    // this, so a page describes its rows and never also has to total their heights.
    double extent_of(UObject* target) const {
        double bottom=0;
        for(const auto& p:placed) if(p.canvas==target) bottom=std::max(bottom,p.y+p.h);
        return bottom;
    }
    void place(UObject* widget, double x, double y, double width, double height) {
        Call add(canvas, L"AddChildToCanvas", 2); add.set(L"content", widget); add.run();
        auto* slot = add.get<UObject*>();
        invoke(slot, L"SetPosition", L"InPosition", Vec2{(x-origin_x)*scale, (y-origin_y)*scale});
        invoke(slot, L"SetSize", L"InSize", Vec2{width*scale, height*scale});
        placed.push_back({widget, canvas, x-origin_x, y-origin_y, width, height});
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
using DyeMipUpdate=void(*)(UObject*,bool);
// The Canvas dye composite (Begin/EndDrawCanvasToRenderTarget) writes only mip 0;
// UE regenerates the lower mips only from UTextureRenderTarget2D::UpdateResourceImmediate,
// which the engine itself calls right after DrawMaterialToRenderTarget. That method is not
// a reflected UFUNCTION, so it cannot be reached through normal reflection. Rather than pin
// its absolute address per game build (every patch relocates the image and breaks it), anchor
// on the reflected CreateRenderTarget2D wrapper, whose own body calls UpdateResourceImmediate(true),
// and follow that relative call to its target. This depends on the UE 5.6.1 codegen of that one
// wrapper, not on the game build, so ordinary content patches no longer break dye. If the pattern
// is ever absent the caller throws and dye stays safely disabled instead of jumping to a wrong address.
DyeMipUpdate resolve_dye_mip_update() {
    auto* module=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if(!module) throw std::runtime_error("Dye mipmaps require a verified Mortal Shell II build");
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) throw std::runtime_error("Unknown game image for dye mipmaps");
    auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(module+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE) throw std::runtime_error("Unknown game image for dye mipmaps");
    const uintptr_t image_size=nt->OptionalHeader.SizeOfImage;
    auto* library=find(L"/Script/Engine.Default__KismetRenderingLibrary");
    auto* create=library ? library->GetFunctionByNameInChain(L"CreateRenderTarget2D") : nullptr;
    if(!create) throw std::runtime_error("Dye mipmaps require a verified Mortal Shell II build");
    const auto* wrapper=reinterpret_cast<const unsigned char*>(create->GetFuncPtr());
    if(wrapper<module || wrapper>=module+image_size) throw std::runtime_error("Dye mipmaps require a verified Mortal Shell II build");
    // UpdateResourceImmediate prologue: push rbx; push rdi; sub rsp,0A8h; mov rax,[rip+GStackGuard]; xor rax,rsp.
    // Bytes 13-16 are the RIP-relative stack-guard displacement and legitimately drift, so they are masked out.
    static constexpr unsigned char sig[]={0x40,0x53,0x57,0x48,0x81,0xec,0xa8,0,0,0,0x48,0x8b,0x05,0,0,0,0,0x48,0x33,0xc4};
    static constexpr bool fixed[]={1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1};
    // The wrapper's call to UpdateResourceImmediate(true) sits a few hundred bytes in; the window stays
    // well short of the adjacent CreateRenderTarget2DArray/Volume wrappers (~0x800 away) so the first
    // match is the 2D one.
    constexpr uintptr_t window=0x600;
    const uintptr_t start=static_cast<uintptr_t>(wrapper-module);
    const uintptr_t limit=std::min<uintptr_t>(start+window,image_size);
    for(uintptr_t at=start; at+10<=limit; ++at) {
        // mov dl,1 ; mov rcx,<reg> ; call rel32   -> this->UpdateResourceImmediate(/*bClearRenderTarget=*/true)
        if(module[at]!=0xb2 || module[at+1]!=0x01 || module[at+2]!=0x48 || module[at+3]!=0x8b) continue;
        if(module[at+4]<0xc8 || module[at+4]>0xcf || module[at+5]!=0xe8) continue;
        int32_t rel; std::memcpy(&rel,module+at+6,sizeof rel);
        const uintptr_t target=at+10+static_cast<uintptr_t>(static_cast<int64_t>(rel));
        if(target>=image_size || target+sizeof(sig)>image_size) continue;
        bool ok=true;
        for(size_t i=0;i<sizeof(sig);++i) if(fixed[i] && module[target+i]!=sig[i]) {ok=false;break;}
        if(!ok) continue;
        return reinterpret_cast<DyeMipUpdate>(const_cast<unsigned char*>(module)+target);
    }
    throw std::runtime_error("Dye mipmaps require a verified Mortal Shell II build");
}
// Resolved once and cached: the native mip-regen entry point, or null when this build/environment
// does not expose it (an off build, or another mod moved the reflected wrapper the scan anchors on).
// Null is not fatal: dye then uses a single-mip texture instead of being disabled, so colour still
// applies. The module scan is deterministic within a session, so a null result will not flip later.
DyeMipUpdate dye_mip_update() {
    static const DyeMipUpdate fn = [] () -> DyeMipUpdate {
        try { return resolve_dye_mip_update(); } catch(...) { return nullptr; }
    }();
    return fn;
}
void update_dye_mips(UObject* target) {
    auto update=dye_mip_update();
    if(!update) return;   // single-mip dye: nothing to regenerate, and colour has already been drawn
    if(!target->IsA(static_cast<UClass*>(find(L"/Script/Engine.TextureRenderTarget2D")))) throw std::runtime_error("Invalid dye render target");
    update(target,false);
}
}
namespace {
// 1.0: the springs that give a body its secondary motion sit on the mesh's post-process
// anim instance, one FAnimNode_SpringBone struct property per node. The blueprint names
// them in compile order (AnimGraphNode_SpringBone, _1, _2 and so on) and a recompile can
// shuffle that, so CSS matches on the bone a node drives and never on the property name.
struct SpringNode {
    std::byte* data=nullptr;
    int32_t stiffness=0, damping=0, max_displacement=0, error_reset=0;
    // Bitfield bools carry their mask in the property, so go through it, not memcpy.
    FBoolProperty *limit=nullptr, *translate[3]={}, *rotate[3]={};
    double get(int32_t at) const { double v; std::memcpy(&v,data+at,sizeof v); return v; }
    void put(int32_t at,double v) const { std::memcpy(data+at,&v,sizeof v); }
    bool flag(FBoolProperty* p) const { return p && p->GetPropertyValueInContainer(data); }
    void set_flag(FBoolProperty* p,bool v) const { if(p) p->SetPropertyValueInContainer(data,v); }
};
template<class Snapshot> Snapshot capture_spring(const SpringNode& node) {
    return {node.get(node.stiffness),node.get(node.damping),node.get(node.max_displacement),
        node.get(node.error_reset),node.flag(node.limit),
        {node.flag(node.translate[0]),node.flag(node.translate[1]),node.flag(node.translate[2])},
        {node.flag(node.rotate[0]),node.flag(node.rotate[1]),node.flag(node.rotate[2])}};
}
template<class Snapshot> void apply_spring(const SpringNode& node,const Snapshot& value) {
    node.put(node.stiffness,value.stiffness); node.put(node.damping,value.damping);
    node.put(node.max_displacement,value.max_displacement); node.put(node.error_reset,value.error_reset);
    node.set_flag(node.limit,value.limit);
    for(size_t axis=0;axis<3;++axis) {
        node.set_flag(node.translate[axis],value.translate[axis]);
        node.set_flag(node.rotate[axis],value.rotate[axis]);
    }
    if(capture_spring<Snapshot>(node)!=value) throw std::runtime_error("Preview spring read-back failed");
}
// SetMorphTarget records a curve on the component whether or not the mesh has a shape by
// that name, and GetMorphTarget reads that same curve straight back, so a read-back proves
// only that the number was stored. Ask the mesh what it actually carries. A UMorphTarget's
// object name is the morph name, which is what CSSImportMesh writes and what an author
// puts in `morph`.
bool mesh_has_morph(UObject* mesh, const std::string& name) {
    if(!mesh) return false;
    auto* p=mesh->GetPropertyByNameInChain(L"MorphTargets");
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Mesh morph target layout mismatch");
    auto* array=static_cast<FArrayProperty*>(p);
    if(array->GetInner()->GetElementSize()!=sizeof(UObject*)) throw std::runtime_error("Mesh morph target array layout mismatch");
    FScriptArrayHelper targets(array,reinterpret_cast<std::byte*>(mesh)+array->GetOffset_Internal());
    if(targets.Num()<0 || targets.Num()>4096) throw std::runtime_error("Invalid morph target count");
    const auto wanted=wide(name);
    for(int i=0;i<targets.Num();++i) {
        UObject* target=nullptr;
        std::memcpy(&target,targets.GetRawPtr(i),sizeof target);
        if(target && target->GetName()==wanted) return true;
    }
    return false;
}
UObject* post_process_instance(UObject* component) {
    if(!component) return nullptr;
    Call call(component,L"GetPostProcessInstance",1); call.run();
    return call.get<UObject*>();
}
std::map<std::string,SpringNode> spring_nodes(UObject* anim) {
    std::map<std::string,SpringNode> out;
    if(!anim) return out;
    auto* node_type=static_cast<UScriptStruct*>(find(L"/Script/AnimGraphRuntime.AnimNode_SpringBone"));
    auto* stiffness=field(node_type,L"SpringStiffness",sizeof(double));
    auto* damping=field(node_type,L"SpringDamping",sizeof(double));
    auto* max_disp=field(node_type,L"MaxDisplacement",sizeof(double));
    auto* err=field(node_type,L"ErrorResetThresh",sizeof(double));
    auto boolprop=[&](const wchar_t* name)->FBoolProperty*{
        auto* p=node_type->GetPropertyByNameInChain(name);
        if(!p || !p->IsA<FBoolProperty>() || p->GetOffset_Internal()<0) throw std::runtime_error("Spring flag layout does not match this build");
        return static_cast<FBoolProperty*>(p);
    };
    auto* limit=boolprop(L"bLimitDisplacement");
    FBoolProperty* trans[3]={boolprop(L"bTranslateX"),boolprop(L"bTranslateY"),boolprop(L"bTranslateZ")};
    FBoolProperty* rot[3]={boolprop(L"bRotateX"),boolprop(L"bRotateY"),boolprop(L"bRotateZ")};
    auto* spring_bone=node_type->GetPropertyByNameInChain(L"SpringBone");
    auto* bone_name=field(find(L"/Script/Engine.BoneReference"),L"BoneName",sizeof(FName));
    if(!spring_bone || !spring_bone->IsA<FStructProperty>() || spring_bone->GetOffset_Internal()<0 ||
       bone_name->GetOffset_Internal()<0 ||
       spring_bone->GetOffset_Internal()+bone_name->GetOffset_Internal()+int32_t(sizeof(FName))>node_type->GetPropertiesSize())
        throw std::runtime_error("Spring node layout does not match this build");
    const int32_t name_at=spring_bone->GetOffset_Internal()+bone_name->GetOffset_Internal();
    auto* type=anim->GetClassPrivate();
    if(!type) throw std::runtime_error("Animation instance has no class");
    for(auto* p:type->ForEachProperty()) {
        if(!p->IsA<FStructProperty>() || p->GetArrayDim()!=1 || p->GetOffset_Internal()<0) continue;
        if(static_cast<FStructProperty*>(p)->GetStruct().Get()!=node_type) continue;
        if(p->GetElementSize()!=node_type->GetPropertiesSize()) throw std::runtime_error("Spring node size does not match this build");
        auto* data=reinterpret_cast<std::byte*>(anim)+p->GetOffset_Internal();
        FName bone; std::memcpy(&bone,data+name_at,sizeof bone);
        auto name=narrow(bone.ToString());
        if(name.empty() || name=="None") continue;
        if(out.size()>=256) throw std::runtime_error("Spring node count exceeds bound");
        out.emplace(std::move(name),SpringNode{data,stiffness->GetOffset_Internal(),damping->GetOffset_Internal(),
            max_disp->GetOffset_Internal(),err->GetOffset_Internal(),limit,
            {trans[0],trans[1],trans[2]},{rot[0],rot[1],rot[2]}});
    }
    return out;
}
#include "engine_dynamics.inl"
}
void WornItems::release() {
    for(auto& worn:worn_) if(auto* component=worn.component.Get()) {
        Call owner(component,L"GetOwner",1); owner.run();
        Call destroy(component,L"K2_DestroyComponent",1); destroy.set(L"Object",owner.get<UObject*>()); destroy.run();
    }
    worn_.clear(); body_.Reset(); identity_.clear();
}
std::vector<std::string> WornItems::ids() const {
    std::vector<std::string> result;
    for(const auto& worn:worn_) result.push_back(worn.id);
    return result;
}
void WornItems::sync_morph(const std::string& morph, float weight) {
    auto name=FName(wide(morph).c_str(),FNAME_Add);
    for(auto& worn:worn_) {
        if(auto* comp=worn.component.Get()) {
            if(auto* mesh=mesh_asset(comp)) {
                if(mesh_has_morph(mesh,morph)) {
                    Call set(comp,L"SetMorphTarget",3);
                    set.set(L"MorphTargetName",name);
                    set.set(L"Value",weight);
                    set.set(L"bRemoveZeroWeight",false);
                    set.run();
                }
            }
        }
    }
}
void WornItems::sync_morphs(const std::map<std::string, float>& driven_morphs) {
    for(const auto& [morph,weight]:driven_morphs) sync_morph(morph,weight);
}
std::set<int> WornItems::update(UObject* body,const std::string& identity,const std::vector<Item>& items) {
    std::set<int> hidden;
    // A component the game threw away with the pawn leaves a dead handle behind, so a
    // rebuild is also how CSS recovers from one rather than clinging to it.
    const bool stale=std::any_of(worn_.begin(),worn_.end(),[](const auto& worn){return !worn.component.Get();});
    if(!body) { release(); return hidden; }
    if(body_.Get()!=body || identity_!=identity || stale) {
        release();
        body_=body; identity_=identity;
        // Low order first, so an author reads the list the way it stacks.
        std::vector<const Item*> ordered;
        for(const auto& entry:items) if(entry.slot!=ItemSlot::Body) ordered.push_back(&entry);
        std::stable_sort(ordered.begin(),ordered.end(),[](const Item* a,const Item* b){return a->order<b->order;});
        Call owner(body,L"GetOwner",1); owner.run(); auto* actor=owner.get<UObject*>();
        if(!actor) throw std::runtime_error("The character has no actor to attach items to");
        Call transform(find(L"/Script/Engine.Default__KismetMathLibrary"),L"MakeTransform",4);
        transform.set(L"Location",std::array<double,3>{}); transform.set(L"Rotation",std::array<double,3>{});
        transform.set(L"Scale",std::array<double,3>{1,1,1}); transform.run();
        auto* result=transform.param(L"ReturnValue");
        auto copy_transform=[&](Call& target) {
            auto* input=target.param(L"RelativeTransform");
            if(!input->SameType(result) || input->GetElementSize()!=result->GetElementSize())
                throw std::runtime_error("Item transform layout mismatch");
            input->CopyCompleteValue(target.data(input),transform.data(result));
        };
        try {
            for(const Item* entry:ordered) {
                AssetLoadRoots roots;
                auto* mesh=load(entry->mesh); roots.keep(mesh);
                if(!mesh->IsA(static_cast<UClass*>(find(L"/Script/Engine.SkeletalMesh"))))
                    throw std::runtime_error("Item asset is not a skeletal mesh: "+entry->id);
                if(body_.Get()!=body) throw std::runtime_error("The character changed while items were loading");
                Call add(actor,L"AddComponentByClass",5);
                add.set(L"Class",find(L"/Script/Engine.SkeletalMeshComponent"));
                add.set(L"bManualAttachment",true); add.set(L"bDeferredFinish",true);
                copy_transform(add); add.run();
                auto* component=add.get<UObject*>();
                if(!component) throw std::runtime_error("Could not create the item component: "+entry->id);
                // Recorded before anything else can throw, so a failure still cleans up.
                worn_.push_back({entry->id,WeakObject(component)});
                Call asset(component,L"SetSkeletalMeshAsset",1); asset.set(L"NewMesh",mesh); asset.run();
                // The body drives the pose, and this is set before the component is
                // registered on purpose. Setting it afterwards with bForceUpdate
                // reallocates the follower's transform data on a live component and
                // crashed the game on the next page rebuild, 2026-09-17. The follower
                // runs no animation graph of its own, which is what keeps a dozen cheap.
                Call leader(component,L"SetLeaderPoseComponent",3);
                leader.set(L"NewLeaderBoneComponent",body); leader.set(L"bForceUpdate",true);
                leader.set(L"bInFollowerShouldTickPose",false); leader.run();
                Call collision(component,L"SetCollisionEnabled",1); collision.set(L"NewType",uint8_t{0}); collision.run();
                Call finish(actor,L"FinishAddComponent",3);
                finish.set(L"Component",component); finish.set(L"bManualAttachment",true);
                copy_transform(finish); finish.run();
                Call tick(component,L"SetComponentTickEnabled",1); tick.set(L"bEnabled",false); tick.run();
                Call attach(component,L"K2_AttachToComponent",7);
                attach.set(L"Parent",body); attach.set(L"SocketName",FName(L"None"));
                attach.set(L"LocationRule",uint8_t{0}); attach.set(L"RotationRule",uint8_t{0}); attach.set(L"ScaleRule",uint8_t{0});
                attach.set(L"bWeldSimulatedBodies",false); attach.run();
                if(!attach.get<bool>()) throw std::runtime_error("Item attachment did not complete: "+entry->id);
                for(const auto& [slot,path]:entry->materials) {
                    auto* value=load(path); roots.keep(value);
                    if(!value->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"))))
                        throw std::runtime_error("Item material override is not a material: "+entry->id);
                    material(component,slot,value);
                }
            }
        } catch(...) { release(); throw; }
    }
    for(const auto& entry:items)
        if(entry.slot!=ItemSlot::Body) hidden.insert(entry.hides_sections.begin(),entry.hides_sections.end());
    return hidden;
}
void Appearance::sync_items(const Outfit& outfit,const std::string& variant) {
    const Variant* worn=nullptr;
    for(const auto& v:outfit.variants) if(v.id==variant) worn=&v;
    // Dropping the items also drops what they were covering, or a body would keep a hole
    // in it after the thing filling the hole went away.
    auto drop=[&]{ items_.release(); if(!item_hidden_.empty()) { item_hidden_.clear(); reconcile_sections(); } };
    if(!worn) { drop(); return; }
    auto* component=component_.Get();
    if(!component || !applied_.Get() || mesh_asset(component)!=applied_.Get()) { drop(); return; }
    const auto identity=outfit.id+":"+variant+":"+worn->mesh;
    const auto wanted=items_.update(component,identity,worn->items);
    current_items_=worn->items; current_items_identity_=identity;
    // An item that covers part of the body hides those sections. This is the whole set the
    // worn items want, so dropping an item puts its section back without disturbing a
    // toggle the player set.
    item_hidden_=wanted;
    reconcile_sections();
    items_.sync_morphs(driven_morphs_);
}
int Appearance::lod_count() {
    auto* component=component_.Get(); if(!component) return 1;
    Call count(component,L"GetNumLODs",1); count.run();
    return std::clamp(count.get<int32_t>(),1,16);
}
void Appearance::show_hidden_sections() {
    // Only the toggles' own sections. An item still covering part of the body keeps its
    // section hidden, which is what reconcile_sections works out.
    toggle_hidden_.clear();
    reconcile_sections();
}
// The wardrobe shows a second component, not the one being worn, so every shape has to
// be written to both or the slider moves nothing you can see. Weights live in
// driven_morphs_ precisely so this can replay them.
void Appearance::push_morphs(UObject* component) {
    if(!component) return;
    for(const auto& [morph,weight]:driven_morphs_) {
        Call set(component,L"SetMorphTarget",3);
        set.set(L"MorphTargetName",FName(wide(morph).c_str(),FNAME_Add));
        set.set(L"Value",weight); set.set(L"bRemoveZeroWeight",false); set.run();
    }
    items_.sync_morphs(driven_morphs_);
    menu_items_.sync_morphs(driven_morphs_);
}
void Appearance::reconcile_sections() {
    auto* component=component_.Get();
    if(!component) { applied_hidden_.clear(); return; }
    std::set<int> want=toggle_hidden_;
    want.insert(item_hidden_.begin(),item_hidden_.end());
    if(want==applied_hidden_) return;
    Call count(component,L"GetNumMaterials",1); count.run();
    const int materials=count.get<int>();
    for(int section:want) if(section<0 || section>=materials)
        throw std::runtime_error("Hidden material section is absent on this appearance");
    auto set_shown=[&](int section,bool shown) {
        // Controls address global material slots. INDEX_NONE prevents UE from
        // interpreting the slot as a section index and remapping it at another LOD.
        for(int lod=0;lod<lod_count();++lod) {
            Call set(component,L"ShowMaterialSection",4);
            set.set(L"MaterialID",int32_t(section)); set.set(L"SectionIndex",int32_t{-1});
            set.set(L"bShow",shown); set.set(L"LODIndex",int32_t(lod)); set.run();
            Call readback(component,L"IsMaterialSectionShown",3);
            readback.set(L"MaterialID",int32_t(section)); readback.set(L"LODIndex",int32_t(lod)); readback.run();
            if(readback.get<bool>()!=shown) throw std::runtime_error("Material section read-back failed");
        }
    };
    for(int section:want) if(!applied_hidden_.contains(section)) {
        // Track before writing so cleanup also restores a partially failed change.
        applied_hidden_.insert(section);
        set_shown(section,false);
    }
    const auto previous=applied_hidden_;
    for(int section:previous) if(!want.contains(section)) {
        set_shown(section,true);
        applied_hidden_.erase(section);
    }
}
void Appearance::clear_driven_morphs() {
    for(auto* component:{component_.Get(),menu_component_.Get()})
        if(component) for(const auto& [morph,weight]:driven_morphs_) {
            Call set(component,L"SetMorphTarget",3);
            set.set(L"MorphTargetName",FName(wide(morph).c_str(),FNAME_Add));
            set.set(L"Value",0.f); set.set(L"bRemoveZeroWeight",true); set.run();
        }
    driven_morphs_.clear();
    items_.sync_morphs(driven_morphs_);
    menu_items_.sync_morphs(driven_morphs_);
    formula_offsets_.clear();
}
void Appearance::restore_springs() {
    // Put back what the animation blueprint shipped, not what the package declared as its
    // default: those two are meant to agree, and when they do not the author's asset wins.
    if(!spring_originals_.empty()) try {
        auto nodes=spring_nodes(spring_instance_.Get());
        for(const auto& [bone,o]:spring_originals_) if(auto found=nodes.find(bone); found!=nodes.end()) {
            const auto& n=found->second;
            n.put(n.stiffness,o.stiffness); n.put(n.damping,o.damping);
            n.put(n.max_displacement,o.max_displacement); n.put(n.error_reset,o.error_reset);
            n.set_flag(n.limit,o.limit);
            for(int i=0;i<3;++i) { n.set_flag(n.translate[i],o.translate[size_t(i)]); n.set_flag(n.rotate[i],o.rotate[size_t(i)]); }
        }
    } catch(const std::exception&) { /* The instance went away with the mesh, which restores it anyway. */ }
    spring_originals_.clear(); spring_instance_=nullptr;
}
void Appearance::restore_dynamics() {
    if(auto* instance=dynamics_instance_.Get(); instance && !dynamics_originals_.empty()) {
        const auto nodes=dynamics_nodes(instance);
        bool needs_reset=false;
        for(const auto& [root,original]:dynamics_originals_) {
            const auto node=nodes.find(root);
            if(node==nodes.end()) throw std::runtime_error("Dynamics chain disappeared before restoration");
            needs_reset|=dynamics_reset_required(node->second.capture(),original);
            node->second.apply(original);
        }
        if(needs_reset) reset_dynamics(instance);
    }
    dynamics_originals_.clear(); dynamics_instance_=nullptr;
}
void Appearance::restore_rig() {
    if(auto* instance=body_geometry_instance_.Get(); instance && body_geometry_original_)
        body_geometry_inputs(instance).apply(*body_geometry_original_);
    body_geometry_instance_.Reset(); body_geometry_model_.reset(); body_geometry_morphs_.reset();
    body_geometry_original_.reset(); body_geometry_applied_.reset();
    if(auto* instance=rig_instance_.Get(); instance && rig_original_)
        rig_inputs(instance).apply(*rig_original_);
    rig_original_.reset(); rig_applied_.reset(); rig_instance_.Reset();
    if(auto* instance=body_rig_instance_.Get(); instance && body_rig_original_)
        body_rig_inputs(instance).apply(*body_rig_original_);
    body_rig_original_.reset(); body_rig_applied_.reset(); body_rig_instance_.Reset();
}
void Appearance::restore_menu_physics() {
    if(auto* instance=menu_physics_instance_.Get()) {
        if(menu_body_geometry_original_) body_geometry_inputs(instance).apply(*menu_body_geometry_original_);
        if(menu_rig_original_) rig_inputs(instance).apply(*menu_rig_original_);
        if(menu_body_rig_original_) body_rig_inputs(instance).apply(*menu_body_rig_original_);
        if(!menu_spring_originals_.empty()) {
            const auto nodes=spring_nodes(instance);
            for(const auto& [bone,value]:menu_spring_originals_)
                if(auto found=nodes.find(bone);found!=nodes.end()) apply_spring(found->second,value);
        }
        if(!menu_dynamics_originals_.empty()) {
            const auto nodes=dynamics_nodes(instance);
            bool needs_reset=false;
            for(const auto& [root,value]:menu_dynamics_originals_) if(auto found=nodes.find(root);found!=nodes.end()) {
                needs_reset|=dynamics_reset_required(found->second.capture(),value);
                found->second.apply(value);
            }
            if(needs_reset) reset_dynamics(instance);
        }
    }
    menu_spring_originals_.clear(); menu_dynamics_originals_.clear();
    menu_rig_original_.reset(); menu_rig_applied_.reset(); menu_physics_instance_.Reset();
    menu_body_rig_original_.reset(); menu_body_rig_applied_.reset();
    menu_body_geometry_original_.reset(); menu_body_geometry_applied_.reset();
    if(auto* component=menu_component_.Get(); component && menu_post_process_disabled_) {
        Call set(component,L"SetDisablePostProcessBlueprint",1);
        set.set(L"bInDisablePostProcess",*menu_post_process_disabled_); set.run();
    }
    menu_post_process_disabled_.reset();
}
void Appearance::sync_menu_physics(UObject* component) {
    auto* source=post_process_instance(component_.Get());
    if(!source) { restore_menu_physics(); return; }
    auto* instance=post_process_instance(component);
    if(menu_physics_instance_.Get()!=instance) restore_menu_physics();
    auto* asset=mesh_asset(component);
    auto* authored=asset?read<UObject*>(asset,L"PostProcessAnimBlueprint"):nullptr;
    if(!authored) return;
    if(!menu_post_process_disabled_) {
        Call get(component,L"GetDisablePostProcessBlueprint",1); get.run();
        menu_post_process_disabled_=get.get<bool>();
        Call set(component,L"SetDisablePostProcessBlueprint",1);
        set.set(L"bInDisablePostProcess",false); set.run();
    }
    if(!instance) {
        // The game's preview can retain its main instance when swapping meshes
        // without creating the new mesh's post-process instance. Initialize only
        // this preview once, retaining its existing class override.
        Call init(component,L"SetOverridePostProcessAnimBP",2);
        init.set(L"InPostProcessAnimBlueprint",read<UObject*>(component,L"OverridePostProcessAnimBP"));
        init.set(L"ReinitAnimInstances",true); init.run();
        instance=post_process_instance(component);
        if(!instance) throw std::runtime_error("Preview post-process instance was not created");
    }
    menu_physics_instance_=instance;
    if(rig_original_ && rig_applied_ && rig_instance_.Get()==source) {
        if(menu_rig_applied_!=rig_applied_) {
            const auto to=rig_inputs(instance);
            if(!menu_rig_original_) menu_rig_original_=to.capture();
            to.apply(*rig_applied_);
            menu_rig_applied_=rig_applied_;
        }
    } else if(menu_rig_original_) {
        rig_inputs(instance).apply(*menu_rig_original_);
        menu_rig_original_.reset(); menu_rig_applied_.reset();
    }
    if(body_rig_original_ && body_rig_applied_ && body_rig_instance_.Get()==source) {
        if(menu_body_rig_applied_!=body_rig_applied_) {
            const auto to=body_rig_inputs(instance);
            if(!menu_body_rig_original_) menu_body_rig_original_=to.capture();
            to.apply(*body_rig_applied_);
            menu_body_rig_applied_=body_rig_applied_;
        }
    } else if(menu_body_rig_original_) {
        body_rig_inputs(instance).apply(*menu_body_rig_original_);
        menu_body_rig_original_.reset(); menu_body_rig_applied_.reset();
    }
    if(body_geometry_applied_ && body_geometry_instance_.Get()==source) {
        if(menu_body_geometry_applied_!=body_geometry_applied_) {
            if(instance->GetClassPrivate()!=source->GetClassPrivate())
                throw std::runtime_error("Preview body geometry class differs from player");
            const auto inputs=body_geometry_inputs(instance);
            if(!menu_body_geometry_original_) menu_body_geometry_original_=inputs.capture();
            inputs.apply(*body_geometry_applied_);
            menu_body_geometry_applied_=body_geometry_applied_;
        }
    } else if(menu_body_geometry_original_) {
        body_geometry_inputs(instance).apply(*menu_body_geometry_original_);
        menu_body_geometry_original_.reset(); menu_body_geometry_applied_.reset();
    }
    if(!spring_originals_.empty() || !menu_spring_originals_.empty()) {
        const auto from=spring_nodes(source), to=spring_nodes(instance);
        for(auto it=menu_spring_originals_.begin();it!=menu_spring_originals_.end();) {
            if(spring_instance_.Get()==source && spring_originals_.contains(it->first)) { ++it; continue; }
            if(auto node=to.find(it->first);node!=to.end()) apply_spring(node->second,it->second);
            it=menu_spring_originals_.erase(it);
        }
        if(spring_instance_.Get()==source) for(const auto& [bone,original]:spring_originals_) {
            const auto a=from.find(bone), b=to.find(bone);
            if(a==from.end() || b==to.end()) throw std::runtime_error("Preview spring is missing: "+bone);
            menu_spring_originals_.try_emplace(bone,capture_spring<SpringOriginal>(b->second));
            const auto value=capture_spring<SpringOriginal>(a->second);
            if(capture_spring<SpringOriginal>(b->second)!=value) apply_spring(b->second,value);
        }
    }
    if(!dynamics_originals_.empty() || !menu_dynamics_originals_.empty()) {
        reset_dynamics(instance,false);
        const auto from=dynamics_nodes(source), to=dynamics_nodes(instance);
        bool needs_reset=false;
        auto apply=[&](const DynamicsNode& node,const DynamicsSettings& value) {
            const auto before=node.capture();
            needs_reset|=dynamics_reset_required(before,value);
            if(before!=value) node.apply(value);
        };
        for(auto it=menu_dynamics_originals_.begin();it!=menu_dynamics_originals_.end();) {
            if(dynamics_instance_.Get()==source && dynamics_originals_.contains(it->first)) { ++it; continue; }
            if(auto node=to.find(it->first);node!=to.end()) apply(node->second,it->second);
            it=menu_dynamics_originals_.erase(it);
        }
        if(dynamics_instance_.Get()==source) for(const auto& [root,original]:dynamics_originals_) {
            const auto a=from.find(root), b=to.find(root);
            if(a==from.end() || b==to.end()) throw std::runtime_error("Preview dynamics chain is missing: "+root);
            menu_dynamics_originals_.try_emplace(root,b->second.capture());
            apply(b->second,a->second.capture());
        }
        if(needs_reset) reset_dynamics(instance);
    }
}
void Appearance::sync_body_geometry(UObject* component) {
    auto* instance=post_process_instance(component);
    if(!instance) return;
    if(body_geometry_instance_.Get()!=instance) {
        body_geometry_model_=body_geometry_model(instance);
        body_geometry_original_.reset(); body_geometry_applied_.reset(); body_geometry_morphs_.reset();
        body_geometry_instance_=instance;
        if(body_geometry_model_) {
            for(const auto& name:body_geometry_model_->morphs)
                if(!mesh_has_morph(applied_.Get(),name)) throw std::runtime_error("Body geometry requires missing mesh morph: "+name);
            body_geometry_original_=body_geometry_inputs(instance).capture();
        }
    }
    if(!body_geometry_model_) return;
    std::array<float,6> values{};
    for(size_t i=0;i<values.size();++i) {
        Call get(component,L"GetMorphTarget",2);
        get.set(L"MorphTargetName",FName(wide(body_geometry_model_->morphs[i]).c_str(),FNAME_Add));get.run();
        values[i]=get.get<float>();
    }
    if(body_geometry_morphs_==values) return;
    const auto value=body_geometry_model_->evaluate(values);
    body_geometry_inputs(instance).apply(value);
    body_geometry_applied_=value; body_geometry_morphs_=values;
}

void Appearance::reset_controls() {
    show_hidden_sections();
    restore_springs();
    restore_dynamics();
    restore_rig();
    clear_driven_morphs();
    if(auto* component=component_.Get()) for(const auto& [slot,weak]:control_mids_) {
        if(auto* mid=weak.Get()) {
            Call current(component,L"GetMaterial",2); current.set(L"ElementIndex",slot); current.run();
            if(current.get<UObject*>()==mid) material(component,slot,applied_materials_.contains(slot)?read<UObject*>(mid,L"Parent"):nullptr);
        }
    }
    control_mids_.clear(); dye_targets_.clear(); dye_textures_.clear(); last_values_.clear(); control_outfit_.clear();
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
        auto owned=control_mids_.find(slot);
        UObject* mid=owned==control_mids_.end()?nullptr:owned->second.Get();
        if(parent!=mid) {
            // A gameplay effect or another mod owns unfamiliar dynamic parents.
            if(parent->IsA(dynamic)) continue;
            Call make(component,L"CreateDynamicMaterialInstance",4);
            make.set(L"ElementIndex",slot);make.set(L"SourceMaterial",parent);make.run();
            mid=make.get<UObject*>();
            if(!mid) throw std::runtime_error("Could not create the deformation compatibility material");
            control_mids_[slot]=mid;
        }
        Call set(mid,L"SetScalarParameterValue",2);set.set(L"ParameterName",parameter);set.set(L"Value",0.f);set.run();
        Call readback(mid,L"K2_GetScalarParameterValue",2);readback.set(L"ParameterName",parameter);readback.run();
        if(readback.get<float>()!=0.f) throw std::runtime_error("Deformation parameter read-back failed");
    }
}
void Appearance::customize(const Outfit& outfit,const std::string& variant,const Customization& custom) {
    color_check_valid_=false;   // recaptured below from the first colour CSS actually writes
    const auto& options=outfit.controls_for(variant);
    auto values=control_values(options,custom);
    auto* component=component_.Get();
    if(!component || !applied_.Get() || mesh_asset(component)!=applied_.Get()) throw std::runtime_error("Appearance changed before colors could apply");
    const auto control_identity=outfit.id+":"+variant;
    if(control_outfit_!=control_identity) reset_controls();
    // Dropping a control restores its authored value, including layered parameters.
    // Rebuild from the original material rather than guessing a layer's default.
    if(std::any_of(last_values_.begin(),last_values_.end(),[&](const auto& p){return !values.contains(p.first);})) reset_controls();
    prepare_deformation_materials();
    // Original keeps authored materials, but garment visibility still needs its defaults.
    // Reconcile all controls together so a switch cannot expose another garment's mask.
    try {
        toggle_hidden_=hidden_control_sections(options,values);
        reconcile_sections();
    } catch(...) { reset_controls(); throw; }
    if(values.empty()) { control_outfit_=control_identity; material_debug=material_snapshot(component,applied_.Get()); material_debug["controls"]=Json::object(); material_debug["dye_targets"]=0; remember_materials(); return; }
    // The post-process anim instance is built with the mesh, and a fresh one comes up with
    // the blueprint's own numbers. Notice that rather than quietly losing the player's
    // tuning the first time the shell reloads. Costs nothing until a spring is in use.
    if(!spring_originals_.empty() && (!spring_instance_.Get() || post_process_instance(component)!=spring_instance_.Get())) {
        spring_originals_.clear(); spring_instance_=nullptr;
        for(const auto& control:options.controls) if(control.kind==ControlKind::Spring) last_values_.erase(control.id);
    }
    if(!dynamics_originals_.empty() && (!dynamics_instance_.Get() || post_process_instance(component)!=dynamics_instance_.Get())) {
        dynamics_originals_.clear(); dynamics_instance_=nullptr;
        for(const auto& control:options.controls) if(control.kind==ControlKind::Dynamics) last_values_.erase(control.id);
    }
    if(rig_original_ && (!rig_instance_.Get() || post_process_instance(component)!=rig_instance_.Get())) {
        rig_original_.reset(); rig_applied_.reset(); rig_instance_.Reset();
        for(const auto& control:options.controls) if(control.kind==ControlKind::Rig && !body_rig_control(control)) last_values_.erase(control.id);
    }
    if(body_rig_original_ && (!body_rig_instance_.Get() || post_process_instance(component)!=body_rig_instance_.Get())) {
        body_rig_original_.reset(); body_rig_applied_.reset(); body_rig_instance_.Reset();
        for(const auto& control:options.controls) if(body_rig_control(control)) last_values_.erase(control.id);
    }
    if((body_geometry_model_ || body_geometry_instance_.Get()) && body_geometry_instance_.Get()!=post_process_instance(component))
        last_values_.clear();
    if(values==last_values_) return;
    auto mid_for=[&](int index) {
        Call count(component,L"GetNumMaterials",1); count.run();
        if(index<0 || index>=count.get<int>()) throw std::runtime_error("Color slot is absent on this appearance");
        auto& weak=control_mids_[index];
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
        if(std::any_of(options.controls.begin(),options.controls.end(),[&](const auto& c){return body_rig_control(c) && values.contains(c.id);})) {
            auto* anim=post_process_instance(component);
            if(anim && has_body_rig_inputs(anim)) {
                const auto inputs=body_rig_inputs(anim);
                if(body_rig_instance_.Get()!=anim) { body_rig_original_.reset(); body_rig_instance_=anim; }
                if(!body_rig_original_) body_rig_original_=inputs.capture();
                const auto tuning=body_rig_settings(options.controls,values,*body_rig_original_);
                if(body_rig_applied_!=tuning) inputs.apply(tuning);
                body_rig_applied_=tuning;
            } else if(anim) {
                auto nodes=spring_nodes(anim);
                if(!nodes.empty()) {
                    if(spring_instance_.Get()!=anim) { spring_originals_.clear(); spring_instance_=anim; }
                    for(const auto& control:options.controls) {
                        if(!body_rig_control(control) || !values.contains(control.id)) continue;
                        const auto& val=values.at(control.id);
                        float freq = val[0];
                        float damp_ratio = val[1];
                        float motion = val[2];
                        bool enabled = val[3] == 1.0f;
                        const auto tuning = spring_tuning(freq, damp_ratio);
                        std::vector<std::string> target_bones;
                        if(control.rig) {
                            for(auto r : control.rig->regions) {
                                if(r < body_region_names.size()) target_bones.push_back(body_region_names[r]);
                            }
                        }
                        if(target_bones.empty()) target_bones = control.nodes;
                        for(const auto& bone : target_bones) {
                            auto found = nodes.find(bone);
                            if(found == nodes.end()) continue;
                            const auto& node = found->second;
                            spring_originals_.try_emplace(bone, SpringOriginal{
                                node.get(node.stiffness), node.get(node.damping),
                                node.get(node.max_displacement), node.get(node.error_reset), node.flag(node.limit),
                                {node.flag(node.translate[0]), node.flag(node.translate[1]), node.flag(node.translate[2])},
                                {node.flag(node.rotate[0]), node.flag(node.rotate[1]), node.flag(node.rotate[2])}});
                            const auto& orig = spring_originals_.at(bone);
                            if(!enabled) {
                                node.put(node.stiffness, orig.stiffness * 5.0);
                                node.put(node.damping, orig.damping * 5.0);
                                node.put(node.max_displacement, 0.1);
                            } else {
                                node.put(node.stiffness, tuning.stiffness);
                                node.put(node.damping, tuning.damping);
                                double base_disp = orig.max_displacement > 0.1 ? orig.max_displacement : 2.0;
                                double new_disp = std::clamp(base_disp * double(motion), 0.5, 25.0);
                                node.put(node.max_displacement, new_disp);
                                node.set_flag(node.limit, true);
                            }
                            node.set_flag(node.translate[0], true);
                            node.set_flag(node.translate[1], true);
                            node.set_flag(node.translate[2], true);
                        }
                    }
                }
            }
        }
        auto* library=find(L"/Script/Engine.Default__KismetRenderingLibrary");
        for(const auto& surface:options.surfaces) {
            bool active=false,changed=false;
            for(const auto& [id,file]:surface.layers) {
                active|=values.contains(id);
                changed|=values.contains(id)!=last_values_.contains(id) || (values.contains(id) && last_values_.contains(id) && values.at(id)!=last_values_.at(id));
            }
            if(!changed) continue;
            auto parameter=FName(wide(surface.parameter).c_str(),FNAME_Add);
            auto* first=mid_for(surface.slots.front());
            Call base(read<UObject*>(first,L"Parent"),L"K2_GetTextureParameterValue",2); base.set(L"ParameterName",parameter); base.run();
            auto* original=base.get<UObject*>();
            if(!original) throw std::runtime_error("The selected material has no dyeable base texture");
            UObject* target=original;
            if(active) {
                auto& weak=dye_targets_[surface.id]; target=weak.Get();
                if(!target) {
                    Call create(library,L"CreateRenderTarget2D",8); create.set(L"WorldContextObject",component);
                    create.set(L"Width",surface.resolution); create.set(L"Height",surface.resolution); create.set(L"Format",uint8_t{3});
                    // Auto-mips only when we can regenerate them after the canvas composite. Without that
                    // regen the lower mips stay at the clear colour (a dark body at distance), so where the
                    // mip regen is unreachable we make a single-mip target instead: correct colour at every
                    // distance, only without mip filtering, and dye keeps working.
                    create.set(L"bAutoGenerateMipMaps",dye_mip_update()!=nullptr); create.run(); target=create.get<UObject*>();
                    if(!target) throw std::runtime_error("Could not create the dye texture"); weak=target;
                }
                // 0.4: the render target used to be bound to the materials before the layer
                // textures were imported and drawn. A failed import or draw then left a black
                // dye texture on the body (the random dark, glossy skin). Keep it alive with a
                // load root instead and bind it only after the composite is checked.
                AssetLoadRoots target_root; target_root.keep(target);
                std::vector<std::pair<WeakObject,ControlValue>> layers;
                for(const auto& [id,file]:surface.layers) if(values.contains(id)) {
                    auto& texture=dye_textures_[file];
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
                    if(!canvas) throw std::runtime_error("Could not begin the dye composite");
                    auto draw=[&](UObject* texture,const ControlValue& color,uint8_t blend) {
                        Call call(canvas,L"K2_DrawTexture",9); call.set(L"RenderTexture",texture);
                        call.set(L"ScreenSize",Vec2{double(surface.resolution),double(surface.resolution)}); call.set(L"CoordinateSize",Vec2{1,1});
                        call.set(L"RenderColor",color); call.set(L"BlendMode",blend); call.run();
                    };
                    draw(original,{1,1,1,1},0);
                    for(const auto& [texture,color]:layers) draw(texture.Get(),color,2);
                } catch(...) { end(); throw; }
                end();
                update_dye_mips(target);
                // Readback must succeed, but its RGB may legitimately be black.
                // Fixed nonblack sample points missed sparse UV islands and silently
                // disabled their controls. Unlike ReadRenderTargetPixel's red error
                // sentinel, RawPixelArea returns an empty array when readback fails.
                Call pixels(library,L"ReadRenderTargetRawPixelArea",8);
                pixels.set(L"WorldContextObject",component); pixels.set(L"TextureRenderTarget",target);
                pixels.set(L"MinX",int32_t{0}); pixels.set(L"MinY",int32_t{0});
                pixels.set(L"MaxX",int32_t{1}); pixels.set(L"MaxY",int32_t{1});
                pixels.set(L"bNormalize",false); pixels.run();
                auto* output=pixels.param(L"ReturnValue");
                if(!output->IsA<FArrayProperty>() ||
                   static_cast<FArrayProperty*>(output)->GetInner()->GetElementSize()!=sizeof(ControlValue))
                    throw std::runtime_error("Dye readback array layout mismatch");
                FScriptArrayHelper samples(static_cast<FArrayProperty*>(output),pixels.data(output));
                if(samples.Num()!=1) throw std::runtime_error("Could not read the completed dye texture");
            }
            for(int slot:surface.slots) {
                auto* mid=mid_for(slot);
                Call set(mid,L"SetTextureParameterValue",2); set.set(L"ParameterName",parameter); set.set(L"Value",target); set.run();
                Call readback(mid,L"K2_GetTextureParameterValue",2); readback.set(L"ParameterName",parameter); readback.run();
                if(readback.get<UObject*>()!=target) throw std::runtime_error("Dye texture read-back failed");
            }
        }
        bool dynamics_needs_reset=false;
        for(const auto& control:options.controls) {
            bool active=values.contains(control.id),previous=last_values_.contains(control.id);
            if(!active) continue;
            if(active && previous && values.at(control.id)==last_values_.at(control.id)) continue;
            if(control.kind==ControlKind::Toggle) continue;
            // 1.0: a shape drives a morph target the package cooked into its own mesh.
            // Stock shells carry none and never will, which is checked here rather than
            // left to silently do nothing: every stock mesh reads back zero morph targets.
            if(control.kind==ControlKind::Shape) {
                if(!mesh_has_morph(applied_.Get(),control.morph))
                    throw std::runtime_error("This outfit's mesh has no shape called "+control.morph);
                const float weight=values.at(control.id)[0];
                auto name=FName(wide(control.morph).c_str(),FNAME_Add);
                Call set(component,L"SetMorphTarget",3);
                set.set(L"MorphTargetName",name); set.set(L"Value",weight);
                // Keep a zero weight on the component rather than dropping the curve, so
                // taking the outfit off has something to put back to zero.
                set.set(L"bRemoveZeroWeight",false); set.run();
                Call readback(component,L"GetMorphTarget",2);
                readback.set(L"MorphTargetName",name); readback.run();
                if(std::abs(readback.get<float>()-weight)>.0001f)
                    throw std::runtime_error("Shape weight read-back failed");
                driven_morphs_[control.morph]=weight;
                items_.sync_morph(control.morph,weight);
                menu_items_.sync_morph(control.morph,weight);
                for(const auto& formula:control.formulas) {
                    const double delta=double(weight*formula.multiplier);
                    auto& off=formula_offsets_[formula.target];
                    if(formula.type=="BoneCenterX") off[0]=delta;
                    else if(formula.type=="BoneCenterY") off[1]=delta;
                    else if(formula.type=="BoneCenterZ") off[2]=delta;
                }
                continue;
            }
            if(control.kind==ControlKind::Rig) {
                if(body_rig_control(control)) continue;
                auto* anim=post_process_instance(component);
                if(anim && has_rig_inputs(anim)) {
                    const auto inputs=rig_inputs(anim);
                    const auto tuning=rig_settings(control,values.at(control.id));
                    if(rig_instance_.Get()!=anim) { rig_original_.reset(); rig_instance_=anim; }
                    if(!rig_original_) rig_original_=inputs.capture();
                    inputs.apply(tuning);
                    rig_applied_=tuning;
                } else if(anim) {
                    auto nodes=spring_nodes(anim);
                    if(!nodes.empty()) {
                        const auto& v=values.at(control.id);
                        for(auto& [bname, node] : nodes) {
                            if(bname.find("Hair")!=std::string::npos || bname.find("Ponytail")!=std::string::npos || bname.find("Bangs")!=std::string::npos) {
                                spring_originals_.try_emplace(bname, SpringOriginal{
                                    node.get(node.stiffness), node.get(node.damping),
                                    node.get(node.max_displacement), node.get(node.error_reset), node.flag(node.limit),
                                    {node.flag(node.translate[0]), node.flag(node.translate[1]), node.flag(node.translate[2])},
                                    {node.flag(node.rotate[0]), node.flag(node.rotate[1]), node.flag(node.rotate[2])}});
                                node.put(node.stiffness, double(v[0]));
                                node.put(node.damping, double(v[1]));
                            }
                        }
                    }
                }
                continue;
            }
            // AnimDynamics uses direct solver values. Damping changes require a
            // reset; angular spring forcing and gravity scale update each frame.
            if(control.kind==ControlKind::Dynamics) {
                auto* anim=post_process_instance(component);
                if(!anim) throw std::runtime_error("This outfit has no post-process animation instance");
                reset_dynamics(anim,false);
                if(dynamics_instance_.Get()!=anim) { dynamics_originals_.clear(); dynamics_instance_=anim; }
                const auto nodes=dynamics_nodes(anim);
                const auto tuning=dynamics_settings(control,values.at(control.id));
                // Resolve every requested root before writing any of this control.
                for(const auto& root:control.nodes)
                    if(!nodes.contains(root)) throw std::runtime_error("This outfit has no AnimDynamics chain rooted at "+root);
                for(const auto& root:control.nodes) {
                    const auto& node=nodes.at(root);
                    const auto before=node.capture();
                    dynamics_originals_.try_emplace(root,before);
                    dynamics_needs_reset|=dynamics_reset_required(before,tuning);
                    node.apply(tuning);
                }
                continue;
            }
            // SpringBone has a different model: frequency and damping ratio
            // convert to its second-order translational spring constants.
            if(control.kind==ControlKind::Spring) {
                auto* anim=post_process_instance(component);
                if(!anim) throw std::runtime_error("This outfit's mesh has no animation blueprint to tune");
                if(spring_instance_.Get()!=anim) { spring_originals_.clear(); spring_instance_=anim; }
                auto nodes=spring_nodes(anim);
                const auto& v=values.at(control.id);
                const auto tuning=spring_tuning(v[0],v[1]);
                for(const auto& bone:control.nodes) {
                    auto found=nodes.find(bone);
                    if(found==nodes.end()) throw std::runtime_error("This outfit's skeleton has no spring on "+bone);
                    const auto& node=found->second;
                    // Remember every field on first touch, so a second slider move does not
                    // record CSS's own last write as the author's, and removal is exact.
                    spring_originals_.try_emplace(bone,SpringOriginal{
                        node.get(node.stiffness),node.get(node.damping),
                        node.get(node.max_displacement),node.get(node.error_reset),node.flag(node.limit),
                        {node.flag(node.translate[0]),node.flag(node.translate[1]),node.flag(node.translate[2])},
                        {node.flag(node.rotate[0]),node.flag(node.rotate[1]),node.flag(node.rotate[2])}});
                    node.put(node.stiffness,tuning.stiffness); node.put(node.damping,tuning.damping);
                    if(node.get(node.stiffness)!=tuning.stiffness || node.get(node.damping)!=tuning.damping)
                        throw std::runtime_error("Spring read-back failed");
                    // The travel clamp is what keeps a lively spring on the body. MaxDisplacement
                    // does nothing without its flag, so set both together.
                    if(control.spring_clamp) {
                        node.put(node.max_displacement,double(v[2])); node.set_flag(node.limit,true);
                        if(node.get(node.max_displacement)!=double(v[2]) || !node.flag(node.limit))
                            throw std::runtime_error("Spring travel read-back failed");
                    }
                    const auto& original=spring_originals_.at(bone);
                    const auto axes=spring_axes(control,{original.translate,original.rotate});
                    for(int i=0;i<3;++i) {
                        node.set_flag(node.translate[i],axes.translate[size_t(i)]);
                        node.set_flag(node.rotate[i],axes.rotate[size_t(i)]);
                        if(node.flag(node.translate[i])!=axes.translate[size_t(i)] ||
                           node.flag(node.rotate[i])!=axes.rotate[size_t(i)])
                            throw std::runtime_error("Spring axis read-back failed");
                    }
                    if(control.error_reset>=0) node.put(node.error_reset,control.error_reset);
                }
                continue;
            }
            // 1.0: a choice picks one of the textures the package ships. Same shape as
            // a scalar or vector binding, with SetTextureParameterValueByInfo, which this
            // build reflects alongside the other two.
            if(control.kind==ControlKind::Choice) {
                const int index=std::clamp(int(std::lround(values.at(control.id)[0])),0,int(control.options.size())-1);
                AssetLoadRoots roots;
                auto* texture=load(control.options[index].texture); roots.keep(texture);
                if(!texture) throw std::runtime_error("Choice texture is missing: "+control.options[index].texture);
                for(const auto& binding:control.bindings) {
                    auto* mid=mid_for(binding.slot);
                    auto parameter=FName(wide(binding.parameter).c_str(),FNAME_Add);
                    Call set(mid,L"SetTextureParameterValueByInfo",2);
                    auto* p=set.param(L"ParameterInfo"); auto* info=find(L"/Script/Engine.MaterialParameterInfo");
                    member(set.data(p),p->GetElementSize(),info,L"Name",parameter);
                    member(set.data(p),p->GetElementSize(),info,L"Association",uint8_t(binding.association));
                    member(set.data(p),p->GetElementSize(),info,L"Index",binding.layer);
                    set.set(L"Value",texture); set.run();
                    Call readback(mid,L"K2_GetTextureParameterValueByInfo",2);
                    readback.copy(L"ParameterInfo",set,L"ParameterInfo"); readback.run();
                    if(readback.get<UObject*>()!=texture) throw std::runtime_error("Choice texture read-back failed");
                }
                continue;
            }
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
                if(control.scalar ? std::abs(readback.get<float>()-color[0])>.00001f : readback.get<ControlValue>()!=color)
                    throw std::runtime_error("Color parameter read-back failed");
                // Remember the first colour written as the transition fingerprint (one value, so
                // customization_reset() costs a single reflected read on the slow maintenance tick).
                if(!color_check_valid_) {
                    color_check_mid_=mid; color_check_param_=parameter;
                    color_check_assoc_=uint8_t(binding.association); color_check_layer_=binding.layer;
                    color_check_scalar_=control.scalar; color_check_value_=color; color_check_valid_=true;
                }
            }
        }
        sync_body_geometry(component);
        if(dynamics_needs_reset) reset_dynamics(dynamics_instance_.Get());
        prepare_deformation_materials();
        last_values_=std::move(values); control_outfit_=control_identity;
        material_debug=material_snapshot(component,applied_.Get());
        material_debug["controls"]=last_values_;
        material_debug["dye_targets"]=dye_targets_.size();
        remember_materials();
    } catch(...) { reset_controls(); throw; }
}
// Cheap transition check: re-read the one dye value CSS wrote and see whether it still holds. A
// launchpad/Harbinger gate resets material parameters in place (same MID object), which every
// pointer/aliveness check misses; a divergence here is the only reliable signal that the
// customization was silently reset and needs re-applying. One reflected read; called at ~7 Hz.
bool Appearance::customization_reset() const {
    auto* comp=component_.Get();
    if(!comp || mesh_asset(comp)!=applied_.Get()) return false;   // only while our outfit is worn
    // Colour: re-read the one dye value CSS wrote.
    if(color_check_valid_) if(auto* mid=color_check_mid_.Get()) try {
        Call rb(mid,color_check_scalar_?L"K2_GetScalarParameterValueByInfo":L"K2_GetVectorParameterValueByInfo",2);
        auto* p=rb.param(L"ParameterInfo"); auto* info=find(L"/Script/Engine.MaterialParameterInfo");
        member(rb.data(p),p->GetElementSize(),info,L"Name",color_check_param_);
        member(rb.data(p),p->GetElementSize(),info,L"Association",color_check_assoc_);
        member(rb.data(p),p->GetElementSize(),info,L"Index",color_check_layer_);
        rb.run();
        if(color_check_scalar_) { if(std::abs(double(rb.get<float>())-double(color_check_value_[0]))>.02) return true; }
        else { auto v=rb.get<ControlValue>(); for(int i=0;i<3;++i) if(std::abs(double(v[i])-double(color_check_value_[i]))>.02) return true; }
    } catch(...) {}
    // Body shape: a transition clears the morph-target curves, reverting the sliders. Re-read one
    // driven morph and the body-geometry shape morphs against what CSS set.
    auto morph=[&](const std::string& name)->float {
        Call g(comp,L"GetMorphTarget",2); g.set(L"MorphTargetName",FName(wide(name).c_str(),FNAME_Add)); g.run();
        return g.get<float>();
    };
    if(!driven_morphs_.empty()) try {
        const auto& [name,weight]=*driven_morphs_.begin();
        if(std::abs(double(morph(name))-double(weight))>.02) return true;
    } catch(...) {}
    if(body_geometry_morphs_ && body_geometry_model_) try {
        for(size_t i=0;i<body_geometry_morphs_->size() && i<body_geometry_model_->morphs.size();++i)
            if(std::abs(double(morph(body_geometry_model_->morphs[i]))-double((*body_geometry_morphs_)[i]))>.02) return true;
    } catch(...) {}
    // Hidden sections (clothing the outfit covers): a transition can show them again.
    if(!applied_hidden_.empty()) try {
        const int section=*applied_hidden_.begin();
        Call rb(comp,L"IsMaterialSectionShown",3);
        rb.set(L"MaterialID",int32_t(section)); rb.set(L"LODIndex",int32_t(0)); rb.run();
        if(rb.get<bool>()) return true;   // a section CSS hid is visible again
    } catch(...) {}
    return false;
}
// True while a teleport/gate/traversal is mid-flight. Its falling edge is the moment a gate has
// finished, which is when CSS re-checks and re-applies once - as opposed to polling continuously,
// which would fight a mod's own locomotion-driven visibility.
bool Appearance::transition_active() const {
    return is_quest_or_teleport_active(observed_controller_.Get()) || is_traversal_ability_active(observed_pawn_.Get());
}

}

#include "inventory_ui.inl"

#include "inventory_view.inl"

#include "engine_bridge.inl"

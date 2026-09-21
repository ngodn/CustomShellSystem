// Read game-owned definitions once. Never equip, spawn or initialize a gameplay shell.
static std::string original_string(Call& call) {
    auto* p=call.param(L"ReturnValue");
    if(!p->IsA<FStrProperty>()) throw std::runtime_error("Expected a shell path or name string");
    const auto& chars=static_cast<FString*>(call.data(p))->GetCharArray();
    if(chars.Num()<0 || chars.Num()>2048) throw std::runtime_error("Shell string exceeds bound");
    return chars.Num()?narrow(std::wstring(chars.GetData())):std::string{};
}
static void original_property(Call& call,const wchar_t* parameter,UObject* object,const wchar_t* name) {
    auto* dest=call.param(parameter);
    auto* source=field(object,name,dest->GetElementSize());
    bool compatible=source->SameType(dest);
    // The game constrains these references to its shell class / SkeletalMesh.
    // Kismet accepts UObject references. Widen only within the same soft kind.
    if(!compatible && source->IsA<FSoftClassProperty>() && dest->IsA<FSoftClassProperty>()) {
        auto* from=static_cast<FSoftClassProperty*>(source)->GetMetaClass().Get();
        auto* to=static_cast<FSoftClassProperty*>(dest)->GetMetaClass().Get();
        compatible=from && to && from->IsChildOf(to);
    } else if(!compatible && source->IsA<FSoftObjectProperty>() && dest->IsA<FSoftObjectProperty>() &&
              !source->IsA<FSoftClassProperty>() && !dest->IsA<FSoftClassProperty>()) {
        auto* from=static_cast<FSoftObjectProperty*>(source)->GetPropertyClass().Get();
        auto* to=static_cast<FSoftObjectProperty*>(dest)->GetPropertyClass().Get();
        compatible=from && to && from->IsChildOf(to);
    }
    if(!compatible) throw std::runtime_error("Shell reference type mismatch: "+narrow(name));
    dest->CopyCompleteValue(call.data(dest),reinterpret_cast<std::byte*>(object)+source->GetOffset_Internal());
}
static UObject* original_default(UObject* cls,AssetLoadRoots& roots) {
    if(!cls || !cls->IsA(static_cast<UClass*>(find(L"/Script/CoreUObject.Class"))))
        throw std::runtime_error("Shell definition is not a class");
    roots.keep(cls);
    const auto path=narrow(cls->GetPathName());const auto dot=path.rfind('.');
    if(dot==std::string::npos || !path.ends_with("_C")) throw std::runtime_error("Unexpected shell class path");
    auto* result=load(path.substr(0,dot)+".Default__"+path.substr(dot+1));roots.keep(result);
    if(!result->HasAnyFlags(RF_ClassDefaultObject) || result->GetClassPrivate()!=cls)
        throw std::runtime_error("Shell default object does not match its class");
    return result;
}
Outfit discover_original_shells() {
    auto* settings=find(L"/Script/Sparta.Default__SpartaGameSettings");
    auto* library=find(L"/Script/Engine.Default__KismetSystemLibrary");
    Call names(settings,L"GetShellNames",1);names.run();
    auto* property=names.param(L"ReturnValue");
    if(!property->IsA<FArrayProperty>()) throw std::runtime_error("Shell names are not an array");
    auto* array=static_cast<FArrayProperty*>(property);
    if(!array->GetInner()->IsA<FStrProperty>()) throw std::runtime_error("Shell names are not strings");
    FScriptArrayHelper entries(array,names.data(property));
    if(entries.Num()<1 || entries.Num()>64) throw std::runtime_error("Shell list exceeds bound");
    Outfit outfit;outfit.id=original_shells_id;outfit.name="Use Original Shell";
    outfit.author="Cold Symmetry";outfit.same_skeleton=true;
    outfit.description="Wear an official shell's appearance. Your current shell keeps its abilities and progress.";
    AssetLoadRoots roots;std::set<std::string> ids;
    for(int i=0;i<entries.Num();++i) {
        const auto* name=reinterpret_cast<FString*>(entries.GetRawPtr(i));
        const auto& chars=name->GetCharArray();
        if(chars.Num()<1 || chars.Num()>256) throw std::runtime_error("Invalid shell name");
        const auto label=narrow(std::wstring(chars.GetData()));
        if(label=="Load From Save") continue;
        Call item(settings,L"GetShellItemDefinition",2);item.set(L"Name",*name);item.run();
        auto* definition=original_default(item.get<UObject*>(),roots);
        auto* fragments=definition->GetPropertyByNameInChain(L"Fragments");
        if(!fragments || !fragments->IsA<FArrayProperty>()) throw std::runtime_error("Shell fragments are unavailable");
        auto* fp=static_cast<FArrayProperty*>(fragments);
        if(!fp->GetInner()->IsA<FObjectProperty>() || fp->GetInner()->GetElementSize()!=sizeof(UObject*))
            throw std::runtime_error("Shell fragment layout mismatch");
        FScriptArrayHelper values(fp,reinterpret_cast<std::byte*>(definition)+fragments->GetOffset_Internal());
        if(values.Num()<1 || values.Num()>64) throw std::runtime_error("Shell fragment count exceeds bound");
        std::string class_path;
        for(int n=0;n<values.Num();++n) {
            UObject* fragment{};std::memcpy(&fragment,values.GetRawPtr(n),sizeof(fragment));
            if(!fragment || !fragment->IsA(static_cast<UClass*>(find(L"/Script/Sparta.ItemFragment_SetShellClass")))) continue;
            if(!class_path.empty()) throw std::runtime_error("Duplicate shell class fragment");
            Call convert(library,L"Conv_SoftClassReferenceToString",2);
            original_property(convert,L"SoftClassReference",fragment,L"ShellClass");convert.run();class_path=original_string(convert);
        }
        if(!valid_asset(class_path)) throw std::runtime_error("Shell class reference is missing or invalid: "+label);
        auto* shell=original_default(load(class_path),roots);
        Call mesh(library,L"Conv_SoftObjectReferenceToString",2);
        original_property(mesh,L"SoftObjectReference",shell,L"DefaultMesh");mesh.run();
        Variant variant;variant.mesh=original_string(mesh);
        if(!valid_asset(variant.mesh)) throw std::runtime_error("Default shell mesh is unavailable: "+label);
        auto id=narrow(definition->GetClassPrivate()->GetName());
        constexpr std::string_view prefix="ID_Shell_";
        if(!id.starts_with(prefix) || !id.ends_with("_C")) throw std::runtime_error("Unexpected shell item id");
        id=id.substr(prefix.size(),id.size()-prefix.size()-2);
        std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;});
        if(!valid_id(id) || !ids.insert(id).second) throw std::runtime_error("Invalid or duplicate shell choice");
        variant.id=id;
        Call display(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
        original_property(display,L"InText",definition,L"DisplayName");display.run();variant.name=original_string(display);
        if(variant.name.empty()) variant.name=label;
        Item body;body.id="body";body.name=variant.name;body.mesh=variant.mesh;
        variant.items.push_back(std::move(body));outfit.variants.push_back(std::move(variant));
    }
    if(outfit.variants.empty()) throw std::runtime_error("No official shell appearances found");
    return outfit;
}

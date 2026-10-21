// Read game-owned definitions once. Never equip, spawn or initialize a gameplay shell.
static std::string original_string(Call& call) {
    auto* p=call.param(L"ReturnValue");
    if(!p->IsA<FStrProperty>()) throw std::runtime_error("Expected a shell path or name string");
    const auto& chars=static_cast<FString*>(call.data(p))->GetCharArray();
    if(chars.Num()<0 || chars.Num()>2048) throw std::runtime_error("Shell string exceeds bound");
    return chars.Num()?narrow(std::wstring(chars.GetData())):std::string{};
}
// Copies a reflected soft reference into a Kismet parameter. The game constrains these
// references to its shell class / SkeletalMesh; Kismet accepts UObject references. Widen
// only within the same soft kind.
static void original_copy(Call& call,const wchar_t* parameter,FProperty* source,const void* data) {
    auto* dest=call.param(parameter);
    if(source->GetElementSize()!=dest->GetElementSize()) throw std::runtime_error("Shell reference size mismatch");
    bool compatible=source->SameType(dest);
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
    if(!compatible) throw std::runtime_error("Shell reference type mismatch");
    dest->CopyCompleteValue(call.data(dest),data);
}
static void original_property(Call& call,const wchar_t* parameter,UObject* object,const wchar_t* name) {
    auto* dest=call.param(parameter);
    auto* source=field(object,name,dest->GetElementSize());
    original_copy(call,parameter,source,reinterpret_cast<std::byte*>(object)+source->GetOffset_Internal());
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
static FScriptArrayHelper original_array(UObject* object,const wchar_t* name,FArrayProperty*& property) {
    auto* p=object->GetPropertyByNameInChain(name);
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Shell list is unavailable: "+narrow(name));
    property=static_cast<FArrayProperty*>(p);
    return FScriptArrayHelper(property,reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal());
}
// The settings object holds the shell roster as two parallel arrays: display labels and
// soft class references to the item definitions. CSS reads the references itself and loads
// each class on demand. The game's GetShellItemDefinition only returns definitions that are
// already loaded, and a save that has not unlocked a shell never loads that shell's class,
// so going through it made the whole list vanish for anyone without every shell (1.0.0-beta.5).
// One bad entry is skipped and reported, not fatal: a shell mod that adds an entry the game
// itself cannot resolve should not take the official ones down with it.
// beta.7: a shell's character data is where its socket adjustments live. Read the CharacterId
// it declares, so a look can name its fit shell by tag and CSS can find the table.
static std::string original_character_id(UObject* data) {
    auto* p=field(data,L"CharacterId",sizeof(FName));
    const auto tag=narrow(reinterpret_cast<const FName*>(reinterpret_cast<const std::byte*>(data)+p->GetOffset_Internal())->ToString());
    if(!valid_id(tag) || tag=="None") throw std::runtime_error("Shell character data has no CharacterId");
    return tag;
}
bool original_shade_available(UObject* pawn,const Variant& variant) {
    if(variant.shade_tag.empty()) return true;
    if(!pawn || variant.fit_shell.empty()) return false;
    auto* library=load("/Game/Sparta/Core/Player/BPFL_Player_Equipment.Default__BPFL_Player_Equipment_C");
    AssetLoadRoots roots;roots.keep(library);
    Call unlocked(library,L"IsSkinUnlocked",4);
    unlocked.set(L"EquipmentID",FName(wide(variant.fit_shell).c_str()));
    unlocked.set(L"SkinID",FName(wide(variant.shade_tag).c_str()));
    unlocked.set(L"__WorldContext",pawn);unlocked.run();
    return unlocked.get<bool>();
}
static std::vector<Variant> original_shades(UObject* data,const Variant& base,
        const std::string& definition_path,AssetLoadRoots& roots,std::vector<std::string>& skipped) {
    std::vector<Variant> result;
    if(!data || base.fit_shell.empty()) return result;
    auto* p=data->GetPropertyByNameInChain(L"SkinMaterialsMap");
    if(!p || !p->IsA<FMapProperty>()) return result;
    auto* property=static_cast<FMapProperty*>(p);
    auto* key=property->GetKeyProp();auto* value=property->GetValueProp();
    if(!key->IsA<FStructProperty>() || key->GetSize()!=sizeof(FName) ||
       narrow(static_cast<FStructProperty*>(key)->GetStruct()->GetName())!="GameplayTag" ||
       !value->IsA<FStructProperty>()) throw std::runtime_error("Unexpected shade map types");
    auto* type=static_cast<FStructProperty*>(value)->GetStruct().Get();
    auto* defaults=type->GetPropertyByNameInChain(L"Default");
    if(!defaults || !defaults->IsA<FArrayProperty>() || defaults->GetOffset_Internal()<0 ||
       defaults->GetOffset_Internal()+defaults->GetSize()>value->GetSize())
        throw std::runtime_error("Unexpected shade material array");
    auto* array=static_cast<FArrayProperty*>(defaults);
    if(!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetSize()!=sizeof(UObject*))
        throw std::runtime_error("Unexpected shade material type");
    const auto& layout=property->GetMapLayout();
    if(layout.ValueOffset<key->GetSize() || layout.SetLayout.Size<=0 || layout.SetLayout.Size>4096 ||
       layout.ValueOffset>layout.SetLayout.Size-value->GetSize()) throw std::runtime_error("Unexpected shade map layout");
    auto* map=reinterpret_cast<FScriptMap*>(reinterpret_cast<std::byte*>(data)+p->GetOffset_Internal());
    const auto end=map->GetMaxIndex();
    if(map->Num()<0 || end<map->Num() || end>64) throw std::runtime_error("Shade count exceeds bound");
    for(int i=0;i<end;++i) {
        if(!map->IsValidIndex(i)) continue;
        auto* pair=static_cast<std::byte*>(map->GetData(i,layout));
        const auto tag=narrow(reinterpret_cast<FName*>(pair)->ToString());
        try {
            const auto definition=original_shade_definition(tag);
            if(!definition) {skipped.push_back(base.name+" unsupported shade: "+tag);continue;}
            const std::string suffix(definition->suffix);
            std::string label(definition->fallback_label);
            Variant shade=base;shade.shade_tag=tag;shade.id=base.id+".shade."+suffix;
            std::transform(shade.id.begin(),shade.id.end(),shade.id.begin(),[](unsigned char c){return std::tolower(c);});
            FScriptArrayHelper materials(array,pair+layout.ValueOffset+defaults->GetOffset_Internal());
            if(materials.Num()<1 || materials.Num()>128) throw std::runtime_error("Shade material count exceeds bound");
            for(int slot=0;slot<materials.Num();++slot) {
                UObject* material{};std::memcpy(&material,materials.GetRawPtr(slot),sizeof(material));
                if(!material) continue; // Native null slots retain the mesh's own material.
                const auto path=narrow(material->GetPathName());
                if(!valid_asset(path) || !material->IsA(static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"))))
                    throw std::runtime_error("Invalid shade material");
                shade.materials[slot]=path;
            }
            if(shade.materials.empty()) continue;
            const auto slash=definition_path.rfind('/'),dot=definition_path.find('.',slash);
            if(slash!=std::string::npos && dot!=std::string::npos) {
                const auto stem=definition_path.substr(slash+1,dot-slash-1)+"_"+suffix;
                try {
                    auto* definition=original_default(load(definition_path.substr(0,slash)+"/Skins/"+stem+"."+stem+"_C"),roots);
                    Call display(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
                    original_property(display,L"InText",definition,L"DisplayName");display.run();
                    if(auto localized=original_string(display);!localized.empty()) label=localized;
                } catch(const std::exception& error) {skipped.push_back(base.name+" shade label: "+error.what());}
            }
            shade.original_shade=label;shade.name=base.name+" / "+label;
            shade.items[0].materials=shade.materials;shade.items[0].name=shade.name;
            result.push_back(std::move(shade));
        } catch(const std::exception& error) {skipped.push_back(base.name+" shade "+tag+": "+error.what());}
    }
    return result;
}
Outfit discover_original_shells(std::vector<std::string>& skipped,std::map<std::string,std::string>& character_data) {
    auto* settings=find(L"/Script/Sparta.Default__SpartaGameSettings");
    auto* library=find(L"/Script/Engine.Default__KismetSystemLibrary");
    FArrayProperty* names_property{};FArrayProperty* classes_property{};
    auto names=original_array(settings,L"ShellNames",names_property);
    auto classes=original_array(settings,L"Shells",classes_property);
    if(!names_property->GetInner()->IsA<FStrProperty>()) throw std::runtime_error("Shell names are not strings");
    if(!classes_property->GetInner()->IsA<FSoftClassProperty>()) throw std::runtime_error("Shell definitions are not class references");
    if(names.Num()<1 || names.Num()>64) throw std::runtime_error("Shell list exceeds bound");
    if(names.Num()!=classes.Num()) throw std::runtime_error("Shell names and definitions differ in count");
    Outfit outfit;outfit.id=original_shells_id;outfit.name="Use Original Shell";
    outfit.author="Cold Symmetry";outfit.same_skeleton=true;
    outfit.description="Wear an official shell's appearance. Your current shell keeps its abilities and progress.";
    AssetLoadRoots roots;std::set<std::string> ids;
    for(int i=0;i<names.Num();++i) {
        std::string label;
        try {
            const auto* name=reinterpret_cast<FString*>(names.GetRawPtr(i));
            const auto& chars=name->GetCharArray();
            if(chars.Num()<1 || chars.Num()>256) throw std::runtime_error("Invalid shell name");
            label=narrow(std::wstring(chars.GetData()));
            if(label=="Load From Save") continue;   // a menu entry, not a shell; also caught below by its missing shell class
            Call reference(library,L"Conv_SoftClassReferenceToString",2);
            original_copy(reference,L"SoftClassReference",classes_property->GetInner(),classes.GetRawPtr(i));reference.run();
            const auto definition_path=original_string(reference);
            if(!valid_asset(definition_path)) throw std::runtime_error("Shell definition reference is missing or invalid");
            auto* definition=original_default(load(definition_path),roots);
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
            if(!valid_asset(class_path)) throw std::runtime_error("Shell class reference is missing or invalid");
            auto* shell=original_default(load(class_path),roots);
            Call mesh(library,L"Conv_SoftObjectReferenceToString",2);
            original_property(mesh,L"SoftObjectReference",shell,L"DefaultMesh");mesh.run();
            Variant variant;variant.mesh=original_string(mesh);
            if(!valid_asset(variant.mesh)) throw std::runtime_error("Default shell mesh is unavailable");
            // Optional: without it the look still wears, its gear just keeps the worn shell's fit.
            UObject* data=nullptr;
            try {
                if((data=read<UObject*>(shell,L"CharacterData"))) {
                    variant.fit_shell=original_character_id(data);
                    character_data[variant.fit_shell]=narrow(data->GetPathName());
                }
            } catch(const std::exception& error) { skipped.push_back(label+" socket fit: "+error.what()); }
            auto id=narrow(definition->GetClassPrivate()->GetName());
            constexpr std::string_view prefix="ID_Shell_";
            if(!id.starts_with(prefix) || !id.ends_with("_C")) throw std::runtime_error("Unexpected shell item id: "+id);
            id=id.substr(prefix.size(),id.size()-prefix.size()-2);
            std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;});
            if(!valid_id(id) || !ids.insert(id).second) throw std::runtime_error("Invalid or duplicate shell choice: "+id);
            variant.id=id;
            Call display(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
            original_property(display,L"InText",definition,L"DisplayName");display.run();variant.name=original_string(display);
            if(variant.name.empty()) variant.name=label;
            variant.original_shell=variant.id;variant.original_shade="Original";
            Item body;body.id="body";body.name=variant.name;body.mesh=variant.mesh;
            variant.items.push_back(std::move(body));outfit.variants.push_back(variant);
            try {
                for(auto& shade:original_shades(data,variant,definition_path,roots,skipped))
                    outfit.variants.push_back(std::move(shade));
            } catch(const std::exception& error) {skipped.push_back(label+" shades: "+error.what());}
        } catch(const std::exception& error) {
            skipped.push_back((label.empty()?"#"+std::to_string(i):label)+": "+error.what());
        }
    }
    if(outfit.variants.empty()) throw std::runtime_error("No official shell appearances found");
    return outfit;
}

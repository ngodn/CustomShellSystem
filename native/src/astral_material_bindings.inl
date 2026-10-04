// Included after material and per-slot overlay accessors.
namespace {
bool astral_object_valid(UObject* object) {
    if(!object) return false;
    Call valid(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"IsValid",2);
    valid.set(L"Object",object);valid.run();return valid.get<bool>();
}
UObject* astral_binding_owner(UObject* component) {
    Call owner(component,L"GetOwner",1);owner.run();return owner.get<UObject*>();
}
void astral_global_overlay(UObject* component,UObject* value) {
    Call set(component,L"SetOverlayMaterial",1);set.set(L"NewOverlayMaterial",value);set.run();
    if(read<UObject*>(component,L"OverlayMaterial")!=value)
        throw std::runtime_error("Astral global overlay read-back failed");
}
}
void AstralMaterialBindings::bind(UObject* owner,std::span<const AstralMaterialBinding> inputs) {
    if(!entries_.empty()) throw std::runtime_error("Restore the previous Astral bindings first");
    if(!owner || WeakObject(owner).Get()!=owner || inputs.empty() || inputs.size()>16)
        throw std::runtime_error("Invalid Astral binding owner or component count");
    auto* component_type=static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"));
    std::vector<Entry> prepared;
    AssetLoadRoots roots;
    size_t material_count=0;
    for(const auto& input:inputs) {
        if(!input.component || WeakObject(input.component).Get()!=input.component ||
           !input.component->IsA(component_type) || astral_binding_owner(input.component)!=owner ||
           !input.mesh || WeakObject(input.mesh).Get()!=input.mesh || mesh_asset(input.component)!=input.mesh ||
           input.materials.empty() || input.materials.size()>128 || input.overlays.size()!=input.materials.size())
            throw std::runtime_error("Invalid Astral binding component or slots");
        for(const auto& old:prepared) if(old.component.Get()==input.component)
            throw std::runtime_error("Duplicate Astral binding component");
        const auto defaults=authored_overlays(input.mesh);
        const auto* default_global=read<UObject*>(input.mesh,L"OverlayMaterial");
        if(defaults.size()!=input.materials.size())
            throw std::runtime_error("Astral material slots do not match the mesh");
        Entry entry;
        entry.component=input.component;entry.mesh=input.mesh;
        astral_retain(roots,input.mesh);
        auto retain=[&](UObject* value) {
            if(value) {
                if(WeakObject(value).Get()!=value)
                    throw std::runtime_error("Astral binding material expired");
                astral_retain(roots,value);
            }
            return WeakObject(value);
        };
        entry.originals=material_objects(input.component);
        for(const auto& value:entry.originals) retain(value.Get());
        auto overlays=overlay_slots(input.component);
        for(int i=0;i<overlays.Num();++i) entry.original_overlays.push_back(retain(overlay_at(overlays,i)));
        entry.original_overlay=retain(read<UObject*>(input.component,L"OverlayMaterial"));
        for(size_t i=0;i<input.materials.size();++i) {
            if(!input.materials[i] || WeakObject(input.materials[i]).Get()!=input.materials[i] ||
               (input.overlays[i] && WeakObject(input.overlays[i]).Get()!=input.overlays[i]))
                throw std::runtime_error("Astral prepared material expired");
            if(!dynamic_material(input.materials[i]) || (input.overlays[i] && !dynamic_material(input.overlays[i])))
                throw std::runtime_error("Astral binding requires prepared private MIDs");
            // A null component overlay falls back to the asset's overlay. An
            // authored fabric pass therefore requires its own adapted MID.
            if((defaults[i] || default_global) && !input.overlays[i])
                throw std::runtime_error("Astral binding lacks an authored overlay companion");
            entry.materials.push_back(retain(input.materials[i]));
            entry.overlays.push_back(retain(input.overlays[i]));
            material_count+=1+size_t(input.overlays[i]!=nullptr);
            if(material_count>128) throw std::runtime_error("Astral binding material count exceeds bound");
        }
        prepared.push_back(std::move(entry));
    }
    // All inputs and rollback references are protected before the first write.
    owner_=owner;entries_=std::move(prepared);retained_.take(roots);
    try {
        for(auto& entry:entries_) {
            auto* component=entry.component.Get();
            if(!component || owner_.Get()!=owner || astral_binding_owner(component)!=owner ||
               mesh_asset(component)!=entry.mesh.Get())
                throw std::runtime_error("Astral binding owner or mesh changed");
            for(size_t i=0;i<entry.materials.size();++i) {
                entry.material_writes=i+1;
                material(component,int(i),entry.materials[i].Get());
            }
            // Keep any pre-existing trailing slots until rollback. They are
            // outside this mesh's material count and must not be discarded.
            if(overlay_slots(component).Num()<int(entry.overlays.size()))
                resize_overlay_slots(component,int(entry.overlays.size()));
            auto overlays=overlay_slots(component);
            for(size_t i=0;i<entry.overlays.size();++i) {
                auto* value=entry.overlays[i].Get();
                entry.overlay_writes=i+1;
                std::memcpy(overlays.GetRawPtr(int(i)),&value,sizeof(value));
            }
            entry.global_cleared=true;
            astral_global_overlay(component,nullptr);
            refresh_overlay_slots(component);
        }
        if(!intact()) throw std::runtime_error("Astral material binding read-back failed");
    } catch(...) {
        restore();
        throw;
    }
}
bool AstralMaterialBindings::intact() const {
    auto* owner=owner_.Get();
    if(!owner || entries_.empty()) return false;
    for(const auto& entry:entries_) {
        auto* component=entry.component.Get();
        if(!component || astral_binding_owner(component)!=owner || mesh_asset(component)!=entry.mesh.Get() ||
           read<UObject*>(component,L"OverlayMaterial")) return false;
        const auto actual=material_objects(component);
        auto overlays=overlay_slots(component);
        if(actual.size()<entry.materials.size() || overlays.Num()<int(entry.overlays.size())) return false;
        for(size_t i=0;i<entry.materials.size();++i) {
            if(!entry.materials[i].Get() || actual[i].Get()!=entry.materials[i].Get() ||
               overlay_at(overlays,int(i))!=entry.overlays[i].Get()) return false;
        }
    }
    return true;
}
bool AstralMaterialBindings::restore() noexcept {
    bool restored=true;
    for(auto& entry:entries_) try {
        auto* component=entry.component.Get();
        if(!astral_object_valid(component)) continue;
        if(!owner_.Get() || astral_binding_owner(component)!=owner_.Get())
            continue;
        const bool same_mesh=mesh_asset(component)==entry.mesh.Get();
        const auto actual=material_objects(component);
        bool superseded=read<UObject*>(component,L"OverlayMaterial")!=nullptr;
        for(size_t i=0;i<entry.material_writes;++i) {
            if(i>=actual.size() || !entry.materials[i].Get() || actual[i].Get()!=entry.materials[i].Get()) {
                superseded=true;continue;
            }
            material(component,int(i),same_mesh && i<entry.originals.size()?entry.originals[i].Get():nullptr);
        }
        auto overlays=overlay_slots(component);
        bool changed=false;
        for(size_t i=0;i<entry.overlay_writes;++i) {
            if(int(i)>=overlays.Num() || overlay_at(overlays,int(i))!=entry.overlays[i].Get() ||
               (!same_mesh && !entry.overlays[i].Get())) {
                superseded=true;continue;
            }
            auto* original=same_mesh && i<entry.original_overlays.size()?entry.original_overlays[i].Get():nullptr;
            std::memcpy(overlays.GetRawPtr(int(i)),&original,sizeof(original));changed=true;
        }
        if(same_mesh && !superseded && entry.overlay_writes) {
            // Another writer may have added overlays outside our owned range.
            for(int i=int(entry.overlays.size());i<overlays.Num();++i)
                if(overlay_at(overlays,i)!=(size_t(i)<entry.original_overlays.size()?entry.original_overlays[size_t(i)].Get():nullptr))
                    superseded=true;
        }
        if(same_mesh && !superseded && entry.overlay_writes) {
            resize_overlay_slots(component,int(entry.original_overlays.size()));
            overlays=overlay_slots(component);
            for(size_t i=entry.overlay_writes;i<entry.original_overlays.size();++i) {
                auto* original=entry.original_overlays[i].Get();
                std::memcpy(overlays.GetRawPtr(int(i)),&original,sizeof(original));
            }
            changed=true;
        }
        if(same_mesh && !superseded && entry.global_cleared) astral_global_overlay(component,entry.original_overlay.Get());
        if(changed) refresh_overlay_slots(component);
    } catch(...) { restored=false; }
    entries_.clear();owner_=WeakObject{};retained_.release();
    return restored;
}

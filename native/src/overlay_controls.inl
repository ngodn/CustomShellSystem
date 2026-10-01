// Included after the reflected material helpers. UE 5.6.1 has no per-slot UFUNCTION.
static FScriptArrayHelper overlay_slots(UObject* component) {
    auto* p=optional_field(component,L"MaterialSlotsOverlayMaterial");
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Overlay slots are unavailable");
    auto* a=static_cast<FArrayProperty*>(p);
    if(!!(a->GetArrayFlags() & EArrayPropertyFlags::UsesMemoryImageAllocator))
        throw std::runtime_error("Frozen overlay arrays are unsupported");
    if(!a->GetInner()->IsA<FObjectProperty>() || a->GetInner()->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Overlay slot layout mismatch");
    FScriptArrayHelper slots(a,reinterpret_cast<std::byte*>(component)+p->GetOffset_Internal());
    if(slots.Num()<0 || slots.Num()>128) throw std::runtime_error("Overlay slot count exceeds limit");
    return slots;
}
static void resize_overlay_slots(UObject* component,int count) {
    auto checked=overlay_slots(component);
    if(count<0 || count>128) throw std::runtime_error("Overlay slot count exceeds limit");
    auto* property=optional_field(component,L"MaterialSlotsOverlayMaterial");
    auto* array=reinterpret_cast<FScriptArray*>(reinterpret_cast<std::byte*>(component)+property->GetOffset_Internal());
    // The pinned runtime exports the ordinary allocator, not the SDK helper's
    // optional memory-image allocator. Object-pointer slots need only zeroing.
    if(count>checked.Num()) array->AddZeroed(count-checked.Num(),sizeof(UObject*),alignof(UObject*));
    else if(count<checked.Num()) array->Remove(count,checked.Num()-count,sizeof(UObject*),alignof(UObject*));
}
static UObject* overlay_at(FScriptArrayHelper& slots,int index) {
    UObject* value{};
    if(index>=0 && index<slots.Num()) std::memcpy(&value,slots.GetRawPtr(index),sizeof(value));
    return value;
}
static std::vector<UObject*> authored_overlays(UObject* mesh) {
    Call defaults(mesh,L"GetMaterials",1); defaults.run();
    auto* p=defaults.param(L"ReturnValue");
    if(!p->IsA<FArrayProperty>()) throw std::runtime_error("Default material slots are unavailable");
    auto* a=static_cast<FArrayProperty*>(p);
    auto* overlay=field(find(L"/Script/Engine.SkeletalMaterial"),L"OverlayMaterialInterface",sizeof(UObject*));
    if(overlay->GetOffset_Internal()<0 || overlay->GetOffset_Internal()+sizeof(UObject*)>size_t(a->GetInner()->GetElementSize()))
        throw std::runtime_error("Default overlay layout mismatch");
    FScriptArrayHelper slots(a,defaults.data(p));
    if(slots.Num()<0 || slots.Num()>128) throw std::runtime_error("Overlay slot count exceeds limit");
    std::vector<UObject*> values(slots.Num());
    for(int i=0;i<slots.Num();++i)
        std::memcpy(&values[i],slots.GetRawPtr(i)+overlay->GetOffset_Internal(),sizeof(UObject*));
    return values;
}
static void refresh_overlay_slots(UObject* component) {
    // Both calls complete on the game thread before deferred scene-proxy updates.
    // This retains the exact stored distance (including zero/default) and global
    // effect material. MID parent shaders are unchanged, so no new PSO is needed.
    const float distance=read<float>(component,L"OverlayMaterialMaxDrawDistance");
    if(!std::isfinite(distance)) throw std::runtime_error("Overlay distance is not finite");
    Call refresh(component,L"SetOverlayMaterialMaxDrawDistance",1);
    refresh.set(L"InMaxDrawDistance",distance==0.f?1.f:0.f); refresh.run();
    refresh.set(L"InMaxDrawDistance",distance); refresh.run();
    if(read<float>(component,L"OverlayMaterialMaxDrawDistance")!=distance)
        throw std::runtime_error("Overlay distance restore failed");
}
void OverlayControls::attach(UObject* component,UObject* mesh) {
    if(component_.Get()==component && mesh_.Get()==mesh) return;
    release();
    if(!component || !mesh || mesh_asset(component)!=mesh) throw std::runtime_error("Overlay appearance changed");
    original_count_=overlay_slots(component).Num();
    component_=component; mesh_=mesh;
}
bool OverlayControls::bind(int index,UObject* value,bool refresh) {
    auto* component=component_.Get();
    if(!component || mesh_asset(component)!=mesh_.Get()) throw std::runtime_error("Overlay appearance changed");
    auto slots=overlay_slots(component);
    if(index<0 || index>=128) throw std::runtime_error("Overlay slot exceeds limit");
    auto& entry=entries_.at(index);
    if(overlay_at(slots,index)==value) {
        if(entry.bound.Get()!=value) entry.bound=value;
        return false;
    }
    if(slots.Num()<=index) resize_overlay_slots(component,index+1);
    std::memcpy(slots.GetRawPtr(index),&value,sizeof(value));
    entry.bound=value;
    if(refresh) refresh_overlay_slots(component);
    return true;
}
void OverlayControls::prepare(UObject* component,UObject* mesh) {
    attach(component,mesh);
    if(prepared_) return;
    const auto defaults=authored_overlays(mesh);
    for(size_t i=0;i<defaults.size();++i) if(defaults[i]) mid_for(component,mesh,int(i));
    prepared_=true;
}
UObject* OverlayControls::mid_for(UObject* component,UObject* mesh,int index) {
    attach(component,mesh);
    auto slots=overlay_slots(component);
    if(auto found=entries_.find(index);found!=entries_.end()) {
        auto* mid=found->second.mid.Get();
        auto* actual=overlay_at(slots,index);
        if(!mid) throw std::runtime_error("Overlay control material expired");
        if(actual!=found->second.bound.Get() || !actual) {
            if(actual!=found->second.original.Get() && !(found->second.detached && !actual))
                throw std::runtime_error("Overlay control no longer owns this slot");
        }
        auto* effect=read<UObject*>(component,L"OverlayMaterial");
        bind(index,effect?effect:mid);
        found->second.detached=false;
        return mid;
    }
    // Validate the authored slot even if a component override already exists.
    const auto defaults=authored_overlays(mesh);
    if(index<0 || size_t(index)>=defaults.size()) throw std::runtime_error("Overlay slot is absent on this appearance");
    auto* parent=defaults[index];
    auto* original=overlay_at(slots,index);
    if(original) parent=original;
    if(!parent || dynamic_material(parent)) throw std::runtime_error("Wait for the temporary overlay effect before coloring");
    roots_.keep(original); roots_.keep(parent);
    Call make(find(L"/Script/Engine.Default__KismetMaterialLibrary"),L"CreateDynamicMaterialInstance",5);
    make.set(L"WorldContextObject",component); make.set(L"Parent",parent); make.run();
    auto* mid=make.get<UObject*>();
    if(!mid) throw std::runtime_error("Could not create the overlay control material");
    roots_.keep(mid);
    entries_.emplace(index,Entry{WeakObject(original),WeakObject(mid),{}});
    auto* effect=read<UObject*>(component,L"OverlayMaterial");
    bind(index,effect?effect:mid);
    return mid;
}
void OverlayControls::share(UObject* component,UObject* mesh,const OverlayControls& source) {
    if(source.entries_.empty()) { release(); return; }
    attach(component,mesh);
    if(std::any_of(entries_.begin(),entries_.end(),[&](const auto& entry){return !source.entries_.contains(entry.first);})) {
        release(); attach(component,mesh);
    }
    sync();
    for(const auto& [index,entry]:source.entries_) {
        auto* mid=entry.mid.Get();
        if(!mid) throw std::runtime_error("Source overlay material expired");
        auto slots=overlay_slots(component);
        auto* actual=overlay_at(slots,index);
        if(auto old=entries_.find(index);old!=entries_.end()) {
            if(!actual || actual!=old->second.bound.Get()) continue;
            if(old->second.mid.Get()==mid) continue;
            roots_.keep(mid); old->second.mid=mid;
        } else {
            if(dynamic_material(actual)) continue;
            roots_.keep(actual); roots_.keep(mid);
            entries_.emplace(index,Entry{WeakObject(actual),WeakObject(mid),{}});
        }
        auto* effect=read<UObject*>(component,L"OverlayMaterial");
        bind(index,effect?effect:mid);
    }
}
void OverlayControls::sync() {
    if(entries_.empty()) return;
    auto* component=component_.Get();
    if(!component) { release(); return; }
    if(mesh_asset(component)!=mesh_.Get()) { detach(); return; }
    auto slots=overlay_slots(component);
    // Explicit component overlays are game requests, whether temporary or
    // permanent. Null falls back to this outfit's fabric, not another asset's
    // defaults. The component array retains the forwarded effect for GC.
    auto* effect=read<UObject*>(component,L"OverlayMaterial");
    bool changed=false;
    for(auto& [index,entry]:entries_) {
        auto* actual=overlay_at(slots,index);
        auto* mid=entry.mid.Get();
        // A reset can return our slot to its captured original. Rebind only that
        // known state; another mod's or the game's replacement remains untouched.
        if(mid && ((actual && actual==entry.bound.Get()) || actual==entry.original.Get() || (entry.detached && !actual))) {
            changed=bind(index,effect?effect:mid,false) || changed;
            entry.detached=false;
        }
    }
    if(changed) refresh_overlay_slots(component);
}
void OverlayControls::detach() {
    if(auto* component=component_.Get();component && !entries_.empty()) {
        auto slots=overlay_slots(component);
        bool changed=false;
        for(auto& [index,entry]:entries_) {
            auto* bound=entry.bound.Get();
            if(!bound || overlay_at(slots,index)!=bound) continue;
            entry.detached=true;
            entry.bound.Reset();
            auto* original=mesh_asset(component)==mesh_.Get()?entry.original.Get():nullptr;
            std::memcpy(slots.GetRawPtr(index),&original,sizeof(original)); changed=true;
        }
        if(changed) {
            int count=slots.Num();
            while(count>original_count_ && !overlay_at(slots,count-1)) --count;
            if(count!=slots.Num()) resize_overlay_slots(component,count);
            refresh_overlay_slots(component);
        }
    }
}
void OverlayControls::release() {
    detach();
    entries_.clear(); roots_.release(); component_.Reset(); mesh_.Reset(); original_count_=0; prepared_=false;
}

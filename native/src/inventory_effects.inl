namespace {
double menu_seconds(UObject* component,const wchar_t* clock) {
    Call time(find(L"/Script/Engine.Default__GameplayStatics"),clock,2);
    time.set(L"WorldContextObject",component);time.run();return time.get<double>();
}

UObject* menu_material_at(UObject* component,int slot) {
    Call get(component,L"GetMaterial",2);
    get.set(L"ElementIndex",slot);get.run();return get.get<UObject*>();
}

inventory_effects::Driver menu_flow_driver(UObject* source) {
    auto* instance=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstance"));
    for(int depth=0;source && depth<32;++depth) {
        if(!source->IsA(instance)) return inventory_effects::driver(narrow(source->GetPathName()));
        source=read<UObject*>(source,L"Parent");
    }
    return inventory_effects::Driver::none;
}

float menu_scalar(UObject* mid,const wchar_t* parameter) {
    Call get(mid,L"K2_GetScalarParameterValue",2);
    get.set(L"ParameterName",FName(parameter,FNAME_Add));get.run();return get.get<float>();
}

FBoolProperty* menu_bool(UObject* owner,const wchar_t* name) {
    auto* property=optional_field(owner,name);
    if(!property || !property->IsA<FBoolProperty>() || property->GetArrayDim()!=1) return nullptr;
    return static_cast<FBoolProperty*>(property);
}

void set_menu_cloth_blend(UObject* component,float value) {
    auto* p=field(component,L"ClothBlendWeight",sizeof(float));
    if(!p->IsA<FFloatProperty>()) throw std::runtime_error("Inventory cloth blend layout mismatch");
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(component)+p->GetOffset_Internal(),&value);
}

struct MenuClothTick {
    std::byte* data=nullptr;
    FBoolProperty* pause=nullptr;
    const void* vtable=nullptr;
};

MenuClothTick menu_cloth_tick(UObject* component) {
    auto* base=find_optional(L"/Script/Engine.TickFunction");
    auto* tick=find_optional(L"/Script/Engine.SkeletalMeshComponentClothTickFunction");
    if(!base || !tick || !base->IsA<UScriptStruct>() || !tick->IsA<UScriptStruct>() ||
       static_cast<UStruct*>(tick)->GetSuperStruct()!=base) return {};
    auto* flag=menu_bool(base,L"bTickEvenWhenPaused");
    auto* factory=optional_field(component,L"ClothingSimulationFactory");
    auto* threshold=optional_field(component,L"TeleportDistanceThreshold");
    if(!flag || !factory || !threshold || !factory->IsA<FClassProperty>() || !threshold->IsA<FFloatProperty>() ||
       factory->GetElementSize()!=sizeof(UObject*) ||
       threshold->GetElementSize()!=sizeof(float)) return {};
    const int base_size=static_cast<UStruct*>(base)->GetPropertiesSize();
    const int offset=inventory_effects::cloth_tick_offset(factory->GetOffset_Internal()+sizeof(UObject*),
        threshold->GetOffset_Internal(),component->GetClassPrivate()->GetPropertiesSize(),base_size,
        static_cast<UStruct*>(tick)->GetPropertiesSize(),flag->GetOffset_Internal());
    if(offset<0) return {};
    auto* data=reinterpret_cast<std::byte*>(component)+offset;
    UObject* owner{};const void* vtable{};
    std::memcpy(&owner,data+base_size,sizeof(owner));std::memcpy(&vtable,data,sizeof(vtable));
    if(owner!=component || !vtable) return {};
    return {data,flag,vtable};
}
}

UObject* Appearance::menu_flow_material(int index,UObject* effective,UObject* original_override) {
    auto& stored=menu_flow_[index];
    auto slot=stored;
    slot.original_override=original_override;
    if(slot.source.Get()!=effective || (!effective && slot.copy.Get())) {
        slot.copy.Reset();
        slot.source=effective;
        slot.driver=menu_flow_driver(effective);
    }
    if(slot.driver==inventory_effects::Driver::none || !effective) { stored=slot;return original_override; }
    auto* copy=slot.copy.Get();
    if(!copy) {
        auto* parent=dynamic_material(effective) ? read<UObject*>(effective,L"Parent") : effective;
        if(!parent || dynamic_material(parent)) return original_override;
        Call make(find(L"/Script/Engine.Default__KismetMaterialLibrary"),L"CreateDynamicMaterialInstance",5);
        make.set(L"WorldContextObject",menu_component_.Get());make.set(L"Parent",parent);make.run();
        copy=make.get<UObject*>();
        if(!dynamic_material(copy) || copy==effective || read<UObject*>(copy,L"Parent")!=parent)
            return original_override;
    }
    if(slot.copy.Get()!=copy || slot.revision!=appearance_revision) {
        Call transfer(copy,L"K2_CopyMaterialInstanceParameters",2);
        transfer.set(L"Source",effective);transfer.set(L"bQuickParametersOnly",true);transfer.run();
        if(slot.driver==inventory_effects::Driver::peony_phase) {
            slot.base=menu_scalar(copy,L"FlowPhase");slot.rate=menu_scalar(copy,L"FlowSpeed");
        } else {
            const float fixed=std::clamp(menu_scalar(copy,L"FreezeTime"),0.f,1.f);
            slot.base=fixed*menu_scalar(copy,L"PreviewSeconds");slot.rate=1.f-fixed;
            astral_scalar(copy,L"FreezeTime",1.f);
        }
        slot.revision=appearance_revision;slot.copy=copy;
    }
    stored=slot;
    return copy;
}

void Appearance::sync_menu_cloth(UObject* source,UObject* target,bool paused) {
    auto* disabled=menu_bool(target,L"bDisableClothSimulation");
    auto* source_disabled=menu_bool(source,L"bDisableClothSimulation");
    if(!disabled || !source_disabled) return;
    auto tick=menu_cloth_tick(target);
    auto world_tick=menu_cloth_tick(source);
    bool running=tick.data && world_tick.data && tick.vtable==world_tick.vtable &&
                 !source_disabled->GetPropertyValueInContainer(source);
    if(running) {
        for(auto* mesh:{source,target}) {
            Call suspended(mesh,L"IsClothingSimulationSuspended",1);suspended.run();
            Call simulation(mesh,L"GetClothingSimulationInteractor",1);simulation.run();
            if(suspended.get<bool>() || !simulation.get<UObject*>()) { running=false;break; }
        }
    }
    const float source_blend=read<float>(source,L"ClothBlendWeight");
    std::optional<float> blend;
    if(running && std::isfinite(source_blend)) blend=source_blend;
    menu_cloth_blend_.sync(read<float>(target,L"ClothBlendWeight"),blend,
        [&](float value){set_menu_cloth_blend(target,value);});
    menu_cloth_.sync(disabled->GetPropertyValueInContainer(target),running,false,
        [&](bool value){disabled->SetPropertyValueInContainer(target,value);});
    if(tick.data) menu_cloth_tick_.sync(tick.pause->GetPropertyValueInContainer(tick.data),running && paused,true,
        [&](bool value){tick.pause->SetPropertyValueInContainer(tick.data,value);});
}

void Appearance::restore_menu_flow() {
    if(!menu_clock_) return;
    if(auto* target=menu_component_.Get();target && target!=component_.Get()) {
        auto* world=component_.Get();
        const bool same_mesh=world && mesh_asset(world)==mesh_asset(target);
        const auto overrides_now=same_mesh ? material_objects(world) : std::vector<WeakObject>{};
        for(auto& [index,slot]:menu_flow_) {
            if(auto* copy=slot.copy.Get()) {
                auto* world_override=size_t(index)<overrides_now.size() ? overrides_now[index].Get() : nullptr;
                inventory_effects::restore_slot(menu_material_at(target,index),copy,same_mesh,world_override,
                    slot.original_override.Get(),[&](UObject* value){material(target,index,value);});
            }
        }
    }
    menu_flow_.clear();menu_clock_.reset();
}

void Appearance::restore_menu_effects() {
    restore_menu_flow();
    if(auto* target=menu_component_.Get();target && target!=component_.Get()) {
        if(menu_cloth_blend_.original) menu_cloth_blend_.sync(read<float>(target,L"ClothBlendWeight"),{},
            [&](float value){set_menu_cloth_blend(target,value);});
        if(menu_cloth_.owned) {
            if(auto* flag=menu_bool(target,L"bDisableClothSimulation"))
                menu_cloth_.sync(flag->GetPropertyValueInContainer(target),false,false,
                    [&](bool value){flag->SetPropertyValueInContainer(target,value);});
        }
        if(menu_cloth_tick_.owned) {
            auto tick=menu_cloth_tick(target);
            if(tick.data) menu_cloth_tick_.sync(tick.pause->GetPropertyValueInContainer(tick.data),false,true,
                [&](bool value){tick.pause->SetPropertyValueInContainer(tick.data,value);});
        }
    }
    menu_cloth_={};menu_cloth_tick_={};menu_cloth_blend_={};
}

void Appearance::tick_menu_effects(bool effects_page) {
    if(!menu_clock_ && !menu_cloth_.owned && !menu_cloth_tick_.owned && !menu_cloth_blend_.original) return;
    if(!effects_page || !menu_component_.Get() || menu_component_.Get()==component_.Get() ||
       mesh_asset(menu_component_.Get())!=menu_applied_.Get()) {
        restore_menu_effects();return;
    }
    if(!menu_clock_) return;
    std::optional<double> now;
    for(auto& [index,slot]:menu_flow_) {
        if(auto* copy=slot.copy.Get()) {
            const auto* name=slot.driver==inventory_effects::Driver::peony_phase ? L"FlowPhase" : L"PreviewSeconds";
            if(!now) now=menu_seconds(menu_component_.Get(),L"GetUnpausedTimeSeconds");
            astral_scalar(copy,name,menu_clock_->sample(slot.driver,slot.base,slot.rate,*now));
        }
    }
}

Json Appearance::inspect_menu_effects() const {
    Json result={{"paused_clock",menu_clock_.has_value()},{"flow_slots",Json::array()}};
    auto* target=menu_component_.Get();
    auto* world=component_.Get();
    for(auto [name,component]:{std::pair{"world",world},std::pair{"preview",target}}) {
        if(!component) continue;
        auto& info=result[name];
        info["mesh_component"]=narrow(component->GetPathName());
        if(component==target) {
            info["game_seconds"]=menu_seconds(target,L"GetTimeSeconds");
            info["unpaused_seconds"]=menu_seconds(target,L"GetUnpausedTimeSeconds");
        }
        if(auto* disabled=menu_bool(component,L"bDisableClothSimulation"))
            info["cloth_disabled"]=disabled->GetPropertyValueInContainer(component);
        info["cloth_blend"]=read<float>(component,L"ClothBlendWeight");
        auto tick=menu_cloth_tick(component);
        if(tick.data) info["cloth_tick_when_paused"]=tick.pause->GetPropertyValueInContainer(tick.data);
        Call simulation(component,L"GetClothingSimulationInteractor",1);simulation.run();
        info["cloth_interactor"]=simulation.get<UObject*>()!=nullptr;
    }
    for(const auto& [index,slot]:menu_flow_) {
        auto* copy=slot.copy.Get();auto* source=slot.source.Get();
        if(!target || !world || !copy || !source || menu_material_at(target,index)!=copy) continue;
        const bool phase=slot.driver==inventory_effects::Driver::peony_phase;
        const auto* parameter=phase ? L"FlowPhase" : L"PreviewSeconds";
        Json item={{"slot",index},{"parameter",narrow(parameter)},{"preview",menu_scalar(copy,parameter)},
                   {"world_uses_source",menu_material_at(world,index)==source},{"private",source!=copy}};
        if(dynamic_material(source)) item["world"]=menu_scalar(source,parameter);
        result["flow_slots"].push_back(std::move(item));
    }
    return result;
}

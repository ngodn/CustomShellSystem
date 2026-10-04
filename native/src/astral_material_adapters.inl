// Included after material ownership helpers. Reads only reflected material state.
namespace {
#include "astral_material_catalog.inl"

struct AstralStructView {
    UStruct* type;
    const std::byte* data;
    size_t size;
    static AstralStructView object(UObject* value) {
        auto* type=value->GetClassPrivate();
        return {type,reinterpret_cast<const std::byte*>(value),size_t(type->GetPropertiesSize())};
    }
    FProperty* property(const wchar_t* name) const {
        auto* p=optional_field(type,name);
        if(!p || p->GetArrayDim()!=1 || p->GetOffset_Internal()<0 || p->GetElementSize()<=0 ||
           size_t(p->GetOffset_Internal())+size_t(p->GetElementSize())>size)
            throw std::runtime_error("Astral material property mismatch: "+narrow(name));
        return p;
    }
    bool flag(const wchar_t* name) const {
        auto* p=property(name);
        if(!p->IsA<FBoolProperty>()) throw std::runtime_error("Astral material bool type mismatch");
        return static_cast<FBoolProperty*>(p)->GetPropertyValue(data+p->GetOffset_Internal());
    }
    template<typename Value,typename Property> Value number(const wchar_t* name) const {
        auto* p=property(name);
        if(!p->IsA<Property>() || p->GetElementSize()!=sizeof(Value))
            throw std::runtime_error("Astral material value type mismatch");
        Value result{};std::memcpy(&result,data+p->GetOffset_Internal(),sizeof(result));return result;
    }
    AstralStructView child(const wchar_t* name) const {
        auto* p=property(name);
        if(!p->IsA<FStructProperty>()) throw std::runtime_error("Astral material struct type mismatch");
        auto* child=static_cast<FStructProperty*>(p)->GetStruct().Get();
        if(!child || child->GetStructureSize()>p->GetElementSize())
            throw std::runtime_error("Astral material struct size mismatch");
        return {child,data+p->GetOffset_Internal(),size_t(p->GetElementSize())};
    }
    FScriptArrayHelper array(const wchar_t* name,size_t limit) const {
        auto* p=property(name);
        if(!p->IsA<FArrayProperty>() || p->GetElementSize()!=sizeof(FScriptArray))
            throw std::runtime_error("Astral material array type mismatch");
        FScriptArrayHelper result(static_cast<FArrayProperty*>(p),data+p->GetOffset_Internal());
        if(result.Num()<0 || size_t(result.Num())>limit)
            throw std::runtime_error("Astral material array exceeds bound");
        return result;
    }
    std::array<uint32_t,4> guid() const {
        auto view=child(L"StateId");
        if(view.type->GetNamePrivate()!=FName(L"Guid") || view.size!=16)
            throw std::runtime_error("Astral graph identity layout mismatch");
        std::array<uint32_t,4> value{};std::memcpy(value.data(),view.data,16);return value;
    }
};

bool astral_uber_layer(const AstralStructView& layers) {
    auto values=layers.array(L"Layers",16);
    if(values.Num()!=1 || layers.array(L"Blends",16).Num()!=0) return false;
    auto* inner=static_cast<FArrayProperty*>(layers.property(L"Layers"))->GetInner();
    if(!inner || !inner->IsA<FObjectProperty>() || inner->GetElementSize()!=sizeof(UObject*) || inner->GetArrayDim()!=1)
        throw std::runtime_error("Astral material layer element mismatch");
    auto* value=static_cast<FObjectProperty*>(inner)->GetObjectPropertyValue(values.GetRawPtr(0));
    if(!value || WeakObject(value).Get()!=value ||
       value->GetPathName()!=L"/Game/Sparta/MasterMaterials/Character_Materials/UberShaderV2/MaterialFunctions/ML_UberShaderBase.ML_UberShaderBase")
        return false;
    // Independently decoded native layer, not an arbitrary replacement at the same path.
    if(AstralStructView::object(value).guid()!=std::array<uint32_t,4>{0x377B9133u,0x4A58F5E3u,0x511D0FA4u,0xFB5628B2u})
        return false;
    auto tree=layers.child(L"Tree");
    // Tree members are not UPROPERTYs. UE 5.6.1 MaterialLayersFunctions.h
    // stores two 16-byte TArrays followed by an int32 root (dump size 0x28).
    // Check only empty counts; never follow an unreflected allocation pointer.
    if(tree.type->GetNamePrivate()!=FName(L"MaterialLayersFunctionsTree") || tree.size!=0x28)
        throw std::runtime_error("Astral material layer tree layout mismatch");
    int32_t nodes=0,payloads=0,root=0;
    std::memcpy(&nodes,tree.data+8,4);std::memcpy(&payloads,tree.data+24,4);std::memcpy(&root,tree.data+32,4);
    return nodes==0 && payloads==0 && root==-1;
}

void astral_inherit_material(AstralMaterialState& state,UObject* instance,AstralMaterialFamily family) {
    auto view=AstralStructView::object(instance);
    auto base=view.child(L"BasePropertyOverrides");
    if(base.flag(L"bOverride_BlendMode")) state.blend=base.number<uint8_t,FByteProperty>(L"BlendMode");
    if(base.flag(L"bOverride_TwoSided")) state.two_sided=base.flag(L"TwoSided");
    if(base.flag(L"bOverride_OpacityMaskClipValue")) state.clip=base.number<float,FFloatProperty>(L"OpacityMaskClipValue");
    for(auto* p:base.type->ForEachProperty()) {
        const auto name=p->GetName();
        if(name.starts_with(L"bOverride_") && name!=L"bOverride_BlendMode" &&
           name!=L"bOverride_TwoSided" && name!=L"bOverride_OpacityMaskClipValue" &&
           name!=L"bOverride_ShadingModel" && base.flag(name.c_str())) state.other_base_override=true;
    }
    auto parameters=view.child(L"StaticParametersRuntime");
    if(parameters.flag(L"bHasMaterialLayers"))
        state.replaced_layers=family!=AstralMaterialFamily::uber || !astral_uber_layer(parameters.child(L"MaterialLayers"));
    auto values=parameters.array(L"StaticSwitchParameters",256);
    auto* inner=static_cast<FArrayProperty*>(parameters.property(L"StaticSwitchParameters"))->GetInner();
    if(!inner || !inner->IsA<FStructProperty>() || inner->GetArrayDim()!=1)
        throw std::runtime_error("Astral switch element mismatch");
    auto* type=static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if(!type || type->GetStructureSize()>inner->GetElementSize())
        throw std::runtime_error("Astral switch size mismatch");
    std::set<std::string> seen;
    for(int i=0;i<values.Num();++i) {
        AstralStructView value{type,reinterpret_cast<const std::byte*>(values.GetRawPtr(i)),size_t(inner->GetElementSize())};
        if(!value.flag(L"bOverride")) continue;
        auto info=value.child(L"ParameterInfo");
        if(info.number<uint8_t,FByteProperty>(L"Association")!=2 || info.number<int32_t,FIntProperty>(L"Index")!=-1)
            throw std::runtime_error("Astral layer-specific switches are unsupported");
        const auto name=narrow(info.number<FName,FNameProperty>(L"Name").ToString());
        if(name.empty() || name.size()>128 || !seen.insert(name).second)
            throw std::runtime_error("Astral switch name is invalid or duplicated");
        state.switches[name]=value.flag(L"Value");
    }
}
}

void AstralMaterialAdapters::prepare(UObject* owner,std::span<UObject* const> sources) {
    if(!inputs_.empty()) throw std::runtime_error("Release prior Astral adapters first");
    if(!owner || WeakObject(owner).Get()!=owner || sources.empty() || sources.size()>128)
        throw std::runtime_error("Invalid Astral adapter source set");
    AssetLoadRoots roots;
    auto* interface_type=static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"));
    auto* instance_type=static_cast<UClass*>(find(L"/Script/Engine.MaterialInstance"));
    auto* material_type=static_cast<UClass*>(find(L"/Script/Engine.Material"));
    for(auto* source:sources) {
        if(!source || WeakObject(source).Get()!=source || !source->IsA(interface_type))
            throw std::runtime_error("Invalid Astral adapter material");
        astral_retain(roots,source);
    }
    auto retained_load=[&](const std::string& path) {
        auto* value=load(path);astral_retain(roots,value);return value;
    };
    std::vector<AstralMaterialInput> inputs;
    for(auto* source:sources) {
        std::vector<UObject*> chain;
        auto* root=source;
        while(root->IsA(instance_type)) {
            if(chain.size()>=32 || std::find(chain.begin(),chain.end(),root)!=chain.end())
                throw std::runtime_error("Invalid Astral material inheritance");
            chain.push_back(root);root=read<UObject*>(root,L"Parent");
            if(!root || WeakObject(root).Get()!=root) throw std::runtime_error("Astral material parent expired");
        }
        if(!root->IsA(material_type)) throw std::runtime_error("Astral source has no material root");
        const auto path=narrow(root->GetPathName());
        const auto spec=std::find_if(astral_material_definitions.begin(),astral_material_definitions.end(),
            [&](const auto& value){return value.root==path;});
        if(spec==astral_material_definitions.end()) throw std::runtime_error("No ghost adapter for "+path);
        const auto view=AstralStructView::object(root);
        if(view.guid()!=spec->state_id) throw std::runtime_error("Ghost adapter graph revision differs: "+path);
        AstralMaterialState state;
        state.blend=view.number<uint8_t,FByteProperty>(L"BlendMode");state.two_sided=view.flag(L"TwoSided");
        state.clip=view.number<float,FFloatProperty>(L"OpacityMaskClipValue");state.switches=spec->defaults;
        state.replaced_layers=spec->family==AstralMaterialFamily::uber;
        for(auto it=chain.rbegin();it!=chain.rend();++it) astral_inherit_material(state,*it,spec->family);
        const auto reason=astral_material_rejection(*spec,state);
        if(!reason.empty()) throw std::runtime_error("Ghost adapter rejected "+path+": "+reason);
        const auto companion=astral_material_companion(*spec,state);
        AstralMaterialInput input{source,retained_load(companion),{},{},{}};
        input.textures.push_back({L"CSS_AstralNoise",retained_load(
            "/Game/Sparta/FX/Textures/Noises/BnW/T_noise_0017.T_noise_0017")});
        if(spec->family==AstralMaterialFamily::smoke) input.textures.push_back({L"CSS_EyeNoise",retained_load(
            "/Game/Sparta/FX/Textures/Noises/BnW/T_noise_0082.T_noise_0082")});
        if(spec->family==AstralMaterialFamily::uber) {
            input.required_source_textures.push_back(L"BaseColorMap  non VT");
            if(state.blend==1) input.scalars.push_back({L"CSS_AstralClipValue",state.clip});
        } else if(spec->family==AstralMaterialFamily::authored &&
                  (path=="/Game/CSS/EveHair/M_Hair1.M_Hair1" || path=="/Game/CSS/CommanderWhite/MaterialY/M_Hair.M_Hair")) {
            input.required_source_textures.push_back(L"BaseColorMap  non VT");
        } else if(spec->family==AstralMaterialFamily::refraction) {
            auto* collection=retained_load("/Game/Sparta/Lighting/Blueprints/Deprecated/MPC_LightScenario.MPC_LightScenario");
            Call get(find(L"/Script/Engine.Default__KismetMaterialLibrary"),L"GetScalarParameterValue",4);
            get.set(L"WorldContextObject",owner);get.set(L"Collection",collection);
            get.set(L"ParameterName",FName(L"Reflection Boost Intensity"));get.run();
            input.scalars.push_back({L"CSS_AstralReflectionBoost",get.get<float>()});
            input.required_source_textures.push_back(L"ReflectionMap");
        }
        inputs.push_back(std::move(input));
    }
    retained_.take(roots);inputs_=std::move(inputs);
}

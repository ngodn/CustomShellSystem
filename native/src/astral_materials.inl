// Included after engine.cpp's reflected calls and material type helpers.
namespace {
float astral_opacity(UObject* native_mid) {
    Call readback(native_mid,L"K2_GetScalarParameterValue",2);
    readback.set(L"ParameterName",FName(L"GlobalOpacity",FNAME_Add));
    readback.run();
    const float value=readback.get<float>();
    if(!std::isfinite(value)) throw std::runtime_error("Astral opacity is not finite");
    return std::clamp(value,0.f,1.f);
}
void astral_scalar(UObject* material,const wchar_t* name,float value) {
    Call set(material,L"SetScalarParameterValue",2);
    set.set(L"ParameterName",FName(name,FNAME_Add));set.set(L"Value",value);set.run();
}
void astral_retain(AssetLoadRoots& roots,UObject* object) {
    const auto before=roots.size();
    roots.keep(object);
    if(roots.size()!=before+1) throw std::runtime_error("Astral material lifetime protection unavailable");
}
}
void AstralMaterials::prepare(UObject* owner,UObject* native_mid,
                             std::span<const AstralMaterialInput> inputs,bool corrupted) {
    if(!copies_.empty()) throw std::runtime_error("Detach and release the previous Astral materials first");
    if(!owner || WeakObject(owner).Get()!=owner || !native_mid ||
       WeakObject(native_mid).Get()!=native_mid || !dynamic_material(native_mid) ||
       inputs.empty() || inputs.size()>128)
        throw std::runtime_error("Invalid Astral material preparation");
    auto* interface_class=static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"));
    auto* texture_class=static_cast<UClass*>(find(L"/Script/Engine.Texture"));
    AssetLoadRoots roots;
    // Protect all inputs before the first allocation. A failure leaves no
    // component bound to a partial result and destroys this local root scope.
    for(const auto& input:inputs) {
        if(!input.source || !input.companion || WeakObject(input.source).Get()!=input.source ||
           WeakObject(input.companion).Get()!=input.companion ||
           !input.source->IsA(interface_class) || !input.companion->IsA(interface_class) ||
           dynamic_material(input.companion))
            throw std::runtime_error("Invalid Astral source or companion material");
        astral_retain(roots,input.source);astral_retain(roots,input.companion);
        if(input.textures.size()>16 || input.scalars.size()>16)
            throw std::runtime_error("Astral material controls exceed bound");
        for(size_t i=0;i<input.textures.size();++i) {
            const auto& texture=input.textures[i];
            if(texture.parameter.empty() || texture.parameter.size()>128 ||
               texture.parameter.find(L'\0')!=std::wstring::npos || !texture.texture ||
               WeakObject(texture.texture).Get()!=texture.texture || !texture.texture->IsA(texture_class))
                throw std::runtime_error("Invalid Astral texture binding");
            for(size_t j=0;j<i;++j) if(input.textures[j].parameter==texture.parameter)
                throw std::runtime_error("Duplicate Astral texture binding");
            astral_retain(roots,texture.texture);
        }
        for(size_t i=0;i<input.scalars.size();++i) {
            const auto& scalar=input.scalars[i];
            if(scalar.parameter.empty() || scalar.parameter.size()>128 ||
               scalar.parameter.find(L'\0')!=std::wstring::npos || !std::isfinite(scalar.value) ||
               scalar.parameter==L"CSS_AstralOpacity" || scalar.parameter==L"CSS_AstralCorrupted" ||
               scalar.parameter==L"CSS_AstralUseFixedTime" || scalar.parameter==L"CSS_AstralFixedTime")
                throw std::runtime_error("Invalid Astral scalar binding");
            for(size_t j=0;j<i;++j) if(input.scalars[j].parameter==scalar.parameter)
                throw std::runtime_error("Duplicate Astral scalar binding");
        }
    }
    const WeakObject owner_guard(owner),native_guard(native_mid);
    const float opacity=astral_opacity(native_mid);
    std::vector<Copy> copies;
    copies.reserve(inputs.size());
    std::vector<AstralMaterialInput> unique;
    std::vector<size_t> slots;
    unique.reserve(inputs.size());slots.reserve(inputs.size());
    auto* library=find(L"/Script/Engine.Default__KismetMaterialLibrary");
    for(const auto& input:inputs) {
        const auto existing=std::find_if(unique.begin(),unique.end(),[&](const auto& value) {
            return value.source==input.source && value.companion==input.companion &&
                   value.textures==input.textures && value.scalars==input.scalars;
        });
        if(existing!=unique.end()) {
            slots.push_back(size_t(existing-unique.begin()));
            continue;
        }
        if(owner_guard.Get()!=owner || native_guard.Get()!=native_mid)
            throw std::runtime_error("Astral owner expired during preparation");
        Call make(library,L"CreateDynamicMaterialInstance",5);
        make.set(L"WorldContextObject",owner);make.set(L"Parent",input.companion);make.run();
        auto* copy=make.get<UObject*>();
        if(!copy || copy==input.source || copy==native_mid || !dynamic_material(copy))
            throw std::runtime_error("Astral private material creation failed");
        for(const auto& previous:copies) if(previous.instance.Get()==copy)
            throw std::runtime_error("Astral materials unexpectedly share an instance");
        astral_retain(roots,copy);
        Call transfer(copy,L"K2_CopyMaterialInstanceParameters",2);
        transfer.set(L"Source",input.source);transfer.set(L"bQuickParametersOnly",true);transfer.run();
        // Copy clears destination overrides. Native controls must be written afterward.
        for(const auto& texture:input.textures) {
            const FName name(texture.parameter.c_str(),FNAME_Add);
            Call set(copy,L"SetTextureParameterValue",2);
            set.set(L"ParameterName",name);set.set(L"Value",texture.texture);set.run();
            Call get(copy,L"K2_GetTextureParameterValue",2);
            get.set(L"ParameterName",name);get.run();
            if(get.get<UObject*>()!=texture.texture)
                throw std::runtime_error("Astral texture binding readback failed");
        }
        for(const auto& scalar:input.scalars) {
            astral_scalar(copy,scalar.parameter.c_str(),scalar.value);
            Call get(copy,L"K2_GetScalarParameterValue",2);
            get.set(L"ParameterName",FName(scalar.parameter.c_str(),FNAME_Add));get.run();
            if(get.get<float>()!=scalar.value)
                throw std::runtime_error("Astral scalar binding readback failed");
        }
        astral_scalar(copy,L"CSS_AstralCorrupted",corrupted?1.f:0.f);
        astral_scalar(copy,L"CSS_AstralUseFixedTime",0.f);
        Call initialize(copy,L"InitializeScalarParameterAndGetIndex",4);
        initialize.set(L"ParameterName",FName(L"CSS_AstralOpacity",FNAME_Add));
        initialize.set(L"Value",opacity);initialize.run();
        const auto index=initialize.get<int32_t>(L"OutParameterIndex");
        if(!initialize.get<bool>() || index<0)
            throw std::runtime_error("Astral opacity parameter initialization failed");
        copies.push_back({WeakObject(copy),index});
        slots.push_back(unique.size());unique.push_back(input);
    }
    retained_.take(roots);
    owner_=owner_guard;native_mid_=native_guard;
    copies_=std::move(copies);slots_=std::move(slots);opacity_=opacity;
}
UObject* AstralMaterials::material(size_t index) const {
    if(!owner_.Get() || !native_mid_.Get() || index>=slots_.size()) return nullptr;
    return copies_[slots_[index]].instance.Get();
}
bool AstralMaterials::sync_opacity() {
    if(!owner_.Get() || !native_mid_.Get() || copies_.empty()) return false;
    for(const auto& copy:copies_) if(!copy.instance.Get()) return false;
    const float value=astral_opacity(native_mid_.Get());
    if(opacity_ && *opacity_==value) return true;
    // Indices belong exclusively to their original MID. No parameter copy or
    // clear is allowed after preparation, because either can invalidate them.
    for(const auto& copy:copies_) {
        Call set(copy.instance.Get(),L"SetScalarParameterByIndex",3);
        set.set(L"ParameterIndex",copy.opacity_index);set.set(L"Value",value);set.run();
        if(!set.get<bool>()) return false;
    }
    opacity_=value;
    return true;
}
void AstralMaterials::release() noexcept {
    slots_.clear();copies_.clear();owner_=WeakObject{};native_mid_=WeakObject{};
    opacity_.reset();retained_.release();
}

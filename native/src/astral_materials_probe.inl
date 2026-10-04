// Development-only check of the runtime owner. All writes target unattached MIDs.
Json EngineBridge::probe_astral_materials(void* engine,const Json& request) {
    keep_alive_attach(engine);
    auto* owner=resolve(request.at("owner"));
    auto* native_parent=resolve(request.at("native_parent"));
    auto* interface_class=static_cast<UClass*>(find(L"/Script/Engine.MaterialInterface"));
    if(!owner || !native_parent || !native_parent->IsA(interface_class) || dynamic_material(native_parent))
        throw std::runtime_error("Probe needs a live owner and non-dynamic native material parent");
    const auto& rows=request.at("inputs");
    if(!rows.is_array() || rows.empty() || rows.size()>16)
        throw std::runtime_error("Probe requires 1..16 material pairs");
    struct Parameter { size_t slot; std::string name; const wchar_t* getter; Json before; };
    std::vector<Parameter> parameters;
    std::vector<AstralMaterialInput> inputs;
    AssetLoadRoots roots;
    const auto retained_before=keep_alive::entries.size();
    astral_retain(roots,owner);astral_retain(roots,native_parent);
    auto sample=[&](UObject* material,const Parameter& parameter) {
        Call readback(material,parameter.getter,2);
        readback.set(L"ParameterName",FName(wide(parameter.name).c_str(),FNAME_Add));readback.run();
        auto* result=readback.param(L"ReturnValue");
        return decode(result,readback.data(result),0);
    };
    for(const auto& row:rows) {
        auto* source=resolve(row.at("source"));
        auto* companion=resolve(row.at("companion"));
        if(!source || !companion || !source->IsA(interface_class) || !companion->IsA(interface_class) ||
           dynamic_material(companion)) throw std::runtime_error("Invalid probe material pair");
        astral_retain(roots,source);astral_retain(roots,companion);
        const auto slot=inputs.size();
        inputs.push_back({source,companion,{}, {}});
        for(const auto& [key,getter]:std::array<std::pair<const char*,const wchar_t*>,3>{{
            {"scalars",L"K2_GetScalarParameterValue"}, {"vectors",L"K2_GetVectorParameterValue"},
            {"textures",L"K2_GetTextureParameterValue"}}}) {
            const auto& names=row.at(key);
            if(!names.is_array() || names.size()>64) throw std::runtime_error("Probe parameter list exceeds bound");
            std::set<std::string> unique;
            for(const auto& item:names) {
                const auto name=item.get<std::string>();
                if(name.empty() || name.size()>128 || name.find('\0')!=std::string::npos ||
                   name.starts_with("CSS_Astral") || !unique.insert(name).second)
                    throw std::runtime_error("Invalid or duplicate probe parameter");
                Parameter parameter{slot,name,getter,{}};
                parameter.before=sample(source,parameter);
                if(std::string_view(key)=="scalars" && (!parameter.before.is_number() ||
                   !std::isfinite(parameter.before.get<double>())))
                    throw std::runtime_error("Probe scalar readback is invalid");
                if(std::string_view(key)=="vectors") for(const auto* channel:{"R","G","B","A"}) {
                    const auto& value=parameter.before.at(channel);
                    if(!value.is_number() || !std::isfinite(value.get<double>()))
                        throw std::runtime_error("Probe vector readback is invalid");
                }
                parameters.push_back(std::move(parameter));
            }
        }
    }
    if(parameters.empty()) throw std::runtime_error("Probe needs independently identified source parameters");
    Call make(find(L"/Script/Engine.Default__KismetMaterialLibrary"),L"CreateDynamicMaterialInstance",5);
    make.set(L"WorldContextObject",owner);make.set(L"Parent",native_parent);make.run();
    auto* native_copy=make.get<UObject*>();
    if(!native_copy || !dynamic_material(native_copy)) throw std::runtime_error("Probe native MID creation failed");
    astral_retain(roots,native_copy);
    AstralMaterials copies;
    auto invalid=inputs;
    invalid.back().companion=nullptr;
    bool rejected=false;
    try { copies.prepare(owner,native_copy,invalid,false); }
    catch(const std::exception&) { rejected=true; }
    if(!rejected || copies.size()!=0 || copies.material(0))
        throw std::runtime_error("Probe failed preparation left usable materials");
    Json forms=Json::array();
    auto read_scalar=[&](UObject* material,const char* name) {
        Parameter parameter{0,name,L"K2_GetScalarParameterValue",{}};
        return sample(material,parameter).get<float>();
    };
    for(const bool corrupted:{false,true}) {
        astral_scalar(native_copy,L"GlobalOpacity",.375f);
        copies.prepare(owner,native_copy,inputs,corrupted);
        if(copies.size()!=inputs.size()) throw std::runtime_error("Probe lost material slots");
        rejected=false;
        try { copies.prepare(owner,native_copy,inputs,corrupted); }
        catch(const std::exception&) { rejected=true; }
        if(!rejected || copies.size()!=inputs.size())
            throw std::runtime_error("Probe allowed preparation over live materials");
        for(size_t i=0;i<inputs.size();++i) for(size_t j=0;j<i;++j) {
            const bool same=inputs[i].source==inputs[j].source && inputs[i].companion==inputs[j].companion;
            if((copies.material(i)==copies.material(j))!=same)
                throw std::runtime_error("Probe material sharing differs from source/companion identity");
        }
        for(const auto& parameter:parameters) {
            if(sample(copies.material(parameter.slot),parameter)!=parameter.before)
                throw std::runtime_error("Probe uniform copy differs: "+parameter.name);
        }
        for(size_t i=0;i<copies.size();++i) {
            auto* material=copies.material(i);
            if(read_scalar(material,"CSS_AstralOpacity")!=.375f ||
               read_scalar(material,"CSS_AstralCorrupted")!=(corrupted?1.f:0.f) ||
               read_scalar(material,"CSS_AstralUseFixedTime")!=0.f)
                throw std::runtime_error("Probe initial controls differ");
        }
        for(const float value:{1.f,.5f,.5f,0.f,-.25f,1.25f}) {
            astral_scalar(native_copy,L"GlobalOpacity",value);
            if(!copies.sync_opacity()) throw std::runtime_error("Probe fade synchronization failed");
            for(size_t i=0;i<copies.size();++i)
                if(read_scalar(copies.material(i),"CSS_AstralOpacity")!=std::clamp(value,0.f,1.f))
                    throw std::runtime_error("Probe fade readback differs");
        }
        for(const auto& parameter:parameters) {
            if(sample(inputs[parameter.slot].source,parameter)!=parameter.before ||
               sample(copies.material(parameter.slot),parameter)!=parameter.before)
                throw std::runtime_error("Probe source uniform changed during fade: "+parameter.name);
        }
        forms.push_back(corrupted?"stray":"faithful");
        copies.release();
        if(copies.size()!=0 || copies.material(0) || copies.sync_opacity())
            throw std::runtime_error("Probe material owner did not release");
    }
    roots.release();
    if(keep_alive::entries.size()!=retained_before)
        throw std::runtime_error("Probe retained-object count did not return to baseline");
    return {{"passed",true},{"forms",forms},{"slots",inputs.size()},{"parameters",parameters.size()},
        {"component_bindings_changed",false},{"retained_objects_restored",true},
        {"scope","Private MID uniform copying, fade updates and explicit release only. No rendered or summon validation."}};
}

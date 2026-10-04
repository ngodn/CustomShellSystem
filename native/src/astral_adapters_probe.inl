// Prepare assets without creating components or changing player materials.
Json EngineBridge::probe_astral_adapters(void* engine,Appearance& appearance) {
    keep_alive_attach(engine);appearance.player(engine);
    const auto before=keep_alive::entries.size();
    Json rows=Json::array();
    {
        auto source=appearance.astral_source();
        if(!source) throw std::runtime_error("No settled Genessa appearance to inspect");
        std::vector<UObject*> materials;
        for(const auto& part:source->components) for(size_t i=0;i<part.materials.size();++i) {
            materials.push_back(part.materials[i].Get());
            if(auto* overlay=part.overlays.at(i).Get()) materials.push_back(overlay);
        }
        AstralMaterialAdapters adapters;
        adapters.prepare(source->pawn.Get(),materials);
        for(const auto& input:adapters.inputs()) {
            Json textures=Json::array(),scalars=Json::array();
            for(const auto& value:input.textures) textures.push_back({{"name",narrow(value.parameter)},
                {"asset",narrow(value.texture->GetPathName())}});
            for(const auto& value:input.scalars) scalars.push_back({{"name",narrow(value.parameter)},{"value",value.value}});
            rows.push_back({{"source",narrow(input.source->GetPathName())},
                {"companion",narrow(input.companion->GetPathName())},{"textures",textures},{"scalars",scalars}});
        }
    }
    if(keep_alive::entries.size()!=before) throw std::runtime_error("Astral adapter probe retained objects after release");
    return {{"passed",true},{"materials",rows},{"player_unchanged",true},
        {"scope","Reflected source selection and asset loading only; no rendered double validation."}};
}

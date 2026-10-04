// Development-only serialization of the capture available to double preparation.
Json EngineBridge::observe_astral_source(Appearance& appearance) {
    const auto retained_before=keep_alive::entries.size();
    auto source=appearance.astral_source();
    if(!source) return {{"status","unavailable"},{"appearance_writes",false}};
    Json rows=Json::array();
    for(const auto& component:source->components) {
        Json materials=Json::array(),overlays=Json::array();
        for(const auto& material:component.materials) materials.push_back(handle(material.Get()));
        for(const auto& material:component.overlays) overlays.push_back(handle(material.Get()));
        const auto& settings=component.physics;
        Json physics={{"animation_class",handle(settings.animation_class.Get())},
            {"post_process_disabled",settings.post_process_disabled},{"cloth_disabled",settings.cloth_disabled},
            {"rigid_body_disabled",settings.rigid_body_disabled},{"springs",Json::object()},
            {"dynamics",Json::object()},{"rig",nullptr},{"body_rig",nullptr},{"geometry",nullptr}};
        for(const auto& [name,value]:settings.springs) physics["springs"][name]={
            {"stiffness",value.stiffness},{"damping",value.damping},{"max_displacement",value.max_displacement},
            {"error_reset",value.error_reset},{"limit",value.limit},{"translate",value.translate},{"rotate",value.rotate}};
        for(const auto& [name,value]:settings.dynamics) physics["dynamics"][name]={
            {"angular_spring",value.angular_spring},{"linear_damping",value.linear_damping},
            {"angular_damping",value.angular_damping},{"gravity",value.gravity},
            {"spring_enabled",value.spring_enabled},{"override_linear",value.override_linear},
            {"override_angular",value.override_angular},{"gravity_override",value.gravity_override}};
        if(settings.rig) {
            const auto& value=*settings.rig;
            physics["rig"]={{"stiffness",value.stiffness},{"damping",value.damping},
                {"gravity",value.gravity},{"enabled",value.enabled}};
        }
        if(settings.body_rig) {
            const auto& value=*settings.body_rig;
            physics["body_rig"]={{"frequency",value.frequency},{"damping",value.damping},{"motion",value.motion},
                {"enabled",value.enabled},{"global_frequency",value.global_frequency},
                {"global_damping",value.global_damping},{"global_motion",value.global_motion},{"use_regions",value.use_regions}};
        }
        if(settings.geometry) {
            const auto& value=*settings.geometry;
            physics["geometry"]={{"offsets",value.offsets},{"moments",value.moments},
                {"contact_centers",value.contact_centers},{"contact_axes",value.contact_axes}};
        }
        rows.push_back({{"item",component.item},{"component",handle(component.component.Get())},
            {"mesh",handle(component.mesh.Get())},{"visible",component.visible},{"hidden_in_game",component.hidden_in_game},
            {"materials",std::move(materials)},{"overlays",std::move(overlays)},{"hidden_by_lod",component.hidden_by_lod},
            {"morphs",component.morphs},{"location",component.location},{"rotation",component.rotation},
            {"scale",component.scale},{"physics",std::move(physics)}});
    }
    Json output={{"status","captured"},{"pawn",handle(source->pawn.Get())},{"player_revision",source->player_revision},
        {"components",std::move(rows)},{"appearance_writes",false}};
    source.reset();
    if(keep_alive::entries.size()!=retained_before)
        throw std::runtime_error("Astral source capture retained-object count did not return to baseline");
    output["retained_objects_restored"]=true;
    return output;
}

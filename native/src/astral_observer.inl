// Development diagnostics for the production spawner reader.
namespace {
Json astral_json(AstralIdentity value) { return {{"index",value.index},{"serial",value.serial}}; }
const char* astral_event_name(AstralEventKind event) {
    switch(event) {
    case AstralEventKind::discovered: return "discovered";
    case AstralEventKind::activated: return "activated";
    case AstralEventKind::deactivated: return "deactivated";
    case AstralEventKind::rebound: return "rebound";
    case AstralEventKind::removed: return "removed";
    }
    return "unknown";
}
Json astral_changes(const AstralLifecycle::Changes& changes) {
    Json rows=Json::array();
    for(const auto& event:changes.view()) rows.push_back({{"actor",astral_json(event.actor)},
        {"event",astral_event_name(event.kind)},{"activation",event.activation}});
    return rows;
}
}
Json EngineBridge::observe_astral(void* engine) {
    try {
        const auto snapshot=astral_snapshot(engine);
        const auto changes=snapshot.context.valid()?astral_lifecycle_.observe(snapshot.context,snapshot.view()):astral_lifecycle_.clear();
        Json rows=Json::array();
        for(const auto& row:snapshot.view()) {
            auto* actor=astral_resolve(row.actor);
            auto* component=astral_resolve(row.component);
            Json output={{"actor",astral_json(row.actor)},{"name",narrow(actor->GetFullName())},
                {"id",narrow(read<FName>(component,L"MyID").ToString())},
                {"component",astral_json(row.component)},{"mesh",astral_json(row.mesh)},
                {"native_mid",astral_json(row.native_mid)},{"initialized",row.initialized},
                {"enabled",row.enabled},{"hidden",row.hidden},{"cached",row.cached},{"active",row.active()}};
            if(auto* mid=astral_resolve(row.native_mid)) output["native_opacity"]=astral_opacity(mid);
            rows.push_back(std::move(output));
        }
        return {{"status",changes.accepted?snapshot.status:"rejected"},{"events",astral_changes(changes)},
                {"actors",std::move(rows)},{"appearance_writes",false}};
    } catch(...) { astral_lifecycle_.clear();throw; }
}

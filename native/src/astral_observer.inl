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
Json EngineBridge::observe_astral(void* engine,bool poses) {
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
            if(poses && row.active()) {
                auto* type=static_cast<UClass*>(find(L"/Script/Engine.SkeletalMeshComponent"));
                Call get(actor,L"K2_GetComponentsByClass",2);
                get.set(L"ComponentClass",type);get.run();
                auto* property=get.param(L"ReturnValue");
                if(!property->IsA<FArrayProperty>()) throw std::runtime_error("Astral pose component array differs");
                auto* array=static_cast<FArrayProperty*>(property);
                auto* inner=array->GetInner();
                if(!inner || !inner->IsA<FObjectProperty>() || inner->GetArrayDim()!=1 ||
                   inner->GetElementSize()!=sizeof(UObject*))
                    throw std::runtime_error("Astral pose component element differs");
                FScriptArrayHelper values(array,get.data(property));
                if(values.Num()<0 || values.Num()>32) throw std::runtime_error("Astral pose component count exceeds bound");
                Json components=Json::array();
                for(int i=0;i<values.Num();++i) {
                    auto* mesh=static_cast<FObjectProperty*>(inner)->GetObjectPropertyValue(values.GetRawPtr(i));
                    if(!mesh || !mesh->IsA(type) || astral_owner(mesh)!=actor)
                        throw std::runtime_error("Astral pose component owner differs");
                    Json bones=Json::object();
                    for(const auto* bone:{L"pelvis",L"thigh_l",L"calf_l",L"foot_l",L"thigh_r",L"calf_r",L"foot_r",L"head"}) {
                        Call index(mesh,L"GetBoneIndex",2);index.set(L"BoneName",FName(bone));index.run();
                        if(index.get<int32_t>()<0) continue;
                        Call pose(mesh,L"GetBoneTransform",3);
                        pose.set(L"InBoneName",FName(bone));pose.set(L"TransformSpace",uint8_t{2});pose.run();
                        auto* result=pose.param(L"ReturnValue");
                        bones[narrow(bone)]=decode(result,pose.data(result),0);
                    }
                    components.push_back({{"component",handle(mesh)},{"mesh",handle(mesh_asset(mesh))},
                        {"leader",handle(astral_leader(mesh))},{"effective_source",handle(astral_pose_source(mesh))},
                        {"bones",std::move(bones)}});
                }
                output["poses"]=std::move(components);
            }
            rows.push_back(std::move(output));
        }
        return {{"status",changes.accepted?snapshot.status:"rejected"},{"events",astral_changes(changes)},
                {"actors",std::move(rows)},{"appearance_writes",false}};
    } catch(...) { astral_lifecycle_.clear();throw; }
}

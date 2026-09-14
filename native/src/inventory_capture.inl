// Local filming only. Excluded from release DLLs by CSS_INVENTORY_DEV.
namespace {
struct alignas(16) CaptureTransform {
    std::array<double,4> rotation{0,0,0,1};
    alignas(16) std::array<double,3> location{};
    alignas(16) std::array<double,3> scale{1,1,1};
};
static_assert(sizeof(CaptureTransform)==96);
void capture_view(UObject* pc,UObject* actor) {
    Call view(pc,L"SetViewTargetWithBlend",5); view.set(L"NewViewTarget",actor);
    view.set(L"BlendTime",0.f); view.set(L"BlendFunc",uint8_t{0});
    view.set(L"BlendExp",0.f); view.set(L"bLockOutgoing",false); view.run();
}
}
void InventoryUI::cinema_stop() {
    if(auto* pc=cinema_pc_.Get()) {
        if(view_target(pc)==cinema_camera_.Get()) {
            auto* previous=cinema_before_.Get(); if(!previous) previous=cinema_player_.Get();
            if(previous) capture_view(pc,previous);
        }
    }
    if(auto* hud=cinema_hud_.Get()) invoke(hud,L"SetVisibility",L"InVisibility",cinema_hud_visibility_);
    if(auto* actor=cinema_camera_.Get()) invoke(actor,L"K2_DestroyActor");
    cinema_camera_.Reset(); cinema_player_.Reset(); cinema_pc_.Reset(); cinema_before_.Reset(); cinema_hud_.Reset();
    cinema_deadline_=0;
}
void InventoryUI::cinema_command(UObject* pc,const Json& request) {
    const auto action=request.at("action").get<std::string>();
    if(action=="inventory_cinema_stop") { cinema_stop(); return; }
    if(action=="inventory_cinema_start") {
        cinema_stop();
        if(!pc || inventory_bool(main_.Get(),L"bOpen")) throw std::runtime_error("Close Inventory before filming the world");
        auto* pawn=inventory_object(pc,L"Pawn");
        if(!pawn) throw std::runtime_error("Filming requires a live player");
        cinema_pc_=pc; cinema_player_=pawn;
        cinema_before_=view_target(pc);
        auto* handler=inventory_object(pc,L"User Interface Handler Component");
        auto* hud=inventory_object(handler,L"WBP_Player_HUD");
        if(hud) { Call visibility(hud,L"GetVisibility",1); visibility.run(); cinema_hud_visibility_=visibility.get<uint8_t>(); cinema_hud_=hud; }
        CaptureTransform transform; Call position(pawn,L"K2_GetActorLocation",1); position.run(); transform.location=position.get<std::array<double,3>>();
        auto* lib=find(L"/Script/Engine.Default__GameplayStatics");
        try {
            Call spawn(lib,L"BeginDeferredActorSpawnFromClass",7);
            spawn.set(L"WorldContextObject",pawn); spawn.set(L"ActorClass",static_cast<UClass*>(find(L"/Script/Engine.CameraActor")));
            spawn.set(L"SpawnTransform",transform); spawn.set(L"CollisionHandlingOverride",uint8_t{1});
            spawn.set(L"Owner",pawn); spawn.set(L"TransformScaleMethod",uint8_t{0}); spawn.run();
            auto* camera=spawn.get<UObject*>(); if(!camera) throw std::runtime_error("Filming camera spawn failed"); cinema_camera_=camera;
            Call finish(lib,L"FinishSpawningActor",4); finish.set(L"Actor",camera); finish.set(L"SpawnTransform",transform);
            finish.set(L"TransformScaleMethod",uint8_t{0}); finish.run();
            auto* component=inventory_object(camera,L"CameraComponent");
            if(!component) throw std::runtime_error("Filming camera component unavailable");
            invoke(component,L"SetFieldOfView",L"InFieldOfView",42.f);
            auto* aspect=component->GetPropertyByNameInChain(L"bConstrainAspectRatio");
            if(!aspect || !aspect->IsA<FBoolProperty>()) throw std::runtime_error("Filming camera aspect property unavailable");
            static_cast<FBoolProperty*>(aspect)->SetPropertyValueInContainer(component,false);
            Call facing(pawn,L"K2_GetActorRotation",1); facing.run();
            cinema_from_=cinema_to_={facing.get<std::array<double,3>>()[1],4.,1000.,0.};
            cinema_start_=GetTickCount64(); cinema_duration_=0; cinema_deadline_=cinema_start_+30000;
            cinema_update(true);
            capture_view(pc,camera);
            if(hud) invoke(hud,L"SetVisibility",L"InVisibility",uint8_t{2});
        } catch(...) { cinema_stop(); throw; }
    } else if(action=="inventory_cinema_move") {
        if(!cinema_camera_.Get()) throw std::runtime_error("Filming camera is not active");
        auto target=request.at("view").get<std::array<double,4>>();
        for(auto x:target) if(!std::isfinite(x)) throw std::runtime_error("Invalid filming coordinates");
        target[1]=std::clamp(target[1],-20.,35.); target[2]=std::clamp(target[2],160.,1400.); target[3]=std::clamp(target[3],-60.,80.);
        const auto now=GetTickCount64();
        double t=cinema_duration_?std::clamp(double(now-cinema_start_)/cinema_duration_,0.,1.):1.; t=t*t*(3-2*t);
        for(int i=0;i<4;++i) cinema_from_[i]+=(cinema_to_[i]-cinema_from_[i])*t;
        cinema_to_=target; cinema_start_=now; cinema_duration_=uint64_t(std::clamp(request.value("seconds",5.),0.,20.)*1000);
        cinema_deadline_=now+30000;
    } else throw std::runtime_error("Unknown filming command");
}
void InventoryUI::cinema_update(bool focused) {
    if(!cinema_camera_.Get()) return;
    auto* pawn=cinema_player_.Get(); auto* pc=cinema_pc_.Get();
    if(!focused || !pawn || !pc || inventory_object(pc,L"Pawn")!=pawn || inventory_bool(main_.Get(),L"bOpen") || GetTickCount64()>cinema_deadline_) { cinema_stop(); return; }
    auto now=GetTickCount64(); double t=cinema_duration_?std::clamp(double(now-cinema_start_)/cinema_duration_,0.,1.):1.; t=t*t*(3-2*t);
    auto view=cinema_from_; for(int i=0;i<4;++i) view[i]+=(cinema_to_[i]-view[i])*t;
    constexpr double radians=3.14159265358979323846/180.;
    Call center(pawn,L"K2_GetActorLocation",1); center.run(); auto location=center.get<std::array<double,3>>();
    location[0]+=std::cos(view[0]*radians)*std::cos(view[1]*radians)*view[2];
    location[1]+=std::sin(view[0]*radians)*std::cos(view[1]*radians)*view[2];
    location[2]+=std::sin(view[1]*radians)*view[2]+view[3];
    auto* camera=cinema_camera_.Get();
    Call move(camera,L"K2_SetActorLocationAndRotation",6); move.set(L"NewLocation",location);
    move.set(L"NewRotation",std::array<double,3>{-view[1],std::remainder(view[0]+180.,360.),0});
    move.set(L"bSweep",false); move.set(L"bTeleport",true); move.run();
    if(!move.get<bool>()) throw std::runtime_error("Filming camera movement failed");
}

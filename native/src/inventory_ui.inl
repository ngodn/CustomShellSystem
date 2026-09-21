// Included after the shared reflected UI helpers in engine.cpp.
namespace css {
#ifdef CSS_INVENTORY_DEV
#include "inventory_probe.inl"
#endif
namespace {
UObject* inventory_object(UObject* object,const wchar_t* name) {
    if(!object) return nullptr;
    auto* p=object->GetPropertyByNameInChain(name);
    if(!p || !p->IsA<FObjectProperty>() || p->GetElementSize()!=sizeof(UObject*)) return nullptr;
    return read<UObject*>(object,name);
}
bool inventory_bool(UObject* object,const wchar_t* name) {
    auto* p=object?object->GetPropertyByNameInChain(name):nullptr;
    return p && p->IsA<FBoolProperty>() && static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(object);
}
UObject* inventory_create(UObject* pc,UClass* type) {
    Call c(find(L"/Script/UMG.Default__WidgetBlueprintLibrary"),L"Create",4);
    c.set(L"WorldContextObject",pc); c.set(L"WidgetType",type); c.set(L"OwningPlayer",pc); c.run();
    auto* widget=c.get<UObject*>();
    if(!widget) throw std::runtime_error("Inventory widget creation failed");
    return widget;
}
void inventory_copy(UObject* to,UObject* from,const wchar_t* name) {
    auto* p=to->GetPropertyByNameInChain(name); auto* q=from->GetPropertyByNameInChain(name);
    if(!p || !q || !p->SameType(q)) throw std::runtime_error("Inventory style property mismatch");
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(to)+p->GetOffset_Internal(),reinterpret_cast<std::byte*>(from)+q->GetOffset_Internal());
}
void inventory_navigate(UObject* tabs,int index) {
    Call nav(tabs,L"NavigateToCustomIndex",3); nav.set(L"Index",index); nav.run();
    if(!nav.get<bool>(L"Success")) throw std::runtime_error("Inventory tab navigation rejected the index");
}
std::vector<UObject*> inventory_children(UObject* panel,int limit=64) {
    Call count(panel,L"GetChildrenCount",1); count.run(); auto n=count.get<int32_t>();
    if(n<0 || n>limit) throw std::runtime_error("Inventory child count out of bounds");
    std::vector<UObject*> result;
    for(int i=0;i<n;++i) { Call get(panel,L"GetChildAt",2); get.set(L"Index",i); get.run(); result.push_back(get.get<UObject*>()); }
    return result;
}
void inventory_order(UObject* panel,const std::vector<UObject*>& desired) {
    struct Slot { UObject* child; Margin padding; std::array<std::byte,8> size; uint8_t horizontal,vertical; };
    std::vector<Slot> slots;
    for(auto* child:desired) if(auto* slot=inventory_object(child,L"Slot");slot && slot->GetClassPrivate()->GetName()==L"HorizontalBoxSlot")
        slots.push_back({child,read<Margin>(slot,L"Padding"),read<std::array<std::byte,8>>(slot,L"Size"),read<uint8_t>(slot,L"HorizontalAlignment"),read<uint8_t>(slot,L"VerticalAlignment")});
    AssetLoadRoots roots; for(auto* child:desired) roots.keep(child);
    invoke(panel,L"ClearChildren");
    for(auto* child:desired) { Call add(panel,L"AddChild",2); add.set(L"content",child); add.run(); }
    for(const auto& saved:slots) if(auto* slot=inventory_object(saved.child,L"Slot")) {
        invoke(slot,L"SetPadding",L"InPadding",saved.padding);
        invoke(slot,L"SetSize",L"InSize",saved.size);
        invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",saved.horizontal);
        invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",saved.vertical);
    }
    if(inventory_children(panel)!=desired) throw std::runtime_error("Inventory child ordering did not apply");
}
}
#ifdef CSS_INVENTORY_DEV
#include "inventory_capture.inl"
#endif
#include "inventory_light.inl"
void InventoryUI::detach() {
    camera_tick_restore();
#ifdef CSS_INVENTORY_DEV
    cinema_stop(); capture_duration_=0;
#endif
    camera_stop();
    if(auto* tabs=tabs_.Get(); tabs && tab_.Get() && main_.Get() && active_) inventory_navigate(tabs,0);
    if(auto* page=extension_page_.Get()) invoke(page,L"RemoveFromParent");
    if(auto* tab=extension_tab_.Get()) invoke(tab,L"RemoveFromParent");
    extension_tab_.Reset();extension_page_.Reset();extension_canvas_.Reset();extension_active_=false;
    if(auto* page=page_.Get()) invoke(page,L"RemoveFromParent");
    if(auto* tab=tab_.Get()) invoke(tab,L"RemoveFromParent");
    for(const auto& [widget,padding]:top_padding_) if(auto* child=widget.Get()) if(auto* slot=inventory_object(child,L"Slot")) invoke(slot,L"SetPadding",L"InPadding",padding);
    top_padding_.clear();
    if(auto* tabs=tabs_.Get()) if(auto* nav=inventory_object(tabs,L"NavigationObject")) invoke(nav,L"GetNavigableChildren");
    tab_.Reset(); page_.Reset(); main_.Reset(); tabs_.Reset(); switcher_.Reset(); controller_.Reset();
    canvas_.Reset(); status_.Reset(); scroll_.Reset(); name_input_.Reset();
    choice_scroll_.Reset();choice_key_.clear();choice_selected_.clear();choice_offset_=0;choice_count_=0;
    hits_.clear(); rows_.clear(); sliders_.clear(); bindings_.clear(); transition_widgets_.clear();
    transition_started_=0; closing_=enter_transition_=false;
    active_=false; was_active_=false; dirty_=true;
}
Json InventoryUI::command(void* engine,const Json& command) {
    const auto action=command.at("action").get<std::string>();
#ifdef CSS_INVENTORY_DEV
    // Steam captures the game frame without needing Inventory or a player pawn.
    // Return before the inventory diagnostic walk, which needs those objects.
    if(action=="inventory_shot") {
        auto module=GetModuleHandleW(L"steam_api64.dll");
        if(!module) throw std::runtime_error("Steam screenshot module unavailable");
        auto get=reinterpret_cast<void*(*)()>(GetProcAddress(module,"SteamAPI_SteamScreenshots_v003"));
        auto trigger=reinterpret_cast<void(*)(void*)>(GetProcAddress(module,"SteamAPI_ISteamScreenshots_TriggerScreenshot"));
        if(!get || !trigger) throw std::runtime_error("Steam screenshot API unavailable");
        auto* screenshots=get(); if(!screenshots) throw std::runtime_error("Steam screenshots interface unavailable");
        trigger(screenshots);
        return {{"screenshot_requested",true},{"source","steam"}};
    }
#endif
    auto* viewport=read<UObject*>(static_cast<UObject*>(engine),L"GameViewport");
    auto* world=inventory_object(viewport,L"World");
    Call player(find(L"/Script/Engine.Default__GameplayStatics"),L"GetPlayerController",3);
    player.set(L"WorldContextObject",world); player.set(L"PlayerIndex",int32_t{0}); player.run();
    auto* pc=player.get<UObject*>();
    auto* handler=inventory_object(pc,L"User Interface Handler Component");
    auto* game=inventory_object(handler,L"WBP_Menu_Game");
    auto* main=inventory_object(game,L"WBP_Menu_Main");
    if(!main) throw std::runtime_error("Open Inventory before testing its CSS page");
    if(action=="inventory_attach" && !tab_.Get()) {
        auto* tabs=inventory_object(main,L"BP_HBC_Menu_Game");
        auto* pages=inventory_object(main,L"BP_WS_Menu_Game");
        auto* original=inventory_object(main,L"WBP_NBM_Inventory");
        if(!tabs || !pages || !original) throw std::runtime_error("Inventory layout is unavailable");
        Call count(pages,L"GetChildrenCount",1); count.run();
        if(count.get<int32_t>()!=3) throw std::runtime_error("Unexpected Inventory page count");
        controller_=pc; main_=main; tabs_=tabs; switcher_=pages;
        for(auto* child:inventory_children(tabs)) if(auto* slot=inventory_object(child,L"Slot")) top_padding_.push_back({WeakObject(child),read<std::array<float,4>>(slot,L"Padding")});
        AssetLoadRoots roots;
        auto create_page=[&](const wchar_t* label,WeakObject& tab_ref,WeakObject& page_ref,WeakObject& canvas_ref) {
            auto* tab=inventory_create(pc,original->GetClassPrivate());tab_ref=tab;roots.keep(tab);
            for(auto name:{L"FontData",L"RootSize",L"RootScale",L"DefaultColor",L"SelectedColor",L"bUseHighlight",L"HighlightY"}) inventory_copy(tab,original,name);
            Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
            convert.set(L"InString",FString(label));convert.run();
            auto* p=tab->GetPropertyByNameInChain(L"Text");
            if(!p || !p->SameType(convert.param(L"ReturnValue"))) throw std::runtime_error("Inventory title property mismatch");
            p->CopyCompleteValue(reinterpret_cast<std::byte*>(tab)+p->GetOffset_Internal(),convert.data(convert.param(L"ReturnValue")));
            invoke(tab,L"UpdateText");invoke(tab,L"CommitSize");invoke(tab,L"CommitScale");
            auto* page=inventory_create(pc,static_cast<UClass*>(main->GetClassPrivate()->GetSuperStruct()));page_ref=page;roots.keep(page);
            auto* tree=inventory_object(page,L"WidgetTree");if(!tree) throw std::runtime_error("Extension page has no widget tree");
            auto* canvas=construct(L"/Script/UMG.CanvasPanel",tree);canvas_ref=canvas;object_property(tree,L"RootWidget",canvas);
            Call add(pages,L"AddChild",2);add.set(L"content",page);add.run();
            Call button(tabs,L"AddChildToHorizontalBox",2);button.set(L"content",tab);button.run();
        };
        create_page(L"CSS",tab_,page_,canvas_);
        if(extensions_ && extensions_->ready()) create_page(L"CSSX",extension_tab_,extension_page_,extension_canvas_);
        invoke(inventory_object(tabs,L"NavigationObject"),L"GetNavigableChildren");
        dirty_=true; bind_inputs();
    } else if(action=="inventory_order") {
        inventory_navigate(tabs_.Get(),0);
        auto tabs=inventory_children(tabs_.Get()), pages=inventory_children(switcher_.Get());
        if(tabs.size()!=(extension_tab_.Get()?5:4) || pages.size()!=tabs.size()) throw std::runtime_error("Inventory ordering requires five pages");
        auto move_second=[](auto& values,UObject* value) { auto it=std::find(values.begin(),values.end(),value); if(it==values.end()) throw std::runtime_error("CSS child is missing"); values.erase(it); values.insert(values.begin()+1,value); };
        if(extension_tab_.Get()) {move_second(tabs,extension_tab_.Get());move_second(pages,extension_page_.Get());}
        move_second(tabs,tab_.Get()); move_second(pages,page_.Get());
        inventory_order(switcher_.Get(),pages); inventory_order(tabs_.Get(),tabs);
        // Five titles share the original top bar. Retain native type and spacing.
        for(auto* child:tabs) if(auto* slot=inventory_object(child,L"Slot")) {
            auto padding=top_padding_.empty()?std::array<float,4>{80,0,80,0}:top_padding_.front().second;
            for(const auto& [original,value]:top_padding_) if(original.Get()==child) padding=value;
            const float factor=extension_tab_.Get()?.6f:.75f; padding[0]*=factor; padding[2]*=factor;
            invoke(slot,L"SetPadding",L"InPadding",padding);
            invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{2});
            invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
        }
        invoke(inventory_object(tabs_.Get(),L"NavigationObject"),L"GetNavigableChildren");
        inventory_navigate(tabs_.Get(),0);
#ifdef CSS_INVENTORY_DEV
    } else if(action=="inventory_cssx") {
        dispatch_extension(command.at("event"));
    } else if(action.starts_with("inventory_cinema_")) {
        cinema_command(pc,command);
    } else if(action=="inventory_capture_row") {
        section_=std::clamp(command.value("section",0),0,3); row_=std::max(0,command.value("row",0));
        scroll_offset_=std::max(0.f,command.value("offset",0.f));
        if(auto* s=scroll_.Get()) invoke(s,L"SetScrollOffset",L"NewScrollOffset",scroll_offset_);
        dirty_=true;
    } else if(action=="inventory_capture_motion") {
        if(!active_) throw std::runtime_error("Filming motion requires CSS");
        capture_from_={yaw_,zoom_,pan_,frame_};
        capture_to_=command.at("view").get<std::array<double,4>>();
        for(auto x:capture_to_) if(!std::isfinite(x)) throw std::runtime_error("Invalid capture motion");
        capture_start_=GetTickCount64();
        capture_duration_=uint64_t(std::clamp(command.value("seconds",5.),.1,20.)*1000);

    } else if(action=="inventory_repair_padding") {
        // Repair this development session's old prototype, not a startup path.
        for(auto& [widget,padding]:top_padding_) padding={80,0,80,0};
        for(auto* child:inventory_children(tabs_.Get())) if(auto* slot=inventory_object(child,L"Slot")) invoke(slot,L"SetPadding",L"InPadding",Margin{60,0,60,0});
    } else if(action=="inventory_section") {
        section_=std::clamp(command.value("section",0),0,3); row_=std::max(0,command.value("row",0)); dirty_=true;
    } else if(action=="inventory_select") {
        inventory_navigate(inventory_object(main,L"BP_HBC_Menu_Game"),command.at("index").get<int32_t>());
    } else if(action=="inventory_test_motion") {
        if(!active_) throw std::runtime_error("Motion test requires the active CSS page");
        camera_move(command.at("movement").get<std::array<double,4>>());
    } else if(action=="inventory_test_pointer") {
        auto window=GetForegroundWindow(); DWORD pid=0; GetWindowThreadProcessId(window,&pid);
        if(!active_ || pid!=GetCurrentProcessId()) throw std::runtime_error("Pointer test requires focused CSS");
        auto point=command.at("point").get<std::array<double,2>>();
        RECT client{}; GetClientRect(window,&client);
        if(point[0]<0 || point[1]<0 || point[0]>=client.right || point[1]>=client.bottom) throw std::runtime_error("Pointer test outside game viewport");
        POINT pixel{LONG(point[0]),LONG(point[1])}; ClientToScreen(window,&pixel);
        SetCursorPos(pixel.x,pixel.y);
    } else if(action=="inventory_cssx_search") {
        auto* input=extension_search_input_.Get();
        if(!extension_picker_ || !input) throw std::runtime_error("CSSX option picker is not open");
        text_value(input,command.at("text").get<std::string>());
    } else if(action=="inventory_cssx_text") {
        auto* input=name_input_.Get();
        if(!extension_active_ || !input) throw std::runtime_error("CSSX text field is not open");
        if(command.contains("text")) text_value(input,command.at("text").get<std::string>());
    } else if(action=="inventory_test_mouse") {
        auto window=GetForegroundWindow(); DWORD pid=0; GetWindowThreadProcessId(window,&pid);
        if(!active_ || pid!=GetCurrentProcessId()) throw std::runtime_error("Mouse test requires focused CSS");
        INPUT input{}; input.type=INPUT_MOUSE;
        auto event=command.at("event").get<std::string>();
        if(event=="left_down") input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
        else if(event=="left_up") input.mi.dwFlags=MOUSEEVENTF_LEFTUP;
        else if(event=="right_down") input.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;
        else if(event=="right_up") input.mi.dwFlags=MOUSEEVENTF_RIGHTUP;
        else if(event=="wheel") { input.mi.dwFlags=MOUSEEVENTF_WHEEL; input.mi.mouseData=DWORD(command.at("delta").get<int>()); }
        else throw std::runtime_error("Unknown test mouse event");
        if(SendInput(1,&input,sizeof(input))!=1) throw std::runtime_error("Mouse test input rejected");
    } else if(action=="inventory_test_key") {
        DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid);
        if(pid!=GetCurrentProcessId()) throw std::runtime_error("Key test requires focused game");
        const auto key=command.at("key").get<std::string>();
        const WORD code=key.size()==1 && std::string("IWASDEFR").find(key[0])!=std::string::npos?WORD(key[0]):
            key=="Space"?VK_SPACE:key=="Shift"?VK_LSHIFT:key=="Ctrl"?VK_LCONTROL:key=="Escape"?VK_ESCAPE:key=="Home"?VK_HOME:key=="F8"?VK_F8:0;
        if(!code) throw std::runtime_error("Unsupported menu test key");
        INPUT input{}; input.type=INPUT_KEYBOARD; input.ki.wVk=code;
        input.ki.dwFlags=command.value("down",false)?0:KEYEVENTF_KEYUP;
        if(SendInput(1,&input,sizeof(input))!=1) throw std::runtime_error("Key test input rejected");
    } else if(action=="inventory_light_start") { light_start();
    } else if(action=="inventory_light_move") {
        if(!light_edit_) throw std::runtime_error("Preview light mode is not active");
        light_move(command.at("horizontal").get<double>(),command.at("vertical").get<double>());
    } else if(action=="inventory_light_stop") { light_stop();
    } else if(action=="inventory_detach") { detach();
#endif
    }
#ifdef CSS_INVENTORY_DEV
    auto result=inventory_probe(pc);
    result["css"]=diagnostics();
    result["light_control"]={{"active",light_edit_},{"yaw",light_orbit_.yaw},{"pitch",light_orbit_.pitch}};
    result["cinema"]={{"active",cinema_camera_.Get()!=nullptr},{"from",cinema_from_},{"to",cinema_to_}};
    if(auto* camera=cinema_camera_.Get()) {
        for(auto [label,object]:{std::pair{"camera",camera},std::pair{"player",cinema_player_.Get()}}) if(object) {
            Call pos(object,L"K2_GetActorLocation",1); pos.run();
            Call rot(object,L"K2_GetActorRotation",1); rot.run();
            result["cinema"][label]={{"location",pos.get<std::array<double,3>>()},{"rotation",rot.get<std::array<double,3>>()}};
        }
        auto* manager=inventory_object(pc,L"PlayerCameraManager");
        Call loc(manager,L"GetCameraLocation",1); loc.run();
        Call rot(manager,L"GetCameraRotation",1); rot.run();
        result["cinema"]["render_location"]=loc.get<std::array<double,3>>();
        result["cinema"]["render_rotation"]=rot.get<std::array<double,3>>();
        auto* component=inventory_object(camera,L"CameraComponent");
        Call cloc(component,L"K2_GetComponentLocation",1); cloc.run();
        Call crot(component,L"K2_GetComponentRotation",1); crot.run();
        result["cinema"]["component_location"]=cloc.get<std::array<double,3>>();
        result["cinema"]["component_rotation"]=crot.get<std::array<double,3>>();
    }
    result["hit_points"]=Json::array();
    for(const auto& hit:hits_) if(auto* widget=hit.widget.Get()) {
        Call geometry(widget,L"GetCachedGeometry",1); geometry.run();
        Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
        size.copy(L"Geometry",geometry,L"ReturnValue"); size.run(); auto extent=size.get<Vec2>();
        Call point(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"LocalToViewport",5);
        point.set(L"WorldContextObject",pc); point.copy(L"Geometry",geometry,L"ReturnValue");
        point.set(L"LocalCoordinate",Vec2{extent.x/2,extent.y/2}); point.run(); auto pixel=point.get<Vec2>(L"PixelPosition");
        result["hit_points"].push_back({{"action",hit.action},{"pixel",{pixel.x,pixel.y}}});
    }
    if(auto* mapping=inventory_object(handler,L"InputMapping")) {
        result["mapping_path"]=narrow(mapping->GetPathName());
        if(auto* p=mapping->GetPropertyByNameInChain(L"Mappings");p && p->IsA<FArrayProperty>()) {
            auto* a=static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(mapping)+p->GetOffset_Internal());
            if(values.Num()>256) throw std::runtime_error("Input map too large");
            auto* info=find(L"/Script/EnhancedInput.EnhancedActionKeyMapping");
            auto* ap=field(info,L"Action",8); auto* kp=info->GetPropertyByNameInChain(L"Key");
            for(int i=0;i<values.Num();++i) {
                auto* data=values.GetRawPtr(i); UObject* action{}; FName key{};
                std::memcpy(&action,data+ap->GetOffset_Internal(),8);
                auto* kn=field(find(L"/Script/InputCore.Key"),L"KeyName",sizeof(FName));
                std::memcpy(&key,data+kp->GetOffset_Internal()+kn->GetOffset_Internal(),sizeof(key));
                result["input_mappings"].push_back({{"action",action?narrow(action->GetPathName()):""},{"key",narrow(key.ToString())}});
            }
        }
    }
    if(auto* display=inventory_object(handler,L"ActiveDisplayMenu")) {
        auto* child=inventory_object(inventory_object(display,L"CameraActor_DisplayMenu"),L"ChildActor");
        result["camera_child"]=child?narrow(child->GetPathName()):"";
        auto* camera=view_target(pc); result["effective_camera"]=camera?narrow(camera->GetPathName()):"";
        if(camera) for(auto* p:static_cast<UStruct*>(camera->GetClassPrivate())->ForEachProperty()) result["camera_properties"].push_back({{"name",narrow(p->GetName())},{"size",p->GetElementSize()}});
    }
    return result;
#else
    return diagnostics();
#endif
}
}

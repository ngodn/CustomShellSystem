// Temporary read-only reflection probe. No input, menu, or camera mutations.
static Json inventory_probe(UObject* pc) {
    auto object_field=[](UObject* object,const wchar_t* name)->UObject* {
        if(!object) return nullptr;
        auto* p=object->GetPropertyByNameInChain(name);
        if(!p || !p->IsA<FObjectProperty>() || p->GetElementSize()!=sizeof(UObject*)) return nullptr;
        return read<UObject*>(object,name);
    };
    auto describe=[](UObject* object) {
        Json result=Json::object();
        if(!object) return result;
        result["path"]=narrow(object->GetPathName());
        result["class"]=narrow(object->GetClassPrivate()->GetPathName());
        if(object->GetFunctionByNameInChain(L"GetCachedGeometry")) {
            Call geometry(object,L"GetCachedGeometry",1); geometry.run();
            Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
            size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
            auto value=size.get<Vec2>(); result["local_size"]={value.x,value.y};
        }
        result["properties"]=Json::array();
        for(auto* p:static_cast<UStruct*>(object->GetClassPrivate())->ForEachProperty())
            result["properties"].push_back({{"name",narrow(p->GetName())},{"size",p->GetElementSize()}});
        for(auto name:{L"GetMenuWidget",L"GetCharacterWidget",L"GetMapWidgetIndex",L"HandleGameMenu",L"HandleMapTab",L"HandleTarstonesTab",L"HandleDisplayMenu",L"OnMenuOpen",L"OnMenuClose",L"UpdateSubTabIndex",L"UpdateActiveWidgetFromIndex",L"Initialize",L"NavigateToCustomIndex",L"HandleInput",L"GetDisplayMenuCharacter",L"GetCameraActor",L"RotateDisplayActor",L"UpdateDisplayYaw",L"ResetDisplayYaw",L"UpdateDisplayVector",L"ResetDisplayVector",L"SwitchActiveCamera",L"BindInputs",L"UnbindInputs",L"SetEnabledState"}) {
            if(auto* fn=object->GetFunctionByNameInChain(name)) {
                Json info={{"path",narrow(fn->GetPathName())},{"parameters",Json::array()}};
                for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm))
                    info["parameters"].push_back({{"name",narrow(p->GetName())},{"size",p->GetElementSize()}});
                result["functions"][narrow(name)]=info;
            }
        }
        for(auto name:{L"MainTabIndex",L"SubTabIndex",L"InventoryTabIndex",L"ActiveWidgetIndex",L"ActiveIndex",L"NavigableIndex",L"PanelIndex",L"PauseGameCounter"})
            if(auto* p=object->GetPropertyByNameInChain(name);p && p->IsA<FIntProperty>()) result[narrow(name)]=read<int32_t>(object,name);
        for(auto name:{L"bOpen",L"bEnabled",L"IsActive",L"ApplyLighting",L"bEnableVirtualCursor"})
            if(auto* p=object->GetPropertyByNameInChain(name);p && p->IsA<FBoolProperty>()) result[narrow(name)]=static_cast<FBoolProperty*>(p)->GetPropertyValueInContainer(object);
        if(object->GetFunctionByNameInChain(L"GetActiveWidgetIndex")) { Call c(object,L"GetActiveWidgetIndex",1); c.run(); result["active_index"]=c.get<int32_t>(); }
        for(auto name:{L"CurrentYaw",L"TargetYaw",L"DefaultYaw"})
            if(auto* p=object->GetPropertyByNameInChain(name);p && p->IsA<FDoubleProperty>()) result[narrow(name)]=read<double>(object,name);
        for(auto name:{L"CurrentLocation",L"TargetLocation"})
            if(auto* p=object->GetPropertyByNameInChain(name);p && p->IsA<FStructProperty>() && p->GetElementSize()==24) result[narrow(name)]=read<std::array<double,3>>(object,name);
        for(auto name:{L"InputAxis_Thumbstick_Right",L"InputAxis_Thumbstick_Left"})
            if(auto* p=object->GetPropertyByNameInChain(name);p && p->IsA<FStructProperty>() && p->GetElementSize()==16) result[narrow(name)]=read<std::array<double,2>>(object,name);
        if(auto* p=object->GetPropertyByNameInChain(L"AcceptedInputs");p && p->IsA<FArrayProperty>()) {
            auto* values=reinterpret_cast<TArray<uint8_t>*>(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal());
            auto* inner=static_cast<FArrayProperty*>(p)->GetInner();
            if(inner && inner->GetElementSize()==1 && values->Num()>=0 && values->Num()<=64) {
                result["accepted_inputs"]=Json::array();
                for(int i=0;i<values->Num();++i) result["accepted_inputs"].push_back((*values)[i]);
            }
        }
        return result;
    };
    auto* handler=object_field(pc,L"User Interface Handler Component");
    auto* game=object_field(handler,L"WBP_Menu_Game");
    auto* main=object_field(game,L"WBP_Menu_Main");
    auto* display=object_field(handler,L"ActiveDisplayMenu");
    Json result={{"handler",describe(handler)},{"game_menu",describe(game)},{"main_page",describe(main)},{"display",describe(display)}};
    if(handler && handler->GetFunctionByNameInChain(L"GetMapWidgetIndex")) {
        Call c(handler,L"GetMapWidgetIndex",2); c.run(); result["map_shortcut_index"]=c.get<int32_t>(); result["map_shortcut_valid"]=c.get<bool>(L"Success");
    }
    auto children=[&](UObject* panel) {
        Json out=Json::array();
        if(!panel || !panel->GetFunctionByNameInChain(L"GetChildrenCount")) return out;
        Call count(panel,L"GetChildrenCount",1); count.run();
        auto n=count.get<int32_t>();
        if(n<0 || n>128) throw std::runtime_error("Inventory probe child count exceeds bound");
        for(int i=0;i<n;++i) {
            Call child(panel,L"GetChildAt",2); child.set(L"Index",i); child.run();
            auto* widget=child.get<UObject*>();
            Json node=describe(widget); node["child_index"]=i;
            out.push_back(node);
        }
        return out;
    };
    for(auto name:{L"BP_HBC_Menu_Game",L"BP_WS_Menu_Game",L"WBP_MGT_Character",L"WBP_MGT_Tarstones",L"WBP_MGT_WorldMap",L"WBP_IL_GameplayMenu",L"WBP_NBM_Inventory",L"WBP_NBM_Tarstones",L"WBP_NBM_Map"}) {
        auto* object=object_field(main,name);
        result["main_objects"][narrow(name)]=describe(object);
        if(object && object->GetFunctionByNameInChain(L"GetChildrenCount")) result["main_objects"][narrow(name)]["children"]=children(object);
        if(auto* navigation=object_field(object,L"NavigationObject")) result["main_objects"][narrow(name)]["navigation"]=describe(navigation);
    }
    auto* character=object_field(main,L"WBP_MGT_Character");
    for(auto name:{L"WBP_IL_Inventory_Top",L"WBP_IL_Character_Navigation",L"WBP_IL_Character_Secondary",L"WBP_IL_Character_Back",L"WBP_CNH_Character",L"WBP_Equipment_Description",L"WBP_Player_Inventory",L"BP_HBC_InventoryFilter"})
        result["character_objects"][narrow(name)]=describe(object_field(character,name));
    if(display) {
        for(auto name:{L"CameraActor_DisplayMenu",L"CameraState",L"CharacterChildActor",L"Scene_DisplayActor",L"RectLight_Left",L"RectLight_AxeLight",L"SpotLight_Fill",L"SpotLight_Rim",L"PostProcess_DisplayMenuCharacter"}) {
            auto* component=object_field(display,name);
            result["display_components"][narrow(name)]=describe(component);
            if(component && component->GetFunctionByNameInChain(L"GetChildActor")) {
                Call c(component,L"GetChildActor",1); c.run(); auto* child=c.get<UObject*>();
                result["display_components"][narrow(name)]["child_actor"]=describe(child);
                if(auto* camera=object_field(child,L"CameraComponent")) result["display_components"][narrow(name)]["camera_component"]=describe(camera);
            }
        }
        if(auto* fn=display->GetFunctionByNameInChain(L"GetCameraActor"); fn && fn->GetNumParms()==2) {
            Call c(display,L"GetCameraActor",2); c.run();
            result["display_camera"]=describe(c.get<UObject*>());
        }
        if(auto* fn=display->GetFunctionByNameInChain(L"GetDisplayMenuCharacter"); fn && fn->GetNumParms()==1) {
            Call c(display,L"GetDisplayMenuCharacter",1); c.run();
            result["display_character"]=describe(c.get<UObject*>());
        }
    }
    return result;
}

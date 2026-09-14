namespace css {
void InventoryUI::build_extensions() {
    try {build_extension_page();}
    catch(const std::exception& error) {
        // A broken extension view must not detach CSS or the native game tabs.
        auto* canvas=extension_canvas_.Get();auto* page=extension_page_.Get();
        if(!canvas || !page) throw;
        invoke(canvas,L"ClearChildren");hits_.clear();rows_.clear();sliders_.clear();
        extension_loading_.clear();extension_description_.Reset();name_input_.Reset();transition_widgets_.clear();
        auto* tree=inventory_object(page,L"WidgetTree");
        auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
        const double height=std::max(360.,layout_size_[1]),width=std::max(640.,layout_size_[0])/height*1080.;
        Layout ui{tree,canvas,height/1080.,serif};
        ui.box(0,0,width,1080,Color{.005f,.004f,.003f,1});
        ui.label("This extension page could not be displayed",80,170,width-160,80,28);
        ui.label(error.what(),80,280,width-160,200,21);
        auto* back=ui.button("Return to library",80,540,300,48);
        hits_.push_back({WeakObject(back),{{"action","x_back"}},false});
        extension_details_=false;extension_error_=error.what();dirty_=false;enter_transition_=false;transition_started_=0;
    }
}
void InventoryUI::build_extension_page() {
    auto* page=extension_page_.Get();auto* canvas=extension_canvas_.Get();auto* pc=controller_.Get();
    if(!page || !canvas || !pc) return;
    if(auto* description=extension_description_.Get()) {Call offset(description,L"GetScrollOffset",1);offset.run();extension_description_offset_=offset.get<float>();}
    extension_description_.Reset();
    invoke(canvas,L"ClearChildren");hits_.clear();rows_.clear();sliders_.clear();scroll_.Reset();name_input_.Reset();transition_widgets_.clear();extension_loading_.clear();
    auto* tree=inventory_object(page,L"WidgetTree");
    Call geometry(switcher_.Get(),L"GetCachedGeometry",1);geometry.run();
    Call dimensions(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);dimensions.copy(L"Geometry",geometry,L"ReturnValue");dimensions.run();
    auto extent=dimensions.get<Vec2>();if(extent.x<640 || extent.y<360) return;layout_size_={extent.x,extent.y};
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    auto* title=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
    ExtensionKit ui{{{tree,canvas,extent.y/1080.,serif},title}};const double width=extent.x/extent.y*1080.;
    const auto light=ExtensionKit::ink,muted=ExtensionKit::secondary,accent=ExtensionKit::gold;
    auto bind=[&](UObject* widget,Json action){hits_.push_back({WeakObject(widget),std::move(action),false});};
    auto button=[&](const std::string& text,double x,double y,double w,double h,Json action,bool active=false,bool enabled=true,float size=20){auto* b=ui.button(text,x,y,w,h,active,enabled,size);bind(b,std::move(action));return b;};
    auto line=[&](double x,double y,double w){ui.image(load("/Game/Sparta/UI/Common/Textures/T_UI_DescriptionHeader_Divider.T_UI_DescriptionHeader_Divider"),x,y,w,2);};
    auto prompt=[&](const std::string& action,const std::string& text,double x,double y,double w,uint8_t icon) {
        auto* cls=static_cast<UClass*>(load("/Game/Sparta/UI/Core/Navigation/WBP_Prompt.WBP_Prompt_C"));
        auto* widget=inventory_create(pc,cls);
        for(const auto& binding:bindings_) if(binding.action==action) {
            object_property(widget,L"InputAction",binding.input_action.Get());
            for(auto key:binding.keys) if(!key.starts_with("Gamepad_")) {
                if(key=="SpaceBar") key="Spacebar";
                if(key=="LeftControl") key="Ctrl";
                for(const auto& [name,value]:inventory_keyboard_icons) if(name==key) {inventory_value(widget,L"KBMPrompt",value);break;}
                break;
            }
        }
        inventory_value(widget,L"ControllerPrompt",icon);
        inventory_value(widget,L"PromptSize",Vec2{80,80});
        inventory_value(widget,L"OverrideControllerSize",Vec2{80,80});
        inventory_value(widget,L"OverrideKBMSize",Vec2{80,80});
        ui.place(widget,x,y,28,28);invoke(widget,L"UpdatePrompt");invoke(widget,L"UpdatePromptSize");
        invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3});
        if(!text.empty()) ui.text(text,x+36,y,w-36,32,18,muted);
        input_prompt_=widget;
        return widget;
    };
    auto texture=[&](const std::string& file,double x,double y,double w,double h){
        if(file.empty()) return false;auto path=fs::u8path(file);if(!fs::exists(path)) return false;
        auto& saved=textures_[file];auto* image=saved.Get();
        if(!image){Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);import.set(L"WorldContextObject",pc);import.set(L"Filename",FString(path.c_str()));import.run();image=import.get<UObject*>();saved=image;}
        if(!image) return false;ui.image(image,x,y,w,h);return true;
    };
    if(!extensions_) {ui.text("CSSX is not installed",80,190,width-160,60,32,light);dirty_=false;return;}
    extension_library_=extensions_->request({{"op","library"}});
    const auto& entries=extension_library_.at("extensions");
    if(extension_id_.empty()) {
        ui.box(0,0,width,1080,Color{.004f,.004f,.004f,1});
        ui.label("CSSX",88,65,width-176,72,42,light);
        ui.text("Custom Shell System Extensions",90,133,width-180,40,21,muted);line(90,185,width-180);
        extension_paging_.count=entries.size();extension_paging_.normalize();
        const double gap=22,card_w=(width-180-2*gap)/3,card_h=228,top=214;
        const size_t first=extension_paging_.page*9;
        for(size_t slot=0;slot<9;++slot) {
            const double x=90+(slot%3)*(card_w+gap),y=top+(slot/3)*(card_h+gap);
            if(first+slot>=entries.size()) {ui.box(x,y,card_w,card_h,Color{.013f,.012f,.01f,.35f});continue;}
            const auto& entry=entries[first+slot];const bool selected=first+slot==extension_paging_.selected;
            ui.box(x-2,y-2,card_w+4,card_h+4,selected?accent:Color{.04f,.035f,.027f,.5f});
            auto* hit=button("",x,y,card_w,card_h,{{"action","x_open"},{"id",entry.at("id")}},selected,true);
            ui.box(x+1,y+1,card_w-2,card_h-2,Color{.009f,.009f,.008f,.94f});
            const auto banner=entry.value("banner",std::string{});
            if(!texture(banner,x+2,y+2,card_w-4,145)) {
                ui.box(x+2,y+2,card_w-4,145,Color{.025f,.025f,.02f,1});ui.sigil(x+card_w/2-29,y+27,58,accent);
            }
            ui.text(entry.at("title").get<std::string>(),x+18,y+153,card_w-36,34,24,light);
            const auto details=entry.value("available",true)?entry.at("author").get<std::string>()+"  /  "+entry.at("version").get<std::string>():"Unavailable: check extension log";
            ui.text(details,x+18,y+193,card_w-36,27,16,muted);
            rows_.push_back({{},WeakObject(hit),{{"action","x_open"},{"id",entry.at("id")}},nullptr,nullptr,nullptr,nullptr});

        }
        const double bottom=985;
        prompt("previous_section","",width/2-156,bottom+5,28,8);
        button("",width/2-160,bottom,50,38,{{"action","x_page"},{"direction",-1}},false,extension_paging_.page>0);
        ui.text(std::to_string(extension_paging_.page+1)+" / "+std::to_string(extension_paging_.pages()),width/2-54,bottom,110,38,20,light);
        prompt("next_section","",width/2+124,bottom+5,28,9);
        button("",width/2+110,bottom,50,38,{{"action","x_page"},{"direction",1}},false,extension_paging_.page+1<extension_paging_.pages());
        prompt("accept","Open extension",90,bottom,220,3);
        prompt("close","Close",width-240,bottom,160,5);
        if(entries.empty()) ui.text("No extensions installed. Add extension folders to CustomShellSystem/extensions.",180,420,width-360,100,26,light);
        if(!extension_library_.at("errors").empty()) ui.text("Some extensions could not load. See logs/cssx.jsonl.",90,1030,width-180,32,16,muted);
    } else {
        const Json* entry=nullptr;for(const auto& e:entries) if(e.at("id")==extension_id_) entry=&e;
        if(!entry) {extension_id_.clear();dirty_=true;return;}
        const bool inventory_layout=entry->at("layout")=="inventory";
        if(!inventory_layout) ui.box(0,0,width,1080,Color{.004f,.004f,.004f,1});
        const double left=inventory_layout?30:80,panel=inventory_layout?440:(width-220)*.57,right=inventory_layout?width-424:left+panel+60,info=width-right-70;
        button("< Library",left,55,180,38,{{"action","x_back"}},false,true,18);
        ui.label(entry->at("title").get<std::string>(),left,108,width-left-80,56,30,light);
        ui.text("by "+entry->at("author").get<std::string>()+"  /  "+entry->at("version").get<std::string>(),left,162,width-left-80,32,18,muted);
        try {extension_model_=extensions_->request({{"op","model"},{"id",extension_id_}});}
        catch(const std::exception& error){ui.text(error.what(),left,250,width-160,220,24,light);dirty_=false;return;}
        const auto& sections=extension_model_.at("sections");
        extension_section_=std::clamp(extension_section_,0,std::max(0,int(sections.size())-1));
        const int count=int(sections.size());
        const int tab_count=std::min(count,std::max(1,int((panel-100)/115)));
        const int first_tab=std::clamp(extension_section_-tab_count/2,0,std::max(0,count-tab_count));
        const double tab_width=(panel-100)/std::max(1,tab_count);
        ui.decoration("T_UI_Nav_TitleBG",left,207,panel,48);
        prompt("previous_section","",left+6,217,28,8);
        button("",left,208,40,42,{{"action","x_section_delta"},{"delta",-1}},false,count>1);
        for(int i=first_tab;i<first_tab+tab_count;++i) {
            const double x=left+50+(i-first_tab)*tab_width;
            button(sections[i].at("title").get<std::string>(),x,210,tab_width,38,{{"action","x_section"},{"section",i}},extension_section_==i,true,19);
            if(extension_section_==i) ui.decoration("T_UI_TopBarHighlightLine",x,245,tab_width,8);
        }
        prompt("next_section","",left+panel-34,217,28,9);
        button("",left+panel-40,208,40,42,{{"action","x_section_delta"},{"delta",1}},false,count>1);
        ui.panel(right-20,270,info+40,646);
        if(!sections.empty()) {
            const auto& controls=sections[extension_section_].at("controls");
            extension_row_=std::clamp(extension_row_,0,std::max(0,int(controls.size())-1));
            constexpr int visible=9;
            const int first=std::clamp(extension_row_-visible+1,0,std::max(0,int(controls.size())-visible));
            for(int i=first;i<std::min(first+visible,int(controls.size()));++i) {
                const auto& c=controls[i];
                auto* hit=ui.row(c,left,282+(i-first)*ExtensionKit::row_height,panel-14,i==extension_row_);
                bind(hit,{{"action","x_row"},{"row",i}});
            }
            if(controls.size()>visible) {
                const double track=visible*ExtensionKit::row_height;
                ui.box(left+panel-5,282,3,track,Color{.04f,.035f,.025f,1});
                ui.box(left+panel-5,282+track*first/controls.size(),3,track*visible/controls.size(),accent);
                button("<",left,890,44,38,{{"action","x_scroll"},{"delta",-visible}},false,first>0);
                ui.text(std::to_string(first+1)+" - "+std::to_string(std::min(first+visible,int(controls.size())))+" of "+std::to_string(controls.size()),left+60,897,panel-120,30,17,muted);
                button(">",left+panel-44,890,44,38,{{"action","x_scroll"},{"delta",visible}},false,first+visible<int(controls.size()));
            }
            if(!controls.empty()) {
                const auto& c=controls[extension_row_];const auto kind=c.at("type").get<std::string>();
                const bool enabled=extensions::interactive(c);
                ui.label(c.at("label").get<std::string>(),right,298,info,74,26,light);
                line(right,384,info);
                const auto detail_key=extension_id_+"/"+c.at("id").get<std::string>();
                if(extension_description_key_!=detail_key) {extension_description_key_=detail_key;extension_description_offset_=0;extension_details_=false;}
                extension_description_=ui.description(c.value("description",std::string{}),right,408,info,142);
                invoke(extension_description_.Get(),L"SetScrollOffset",L"NewScrollOffset",extension_description_offset_);
                if(extension_details_) ui.box(right-8,408,2,142,accent);
                button("",right,551,info,30,{{"action","x_details"}});
                prompt("secondary",extension_details_?"Up / Down scroll details":"Read details",right,552,info,4);
                const auto value=extensions::display_value(c);
                if(kind=="radio") {
                    const auto& options=c.at("options");
                    for(size_t i=0;i<options.size();++i) {
                        const double y=582+i*34;const bool chosen=options[i].at("id")==c.at("value");
                        button("",right,y,info,32,{{"action","x_value"},{"value",options[i].at("id")}},chosen,enabled);
                        ui.selection_mark(right+8,y+10,chosen);
                        ui.text(options[i].at("label").get<std::string>(),right+36,y+3,info-48,30,19,chosen?accent:muted);
                    }
                } else if(kind=="slider") {
                    auto* slider=construct(L"/Script/UMG.Slider",tree);
                    for(const auto& setting:{std::pair{L"SetMinValue","min"},std::pair{L"SetMaxValue","max"},std::pair{L"SetStepSize","step"},std::pair{L"SetValue","value"}})
                        invoke(slider,setting.first,L"InValue",c.at(setting.second).get<float>());
                    invoke(slider,L"SetSliderBarColor",L"InValue",Color{.08f,.07f,.05f,1});
                    invoke(slider,L"SetSliderHandleColor",L"InValue",accent);
                    invoke(slider,L"SetIsEnabled",L"bInIsEnabled",enabled);
                    ui.place(slider,right+8,615,info-16,36);
                    auto* label=ui.text(value,right,582,info,32,23,light);
                    sliders_.push_back({WeakObject(slider),WeakObject(label),{},{{"action","x_value"}},c.at("value").get<float>(),true});
                    prompt("left","",right,668,28,15);prompt("right","Adjust",right+36,668,info-36,16);
                } else if(kind=="number" || kind=="choice") {
                    button("<",right,600,48,48,{{"action","x_adjust"},{"delta",-1}},false,enabled);
                    button(">",right+info-48,600,48,48,{{"action","x_adjust"},{"delta",1}},false,enabled);
                    auto* value_text=ui.text(value,right+58,607,info-116,65,23,light);
                    invoke(value_text,L"SetJustification",L"InJustification",uint8_t{1});
                    prompt("left","",right,694,28,15);prompt("right","Adjust",right+36,694,info-36,16);
                } else if(kind=="text") {
                    ui.box(right,590,info,48,Color{.04f,.033f,.025f,1});
                    const auto key=extension_id_+"/"+c.at("id").get<std::string>();
                    if(extension_text_key_!=key) {extension_text_key_=key;extension_text_draft_=c.at("value").get<std::string>();}
                    name_input_=ui.text_input(extension_text_draft_,right+12,596,info-24,enabled);
                    button("Save text",right,654,info,46,{{"action","x_text"}},false,enabled);
                } else if(kind=="progress" || kind=="loading") {
                    const bool loading=kind=="loading" && c.at("value").get<bool>();
                    auto* bar=ui.progress(right,626,info,kind=="progress"?c.at("value").get<double>():1.,loading);
                    if(loading) extension_loading_.emplace_back(bar);
                    ui.text(value,right,578,info,34,23,light);
                } else if(kind!="label") {
                    const auto label=kind=="toggle"?(c.at("value").get<bool>()?"Disable":"Enable"):c.at("label").get<std::string>();
                    auto* action=button("",right,602,info,52,{{"action","x_activate"}},false,enabled);
                    ui.box(right,602,info,52,Color{.035f,.03f,.018f,.9f});
                    prompt("accept",label,right+14,614,info-28,3);
                    if(!enabled) invoke(action,L"SetIsEnabled",L"bInIsEnabled",false);
                }
                if(c.value("busy",false)) {
                    extension_loading_.emplace_back(ui.progress(right,890,info,0,true));
                }
            }
        }
        line(right,854,info);
        ui.text(extension_error_.empty()?extension_model_.value("status",std::string{}):extension_error_,right,866,info,45,17,muted);
        prompt("up","",left,968,28,13);prompt("down","Browse settings",left+36,968,220,14);
        button("",left,1020,180,38,{{"action","x_back"}});
        prompt("close","Library",left,1024,170,5);
        if(!extension_confirm_.is_null()) {
            ui.box(0,0,width,1080,Color{0,0,0,.88f});const auto text=extension_confirm_.value("message",std::string("Confirm this action?"));
            ui.box(width/2-320,320,640,360,Color{.016f,.014f,.01f,1});ui.text("Confirm action",width/2-290,350,580,50,30,light);
            ui.text(text,width/2-290,420,580,140,22,light);
            button("Cancel",width/2-275,596,250,48,{{"action","x_cancel"}});
            button("Confirm",width/2+25,596,250,48,{{"action","x_confirm"}},true);
        }
    }
    transition_widgets_.clear();
    for(auto* child:inventory_children(canvas,256))
        transition_widgets_.push_back({WeakObject(child),{extension_slide_*70.*ui.scale,0}});
    dirty_=false;
    if(enter_transition_) {transition_started_=GetTickCount64();enter_transition_=false;}
}
Json InventoryUI::dispatch_extension(const Json& action) {
    try {return dispatch_extension_action(action);}
    catch(const std::exception& e) {extension_error_=e.what();dirty_=true;return {};}
}
Json InventoryUI::dispatch_extension_action(const Json& action) {
    const auto name=action.value("action",std::string{});
    extension_error_.clear();
    if(!extension_confirm_.is_null() && name!="x_cancel" && name!="x_back" && name!="x_confirm") return {};
    if(name=="x_details") {extension_details_=!extension_details_;dirty_=true;return {};}
    if(name=="x_cancel") {extension_confirm_=nullptr;dirty_=true;return {};}
    if(name=="x_back") {if(extension_details_) {extension_details_=false;dirty_=true;return {};}if(!extension_confirm_.is_null()) extension_confirm_=nullptr;else {extension_id_.clear();extension_model_=nullptr;extension_details_=false;}dirty_=enter_transition_=true;return {};}
    if(name=="x_page") {extension_slide_=action.at("direction").get<int>()<0?-1:1;extension_paging_.slide(extension_slide_);dirty_=enter_transition_=true;return {};}
    if(name=="x_open") {extension_id_=action.at("id").get<std::string>();extension_section_=extension_row_=0;extension_confirm_=nullptr;dirty_=enter_transition_=true;return {};}
    if(name=="x_section" || name=="x_section_delta") {
        extension_slide_=name=="x_section_delta"?action.at("delta").get<int>():1;
        const int count=extension_model_.contains("sections")?static_cast<int>(extension_model_["sections"].size()):0;
        if(count) extension_section_=name=="x_section"?std::clamp(action.at("section").get<int>(),0,count-1):(extension_section_+action.at("delta").get<int>()+count)%count;
        extension_row_=0;dirty_=enter_transition_=true;return {};
    }
    if(name=="x_scroll" && extension_model_.contains("sections") && !extension_model_["sections"].empty()) {
        const int count=int(extension_model_["sections"][extension_section_]["controls"].size());
        extension_row_=std::clamp(extension_row_+action.at("delta").get<int>(),0,std::max(0,count-1));dirty_=true;return {};
    }
    if(name=="x_row") {extension_row_=action.at("row").get<int>();dirty_=true;return {};}
    if(extension_id_.empty() || !extension_model_.contains("sections") || extension_model_["sections"].empty()) return {};
    const auto& controls=extension_model_["sections"][extension_section_]["controls"];
    if(extension_row_<0 || extension_row_>=int(controls.size())) return {};
    const auto& c=controls[extension_row_];if(!extensions::interactive(c)) return {};
    Json event={{"id",c.at("id")}};const auto type=c.at("type").get<std::string>();
    if(name=="x_confirm") {event=extension_confirm_.at("event");event["confirmed"]=true;extension_confirm_=nullptr;}
    else if(name=="x_value" || name=="x_text") {
        if(name=="x_text" && type=="text") event["value"]=inventory_text(name_input_.Get(),4096);
        else if(name=="x_value" && (type=="radio" || type=="slider"))
            event["value"]=type=="slider"?Json(extensions::snap_value(c,action.at("value").get<double>())):action.at("value");
        else return {};
        if(c.contains("confirm")) {extension_confirm_={{"event",event},{"message",c.at("confirm")}};dirty_=true;return {};}
    }
    else if(name=="x_activate" || name=="x_adjust") {
        if(type=="toggle" && name=="x_activate") event["value"]=!c.at("value").get<bool>();
        else if(extensions::adjustable(c)) event["value"]=extensions::adjusted_value(c,action.value("delta",1));
        else if(type!="button" || name!="x_activate") return {};
        if(c.contains("confirm")) {extension_confirm_={{"event",event},{"message",c.at("confirm")}};dirty_=true;return {};}
    } else return {};
    try {extensions_->request({{"op","event"},{"id",extension_id_},{"event",event}});}
    catch(const std::exception& error) {extension_error_=error.what();}
    dirty_=true;return {};
}
bool InventoryUI::extension_input(const std::string& key) {
    if(key=="reset_view") return false;
    if(key=="close") {if(extension_id_.empty()) return false;dispatch_extension({{"action","x_back"}});return true;}
    if(!extension_confirm_.is_null()) {if(key=="accept") dispatch_extension({{"action","x_confirm"}});return true;}
    if(!extension_id_.empty() && key=="secondary") {dispatch_extension({{"action","x_details"}});return true;}
    if(extension_details_ && (key=="up" || key=="down")) {
        if(auto* description=extension_description_.Get()) {
            Call offset(description,L"GetScrollOffset",1);offset.run();
            invoke(description,L"SetScrollOffset",L"NewScrollOffset",std::max(0.f,offset.get<float>()+(key=="up"?-60.f:60.f)));
        }
        return true;
    }
    if(extension_id_.empty()) {
        if(key=="left" || key=="right" || key=="up" || key=="down") {extension_paging_.move(key=="left"?-1:key=="right"?1:0,key=="up"?-1:key=="down"?1:0);dirty_=true;}
        else if(key=="previous_section" || key=="next_section") dispatch_extension({{"action","x_page"},{"direction",key=="previous_section"?-1:1}});
        else if(key=="accept" && extension_library_.contains("extensions") && extension_paging_.selected<extension_library_["extensions"].size()) dispatch_extension({{"action","x_open"},{"id",extension_library_["extensions"][extension_paging_.selected]["id"]}});
    } else {
        if(key=="previous_section" || key=="next_section") dispatch_extension({{"action","x_section_delta"},{"delta",key=="previous_section"?-1:1}});
        else if(key=="up" || key=="down") {int count=extension_model_.contains("sections") && !extension_model_["sections"].empty()?static_cast<int>(extension_model_["sections"][extension_section_]["controls"].size()):0;extension_row_=std::clamp(extension_row_+(key=="up"?-1:1),0,std::max(0,count-1));dirty_=true;}
        else if(key=="left" || key=="right") dispatch_extension({{"action","x_adjust"},{"delta",key=="left"?-1:1}});
        else if(key=="accept") dispatch_extension({{"action","x_activate"}});
    }
    return true;
}
}

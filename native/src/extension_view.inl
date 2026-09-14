namespace css {
void InventoryUI::build_extensions() {
    auto* page=extension_page_.Get();auto* canvas=extension_canvas_.Get();auto* pc=controller_.Get();
    if(!page || !canvas || !pc) return;
    invoke(canvas,L"ClearChildren");hits_.clear();rows_.clear();sliders_.clear();scroll_.Reset();name_input_.Reset();transition_widgets_.clear();
    auto* tree=inventory_object(page,L"WidgetTree");
    Call geometry(switcher_.Get(),L"GetCachedGeometry",1);geometry.run();
    Call dimensions(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);dimensions.copy(L"Geometry",geometry,L"ReturnValue");dimensions.run();
    auto extent=dimensions.get<Vec2>();if(extent.x<640 || extent.y<360) return;layout_size_={extent.x,extent.y};
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    auto* title=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
    InventoryLayout ui{{tree,canvas,extent.y/1080.,serif},title};const double width=extent.x/extent.y*1080.;
    const Color light{.48f,.43f,.34f,1},muted{.27f,.24f,.19f,1},accent{.42f,.34f,.22f,1};
    auto bind=[&](UObject* widget,Json action){hits_.push_back({WeakObject(widget),std::move(action),false});};
    auto button=[&](const std::string& text,double x,double y,double w,double h,Json action,bool active=false,bool enabled=true,float size=20){auto* b=ui.button(text,x,y,w,h,active,enabled,size);bind(b,std::move(action));return b;};
    auto line=[&](double x,double y,double w){ui.image(load("/Game/Sparta/UI/Common/Textures/T_UI_DescriptionHeader_Divider.T_UI_DescriptionHeader_Divider"),x,y,w,2);};
    auto texture=[&](const std::string& file,double x,double y,double w,double h){
        if(file.empty()) return false;auto path=fs::u8path(file);if(!fs::exists(path)) return false;
        auto& saved=textures_[file];auto* image=saved.Get();
        if(!image){Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);import.set(L"WorldContextObject",pc);import.set(L"Filename",FString(path.c_str()));import.run();image=import.get<UObject*>();saved=image;}
        if(!image) return false;ui.image(image,x,y,w,h);return true;
    };
    if(!extensions_) {ui.label("CSSX is not installed",80,190,width-160,60,32,light);dirty_=false;return;}
    extension_library_=extensions_->request({{"op","library"}});
    const auto& entries=extension_library_.at("extensions");
    if(extension_id_.empty()) {
        ui.box(0,0,width,1080,Color{.004f,.004f,.004f,.97f});
        ui.label("CSSX",88,65,width-176,72,46,light);
        ui.label("Custom Shell System Extensions",90,133,width-180,40,21,muted);line(90,185,width-180);
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
            ui.label(entry.at("title").get<std::string>(),x+18,y+153,card_w-36,34,24,light);
            const auto details=entry.value("available",true)?entry.at("author").get<std::string>()+"  /  "+entry.at("version").get<std::string>():"Unavailable: check extension log";
            ui.label(details,x+18,y+193,card_w-36,27,16,muted);
            rows_.push_back({{},WeakObject(hit),{{"action","x_open"},{"id",entry.at("id")}},nullptr,nullptr,nullptr,nullptr});
            transition_widgets_.push_back({WeakObject(hit),{100,0}});
        }
        const double bottom=985;
        button("<",width/2-160,bottom,50,38,{{"action","x_page"},{"direction",-1}},false,extension_paging_.page>0);
        ui.label(std::to_string(extension_paging_.page+1)+" / "+std::to_string(extension_paging_.pages()),width/2-54,bottom,110,38,20,light);
        button(">",width/2+110,bottom,50,38,{{"action","x_page"},{"direction",1}},false,extension_paging_.page+1<extension_paging_.pages());
        ui.label(gamepad_?"D-pad Browse     A Open     LT / RT Page":"WASD Browse     Space Open     Z / X Page",90,bottom,width/2-260,45,18,muted);
        if(entries.empty()) ui.label("No extensions installed. Add extension folders to CustomShellSystem/extensions.",180,420,width-360,100,26,light);
        if(!extension_library_.at("errors").empty()) ui.label("Some extensions could not load. See logs/cssx.jsonl.",90,1030,width-180,32,16,muted);
    } else {
        const Json* entry=nullptr;for(const auto& e:entries) if(e.at("id")==extension_id_) entry=&e;
        if(!entry) {extension_id_.clear();dirty_=true;return;}
        const bool inventory_layout=entry->at("layout")=="inventory";
        if(!inventory_layout) ui.box(0,0,width,1080,Color{.004f,.004f,.004f,.97f});
        const double left=inventory_layout?30:80,panel=inventory_layout?440:(width-220)*.57,right=inventory_layout?width-424:left+panel+60,info=width-right-70;
        button("< Library",left,55,180,38,{{"action","x_back"}},false,true,18);
        ui.label(entry->at("title").get<std::string>(),left,110,width-left-80,56,34,light);
        ui.label("by "+entry->at("author").get<std::string>()+"  /  "+entry->at("version").get<std::string>(),left,162,width-left-80,32,18,muted);
        try {extension_model_=extensions_->request({{"op","model"},{"id",extension_id_}});}
        catch(const std::exception& error){ui.label(error.what(),left,250,width-160,220,24,light);dirty_=false;return;}
        const auto& sections=extension_model_.at("sections");
        extension_section_=std::clamp(extension_section_,0,std::max(0,int(sections.size())-1));
        const double tab_width=(panel-64)/std::max(size_t{1},sections.size());
        line(left,252,panel);
        button("<",left,210,28,36,{{"action","x_section_delta"},{"delta",-1}},false,sections.size()>1);
        for(size_t i=0;i<sections.size();++i) {
            button(sections[i].at("title").get<std::string>(),left+32+i*tab_width,210,tab_width,36,{{"action","x_section"},{"section",i}},extension_section_==int(i),true,17);
            if(extension_section_==int(i)) ui.image(load("/Game/Sparta/UI/Common/Textures/T_UI_TopBarHighlightLine.T_UI_TopBarHighlightLine"),left+32+i*tab_width,244,tab_width,8);
        }
        button(">",left+panel-28,210,28,36,{{"action","x_section_delta"},{"delta",1}},false,sections.size()>1);
        ui.box(right-14,235,info+28,670,Color{.008f,.007f,.006f,.75f});
        if(!sections.empty()) {
            const auto& controls=sections[extension_section_].at("controls");extension_row_=std::clamp(extension_row_,0,std::max(0,int(controls.size())-1));
            const int visible=9,first=std::max(0,extension_row_-visible+1);
            for(int i=first;i<std::min(first+visible,int(controls.size()));++i) {
                const auto& c=controls[i];const double y=282+(i-first)*69;const bool selected=i==extension_row_;
                if(selected) ui.box(left,y,panel,62,Color{.095f,.08f,.05f,.64f});
                const auto kind=c.at("type").get<std::string>();
                std::string value=kind=="toggle"?(c.at("value").get<bool>()?"On":"Off"):kind=="number"?c.at("value").dump():kind=="text"?c.at("value").get<std::string>():"";
                if(kind=="choice") for(const auto& o:c.at("options")) if(o.at("id")==c.at("value")) value=o.at("label").get<std::string>();
                button(c.at("label").get<std::string>(),left+12,y+5,panel-24,30,{{"action","x_row"},{"row",i}},selected,true,20);
                if(!value.empty()) ui.label(value,left+18,y+34,panel-36,26,17,muted);
            }
            if(!controls.empty()) {
                const auto& c=controls[extension_row_];const auto kind=c.at("type").get<std::string>();
                ui.label(c.at("label").get<std::string>(),right,268,info,75,28,light);line(right,350,info);
                ui.label(c.value("description",std::string{}),right,380,info,205,20,muted);
                auto action=Json{{"action","x_activate"}};
                if(kind=="number" || kind=="choice") {
                    button("<",right,625,48,48,{{"action","x_adjust"},{"delta",-1}},false,c.value("enabled",true));
                    button(">",right+info-48,625,48,48,{{"action","x_adjust"},{"delta",1}},false,c.value("enabled",true));
                    ui.label(kind=="number"?c.at("value").dump():c.at("value").get<std::string>(),right+58,630,info-116,70,22,light);
                } else if(kind!="label") button(kind=="toggle"?(c.at("value").get<bool>()?"Disable":"Enable"):"Apply",right,625,info,50,action,false,c.value("enabled",true));
            }
        }
        ui.label(extension_model_.value("status",std::string{}),right,760,info,126,18,muted);
        ui.label(gamepad_?"D-pad Browse / adjust     A Apply     LT / RT Section     B Library":"W / S Browse     A / D Adjust     Space Apply     Z / X Section     Esc Library",left,970,width-left-70,50,18,muted);
        if(!extension_confirm_.is_null()) {
            ui.box(0,0,width,1080,Color{0,0,0,.88f});const auto text=extension_confirm_.value("message",std::string("Confirm this action?"));
            ui.box(width/2-320,320,640,360,Color{.016f,.014f,.01f,1});ui.label("Confirm action",width/2-290,350,580,50,30,light);
            ui.label(text,width/2-290,420,580,140,22,light);
            button("Cancel",width/2-275,596,250,48,{{"action","x_cancel"}});
            button("Confirm",width/2+25,596,250,48,{{"action","x_confirm"}},true);
        }
    }
    dirty_=false;
    if(enter_transition_) {transition_started_=GetTickCount64();enter_transition_=false;}
}
Json InventoryUI::dispatch_extension(const Json& action) {
    const auto name=action.value("action",std::string{});
    if(name=="x_cancel") {extension_confirm_=nullptr;dirty_=true;return {};}
    if(name=="x_back") {if(!extension_confirm_.is_null()) extension_confirm_=nullptr;else {extension_id_.clear();extension_model_=nullptr;}dirty_=enter_transition_=true;return {};}
    if(name=="x_page") {extension_paging_.slide(action.at("direction").get<int>());dirty_=enter_transition_=true;return {};}
    if(name=="x_open") {extension_id_=action.at("id").get<std::string>();extension_section_=extension_row_=0;extension_confirm_=nullptr;dirty_=enter_transition_=true;return {};}
    if(name=="x_section" || name=="x_section_delta") {
        const int count=extension_model_.contains("sections")?static_cast<int>(extension_model_["sections"].size()):0;
        if(count) extension_section_=name=="x_section"?std::clamp(action.at("section").get<int>(),0,count-1):(extension_section_+action.at("delta").get<int>()+count)%count;
        extension_row_=0;dirty_=enter_transition_=true;return {};
    }
    if(name=="x_row") {extension_row_=action.at("row").get<int>();dirty_=true;return {};}
    if(extension_id_.empty() || !extension_model_.contains("sections") || extension_model_["sections"].empty()) return {};
    const auto& controls=extension_model_["sections"][extension_section_]["controls"];
    if(extension_row_<0 || extension_row_>=int(controls.size())) return {};
    const auto& c=controls[extension_row_];if(!c.value("enabled",true)) return {};
    Json event={{"id",c.at("id")}};const auto type=c.at("type").get<std::string>();
    if(name=="x_confirm") {event=extension_confirm_.at("event");event["confirmed"]=true;extension_confirm_=nullptr;}
    else if(name=="x_activate" || name=="x_adjust") {
        if(type=="toggle") event["value"]=!c.at("value").get<bool>();
        else if(type=="number") event["value"]=std::clamp(c.at("value").get<double>()+action.value("delta",1)*c.at("step").get<double>(),c.at("min").get<double>(),c.at("max").get<double>());
        else if(type=="choice") {const auto& options=c.at("options");int index=0;for(size_t i=0;i<options.size();++i) if(options[i].at("id")==c.at("value")) index=int(i);index=(index+action.value("delta",1)+int(options.size()))%int(options.size());event["value"]=options[index].at("id");}
        else if(type!="button") return {};
        if(c.contains("confirm")) {extension_confirm_={{"event",event},{"message",c.at("confirm")}};dirty_=true;return {};}
    } else return {};
    try {extensions_->request({{"op","event"},{"id",extension_id_},{"event",event}});}
    catch(const std::exception& error) {extension_model_["status"]=error.what();}
    dirty_=true;return {};
}
bool InventoryUI::extension_input(const std::string& key) {
    if(key=="reset_view") return false;
    if(key=="close") {if(extension_id_.empty()) return false;dispatch_extension({{"action","x_back"}});return true;}
    if(!extension_confirm_.is_null()) {if(key=="accept") dispatch_extension({{"action","x_confirm"}});return true;}
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

namespace css {
namespace {
template<class T> void inventory_value(UObject* object,const wchar_t* name,const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto* p=field(object,name,sizeof(T));
    std::memcpy(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal(),&value,sizeof(T));
}
constexpr Color inventory_ink{.287441f,.250158f,.223228f,1};
constexpr Color inventory_dim{.18f,.16f,.13f,1};
struct InventoryLayout : Layout {
    UObject* title_font;
    UObject* label(const std::string& text,double x,double y,double w,double h,float size,Color color=inventory_ink) {
        auto* widget=Layout::label(text,x,y,w,h,size,color);
        font_size(widget,size*float(scale),size>=22?title_font:serif);
        return widget;
    }
    UObject* button(const std::string& text,double x,double y,double w,double h,bool active=false,bool enabled=true,float size=20) {
        auto* widget=Layout::button(text,x,y,w,h,false,enabled,size);
        invoke(widget,L"SetBackgroundColor",L"InBackgroundColor",Color{1,1,1,1});
        Call child(widget,L"GetContent",1); child.run(); auto* text_block=child.get<UObject*>();
        font_size(text_block,size*float(scale),size>=22?title_font:serif);
        invoke(text_block,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{enabled?(active?Color{.55f,.48f,.34f,1}:inventory_ink):inventory_dim});
        return widget;
    }
};
bool inventory_key(UObject* pc,const std::string& key) {
    Call call(pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
    member(call.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(wide(key).c_str()));
    call.run(); return call.get<bool>();
}
std::string inventory_text(UObject* widget) {
    if(!widget) return {};
    Call text(widget,L"GetText",1); text.run();
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
    convert.copy(L"InText",text,L"ReturnValue"); convert.run();
    const auto& value=*static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars=value.GetCharArray();
    if(chars.Num()>256) throw std::runtime_error("Template name is too long");
    return chars.Num()?narrow(std::wstring(chars.GetData())):std::string{};
}
}
void InventoryUI::bind_inputs() {
    bindings_.clear();
    auto* pc=controller_.Get(); auto* handler=inventory_object(pc,L"User Interface Handler Component");
    auto* mapping=inventory_object(handler,L"InputMapping");
    auto* p=mapping?mapping->GetPropertyByNameInChain(L"Mappings"):nullptr;
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Native inventory input mapping is unavailable");
    Call subsystem(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetLocalPlayerSubSystemFromPlayerController",3);
    subsystem.set(L"PlayerController",pc); subsystem.set(L"Class",static_cast<UClass*>(find(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem"))); subsystem.run();
    auto* input=subsystem.get<UObject*>();
    if(!input) throw std::runtime_error("Native player input subsystem is unavailable");
    const std::map<std::wstring,std::string> actions={
        {L"IA_Menu_Up","up"},{L"IA_Menu_Down","down"},
        {L"IA_Menu_Left_Primary","left"},{L"IA_Menu_Right_Primary","right"},
        {L"IA_Menu_Left_Tertiary","previous_section"},{L"IA_Menu_Right_Tertiary","next_section"},
        {L"IA_Menu_Confirm_Primary_Press","accept"},{L"IA_Menu_Confirm_Secondary_Press","secondary"},
        {L"IA_Menu_Confirm_Tertiary_Press","tertiary"},{L"IA_Menu_Back","close"}};
    auto* a=static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(mapping)+p->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>256) throw std::runtime_error("Native input map exceeds bound");
    auto* ap=field(find(L"/Script/EnhancedInput.EnhancedActionKeyMapping"),L"Action",8);
    auto* kn=field(find(L"/Script/InputCore.Key"),L"KeyName",sizeof(FName));
    std::set<UObject*> seen;
    for(int i=0;i<values.Num();++i) {
        UObject* action{}; std::memcpy(&action,values.GetRawPtr(i)+ap->GetOffset_Internal(),8);
        if(!action || !seen.insert(action).second || !actions.contains(action->GetName())) continue;
        Binding binding; binding.action=actions.at(action->GetName()); binding.input_action=action;
        Call query(input,L"QueryKeysMappedToAction",2); query.set(L"Action",action); query.run();
        auto* out=query.param(L"ReturnValue");
        if(!out->IsA<FArrayProperty>()) throw std::runtime_error("Mapped input keys are not an array");
        auto* array=static_cast<FArrayProperty*>(out); FScriptArrayHelper keys(array,query.data(out));
        if(keys.Num()<0 || keys.Num()>32 || kn->GetOffset_Internal()+8>array->GetInner()->GetElementSize()) throw std::runtime_error("Mapped input key layout mismatch");
        for(int n=0;n<keys.Num();++n) {
            FName key{}; std::memcpy(&key,keys.GetRawPtr(n)+kn->GetOffset_Internal(),sizeof(key));
            auto name=narrow(key.ToString());
            // The analog sticks belong to the character view on this page.
            if(name=="Gamepad_LeftX" || name=="Gamepad_LeftY" || name=="Gamepad_RightX" || name=="Gamepad_RightY") continue;
            binding.keys.push_back(name);
        }
        bindings_.push_back(std::move(binding));
    }
    bindings_.push_back({"reset_view",{"Home","Gamepad_RightThumbstick"},false,0,{}});
}
void InventoryUI::build(const Catalog& catalog,const State& state,Appearance& appearance) {
    auto* page=page_.Get(); auto* canvas=canvas_.Get(); auto* pc=controller_.Get();
    if(!page || !canvas || !pc) return;
    if(auto* scroll=scroll_.Get()) { Call offset(scroll,L"GetScrollOffset",1); offset.run(); scroll_offset_=offset.get<float>(); }
    invoke(canvas,L"ClearChildren"); hits_.clear(); rows_.clear(); sliders_.clear(); scroll_.Reset(); name_input_.Reset();
    auto* tree=inventory_object(page,L"WidgetTree");
    // This page lives inside the game's scaled menu canvas, not the viewport.
    Call geometry(switcher_.Get(),L"GetCachedGeometry",1); geometry.run();
    Call dimensions(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
    dimensions.copy(L"Geometry",geometry,L"ReturnValue"); dimensions.run();
    auto viewport=dimensions.get<Vec2>();
    if(viewport.y<240 || viewport.x<320) return;
    layout_size_={viewport.x,viewport.y};
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    auto* trajan=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
    InventoryLayout ui{{tree,canvas,viewport.y/1080.0,serif},trajan};
    double width=viewport.x/viewport.y*1080., left=30, panel=440, right=width-424;
    const Color muted=inventory_ink, ivory{.48f,.43f,.34f,1}, gold{.42f,.34f,.22f,1};
    const Color panel_color{.012f,.011f,.008f,.28f};
    auto decoration=[&](const char* name,double x,double y,double w,double h) {
        std::string path="/Game/Sparta/UI/Common/Textures/"; path+=name; path+="."; path+=name;
        return ui.image(load(path),x,y,w,h);
    };
    auto thumbnail=[&](const Outfit& outfit,double x,double y,double size) {
        if(outfit.thumbnail.empty() || !fs::exists(outfit.thumbnail)) return;
        auto key=path_utf8(outfit.thumbnail); auto& cached=textures_[key]; auto* texture=cached.Get();
        if(!texture) {
            Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
            import.set(L"WorldContextObject",pc); import.set(L"Filename",FString(outfit.thumbnail.c_str())); import.run(); texture=import.get<UObject*>(); cached=texture;
        }
        if(texture) { decoration("T_UI_Icon_Shell_BG_Black",x-3,y-3,size+6,size+6); ui.image(texture,x,y,size,size); }
    };
    if(!logo_path_.empty() && fs::exists(logo_path_)) {
        auto& cached=textures_[path_utf8(logo_path_)]; auto* texture=cached.Get();
        if(!texture) {
            Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
            import.set(L"WorldContextObject",pc); import.set(L"Filename",FString(logo_path_.c_str())); import.run(); texture=import.get<UObject*>(); cached=texture;
        }
        if(texture) ui.image(texture,left-60,65,panel+120,(panel+120)/3.);
    }
    // Keep the logo clear of both navigation bars, using the space above the list.
    constexpr double section_y=270;
    decoration("T_UI_Nav_TitleBG",left-12,section_y-3,panel+24,49);
    decoration("T_UI_DescriptionHeader_Divider",right,252,360,2);
    ui.box(right-12,180,384,752,Color{.006f,.005f,.004f,.38f});
    auto prompt=[&](const std::string& action,const std::string& text,double x,double y,double w,uint8_t fallback=0) {
        auto* cls=static_cast<UClass*>(load("/Game/Sparta/UI/Core/Navigation/WBP_Prompt.WBP_Prompt_C"));
        auto* widget=inventory_create(pc,cls);
        for(const auto& b:bindings_) if(b.action==action) {
            object_property(widget,L"InputAction",b.input_action.Get());
            for(auto key:b.keys) if(!key.starts_with("Gamepad_")) {
                if(key=="SpaceBar") key="Spacebar";
                if(key=="LeftControl") key="Ctrl";
                for(const auto& [name,value]:inventory_keyboard_icons) if(name==key) { inventory_value(widget,L"KBMPrompt",value); break; }
                break;
            }
        }
        inventory_value(widget,L"ControllerPrompt",fallback);
        inventory_value(widget,L"PromptSize",Vec2{80,80});
        inventory_value(widget,L"OverrideControllerSize",Vec2{80,80});
        inventory_value(widget,L"OverrideKBMSize",Vec2{80,80});
        ui.place(widget,x,y,28,28); invoke(widget,L"UpdatePrompt"); invoke(widget,L"UpdatePromptSize");
        invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3});
        if(!text.empty()) ui.label(text,x+34,y+1,w-34,30,16,muted);
        return widget;
    };
    auto bind=[&](UObject* widget,Json action) { hits_.push_back({WeakObject(widget),std::move(action),false}); };
    const char* sections[]={"SHELL","COLOR","TEMPLATES"};
    std::array<UObject*,3> section_buttons{};
    std::array<double,3> section_widths{};
    double text_width=0;
    for(int i=0;i<3;++i) {
        auto* button=ui.button(sections[i],left,section_y,panel,40,section_==i,true,17);
        section_buttons[i]=button;
        invoke(button,L"ForceLayoutPrepass");
        Call child(button,L"GetContent",1); child.run();
        Call size(child.get<UObject*>(),L"GetDesiredSize",1); size.run();
        section_widths[i]=size.get<Vec2>().x/ui.scale;
        text_width+=section_widths[i];
        bind(button,{{"action","ui_section"},{"section",i}});
    }
    // Space the actual words, so the longer Templates label cannot crowd RT.
    const double section_gap=std::max(0.,(panel-56-text_width)/4);
    double section_x=left+28+section_gap;
    for(int i=0;i<3;++i) {
        auto* slot=inventory_object(section_buttons[i],L"Slot");
        invoke(slot,L"SetPosition",L"InPosition",Vec2{(section_x-section_gap/4)*ui.scale,section_y*ui.scale});
        invoke(slot,L"SetSize",L"InSize",Vec2{(section_widths[i]+section_gap/2)*ui.scale,40*ui.scale});
        if(section_==i) {
            // Preserve the native highlight's height so its soft line survives scaling.
            const double highlight_width=section_widths[i]*1.5;
            auto* highlight=decoration("T_UI_TopBarHighlightLine",section_x-(highlight_width-section_widths[i])/2,
                                       section_y+32,highlight_width,10);
            invoke(highlight,L"SetColorAndOpacity",L"InColorAndOpacity",Color{1,1,1,.6f});
        }
        section_x+=section_widths[i]+section_gap;
    }
    input_prompt_=prompt("previous_section","",left,section_y+7,28,8); prompt("next_section","",left+panel-28,section_y+7,28,9);
    auto selection=state.selections.find(appearance.shell);
    const Outfit* worn=nullptr;
    if(selection!=state.selections.end()) for(const auto& outfit:catalog.outfits) if(outfit.id==selection->second.outfit) worn=&outfit;
    auto scroll_begin=[&](int count,double row_height,double y=328) {
        auto* scroll=construct(L"/Script/UMG.ScrollBox",tree); scroll_=scroll;
        // Use the native inventory scrollbar brush without its stick listener.
        auto* character=inventory_object(main_.Get(),L"WBP_MGT_Character");
        auto* bar=inventory_object(inventory_object(character,L"WBP_CSB_Style2"),L"Image_Bar");
        if(bar) {
            auto* style=scroll->GetPropertyByNameInChain(L"WidgetBarStyle");
            auto* brush=bar->GetPropertyByNameInChain(L"Brush");
            if(style && style->IsA<FStructProperty>() && brush) {
                auto* info=find(L"/Script/SlateCore.ScrollBarStyle");
                for(auto name:{L"NormalThumbImage",L"HoveredThumbImage",L"DraggedThumbImage"}) {
                    auto* target=info->GetPropertyByNameInChain(name);
                    if(target && target->SameType(brush)) target->CopyCompleteValue(reinterpret_cast<std::byte*>(scroll)+style->GetOffset_Internal()+target->GetOffset_Internal(),reinterpret_cast<std::byte*>(bar)+brush->GetOffset_Internal());
                }
            }
        }
        invoke(scroll,L"SetAllowOverscroll",L"NewAllowOverscroll",false);
        invoke(scroll,L"SetAnimateWheelScrolling",L"bShouldAnimateWheelScrolling",true);
        invoke(scroll,L"SetAlwaysShowScrollbar",L"NewAlwaysShowScrollbar",count*row_height>600);
        invoke(scroll,L"SetScrollbarThickness",L"NewScrollbarThickness",Vec2{4*ui.scale,4*ui.scale});
        ui.place(scroll,left,y,panel,900-y);
        auto* size=construct(L"/Script/UMG.SizeBox",tree);
        invoke(size,L"SetHeightOverride",L"InHeightOverride",float(std::max(1,count)*row_height*ui.scale));
        auto* list=construct(L"/Script/UMG.CanvasPanel",tree); content(size,list);
        Call add(scroll,L"AddChild",2); add.set(L"content",size); add.run();
        ui.canvas=list; ui.origin_x=left; ui.origin_y=y;
        invoke(scroll,L"SetScrollOffset",L"NewScrollOffset",scroll_offset_);
    };
    auto scroll_end=[&] { ui.canvas=canvas; ui.origin_x=0; ui.origin_y=0; };
    auto row=[&](int index,const std::string& title,const std::string& subtitle,double y,double h,Json accept,Json previous=Json{},Json next=Json{},Json secondary=Json{},Json tertiary=Json{}) {
        bool selected=index==row_;
        auto* marker=ui.box(left,y,panel-10,h-5,selected?Color{.035f,.030f,.019f,.30f}:panel_color);
        auto* button=ui.button("",left,y,panel-10,h-5,selected);
        bind(button,{{"action","ui_row"},{"row",index},{"apply",false}});
        const double inset=section_==0 && index>0?88:18;
        auto single_line=[&](UObject* text) {
            invoke(text,L"SetAutoWrapText",L"InAutoTextWrap",false);
            invoke(text,L"SetClipping",L"InClipping",uint8_t{1});
            invoke(text,L"SetTextOverflowPolicy",L"InOverflowPolicy",uint8_t{1});
        };
        single_line(ui.label(title,left+inset,y+9,panel-inset-38,32,18,selected?ivory:muted));
        // Reserve a separate column for Equipped, including long variant names.
        const double status_width=section_==0 && index>0?100:20;
        if(!subtitle.empty()) single_line(ui.label(subtitle,left+inset,y+41,panel-inset-status_width,24,15,muted));
        if(selected) { decoration("T_UI_TopBarHighlightLine",left+8,y+1,panel-26,2); ui.box(left,y+8,1,h-20,gold); }
        rows_.push_back({WeakObject(marker),WeakObject(button),accept,previous,next,secondary,tertiary});
    };
    auto direction_hint=[&](bool horizontal,const std::string& label,double x,double y,double width) {
        if(gamepad_) prompt("",label,x,y,width,horizontal?11:10);
        else {
            prompt(horizontal?"left":"up","",x,y,28,horizontal?15:13);
            prompt(horizontal?"right":"down",label,x+34,y,width-34,horizontal?16:14);
        }
    };
    auto action_button=[&](const std::string& binding,const std::string& label,double y,Json action,uint8_t icon,bool enabled=true) {
        auto* button=ui.button("",right,y,360,43,false,enabled);
        bind(button,std::move(action));
        prompt(binding,label,right+12,y+7,330,icon);
    };
    auto detail=[&](const std::string& title,const std::string& subtitle,const std::string& body) {
        auto* header=ui.label(title,right,205,360,76,22);
        invoke(header,L"SetJustification",L"InJustification",uint8_t{1});
        auto* sub=ui.label(subtitle,right,294,360,34,16,gold);
        invoke(sub,L"SetJustification",L"InJustification",uint8_t{1});
        ui.label(body,right+16,350,328,126,16,muted);
    };
    if(section_==0) {
        row_=std::clamp(row_,0,int(catalog.outfits.size()));
        scroll_begin(int(catalog.outfits.size())+1,85);
        row(0,"Original appearance","Restore your current shell",328,85,{{"action","restore"}});
        for(size_t i=0;i<catalog.outfits.size();++i) {
            const auto& outfit=catalog.outfits[i]; size_t v=0;
            bool chosen=worn==&outfit;
            if(chosen) for(size_t j=0;j<outfit.variants.size();++j) if(outfit.variants[j].id==selection->second.variant) v=j;
            bool compatible=catalog.compatible(outfit.id,appearance.shell);
            auto wear=[&](size_t index) { return compatible?Json{{"action","select"},{"outfit",outfit.id},{"variant",outfit.variants[index].id}}:Json{}; };
            row(int(i)+1,outfit.name,outfit.variants[v].name,328+(i+1)*85,85,wear(v),wear((v+outfit.variants.size()-1)%outfit.variants.size()),wear((v+1)%outfit.variants.size()),{},{{"action","favorite"},{"outfit",outfit.id}});
            thumbnail(outfit,left+14,328+(i+1)*85+9,62);
            if(state.favorites.contains(outfit.id)) ui.star(left+panel-29,328+(i+1)*85+24,7,gold);
            if(chosen) ui.label("Equipped",left+panel-99,328+(i+1)*85+44,82,24,14,gold);
        }
        scroll_end();
        if(catalog.outfits.empty()) ui.label(catalog.empty_message(),left+18,435,panel-36,130,18,muted);
        if(row_==0) {
            detail("Original appearance","Your current shell","Restore the appearance supplied by the game and any installed base replacements. Your shell's abilities stay the same.");
            action_button("accept","Restore original",520,rows_[0].accept,3);
        } else {
            const auto& outfit=catalog.outfits[row_-1];
            detail(outfit.name,"By "+outfit.author,outfit.description.empty()?"Choose an outfit variant. Appearance changes keep your current shell's abilities.":outfit.description);
            auto& selected=rows_[row_];
            std::string variant=outfit.variants.front().name;
            if(worn==&outfit) for(const auto& v:outfit.variants) if(v.id==selection->second.variant) variant=v.name;
            ui.label("Variant",right,488,360,28,16,muted);
            auto* name=ui.label(variant,right+44,530,272,55,21,ivory);
            invoke(name,L"SetJustification",L"InJustification",uint8_t{1});
            if(outfit.variants.size()>1) {
                bind(ui.button("<",right,520,44,44),selected.previous);
                bind(ui.button(">",right+316,520,44,44),selected.next);
                direction_hint(true,"Change variant",right+72,595,280);
            }
            action_button("accept","Wear",655,selected.accept,3,!selected.accept.is_null());
            action_button("tertiary",state.favorites.contains(outfit.id)?"Remove favorite":"Add favorite",713,selected.tertiary,2);
        }
    } else if(section_==1) {
        if(!worn || worn->colors_for(selection->second.variant).controls.empty()) detail("Colors","No editable parts","Wear an outfit with color options to customize it here.");
        else {
            const auto& options=worn->colors_for(selection->second.variant); const auto& custom=selection->second.colors;
            auto values=color_values(options,custom); size_t palette=0;
            for(size_t i=0;i<options.palettes.size();++i) if(options.palettes[i].id==custom.palette) palette=i+1;
            auto palette_action=[&](size_t i) { return Json{{"action","palette"},{"palette",i?options.palettes[i-1].id:"original"}}; };
            row_=std::clamp(row_,0,int(options.controls.size()));
            scroll_begin(int(options.controls.size())+1,68);
            row(0,palette?options.palettes[palette-1].name:"Original colors","Color preset",328,68,palette_action(0),palette_action((palette+options.palettes.size())%(options.palettes.size()+1)),palette_action((palette+1)%(options.palettes.size()+1)));
            for(size_t i=0;i<options.controls.size();++i) {
                const auto& c=options.controls[i];
                Json minus={{"action","color"},{"control",c.id},{"channel",c.scalar?0:color_channel_},{"delta",-1}},plus=minus; plus["delta"]=1;
                row(int(i)+1,c.name,c.scalar?"Intensity":"RGB color",328+(i+1)*68,68,{{"action","reset_color"},{"control",c.id}},minus,plus,{{"action","ui_channel"}},{{"action","palette"},{"palette","original"}});
            }
            scroll_end();
            if(row_==0) {
                detail("Color preset",worn->name,"Use Left / Right to choose a palette. Original restores the author's colors.");
                for(size_t i=0;i<=options.palettes.size();++i) bind(ui.button(i?options.palettes[i-1].name:"Original",right,510+i*46,360,42,palette==i,true,21),palette_action(i));
                direction_hint(true,"Change color preset",right,854,360);
                action_button("accept","Restore original colors",892,palette_action(0),3);
            } else {
                const auto& control=options.controls[row_-1]; auto value=values.contains(control.id)?values.at(control.id):control.value;
                detail(control.name,worn->name,control.scalar?"Adjust the intensity for this part.":"Adjust Red, Green and Blue. Select a channel, then adjust it with Left / Right or its slider.");
                const char* channels[]={"Red","Green","Blue"};
                for(int channel=0;channel<(control.scalar?1:3);++channel) {
                    double y=510+channel*80;
                    auto* heading=ui.label(control.scalar?"Intensity":channels[channel],right,y,230,28,19,control.scalar || channel==color_channel_?gold:ivory);
                    auto* slider=construct(L"/Script/UMG.Slider",tree);
                    invoke(slider,L"SetMinValue",L"InValue",control.minimum); invoke(slider,L"SetMaxValue",L"InValue",control.maximum);
                    invoke(slider,L"SetStepSize",L"InValue",control.step); invoke(slider,L"SetValue",L"InValue",value[channel]);
                    invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1}); invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                    ui.place(slider,right,y+29,290,30);
                    auto* label=ui.label(slider_text(value[channel],control.scalar),right+305,y+29,55,30,18);
                    sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),{{"action","color"},{"control",control.id},{"channel",channel},{"refresh",false}},value[channel],control.scalar});
                }
                direction_hint(true,control.scalar?"Adjust intensity":"Adjust selected channel",right,756,360);
                if(!control.scalar) action_button("secondary","Select next channel",795,rows_[row_].secondary,4);
                action_button("accept","Reset part",841,rows_[row_].accept,3);
                action_button("tertiary","Reset all colors",887,rows_[row_].tertiary,2);
            }
        }
    } else {
        std::vector<std::string> names; for(const auto& [name,_]:state.presets) names.push_back(name);
        row_=std::clamp(row_,0,int(names.size()));
        scroll_begin(int(names.size())+1,77);
        row(0,"New template","Save your current appearance",328,77,{{"action","ui_save_template"}});
        for(size_t i=0;i<names.size();++i) row(int(i)+1,names[i],"Saved appearance and colors",328+(i+1)*77,77,{{"action","load_look"},{"name",names[i]}},{},{},{{"action","save_look"},{"name",names[i]}},{{"action","delete_look"},{"name",names[i]}});
        scroll_end();
        auto selected=row_?names[row_-1]:std::string{};
        detail(row_?selected:"New template","Appearance and colors",row_?"Load this template, replace it with your current appearance, or give it a new name.":"Choose a name, then save. A controller can save with the suggested name.");
        auto* input=construct(L"/Script/UMG.EditableText",tree); name_input_=input;
        std::string suggested=selected;
        if(suggested.empty()) { int n=1; do { suggested="look."+std::to_string(n++); } while(state.presets.contains(suggested)); }
        text_value(input,suggested);
        Call current_font(input,L"GetFont",1); current_font.run();
        Call set_font(input,L"SetFont",1); set_font.copy(L"InFontInfo",current_font,L"ReturnValue");
        auto* font=set_font.param(L"InFontInfo"); auto* font_info=find(L"/Script/SlateCore.SlateFontInfo");
        member(set_font.data(font),font->GetElementSize(),font_info,L"FontObject",serif);
        member(set_font.data(font),font->GetElementSize(),font_info,L"Size",18*float(ui.scale));
        member(set_font.data(font),font->GetElementSize(),font_info,L"TypefaceFontName",FName(L"Regular")); set_font.run();
        ui.box(right,502,360,48,Color{.04f,.035f,.025f,.7f}); ui.place(input,right+12,507,336,40);
        ui.label("Letters, numbers, periods, underscores or hyphens",right,564,360,55,16,muted);
        if(!row_) action_button("accept","Save template",631,{{"action","ui_save_template"}},3);
        else {
            action_button("accept","Load template",631,rows_[row_].accept,3);
            action_button("secondary","Replace with current look",681,rows_[row_].secondary,4);
            bind(ui.button("Rename",right,741,360,43,false,true,20),{{"action","ui_rename_template"},{"name",selected}});
            action_button("tertiary","Delete template",801,rows_[row_].tertiary,2);
        }
    }
    decoration("T_UI_DescriptionHeader_Divider",left,931,panel,2);
    decoration("T_UI_DescriptionHeader_Divider",right,931,360,2);
    status_=ui.label("",right,953,360,60,16,muted);
    direction_hint(false,section_==0?"Browse shells":section_==1?"Browse color parts":"Browse templates",left,947,panel);
    bind(ui.button("",left,1025,140,38),{{"action","ui_close"}});
    prompt("close","Close",left,1030,140,5);
    bind(ui.button("",width/2-75,980,160,40),{{"action","ui_reset_view"}});
    auto* reset_prompt=prompt("reset_view","Reset view",width/2-70,985,155,0);
    inventory_value(reset_prompt,L"ControllerPrompt",uint8_t{29});
    for(const auto& [key,value]:inventory_keyboard_icons) if(key=="Home") inventory_value(reset_prompt,L"KBMPrompt",value);
    invoke(reset_prompt,L"UpdatePrompt");
    if(gamepad_) {
        prompt("","Rotate / zoom",width/2-210,1030,220,30);
        prompt("","Move framing",width/2+30,1030,210,21);
    } else {
        auto* hint=ui.label("Right-drag rotate  /  Wheel zoom  /  Left-drag move",width/2-255,1030,510,34,15);
        invoke(hint,L"SetJustification",L"InJustification",uint8_t{1});
    }
    transition_widgets_.clear();
    for(auto* child:inventory_children(canvas)) if(auto* slot=inventory_object(child,L"Slot")) {
        Call position(slot,L"GetPosition",1); position.run(); auto p=position.get<Vec2>();
        const double x=p.x/ui.scale;
        transition_widgets_.push_back({WeakObject(child),{x<left+panel+25?-150.*ui.scale:x>=right-25?150.*ui.scale:0.,x>=left+panel+25 && x<right-25?30.*ui.scale:0.}});
    }
    if(enter_transition_) { transition_started_=GetTickCount64(); enter_transition_=false; }
    last_message_.clear(); dirty_=false;
}
void InventoryUI::close_menu() {
    if(!active_ || !inventory_bool(main_.Get(),L"bOpen")) return;
    camera_stop();
    auto* handler=inventory_object(controller_.Get(),L"User Interface Handler Component");
    // Inventory uses this path to close. Opening must go through native input.
    Call close(handler,L"HandleGameMenu",2); close.set(L"SubTabIndex",int32_t{0}); close.set(L"AllowClose",true); close.run();
    active_=was_active_=closing_=false; transition_started_=0;
}
void InventoryUI::animate(uint64_t now) {
    if(!transition_started_) return;
    const double t=std::clamp((now-transition_started_)/(closing_?160.:500.),0.,1.);
    const double eased=1.-std::pow(1.-t,3.);
    const double opacity=closing_?1.-eased:eased;
    for(const auto& item:transition_widgets_) if(auto* widget=item.widget.Get()) {
        invoke(widget,L"SetRenderOpacity",L"InOpacity",float(opacity));
        invoke(widget,L"SetRenderTranslation",L"Translation",Vec2{item.offset[0]*(1.-opacity),item.offset[1]*(1.-opacity)});
    }
    if(t>=1.) { transition_started_=0; if(closing_) close_menu(); }
}
Json InventoryUI::dispatch(Json action,const State& state) {
    if(action.is_null()) return {};
    auto name=action.value("action","");
    if(name.starts_with("x_")) return dispatch_extension(action);
    if(name=="ui_section") { const int next=std::clamp(action.at("section").get<int>(),0,2); if(next==section_) return {}; section_=next; enter_transition_=true; row_=0; scroll_offset_=0; if(auto* s=scroll_.Get()) invoke(s,L"SetScrollOffset",L"NewScrollOffset",0.f); dirty_=true; return {}; }
    if(name=="ui_row") { row_=std::clamp(action.at("row").get<int>(),0,std::max(0,int(rows_.size())-1)); dirty_=true; if(section_!=1 && action.value("apply",false) && !rows_.empty()) return dispatch(rows_[row_].accept,state); return {}; }
    if(name=="ui_channel") { color_channel_=(color_channel_+1)%3; dirty_=true; return {}; }
    if(name=="ui_reset_view") { camera_stop(); camera_start(); return {}; }
    if(name=="ui_close") {
        if(active_ && !closing_) { closing_=true; transition_started_=GetTickCount64(); }
        return {};
    }
    if(name=="ui_save_template") return {{"action","save_look"},{"name",inventory_text(name_input_.Get())}};
    if(name=="ui_rename_template") return {{"action","rename_look"},{"name",action.at("name")},{"new_name",inventory_text(name_input_.Get())}};
    return action;
}
void InventoryUI::camera_start() {
    auto* handler=inventory_object(controller_.Get(),L"User Interface Handler Component");
    auto* display=inventory_object(handler,L"ActiveDisplayMenu");
    if(!display || !display->GetPropertyByNameInChain(L"TargetYaw")) return;
    display_=display; yaw_before_=read<double>(display,L"TargetYaw"); yaw_=yaw_before_;
    location_before_=read<std::array<double,3>>(display,L"TargetLocation");
    auto* camera=inventory_object(inventory_object(display,L"CameraActor_DisplayMenu"),L"ChildActor");
    auto* component=inventory_object(camera,L"CameraComponent");
    if(component && component->GetPropertyByNameInChain(L"CurrentFocalLength")) {
        camera_component_=component; lens_before_=read<float>(component,L"CurrentFocalLength");
        camera_rotation_before_=read<std::array<double,3>>(component,L"RelativeRotation");
        camera_location_before_=read<std::array<double,3>>(component,L"RelativeLocation");
        Call rotation(component,L"K2_GetComponentRotation",1); rotation.run(); camera_world_rotation_=rotation.get<std::array<double,3>>();
        Call location(component,L"K2_GetComponentLocation",1); location.run(); camera_world_location_=location.get<std::array<double,3>>();
    }
    zoom_=frame_=pan_=0; motion_.reset(); drag_pan_=drag_rotate_=false; mouse_left_=mouse_right_=false;
}
void InventoryUI::camera_stop() {
    motion_.reset(); drag_pan_=drag_rotate_=false;
    if(auto* display=display_.Get()) { invoke(display,L"UpdateDisplayYaw",L"NewValue",yaw_before_); invoke(display,L"UpdateDisplayVector",L"NewValue",location_before_); }
    if(auto* camera=camera_component_.Get()) {
        invoke(camera,L"SetCurrentFocalLength",L"InFocalLength",lens_before_);
        Call rotate(camera,L"K2_SetRelativeRotation",4); rotate.set(L"NewRotation",camera_rotation_before_); rotate.set(L"bSweep",false); rotate.set(L"bTeleport",true); rotate.run();
        Call location(camera,L"K2_SetRelativeLocation",4); location.set(L"NewLocation",camera_location_before_); location.set(L"bSweep",false); location.set(L"bTeleport",true); location.run();
    }
    display_.Reset(); camera_component_.Reset();
}
void InventoryUI::camera_update(double delta,bool invert_x) {
    auto* display=display_.Get(); auto* handler=inventory_object(controller_.Get(),L"User Interface Handler Component");
    if(!display || !handler) return;
    auto right=read<std::array<double,2>>(handler,L"InputAxis_Thumbstick_Right");
    auto left=read<std::array<double,2>>(handler,L"InputAxis_Thumbstick_Left");
    camera_move(motion_.step(right,left,delta,invert_x));
    auto* pc=controller_.Get();
    const bool left_down=inventory_key(pc,"LeftMouseButton"),right_down=inventory_key(pc,"RightMouseButton");
    Call mouse(pc,L"GetMousePosition",3); mouse.run();
    if(!gamepad_ && mouse.get<bool>()) {
        Call size(find(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetViewportSize",2);
        size.set(L"WorldContextObject",pc); size.run(); auto viewport=size.get<Vec2>();
        if(viewport.x>0 && viewport.y>0 && layout_size_[1]>0) {
            const double width=layout_size_[0]/layout_size_[1]*1080.;
            std::array<double,2> point{mouse.get<float>(L"LocationX")/viewport.x*width,mouse.get<float>(L"LocationY")/viewport.y*1080.};
            const bool center=point[0]>490 && point[0]<width-440 && point[1]>90 && point[1]<970;
            if(!left_down) drag_pan_=false;
            else if(!mouse_left_ && center) drag_pan_=true;
            if(!right_down) drag_rotate_=false;
            else if(!mouse_right_ && center) drag_rotate_=true;
            std::array<double,4> movement{};
            if((drag_pan_ && mouse_left_) || (drag_rotate_ && mouse_right_)) {
                auto dx=point[0]-mouse_before_[0],dy=point[1]-mouse_before_[1];
                if(std::abs(dx)<150 && std::abs(dy)<150) {
                    if(drag_rotate_) movement[0]=dx*.45*(invert_x?-1:1);
                    if(drag_pan_) { movement[2]=dx*.24/(1+zoom_); movement[3]=-dy*.24/(1+zoom_); }
                }
            }
            if(center) {
                Call wheel(pc,L"GetInputAnalogKeyState",2); auto* key=wheel.param(L"Key");
                member(wheel.data(key),key->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(L"MouseWheelAxis"));
                wheel.run(); movement[1]=std::clamp(double(wheel.get<float>()),-1.,1.)*.08;
            }
            camera_move(movement); mouse_before_=point;
        }
    } else drag_pan_=drag_rotate_=false;
    mouse_left_=left_down; mouse_right_=right_down;
}
void InventoryUI::camera_move(const std::array<double,4>& movement) {
    auto* display=display_.Get(); if(!display) return;
    auto [x,z,h,v]=movement;
    if(x) {
        // Native UpdateYaw uses scalar FInterpTo, so wrapping at 180 reverses it.
        yaw_+=x; invoke(display,L"UpdateDisplayYaw",L"NewValue",yaw_);
        // CSS smooths velocity. Settle the native interpolation to avoid a second
        // half-second lag on top of that response. Only touch the active CSS page.
        invoke(display,L"UpdateYaw",L"DeltaTime",1.);
    }
    if(auto* camera=camera_component_.Get()) {
        if(z) { zoom_=std::clamp(zoom_+z,-.45,1.); invoke(camera,L"SetCurrentFocalLength",L"InFocalLength",float(lens_before_*(1+zoom_))); }
        if(h || v) {
            pan_=std::clamp(pan_+h,-90.,90.); frame_=std::clamp(frame_+v,-70.,70.);
            // Move the camera opposite the stick in its screen plane. The
            // character follows the stick without leaving the native light rig.
            const double p=camera_world_rotation_[0]*pi/180., y=camera_world_rotation_[1]*pi/180.;
            std::array<double,3> right_axis{-std::sin(y),std::cos(y),0};
            std::array<double,3> up_axis{-std::sin(p)*std::cos(y),-std::sin(p)*std::sin(y),std::cos(p)};
            auto position=camera_world_location_;
            for(int i=0;i<3;++i) position[i]-=right_axis[i]*pan_+up_axis[i]*frame_;
            Call move(camera,L"K2_SetWorldLocation",4); move.set(L"NewLocation",position); move.set(L"bSweep",false); move.set(L"bTeleport",true); move.run();
        }
    }
}
Json InventoryUI::poll(void* engine,const Catalog& catalog,const State& state,Appearance& appearance,float,bool focused) {
#ifdef CSS_INVENTORY_DEV
    cinema_update(focused);
#endif
    if(!enabled_) return {};
    auto now=GetTickCount64();
    if(main_.Get() && now>=discover_after_) {
        discover_after_=now+500;
        auto* player=appearance.player(engine); auto* current=inventory_object(player,L"Controller");
        auto* handler=inventory_object(current,L"User Interface Handler Component");
        auto* current_main=inventory_object(inventory_object(handler,L"WBP_Menu_Game"),L"WBP_Menu_Main");
        if(current!=controller_.Get() || current_main!=main_.Get()) detach();
    }
    if(!main_.Get()) {
        if(now<discover_after_) return {}; discover_after_=now+500;
        auto* player=appearance.player(engine); auto* pc=inventory_object(player,L"Controller");
        auto* handler=inventory_object(pc,L"User Interface Handler Component");
        auto* main=inventory_object(inventory_object(handler,L"WBP_Menu_Game"),L"WBP_Menu_Main");
        if(!main || !inventory_bool(main,L"bOpen")) return {};
        command(engine,{{"action","inventory_attach"}}); command(engine,{{"action","inventory_order"}});
    }
    auto* main=main_.Get(); auto* switcher=switcher_.Get();
    if(!main || !switcher || !page_.Get()) { detach(); return {}; }
    Call selected(switcher,L"GetActiveWidget",1); selected.run();
    const bool extension_before=extension_active_;
    extension_active_=inventory_bool(main,L"bOpen") && selected.get<UObject*>()==extension_page_.Get();
    active_=inventory_bool(main,L"bOpen") && (selected.get<UObject*>()==page_.Get() || extension_active_);
    if(extension_before!=extension_active_) {dirty_=enter_transition_=true;hits_.clear();rows_.clear();sliders_.clear();scroll_.Reset();name_input_.Reset();}
    if(active_ && !was_active_) { appearance.player(engine); bind_inputs(); camera_start(); dirty_=enter_transition_=true; closing_=false; for(auto& b:bindings_) { b.down=true; b.repeat=now+400; } }
    if(!active_ && was_active_) { camera_stop(); closing_=false; transition_started_=0; }
    was_active_=active_;
    double elapsed=last_tick_?std::clamp((now-last_tick_)/1000.,0.,.05):0.; last_tick_=now;
#ifdef CSS_INVENTORY_DEV
    if(!active_ || !focused) capture_duration_=0;
#endif
    if(!active_) return {};
    if(now>=layout_check_) {
        layout_check_=now+500;
        Call geometry(switcher,L"GetCachedGeometry",1); geometry.run();
        Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2); size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        auto current=size.get<Vec2>();
        if(std::abs(current.x-layout_size_[0])>.5 || std::abs(current.y-layout_size_[1])>.5) dirty_=true;
    }
    if(auto* prompt=input_prompt_.Get()) {
        const bool gamepad=read<uint8_t>(prompt,L"InputType")==1;
        if(gamepad!=gamepad_) { gamepad_=gamepad; dirty_=true; }
    }
    if(extension_active_ && extensions_ && now>=extension_check_) {
        extension_check_=now+250;
        auto library=extensions_->request({{"op","library"}});
        if(library.value("revision",uint64_t{})!=extension_revision_) {extension_revision_=library.value("revision",uint64_t{});dirty_=true;}
        extension_library_=std::move(library);
    }
    if(dirty_) {if(extension_active_) build_extensions();else build(catalog,state,appearance);}
    animate(GetTickCount64());
    if(!active_ || closing_) return {};
    if(!focused) { motion_.reset(); drag_pan_=drag_rotate_=false; return {}; }
    bool typing=false;
    if(auto* input=name_input_.Get()) { Call focus(input,L"HasKeyboardFocus",1); focus.run(); typing=focus.get<bool>(); }
    if(!typing) camera_update(elapsed,state.invert_orbit_x);
    else { motion_.reset(); drag_pan_=drag_rotate_=false; }
#ifdef CSS_INVENTORY_DEV
    if(capture_duration_) {
        double t=std::clamp(double(now-capture_start_)/capture_duration_,0.,1.);
        const double smooth=t*t*(3-2*t);
        auto target=capture_from_;
        for(int i=0;i<4;++i) target[i]+=(capture_to_[i]-target[i])*smooth;
        camera_move({target[0]-yaw_,target[1]-zoom_,target[2]-pan_,target[3]-frame_});
        if(t>=1.) capture_duration_=0;
    }
#endif
    if(!typing) for(auto& binding:bindings_) {
        bool down=false; for(const auto& key:binding.keys) if(inventory_key(controller_.Get(),key)) { down=true; break; }
        bool repeat=binding.action=="up" || binding.action=="down" || binding.action=="left" || binding.action=="right";
        bool triggered=down && (!binding.down || (repeat && now>=binding.repeat));
        if(down && !binding.down) binding.repeat=now+360; else if(triggered) binding.repeat=now+110;
        binding.down=down;
        if(!triggered) continue;
        if(extension_active_ && extension_input(binding.action)) return {};
        if(binding.action=="close") return dispatch({{"action","ui_close"}},state);
        if(binding.action=="reset_view") return dispatch({{"action","ui_reset_view"}},state);
        if(binding.action=="previous_section" || binding.action=="next_section") return dispatch({{"action","ui_section"},{"section",(section_+(binding.action=="next_section"?1:2))%3}},state);
        if(rows_.empty()) continue;
        if(binding.action=="up" || binding.action=="down") {
            row_=std::clamp(row_+(binding.action=="up"?-1:1),0,int(rows_.size())-1);
            if(auto* scroll=scroll_.Get()) { Call reveal(scroll,L"ScrollWidgetIntoView",4); reveal.set(L"WidgetToFind",rows_[row_].widget.Get()); reveal.set(L"AnimateScroll",false); reveal.set(L"ScrollDestination",uint8_t{0}); reveal.set(L"Padding",8.f); reveal.run(); }
            dirty_=true; return {};
        }
        const auto& row=rows_[row_];
        auto action=binding.action=="left"?row.previous:binding.action=="right"?row.next:binding.action=="accept"?row.accept:binding.action=="secondary"?row.secondary:row.tertiary;
        return dispatch(action,state);
    }
    for(auto& slider:sliders_) if(auto* widget=slider.widget.Get()) {
        Call value(widget,L"GetValue",1); value.run(); auto v=value.get<float>();
        if(std::abs(v-slider.previous)>.00001f) {
            slider.previous=v; text_value(slider.label.Get(),slider_text(v,slider.scalar));
            color_channel_=slider.action.at("channel").get<int>();
            // Preserve mouse capture while the slider is dragged. Update the
            // existing heading and controller actions without rebuilding widgets.
            for(const auto& channel:sliders_) if(auto* heading=channel.heading.Get())
                invoke(heading,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{channel.action.at("channel").get<int>()==color_channel_?gold:ivory});
            if(row_>=0 && row_<int(rows_.size())) for(auto* action:{&rows_[row_].previous,&rows_[row_].next})
                if(action->is_object() && action->value("action",std::string{})=="color") (*action)["channel"]=color_channel_;
            auto action=slider.action; action["value"]=v; return action;
        }
    }
    for(auto& hit:hits_) if(auto* widget=hit.widget.Get()) {
        Call pressed(widget,L"IsPressed",1); pressed.run(); bool down=pressed.get<bool>();
        bool click=down && !hit.down; hit.down=down;
        if(click) return dispatch(hit.action,state);
    }
    return {};
}
Json InventoryUI::diagnostics() const {
    Json value={{"cssx_active",extension_active_},{"extension",extension_id_},{"extension_page",extension_paging_.page},{"attached",tab_.Get()!=nullptr},{"active",active_},{"section",section_},{"row",row_},{"rows",rows_.size()},{"camera",display_.Get()!=nullptr},{"layout_size",layout_size_},{"yaw",yaw_},{"pan",pan_},{"zoom",zoom_},{"frame",frame_},{"gamepad",gamepad_}};
    for(const auto& b:bindings_) value["bindings"][b.action]=b.keys;
    return value;
}
}
namespace css {
void InventoryUI::message(const std::string& value) {
    if(value==last_message_) return;
    const bool template_status=value.starts_with("Template ") && section_!=2;
    const bool routine=template_status || value.starts_with("Wearing ") || value=="Colors updated." || value=="Original appearance restored." || value.starts_with("Your saved appearance") || value.starts_with("Choose an appearance") || value.starts_with("No CSS outfit packages found.");
    if(auto* widget=status_.Get()) { text_value(widget,routine?"":value); last_message_=value; }
}
}

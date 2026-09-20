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
    void selection_mark(double x,double y,bool selected) {
        auto* frame=box(x,y,12,12,Color{.31f,.275f,.225f,1});invoke(frame,L"SetRenderTransformAngle",L"Angle",45.f);
        auto* inner=box(x+2,y+2,8,8,Color{.01f,.008f,.006f,1});invoke(inner,L"SetRenderTransformAngle",L"Angle",45.f);
        if(selected) {auto* dot=box(x+4,y+4,4,4,Color{.42f,.34f,.22f,1});invoke(dot,L"SetRenderTransformAngle",L"Angle",45.f);}
    }
};
// 0.4: a short, deterministic strip of colours for one part, so a controller can pick
// one without anybody having to think in RGB. What the author chose comes first, then
// what each palette gives this part, then a hue ring and a brightness ramp off the
// author's own colour. Exact RGB is still there behind a toggle for when you want it.
std::vector<ControlValue> color_swatches(const ControlSet& options,const Control& control) {
    std::vector<ControlValue> out;
    auto add=[&](ControlValue value) {
        for(size_t i=0;i<3;++i) value[i]=std::clamp(value[i],control.minimum,control.maximum);
        for(const auto& had:out)
            if(std::abs(had[0]-value[0])+std::abs(had[1]-value[1])+std::abs(had[2]-value[2])<.03f) return;
        // Four rows of six. The panel has the room for it now that it starts under the
        // top bar, and a wider strip is the whole point of picking rather than mixing.
        if(out.size()<24) out.push_back(value);
    };
    add(control.value);
    for(const auto& palette:options.palettes) {
        auto found=palette.values.find(control.id);
        if(found!=palette.values.end()) add(found->second);
    }
    // A hue-locked part is a material rather than a colour, so its strip stays on its
    // own hue and offers depth instead: rotating it is the thing that looks broken.
    if(control.hue_locked) {
        for(float scale:{.5f,.65f,.8f,.9f,1.1f,1.25f,1.45f,1.7f}) add(apply_tint({0,1,scale},control.value,false));
        for(float saturation:{.35f,.6f,.8f,1.25f,1.6f}) add(apply_tint({0,saturation,1},control.value,true));
    } else {
        for(int step=1;step<12;++step) add(apply_tint({step*30.f,1,1},control.value,false));
        for(float scale:{.55f,.75f,.9f,1.15f,1.4f,1.7f}) add(apply_tint({0,1,scale},control.value,false));
        for(float saturation:{.4f,.7f,1.35f}) add(apply_tint({0,saturation,1},control.value,false));
    }
    return out;
}
size_t nearest_swatch(const std::vector<ControlValue>& swatches,const ControlValue& value) {
    size_t best=0; float closest=1e9f;
    for(size_t i=0;i<swatches.size();++i) {
        float distance=0;
        for(size_t c=0;c<3;++c) distance+=(swatches[i][c]-value[c])*(swatches[i][c]-value[c]);
        if(distance<closest) { closest=distance; best=i; }
    }
    return best;
}
bool inventory_key(UObject* pc,const std::string& key) {
    Call call(pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
    member(call.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(wide(key).c_str()));
    call.run(); return call.get<bool>();
}
std::string inventory_text(UObject* widget,int limit=256) {
    if(!widget) return {};
    Call text(widget,L"GetText",1); text.run();
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
    convert.copy(L"InText",text,L"ReturnValue"); convert.run();
    const auto& value=*static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars=value.GetCharArray();
    if(chars.Num()>limit) throw std::runtime_error("Text is too long");
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
    // The right panel used to start level with the list, leaving a band across the top
    // of it with nothing in it. It starts under the top bar instead, and the controls
    // under the description start 90 higher, which is the room a long list of options
    // needs before it runs into the buttons at the bottom.
    constexpr double panel_top=96, detail_y=121, controls_y=420, panel_bottom=932;
    decoration("T_UI_DescriptionHeader_Divider",right,detail_y+47,360,2);
    ui.box(right-12,panel_top,384,panel_bottom-panel_top,Color{.006f,.005f,.004f,.38f});
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
    const char* sections[]={"SHELL","CUSTOMIZE","LOCOMOTION","PROFILE"};
    constexpr int section_count=4;
    // The labels live in a clipped strip between the LT/RT prompts, like the game's
    // inventory tabs: a fixed gap between words, the selected label always whole, and
    // its neighbours cut at the strip edges until you move to them.
    const double strip_left=left+28, strip_right=left+panel-28, available=strip_right-strip_left;
    auto* strip=construct(L"/Script/UMG.CanvasPanel",tree);
    ui.place(strip,strip_left,section_y,available,40);
    invoke(strip,L"SetClipping",L"InClipping",uint8_t{1});
    ui.canvas=strip; ui.origin_x=strip_left; ui.origin_y=section_y;
    std::array<UObject*,section_count> section_buttons{};
    std::array<double,section_count> section_widths{}, section_x{};
    double text_width=0;
    for(int i=0;i<section_count;++i) {
        auto* button=ui.button(sections[i],strip_left,section_y,available,40,section_==i,true,17);
        section_buttons[i]=button;
        invoke(button,L"ForceLayoutPrepass");
        Call child(button,L"GetContent",1); child.run();
        Call size(child.get<UObject*>(),L"GetDesiredSize",1); size.run();
        section_widths[i]=size.get<Vec2>().x/ui.scale;
        text_width+=section_widths[i];
        bind(button,{{"action","ui_section"},{"section",i}});
    }
    // All four labels want to be on screen at once: a player who cannot see PROFILE does
    // not know it is there. The strip was already clipping it at 0.4.1's sizes, and
    // CUSTOMIZE is four letters longer than the COLOR it replaced, so rather than slide a
    // tab out of view the labels shrink until the set fits. A label's width scales with
    // its font size, so the one measurement above is enough to pick the size.
    constexpr double min_gap=34, tight_gap=13, tab_font=17, smallest_font=12;
    double wanted=text_width+(section_count+1)*tight_gap;
    if(wanted>available) {
        const double font=std::max(smallest_font,
            std::floor(tab_font*(available-(section_count+1)*tight_gap)/text_width));
        text_width=0;
        for(int i=0;i<section_count;++i) {
            Call child(section_buttons[i],L"GetContent",1); child.run();
            auto* label=child.get<UObject*>();
            font_size(label,float(font*ui.scale));
            invoke(section_buttons[i],L"ForceLayoutPrepass");
            Call size(label,L"GetDesiredSize",1); size.run();
            section_widths[i]=size.get<Vec2>().x/ui.scale;
            text_width+=section_widths[i];
        }
        wanted=text_width+(section_count+1)*tight_gap;
    }
    // Below the smallest readable size there is nothing left to give, and the old
    // behaviour takes over: centre the selected label and let its neighbours run off.
    const bool sliding=wanted>available;
    const double section_gap=sliding?min_gap:(available-text_width)/(section_count+1);
    double cursor=strip_left+(sliding?0:section_gap);
    for(int i=0;i<section_count;++i) { section_x[i]=cursor; cursor+=section_widths[i]+section_gap; }
    if(sliding) {
        // Centre the selected label, without opening a hole at either end of the strip.
        double shift=(strip_left+strip_right)/2-(section_x[section_]+section_widths[section_]/2);
        shift=std::min(shift,strip_left-section_x[0]);
        shift=std::max(shift,strip_right-(section_x[section_count-1]+section_widths[section_count-1]));
        for(auto& x:section_x) x+=shift;
    }
    for(int i=0;i<section_count;++i) {
        auto* slot=inventory_object(section_buttons[i],L"Slot");
        invoke(slot,L"SetPosition",L"InPosition",Vec2{(section_x[i]-strip_left-section_gap/4)*ui.scale,0.});
        invoke(slot,L"SetSize",L"InSize",Vec2{(section_widths[i]+section_gap/2)*ui.scale,40*ui.scale});
    }
    ui.canvas=canvas; ui.origin_x=0; ui.origin_y=0;
    {
        // Preserve the native highlight's height so its soft line survives scaling.
        const double highlight_width=section_widths[section_]*1.5;
        auto* highlight=decoration("T_UI_TopBarHighlightLine",section_x[section_]-(highlight_width-section_widths[section_])/2,
                                   section_y+32,highlight_width,10);
        invoke(highlight,L"SetColorAndOpacity",L"InColorAndOpacity",Color{1,1,1,.6f});
    }
    input_prompt_=prompt("previous_section","",left,section_y+7,28,8); prompt("next_section","",left+panel-28,section_y+7,28,9);
    auto selection=state.selections.find(appearance.shell);
    const Outfit* worn=nullptr;
    if(selection!=state.selections.end()) for(const auto& outfit:catalog.outfits) if(outfit.id==selection->second.outfit) worn=&outfit;
    // A list says where it starts and what goes in it. scroll_end measures how far the
    // rows actually reached and sizes the box to that, so a page with rows of several
    // heights does not also have to keep a running total of them.
    UObject* list_box=nullptr; double list_top=0, list_y=0;
    auto scroll_begin=[&](double y=328) {
        list_top=list_y=y;
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
        invoke(scroll,L"SetScrollbarThickness",L"NewScrollbarThickness",Vec2{4*ui.scale,4*ui.scale});
        ui.place(scroll,left,y,panel,900-y);
        auto* size=construct(L"/Script/UMG.SizeBox",tree); list_box=size;
        auto* list=construct(L"/Script/UMG.CanvasPanel",tree); content(size,list);
        Call add(scroll,L"AddChild",2); add.set(L"content",size); add.run();
        ui.canvas=list; ui.origin_x=left; ui.origin_y=y;
        invoke(scroll,L"SetScrollOffset",L"NewScrollOffset",scroll_offset_);
    };
    auto scroll_end=[&] {
        const double height=std::max(1.,ui.extent_of(ui.canvas));
        invoke(list_box,L"SetHeightOverride",L"InHeightOverride",float(height*ui.scale));
        if(auto* scroll=scroll_.Get()) invoke(scroll,L"SetAlwaysShowScrollbar",L"NewAlwaysShowScrollbar",height>900-list_top);
        ui.canvas=canvas; ui.origin_x=0; ui.origin_y=0;
    };
    // 0.4: a colour row carries a chip of the colour it paints. A list of colours that
    // never shows one is the single worst thing about the old tab.
    const Color* row_swatch=nullptr;
    double row_indent=0;
    bool row_thumb=false; // Reserve space for outfit thumbnails.
    // A section header groups the rows under it, the way CUSTOMIZE separates OUTFIT from
    // BODY. It is not selectable and takes no row index.
    auto section=[&](const std::string& title) { ui.label(title,left+18,list_y,panel-36,22,14,gold); list_y+=26; };
    auto gap=[&](double height) { list_y+=height; };
    auto row=[&](int index,const std::string& title,const std::string& subtitle,double h,Json accept,Json previous=Json{},Json next=Json{},Json secondary=Json{},Json tertiary=Json{}) {
        const double y=list_y; list_y+=h;
        bool selected=index==row_;
        auto* marker=ui.box(left,y,panel-10,h-5,selected?Color{.035f,.030f,.019f,.30f}:panel_color);
        auto* button=ui.button("",left,y,panel-10,h-5,selected);
        bind(button,{{"action","ui_row"},{"row",index},{"apply",false}});
        double inset=row_thumb?88:18;
        inset+=row_indent;
        if(row_swatch) {
            ui.box(left+inset-2,y+h/2-17,34,34,Color{.05f,.045f,.03f,1});
            ui.box(left+inset,y+h/2-15,30,30,*row_swatch);
            inset+=44;
        }
        auto single_line=[&](UObject* text) {
            invoke(text,L"SetAutoWrapText",L"InAutoTextWrap",false);
            invoke(text,L"SetClipping",L"InClipping",uint8_t{1});
            invoke(text,L"SetTextOverflowPolicy",L"InOverflowPolicy",uint8_t{1});
        };
        single_line(ui.label(title,left+inset,y+9,panel-inset-38,32,18,selected?ivory:muted));
        // Reserve a separate column for Equipped, including long variant names.
        const double status_width=row_thumb?100:20;
        if(!subtitle.empty()) single_line(ui.label(subtitle,left+inset,y+41,panel-inset-status_width,24,15,muted));
        if(selected) { decoration("T_UI_TopBarHighlightLine",left+8,y+1,panel-26,2); ui.box(left,y+8,1,h-20,gold); }
        rows_.push_back({WeakObject(marker),WeakObject(button),accept,previous,next,secondary,tertiary});
        row_swatch=nullptr; row_indent=0; row_thumb=false;
        return y;
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
        auto* header=ui.label(title,right,detail_y,360,76,22);
        invoke(header,L"SetJustification",L"InJustification",uint8_t{1});
        auto* sub=ui.label(subtitle,right,detail_y+89,360,34,16,gold);
        invoke(sub,L"SetJustification",L"InJustification",uint8_t{1});
        ui.label(body,right+16,detail_y+145,328,126,16,muted);
    };
    if(section_==0) {
        // Pin the equipped outfit to the top only when entering the tab or returning to it,
        // NEVER dynamically while the player is actively browsing/selecting inside the SHELL
        // view (so selecting an outfit or cycling variants never jumps the list out from under the cursor).
        const Outfit* pinned = nullptr;
        if(!pinned_outfit_id_.empty()) {
            for(const auto& o:catalog.outfits) if(o.id==pinned_outfit_id_) { pinned=&o; break; }
        } else {
            pinned = worn;
        }
        if(pinned && !catalog.compatible(pinned->id, appearance.shell)) pinned = nullptr;

        std::vector<const Outfit*> ordered;
        if(pinned) ordered.push_back(pinned);
        for(const auto& o:catalog.outfits) if(&o!=pinned) ordered.push_back(&o);
        const int total=int(ordered.size())+2;
        row_=std::clamp(row_,0,total-1);
        scroll_begin();
        const Json harbinger{{"action","harbinger_mirror"},{"value",!state.harbinger_mirror}};
        row(0,"Harbinger outfit",state.harbinger_mirror?"Carry from shell":"Keeps its own",85,harbinger,harbinger,harbinger);
        row(1,"Original appearance","Restore your current shell",85,{{"action","restore"}});
        for(size_t p=0;p<ordered.size();++p) {
            const auto& outfit=*ordered[p]; size_t v=0;
            bool chosen=worn==&outfit;
            if(chosen) for(size_t j=0;j<outfit.variants.size();++j) if(outfit.variants[j].id==selection->second.variant) v=j;
            bool compatible=catalog.compatible(outfit.id,appearance.shell);
            auto wear=[&](size_t index) { return compatible?Json{{"action","select"},{"outfit",outfit.id},{"variant",outfit.variants[index].id}}:Json{}; };
            row_thumb=true;
            const double y=row(int(p)+2,outfit.name,outfit.variants[v].name,85,wear(v),wear((v+outfit.variants.size()-1)%outfit.variants.size()),wear((v+1)%outfit.variants.size()),{},{{"action","favorite"},{"outfit",outfit.id}});
            thumbnail(outfit,left+14,y+9,62);
            if(state.favorites.contains(outfit.id)) ui.star(left+panel-29,y+24,7,gold);
            if(chosen) ui.label("Equipped",left+panel-99,y+44,82,24,14,gold);
        }
        scroll_end();
        if(catalog.outfits.empty()) ui.label(catalog.empty_message(),left+18,435,panel-36,130,18,muted);
        if(row_==0) {
            detail("Harbinger outfit",state.harbinger_mirror?"Carry from shell":"Keeps its own",
                   state.harbinger_mirror
                     ?"When you sever into the Harbinger, it wears your current shell's outfit, so dying mid-fight keeps your look. Cosmetic only."
                     :"The Harbinger keeps its own saved outfit. Turn this on to carry your shell's outfit over automatically.");
            action_button("accept",state.harbinger_mirror?"Give Harbinger its own":"Carry outfit into Harbinger",655,rows_[0].accept,3);
        } else if(row_==1) {
            detail("Original appearance","Your current shell","Restore the appearance supplied by the game and any installed base replacements. Your shell's abilities stay the same.");
            action_button("accept","Restore original",controls_y+20,rows_[1].accept,3);
            action_button("secondary","Search catalog...",771,{{"action","ui_browse_shells"}},4);
        } else {
            const auto& outfit=*ordered[row_-2];
            detail(outfit.name,"By "+outfit.author,outfit.description.empty()?"Choose an outfit variant. Appearance changes keep your current shell's abilities.":outfit.description);
            auto& selected=rows_[row_];
            std::string variant=outfit.variants.front().name;
            if(worn==&outfit) for(const auto& v:outfit.variants) if(v.id==selection->second.variant) variant=v.name;
            ui.label("Variant",right,controls_y-22,360,28,16,muted);
            auto* name=ui.label(variant,right+44,controls_y+20,272,55,21,ivory);
            invoke(name,L"SetJustification",L"InJustification",uint8_t{1});
            if(outfit.variants.size()>1) {
                bind(ui.button("<",right,controls_y+10,44,44),selected.previous);
                bind(ui.button(">",right+316,controls_y+10,44,44),selected.next);
                direction_hint(true,"Change variant",right+72,controls_y+85,280);
            }
            action_button("accept","Wear",655,selected.accept,3,!selected.accept.is_null());
            action_button("tertiary",state.favorites.contains(outfit.id)?"Remove favorite":"Add favorite",713,selected.tertiary,2);
            action_button("secondary","Search catalog...",771,{{"action","ui_browse_shells"}},4);
        }
    } else if(section_==1) {
        if(!worn || worn->controls_for(selection->second.variant).controls.empty()) detail("Customize","Nothing to adjust","Wear an outfit whose author left something adjustable, and it shows up here.");
        else {
            const auto& options=worn->controls_for(selection->second.variant); const auto& custom=selection->second.custom;
            auto values=control_values(options,custom);
            auto palette_action=[&](size_t i) { return Json{{"action","palette"},{"palette",i?options.palettes[i-1].id:"original"}}; };

            struct TemplateItem {
                std::string id, name, subtitle, kind_name, description;
                Json action;
                bool is_palette;
            };
            std::vector<TemplateItem> tmpl_items;
            tmpl_items.push_back({
                "original", "Original", "The author's materials", "Original",
                "Original restores the author's own materials exactly, and cannot be tinted.",
                palette_action(0), true
            });
            for(size_t i=0;i<options.palettes.size();++i) {
                const auto& p=options.palettes[i];
                tmpl_items.push_back({
                    p.id, p.name, "Color Palette", "Palette",
                    "Author-curated color palette: "+p.name,
                    palette_action(i+1), true
                });
            }
            for(const auto& t:worn->templates) {
                std::string kind_str="Combination";
                if(t.kind==TemplateKind::Palette) kind_str="Palette";
                else if(t.kind==TemplateKind::Archetype) kind_str="Archetype";
                else if(t.kind==TemplateKind::Physics) kind_str="Physics";
                else if(t.kind==TemplateKind::Hair) kind_str="Hair";
                else if(t.kind==TemplateKind::Jewelry) kind_str="Jewelry";
                else if(t.kind==TemplateKind::Glow) kind_str="Glow";
                else if(t.kind==TemplateKind::Accessory) kind_str="Accessory";
                else if(t.kind==TemplateKind::Fabric) kind_str="Fabric";
                else if(t.kind==TemplateKind::Anatomy) kind_str="Anatomy";
                tmpl_items.push_back({
                    t.id, t.name, kind_str+" Preset", kind_str,
                    t.data.value("description",std::string("Author preset for outfit combination, body archetype, or physics.")),
                    Json{{"action","template"},{"template",t.id}}, false
                });
            }
            size_t active_tmpl=0;
            for(size_t i=0;i<tmpl_items.size();++i) {
                if(tmpl_items[i].is_palette && tmpl_items[i].id==custom.palette) {
                    active_tmpl=i; break;
                }
            }
            const auto& cur_tmpl=tmpl_items[active_tmpl];
            const size_t total_templates=tmpl_items.size();
            auto tmpl_step=[&](int dir) {
                size_t next=(active_tmpl+total_templates+dir)%total_templates;
                return tmpl_items[next].action;
            };

            const bool has_dyed_palette = custom.palette != "original";
            // 0.4: the tab is a template/palette, then one section per group, each opening with the
            // tint that moves everything under it. See docs/control-convention.md.
            struct Entry { bool tint; ControlGroup group; int control; };
            std::vector<Entry> entries;
            entries.push_back({false,ControlGroup::Outfit,-1});     // the template/palette itself
            for(auto group:{ControlGroup::Outfit,ControlGroup::Body}) {
                std::vector<int> members;
                for(size_t i=0;i<options.controls.size();++i) if(options.controls[i].group==group) members.push_back(int(i));
                if(members.empty()) continue;
                // Tinting needs a palette: Original is not dyed, so there is nothing to move.
                const bool tintable=has_dyed_palette && std::any_of(members.begin(),members.end(),
                    [&](int i){return !options.controls[i].scalar;});
                if(tintable) entries.push_back({true,group,-1});
                for(int i:members) entries.push_back({false,group,i});
            }
            row_=std::clamp(row_,0,int(entries.size())-1);

            auto swatch_of=[&](const Control& c) {
                auto v=values.contains(c.id)?values.at(c.id):c.value;
                if(c.kind==ControlKind::Glow) {
                    const float t=std::clamp(v[0]/std::max(c.maximum,.001f),0.f,1.f);
                    return Color{1.f*t+.15f,0.85f*t+.08f,0.3f*t+.03f,1};
                }
                if(c.kind==ControlKind::Opacity) {
                    const float a=std::clamp(v[0],0.f,1.f);
                    return Color{0.65f*a+.2f,0.65f*a+.2f,0.7f*a+.2f,1};
                }
                if(c.scalar) { const float t=std::clamp(v[0]/std::max(c.maximum,.001f),0.f,1.f); return Color{gold.r*t+.02f,gold.g*t+.02f,gold.b*t+.02f,1}; }
                return Color{srgb_linear(v[0]),srgb_linear(v[1]),srgb_linear(v[2]),1};
            };
            auto tint_of=[&](ControlGroup group) {
                auto found=custom.tints.find(control_group_name(group));
                return found==custom.tints.end()?ColorTint{}:found->second;
            };

            const double header=62, line=68;
            scroll_begin(328);
            row_swatch=nullptr;
            row(0,cur_tmpl.name,cur_tmpl.subtitle,header,
                cur_tmpl.action,tmpl_step(-1),tmpl_step(1));
            gap(10);
            for(size_t i=1;i<entries.size();++i) {
                const auto& entry=entries[i];
                if(entry.tint) {
                    section(entry.group==ControlGroup::Body?"BODY":"OUTFIT");
                    const auto tint=tint_of(entry.group);
                    const std::string state=tint.neutral()?"Shift this whole group":
                        "hue "+std::to_string(int(tint.hue))+", sat "+std::to_string(int(tint.saturation*100))+"%, bright "+std::to_string(int(tint.brightness*100))+"%";
                    Json reset={{"action","reset_tint"},{"group",control_group_name(entry.group)}};
                    Json minus={{"action","tint"},{"group",control_group_name(entry.group)},{"field",tint_field_index_==0?"hue":tint_field_index_==1?"saturation":"brightness"},{"delta",-1}},plus=minus; plus["delta"]=1;
                    row(int(i),"Tint",state,line,reset,minus,plus,{{"action","ui_tint_field"}});
                    continue;
                }
                const auto& c=options.controls[entry.control];
                const auto chip=swatch_of(c);
                const auto held=values.contains(c.id)?values.at(c.id):c.value;
                // A switch and a list of textures have no colour to show, so they carry
                // their state in the subtitle instead of a chip.
                if(c.kind==ControlKind::Color || c.kind==ControlKind::Intensity || c.kind==ControlKind::Scalar || c.kind==ControlKind::Glow || c.kind==ControlKind::Opacity) row_swatch=&chip;
                row_indent=12;
                size_t pal_idx=0;
                for(size_t p=0;p<options.palettes.size();++p) if(options.palettes[p].id==custom.palette) pal_idx=p+1;
                const std::string pal_name=pal_idx?options.palettes[pal_idx-1].name:"Original";
                const bool from_palette=pal_idx && options.palettes[pal_idx-1].values.contains(c.id);
                std::string source=custom.values.contains(c.id)?"Custom":from_palette?pal_name:"Original";
                Json accept={{"action","reset_control"},{"control",c.id}};
                if(c.kind==ControlKind::Toggle) {
                    const bool on=held[0]>=.5f;
                    source=on?"Shown":"Hidden";
                    // Enter flips a switch. Resetting one is what Reset all is for.
                    accept={{"action","control"},{"control",c.id},{"channel",0},{"value",on?0:1}};
                } else if(c.kind==ControlKind::Choice) {
                    const int here=std::clamp(int(std::lround(held[0])),0,int(c.options.size())-1);
                    source=c.options[here].name;
                } else if(c.kind==ControlKind::Spring) {
                    source="Bounce "+slider_text(held[0],true)+" Hz, settle "+std::to_string(int(std::lround(held[1]*100)))+"%";
                    if(c.spring_clamp) source+=", travel "+slider_text(held[2],true)+" cm";
                } else if(c.kind==ControlKind::Dynamics || c.kind==ControlKind::Rig) {
                    source=body_rig_control(c) ? "Bounce "+slider_text(held[0],true)+" Hz, damping "+slider_text(held[1],true)+", motion "+slider_text(held[2],true)
                        : "Stiffness "+slider_text(held[0],true)+", damping "+slider_text(held[1],true)+", gravity "+slider_text(held[2],true);
                    if(c.kind==ControlKind::Rig && held[3]==0) source="Motion off";
                } else if(c.kind==ControlKind::Glow) {
                    source="Glow "+slider_text(held[0],true)+" cd/m²";
                    if(c.pulse_hz>0) source+=", pulse "+slider_text(c.pulse_hz,true)+" Hz";
                } else if(c.kind==ControlKind::Opacity) {
                    source="Opacity "+std::to_string(int(std::lround(held[0]*100)))+"%";
                } else if(c.kind==ControlKind::Shape) {
                    source="Weight "+slider_text(held[0],true);
                }
                const int channel=channel_%control_channel_count(c);
                Json minus={{"action","control"},{"control",c.id},{"channel",channel},{"delta",-1}},plus=minus; plus["delta"]=1;
                if(!c.scalar && !exact_color_) {
                    // Left and Right walk the strip instead of nudging one channel, which
                    // is the whole point of having one.
                    const auto strip=color_swatches(options,c);
                    const auto here=nearest_swatch(strip,values.contains(c.id)?values.at(c.id):c.value);
                    auto pick=[&](size_t index) {
                        const auto& v=strip[index];
                        return Json{{"action","control"},{"control",c.id},{"rgb",{v[0],v[1],v[2]}}};
                    };
                    minus=pick((here+strip.size()-1)%strip.size());
                    plus=pick((here+1)%strip.size());
                }
                row(int(i),c.name,source,line,accept,minus,plus,
                    {{"action","ui_channel"},{"count",control_channel_count(c)}},
                    {{"action","palette"},{"palette","original"}});
            }
            scroll_end();

            const auto& entry=entries[row_];
            const auto confirm_reset_all=Json{{"action","ui_confirm"},{"title","Reset All Customizations"},{"message","Reset all customization overrides for this outfit back to original defaults?"},{"target",rows_[row_].tertiary}};
            if(row_==0) {
                detail("Templates & Presets",worn->name,"Use Left / Right to cycle templates. Choose an author combination, material palette, body archetype, or physics preset.");
                const size_t show_count=std::min(tmpl_items.size(),size_t(6));
                for(size_t i=0;i<show_count;++i) {
                    const auto& item=tmpl_items[i];
                    bind(ui.button(item.name,right,controls_y+i*46,360,42,active_tmpl==i,true,20),item.action);
                }
                const double by=controls_y+show_count*46;
                bind(ui.button("Browse templates...",right,by+6,360,40,false,true,18),
                     {{"action","ui_browse_templates"}});
                direction_hint(true,"Cycle template",right,by+52,360);
                action_button("accept","Restore original",by+92,palette_action(0),3);
            } else if(entry.tint) {
                const auto tint=tint_of(entry.group);
                detail(entry.group==ControlGroup::Body?"Body tint":"Outfit tint",worn->name,
                       "Shift every part in this group together. Metal, gems and skin keep their own hue and take only the brightness and saturation, so a recolour cannot turn gold green.");
                const char* fields[]={"Hue","Saturation","Brightness"};
                const float lows[]={-180,0,0}, highs[]={180,2,2}, steps_[]={5,.05f,.05f};
                const float current[]={tint.hue,tint.saturation,tint.brightness};
                for(int field=0;field<3;++field) {
                    double sy=controls_y+field*80;
                    auto* heading=ui.label(fields[field],right,sy,230,28,19,field==tint_field_index_?gold:ivory);
                    auto* slider=construct(L"/Script/UMG.Slider",tree);
                    invoke(slider,L"SetMinValue",L"InValue",lows[field]); invoke(slider,L"SetMaxValue",L"InValue",highs[field]);
                    invoke(slider,L"SetStepSize",L"InValue",steps_[field]); invoke(slider,L"SetValue",L"InValue",current[field]);
                    invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1}); invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                    ui.place(slider,right,sy+29,290,30);
                    auto* label=ui.label(field?slider_text(current[field],true):std::to_string(int(current[field])),right+305,sy+29,55,30,18);
                    sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),
                        {{"action","tint"},{"group",control_group_name(entry.group)},{"field",field==0?"hue":field==1?"saturation":"brightness"},{"refresh",false}},current[field],true,""});
                }
                direction_hint(true,"Adjust selected slider",right,controls_y+246,360);
                action_button("secondary","Select next slider",795,rows_[row_].secondary,4);
                action_button("accept","Reset tint",841,rows_[row_].accept,3);
            } else {
                const auto& control=options.controls[entry.control]; auto value=values.contains(control.id)?values.at(control.id):control.value;
                auto set_to=[&](double v) { return Json{{"action","control"},{"control",control.id},{"channel",0},{"value",v}}; };
                if(control.kind==ControlKind::Toggle) {
                    // A switch, not a slider. Two buttons say which state you are in as
                    // well as offering the other one, the way the walk setting does.
                    const bool on=value[0]>=.5f;
                    detail(control.name,worn->name,"Show or hide this part of the outfit. Your saved looks keep it.");
                    bind(ui.button("Shown",right,controls_y+50,360,42,on,true,21),set_to(1));
                    bind(ui.button("Hidden",right,controls_y+96,360,42,!on,true,21),set_to(0));
                    direction_hint(true,on?"Hide this part":"Show this part",right,controls_y+154,360);
                    // Same rhythm as the swatch page, so the reset is in one place on
                    // every part no matter what kind of control it is.
                    action_button("accept",on?"Hide":"Show",795,set_to(on?0:1),3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else if(control.kind==ControlKind::Choice) {
                    const int here=std::clamp(int(std::lround(value[0])),0,int(control.options.size())-1);
                    detail(control.name,worn->name,"Choose which of the author's textures this part wears.");
                    // Sixteen options down one column would run past the buttons at the
                    // bottom, so a long list pairs up instead of overflowing.
                    const size_t count=control.options.size();
                    const bool paired=count>8;
                    const double wide=paired?176:360, step=paired?184:0;
                    const size_t rows=paired?(count+1)/2:count;
                    for(size_t i=0;i<count;++i) {
                        const double bx=right+(paired && i>=rows?step:0), by=controls_y+double(paired?i%rows:i)*46;
                        bind(ui.button(control.options[i].name,bx,by,wide,42,int(i)==here,true,paired?18:21),set_to(double(i)));
                    }
                    double cy=controls_y+double(rows)*46+10;
                    if(count>4) {
                        bind(ui.button("Search choices...",right,cy,360,38,false,true,18),
                             {{"action","ui_browse_choice"},{"control",control.id}});
                        cy+=44;
                    }
                    direction_hint(true,"Choose",right,cy,360);
                    action_button("accept","Reset part",841,rows_[row_].accept,3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else if(control.kind==ControlKind::Dynamics || control.kind==ControlKind::Rig) {
                    const bool rig=control.kind==ControlKind::Rig;
                    const bool body=body_rig_control(control);
                    const int fieldcount=control_channel_count(control);
                    detail(control.name,worn->name,
                           body ? "Frequency sets the bounce speed. Damping controls how quickly it settles. Motion amount controls the response to movement." :
                           "Stiffness controls how strongly this part returns toward its rest direction. "
                           "Damping reduces motion. Gravity changes downward pull; negative values pull upward.");
                    const char* fields[]={body?"Frequency (Hz)":"Stiffness",body?"Damping ratio":"Damping",body?"Motion amount":"Gravity"};
                    for(int field=0;field<3;++field) {
                        const auto range=control_channel(control,field);
                        const double sy=controls_y+field*80;
                        auto* heading=ui.label(fields[field],right,sy,230,28,19,field==channel_%fieldcount?gold:ivory);
                        auto* slider=construct(L"/Script/UMG.Slider",tree);
                        invoke(slider,L"SetMinValue",L"InValue",range.minimum);
                        invoke(slider,L"SetMaxValue",L"InValue",range.maximum);
                        invoke(slider,L"SetStepSize",L"InValue",range.step);
                        invoke(slider,L"SetValue",L"InValue",value[field]);
                        invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1});
                        invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                        ui.place(slider,right,sy+29,262,30);
                        auto* label=ui.label(slider_text(value[field],true),right+270,sy+29,90,30,18);
                        sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),
                            {{"action","control"},{"control",control.id},{"channel",field},{"refresh",false}},
                            value[field],true,""});
                    }
                    if(rig) bind(ui.button(value[3]==1?"Motion: On":"Motion: Off",right,controls_y+240,360,38,
                        channel_%fieldcount==3,true,19),
                        {{"action","control"},{"control",control.id},{"channel",3},{"value",value[3]==1?0:1}});
                    direction_hint(true,"Adjust selected setting",right,controls_y+(rig?286:246),360);
                    action_button("secondary","Select next setting",795,Json{{"action","ui_channel"},{"count",fieldcount}},4);
                    action_button("accept","Reset part",841,rows_[row_].accept,3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else if(control.kind==ControlKind::Spring) {
                    // Two or three sliders, the same shape as a group tint. The numbers are
                    // frequency, damping ratio, and (with a clamp) travel; the words say what
                    // they do. Travel is how far the part may swing, which is what keeps a
                    // lively bounce on the body instead of letting it fly off.
                    const int fieldcount=control.spring_clamp?3:2;
                    const int selected=channel_%fieldcount;
                    detail(control.name,worn->name,control.spring_clamp?
                           "Bounce is how quickly this part moves, Settle how quickly it stops, "
                           "Travel how far it swings. More travel is a bigger jiggle; if it keeps "
                           "moving after you stop, turn Settle up.":
                           "Bounce is how quickly this part moves. Settle is how quickly it stops. "
                           "If it keeps going after you do, turn settle up.");
                    const char* fields[]={"Bounce","Settle","Travel"};
                    const float lows[]={control.minimum,control.damping_minimum,control.displacement_minimum};
                    const float highs[]={control.maximum,control.damping_maximum,control.displacement_maximum};
                    const float sizes[]={control.step,control.damping_step,control.displacement_step};
                    for(int field=0;field<fieldcount;++field) {
                        const double sy=controls_y+field*80;
                        auto* heading=ui.label(fields[field],right,sy,230,28,19,field==selected?gold:ivory);
                        auto* slider=construct(L"/Script/UMG.Slider",tree);
                        invoke(slider,L"SetMinValue",L"InValue",lows[field]); invoke(slider,L"SetMaxValue",L"InValue",highs[field]);
                        invoke(slider,L"SetStepSize",L"InValue",sizes[field]); invoke(slider,L"SetValue",L"InValue",value[field]);
                        invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1}); invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                        ui.place(slider,right,sy+29,262,30);
                        // "1.60 Hz" needs more room than a bare number, so the readout is
                        // wider here than on the channel sliders and the bar gives it back.
                        const std::string readout=field==1?std::to_string(int(std::lround(value[1]*100)))+"%"
                                                 :field==2?slider_text(value[2],true)+" cm"
                                                 :slider_text(value[0],true)+" Hz";
                        auto* label=ui.label(readout,right+270,sy+29,90,30,18);
                        sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),
                            {{"action","control"},{"control",control.id},{"channel",field},{"refresh",false}},
                            value[field],true,field==1?"%":field==2?" cm":" Hz"});
                    }
                    direction_hint(true,"Adjust selected slider",right,controls_y+fieldcount*80+6,360);
                    action_button("secondary","Select next slider",795,Json{{"action","ui_channel"},{"count",fieldcount}},4);
                    action_button("accept","Reset part",841,rows_[row_].accept,3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else if(control.kind==ControlKind::Glow) {
                    detail(control.name,worn->name,"Adjust emissive glow radiance. Turn up intensity for arcane luminescence; breathing pulse animates in combat.");
                    const int fieldcount=control.pulse_hz>0?2:1;
                    const int selected=channel_%fieldcount;
                    const char* fields[]={"Intensity","Pulse Rate"};
                    const float lows[]={control.minimum,0.f};
                    const float highs[]={control.maximum,5.f};
                    const float sizes[]={control.step,0.1f};
                    for(int field=0;field<fieldcount;++field) {
                        const double sy=controls_y+field*80;
                        auto* heading=ui.label(fields[field],right,sy,230,28,19,field==selected?gold:ivory);
                        auto* slider=construct(L"/Script/UMG.Slider",tree);
                        invoke(slider,L"SetMinValue",L"InValue",lows[field]); invoke(slider,L"SetMaxValue",L"InValue",highs[field]);
                        invoke(slider,L"SetStepSize",L"InValue",sizes[field]); invoke(slider,L"SetValue",L"InValue",value[field]);
                        invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1}); invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                        ui.place(slider,right,sy+29,262,30);
                        const std::string readout=field==0?slider_text(value[0],true)+" cd/m²":slider_text(value[1],true)+" Hz";
                        auto* label=ui.label(readout,right+270,sy+29,90,30,18);
                        sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),
                            {{"action","control"},{"control",control.id},{"channel",field},{"refresh",false}},
                            value[field],true,field==0?" cd/m²":" Hz"});
                    }
                    direction_hint(true,"Adjust glow intensity",right,controls_y+fieldcount*80+6,360);
                    if(fieldcount>1) action_button("secondary","Select next slider",795,Json{{"action","ui_channel"},{"count",fieldcount}},4);
                    action_button("accept","Reset part",841,rows_[row_].accept,3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else if(control.kind==ControlKind::Opacity) {
                    detail(control.name,worn->name,"Adjust fabric transparency and sheer alpha. 0% is invisible sheer; 100% is solid opaque.");
                    const double sy=controls_y;
                    auto* heading=ui.label("Opacity",right,sy,230,28,19,gold);
                    auto* slider=construct(L"/Script/UMG.Slider",tree);
                    invoke(slider,L"SetMinValue",L"InValue",control.minimum); invoke(slider,L"SetMaxValue",L"InValue",control.maximum);
                    invoke(slider,L"SetStepSize",L"InValue",control.step); invoke(slider,L"SetValue",L"InValue",value[0]);
                    invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1}); invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                    ui.place(slider,right,sy+29,270,30);
                    auto* label=ui.label(std::to_string(int(std::lround(value[0]*100)))+"%",right+280,sy+29,80,30,18);
                    sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),
                        {{"action","control"},{"control",control.id},{"channel",0},{"refresh",false}},
                        value[0],false,"%"});
                    direction_hint(true,"Adjust opacity",right,controls_y+86,360);
                    action_button("accept","Reset part",841,rows_[row_].accept,3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else if(!control.scalar && !exact_color_) {
                    // The swatch strip: the author's colour, this part in every palette,
                    // then shades of it. Picking is the common case, so it is what the
                    // pane opens on; Exact colour is one button away.
                    const auto strip=color_swatches(options,control);
                    const auto here=nearest_swatch(strip,value);
                    detail(control.name,worn->name,control.hue_locked
                        ? "Choose a shade. This part keeps its own hue on purpose: it reads as a material rather than a colour, and rotating it is what makes a recolour look wrong."
                        : "Choose a colour. The first is the author's, then this part in each palette, then hues and shades of it.");
                    // A chip is drawn, not styled. flat_button clears every brush a CSS
                    // button has, so setting a background colour on one paints nothing;
                    // the colour is a box and a transparent button sits on it to take
                    // the click, which is what the list rows do too.
                    constexpr double chip=56, pitch=60, columns=6;
                    for(size_t i=0;i<strip.size();++i) {
                        const double sx=right+double(i%size_t(columns))*pitch, sy=controls_y+double(i/size_t(columns))*pitch;
                        // Only the chosen chip gets a surround, which keeps the widget
                        // count on this page down as well as reading more clearly.
                        if(i==here) ui.box(sx-3,sy-3,chip+6,chip+6,gold);
                        ui.box(sx,sy,chip,chip,{srgb_linear(strip[i][0]),srgb_linear(strip[i][1]),srgb_linear(strip[i][2]),1});
                        bind(ui.button("",sx,sy,chip,chip),
                             {{"action","control"},{"control",control.id},{"rgb",{strip[i][0],strip[i][1],strip[i][2]}}});
                    }
                    direction_hint(true,"Choose a colour",right,
                                   controls_y+double((strip.size()+size_t(columns)-1)/size_t(columns))*pitch+8,360);
                    action_button("secondary","Exact colour",795,Json{{"action","ui_exact"}},4);
                    action_button("accept","Reset part",841,rows_[row_].accept,3);
                    action_button("tertiary","Reset all",887,confirm_reset_all,2);
                } else {
                // A shape moves geometry rather than a material value, so it says so. Every
                // other single-number control is a strength of some kind and keeps the old
                // wording, which is what the packages already installed were written for.
                const bool shape=control.kind==ControlKind::Shape;
                detail(control.name,worn->name,
                       shape?"Adjust this part of your character. Reset part restores the outfit's original shape. Save a profile to keep your changes."
                       :control.scalar?"Adjust the intensity for this part."
                       :"Adjust Red, Green and Blue. Select a channel, then adjust it with Left / Right or its slider.");
                const char* channels[]={"Red","Green","Blue"};
                for(int channel=0;channel<(control.scalar?1:3);++channel) {
                    double sy=controls_y+channel*80;
                    auto* heading=ui.label(shape?"Amount":control.scalar?"Intensity":channels[channel],right,sy,230,28,19,control.scalar || channel==channel_?gold:ivory);
                    auto* slider=construct(L"/Script/UMG.Slider",tree);
                    invoke(slider,L"SetMinValue",L"InValue",control.minimum); invoke(slider,L"SetMaxValue",L"InValue",control.maximum);
                    invoke(slider,L"SetStepSize",L"InValue",control.step); invoke(slider,L"SetValue",L"InValue",value[channel]);
                    invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.09f,.07f,1}); invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
                    ui.place(slider,right,sy+29,290,30);
                    auto* label=ui.label(slider_text(value[channel],control.scalar),right+305,sy+29,55,30,18);
                    sliders_.push_back({WeakObject(slider),WeakObject(label),WeakObject(heading),{{"action","control"},{"control",control.id},{"channel",channel},{"refresh",false}},value[channel],control.scalar,""});
                }
                direction_hint(true,shape?"Adjust shape":control.scalar?"Adjust intensity":"Adjust selected channel",right,controls_y+246,360);
                if(!control.scalar) action_button("secondary","Select next channel",795,rows_[row_].secondary,4);
                action_button("accept","Reset part",841,rows_[row_].accept,3);
                action_button("tertiary",control.scalar?"Reset all":"Back to swatches",887,
                              control.scalar?confirm_reset_all:Json{{"action","ui_exact"}},2);
                }
            }
        }
    } else if(section_==2) {
        // Per-player locomotion options that profiles save with the outfit.
        // Only walking is offered. Walking and standing borrow the Cultist Spear Lady,
        // which is the game's own blendspace, so nothing extra is installed. Jog and
        // sprint are held at normal until their stride is properly matched; the runtime
        // still carries the code, and data.cpp keeps both settings pinned to normal.
        const bool has_walk_mod=appearance.walk.walk_mod_active();
        const auto& walk_mod_name=appearance.walk.walk_mod_name();
        const bool feminine=state.walk_animation=="feminine";
        row_=0;
        const Json toggle{{"action","walk_animation"},{"value",feminine?"normal":"feminine"}};
        const std::string sub=feminine?"Feminine":(has_walk_mod?"Normal, "+walk_mod_name:"Normal");
        scroll_begin();
        row(0,"Walk animation",sub,77,toggle,toggle,toggle);
        scroll_end();
        std::string body=feminine?"Feminine borrows the Cultist Spear Lady's walk and standing pose for any shell, and walks at the pace her cycle was made for so the feet stay planted. CSS holds the animation itself, so another walk mod cannot take it back."
                                 :"Normal keeps the game's own walking, and leaves any installed walk mod free to drive it.";
        if(has_walk_mod) body+=feminine?(" "+walk_mod_name+" by argisht is installed and is being overridden."):(" "+walk_mod_name+" by argisht is installed and is handling it.");
        body+=" Jogging and sprinting stay on the game's own animation.";
        detail("Walk animation",sub,body);
        bind(ui.button("Normal",right,controls_y+50,360,42,!feminine,true,21),{{"action","walk_animation"},{"value","normal"}});
        bind(ui.button("Feminine",right,controls_y+96,360,42,feminine,true,21),{{"action","walk_animation"},{"value","feminine"}});
        direction_hint(true,"Change walk animation",right,controls_y+154,360);
        action_button("accept",feminine?"Use normal":"Use feminine",controls_y+194,toggle,3);
        std::string note;
        if(has_walk_mod) note=feminine?("Installed: "+walk_mod_name+" by argisht. CSS is holding the walk instead; choose Normal to hand it back.")
                                      :("Installed: "+walk_mod_name+" by argisht. It drives walking while Walk animation is Normal.");
        else if(feminine) note=appearance.walk.engaged()?("Active now: "+appearance.walk.reason()+". Profiles keep this setting.")
                                                        :"Armed. Move to see it. Profiles keep this setting.";
        else note="Everything normal. CSS is not touching locomotion.";
        ui.label(note,right,controls_y+250,360,60,14,muted);
    } else {
        std::vector<std::string> names; for(const auto& [name,_]:state.presets) names.push_back(name);
        row_=std::clamp(row_,0,int(names.size()));
        scroll_begin();
        row(0,"New profile","Save your current character profile",77,{{"action","ui_save_profile"}});
        for(size_t i=0;i<names.size();++i) {
            Json rep_act{{"action","ui_confirm"},{"title","Overwrite Profile"},{"message","Overwrite profile '"+names[i]+"' with your current character snapshot?"},{"target",Json{{"action","save_look"},{"name",names[i]}}}};
            Json del_act{{"action","ui_confirm"},{"title","Delete Profile"},{"message","Delete saved profile '"+names[i]+"'?\nThis action cannot be undone."},{"target",Json{{"action","delete_look"},{"name",names[i]}}}};
            row(int(i)+1,names[i],"Saved character profile",77,{{"action","load_look"},{"name",names[i]}},{},{},rep_act,del_act);
        }
        scroll_end();
        auto selected=row_?names[row_-1]:std::string{};
        detail(row_?selected:"New profile","Character profile and settings",row_?"Load this profile, replace it with your current character, or give it a new name.":"Choose a name, then save. A controller can save with the suggested name.");
        auto* input=construct(L"/Script/UMG.EditableText",tree); name_input_=input;
        std::string suggested=selected;
        if(suggested.empty()) { int n=1; do { suggested="profile."+std::to_string(n++); } while(state.presets.contains(suggested)); }
        text_value(input,suggested);
        Call current_font(input,L"GetFont",1); current_font.run();
        Call set_font(input,L"SetFont",1); set_font.copy(L"InFontInfo",current_font,L"ReturnValue");
        auto* font=set_font.param(L"InFontInfo"); auto* font_info=find(L"/Script/SlateCore.SlateFontInfo");
        member(set_font.data(font),font->GetElementSize(),font_info,L"FontObject",serif);
        member(set_font.data(font),font->GetElementSize(),font_info,L"Size",18*float(ui.scale));
        member(set_font.data(font),font->GetElementSize(),font_info,L"TypefaceFontName",FName(L"Regular")); set_font.run();
        ui.box(right,controls_y-8,360,48,Color{.04f,.035f,.025f,.7f}); ui.place(input,right+12,controls_y-3,336,40);
        ui.label("Letters, numbers, periods, underscores or hyphens",right,controls_y+54,360,55,16,muted);
        if(!row_) action_button("accept","Save profile",631,{{"action","ui_save_profile"}},3);
        else {
            action_button("accept","Load profile",631,rows_[row_].accept,3);
            auto confirm_replace = Json{{"action", "ui_confirm"}, {"title", "Overwrite Profile"}, {"message", "Overwrite profile '" + selected + "' with your current character snapshot?"}, {"target", rows_[row_].secondary}};
            auto confirm_delete = Json{{"action", "ui_confirm"}, {"title", "Delete Profile"}, {"message", "Delete saved profile '" + selected + "'?\nThis action cannot be undone."}, {"target", rows_[row_].tertiary}};
            action_button("secondary","Replace with current character",681,confirm_replace,4);
            bind(ui.button("Rename",right,controls_y+231,360,43,false,true,20),{{"action","ui_rename_profile"},{"name",selected}});
            action_button("tertiary","Delete profile",801,confirm_delete,2);
        }
    }
    decoration("T_UI_DescriptionHeader_Divider",left,931,panel,2);
    decoration("T_UI_DescriptionHeader_Divider",right,931,360,2);
    status_=ui.label("",right,953,360,60,16,muted);
    direction_hint(false,section_==0?"Browse shells":section_==1?"Browse parts":section_==2?"Browse animation options":"Browse profiles",left,947,panel);
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
    auto modal_box = [&](const std::string& title, double w, double h) {
        const double x = (width - w) / 2, y = (1080 - h) / 2;
        ui.box(0, 0, width, 1080, Color{0, 0, 0, .92f});
        ui.box(x - 1, y - 1, w + 2, h + 2, Color{.13f, .10f, .06f, 1});
        ui.box(x, y, w, h, Color{.012f, .010f, .007f, 1});
        decoration("T_UI_Nav_TitleBG", x + 1, y + 1, w - 2, 102);
        auto* tlabel = ui.label(title, x + 32, y + 27, w - 64, 70, 26, ivory);
        invoke(tlabel, L"SetJustification", L"InJustification", uint8_t{1});
        decoration("T_UI_DescriptionHeader_Divider", x + 32, y + 112, w - 64, 2);
        decoration("T_UI_DescriptionHeader_Divider", x + 32, y + h - 82, w - 64, 2);
        return std::array<double, 4>{x, y, w, h};
    };
    auto make_text_input = [&](const std::string& value, double x, double y, double w, bool enabled) {
        auto* input = construct(L"/Script/UMG.EditableText", tree);
        text_value(input, value);
        Call current(input, L"GetFont", 1); current.run();
        Call set(input, L"SetFont", 1); set.copy(L"InFontInfo", current, L"ReturnValue");
        auto* font = set.param(L"InFontInfo"); auto* info = find(L"/Script/SlateCore.SlateFontInfo");
        member(set.data(font), font->GetElementSize(), info, L"FontObject", serif);
        member(set.data(font), font->GetElementSize(), info, L"Size", 20 * float(ui.scale));
        member(set.data(font), font->GetElementSize(), info, L"TypefaceFontName", FName(L"Regular")); set.run();
        invoke(input, L"SetIsEnabled", L"bInIsEnabled", enabled); ui.place(input, x, y, w, 40);
        return input;
    };
    if(!confirm_action_.is_null()) {
        const auto m = modal_box(confirm_action_.value("title", std::string("Confirm Action")), std::min(640., width - 140.), 380);
        const double mx = m[0], my = m[1], mw = m[2], mh = m[3];
        std::string msg = confirm_action_.value("message", std::string("Are you sure you want to proceed?"));
        auto* mlabel = ui.label(msg, mx + 32, my + 130, mw - 64, 130, 18, muted);
        invoke(mlabel, L"SetJustification", L"InJustification", uint8_t{1});
        invoke(mlabel, L"SetAutoWrapText", L"InAutoTextWrap", true);
        const double btn_w = (mw - 80) / 2, btn_y = my + mh - 64;
        bind(ui.button("", mx + 24, btn_y, btn_w, 44), {{"action", "ui_confirm_cancel"}});
        ui.box(mx + 24, btn_y, btn_w, 44, Color{.024f, .021f, .016f, 1});
        prompt("close", "Cancel", mx + 40, btn_y + 8, btn_w - 24, 5);
        bind(ui.button("", mx + mw - btn_w - 24, btn_y, btn_w, 44, true), {{"action", "ui_confirm_proceed"}});
        ui.box(mx + mw - btn_w - 24, btn_y, btn_w, 44, Color{.06f, .048f, .026f, 1});
        prompt("accept", "Confirm", mx + mw - btn_w - 8, btn_y + 8, btn_w - 24, 3);
    }
    if(native_picker_) {
        const auto m = modal_box(native_picker_title_.empty() ? "Browse" : native_picker_title_, std::min(880., width - 140.), 830);
        const double mx = m[0], my = m[1], mw = m[2], mh = m[3];
        ui.label("Search by name or keyword", mx + 32, my + 128, mw - 64, 30, 18, muted);
        ui.box(mx + 32, my + 166, mw - 64, 46, Color{.04f, .033f, .024f, 1});
        native_search_input_ = make_text_input(native_search_query_, mx + 44, my + 170, mw - 88, true);
        auto* list = construct(L"/Script/UMG.CanvasPanel", tree);
        native_search_results_ = list;
        ui.place(list, mx + 32, my + 234, mw - 64, 432);
        native_search_count_ = ui.label("", mx + 32, my + 680, mw - 64, 30, 18, muted);
        bind(ui.button("", mx + 24, my + mh - 64, 180, 44), {{"action", "ui_pick_cancel"}});
        prompt("close", "Back", mx + 36, my + mh - 53, 150, 5);
        bind(ui.button("", mx + mw - 234, my + mh - 64, 210, 44, true), {{"action", "ui_pick_apply"}});
        prompt("accept", "Select", mx + mw - 220, my + mh - 53, 180, 3);
        build_native_picker_results();
    }
    transition_widgets_.clear();
    // The slide-in needs every widget on the page and where it sits. That used to mean
    // asking the engine to enumerate the canvas, which is bounded, and one panel wide
    // enough to pass the bound reported CSS unavailable and took the tab off the strip.
    // The builder already knows what it placed and where, so read that instead. There
    // is no engine call here any more and nothing left to trip.
    //
    // Rows and tab labels sit on their own canvases inside the page and move with their
    // parent, so only the page's own children are collected.
    for(const auto& p:ui.placed) {
        if(p.canvas!=canvas) continue;
        transition_widgets_.push_back({WeakObject(p.widget),
            {p.x<left+panel+25?-150.*ui.scale:p.x>=right-25?150.*ui.scale:0.,
             p.x>=left+panel+25 && p.x<right-25?30.*ui.scale:0.}});
    }
    // Two separate budgets, and only one of them grows with the catalog. The page is
    // fixed chrome plus whatever the right panel is showing; the list is rows, about
    // six widgets each, on a canvas of its own inside the scroll box. Both are reported
    // so the cost of a page is a number we watch rather than one we find out about.
    page_widgets_=int(transition_widgets_.size());
    nested_widgets_=int(ui.placed.size())-page_widgets_;
    if(enter_transition_) { transition_started_=GetTickCount64(); enter_transition_=false; }
    last_message_.clear(); dirty_=false;
}
void InventoryUI::build_native_picker_results() {
    auto* canvas=native_search_results_.Get(); if(!canvas) return;
    invoke(canvas,L"ClearChildren");
    std::erase_if(hits_,[](const auto& hit){return hit.action.value("action",std::string{})=="ui_pick_row";});
    auto* page=page_.Get(); if(!page) return;
    auto* tree=inventory_object(page,L"WidgetTree");
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    auto* title=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
    InventoryLayout ui{{tree,canvas,layout_size_[1]/1080.,serif},title};
    const double width=std::min(880.,layout_size_[0]/layout_size_[1]*1080.-140.)-64;
    const auto& search=native_options_;
    const size_t first=search.selected/8*8;
    for(size_t i=first;i<std::min(first+8,search.matches.size());++i) {
        const auto& option=search.options[search.matches[i]];
        const double y=(i-first)*54.;
        auto* button=ui.button("",0,y,width,50,i==search.selected);
        if(i==search.selected) ui.box(0,y,width,50,Color{.055f,.045f,.027f,1});
        ui.selection_mark(16,y+19,i==search.selected);
        ui.label(option.at("label").get<std::string>(),44,y+10,width-60,32,21,i==search.selected?Color{.42f,.34f,.22f,1}:inventory_ink);
        hits_.push_back({WeakObject(button),{{"action","ui_pick_row"},{"row",i}},false});
    }
    if(search.matches.empty()) ui.label("No matching options",20,140,width-40,40,22,inventory_ink);
    if(auto* count=native_search_count_.Get()) text_value(count,std::to_string(search.matches.size())+" matches / "+std::to_string(search.options.size())+" options");
}
void InventoryUI::close_menu() {
    if(!main_.Get() || !inventory_bool(main_.Get(),L"bOpen")) return;
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
    if(name=="ui_section") { const int next=std::clamp(action.at("section").get<int>(),0,3); if(next==section_) return {}; section_=next; enter_transition_=true; row_=0; scroll_offset_=0; if(auto* s=scroll_.Get()) invoke(s,L"SetScrollOffset",L"NewScrollOffset",0.f); dirty_=true; return {}; }
    if(name=="ui_row") { row_=std::clamp(action.at("row").get<int>(),0,std::max(0,int(rows_.size())-1)); dirty_=true; if(section_!=1 && action.value("apply",false) && !rows_.empty()) return dispatch(rows_[row_].accept,state); return {}; }
    // The channel count comes from the page, because a spring has two and a colour three.
    if(name=="ui_channel") { channel_=(channel_+1)%std::clamp(action.value("count",3),1,4); dirty_=true; return {}; }
    if(name=="ui_tint_field") { tint_field_index_=(tint_field_index_+1)%3; dirty_=true; return {}; }
    if(name=="ui_exact") { exact_color_=!exact_color_; dirty_=true; return {}; }
    if(name=="ui_reset_view") { camera_stop(); camera_start(); return {}; }
    if(name=="ui_close") {
        if(active_ && !closing_) { closing_=true; transition_started_=GetTickCount64(); }
        return {};
    }
    if(name=="ui_confirm") {
        confirm_action_={
            {"action",action.at("target")},
            {"title",action.value("title",std::string("Confirm Action"))},
            {"message",action.value("message",std::string("Are you sure you want to proceed?"))}
        };
        dirty_=true; return {};
    }
    if(name=="ui_confirm_cancel") { confirm_action_=nullptr; dirty_=true; return {}; }
    if(name=="ui_confirm_proceed") {
        auto target=confirm_action_.value("action",Json{});
        confirm_action_=nullptr; dirty_=true;
        return dispatch(target,state);
    }
    if(name=="ui_browse_shells" && catalog_) {
        Json opts=Json::array();
        for(const auto& o:catalog_->outfits) {
            opts.push_back({{"id",o.id},{"label",o.name+" ("+o.author+")"}});
        }
        native_options_.reset(opts);
        native_picker_=true;
        native_picker_title_="Browse Outfits & Shells";
        native_picker_kind_="outfit";
        native_picker_target_.clear();
        native_search_query_.clear();
        dirty_=true;
        return {};
    }
    if(name=="ui_browse_templates" && catalog_ && appearance_) {
        auto sel=state.selections.find(appearance_->shell);
        const Outfit* worn_outfit=nullptr;
        if(sel!=state.selections.end()) for(const auto& o:catalog_->outfits) if(o.id==sel->second.outfit) worn_outfit=&o;
        if(!worn_outfit) return {};
        const auto& opts_set=worn_outfit->controls_for(sel->second.variant);
        Json opts=Json::array();
        opts.push_back({{"id","palette:original"},{"label","[Original] Author Default Materials"}});
        for(const auto& p:opts_set.palettes) {
            opts.push_back({{"id","palette:"+p.id},{"label","[Palette] "+p.name}});
        }
        for(const auto& t:worn_outfit->templates) {
            std::string tag=t.kind==TemplateKind::Combination?"Combination":
                            t.kind==TemplateKind::Archetype?"Archetype":
                            t.kind==TemplateKind::Physics?"Physics":
                            t.kind==TemplateKind::Hair?"Hair":
                            t.kind==TemplateKind::Jewelry?"Jewelry":
                            t.kind==TemplateKind::Glow?"Glow":"Template";
            opts.push_back({{"id","template:"+t.id},{"label","["+tag+"] "+t.name}});
        }
        native_options_.reset(opts);
        native_picker_=true;
        native_picker_title_="Browse Templates & Presets";
        native_picker_kind_="template";
        native_picker_target_.clear();
        native_search_query_.clear();
        dirty_=true;
        return {};
    }
    if(name=="ui_browse_choice" && catalog_ && appearance_) {
        auto sel=state.selections.find(appearance_->shell);
        const Outfit* worn_outfit=nullptr;
        if(sel!=state.selections.end()) for(const auto& o:catalog_->outfits) if(o.id==sel->second.outfit) worn_outfit=&o;
        if(!worn_outfit) return {};
        const auto& opts_set=worn_outfit->controls_for(sel->second.variant);
        std::string cid=action.at("control").get<std::string>();
        const Control* ctrl=opts_set.find(cid);
        if(!ctrl || ctrl->kind!=ControlKind::Choice) return {};
        Json opts=Json::array();
        for(size_t i=0;i<ctrl->options.size();++i) {
            opts.push_back({{"id",std::to_string(i)},{"label",ctrl->options[i].name}});
        }
        native_options_.reset(opts);
        native_picker_=true;
        native_picker_title_="Choose "+ctrl->name;
        native_picker_kind_="choice";
        native_picker_target_=cid;
        native_search_query_.clear();
        dirty_=true;
        return {};
    }
    if(name=="ui_pick_row") {
        native_options_.selected=std::min(action.at("row").get<size_t>(),native_options_.matches.empty()?size_t{}:native_options_.matches.size()-1);
        build_native_picker_results();
        return {};
    }
    if(name=="ui_pick_cancel") {
        native_picker_=false; dirty_=true; return {};
    }
    if(name=="ui_pick_apply") {
        const auto val=native_options_.value();
        if(val.is_null()) return {};
        native_picker_=false; dirty_=true;
        if(native_picker_kind_=="outfit") {
            const std::string outfit_id=val.get<std::string>();
            std::string variant_id;
            if(catalog_) for(const auto& o:catalog_->outfits) if(o.id==outfit_id && !o.variants.empty()) { variant_id=o.variants.front().id; break; }
            return {{"action","select"},{"outfit",outfit_id},{"variant",variant_id}};
        } else if(native_picker_kind_=="choice") {
            int idx=val.is_number()?val.get<int>():std::stoi(val.get<std::string>());
            return {{"action","control"},{"control",native_picker_target_},{"channel",0},{"value",double(idx)}};
        } else if(native_picker_kind_=="template") {
            std::string tid=val.get<std::string>();
            if(tid.starts_with("palette:")) return {{"action","palette"},{"palette",tid.substr(8)}};
            if(tid.starts_with("template:")) return {{"action","template"},{"template",tid.substr(9)}};
            return {{"action","template"},{"template",tid}};
        }
        return {};
    }
    if(name=="ui_save_template" || name=="ui_save_profile") return {{"action","save_look"},{"name",inventory_text(name_input_.Get())}};
    if(name=="ui_rename_template" || name=="ui_rename_profile") return {{"action","rename_look"},{"name",action.at("name")},{"new_name",inventory_text(name_input_.Get())}};
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
    catalog_=&catalog; appearance_=&appearance;
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
    const bool in_shell_view = active_ && !extension_active_ && (section_ == 0);
    if(was_in_shell_view_ && !in_shell_view) {
        // Just left the SHELL view (switched tabs, opened CSSX, or closed menu).
        // Update pinned outfit to whatever is currently worn so it is pinned on next visit.
        auto sel = state.selections.find(appearance.shell);
        pinned_outfit_id_ = (sel != state.selections.end()) ? sel->second.outfit : "";
    } else if(!was_in_shell_view_ && in_shell_view) {
        // Just entered the SHELL view. Snapshot whatever outfit is currently worn.
        auto sel = state.selections.find(appearance.shell);
        pinned_outfit_id_ = (sel != state.selections.end()) ? sel->second.outfit : "";
    }
    was_in_shell_view_ = in_shell_view;
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
    bool editing_extension=false, editing_native=false;
    if(extension_active_ && extension_picker_) if(auto* search=extension_search_input_.Get()) {
        // Keep the search field and caret alive. Replace only result rows.
        try {
            const auto query=inventory_text(search,256);
            if(query!=extension_search_query_) {
                extension_search_query_=query;
                if(extension_options_.filter(query)) build_extension_results();
            }
        } catch(const std::exception& error) {extension_error_=error.what();}
        editing_extension=true;
    }
    if(native_picker_) if(auto* search=native_search_input_.Get()) {
        try {
            const auto query=inventory_text(search,256);
            if(query!=native_search_query_) {
                native_search_query_=query;
                if(native_options_.filter(query)) build_native_picker_results();
            }
        } catch(...) {}
        editing_native=true;
    }
    if(extension_active_) {
        if(auto* input=name_input_.Get()) {
            Call focus(input,L"HasKeyboardFocus",1);focus.run();editing_extension=editing_extension || focus.get<bool>();
            try {extension_text_draft_=inventory_text(input,4096);}
            catch(const std::exception& e) {extension_error_=e.what();editing_extension=true;}
        }
        if(!sliders_.empty() && inventory_key(controller_.Get(),"LeftMouseButton")) editing_extension=true;
    }
    if(dirty_ && !editing_extension && !editing_native) {if(extension_active_) build_extensions();else build(catalog,state,appearance);}
    animate(GetTickCount64());
    if(extension_active_) for(const auto& bar:extension_loading_) if(auto* widget=bar.Get()) {
        const float pulse=.5f+.5f*float(std::sin(now*.004));
        invoke(widget,L"SetRenderOpacity",L"InOpacity",.35f+.65f*pulse);
    }
    if(!active_ || closing_) return {};
    // Scripted filming continues without desktop focus; input still requires it.
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
    if(!focused) { motion_.reset(); drag_pan_=drag_rotate_=false; return {}; }
    bool typing=false;
    if(auto* input=name_input_.Get()) { Call focus(input,L"HasKeyboardFocus",1); focus.run(); typing=focus.get<bool>(); }
    if(native_picker_) if(auto* search=native_search_input_.Get()) { Call focus(search,L"HasKeyboardFocus",1); focus.run(); typing=typing || focus.get<bool>(); }
    bool character_controls=!extension_active_;
    if(extension_active_ && !extension_id_.empty()) for(const auto& entry:extension_library_.at("extensions"))
        if(entry.at("id")==extension_id_) character_controls=entry.at("layout")=="inventory";
    if(extension_picker_ && extension_search_input_.Get()) {Call focus(extension_search_input_.Get(),L"HasKeyboardFocus",1);focus.run();typing=typing || focus.get<bool>();}
    if(extension_details_ || extension_picker_ || !extension_confirm_.is_null() || native_picker_ || !confirm_action_.is_null()) character_controls=false;
    if(!typing && character_controls) camera_update(elapsed,state.invert_orbit_x);
    else { motion_.reset(); drag_pan_=drag_rotate_=false; }
    if(extension_active_ && !typing && now>=extension_wheel_after_) {
        Call wheel(controller_.Get(),L"GetInputAnalogKeyState",2);auto* key=wheel.param(L"Key");
        member(wheel.data(key),key->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(L"MouseWheelAxis"));wheel.run();
        const float scroll=wheel.get<float>();
        bool over_details=false;
        if(auto* description=extension_description_.Get()) {Call hover(description,L"IsHovered",1);hover.run();over_details=hover.get<bool>();}
        if(std::abs(scroll)>.01f && extension_picker_) {
            extension_wheel_after_=now+100;return dispatch_extension({{"action","x_pick_move"},{"delta",scroll<0?1:-1}});
        }
        if(std::abs(scroll)>.01f && !character_controls && !over_details && !extension_details_) {
            extension_wheel_after_=now+100;
            return dispatch_extension(extension_id_.empty()?Json{{"action","x_page"},{"direction",scroll<0?1:-1}}:Json{{"action","x_scroll"},{"delta",scroll<0?1:-1}});
        }
    }
    if(!extension_active_ && native_picker_ && !typing && now>=native_wheel_after_) {
        Call wheel(controller_.Get(),L"GetInputAnalogKeyState",2);auto* key=wheel.param(L"Key");
        member(wheel.data(key),key->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(L"MouseWheelAxis"));wheel.run();
        const float scroll=wheel.get<float>();
        if(std::abs(scroll)>.01f) {
            native_wheel_after_=now+100;
            native_options_.move(scroll<0?1:-1);
            build_native_picker_results();
        }
    }
    if(typing) for(auto& binding:bindings_) {
        bool down=false,allowed=false;
        for(const auto& key:binding.keys) if(inventory_key(controller_.Get(),key)) {
            down=true;if((extension_picker_ || native_picker_) && (key.starts_with("Gamepad_") || key=="Escape")) allowed=true;
        }
        const bool repeat=binding.action=="up" || binding.action=="down";
        const bool trigger=allowed && down && (!binding.down || (repeat && now>=binding.repeat));
        if(down && !binding.down) binding.repeat=now+360;
        else if(trigger) binding.repeat=now+110;
        else if(!allowed) binding.repeat=now+360;
        binding.down=down;
        if(trigger) {
            if(extension_active_) {extension_input(binding.action);return {};}
            if(native_picker_) {
                if(binding.action=="up") { native_options_.move(-1); build_native_picker_results(); return {}; }
                if(binding.action=="down") { native_options_.move(1); build_native_picker_results(); return {}; }
                if(binding.action=="accept") return dispatch({{"action","ui_pick_apply"}},state);
                if(binding.action=="close") return dispatch({{"action","ui_pick_cancel"}},state);
            }
        }
    }
    if(!typing) for(auto& binding:bindings_) {
        bool down=false; for(const auto& key:binding.keys) if(inventory_key(controller_.Get(),key)) { down=true; break; }
        bool repeat=binding.action=="up" || binding.action=="down" || binding.action=="left" || binding.action=="right";
        bool triggered=down && (!binding.down || (repeat && now>=binding.repeat));
        if(down && !binding.down) binding.repeat=now+360; else if(triggered) binding.repeat=now+110;
        binding.down=down;
        if(!triggered) continue;
        if(extension_active_ && extension_input(binding.action)) return {};
        if(!confirm_action_.is_null()) {
            if(binding.action=="accept") return dispatch({{"action","ui_confirm_proceed"}},state);
            if(binding.action=="close") return dispatch({{"action","ui_confirm_cancel"}},state);
            continue;
        }
        if(native_picker_) {
            if(binding.action=="up") { native_options_.move(-1); build_native_picker_results(); return {}; }
            if(binding.action=="down") { native_options_.move(1); build_native_picker_results(); return {}; }
            if(binding.action=="accept") return dispatch({{"action","ui_pick_apply"}},state);
            if(binding.action=="close") return dispatch({{"action","ui_pick_cancel"}},state);
            continue;
        }
        if(binding.action=="close") return dispatch({{"action","ui_close"}},state);
        if(binding.action=="reset_view") return dispatch({{"action","ui_reset_view"}},state);
        if(binding.action=="previous_section" || binding.action=="next_section") return dispatch({{"action","ui_section"},{"section",(section_+(binding.action=="next_section"?1:3))%4}},state);
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
            if(extension_active_ && slider.action.value("action",std::string{})=="x_value") {
                auto c=extension_model_["sections"][extension_section_]["controls"][extension_row_];
                c["value"]=extensions::snap_value(c,v);
                text_value(slider.label.Get(),extensions::display_value(c));
                if(inventory_key(controller_.Get(),"LeftMouseButton")) continue;
                slider.previous=v;
                return dispatch_extension({{"action","x_value"},{"value",c.at("value")}});
            }
            // Hue is degrees, not a 0 to 100 proportion, so it reads as a whole number.
            const bool tint_slider=slider.action.contains("field");
            const bool degrees=tint_slider && slider.action.at("field")=="hue";
            slider.previous=v;
            text_value(slider.label.Get(),degrees?std::to_string(int(v)):
                       slider.unit=="%"?std::to_string(int(std::lround(v*100)))+"%":
                       slider_text(v,slider.scalar)+slider.unit);
            // Preserve mouse capture while the slider is dragged. Update the existing
            // headings and controller actions without rebuilding widgets. Colour
            // sliders carry a channel and tint sliders carry a field, so each kind
            // tracks its own selection and neither reads the other's key.
            if(tint_slider) {
                const auto field=slider.action.at("field").get<std::string>();
                tint_field_index_=field=="hue"?0:field=="saturation"?1:2;
                for(const auto& other:sliders_) if(auto* heading=other.heading.Get())
                    if(other.action.contains("field"))
                        invoke(heading,L"SetColorAndOpacity",L"InColorAndOpacity",
                               SlateColor{other.action.at("field")==field?gold:ivory});
                if(row_>=0 && row_<int(rows_.size())) for(auto* action:{&rows_[row_].previous,&rows_[row_].next})
                    if(action->is_object() && action->value("action",std::string{})=="tint") (*action)["field"]=field;
            } else if(slider.action.contains("channel")) {
                channel_=slider.action.at("channel").get<int>();
                for(const auto& channel:sliders_) if(auto* heading=channel.heading.Get())
                    if(channel.action.contains("channel"))
                        invoke(heading,L"SetColorAndOpacity",L"InColorAndOpacity",
                               SlateColor{channel.action.at("channel").get<int>()==channel_?gold:ivory});
                if(row_>=0 && row_<int(rows_.size())) for(auto* action:{&rows_[row_].previous,&rows_[row_].next})
                    if(action->is_object() && action->value("action",std::string{})=="control" && action->contains("channel"))
                        (*action)["channel"]=channel_;
            }
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
    Json value={{"cssx_active",extension_active_},{"extension",extension_id_},{"extension_page",extension_paging_.page},{"attached",tab_.Get()!=nullptr},{"active",active_},{"section",section_},{"row",row_},{"rows",rows_.size()},{"camera",display_.Get()!=nullptr},{"page_widgets",page_widgets_},{"nested_widgets",nested_widgets_},{"layout_size",layout_size_},{"yaw",yaw_},{"pan",pan_},{"zoom",zoom_},{"frame",frame_},{"gamepad",gamepad_}};
    #ifdef CSS_INVENTORY_DEV
    if(extension_active_ && name_input_.Get()) value["text"]=inventory_text(name_input_.Get(),4096);
    value["picker"]=extension_picker_;
    if(extension_picker_) {value["query"]=extension_search_query_;value["matches"]=extension_options_.matches.size();value["selected_option"]=extension_options_.value();}
    value["confirm_dialog"]=!confirm_action_.is_null();
    value["native_picker"]=native_picker_;
    if(native_picker_) {value["native_query"]=native_search_query_;value["native_matches"]=native_options_.matches.size();value["native_selected"]=native_options_.value();}
    value["menu_open"]=main_.Get() && inventory_bool(main_.Get(),L"bOpen");
    for(const auto& b:bindings_) value["bindings"][b.action]=b.keys;
    #endif
    return value;
}
}
namespace css {
void InventoryUI::message(const std::string& value) {
    if(value==last_message_) return;
    const bool template_status=(value.starts_with("Template ") || value.starts_with("Profile ")) && section_!=3;
    const bool routine=template_status || value.starts_with("Wearing ") || value=="Settings updated." || value=="Original appearance restored." || value.starts_with("Your saved appearance") || value.starts_with("Choose an appearance") || value.starts_with("No CSS outfit packages found.");
    if(auto* widget=status_.Get()) { text_value(widget,routine?"":value); last_message_=value; }
}
}

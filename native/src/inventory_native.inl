// Included from engine.cpp after inventory_ui.inl. The CSS page built from the game's own
// widget blueprints (beta.5): the Change Shade list rows and category headers, the options
// menu's selector and slider rows, the Inventory details window, its prompts and the
// confirmation dialog. They are used as views only: every native result (a click, a
// confirm, an arrow) is a Blueprint delegate CSS cannot bind, so CSS keeps reading the
// keys itself and polls each widget's own transparent button for mouse clicks. See
// docs/inventory-ui-development.md.
namespace css {
namespace {
constexpr const char* native_row_class="/Game/Sparta/UI/Menu/LandingArea/WBP_NB_SkinOption.WBP_NB_SkinOption_C";
constexpr const char* native_header_class="/Game/Sparta/UI/Menu/LandingArea/WBP_Skin_Category.WBP_Skin_Category_C";
constexpr const char* native_option_class="/Game/Sparta/UI/Menu/WBP_NB_Option.WBP_NB_Option_C";
constexpr const char* native_slider_class="/Game/Sparta/UI/Menu/WBP_NB_Option_Slider.WBP_NB_Option_Slider_C";
constexpr const char* native_divider_class="/Game/Sparta/UI/Menu/Options/WBP_SettingDivider.WBP_SettingDivider_C";
constexpr const char* native_tab_class="/Game/Sparta/UI/Menu/WBP_NB_Menu.WBP_NB_Menu_C";
constexpr const char* native_prompt_class="/Game/Sparta/UI/Core/Navigation/WBP_Prompt.WBP_Prompt_C";
constexpr const char* native_details_class="/Game/Sparta/UI/Menu/Equipment/WBP_Equipment_Description.WBP_Equipment_Description_C";
constexpr const char* native_dialog_class="/Game/Sparta/UI/Menu/Misc/WBP_ConfirmationPrompt_Default.WBP_ConfirmationPrompt_Default_C";
constexpr const char* native_listener_class="/Game/Sparta/UI/Core/Navigation/WBP_InputListener.WBP_InputListener_C";
constexpr const char* native_fade_class="/Game/Sparta/UI/Core/WBP_ScrollBoxFadeHandler.WBP_ScrollBoxFadeHandler_C";
constexpr const char* native_white="/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture";
// The game's menus are authored on a 3840x2160 canvas; the page is laid out in those units
// and scaled like the Inventory's own ScaleBox, so native widgets keep their proportions.
constexpr double native_height=2160, native_column=1136;
// ESlateVisibility
constexpr uint8_t shown_hit=0, collapsed=1, shown_passive=3, shown_self_passive=4;

UClass* native_class(const char* path) { return static_cast<UClass*>(load(path)); }
UObject* native_texture(const char* name) {
    std::string path="/Game/Sparta/UI/Common/Textures/"; path+=name; path+="."; path+=name;
    return load(path);
}
UObject* native_part(UObject* widget,const wchar_t* name) {
    auto* part=inventory_object(widget,name);
    if(!part) throw std::runtime_error("Native widget part is missing: "+narrow(name));
    return part;
}
// Every navigable game widget carries a transparent UButton (WBP_NavigationButton.ButtonWidget).
// Its IsPressed is how CSS sees a click without binding the widget's delegates.
UObject* native_button(UObject* widget,const wchar_t* member) {
    return native_part(native_part(widget,member),L"ButtonWidget");
}
UObject* native_add(UObject* panel,UObject* child) {
    Call add(panel,L"AddChild",2); add.set(L"content",child); add.run(); return add.get<UObject*>();
}
UObject* native_place(UObject* canvas,UObject* child,double x,double y,double w=0,double h=0) {
    Call add(canvas,L"AddChildToCanvas",2); add.set(L"content",child); add.run();
    auto* slot=add.get<UObject*>();
    invoke(slot,L"SetPosition",L"InPosition",Vec2{x,y});
    if(w>0) invoke(slot,L"SetSize",L"InSize",Vec2{w,h});
    else invoke(slot,L"SetAutoSize",L"InbAutoSize",true);
    return slot;
}
void native_padding(UObject* slot,Margin padding) { if(slot) invoke(slot,L"SetPadding",L"InPadding",padding); }
// Overlay slots start top-left; backgrounds and layers must fill their cell.
UObject* native_fill(UObject* slot) {
    invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{0});
    invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{0});
    return slot;
}
void native_visibility(UObject* widget,uint8_t value) { invoke(widget,L"SetVisibility",L"InVisibility",value); }
void native_brush(UObject* image,UObject* texture) {
    Call set(image,L"SetBrushFromTexture",2); set.set(L"Texture",texture); set.set(L"bMatchSize",false); set.run();
}
UObject* native_image(UObject* tree,UObject* texture,Color tint={1,1,1,1}) {
    auto* image=construct(L"/Script/UMG.Image",tree);
    native_brush(image,texture);
    invoke(image,L"SetColorAndOpacity",L"InColorAndOpacity",tint);
    native_visibility(image,shown_passive);
    return image;
}
// FText properties are written through the engine's own string conversion.
void native_text_property(UObject* widget,const wchar_t* name,const std::string& value) {
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
    convert.set(L"InString",FString(wide(value).c_str())); convert.run();
    auto* p=widget->GetPropertyByNameInChain(name);
    auto* result=convert.param(L"ReturnValue");
    if(!p || !p->SameType(result)) throw std::runtime_error("Native text property mismatch: "+narrow(name));
    p->CopyCompleteValue(reinterpret_cast<std::byte*>(widget)+p->GetOffset_Internal(),convert.data(result));
}
void native_call_text(UObject* widget,const wchar_t* function,const wchar_t* param,const std::string& value) {
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
    convert.set(L"InString",FString(wide(value).c_str())); convert.run();
    Call set(widget,function,1); set.copy(param,convert,L"ReturnValue"); set.run();
}
UObject* native_text_block(UObject* tree,float size,UObject* font,Color color,bool wrap) {
    auto* text=construct(L"/Script/UMG.TextBlock",tree);
    font_size(text,size,font);
    invoke(text,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{color});
    invoke(text,L"SetAutoWrapText",L"InAutoTextWrap",wrap);
    native_visibility(text,shown_passive);
    return text;
}
// The game's prompts draw no box when hovered, so these buttons draw nothing at all; a
// click plays the glyph's own key-press flash instead (see the Hit glyph).
UObject* native_flat_button(UObject* tree) {
    auto* button=construct(L"/Script/UMG.Button",tree);
    flat_button(button,false,0.f);
    auto* focusable=button->GetPropertyByNameInChain(L"IsFocusable");
    if(!focusable || !focusable->IsA<FBoolProperty>()) throw std::runtime_error("Button focus property mismatch");
    static_cast<FBoolProperty*>(focusable)->SetPropertyValueInContainer(button,false);
    return button;
}
// The game's palette, read off its widget trees (sRGB values converted to linear).
constexpr Color native_body{.49f,.42f,.30f,1};     // #BAAE94 option text
constexpr Color native_muted{.24f,.21f,.17f,1};    // prompt labels on dark art
constexpr Color native_prompt{.40f,.37f,.33f,1};   // bottom bar labels
}

void InventoryUI::native_visible(NativeItem& item,bool on) {
    if(item.shown==int(on)) return;
    if(auto* widget=item.widget.Get()) native_visibility(widget,on?shown_self_passive:collapsed);
    item.shown=on;
}
void InventoryUI::native_text(UObject* block,std::string& shown,const std::string& value) {
    if(!block || shown==value) return;
    text_value(block,value); shown=value;
}
void InventoryUI::native_state(NativeItem& item,bool selected) {
    if(item.selected==int(selected)) return;
    if(auto* widget=item.widget.Get()) invoke(widget,selected?L"OnSelectedState":L"OnNullState");
    item.selected=selected;
}
// Glyph for a CSS binding: the game's own InputAction where the binding has one, so a
// rebound key shows its new icon, else the fixed KBM/controller enum values.
void InventoryUI::native_glyph(UObject* widget,const std::string& action,uint8_t fallback,uint8_t keyboard) {
    bool bound=false;
    for(const auto& b:bindings_) if(b.action==action) {
        bound=true;
        // These two controller shortcuts differ from their native actions.
        // An InputAction would refresh the icon back to the game's mapping.
        if(action!="toggle_light" && action!="tertiary") object_property(widget,L"InputAction",b.input_action.Get());
        else {
            object_property(widget,L"InputAction",nullptr);
            fallback=45; // E_ControllerButton::None, no conflicting shortcut.
            for(const auto& key:b.keys) {
                if(key=="Gamepad_FaceButton_Top") fallback=2;
                if(key=="Gamepad_Special_Left") fallback=17;
            }
        }
        for(auto key:b.keys) if(!key.starts_with("Gamepad_")) {
            if(key=="SpaceBar") key="Spacebar";
            if(key=="LeftControl") key="Ctrl";
            for(const auto& [name,value]:inventory_keyboard_icons) if(name==key) { inventory_value(widget,L"KBMPrompt",value); break; }
            break;
        }
    }
    if(!bound) object_property(widget,L"InputAction",nullptr);
    if(keyboard!=255) inventory_value(widget,L"KBMPrompt",keyboard);
    inventory_value(widget,L"ControllerPrompt",fallback);
    inventory_value(widget,L"PromptSize",Vec2{80,80});
    inventory_value(widget,L"OverrideControllerSize",Vec2{80,80});
    inventory_value(widget,L"OverrideKBMSize",Vec2{80,80});
    invoke(widget,L"UpdatePrompt"); invoke(widget,L"UpdatePromptSize");
}
void InventoryUI::native_forget() {
    native_dialog_close();
    for(auto* stack:{&tab_items_,&list_,&panel_head_,&panel_,&actions_,&footer_,&camera_bar_}) { stack->cells.clear(); stack->used=0; stack->box.Reset(); }
    design_.Reset(); left_root_.Reset(); right_root_.Reset(); center_root_.Reset();
    list_scroll_.Reset(); strip_scroll_.Reset(); details_.Reset(); panel_scroll_.Reset(); panel_size_.Reset(); status_text_.Reset(); strip_previous_.Reset(); strip_next_.Reset(); strip_previous_glyph_.Reset(); strip_next_glyph_.Reset();
    design_w_=design_scale_=0; shown_section_=revealed_row_=-1;
    panel_context_.clear(); panel_revealed_=nullptr; panel_fit_pending_=false; panel_max_=720.f; pending_reveals_={};
    detail_title_.clear(); detail_sub_.clear(); detail_body_.clear(); status_shown_.clear();
    detail_icon_=reinterpret_cast<const void*>(1);
}
// The page skeleton: three columns on a design-unit canvas, the tab strip, the list, the
// details window and the prompt bars. Built once per layout size; builds only fill it.
bool InventoryUI::native_page(double width,double height) {
    const double scale=height/native_height, design_width=width/scale;
    if(design_.Get() && std::abs(design_width-design_w_)<1 && std::abs(scale-design_scale_)<1e-4) return true;
    auto* canvas=canvas_.Get(); auto* page=page_.Get(); auto* pc=controller_.Get();
    if(!canvas || !page || !pc) return false;
    native_forget();
    invoke(canvas,L"ClearChildren");
    auto* tree=inventory_object(page,L"WidgetTree");
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    AssetLoadRoots roots;
    // canvas > ScaleBox (user scale) > SizeBox (design size) > CanvasPanel in design units.
    auto* scaler=construct(L"/Script/UMG.ScaleBox",tree);
    invoke(scaler,L"SetStretch",L"InStretch",uint8_t{7});            // EStretch::UserSpecified
    invoke(scaler,L"SetUserSpecifiedScale",L"InUserSpecifiedScale",float(scale));
    native_place(canvas,scaler,0,0,width,height);
    auto* frame=construct(L"/Script/UMG.SizeBox",tree);
    invoke(frame,L"SetWidthOverride",L"InWidthOverride",float(design_width));
    invoke(frame,L"SetHeightOverride",L"InHeightOverride",float(native_height));
    content(scaler,frame);
    auto* design=construct(L"/Script/UMG.CanvasPanel",tree); content(frame,design);
    design_=design; design_w_=design_width; design_scale_=scale;
    // The Inventory fades its scroll boxes at the edges: a RetainerBox around the scroll,
    // and a WBP_ScrollBoxFadeHandler that maps the scroll offset onto the fade material.
    // Its material and curves are copied from the Inventory's own handler.
    auto* character=inventory_object(main_.Get(),L"WBP_MGT_Character");
    auto faded=[&](UObject* holder,UObject* scroll,const wchar_t* source,UObject* host) {
        auto* retainer=construct(L"/Script/UMG.RetainerBox",tree);
        content(holder,retainer); content(retainer,scroll);
        auto* native=inventory_object(character,source);
        if(!native) return;
        auto* handler=inventory_create(pc,native_class(native_fade_class));
        native_place(host,handler,0,0,1,1);
        for(auto name:{L"FadeMaterial",L"EdgeStart",L"EdgeEnd"}) inventory_copy(handler,native,name);
        Call init(handler,L"Initialize",2); init.set(L"RetainerBox",retainer); init.set(L"ScrollBox",scroll); init.run();
        invoke(handler,L"SetEnabledState",L"Enabled",true);
    };
    auto column=[&](double x,double w) {
        auto* root=construct(L"/Script/UMG.CanvasPanel",tree);
        native_place(design,root,x,0,w,native_height);
        return root;
    };
    auto* left=column(0,native_column), *right=column(design_width-native_column,native_column);
    auto* center=column(native_column,std::max(1.,design_width-2*native_column));
    left_root_=left; right_root_=right; center_root_=center;
    // Logo: CSS's own mark stays above the tab strip (runtime-imported texture).
    if(!logo_path_.empty() && fs::exists(logo_path_)) {
        auto& cached=textures_[path_utf8(logo_path_)]; auto* texture=cached.Get();
        if(!texture) {
            Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
            import.set(L"WorldContextObject",pc); import.set(L"Filename",FString(logo_path_.c_str())); import.run();
            texture=import.get<UObject*>(); cached=texture;
        }
        if(texture) native_place(left,native_image(tree,texture),8,100,1120,373);
    }
    // Section strip: the Inventory filter strip recipe. T_UI_Nav_TitleBG, a Z/X prompt each
    // side, and the tabs in a clipped horizontal scroll that slides the selected tab in.
    // The frame and its contents share one overlay, the row centred in it, so the Z and X
    // glyphs sit the same distance in from each end of the frame.
    auto* strip_frame=construct(L"/Script/UMG.Overlay",tree);
    native_place(left,strip_frame,40,520,1056,150);
    native_fill(native_add(strip_frame,native_image(tree,native_texture("T_UI_Nav_TitleBG"))));
    auto* strip=construct(L"/Script/UMG.HorizontalBox",tree);
    auto* strip_slot=native_add(strip_frame,strip);
    invoke(strip_slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{2});
    invoke(strip_slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
    // Each glyph sits on a flat button, so a click on Z or X switches tabs like the key.
    auto prompt=[&](const std::string& action,uint8_t fallback,WeakObject& hit,WeakObject& shown) {
        auto* button=native_flat_button(tree);
        auto* slot=native_add(strip,button);
        invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
        auto* glyph=inventory_create(pc,native_class(native_prompt_class));
        content(button,glyph);
        native_glyph(glyph,action,fallback);
        native_visibility(glyph,shown_passive);
        hit=button; shown=glyph;
        return glyph;
    };
    input_prompt_=prompt("previous_section",8,strip_previous_,strip_previous_glyph_);
    auto* clip=construct(L"/Script/UMG.SizeBox",tree);
    invoke(clip,L"SetWidthOverride",L"InWidthOverride",780.f); invoke(clip,L"SetHeightOverride",L"InHeightOverride",80.f);
    auto* clip_slot=native_add(strip,clip);
    invoke(clip_slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
    native_padding(clip_slot,Margin{18,0,18,0});
    auto* scroll=construct(L"/Script/UMG.ScrollBox",tree);
    invoke(scroll,L"SetOrientation",L"NewOrientation",uint8_t{0});
    invoke(scroll,L"SetScrollBarVisibility",L"NewScrollBarVisibility",uint8_t{1});
    faded(clip,scroll,L"WBP_SBFH_InventoryFilter",left); strip_scroll_=scroll;
    auto* tabs=construct(L"/Script/UMG.HorizontalBox",tree);
    native_add(scroll,tabs); tab_items_.box=tabs;
    prompt("next_section",9,strip_next_,strip_next_glyph_);
    native_place(left,native_image(tree,native_texture("T_UI_Nav_Title_Divider")),0,690,native_column,6);
    // The list: Change Shade's 1000-wide scroll with the Inventory scrollbar brush.
    auto* list_size=construct(L"/Script/UMG.SizeBox",tree);
    invoke(list_size,L"SetWidthOverride",L"InWidthOverride",1040.f);
    invoke(list_size,L"SetHeightOverride",L"InHeightOverride",1250.f);
    native_place(left,list_size,48,720);
    auto* list_scroll=construct(L"/Script/UMG.ScrollBox",tree);
    if(auto* bar=inventory_object(inventory_object(inventory_object(main_.Get(),L"WBP_MGT_Character"),L"WBP_CSB_Style2"),L"Image_Bar")) {
        auto* style=list_scroll->GetPropertyByNameInChain(L"WidgetBarStyle");
        auto* brush=bar->GetPropertyByNameInChain(L"Brush");
        if(style && style->IsA<FStructProperty>() && brush) {
            auto* info=find(L"/Script/SlateCore.ScrollBarStyle");
            for(auto name:{L"NormalThumbImage",L"HoveredThumbImage",L"DraggedThumbImage"}) {
                auto* target=info->GetPropertyByNameInChain(name);
                if(target && target->SameType(brush)) target->CopyCompleteValue(reinterpret_cast<std::byte*>(list_scroll)+style->GetOffset_Internal()+target->GetOffset_Internal(),reinterpret_cast<std::byte*>(bar)+brush->GetOffset_Internal());
            }
        }
    }
    invoke(list_scroll,L"SetAllowOverscroll",L"NewAllowOverscroll",false);
    invoke(list_scroll,L"SetAnimateWheelScrolling",L"bShouldAnimateWheelScrolling",true);
    invoke(list_scroll,L"SetScrollbarThickness",L"NewScrollbarThickness",Vec2{8,8});
    faded(list_size,list_scroll,L"WBP_SBFH_Inventory",left); list_scroll_=list_scroll; scroll_=list_scroll;
    auto* list=construct(L"/Script/UMG.VerticalBox",tree);
    native_add(list_scroll,list); list_.box=list;
    // Bottom prompt bar, where the Inventory has "Esc Close  WASD Navigate".
    auto* footer=construct(L"/Script/UMG.HorizontalBox",tree);
    native_place(left,footer,56,2020); footer_.box=footer;
    // Camera controls under the character.
    auto* camera=construct(L"/Script/UMG.HorizontalBox",tree);
    auto* camera_slot=native_place(center,camera,0,-60);
    struct Anchors { Vec2 minimum, maximum; };
    invoke(camera_slot,L"SetAnchors",L"InAnchors",Anchors{{.5,1},{.5,1}});
    invoke(camera_slot,L"SetAlignment",L"InAlignment",Vec2{.5,1});
    camera_bar_.box=camera;
    // The Inventory details window. Construct hides it and fills it with designer samples;
    // Show() and clearing those boxes leaves the native frame for CSS's content.
    auto* details=inventory_create(pc,native_class(native_details_class));
    native_place(right,details,95,150);
    details_=details;
    invoke(details,L"Show");
    invoke(native_part(details,L"SizeBox_Main"),L"SetWidthOverride",L"InWidthOverride",945.f);
    invoke(details,L"CollapseDetails");
    for(auto name:{L"VB_AbilityList",L"VB_EffectList",L"VB_TooltipList",L"VB_CustomWidgets",L"VB_DynamicPrompts"})
        if(auto* box=inventory_object(details,name)) invoke(box,L"ClearChildren");
    if(auto* sample=inventory_object(details,L"DetailsPrompt")) native_visibility(sample,collapsed);
    native_visibility(native_part(details,L"Size_SubHeader"),collapsed);
    // What must stay put while the part below scrolls, such as the search field.
    auto* head=construct(L"/Script/UMG.VerticalBox",tree);
    native_add(native_part(details,L"VB_CustomWidgets"),head);
    panel_head_.box=head;
    auto* panel_size=construct(L"/Script/UMG.SizeBox",tree);
    invoke(panel_size,L"SetMaxDesiredHeight",L"InMaxDesiredHeight",panel_max_);
    native_add(native_part(details,L"VB_CustomWidgets"),panel_size);
    panel_size_=panel_size;
    auto* panel_scroll=construct(L"/Script/UMG.ScrollBox",tree);
    invoke(panel_scroll,L"SetScrollBarVisibility",L"NewScrollBarVisibility",uint8_t{1});
    invoke(panel_scroll,L"SetAllowOverscroll",L"NewAllowOverscroll",false);
    invoke(panel_scroll,L"SetAnimateWheelScrolling",L"bShouldAnimateWheelScrolling",true);
    faded(panel_size,panel_scroll,L"WBP_SBFH_Inventory",right); panel_scroll_=panel_scroll;
    auto* panel=construct(L"/Script/UMG.VerticalBox",tree);
    native_add(panel_scroll,panel); panel_.box=panel;
    auto* actions=construct(L"/Script/UMG.VerticalBox",tree);
    native_padding(native_add(native_part(details,L"VB_DynamicPrompts"),actions),Margin{0,10,0,10});
    actions_.box=actions;
    // Feedback line ("Wearing ...") under the window.
    auto* status=native_text_block(tree,30,serif,native_muted,true);
    native_place(right,status,95,1990,945,110);
    status_text_=status; status_=status;
    transition_widgets_={{WeakObject(left),{-150,0}},{WeakObject(right),{150,0}},{WeakObject(center),{0,30}}};
    return true;
}
void InventoryUI::native_reveal_pending() {
    for(auto& pending:pending_reveals_) {
        if(pending.frames<=0 || --pending.frames>0) continue;
        auto* scroll=pending.scroll.Get(); auto* target=pending.target.Get();
        if(!scroll || !target) continue;
        Call call(scroll,L"ScrollWidgetIntoView",4);
        call.set(L"WidgetToFind",target); call.set(L"AnimateScroll",false);
        call.set(L"ScrollDestination",pending.destination); call.set(L"Padding",120.f); call.run();
    }
}
// The window's own parts (title, picture, description, prompts) keep their size; the
// scrolling part gets the height left between the window's top and the status line.
void InventoryUI::native_fit_panel() {
    if(!panel_fit_pending_) return;
    panel_fit_pending_=false;
    auto* details=details_.Get(); auto* size=panel_size_.Get();
    if(!details || !size) return;
    Call window(details,L"GetDesiredSize",1); window.run();
    Call panel(size,L"GetDesiredSize",1); panel.run();
    constexpr float window_top=150.f, status_top=1990.f, gap=24.f;
    const float fixed=float(window.get<Vec2>().y-panel.get<Vec2>().y);
    const float room=std::clamp(status_top-gap-window_top-fixed,320.f,1200.f);
    if(std::abs(room-panel_max_)<=2.f) return;
    invoke(size,L"SetMaxDesiredHeight",L"InMaxDesiredHeight",room);
    panel_max_=room;
    panel_fit_pending_=true;   // measure again once the new height has laid out
}
InventoryUI::NativeItem& InventoryUI::native_take(NativeStack& stack,NativeKind kind) {
    auto* column=stack.box.Get();
    if(!column) throw std::runtime_error("Native page container is unavailable");
    if(stack.used==stack.cells.size()) {
        auto* holder=construct(L"/Script/UMG.Overlay",inventory_object(page_.Get(),L"WidgetTree"));
        native_add(column,holder);
        native_visibility(holder,shown_self_passive);
        stack.cells.push_back({WeakObject(holder),{},1});
    }
    auto& cell=stack.cells[stack.used++];
    auto* box=cell.holder.Get();
    if(!box) throw std::runtime_error("Native page cell is unavailable");
    if(cell.shown!=1) { native_visibility(box,shown_self_passive); cell.shown=1; }
    NativeItem* wanted=nullptr;
    for(auto& item:cell.kinds) if(item.kind==kind && item.widget.Get()) wanted=&item;
    for(auto& item:cell.kinds) if(&item!=wanted) native_visible(item,false);
    if(wanted) { native_visible(*wanted,true); return *wanted; }
    auto* pc=controller_.Get(); auto* tree=inventory_object(page_.Get(),L"WidgetTree");
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    NativeItem item; item.kind=kind; item.shown=1;
#ifdef CSS_INVENTORY_DEV
    ++created_widgets_;
#endif
    UObject* widget=nullptr; UObject* slot=nullptr;
    switch(kind) {
    case NativeKind::row:
        widget=inventory_create(pc,native_class(native_row_class)); slot=native_add(box,widget);
        item.text_block=native_part(widget,L"Button_Text");
        item.hit=native_button(widget,L"MyNavigationButton");
        break;
    case NativeKind::header:
        widget=inventory_create(pc,native_class(native_header_class)); slot=native_add(box,widget);
        item.text_block=native_part(widget,L"Text_Category");
        break;
    case NativeKind::option: case NativeKind::slider: {
        const bool slider=kind==NativeKind::slider;
        widget=inventory_create(pc,native_class(slider?native_slider_class:native_option_class)); slot=native_add(box,widget);
        item.text_block=native_part(widget,L"Text_Option");
        item.value_block=native_part(widget,L"Text_Option_Value");
        item.hit=native_button(widget,L"WBP_NavButton_Main");
        if(slider) {
            item.hit_left=native_part(native_part(widget,L"WBP_ArrowButton_L"),L"ArrowButton");
            item.hit_right=native_part(native_part(widget,L"WBP_ArrowButton_R"),L"ArrowButton");
            item.extra=native_part(widget,L"WBP_GenericBar");
        } else {
            item.hit_left=native_part(widget,L"Button_Left");
            item.hit_right=native_part(widget,L"Button_Right");
        }
        break;
    }
    case NativeKind::divider:
        widget=inventory_create(pc,native_class(native_divider_class)); slot=native_add(box,widget);
        item.text_block=native_part(widget,L"Text_DividerName");
        break;
    case NativeKind::tab: {
        widget=inventory_create(pc,native_class(native_tab_class)); slot=native_add(box,widget);
        // Style copied from the Inventory's own filter tabs (Trajan SemiBold 32, spacing 25).
        auto* filter=inventory_object(inventory_object(main_.Get(),L"WBP_MGT_Character"),L"BP_HBC_InventoryFilter");
        auto children=filter?inventory_children(filter):std::vector<UObject*>{};
        if(!children.empty() && children.front()) {
            for(auto name:{L"FontData",L"SelectedColor",L"DefaultColor",L"HighlightY",L"bUseHighlight"}) inventory_copy(widget,children.front(),name);
            if(auto* native_slot=inventory_object(children.front(),L"Slot")) native_padding(slot,read<Margin>(native_slot,L"Padding"));
        } else native_padding(slot,Margin{25,0,25,0});
        item.hit=native_button(widget,L"WBP_NavigationButton");
        break;
    }
    case NativeKind::action: {
        // A prompt the way the details window lists "Equip as Active": the game's glyph
        // and label, on a flat button so a mouse can click it.
        widget=native_flat_button(tree); slot=native_add(box,widget);
        auto* line=construct(L"/Script/UMG.HorizontalBox",tree); content(widget,line);
        UObject* glyphs[2]{};
        for(auto*& glyph:glyphs) {
            glyph=inventory_create(pc,native_class(native_prompt_class));
            auto* glyph_slot=native_add(line,glyph);
            invoke(glyph_slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
            native_padding(glyph_slot,Margin{0,0,4,0});
            native_visibility(glyph,shown_passive);
        }
        native_visibility(glyphs[1],collapsed);
        auto* glyph=glyphs[0]; item.cells={WeakObject(glyphs[1])};
        auto* label=native_text_block(tree,32,serif,native_body,false);
        auto* label_slot=native_add(line,label);
        invoke(label_slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
        native_padding(label_slot,Margin{14,0,0,0});
        item.hit=widget; item.extra=glyph; item.text_block=label;
        break;
    }
    case NativeKind::swatches: {
        widget=construct(L"/Script/UMG.UniformGridPanel",tree); slot=native_add(box,widget);
        invoke(widget,L"SetSlotPadding",L"InSlotPadding",Margin{6,6,6,6});
        break;
    }
    case NativeKind::input: {
        widget=construct(L"/Script/UMG.Overlay",tree); slot=native_add(box,widget);
        auto* back=native_image(tree,native_texture("T_UI_Resource_BG_02"));
        native_fill(native_add(widget,back));
        auto* input=construct(L"/Script/UMG.EditableText",tree);
        Call current(input,L"GetFont",1); current.run();
        Call set(input,L"SetFont",1); set.copy(L"InFontInfo",current,L"ReturnValue");
        auto* font=set.param(L"InFontInfo"); auto* info=find(L"/Script/SlateCore.SlateFontInfo");
        member(set.data(font),font->GetElementSize(),info,L"FontObject",serif);
        member(set.data(font),font->GetElementSize(),info,L"Size",34.f);
        member(set.data(font),font->GetElementSize(),info,L"TypefaceFontName",FName(L"Regular")); set.run();
        native_padding(native_fill(native_add(widget,input)),Margin{28,16,28,16});
        native_call_text(input,L"SetHintText",L"InHintText","Type a name or keyword");
        item.extra=input;
        break;
    }
    case NativeKind::paragraph: {
        widget=native_text_block(tree,30,serif,native_muted,true); slot=native_add(box,widget);
        item.text_block=widget;
        break;
    }
    case NativeKind::none: throw std::runtime_error("Native item without a kind");
    }
    item.widget=widget;
    native_slot(kind,slot);
    native_setup(item);
    cell.kinds.push_back(std::move(item));
    return cell.kinds.back();
}
// Layout the game's blueprints reset in their Construct, which runs again every time the
// menu reopens: applied at creation and again by native_invalidate.
void InventoryUI::native_setup(NativeItem& item) {
    auto* widget=item.widget.Get();
    if(!widget) return;
    switch(item.kind) {
    case NativeKind::row:
        // Change Shade's names are short; outfit names are not. Its overlay slot fills, which
        // would stretch the box past its width; left-aligned, the width is what the name gets.
        invoke(native_part(widget,L"SizeBox_Name"),L"SetWidthOverride",L"InWidthOverride",620.f);
        invoke(inventory_object(native_part(widget,L"SizeBox_Name"),L"Slot"),L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{1});
        invoke(item.text_block.Get(),L"SetTextOverflowPolicy",L"InOverflowPolicy",uint8_t{1});
        // Slate only draws the ellipsis on left- or right-justified text; the row centres its
        // name (which already sits left in its box), so justify left.
        invoke(item.text_block.Get(),L"SetJustification",L"InJustification",uint8_t{0});
        break;
    case NativeKind::option: case NativeKind::slider:
        if(auto* name=inventory_object(widget,L"SizeBox_OptionName")) invoke(name,L"SetWidthOverride",L"InWidthOverride",300.f);
        if(item.kind==NativeKind::slider) {
            if(auto* arrows=inventory_object(widget,L"SizeBox_SliderAndArrows")) invoke(arrows,L"SetWidthOverride",L"InWidthOverride",430.f);
        } else {
            // Construct sizes the value box from OptionWidthOverride, which only the
            // settings logic object sets; without it the value and arrows are zero-width.
            invoke(native_part(widget,L"SizeBox_OptionValueAndArrows"),L"SetWidthOverride",L"InWidthOverride",560.f);
            invoke(native_part(widget,L"SizeBox_OptionValue"),L"SetWidthOverride",L"InWidthOverride",440.f);
            invoke(native_part(widget,L"Spacer_57"),L"SetSize",L"InSize",Vec2{24,0});
        }
        break;
    default: break;
    }
}
// The menu reopened: its blueprints ran Construct again and put back their designer
// text, badges, highlights and box sizes. Forget what the page believes is on screen so
// the next build writes all of it, and redo the layout Construct undid.
void InventoryUI::native_invalidate() {
    for(auto* stack:{&tab_items_,&list_,&panel_head_,&panel_,&actions_,&footer_,&camera_bar_})
        for(auto& cell:stack->cells) {
            cell.shown=-1;
            for(auto& item:cell.kinds) {
                item.text.clear(); item.value.clear(); item.glyph.clear();
                item.selected=item.badge=item.shown=item.enabled=item.icon_shown=-1;
                item.icon=nullptr; item.chip={-1,-1,-1,-1}; item.fill=-2.f; item.name_width=-1.f;
                native_setup(item);
            }
        }
    detail_title_.clear(); detail_sub_.clear(); detail_body_.clear(); status_shown_.clear();
    detail_icon_=reinterpret_cast<const void*>(1);
    if(auto* details=details_.Get()) {
        invoke(details,L"Show");
        invoke(native_part(details,L"SizeBox_Main"),L"SetWidthOverride",L"InWidthOverride",945.f);
        invoke(details,L"CollapseDetails");
        if(auto* sample=inventory_object(details,L"DetailsPrompt")) native_visibility(sample,collapsed);
    }
    panel_context_.clear(); panel_revealed_=nullptr; revealed_row_=-1; shown_section_=-1;
    panel_fit_pending_=true;
}
// Slot padding per kind, inside the cell.
void InventoryUI::native_slot(NativeKind kind,UObject* slot) {
    native_fill(slot);   // an overlay slot starts top-left; the cell stands in for a box slot, which fills
    switch(kind) {
    case NativeKind::divider: native_padding(slot,Margin{30,24,30,6}); break;
    case NativeKind::action: native_padding(slot,Margin{40,4,20,4}); break;
    case NativeKind::swatches: native_padding(slot,Margin{40,16,40,16}); break;
    case NativeKind::input: native_padding(slot,Margin{40,12,40,12}); break;
    case NativeKind::paragraph: native_padding(slot,Margin{40,10,40,10}); break;
    default: break;   // rows, headers, options and sliders size themselves; tabs are styled at creation
    }
}
void InventoryUI::native_finish(NativeStack& stack) {
    for(size_t i=stack.used;i<stack.cells.size();++i) {
        auto& cell=stack.cells[i];
        if(cell.shown==0) continue;
        if(auto* holder=cell.holder.Get()) native_visibility(holder,collapsed);
        cell.shown=0;
    }
}
// The confirmation is the game's WBP_ConfirmationPrompt_Default, driven the way Traverse
// drives it: texts, InitData, its own listener off, every live menu listener frozen while
// it is up (so Q/E cannot switch tabs underneath), and the highlight set by CSS's keys.
void InventoryUI::native_dialog() {
    const auto title=confirm_action_.value("title",std::string("Confirm"));
    const auto text=confirm_action_.value("message",std::string("Are you sure?"));
    const auto key=title+"\n"+text;
    auto* dialog=dialog_.Get();
    if(!dialog || dialog_shown_!=key) {
        native_dialog_close();
        auto* pc=controller_.Get();
        dialog=inventory_create(pc,native_class(native_dialog_class));
        dialog_=dialog; dialog_shown_=key; dialog_focus_=0; dialog_focus_shown_=-1;
        native_text_property(dialog,L"PromptText",title);
        native_text_property(dialog,L"PrimaryOptionText","Confirm");
        native_text_property(dialog,L"SecondaryOptionText","Cancel");
        native_text_property(dialog,L"OptionalDescription",text);
        invoke(dialog,L"InitData");
        invoke(dialog,L"AddToViewport",L"ZOrder",int32_t{1000});
        invoke(dialog,L"DisableInputListener");
        invoke(dialog,L"HandleDescription",L"Valid",true);
        dialog_primary_=inventory_object(dialog,L"PrimaryOption");
        dialog_secondary_=inventory_object(dialog,L"SecondaryOption");
        // Each option carries its input glyph beside the label, as Traverse's dialog does.
        auto* tree=inventory_object(dialog,L"WidgetTree");
        auto* trajan=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
        auto glyph_label=[&](UObject* option,const std::string& label,const std::string& binding,uint8_t fallback) {
            if(!option || !tree) return;
            auto* overlay=inventory_object(option,L"Overlay_Main");
            auto* text=inventory_object(option,L"Button_Text");
            if(!overlay || !text) return;
            native_visibility(text,collapsed);
            auto* line=construct(L"/Script/UMG.HorizontalBox",tree);
            auto* slot=native_add(overlay,line);
            invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{2});
            invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
            native_visibility(line,shown_passive);
            auto* glyph=inventory_create(pc,native_class(native_prompt_class));
            invoke(native_add(line,glyph),L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
            native_glyph(glyph,binding,fallback);
            native_visibility(glyph,shown_passive);
            auto* name=native_text_block(tree,32,trajan,Color{.12f,.11f,.09f,1},false);
            text_value(name,label);
            auto* name_slot=native_add(line,name);
            invoke(name_slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
            native_padding(name_slot,Margin{12,0,0,0});
        };
        glyph_label(dialog_primary_.Get(),"Confirm","accept",3);
        glyph_label(dialog_secondary_.Get(),"Cancel","close",5);
        Call all(find(L"/Script/UMG.Default__WidgetBlueprintLibrary"),L"GetAllWidgetsOfClass",4);
        all.set(L"WorldContextObject",pc); all.set(L"WidgetClass",native_class(native_listener_class)); all.set(L"TopLevelOnly",false); all.run();
        auto* found=all.param(L"FoundWidgets");
        if(found->IsA<FArrayProperty>()) {
            FScriptArrayHelper listeners(static_cast<FArrayProperty*>(found),all.data(found));
            for(int i=0;i<std::min(listeners.Num(),256);++i) {
                UObject* listener{}; std::memcpy(&listener,listeners.GetRawPtr(i),sizeof(listener));
                if(!listener) continue;
                Call enabled(listener,L"IsEnabled",1); enabled.run();
                if(!enabled.get<bool>()) continue;
                invoke(listener,L"SetEnabledState",L"bEnabled",false);
                frozen_listeners_.emplace_back(listener);
            }
        }
    }
    if(dialog_focus_shown_!=dialog_focus_) {
        auto* on=(dialog_focus_==0?dialog_primary_:dialog_secondary_).Get();
        auto* off=(dialog_focus_==0?dialog_secondary_:dialog_primary_).Get();
        if(off) invoke(off,L"OnNullState");
        if(on) invoke(on,L"OnHighlightedState");
        dialog_focus_shown_=dialog_focus_;
    }
    if(auto* primary=dialog_primary_.Get()) hits_.push_back({WeakObject(native_button(primary,L"WBP_NavigationButton")),{{"action","ui_confirm_proceed"}},false,{},{}});
    if(auto* secondary=dialog_secondary_.Get()) hits_.push_back({WeakObject(native_button(secondary,L"WBP_NavigationButton")),{{"action","ui_confirm_cancel"}},false,{},{}});
}
void InventoryUI::native_dialog_close() {
    for(auto& listener:frozen_listeners_) if(auto* l=listener.Get()) { try { invoke(l,L"SetEnabledState",L"bEnabled",true); } catch(...) {} }
    frozen_listeners_.clear();
    if(auto* dialog=dialog_.Get()) { try { invoke(dialog,L"RemoveFromParent"); } catch(...) {} }
    dialog_.Reset(); dialog_primary_.Reset(); dialog_secondary_.Reset(); dialog_shown_.clear(); dialog_focus_shown_=-1;
}
}

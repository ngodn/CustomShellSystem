namespace css {
namespace {
// One visual vocabulary for both layouts and every extension. Coordinates use
// the native Inventory canvas at a reference height of 1080.
struct ExtensionKit : InventoryLayout {
    static constexpr Color ink{.48f,.43f,.34f,1};
    static constexpr Color secondary{.31f,.275f,.225f,1};
    static constexpr Color gold{.42f,.34f,.22f,1};
    static constexpr Color disabled{.17f,.15f,.125f,1};
    static constexpr double row_height=66, inset=20;
    void overflow(UObject* widget,bool wrap) {
        invoke(widget,L"SetAutoWrapText",L"InAutoTextWrap",wrap);
        invoke(widget,L"SetClipping",L"InClipping",uint8_t{1});
        invoke(widget,L"SetTextOverflowPolicy",L"InOverflowPolicy",uint8_t{1});
    }
    UObject* label(const std::string& value,double x,double y,double w,double h,float size,Color color=ink) {
        auto* widget=InventoryLayout::label(value,x,y,w,h,size,color);overflow(widget,h>45);return widget;
    }
    UObject* button(const std::string& value,double x,double y,double w,double h,bool active=false,bool enabled=true,float size=20) {
        auto* widget=InventoryLayout::button(value,x,y,w,h,active,enabled,size);
        Call child(widget,L"GetContent",1);child.run();overflow(child.get<UObject*>(),false);return widget;
    }
    UObject* text(const std::string& value,double x,double y,double w,double h,float size=20,Color color=ink) {
        auto* widget=InventoryLayout::label(value,x,y,w,h,size,color);
        font_size(widget,size*float(scale),serif);overflow(widget,h>45);
        return widget;
    }
    UObject* decoration(const char* name,double x,double y,double w,double h) {
        std::string path="/Game/Sparta/UI/Common/Textures/";path+=name;path+=".";path+=name;
        return image(load(path),x,y,w,h);
    }
    void divider(double x,double y,double w) {decoration("T_UI_DescriptionHeader_Divider",x,y,w,2);}
    void panel(double x,double y,double w,double h) {
        box(x,y,w,h,Color{.009f,.008f,.006f,.72f});divider(x+inset,y,w-2*inset);
    }
    UObject* row(const Json& c,double x,double y,double w,bool selected) {
        const bool enabled=extensions::interactive(c);
        auto* hit=button("",x,y,w,row_height-2,selected,true);
        if(selected) {
            box(x,y,w,row_height-2,Color{.07f,.056f,.033f,.66f});
            box(x,y+10,2,row_height-22,gold);
        }
        text(c.at("label").get<std::string>(),x+inset,y+16,w*.64-inset,34,21,enabled?ink:secondary);
        auto* value=text(extensions::display_value(c),x+w*.66,y+17,w*.34-inset,32,19,enabled?gold:secondary);
        invoke(value,L"SetJustification",L"InJustification",uint8_t{2});
        return hit;
    }
    UObject* description(const std::string& value,double x,double y,double w,double h) {
        auto* scroll=construct(L"/Script/UMG.ScrollBox",tree);
        invoke(scroll,L"SetAllowOverscroll",L"NewAllowOverscroll",false);
        invoke(scroll,L"SetAnimateWheelScrolling",L"bShouldAnimateWheelScrolling",true);
        invoke(scroll,L"SetScrollbarThickness",L"NewScrollbarThickness",Vec2{3*scale,3*scale});
        place(scroll,x,y,w,h);
        auto* label=construct(L"/Script/UMG.TextBlock",tree);
        text_value(label,value);font_size(label,21*float(scale),serif);
        invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{secondary});
        invoke(label,L"SetAutoWrapText",L"InAutoTextWrap",true);
        // WrapTextAt exists in this cook, but its setter is not reflected.
        inventory_value(label,L"WrapTextAt",float((w-14)*scale));
        invoke(label,L"SetVisibility",L"InVisibility",uint8_t{3});
        Call add(scroll,L"AddChild",2);add.set(L"content",label);add.run();
        return scroll;
    }
    UObject* text_input(const std::string& value,double x,double y,double w,bool enabled) {
        auto* input=construct(L"/Script/UMG.EditableText",tree);
        text_value(input,value);
        // EditableText stores its font in a different layout from TextBlock.
        // Use reflected accessors, as the native CSS Templates editor does.
        Call current(input,L"GetFont",1);current.run();
        Call set(input,L"SetFont",1);set.copy(L"InFontInfo",current,L"ReturnValue");
        auto* font=set.param(L"InFontInfo");auto* info=find(L"/Script/SlateCore.SlateFontInfo");
        member(set.data(font),font->GetElementSize(),info,L"FontObject",serif);
        member(set.data(font),font->GetElementSize(),info,L"Size",20*float(scale));
        member(set.data(font),font->GetElementSize(),info,L"TypefaceFontName",FName(L"Regular"));set.run();
        invoke(input,L"SetIsEnabled",L"bInIsEnabled",enabled);place(input,x,y,w,40);
        return input;
    }
    UObject* progress(double x,double y,double w,double value,bool loading) {
        box(x,y,w,5,Color{.06f,.05f,.035f,1});
        return box(x,y,std::max(2.,w*(loading?.3:std::clamp(value,0.,1.))),5,gold);
    }
    void selection_mark(double x,double y,bool selected) {
        auto* frame=box(x,y,12,12,secondary);invoke(frame,L"SetRenderTransformAngle",L"Angle",45.f);
        auto* inner=box(x+2,y+2,8,8,Color{.01f,.008f,.006f,1});invoke(inner,L"SetRenderTransformAngle",L"Angle",45.f);
        if(selected) {auto* dot=box(x+4,y+4,4,4,gold);invoke(dot,L"SetRenderTransformAngle",L"Angle",45.f);}
    }
};
}
}

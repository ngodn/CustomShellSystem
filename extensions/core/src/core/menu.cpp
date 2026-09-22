#include "menu.hpp"
#include "controls.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace cssx {
using namespace engine;
namespace {
// Palette: Mortal Shell's parchment ink on near-black, gold for selection.
constexpr Color ink{.62f,.56f,.45f,1}, bright{.86f,.80f,.68f,1}, muted{.36f,.32f,.26f,1}, gold{.72f,.56f,.28f,1};
constexpr Color danger{.72f,.28f,.22f,1}, warning{.78f,.58f,.22f,1}, good{.42f,.62f,.36f,1};
constexpr Color backdrop{.012f,.010f,.008f,.96f}, panel{.030f,.026f,.020f,.92f}, row_selected{.09f,.072f,.040f,.85f}, line{.16f,.13f,.09f,1};
constexpr double reference_h=1080;
constexpr double strip_top=270, content_top=340, panel_top=80;   // left column: artwork, strip, rows; right panel from the top
constexpr int controls_visible=10;                   // mouse-wheel scroll step bound; the page computes the real count
// Controller prompt glyph ids of the game's WBP_Prompt (E_ControllerButton).
constexpr uint8_t glyph_accept=3, glyph_secondary=4, glyph_back=5, glyph_left_bumper=8, glyph_right_bumper=9, glyph_up=13, glyph_down=14, glyph_left=15, glyph_right=16;
// Keyboard glyph ids of the game's WBP_Prompt (E_KeyboardMouseButton): the
// letters run contiguously from A=33 to Z=58; the rest were read off screen.
const std::vector<std::pair<std::string,uint8_t>> keyboard_icons=[]{
    std::vector<std::pair<std::string,uint8_t>> v{{"LeftMouseButton",0},{"RightMouseButton",1},{"BackSpace",6},{"Tab",7},{"Enter",8},{"Escape",11},{"Spacebar",12},{"Left",17},{"Up",18},{"Right",19},{"Down",20},{"Ctrl",255},{"Home",16}};
    for(char c='A';c<='Z';++c) v.push_back({std::string(1,c),uint8_t(33+(c-'A'))});
    return v; }();
std::string effect_label(const Json& c) {
    const auto effect=c.value("effect",std::string{});
    if(effect=="irreversible") return "Irreversible: this changes your save";
    if(effect=="persistent") return "Persists in your save";
    if(effect=="reversible") return "Reversible while the menu owns it";
    return {};
}
Color severity_color(const Json& c) {
    const auto s=c.value("severity",std::string{});
    return s=="danger"?danger:s=="warning"?warning:gold;
}
struct Modal { double x,y,w,h; };
Modal modal_frame(Layout& ui,const std::string& title,double width,double height) {
    const double w=std::min(920.,width-160.),x=(width-w)/2,y=(reference_h-height)/2;
    ui.box(0,0,width,reference_h,Color{0,0,0,.90f});
    ui.box(x-1,y-1,w+2,height+2,line);
    ui.box(x,y,w,height,Color{.014f,.012f,.009f,1});
    ui.label(title,x+32,y+24,w-64,60,27,bright,true);
    ui.box(x+32,y+92,w-64,1,line);
    ui.box(x+32,y+height-82,w-64,1,line);
    return {x,y,w,height};
}
UObject* scroll_text(Layout& ui,const std::string& value,double x,double y,double w,double h,float size,Color color) {
    auto* scroll=construct(L"/Script/UMG.ScrollBox",ui.tree);
    invoke(scroll,L"SetAllowOverscroll",L"NewAllowOverscroll",false);
    invoke(scroll,L"SetAnimateWheelScrolling",L"bShouldAnimateWheelScrolling",true);
    invoke(scroll,L"SetScrollbarThickness",L"NewScrollbarThickness",Vec2{3*ui.scale,3*ui.scale});
    ui.place(scroll,x,y,w,h);
    auto* label=construct(L"/Script/UMG.TextBlock",ui.tree);
    text_value(label,value); font_size(label,size*float(ui.scale),ui.serif);
    invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{color});
    invoke(label,L"SetAutoWrapText",L"InAutoTextWrap",true);
    raw_value(label,L"WrapTextAt",float((w-14)*ui.scale));
    invoke(label,L"SetVisibility",L"InVisibility",uint8_t{3});
    Call add(scroll,L"AddChild",2); add.set(L"content",label); add.run();
    return scroll;
}
UObject* text_input(Layout& ui,const std::string& value,double x,double y,double w,bool enabled) {
    auto* input=construct(L"/Script/UMG.EditableText",ui.tree);
    text_value(input,value);
    Call current(input,L"GetFont",1); current.run();
    Call set(input,L"SetFont",1); set.copy(L"InFontInfo",current,L"ReturnValue");
    auto* font=set.param(L"InFontInfo"); auto* info=find(L"/Script/SlateCore.SlateFontInfo");
    member(set.data(font),font->GetElementSize(),info,L"FontObject",ui.serif);
    member(set.data(font),font->GetElementSize(),info,L"Size",20*float(ui.scale));
    member(set.data(font),font->GetElementSize(),info,L"TypefaceFontName",FName(L"Regular")); set.run();
    invoke(input,L"SetIsEnabled",L"bInIsEnabled",enabled); ui.place(input,x,y,w,40);
    return input;
}
void mark(Layout& ui,double x,double y,bool selected) {
    auto* frame=ui.box(x,y,12,12,muted); invoke(frame,L"SetRenderTransformAngle",L"Angle",45.f);
    auto* inner=ui.box(x+2,y+2,8,8,Color{.01f,.008f,.006f,1}); invoke(inner,L"SetRenderTransformAngle",L"Angle",45.f);
    if(selected) { auto* dot=ui.box(x+4,y+4,4,4,gold); invoke(dot,L"SetRenderTransformAngle",L"Angle",45.f); }
}
}
namespace {
UObject* nav_children_refresh(UObject* tabs) { if(auto* nav=object_of(tabs,L"NavigationObject")) invoke(nav,L"GetNavigableChildren"); return tabs; }
void reorder(UObject* panel,const std::vector<UObject*>& desired) {
    // Re-add children in the desired order keeping each HorizontalBoxSlot's layout.
    struct Slot { UObject* child; Margin padding; std::array<std::byte,8> size; uint8_t horizontal,vertical; };
    std::vector<Slot> slots;
    for(auto* child:desired) if(auto* slot=object_of(child,L"Slot"); slot && slot->GetClassPrivate()->GetName()==L"HorizontalBoxSlot")
        slots.push_back({child,read<Margin>(slot,L"Padding"),read<std::array<std::byte,8>>(slot,L"Size"),read<uint8_t>(slot,L"HorizontalAlignment"),read<uint8_t>(slot,L"VerticalAlignment")});
    std::vector<UObject*> rooted; for(auto* child:desired) if(!child->IsRootSet()) { child->SetRootSet(); rooted.push_back(child); }
    try {
        invoke(panel,L"ClearChildren");
        for(auto* child:desired) { Call add(panel,L"AddChild",2); add.set(L"content",child); add.run(); }
        for(const auto& saved:slots) if(auto* slot=object_of(saved.child,L"Slot")) {
            invoke(slot,L"SetPadding",L"InPadding",saved.padding);
            invoke(slot,L"SetSize",L"InSize",saved.size);
            invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",saved.horizontal);
            invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",saved.vertical);
        }
    } catch(...) { for(auto* child:rooted) child->ClearRootSet(); throw; }
    for(auto* child:rooted) child->ClearRootSet();
    if(children(panel)!=desired) throw std::runtime_error("Player Menu child ordering did not apply");
}
}
void Menu::navigate(int index) {
    auto* tabs=tabs_.Get(); if(!tabs) return;
    Call nav(tabs,L"NavigateToCustomIndex",3); nav.set(L"Index",int32_t{index}); nav.run();
    if(!nav.get<bool>(L"Success")) throw std::runtime_error("Player Menu tab navigation rejected the index");
}
bool Menu::attach(const PlayerContext& player) {
    auto* handler=object_of(player.pc,L"User Interface Handler Component");
    auto* game=object_of(handler,L"WBP_Menu_Game");
    auto* main=object_of(game,L"WBP_Menu_Main");
    if(!main || !bool_of(main,L"bOpen")) return false;
    auto* tabs=object_of(main,L"BP_HBC_Menu_Game"); auto* pages=object_of(main,L"BP_WS_Menu_Game"); auto* original=object_of(main,L"WBP_NBM_Inventory");
    if(!tabs || !pages || !original) throw std::runtime_error("Player Menu layout is unavailable");
    const auto page_list=children(pages); const auto tab_list=children(tabs);
    // CSS adds its own tab first (it needs exactly three pages when it attaches
    // and orders four). Wait for it when CSS is installed; give up waiting
    // after two seconds so a broken CSS never hides CSSX.
    const auto now=GetTickCount64();
    const size_t expected=css_present_?4:3;
    if(page_list.size()!=expected || tab_list.size()!=expected) {
        if(!attach_wait_since_) attach_wait_since_=now;
        if(page_list.size()<3 || page_list.size()>4 || now-attach_wait_since_<2000) return false;
    }
    if(page_list.size()>4) throw std::runtime_error("Player Menu already has "+std::to_string(page_list.size())+" pages; another mod owns the extra tab");
    attach_wait_since_=0;
    auto* tab=create_widget(player.pc,original->GetClassPrivate());
    for(auto name:{L"FontData",L"RootSize",L"RootScale",L"DefaultColor",L"SelectedColor",L"bUseHighlight",L"HighlightY"}) {
        auto* p=tab->GetPropertyByNameInChain(name); auto* q=original->GetPropertyByNameInChain(name);
        if(!p || !q || !p->SameType(q)) throw std::runtime_error("Player Menu tab style mismatch");
        p->CopyCompleteValue(reinterpret_cast<std::byte*>(tab)+p->GetOffset_Internal(),reinterpret_cast<std::byte*>(original)+q->GetOffset_Internal());
    }
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_StringToText",2);
    convert.set(L"InString",FString(L"CSSX")); convert.run();
    auto* text=tab->GetPropertyByNameInChain(L"Text");
    if(!text || !text->SameType(convert.param(L"ReturnValue"))) throw std::runtime_error("Player Menu tab title mismatch");
    text->CopyCompleteValue(reinterpret_cast<std::byte*>(tab)+text->GetOffset_Internal(),convert.data(convert.param(L"ReturnValue")));
    invoke(tab,L"UpdateText"); invoke(tab,L"CommitSize"); invoke(tab,L"CommitScale");
    auto* page=create_widget(player.pc,static_cast<UClass*>(main->GetClassPrivate()->GetSuperStruct()));
    auto* tree=object_of(page,L"WidgetTree"); if(!tree) throw std::runtime_error("CSSX page has no widget tree");
    auto* canvas=construct(L"/Script/UMG.CanvasPanel",tree); object_property(tree,L"RootWidget",canvas);
    { Call add(pages,L"AddChild",2); add.set(L"content",page); add.run(); }
    { Call add(tabs,L"AddChildToHorizontalBox",2); add.set(L"content",tab); add.run(); }
    pc_=player.pc; handler_=handler; main_=main; tabs_=tabs; switcher_=pages; page_=page; tab_=tab; tree_=tree; canvas_=canvas;
    order_tabs();
    if(deps_.log) deps_.log("CSSX tab attached to the Player Menu at index "+std::to_string(tab_index_));
    return true;
}
void Menu::order_tabs() {
    auto tabs=children(tabs_.Get()); auto pages=children(switcher_.Get());
    if(tabs.size()!=pages.size() || tabs.size()<4 || tabs.size()>5) throw std::runtime_error("Player Menu tab count is unexpected");
    // Inventory, [CSS], CSSX, Tarstones, Map. CSS puts itself at 1; we go after it.
    const int index=int(tabs.size())==5?2:1;
    auto move_to=[&](auto& values,UObject* value){ auto it=std::find(values.begin(),values.end(),value); if(it==values.end()) throw std::runtime_error("CSSX child is missing"); values.erase(it); values.insert(values.begin()+index,value); };
    move_to(tabs,tab_.Get()); move_to(pages,page_.Get());
    reorder(switcher_.Get(),pages); reorder(tabs_.Get(),tabs);
    // Share the original top bar spacing across the extra title(s), as CSS does.
    const float factor=tabs.size()==5?.6f:.75f;
    std::array<float,4> reference{80,0,80,0};
    for(auto* child:tabs) if(child!=tab_.Get()) if(auto* slot=object_of(child,L"Slot")) { reference=read<std::array<float,4>>(slot,L"Padding"); break; }
    if(auto* slot=object_of(tab_.Get(),L"Slot")) {
        auto padding=reference; padding[0]*=factor; padding[2]*=factor;
        invoke(slot,L"SetPadding",L"InPadding",padding);
        invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{2});
        invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
    }
    nav_children_refresh(tabs_.Get());
    tab_index_=index;
}
bool Menu::open(const PlayerContext& player,std::string* reason) {
    auto fail=[&](const std::string& why){ if(reason) *reason=why; return false; };
    if(!player.pc) return fail("No player controller yet");
    auto* handler=object_of(player.pc,L"User Interface Handler Component");
    if(!handler) return fail("The game's UI handler component is unavailable");
    if(active_) return true;
    auto* main=object_of(object_of(handler,L"WBP_Menu_Game"),L"WBP_Menu_Main");
    if(main && bool_of(main,L"bOpen")) { if(page_.Get()) { navigate(tab_index_); return true; } return fail("Player Menu is open but the CSSX tab is not attached yet"); }
    // Open the Player Menu through the game's own handler, then select the
    // CSSX tab once it is attached (see tick).
    Call open_call(handler,L"HandleGameMenu",2); open_call.set(L"SubTabIndex",int32_t{0}); open_call.set(L"AllowClose",false); open_call.run();
    open_requested_=true; open_requested_at_=GetTickCount64();
    return true;
}
void Menu::close() {
    if(!main_.Get() || !bool_of(main_.Get(),L"bOpen")) return;
    auto* handler=handler_.Get(); if(!handler) return;
    Call close_call(handler,L"HandleGameMenu",2); close_call.set(L"SubTabIndex",int32_t{0}); close_call.set(L"AllowClose",true); close_call.run();
    active_=was_active_=false;
}
void Menu::forget() {
    page_.Reset(); tab_.Reset(); tree_.Reset(); canvas_.Reset(); main_.Reset(); tabs_.Reset(); switcher_.Reset(); handler_.Reset(); pc_.Reset(); prompt_.Reset();
    hits_.clear(); sliders_.clear(); bindings_.clear(); textures_.clear();
    search_input_.Reset(); search_results_.Reset(); search_count_.Reset(); description_.Reset(); name_input_.Reset();
    active_=was_active_=false; tab_index_=-1; attach_wait_since_=0;
}
void Menu::detach() {
    if(auto* tabs=tabs_.Get(); tabs && tab_.Get() && main_.Get() && active_) { try { navigate(0); } catch(...) {} }
    if(auto* page=page_.Get()) { try { invoke(page,L"RemoveFromParent"); } catch(...) {} }
    if(auto* tab=tab_.Get()) { try { invoke(tab,L"RemoveFromParent"); } catch(...) {} }
    if(auto* tabs=tabs_.Get()) { try { nav_children_refresh(tabs); } catch(...) {} }
    forget();
}
void Menu::bind_inputs() {
    bindings_.clear();
    auto* pc=pc_.Get(); auto* handler=handler_.Get();
    auto* mapping=object_of(handler,L"InputMapping");
    auto* p=mapping?mapping->GetPropertyByNameInChain(L"Mappings"):nullptr;
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Menu input mapping is unavailable");
    Call subsystem(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetLocalPlayerSubSystemFromPlayerController",3);
    subsystem.set(L"PlayerController",pc); subsystem.set(L"Class",static_cast<UClass*>(find(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem"))); subsystem.run();
    auto* input=subsystem.get<UObject*>();
    if(!input) throw std::runtime_error("Player input subsystem is unavailable");
    const std::map<std::wstring,std::string> actions={
        {L"IA_Menu_Up","up"},{L"IA_Menu_Down","down"},{L"IA_Menu_Left_Primary","left"},{L"IA_Menu_Right_Primary","right"},
        {L"IA_Menu_Left_Tertiary","previous_section"},{L"IA_Menu_Right_Tertiary","next_section"},
        {L"IA_Menu_Confirm_Primary_Press","accept"},{L"IA_Menu_Confirm_Secondary_Press","secondary"},{L"IA_Menu_Back","close"}};
    auto* a=static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(mapping)+p->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>256) throw std::runtime_error("Input map exceeds bound");
    auto* mapping_struct=find(L"/Script/EnhancedInput.EnhancedActionKeyMapping");
    auto* ap=field(mapping_struct,L"Action",8);
    auto* key_field=mapping_struct->GetPropertyByNameInChain(L"Key");
    auto* kn=field(find(L"/Script/InputCore.Key"),L"KeyName",sizeof(FName));
    if(!key_field || key_field->GetOffset_Internal()<0 || kn->GetOffset_Internal()+int(sizeof(FName))>key_field->GetElementSize()) throw std::runtime_error("Input mapping key layout mismatch");
    const int element=a->GetInner()->GetElementSize();
    if(ap->GetOffset_Internal()+8>element || key_field->GetOffset_Internal()+key_field->GetElementSize()>element) throw std::runtime_error("Input mapping element layout mismatch");
    auto usable=[](const std::string& name){ return !(name=="Gamepad_LeftX" || name=="Gamepad_LeftY" || name=="Gamepad_RightX" || name=="Gamepad_RightY"); };
    std::map<UObject*,size_t> index;
    // Pass 1: the keys the context itself declares (covers every action even
    // before Enhanced Input has rebuilt the player's applied mappings).
    for(int i=0;i<values.Num();++i) {
        UObject* action{}; std::memcpy(&action,values.GetRawPtr(i)+ap->GetOffset_Internal(),8);
        if(!action) continue;
        const auto found=actions.find(action->GetName());
        if(found==actions.end()) continue;
        auto it=index.find(action);
        if(it==index.end()) { Binding binding; binding.input_action=action; binding.action=found->second; bindings_.push_back(std::move(binding)); it=index.emplace(action,bindings_.size()-1).first; }
        FName key{}; std::memcpy(&key,values.GetRawPtr(i)+key_field->GetOffset_Internal()+kn->GetOffset_Internal(),sizeof(key));
        auto name=narrow(key.ToString());
        auto& keys=bindings_[it->second].keys;
        if(usable(name) && !name.empty() && name!="None" && std::find(keys.begin(),keys.end(),name)==keys.end()) keys.push_back(name);
    }
    // The page has no character preview, so the left stick can navigate too.
    static const std::map<std::string,std::vector<std::string>> stick={{"up",{"Gamepad_LeftStick_Up"}},{"down",{"Gamepad_LeftStick_Down"}},{"left",{"Gamepad_LeftStick_Left"}},{"right",{"Gamepad_LeftStick_Right"}}};
    for(auto& binding:bindings_) if(auto extra=stick.find(binding.action); extra!=stick.end()) for(const auto& k:extra->second) if(std::find(binding.keys.begin(),binding.keys.end(),k)==binding.keys.end()) binding.keys.push_back(k);
    // Pass 2: the player's applied mappings (remaps). When the query answers,
    // it replaces the declared keys for that action.
    for(auto& binding:bindings_) {
        try {
            Call query(input,L"QueryKeysMappedToAction",2); query.set(L"Action",binding.input_action.Get()); query.run();
            auto* out=query.param(L"ReturnValue");
            if(!out->IsA<FArrayProperty>()) continue;
            auto* array=static_cast<FArrayProperty*>(out); FScriptArrayHelper keys(array,query.data(out));
            if(keys.Num()<=0 || keys.Num()>32 || kn->GetOffset_Internal()+8>array->GetInner()->GetElementSize()) continue;
            std::vector<std::string> applied;
            for(int n=0;n<keys.Num();++n) { FName key{}; std::memcpy(&key,keys.GetRawPtr(n)+kn->GetOffset_Internal(),sizeof(key)); auto name=narrow(key.ToString()); if(usable(name)) applied.push_back(name); }
            if(!applied.empty()) { for(const auto& k:binding.keys) if(k.starts_with("Gamepad_LeftStick_") && std::find(applied.begin(),applied.end(),k)==applied.end()) applied.push_back(k); binding.keys=std::move(applied); }
        } catch(...) {}
    }
    if(bindings_.empty()) throw std::runtime_error("No menu navigation actions were found in the input mapping");
}
void Menu::poll_input(const PlayerContext& player,uint64_t now) {
    auto* pc=player.pc; if(!pc) return;
    // Each key test is a reflected call; only the keys of the device in use
    // are tested (the game's prompt widget reports the device), which halves
    // the per-frame cost while the page is open. A press on the other device
    // flips the prompt within a frame, so nothing is lost.
    for(auto& b:bindings_) {
        bool down=false;
        for(const auto& name:b.keys) {
            const bool pad=name.starts_with("Gamepad_");
            if(pad!=gamepad_ && prompt_.Get()) continue;
            Call call(pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
            member(call.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(wide(name).c_str()));
            call.run(); if(call.get<bool>()) { down=true; break; }
        }
        const bool repeating=b.action=="up" || b.action=="down" || b.action=="left" || b.action=="right";
        if(down && (!b.down || (repeating && now>=b.repeat))) {
            b.repeat=now+(b.down?90:400);
            b.down=true;
            key(b.action);
            return;   // one action per frame keeps navigation predictable
        }
        if(!down) b.down=false;
    }
}
void Menu::poll_mouse(const PlayerContext& player) {
    auto* pc=player.pc; if(!pc || !player.world) return;
    Call pos(find(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetMousePositionOnViewport",2);
    pos.set(L"WorldContextObject",player.world); pos.run();
    const auto m=pos.get<Vec2>();
    const bool moved=std::abs(m.x-mouse_[0])>0.5 || std::abs(m.y-mouse_[1])>0.5;
    mouse_={m.x,m.y};
    Call left(pc,L"IsInputKeyDown",2); auto* p=left.param(L"Key");
    member(left.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(L"LeftMouseButton"));
    left.run(); const bool down=left.get<bool>();
    const bool pressed=down && !mouse_left_; mouse_left_=down;
    if(!moved && !pressed && !down) return;
    // Sliders: preview while dragging, commit on release.
    for(auto& s:sliders_) {
        auto* widget=s.widget.Get(); if(!widget) continue;
        Call value(widget,L"GetValue",1); value.run(); const float v=value.get<float>();
        if(std::abs(v-s.previous)>1e-6f) {
            s.previous=v;
            if(auto* label=s.label.Get()) text_value(label,display_value([&]{ auto c=s.control; c["value"]=snap_value(s.control,v); return c; }()));
            if(!down) act({{"action","value"},{"value",snap_value(s.control,v)}});
        }
    }
    if(!pressed) return;
    for(const auto& hit:hits_) {
        auto* widget=hit.widget.Get(); if(!widget) continue;
        Call hovered(widget,L"IsHovered",1); hovered.run();
        if(hovered.get<bool>()) { act(hit.action); return; }
    }
}
void Menu::tick(const PlayerContext& player,double) {
    const auto now=GetTickCount64();
    // Attached to a menu instance that went away (travel, new controller): forget it.
    if(page_.Get() && (player.pc!=pc_.Get() || !main_.Get() || !handler_.Get())) { if(deps_.log) deps_.log("CSSX tab dropped with its Player Menu instance"); forget(); }
    if(!page_.Get()) {
        if(now<discover_after_) return; discover_after_=now+250;
        if(!player.pc) return;
        try { if(!attach(player)) return; } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("CSSX tab attach failed: ")+e.what()); discover_after_=now+2000; return; }
    }
    auto* main=main_.Get(); auto* switcher=switcher_.Get(); if(!main || !switcher) { forget(); return; }
    const bool menu_open=bool_of(main,L"bOpen");
    if(open_requested_ && menu_open) { open_requested_=false; try { navigate(tab_index_); } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Could not select the CSSX tab: ")+e.what()); } }
    if(open_requested_ && now-open_requested_at_>3000) open_requested_=false;
    if(!menu_open) { active_=false; if(was_active_) { was_active_=false; } return; }
    Call selected(switcher,L"GetActiveWidget",1); selected.run();
    active_=selected.get<UObject*>()==page_.Get();
    if(active_ && !was_active_) {
        bindings_ready_=false; bind_retry_=0;
        try { bind_inputs(); } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Menu input binding failed: ")+e.what()); }
        for(auto& b:bindings_) { b.down=true; b.repeat=now+400; }
        mouse_left_=true; dirty_=true; enter_=true; error_.clear(); confirm_=nullptr; details_=picker_=false;
        refresh_library(true);
    }
    if(!active_ && was_active_) { hits_.clear(); sliders_.clear(); }
    was_active_=active_;
    if(!active_) return;
    // Enhanced Input rebuilds its key mappings a tick after the game adds the
    // menu context, so the first query can come back empty. Retry until keys
    // appear, then redraw the hints with the real glyphs.
    if(!bindings_ready_ && now>=bind_retry_) {
        bind_retry_=now+200;
        try {
            bind_inputs();
            bindings_ready_=std::any_of(bindings_.begin(),bindings_.end(),[](const Binding& b){ return !b.keys.empty(); });
            for(auto& b:bindings_) { b.down=true; b.repeat=now+400; }
            if(bindings_ready_) dirty_=true;
        } catch(...) {}
    }
    if(now>=layout_check_) {
        layout_check_=now+500;
        Call geometry(switcher,L"GetCachedGeometry",1); geometry.run();
        Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2); size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        const auto extent=size.get<Vec2>();
        if(std::abs(extent.x-viewport_[0])>.5 || std::abs(extent.y-viewport_[1])>.5) { viewport_={extent.x,extent.y}; dirty_=true; }
    }
    if(auto* prompt=prompt_.Get()) { try { const bool gamepad=read<uint8_t>(prompt,L"InputType")==1; if(gamepad!=gamepad_) { gamepad_=gamepad; dirty_=true; } } catch(...) {} }
    if(now>=library_check_) { library_check_=now+250; refresh_library(false); }
    if(picker_) if(auto* search=search_input_.Get()) {
        try { const auto query=text_of(search,256); if(query!=search_query_) { search_query_=query; if(options_.filter(query)) build_results(); } }
        catch(const std::exception& e) { error_=e.what(); }
    }
    try { poll_input(player,now); poll_mouse(player); }
    catch(const std::exception& e) { error_=e.what(); dirty_=true; }
    if(!active_) return;
    if(dirty_) { try { build(); } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Menu build failed: ")+e.what()); error_=e.what(); dirty_=false; } }
}
void Menu::refresh_library(bool force) {
    if(!deps_.runtime) { library_={{"revision",0},{"extensions",Json::array()},{"errors",Json::array()}}; return; }
    auto library=deps_.runtime->library();
    const auto revision=library.value("revision",uint64_t{});
    if(force || revision!=library_revision_) { library_revision_=revision; library_=std::move(library); if(screen_==Screen::Extension) refresh_model(); dirty_=true; }
    else if(screen_==Screen::Library) { library_=std::move(library); }   // status summaries may change without a revision bump
}
void Menu::refresh_model() {
    if(!deps_.runtime || extension_id_.empty()) { model_=nullptr; return; }
    try { model_=deps_.runtime->model(extension_id_); }
    catch(const std::exception& e) { model_=nullptr; error_=e.what(); }
}
const Json* Menu::current_control() const {
    if(screen_!=Screen::Extension || !model_.is_object() || !model_.contains("sections")) return nullptr;
    const auto& sections=model_["sections"]; if(section_<0 || section_>=int(sections.size())) return nullptr;
    const auto& controls=sections[section_]["controls"]; if(row_<0 || row_>=int(controls.size())) return nullptr;
    return &controls[row_];
}
void Menu::send_event(const Json& event) {
    try { deps_.runtime->event(extension_id_,event); error_.clear(); }
    catch(const std::exception& e) { error_=e.what(); }
    refresh_library(true); refresh_model(); dirty_=true;
}
void Menu::key(const std::string& action) {
    if(picker_) {
        if(action=="close") act({{"action","pick_cancel"}});
        else if(action=="up" || action=="down" || action=="previous_section" || action=="next_section") { options_.move(action=="up"?-1:action=="down"?1:action=="previous_section"?-8:8); build_results(); }
        else if(action=="accept") act({{"action","pick_apply"}});
        return;
    }
    if(!confirm_.is_null()) { if(action=="accept") act({{"action","confirm"}}); else if(action=="close") act({{"action","cancel"}}); return; }
    if(details_) {
        if(action=="close" || action=="secondary") act({{"action","details"}});
        else if((action=="up" || action=="down")) if(auto* d=description_.Get()) { Call offset(d,L"GetScrollOffset",1); offset.run(); invoke(d,L"SetScrollOffset",L"NewScrollOffset",std::max(0.f,offset.get<float>()+(action=="up"?-60.f:60.f))); }
        return;
    }
    if(screen_==Screen::Library) {
        const int count=int(library_["extensions"].size());
        if(action=="up" || action=="down") { library_row_=std::clamp(library_row_+(action=="up"?-1:1),0,std::max(0,count-1)); dirty_=true; }
        else if(action=="accept") act({{"action","open"},{"row",library_row_}});
        else if(action=="previous_section" || action=="next_section") act({{"action","framework_tab"},{"section",1}});
        else if(action=="close") close();   // like CSS: the page closes the Player Menu itself
        return;
    }
    if(screen_==Screen::Settings) {
        if(action=="close") close();
        else if(action=="previous_section" || action=="next_section") act({{"action","framework_tab"},{"section",0}});
        else if(action=="up" || action=="down") { settings_row_=std::clamp(settings_row_+(action=="up"?-1:1),0,3); dirty_=true; }
        else if(action=="left" || action=="right" || action=="accept") act({{"action","settings_adjust"},{"delta",action=="left"?-1:1}});
        return;
    }
    // Extension screen
    if(action=="close") { act({{"action","library"}}); return; }
    if(action=="secondary") { act({{"action","details"}}); return; }
    if(action=="previous_section" || action=="next_section") { act({{"action","section_delta"},{"delta",action=="previous_section"?-1:1}}); return; }
    if(action=="up" || action=="down") { act({{"action","row_delta"},{"delta",action=="up"?-1:1}}); return; }
    if(action=="left" || action=="right") { act({{"action","adjust"},{"delta",action=="left"?-1:1}}); return; }
    if(action=="accept") act({{"action","activate"}});
}
void Menu::act(const Json& action) {
    const auto name=action.value("action",std::string{});
    if(name!="value") error_.clear();
    if(picker_) {
        if(name=="pick_cancel" || name=="library") { picker_=false; dirty_=true; return; }
        if(name=="pick_row") { options_.selected=std::min(action.at("row").get<size_t>(),options_.matches.empty()?size_t{}:options_.matches.size()-1); build_results(); return; }
        if(name=="pick_apply") {
            const auto value=options_.value(); if(value.is_null()) return;
            const auto* c=current_control(); if(!c) return;
            Json event={{"id",c->at("id")},{"value",value}};
            picker_=false;
            if(c->contains("confirm")) confirm_={{"event",event},{"message",c->at("confirm")}};
            else send_event(event);
            dirty_=true; return;
        }
        return;
    }
    if(!confirm_.is_null()) {
        if(name=="confirm") { auto event=confirm_.at("event"); event["confirmed"]=true; confirm_=nullptr; send_event(event); }
        else if(name=="cancel") { confirm_=nullptr; dirty_=true; }
        return;
    }
    if(name=="details") { details_=!details_; details_offset_=0; dirty_=true; return; }
    if(details_) return;
    if(name=="library") { screen_=Screen::Library; extension_id_.clear(); model_=nullptr; dirty_=true; enter_=true; return; }
    if(name=="open") {
        const int row=action.value("row",library_row_); library_row_=row;
        const auto& entries=library_["extensions"];
        if(row>=int(entries.size())) return;
        const auto& entry=entries[row];
        if(!entry.value("available",false)) { error_="This extension is unavailable: "+entry.value("error",std::string{}); dirty_=true; return; }
        extension_id_=entry.at("id").get<std::string>(); screen_=Screen::Extension; section_=row_=first_row_=0; confirm_=nullptr;
        refresh_model(); dirty_=true; enter_=true; return;
    }
    if(name=="framework_tab") { screen_=action.value("section",0)==1?Screen::Settings:Screen::Library; extension_id_.clear(); model_=nullptr; dirty_=true; enter_=true; return; }
    if(name=="settings_row" && screen_==Screen::Settings) { settings_row_=std::clamp(action.value("row",0),0,3); dirty_=true; return; }
    if(name=="run" && screen_==Screen::Extension && model_.is_object()) {
        // Activate a control by id from anywhere on the page (notice strip).
        const auto id=action.value("id",std::string{});
        for(const auto& section:model_.value("sections",Json::array())) for(const auto& c:section.value("controls",Json::array())) if(c.value("id",std::string{})==id) {
            if(!interactive(c)) { error_="That action is not available right now."; dirty_=true; return; }
            Json event={{"id",id}};
            if(c.contains("confirm")) { confirm_={{"event",event},{"message",c.at("confirm")}}; dirty_=true; }
            else send_event(event);
            return;
        }
        return;
    }
    if(name=="settings_adjust" && screen_==Screen::Settings) {
        auto& s=*deps_.settings; const int delta=action.value("delta",1);
        switch(settings_row_) {
        case 0: s.ui_scale=std::clamp(std::round((s.ui_scale+delta*0.05)*100)/100,0.75,1.5); break;
        case 1: s.show_extension_status=!s.show_extension_status; break;
        default: break;
        }
        try { if(deps_.save_settings) deps_.save_settings(); } catch(const std::exception& e) { error_=std::string("Settings not saved: ")+e.what(); }
        dirty_=true; return;
    }
    if(screen_!=Screen::Extension || !model_.is_object()) return;
    const auto& sections=model_["sections"]; const int count=int(sections.size());
    if(name=="section" || name=="section_delta") {
        if(count) section_=name=="section"?std::clamp(action.at("section").get<int>(),0,count-1):(section_+action.at("delta").get<int>()+count)%count;
        row_=first_row_=0; dirty_=true; enter_=true; return;
    }
    if(!count) return;
    const auto& controls=sections[section_]["controls"]; const int rows=int(controls.size());
    if(name=="row_delta") { row_=std::clamp(row_+action.at("delta").get<int>(),0,std::max(0,rows-1)); dirty_=true; return; }
    if(name=="row") { row_=std::clamp(action.at("row").get<int>(),0,std::max(0,rows-1)); dirty_=true; return; }
    if(name=="scroll") { first_row_=std::clamp(first_row_+action.at("delta").get<int>(),0,std::max(0,rows-controls_visible)); row_=std::clamp(row_,first_row_,first_row_+controls_visible-1); dirty_=true; return; }
    const auto* c=current_control(); if(!c || !interactive(*c)) return;
    const auto type=c->at("type").get<std::string>();
    if(type=="choice" && (name=="pick" || name=="activate")) {
        options_.reset(c->at("options"),[](const std::string& text){
            const auto source=wide(text);
            const int length=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,source.data(),int(source.size()),nullptr,0,nullptr,nullptr,0);
            if(length<=0) return OptionSearch::ascii_fold(text);
            std::wstring result(size_t(length),L'\0');
            if(!LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,source.data(),int(source.size()),result.data(),length,nullptr,nullptr,0)) return OptionSearch::ascii_fold(text);
            return narrow(result);
        });
        for(size_t i=0;i<options_.matches.size();++i) if(options_.options[i].at("id")==c->at("value")) options_.selected=i;
        search_query_.clear(); picker_=true; dirty_=true; return;
    }
    Json event={{"id",c->at("id")}};
    if(name=="value" || name=="text") {
        if(name=="text" && type=="text") event["value"]=text_of(name_input_.Get(),4096);
        else if(name=="value" && (type=="radio" || type=="slider")) event["value"]=type=="slider"?Json(snap_value(*c,action.at("value").get<double>())):action.at("value");
        else return;
    } else if(name=="activate" || name=="adjust") {
        if(type=="toggle" && name=="activate") event["value"]=!c->at("value").get<bool>();
        else if(adjustable(*c)) event["value"]=adjusted_value(*c,action.value("delta",1));
        else if(type!="button" || name!="activate") return;
    } else return;
    if(c->contains("confirm")) { confirm_={{"event",event},{"message",c->at("confirm")}}; dirty_=true; return; }
    send_event(event);
}
bool Menu::texture(Layout& ui,const std::string& file,double x,double y,double w,double h) {
    if(file.empty()) return false; auto path=utf8_path(file); std::error_code ec; if(!fs::exists(path,ec)) return false;
    auto& saved=textures_[file]; auto* image=saved.Get();
    if(!image) {
        Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
        import.set(L"WorldContextObject",pc_.Get()); import.set(L"Filename",FString(path.c_str())); import.run();
        image=import.get<UObject*>(); if(!image) return false; saved=image;
    }
    ui.image(image,x,y,w,h); return true;
}
UObject* Menu::prompt(Layout& ui,const std::string& action,const std::string& text,double x,double y,double w,uint8_t icon) {
    // Until the game's bindings are known the prompt widget would show its
    // default (mouse) glyph, so draw the text alone and let the rebuild that
    // follows binding discovery add the real glyphs.
    if(!bindings_ready_ && !gamepad_) { if(!text.empty()) ui.label(text,x+12,y+2,w-12,30,18,muted); return nullptr; }
    auto* cls=static_cast<UClass*>(load("/Game/Sparta/UI/Core/Navigation/WBP_Prompt.WBP_Prompt_C"));
    auto* widget=create_widget(pc_.Get(),cls);
    for(const auto& binding:bindings_) if(binding.action==action) {
        object_property(widget,L"InputAction",binding.input_action.Get());
        for(auto k:binding.keys) if(!k.starts_with("Gamepad_")) {
            if(k=="SpaceBar") k="Spacebar"; if(k=="LeftControl") k="Ctrl";
            for(const auto& [name,value]:keyboard_icons) if(name==k && value!=255) { raw_value(widget,L"KBMPrompt",value); break; }
            break;
        }
    }
    raw_value(widget,L"ControllerPrompt",icon);
    raw_value(widget,L"PromptSize",Vec2{80,80}); raw_value(widget,L"OverrideControllerSize",Vec2{80,80}); raw_value(widget,L"OverrideKBMSize",Vec2{80,80});
    ui.place(widget,x,y-4,64,36); invoke(widget,L"UpdatePrompt"); invoke(widget,L"UpdatePromptSize");
    invoke(widget,L"SetVisibility",L"InVisibility",uint8_t{3});
    if(!text.empty()) ui.label(text,x+76,y+2,w-76,30,18,muted);
    prompt_=widget;
    return widget;
}
void Menu::frame(Layout& ui,double width,double column_w,const std::string& title,const std::string& subtitle,const std::vector<std::string>& tabs,int selected,const std::string& tab_action) {
    // The CSS layout: artwork top-left, page title beside it, then a tab
    // strip. Everything here stays inside the left column (70..70+column_w)
    // so the right panel can start at the very top.
    const double art_w=std::min(400.,column_w*0.42),art_h=art_w*0.413;
    const bool art=texture(ui,path_utf8(deps_.root/"assets/banner.png"),70,80,art_w,art_h);
    const double tx=art?70+art_w+30:70,tw=70+column_w-tx;
    ui.label(title,tx,116,tw,52,30,bright,true);
    ui.label(subtitle,tx+4,166,tw,28,16,muted);
    const double y=strip_top,left=70,right=70+column_w;
    // Tabs in Trajan capitals. Slate lays the strip out (a horizontal box of
    // auto-sized tabs with fixed padding) so the gaps are exact whatever the
    // font's metrics are. The estimate below only decides how many tabs fit
    // between the two bumper glyphs; hidden ones are reachable by bumper.
    auto glyph_w=[](char c){ switch(c) { case 'I': case 'J': case 'L': case '1': case ' ': return 8.0; case 'M': case 'W': return 20.0;
        case 'A': case 'H': case 'K': case 'N': case 'U': case 'V': case 'X': case 'Y': case 'D': case 'G': case 'O': case 'Q': return 16.0; default: return 14.0; } };
    std::vector<std::string> names; std::vector<double> widths;
    for(const auto& t:tabs) { std::string n=t; for(auto& ch:n) ch=char(std::toupper((unsigned char)ch)); double w=30; for(char c:n) w+=glyph_w(c); names.push_back(n); widths.push_back(w); }
    const bool many=tabs.size()>1;
    const double avail=right-left-(many?2*60:0)-40;
    size_t first=0,last=names.size();
    { double total=0; for(double w:widths) total+=w;
      if(total>avail) {
        size_t lo=size_t(std::clamp(selected,0,int(names.size())-1)),hi=lo+1; double used=widths[lo];
        while(true) {
            bool grew=false;
            if(hi<names.size() && used+widths[hi]<=avail) { used+=widths[hi]; ++hi; grew=true; }
            if(lo>0 && used+widths[lo-1]<=avail) { used+=widths[lo-1]; --lo; grew=true; }
            if(!grew) break;
        }
        first=lo; last=hi;
      } }
    double x=left;
    if(many) { prompt(ui,"previous_section","",x,y+6,40,glyph_left_bumper); x+=60; }
    auto* box=construct(L"/Script/UMG.HorizontalBox",ui.tree);
    ui.place(box,x,y,right-60-x,48);
    auto add_text=[&](const std::string& text,Color color,bool on,const Json* action)->UObject* {
        auto* button=construct(L"/Script/UMG.Button",ui.tree); flat_button(button,on);
        { auto* focusable=button->GetPropertyByNameInChain(L"IsFocusable"); if(focusable && focusable->IsA<FBoolProperty>()) static_cast<FBoolProperty*>(focusable)->SetPropertyValueInContainer(button,false); }
        invoke(button,L"SetBackgroundColor",L"InBackgroundColor",Color{1,1,1,1});
        auto* column=construct(L"/Script/UMG.VerticalBox",ui.tree);
        auto* label=construct(L"/Script/UMG.TextBlock",ui.tree);
        text_value(label,text); font_size(label,15*float(ui.scale),ui.title_font);
        invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{color});
        invoke(label,L"SetVisibility",L"InVisibility",uint8_t{3});
        { Call add(column,L"AddChildToVerticalBox",2); add.set(L"content",label); add.run(); auto* slot=add.get<UObject*>();
          invoke(slot,L"SetPadding",L"InPadding",Margin{float(14*ui.scale),float(9*ui.scale),float(14*ui.scale),float(4*ui.scale)}); }
        auto* underline=construct(L"/Script/UMG.Border",ui.tree);
        invoke(underline,L"SetBrushColor",L"InBrushColor",on?gold:Color{0,0,0,0});
        invoke(underline,L"SetPadding",L"InPadding",Margin{0,float(1*ui.scale),0,float(1*ui.scale)});
        invoke(underline,L"SetVisibility",L"InVisibility",uint8_t{4});
        { Call add(column,L"AddChildToVerticalBox",2); add.set(L"content",underline); add.run(); auto* slot=add.get<UObject*>();
          invoke(slot,L"SetPadding",L"InPadding",Margin{float(8*ui.scale),0,float(8*ui.scale),0}); invoke(slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{3}); }
        { Call add(button,L"AddChild",2); add.set(L"content",column); add.run(); }
        { Call add(box,L"AddChildToHorizontalBox",2); add.set(L"content",button); add.run(); auto* slot=add.get<UObject*>();
          invoke(slot,L"SetPadding",L"InPadding",Margin{0,0,float(4*ui.scale),0}); invoke(slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2}); }
        if(action) hits_.push_back({WeakObject(button),*action,false});
        return button;
    };
    if(first>0) { Json prev={{"action",tab_action},{"section",int(first)-1}}; add_text("...",muted,false,&prev); }
    for(size_t i=first;i<last;++i) { Json act={{"action",tab_action},{"section",int(i)}}; add_text(names[i],int(i)==selected?bright:ink,int(i)==selected,&act); }
    if(last<names.size()) { Json next={{"action",tab_action},{"section",int(last)}}; add_text("...",muted,false,&next); }
    if(many) prompt(ui,"next_section","",right-44,y+6,40,glyph_right_bumper);
    ui.box(left,strip_top+52,column_w,1,line);
}
std::string Menu::perf_line(bool brief) const {
    Json p=deps_.perf?deps_.perf():Json::object();
    if(!p.is_object() || p.value("frames",0)<10) return brief?"Measuring...":"Performance: measuring (needs a few seconds in the world)";
    char text[160];
    if(brief) std::snprintf(text,sizeof text,"%.2f ms per frame",p.value("core_mean_us",0.0)/1000);
    else std::snprintf(text,sizeof text,"CSSX cost %.2f ms of %.1f ms per frame (%.0f fps), peak %.2f ms",p.value("core_mean_us",0.0)/1000,p.value("median_ms",0.0),p.value("hz",0.0),p.value("core_max_us",0.0)/1000);
    return text;
}
std::string Menu::perf_detail() const {
    Json p=deps_.perf?deps_.perf():Json::object();
    if(!p.is_object() || p.value("frames",0)<10) return "Frame cost of CSSX itself over the last ten seconds. It needs a few seconds in the world to measure.";
    char text[400];
    std::snprintf(text,sizeof text,"Over the last ten seconds the game rendered at %.0f fps (median %.1f ms per frame). CSSX, its menu and every extension together took %.3f ms per frame on average and %.3f ms at most (99th percentile %.3f ms). With the Player Menu closed this is the whole cost; nothing else runs per frame.",
        p.value("hz",0.0),p.value("median_ms",0.0),p.value("core_mean_us",0.0)/1000,p.value("core_max_us",0.0)/1000,p.value("core_p99_us",0.0)/1000);
    return text;
}
void Menu::build_footer(Layout& ui,double width,const std::vector<std::pair<std::string,std::string>>& left,const std::vector<std::pair<std::string,std::string>>& right) {
    // Reserved hint band: left = navigation, right = contextual actions. The
    // status line sits above it and never overlaps.
    const double y=reference_h-70;
    ui.box(60,y-14,width-120,1,line);
    struct Glyph { const char* action; uint8_t icon; };
    static const std::map<std::string,uint8_t> icons={{"accept",glyph_accept},{"secondary",glyph_secondary},{"close",glyph_back},{"previous_section",glyph_left_bumper},{"next_section",glyph_right_bumper},{"up",glyph_up},{"down",glyph_down},{"left",glyph_left},{"right",glyph_right}};
    // Glyphs from the game's prompt widget are up to ~64 units wide (keyboard
    // keys such as ESC or Enter), so labels start 76 units in and each item
    // reserves room for its glyph plus text.
    double x=70;
    for(const auto& [action,text]:left) { prompt(ui,action,text,x,y,320,icons.at(action)); x+=text.empty()?72:std::min(340.,100+text.size()*10.5); }
    double rx=width-70;
    for(auto it=right.rbegin();it!=right.rend();++it) { const double w=std::min(360.,100+it->second.size()*10.5); rx-=w; prompt(ui,it->first,it->second,rx,y,w,icons.at(it->first)); rx-=28; }
    const auto status=error_.empty()?(model_.is_object()?model_.value("status",std::string{}):std::string{}):error_;
    if(!status.empty()) ui.label(status,70,y-58,width-140,36,18,error_.empty()?muted:danger);
}
void Menu::build() {
    auto* canvas=canvas_.Get(); auto* tree=tree_.Get(); if(!canvas || !tree) return;
    const auto started=monotonic_us();
    invoke(canvas,L"ClearChildren"); hits_.clear(); sliders_.clear(); search_input_.Reset(); search_results_.Reset(); search_count_.Reset(); description_.Reset(); name_input_.Reset();
    if(viewport_[0]<640 || viewport_[1]<360) {
        Call geometry(switcher_.Get(),L"GetCachedGeometry",1); geometry.run();
        Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2); size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        const auto extent=size.get<Vec2>(); viewport_={extent.x,extent.y};
        if(viewport_[0]<640 || viewport_[1]<360) { dirty_=false; return; }
    }
    // The page canvas is the switcher area under the game's top bar, in
    // slate units. Reference height 1080 maps onto it.
    const double scale=viewport_[1]/reference_h*deps_.settings->ui_scale;
    const double width=viewport_[0]/scale;
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    auto* title=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
    Layout ui{tree,canvas,scale,serif,title};
    ui.box(0,0,width,reference_h,backdrop);
    switch(screen_) {
    case Screen::Library: build_library(ui,width); break;
    case Screen::Extension: build_extension(ui,width); break;
    case Screen::Settings: build_settings(ui,width); break;
    }
    build_modal(ui,width);
    dirty_=false; enter_=false;
    const auto took=monotonic_us()-started;
    ++cost_.builds; cost_.build_us+=took; cost_.last_build_us=took; cost_.widgets=ui.placed.size();
}
void Menu::build_library(Layout& ui,double width) {
    const auto& entries=library_["extensions"];
    const double list_x=70,list_w=std::min(1000.,width*0.50),row_h=66,top=content_top,ptop=panel_top,panel_h=reference_h-128-16-panel_top;
    frame(ui,width,list_w,"Extensions","CSSX "+deps_.version,{"Library","Settings"},0,"framework_tab");
    const int count=int(entries.size());
    library_row_=std::clamp(library_row_,0,std::max(0,count-1));
    const int visible=std::max(3,int((reference_h-128-16-top-20)/row_h));
    const int first=std::clamp(library_row_-visible+1,0,std::max(0,count-visible));
    for(int i=first;i<std::min(first+visible,count);++i) {
        const double y=top+(i-first)*row_h; const bool selected=i==library_row_;
        auto* hit=ui.button("",list_x,y,list_w,row_h-6,selected,true);
        hits_.push_back({WeakObject(hit),{{"action","open"},{"row",i}},false});
        if(selected) { ui.box(list_x,y,list_w,row_h-6,row_selected); ui.box(list_x,y+9,3,row_h-24,gold); }
        {
            const auto& e=entries[i]; const bool available=e.value("available",false);
            ui.label(e.value("title",std::string{}),list_x+24,y+7,list_w*0.62,32,22,available?(selected?bright:ink):muted);
            ui.label("by "+e.value("author",std::string{})+"  /  "+e.value("version",std::string{}),list_x+24,y+37,list_w*0.62,22,14,muted);
            std::string right=available?e.value("status",Json::object()).value("summary",std::string{}):"Unavailable";
            if(!deps_.settings->show_extension_status && available) right.clear();
            const bool active=available && e.value("status",Json::object()).value("active",false);
            ui.label(right,list_x+list_w*0.64,y+15,list_w*0.34,32,17,available?(active?good:muted):danger,false,2);
        }
    }
    if(count>visible) {
        const double track=visible*row_h;
        ui.box(list_x+list_w+8,top,3,track,Color{.05f,.04f,.03f,1});
        ui.box(list_x+list_w+8,top+track*first/count,3,std::max(12.,track*visible/count),gold);
    }
    // Right panel: what the selected entry is, plus framework notices.
    const double px=list_x+list_w+36,pw=width-px-70;
    ui.box(px,ptop,pw,panel_h,panel); ui.box(px+24,ptop,pw-48,1,line);
    if(library_row_<int(entries.size())) {
        const auto& e=entries[library_row_];
        ui.label(e.value("title",std::string{}),px+24,ptop+20,pw-48,44,26,bright,true);
        const auto banner=e.value("banner",std::string{});
        double y=ptop+76;
        if(!banner.empty() && texture(ui,banner,px+24,y,pw-48,(pw-48)*0.34)) y+=(pw-48)*0.34+16;
        auto* d=scroll_text(ui,e.value("description",std::string{}),px+24,y,pw-48,std::max(120.,panel_h-(y-ptop)-150),19,ink); description_=d;
        y=ptop+panel_h-130;
        const auto& err=e.value("error",std::string{});
        if(!err.empty()) ui.label("Unavailable: "+err,px+24,y,pw-48,100,17,danger);
        else {
            ui.label("Extension id  "+e.value("id",std::string{}),px+24,y,pw-48,26,15,muted); y+=26;
            ui.label(std::string("Kind  ")+e.value("kind",std::string{})+"   /   API "+std::to_string(e.value("api",0)),px+24,y,pw-48,26,15,muted); y+=26;
            const auto& cost=e.value("cost",Json::object());
            if(cost.value("tick_calls",uint64_t{})) {
                const double per=double(cost.value("tick_us",uint64_t{}))/double(cost.value("tick_calls",uint64_t{1}));
                char text[96]; std::snprintf(text,sizeof text,"Average tick %.0f us over %llu ticks",per,(unsigned long long)cost.value("tick_calls",uint64_t{}));
                ui.label(text,px+24,y,pw-48,26,15,muted);
            }
        }
    } else {
        ui.label("CSSX "+deps_.version,px+24,ptop+20,pw-48,44,26,bright,true);
        scroll_text(ui,"Custom Shell System Extensions: one menu for every extension. Open it with "+[&]{ std::string s; for(const auto& k:deps_.settings->open_keyboard) s+=(s.empty()?"":" + ")+k; return s; }()+" on the keyboard or "+[&]{ std::string s; for(const auto& k:deps_.settings->open_gamepad) s+=(s.empty()?"":" + ")+k; return s; }()+" on a controller, or switch to this tab in the Player Menu. Extensions install as folders under Mods/CSSX/extensions.",px+24,ptop+76,pw-48,220,19,ink);
        ui.label(perf_line(),px+24,ptop+panel_h-130,pw-48,26,15,muted);
        ui.label("Settings tab: scale, status, open keys, performance",px+24,ptop+panel_h-104,pw-48,26,15,muted);
    }
    Json notice=deps_.notice?deps_.notice():Json::object();
    const auto text=notice.value("notice",std::string{});
    if(!text.empty()) { ui.box(px+24,ptop+panel_h-72,pw-48,1,line); ui.label(text,px+24,ptop+panel_h-64,pw-48,56,15,warning); }
    else if(!library_["errors"].empty()) { ui.box(px+24,ptop+panel_h-72,pw-48,1,line); ui.label(std::to_string(library_["errors"].size())+" extension folder(s) could not load. See logs/cssx.jsonl.",px+24,ptop+panel_h-64,pw-48,56,15,warning); }
    if(entries.empty()) ui.label("No extensions installed. Add extension folders under Mods/CSSX/extensions.",list_x,top+visible*row_h+12,list_w,40,18,muted);
    build_footer(ui,width,{{"up",""},{"down","Browse"},{"previous_section",""},{"next_section","Settings"},{"close","Close"}},{{"accept","Open"}});
}
void Menu::build_settings(Layout& ui,double width) {
    const double x=70,w=std::min(1000.,width*0.50),row_h=62,top=content_top,ptop=panel_top,panel_h=reference_h-128-16-panel_top;
    frame(ui,width,w,"Extensions","CSSX "+deps_.version,{"Library","Settings"},1,"framework_tab");
    const auto& s=*deps_.settings;
    struct Row { std::string label,value,hint; };
    char scale[16]; std::snprintf(scale,sizeof scale,"%.0f%%",s.ui_scale*100);
    // The Player Menu owns pause, cursor and HUD, so only these remain.
    const std::vector<Row> rows={
        {"Menu scale",scale,"Size of this page relative to the 1080p layout. 75% to 150%."},
        {"Show extension status in the library",s.show_extension_status?"On":"Off","Extensions can report a one-line status, for example active cheats."},
        {"Open keys",[&]{ auto shortname=[](std::string k){ if(k=="Gamepad_LeftThumbstick") return std::string("L3"); if(k=="Gamepad_RightThumbstick") return std::string("R3"); if(k.starts_with("Gamepad_")) k=k.substr(8); return k; };
            std::string t; for(const auto& k:s.open_keyboard) t+=(t.empty()?"":"+")+k; t+="  /  "; std::string g; for(const auto& k:s.open_gamepad) g+=(g.empty()?"":"+")+shortname(k); return t+g; }(),"Shortcut that opens the Player Menu on the CSSX tab. Edit open_keyboard and open_gamepad in settings.json with the game closed (Unreal key names)."},
        {"Performance",perf_line(true),perf_detail()}};
    settings_row_=std::clamp(settings_row_,0,int(rows.size())-1);
    for(size_t i=0;i<rows.size();++i) {
        const double y=top+i*row_h; const bool selected=int(i)==settings_row_;
        auto* hit=ui.button("",x,y,w,row_h-6,selected,true); hits_.push_back({WeakObject(hit),{{"action","settings_row"},{"row",int(i)}},false});
        if(selected) { ui.box(x,y,w,row_h-6,row_selected); ui.box(x,y+9,3,row_h-24,gold); }
        ui.label(rows[i].label,x+24,y+13,w*0.6,32,21,selected?bright:ink);
        ui.label(rows[i].value,x+w*0.62,y+14,w*0.36,30,19,selected?gold:muted,false,2);
    }
    ui.label("Saved to Mods/CSSX/settings.json",x+4,top+rows.size()*row_h+8,w,26,14,muted);
    const double px=x+w+36,pw=width-px-70;
    ui.box(px,ptop,pw,panel_h,panel); ui.box(px+24,ptop,pw-48,1,line);
    ui.label(rows[settings_row_].label,px+24,ptop+20,pw-48,44,24,bright,true);
    scroll_text(ui,rows[settings_row_].hint,px+24,ptop+76,pw-48,260,19,ink);
    if(settings_row_<2) {
        auto* less=ui.button("<",px+24,ptop+360,56,48,false,true,22,ink); hits_.push_back({WeakObject(less),{{"action","settings_adjust"},{"delta",-1}},false});
        auto* more=ui.button(">",px+pw-80,ptop+360,56,48,false,true,22,ink); hits_.push_back({WeakObject(more),{{"action","settings_adjust"},{"delta",1}},false});
        ui.label(rows[settings_row_].value,px+90,ptop+366,pw-180,40,22,gold,false,1);
    }
    build_footer(ui,width,{{"up",""},{"down","Browse"},{"previous_section",""},{"next_section","Library"},{"close","Close"}},settings_row_<2?std::vector<std::pair<std::string,std::string>>{{"left",""},{"right","Adjust"}}:std::vector<std::pair<std::string,std::string>>{});
}
void Menu::build_extension(Layout& ui,double width) {
    const Json* entry=nullptr; for(const auto& e:library_["extensions"]) if(e.value("id",std::string{})==extension_id_) entry=&e;
    if(!entry) { screen_=Screen::Library; extension_id_.clear(); build_library(ui,width); return; }
    const double list_x=70,list_w=std::min(1000.,width*0.50),row_h=58,top=content_top,ptop=panel_top,panel_h=reference_h-128-16-panel_top;
    if(!model_.is_object() || !model_.contains("sections")) {
        frame(ui,width,list_w,entry->value("title",std::string{}),"by "+entry->value("author",std::string{})+"  /  "+entry->value("version",std::string{}),{},0,"section");
        ui.label(error_.empty()?"This extension has no menu.":error_,70,content_top,width-140,200,22,danger);
        build_footer(ui,width,{{"close","Library"}},{}); return;
    }
    const auto& sections=model_["sections"]; const int count=int(sections.size());
    section_=std::clamp(section_,0,std::max(0,count-1));
    std::vector<std::string> tabs; for(const auto& sct:sections) tabs.push_back(sct.value("title",std::string{}));
    frame(ui,width,list_w,entry->value("title",std::string{}),"by "+entry->value("author",std::string{})+"  /  "+entry->value("version",std::string{}),tabs,section_,"section");
    // Left: control rows, as wide as the CSS list. Right: full-height detail panel.
    const auto& controls=count?sections[section_]["controls"]:Json::array();
    const int rows=int(controls.size());
    const auto section_help=sections[section_].value("description",std::string{});
    double list_top=top;
    if(!section_help.empty()) { ui.label(section_help,list_x+4,top,list_w,30,15,muted); list_top+=34; }
    // Rows must stay inside the page: the game scales every Player Menu tab
    // to the tallest page, so a row placed below the footer shrinks the whole
    // menu including the top bar. The list area ends above the status line.
    const int visible=std::max(3,int((reference_h-128-16-list_top-30)/row_h));
    row_=std::clamp(row_,0,std::max(0,rows-1));
    if(row_<first_row_) first_row_=row_; if(row_>=first_row_+visible) first_row_=row_-visible+1;
    first_row_=std::clamp(first_row_,0,std::max(0,rows-visible));
    for(int i=first_row_;i<std::min(first_row_+visible,rows);++i) {
        const auto& c=controls[i]; const double y=list_top+(i-first_row_)*row_h; const bool selected=i==row_;
        const bool enabled=c.value("enabled",true) && !c.value("busy",false);
        auto* hit=ui.button("",list_x,y,list_w,row_h-4,selected && enabled,true);
        hits_.push_back({WeakObject(hit),{{"action","row"},{"row",i}},false});
        if(selected) { ui.box(list_x,y,list_w,row_h-4,enabled?row_selected:Color{.04f,.04f,.04f,.8f}); ui.box(list_x,y+9,3,row_h-22,enabled?gold:muted); }
        const auto type=c.at("type").get<std::string>();
        ui.label(c.value("label",std::string{}),list_x+22,y+12,list_w*0.6-22,32,20,enabled?(selected?bright:ink):muted);
        std::string value=c.value("busy",false)?"Working...":!c.value("enabled",true)?c.value("disabled_label",std::string("Unavailable")):display_value(c);
        if(!value.empty() && c.contains("unit") && (type=="number" || type=="slider")) value+=c.value("unit",std::string{});
        Color vc=enabled?(type=="toggle"?(c.value("value",false)?good:muted):gold):muted;
        if(c.value("severity",std::string{})=="danger" && type=="button") vc=danger;
        ui.label(value,list_x+list_w*0.6,y+13,list_w*0.4-30,30,18,vc,false,2);
        // Toggles: a small state lamp at the row edge so active cheats read at a glance.
        if(type=="toggle") { const bool on=c.value("value",false); ui.box(list_x+list_w-14,y+row_h/2-8,6,12,on?good:Color{.12f,.10f,.08f,1}); }
    }
    if(rows>visible) {
        const double track=visible*row_h;
        ui.box(list_x+list_w+8,list_top,3,track,Color{.05f,.04f,.03f,1});
        ui.box(list_x+list_w+8,list_top+track*first_row_/rows,3,std::max(12.,track*visible/rows),gold);
        ui.label(std::to_string(first_row_+1)+" - "+std::to_string(std::min(first_row_+visible,rows))+" of "+std::to_string(rows),list_x,list_top+track+6,list_w,26,15,muted);
    }
    if(!rows) ui.label("This section has no controls.",list_x,list_top+20,list_w,40,18,muted);
    // Right: detail and editor for the selected control.
    const double px=list_x+list_w+36,pw=width-px-70;
    ui.box(px,ptop,pw,panel_h,panel); ui.box(px+24,ptop,pw-48,1,line);
    double pt=ptop;
    // Notice strip: the extension's one pending thing (unapplied edits,
    // cleanup) with its action, at the top of the panel, on every section.
    if(model_.contains("notice") && model_["notice"].is_object()) {
        const auto& n=model_["notice"];
        const auto text=n.value("text",std::string{}),label=n.value("label",std::string{}),action=n.value("action",std::string{});
        const double x=px+24,w=pw-48,y=ptop+14;
        auto* hit=ui.button("",x,y,w,44,false,!action.empty()); hits_.push_back({WeakObject(hit),{{"action","run"},{"id",action}},false});
        ui.box(x,y,w,44,Color{.10f,.075f,.035f,.95f}); ui.box(x,y,3,44,gold);
        ui.label(text,x+18,y+8,w-36,30,18,bright);
        if(!label.empty()) ui.label(label+"  >",x+18,y+8,w-36,30,18,gold,false,2);
        pt+=64;
    }
    if(rows) {
        const auto& c=controls[row_]; const auto type=c.at("type").get<std::string>();
        const bool enabled=interactive(c);
        ui.label(c.value("label",std::string{}),px+24,pt+20,pw-48,44,24,bright,true);
        const auto effect=effect_label(c);
        double y=pt+72;
        if(!effect.empty()) { ui.label(effect,px+24,y,pw-48,26,15,severity_color(c)); y+=30; }
        const auto description=c.value("description",std::string{});
        auto* d=scroll_text(ui,description,px+24,y,pw-48,150,19,ink); description_=d;
        if(description.size()>200) { auto* more=ui.button("",px+24,y+152,pw-48,34,false,true); hits_.push_back({WeakObject(more),{{"action","details"}},false}); prompt(ui,"secondary","Read the full description",px+24,y+156,pw-48,glyph_secondary); }
        y+=196;
        const auto hint=c.value("hint",std::string{});
        if(!hint.empty()) { ui.label(hint,px+24,y,pw-48,48,16,muted); y+=52; }
        ui.box(px+24,y,pw-48,1,line); y+=16;
        std::vector<std::pair<std::string,std::string>> right;
        if(type=="radio") {
            const auto& options=c.at("options");
            for(size_t i=0;i<options.size();++i) {
                const double oy=y+i*36; const bool chosen=options[i].at("id")==c.at("value");
                auto* hit=ui.button("",px+24,oy,pw-48,32,chosen,enabled); hits_.push_back({WeakObject(hit),{{"action","value"},{"value",options[i].at("id")}},false});
                mark(ui,px+32,oy+10,chosen);
                ui.label(options[i].value("label",std::string{}),px+60,oy+3,pw-96,30,19,chosen?gold:(enabled?ink:muted));
            }
            if(enabled) right={{"left",""},{"right","Choose"}};
        } else if(type=="slider") {
            auto* slider=construct(L"/Script/UMG.Slider",tree_.Get());
            for(const auto& setting:{std::pair{L"SetMinValue","min"},std::pair{L"SetMaxValue","max"},std::pair{L"SetStepSize","step"},std::pair{L"SetValue","value"}})
                invoke(slider,setting.first,L"InValue",c.at(setting.second).get<float>());
            invoke(slider,L"SetSliderBarColor",L"InValue",Color{.10f,.08f,.06f,1});
            invoke(slider,L"SetSliderHandleColor",L"InValue",gold);
            invoke(slider,L"SetIsEnabled",L"bInIsEnabled",enabled);
            ui.place(slider,px+32,y+44,pw-64,36);
            auto* label=ui.label(display_value(c)+c.value("unit",std::string{}),px+24,y,pw-48,36,24,bright,false,1);
            sliders_.push_back({WeakObject(slider),WeakObject(label),c,c.at("value").get<float>()});
            if(enabled) right={{"left",""},{"right","Adjust"}};
        } else if(type=="number" || type=="choice") {
            auto* less=ui.button("<",px+24,y,56,52,false,enabled,24,ink); hits_.push_back({WeakObject(less),{{"action","adjust"},{"delta",-1}},false});
            auto* more=ui.button(">",px+pw-80,y,56,52,false,enabled,24,ink); hits_.push_back({WeakObject(more),{{"action","adjust"},{"delta",1}},false});
            ui.label(display_value(c)+(type=="number"?c.value("unit",std::string{}):std::string{}),px+88,y+8,pw-176,40,22,bright,false,1);
            if(type=="choice") {
                auto* browse=ui.button("",px+24,y+66,pw-48,44,false,enabled); hits_.push_back({WeakObject(browse),{{"action","pick"}},false});
                if(enabled) prompt(ui,"accept","Browse and search options",px+36,y+74,pw-72,glyph_accept);
            }
            if(enabled) right={{"left",""},{"right","Adjust"}};
        } else if(type=="text") {
            const auto key=extension_id_+"/"+c.at("id").get<std::string>();
            if(text_key_!=key) { text_key_=key; text_draft_=c.value("value",std::string{}); }
            ui.box(px+24,y,pw-48,48,Color{.05f,.04f,.03f,1});
            name_input_=text_input(ui,text_draft_,px+36,y+4,pw-72,enabled);
            auto* save=ui.button("Save text",px+24,y+62,pw-48,46,false,enabled,20,ink); hits_.push_back({WeakObject(save),{{"action","text"}},false});
            if(enabled) right={{"accept","Save text"}};
        } else if(type=="progress" || type=="loading") {
            const bool loading=type=="loading" && c.value("value",false);
            ui.box(px+24,y+30,pw-48,6,Color{.07f,.06f,.04f,1});
            ui.box(px+24,y+30,std::max(2.,(pw-48)*(type=="progress"?std::clamp(c.value("value",0.0),0.0,1.0):(loading?0.3:1.0))),6,gold);
            ui.label(display_value(c),px+24,y-4,pw-48,30,20,bright);
        } else if(type!="label") {
            const auto label=type=="toggle"?(c.value("value",false)?"Turn off":"Turn on"):c.value("label",std::string{});
            auto* action=ui.button("",px+24,y,pw-48,52,false,enabled); hits_.push_back({WeakObject(action),{{"action","activate"}},false});
            ui.box(px+24,y,pw-48,52,enabled?(c.value("severity",std::string{})=="danger"?Color{.16f,.05f,.04f,.9f}:Color{.06f,.05f,.03f,.9f}):Color{.03f,.03f,.03f,.9f});
            if(enabled) prompt(ui,"accept",label,px+40,y+12,pw-80,glyph_accept);
            else ui.label(c.value("busy",false)?"Working...":c.value("disabled_label",std::string("Unavailable")),px+40,y+12,pw-80,30,19,muted);
            if(enabled) right={{"accept",label}};
        }
        if(c.contains("confirm") && enabled) ui.label("Asks for confirmation",px+24,ptop+panel_h-40,pw-48,26,15,muted);
        build_footer(ui,width,{{"close","Library"},{"previous_section",""},{"next_section","Sections"},{"up",""},{"down","Browse"}},right);
    } else build_footer(ui,width,{{"close","Library"},{"previous_section",""},{"next_section","Sections"}},{});
}
void Menu::build_modal(Layout& ui,double width) {
    if(picker_) {
        const auto* c=current_control(); if(!c) { picker_=false; return; }
        const auto m=modal_frame(ui,"Choose "+c->value("label",std::string{}),width,840);
        ui.label("Type to search by name",m.x+32,m.y+108,m.w-64,28,17,muted);
        ui.box(m.x+32,m.y+142,m.w-64,46,Color{.05f,.04f,.03f,1});
        search_input_=text_input(ui,search_query_,m.x+44,m.y+146,m.w-88,true);
        auto* list=construct(L"/Script/UMG.CanvasPanel",tree_.Get()); search_results_=list;
        ui.place(list,m.x+32,m.y+206,m.w-64,440);
        search_count_=ui.label("",m.x+32,m.y+656,m.w-64,28,16,muted);
        auto* cancel=ui.button("",m.x+24,m.y+m.h-66,200,46,false,true); hits_.push_back({WeakObject(cancel),{{"action","pick_cancel"}},false});
        prompt(ui,"close","Back",m.x+36,m.y+m.h-56,170,glyph_back);
        auto* apply=ui.button("",m.x+m.w-244,m.y+m.h-66,220,46,true,true); hits_.push_back({WeakObject(apply),{{"action","pick_apply"}},false});
        prompt(ui,"accept","Select",m.x+m.w-230,m.y+m.h-56,190,glyph_accept);
        build_results();
        return;
    }
    if(details_) {
        const auto* c=current_control(); if(!c) { details_=false; return; }
        const auto m=modal_frame(ui,c->value("label",std::string{}),width,760);
        description_=scroll_text(ui,c->value("description",std::string{}),m.x+32,m.y+112,m.w-64,m.h-220,20,ink);
        auto* back=ui.button("",m.x+24,m.y+m.h-66,200,46,false,true); hits_.push_back({WeakObject(back),{{"action","details"}},false});
        prompt(ui,"close","Back",m.x+36,m.y+m.h-56,170,glyph_back);
        prompt(ui,"up","",m.x+m.w-250,m.y+m.h-56,28,glyph_up); prompt(ui,"down","Scroll",m.x+m.w-214,m.y+m.h-56,180,glyph_down);
        return;
    }
    if(!confirm_.is_null()) {
        const auto m=modal_frame(ui,"Confirm",width,420);
        const auto* c=current_control();
        if(c) { const auto effect=effect_label(*c); if(!effect.empty()) ui.label(effect,m.x+32,m.y+104,m.w-64,26,16,severity_color(*c)); }
        description_=scroll_text(ui,confirm_.value("message",std::string("Continue?")),m.x+32,m.y+136,m.w-64,150,20,ink);
        const double w=(m.w-80)/2,y=m.y+m.h-66;
        auto* cancel=ui.button("",m.x+24,y,w,46,false,true); hits_.push_back({WeakObject(cancel),{{"action","cancel"}},false});
        ui.box(m.x+24,y,w,46,Color{.03f,.026f,.02f,1}); prompt(ui,"close","Cancel",m.x+40,y+9,w-24,glyph_back);
        auto* confirm=ui.button("",m.x+m.w-w-24,y,w,46,true,true); hits_.push_back({WeakObject(confirm),{{"action","confirm"}},false});
        ui.box(m.x+m.w-w-24,y,w,46,c && c->value("severity",std::string{})=="danger"?Color{.16f,.05f,.04f,1}:Color{.07f,.055f,.03f,1}); prompt(ui,"accept","Confirm",m.x+m.w-w-8,y+9,w-24,glyph_accept);
    }
}
void Menu::build_results() {
    auto* canvas=search_results_.Get(); if(!canvas) return;
    invoke(canvas,L"ClearChildren");
    std::erase_if(hits_,[](const auto& hit){ return hit.action.value("action",std::string{})=="pick_row"; });
    auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
    auto* title=load("/Game/Sparta/UI/Fonts/Trajan_Pro_Regular_Font.Trajan_Pro_Regular_Font");
    const double scale=viewport_[1]/reference_h*deps_.settings->ui_scale;
    Layout ui{tree_.Get(),canvas,scale,serif,title};
    const double width=std::min(920.,viewport_[0]/scale-160.)-64;
    const size_t first=options_.selected/8*8;
    for(size_t i=first;i<std::min(first+8,options_.matches.size());++i) {
        const auto& option=options_.options[options_.matches[i]]; const double y=(i-first)*54.;
        auto* button=ui.button("",0,y,width,50,i==options_.selected,true);
        if(i==options_.selected) ui.box(0,y,width,50,row_selected);
        mark(ui,16,y+19,i==options_.selected);
        ui.label(option.value("label",std::string{}),44,y+10,width-60,32,20,i==options_.selected?bright:ink);
        hits_.push_back({WeakObject(button),{{"action","pick_row"},{"row",i}},false});
    }
    if(options_.matches.empty()) ui.label("No matching options",20,140,width-40,40,20,muted);
    if(auto* count=search_count_.Get()) text_value(count,std::to_string(options_.matches.size())+" matches / "+std::to_string(options_.options.size())+" options");
}
Json Menu::diagnostics() const {
    Json bindings=Json::object(); for(const auto& b:bindings_) bindings[b.action]=b.keys;
    return {{"bindings",bindings},{"open",active_},{"attached",page_.Get()!=nullptr},{"tab_index",tab_index_},{"screen",screen_==Screen::Library?"library":screen_==Screen::Extension?"extension":"settings"},{"extension",extension_id_},
            {"section",section_},{"row",row_},{"library_row",library_row_},{"picker",picker_},{"details",details_},{"confirm",!confirm_.is_null()},
            {"error",error_},{"hits",hits_.size()},{"widgets",cost_.widgets},{"builds",cost_.builds},{"last_build_us",cost_.last_build_us},{"viewport",viewport_},{"gamepad",gamepad_}};
}
}

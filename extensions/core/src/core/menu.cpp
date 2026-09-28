// Hosting in the Player Menu, input, navigation and the extension-facing model flow.
// The page itself (game widgets, pooling, builds) is menu_page.cpp.
#include "menu.hpp"
#include "menu_keys.hpp"
#include "controls.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace cssx {
using namespace engine;
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
// Each mapped key's FName is made once; the menu asks for every key every frame.
FName key_name(const std::string& key) {
    static std::unordered_map<std::string,FName> names;
    if(auto it=names.find(key);it!=names.end()) return it->second;
    return names.emplace(key,FName(wide(key).c_str())).first->second;
}
bool key_down(UObject* pc,const std::string& key) {
    Call call(pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
    member(call.data(p),p->GetElementSize(),find_cached(L"/Script/InputCore.Key"),L"KeyName",key_name(key));
    call.run(); return call.get<bool>();
}
bool ccs_tab(UObject* widget) {
    if(!widget) return false;
    auto* text=widget->GetPropertyByNameInChain(L"Text");
    if(!text || !widget->GetClassPrivate()) return false;
    const auto offset=text->GetOffset_Internal();
    const auto size=widget->GetClassPrivate()->GetPropertiesSize();
    if(offset<0 || offset>size || text->GetElementSize()>size-offset) return false;
    Call convert(find_cached(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
    auto* input=convert.param(L"InText");
    if(!text->SameType(input) || text->GetArrayDim()!=1) return false;
    input->CopyCompleteValue(convert.data(input),reinterpret_cast<const std::byte*>(widget)+text->GetOffset_Internal());
    convert.run();
    const auto& value=*static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars=value.GetCharArray();
    return chars.Num()==4 && chars.GetData() && chars.GetData()[3]==L'\0' && std::wstring_view(chars.GetData(),3)==L"CCS";
}
bool has_focus(UObject* widget) {
    if(!widget) return false;
    Call focus(widget,L"HasKeyboardFocus",1); focus.run(); return focus.get<bool>();
}
}
void Menu::navigate(int index) {
    auto* tabs=tabs_.Get(); if(!tabs) return;
    if(index==tab_index_ && page_.Get() && tab_.Get() && switcher_.Get()) {
        // CCS can insert before CSSX after attachment. Resolve our own paired children when opening.
        Call page_index(switcher_.Get(),L"GetChildIndex",2); page_index.set(L"content",page_.Get()); page_index.run();
        Call tab_index(tabs,L"GetChildIndex",2); tab_index.set(L"content",tab_.Get()); tab_index.run();
        index=page_index.get<int32_t>();
        if(index<0 || index!=tab_index.get<int32_t>()) throw std::runtime_error("CSSX tab/page pairing is inconsistent");
        tab_index_=index;
    }
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
    const bool ccs_present=tab_list.size()>=4 && std::any_of(tab_list.begin()+3,tab_list.end(),ccs_tab);
    const size_t expected=3+static_cast<size_t>(css_present_)+static_cast<size_t>(ccs_present);
    if(page_list.size()!=expected || tab_list.size()!=expected) {
        if(!attach_wait_since_) attach_wait_since_=now;
        if(page_list.size()<3 || page_list.size()>expected || now-attach_wait_since_<2000) return false;
    }
    if(page_list.size()>4+static_cast<size_t>(ccs_present)) throw std::runtime_error("Player Menu already has "+std::to_string(page_list.size())+" pages; another mod owns the extra tab");
    attach_wait_since_=0;
    auto* tab=create_widget(player.pc,original->GetClassPrivate());
    for(auto name:{L"FontData",L"RootSize",L"RootScale",L"DefaultColor",L"SelectedColor",L"bUseHighlight",L"HighlightY"}) copy_property(tab,original,name);
    text_property(tab,L"Text","CSSX");
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
    if(tabs.size()!=pages.size() || tabs.size()<4 || tabs.size()>6 || (tabs.size()==6 && !std::any_of(tabs.begin()+3,tabs.end(),ccs_tab))) throw std::runtime_error("Player Menu tab count is unexpected");
    // CCS orders its tab after CSS and before CSSX. Keep CSSX last in either attachment order.
    const int index=int(tabs.size())-1;
    auto move_to=[&](auto& values,UObject* value){ auto it=std::find(values.begin(),values.end(),value); if(it==values.end()) throw std::runtime_error("CSSX child is missing"); values.erase(it); values.insert(values.begin()+index,value); };
    move_to(tabs,tab_.Get()); move_to(pages,page_.Get());
    reorder(switcher_.Get(),pages); reorder(tabs_.Get(),tabs);
    // Share the original top bar spacing across the extra title(s), as CSS does.
    const float factor=tabs.size()>=5?.6f:.75f;
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
    if(deps_.log) deps_.log("Menu: closing the Player Menu from the CSSX page");
    // Leaving the page from here (a key, a click, an extension's menu.close inside an event)
    // must restore what the page borrowed before the tick's own leave path is skipped:
    // the game's input listeners a confirmation froze, and the dialog itself.
    try { dialog_close(); } catch(...) {}
    confirm_=nullptr; picker_=false; hits_.clear(); sliders_.clear(); drag_slider_=-1;
    Call close_call(handler,L"HandleGameMenu",2); close_call.set(L"SubTabIndex",int32_t{0}); close_call.set(L"AllowClose",true); close_call.run();
    active_=was_active_=false;
}
void Menu::forget() {
    try { dialog_close(true); } catch(...) {}
    forget_page();
    unroot_all(); textures_.clear(); warm_texture_=0;
    page_.Reset(); tab_.Reset(); tree_.Reset(); canvas_.Reset(); main_.Reset(); tabs_.Reset(); switcher_.Reset(); handler_.Reset(); pc_.Reset();
    hits_.clear(); sliders_.clear(); bindings_.clear(); drag_slider_=-1;
    search_input_.Reset(); name_input_.Reset(); input_prompt_.Reset();
    active_=was_active_=false; tab_index_=-1; attach_wait_since_=0; viewport_={}; dirty_=true;
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
    Call subsystem(find_cached(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetLocalPlayerSubSystemFromPlayerController",3);
    subsystem.set(L"PlayerController",pc); subsystem.set(L"Class",static_cast<UClass*>(find_cached(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem"))); subsystem.run();
    auto* input=subsystem.get<UObject*>();
    if(!input) throw std::runtime_error("Player input subsystem is unavailable");
    static const std::map<std::wstring,std::string> actions={
        {L"IA_Menu_Up","up"},{L"IA_Menu_Down","down"},{L"IA_Menu_Left_Primary","left"},{L"IA_Menu_Right_Primary","right"},
        {L"IA_Menu_Left_Tertiary","previous_section"},{L"IA_Menu_Right_Tertiary","next_section"},
        {L"IA_Menu_Confirm_Primary_Press","accept"},{L"IA_Menu_Confirm_Secondary_Press","secondary"},{L"IA_Menu_Back","close"}};
    auto* a=static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(mapping)+p->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>256) throw std::runtime_error("Input map exceeds bound");
    auto* mapping_struct=find_cached(L"/Script/EnhancedInput.EnhancedActionKeyMapping");
    auto* ap=field(mapping_struct,L"Action",8);
    auto* key_field=mapping_struct->GetPropertyByNameInChain(L"Key");
    auto* kn=field(find_cached(L"/Script/InputCore.Key"),L"KeyName",sizeof(FName));
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
bool Menu::typing() const {
    // The search field and the text editor take the keyboard; menu keys yield to them.
    return has_focus(search_input_.Get()) || has_focus(name_input_.Get());
}
void Menu::poll_input(const PlayerContext& player,uint64_t now,bool typing) {
    auto* pc=player.pc; if(!pc) return;
    // Each key test is a reflected call; only the keys of the device in use are tested
    // (the game's prompt widget reports the device). A press on the other device flips
    // the prompt within a frame, so nothing is lost.
    for(auto& b:bindings_) {
        bool down=false, allowed=!typing;
        for(const auto& name:b.keys) {
            const bool pad=name.starts_with("Gamepad_");
            if(pad!=gamepad_ && input_prompt_.Get() && !typing) continue;
            if(key_down(pc,name)) { down=true; if(typing && (pad || name=="Escape")) allowed=true; break; }
        }
        const bool repeating=b.action=="up" || b.action=="down" || b.action=="left" || b.action=="right";
        const bool trigger=allowed && down && (!b.down || (repeating && now>=b.repeat));
        if(down && !b.down) b.repeat=now+400;
        else if(trigger) b.repeat=now+90;
        b.down=down;
        if(trigger) { key(b.action); return; }   // one action per frame keeps navigation predictable
    }
}
// Where the pointer sits along a native slider bar, 0..1, and whether it is over the bar.
double Menu::along_bar(const SliderHit& slider,bool& inside) const {
    inside=false;
    auto* bar=slider.bar.Get(); if(!bar) return 0;
    Call geometry(bar,L"GetCachedGeometry",1); geometry.run();
    Call pointer(find_cached(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetMousePositionOnPlatform",1); pointer.run();
    Call local(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"AbsoluteToLocal",3);
    local.copy(L"Geometry",geometry,L"ReturnValue"); local.set(L"AbsoluteCoordinate",pointer.get<Vec2>()); local.run();
    Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
    size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
    const auto at=local.get<Vec2>(); const auto extent=size.get<Vec2>();
    if(extent.x<1 || extent.y<1) return 0;
    inside=at.x>=0 && at.x<=extent.x && at.y>=-extent.y*.75 && at.y<=extent.y*1.75;
    return std::clamp(at.x/extent.x,0.,1.);
}
void Menu::poll_mouse(const PlayerContext& player) {
    if(!player.pc) return;
    // Buttons and sliders only change under the mouse, so they are read while a mouse button
    // is down and for one frame after release (a quick click, the slider's final value). The
    // OS button state holds in every input mode the menu can be in; physical buttons, so
    // swapped buttons are swapped back.
    const bool swapped=GetSystemMetrics(SM_SWAPBUTTON)!=0;
    const bool left_now=(GetAsyncKeyState(swapped?VK_RBUTTON:VK_LBUTTON)&0x8000)!=0;
    const bool mouse_now=left_now || (GetAsyncKeyState(swapped?VK_LBUTTON:VK_RBUTTON)&0x8000)!=0;
    const bool mouse=mouse_now || mouse_was_down_;
    const bool left_pressed=left_now && !left_was_down_;
    const bool left_released=!left_now && left_was_down_;
    mouse_was_down_=mouse_now; left_was_down_=left_now;
    if(!mouse) { for(auto& hit:hits_) hit.down=false; drag_slider_=-1; return; }
    // The game's slider bar only steps with its arrows. A left press that lands on a bar
    // starts a drag; while the button is held the pointer's place along the bar previews
    // the value, and the release commits it as one event.
    if(left_pressed) {
        drag_slider_=-1;
        for(size_t i=0;i<sliders_.size();++i) { bool inside=false; along_bar(sliders_[i],inside); if(inside) { drag_slider_=int(i); break; } }
    }
    if(drag_slider_>=0 && drag_slider_<int(sliders_.size())) {
        auto& slider=sliders_[drag_slider_];
        if(left_now) {
            bool inside=false; const double fraction=along_bar(slider,inside);
            double v=slider.low+fraction*(slider.high-slider.low);
            if(slider.step>0) v=slider.low+std::round((v-slider.low)/slider.step)*slider.step;
            v=std::clamp(v,slider.low,slider.high);
            if(std::abs(v-slider.previous)>1e-9) {
                slider.previous=v;
                auto shown=slider.control; shown["value"]=v;
                if(auto* block=slider.value_block.Get()) text_value(block,display_value(shown)+slider.unit);
                if(auto* bar=slider.bar.Get()) invoke(bar,L"UpdateProgressBar",L"InPercent",float(slider.high>slider.low?(v-slider.low)/(slider.high-slider.low):0));
            }
            return;
        }
        if(left_released) {
            const double v=slider.previous; drag_slider_=-1;
            if(std::abs(v-slider.control.at("value").get<double>())>1e-9) act({{"action","value"},{"value",v}});
            return;
        }
        drag_slider_=-1;
    }
    for(auto& hit:hits_) if(auto* widget=hit.widget.Get()) {
        Call pressed(widget,L"IsPressed",1); pressed.run(); const bool down=pressed.get<bool>();
        const bool click=down && !hit.down; hit.down=down;
        if(!click) continue;
        // A two-key prompt (W / S, A / D): the glyph under the pointer picks the direction.
        if(!hit.parts.empty()) {
            Call pointer(find_cached(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetMousePositionOnPlatform",1); pointer.run();
            for(const auto& [part,action]:hit.parts) if(auto* glyph=part.Get()) {
                Call geometry(glyph,L"GetCachedGeometry",1); geometry.run();
                Call under(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"IsUnderLocation",3);
                under.copy(L"Geometry",geometry,L"ReturnValue"); under.set(L"AbsoluteCoordinate",pointer.get<Vec2>()); under.run();
                if(under.get<bool>()) { try { invoke(glyph,L"TriggerInputAnim"); } catch(...) {} act(action); return; }
            }
        }
        if(auto* glyph=hit.glyph.Get()) { try { invoke(glyph,L"TriggerInputAnim"); } catch(...) {} }
        act(hit.action); return;   // act() may rebuild the page, so return at once
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
    // Menu closed: one cached flag read, no engine call.
    const bool menu_open=bool_of(main,L"bOpen");
    if(open_requested_ && menu_open) { open_requested_=false; try { navigate(tab_index_); } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Could not select the CSSX tab: ")+e.what()); } }
    if(open_requested_ && now-open_requested_at_>3000) open_requested_=false;
    if(!menu_open) {
        active_=false; was_active_=false; transition_started_=0;
        if(!frozen_listeners_.empty() || !dialog_shown_.empty()) { try { dialog_close(); } catch(...) {} }
        return;
    }
    Call selected(switcher,L"GetActiveWidget",1); selected.run();
    active_=selected.get<UObject*>()==page_.Get();
    if(active_ && !was_active_) {
        bindings_ready_=false; bind_retry_=0;
        try { bind_inputs(); ++bindings_generation_; } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Menu input binding failed: ")+e.what()); }
        for(auto& b:bindings_) { b.down=true; b.repeat=now+400; }
        mouse_was_down_=left_was_down_=true; dirty_=true; enter_=true; error_.clear(); confirm_=nullptr; picker_=false; drag_slider_=-1;
        refresh_library(true,now);
        invalidate_page();   // the reopened menu reconstructed every widget on the page
    }
    if(!active_ && was_active_) { hits_.clear(); sliders_.clear(); transition_started_=0; }
    was_active_=active_;
    if(!active_) {
        // Nothing of the game's may stay borrowed while the page is not showing.
        if(!frozen_listeners_.empty() || !dialog_shown_.empty()) { try { dialog_close(); } catch(...) {} }
        warm(now); return;
    }
    // Enhanced Input rebuilds its key mappings a tick after the game adds the
    // menu context, so the first query can come back empty. Retry until keys
    // appear, then redraw the hints with the real glyphs.
    if(!bindings_ready_ && now>=bind_retry_) {
        bind_retry_=now+200;
        try {
            bind_inputs(); ++bindings_generation_;
            bindings_ready_=std::any_of(bindings_.begin(),bindings_.end(),[](const Binding& b){ return !b.keys.empty(); });
            for(auto& b:bindings_) { b.down=true; b.repeat=now+400; }
            if(bindings_ready_) dirty_=true;
        } catch(...) {}
    }
    if(now>=layout_check_) {
        layout_check_=now+500;
        Call geometry(switcher,L"GetCachedGeometry",1); geometry.run();
        Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2); size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        const auto extent=size.get<Vec2>();
        if(std::abs(extent.x-viewport_[0])>.5 || std::abs(extent.y-viewport_[1])>.5) { viewport_={extent.x,extent.y}; dirty_=true; }
    }
    if(auto* prompt=input_prompt_.Get()) { try { const bool gamepad=read<uint8_t>(prompt,L"InputType")==1; if(gamepad!=gamepad_) { gamepad_=gamepad; dirty_=true; } } catch(...) {} }
    if(now>=library_check_) { library_check_=now+250; refresh_library(false,now); }
    bool typing_now=false;
    if(picker_) if(auto* search=search_input_.Get()) {
        try { const auto query=text_of(search,256); if(query!=search_query_) { search_query_=query; if(options_.filter(query)) dirty_=true; } }
        catch(const std::exception& e) { error_=e.what(); }
    }
    try { typing_now=typing(); } catch(...) {}
    fit_panel();        // measures the previous build, which has laid out by now
    reveal_pending();
    if(dirty_) { try { build(); } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Menu build failed: ")+e.what()); error_=e.what(); dirty_=false; } }
    try { animate(now); } catch(const std::exception& e) { transition_started_=0; if(deps_.log) deps_.log(std::string("Menu transition failed: ")+e.what()); }
    if(!active_) return;
    // The mouse wheel walks the search results while the picker is open and nobody types.
    if(picker_ && !typing_now && now>=wheel_after_) {
        try {
            Call wheel(player.pc,L"GetInputAnalogKeyState",2); auto* key=wheel.param(L"Key");
            member(wheel.data(key),key->GetElementSize(),find_cached(L"/Script/InputCore.Key"),L"KeyName",key_name("MouseWheelAxis")); wheel.run();
            const float scroll=wheel.get<float>();
            if(std::abs(scroll)>.01f) { wheel_after_=now+100; options_.move(scroll<0?1:-1); dirty_=true; }
        } catch(...) {}
    }
    try { poll_input(player,now,typing_now); poll_mouse(player); }
    catch(const std::exception& e) { error_=e.what(); dirty_=true; }
}
// While the Player Menu is open on another tab the game is paused and the CSSX page is not
// on screen, so this is where the expensive one-offs go: the page skeleton, texture
// imports, and the first pooled rows. Opening the CSSX tab afterwards costs nothing visible.
void Menu::warm(uint64_t now) {
    if(!page_.Get()) return;
    try {
        if(!design_.Get()) {
            if(now<layout_check_) return; layout_check_=now+500;
            Call geometry(switcher_.Get(),L"GetCachedGeometry",1); geometry.run();
            Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2); size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
            const auto extent=size.get<Vec2>();
            if(extent.x<320 || extent.y<240) return;
            viewport_={extent.x,extent.y};
            page(extent.x,extent.y);
            return;
        }
        // One texture per tick: the framework art, then each extension's banner.
        std::vector<fs::path> files={deps_.root/"assets/logo.png",deps_.root/"assets/banner.png"};
        for(const auto& e:library_entries()) { const auto banner=e.value("banner",std::string{}); if(!banner.empty()) files.push_back(utf8_path(banner)); }
        if(warm_texture_<files.size()) { texture_at(files[warm_texture_++]); return; }
        if(now>=library_check_) { library_check_=now+250; refresh_library(false,now); }
        // The first builds create the pooled widgets four at a time; the page is not the
        // active tab, so nothing of this shows.
        if(dirty_) build();
    } catch(const std::exception& e) { if(deps_.log) deps_.log(std::string("Menu warm-up failed: ")+e.what()); dirty_=false; }
}
void Menu::refresh_library(bool force,uint64_t now) {
    if(!deps_.runtime) { library_={{"revision",0},{"extensions",Json::array()},{"errors",Json::array()}}; return; }
    // The library is copied only when the runtime's revision moved (an event, an invalidate,
    // a reload) or, on the library screen, once a second for the status summaries.
    static uint64_t status_after=0;
    const auto revision=deps_.runtime->revision();
    const bool status_due=screen_==Screen::Library && now>=status_after;
    if(!force && revision==library_revision_ && !status_due) return;
    auto library=deps_.runtime->library();
    if(force || revision!=library_revision_) { library_revision_=revision; library_=std::move(library); if(screen_==Screen::Extension) refresh_model(); dirty_=true; }
    else if(library!=library_) { library_=std::move(library); dirty_=true; }
    status_after=now+1000;
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
    refresh_library(true,GetTickCount64()); refresh_model(); dirty_=true;
}
// What one menu key does on the page right now. A click on a key's glyph comes here too, so
// a prompt clicked with the mouse does exactly what its key does.
void Menu::key(const std::string& action) {
    if(!confirm_.is_null()) {
        if(action=="accept") act({{"action","confirm"}});
        else if(action=="close") act({{"action","cancel"}});
        else if(action=="left" || action=="right" || action=="up" || action=="down") { dialog_focus_=action=="left"||action=="up"?0:1; dirty_=true; }
        return;
    }
    if(picker_) {
        if(action=="close") act({{"action","pick_cancel"}});
        else if(action=="up" || action=="down" || action=="previous_section" || action=="next_section") { options_.move(action=="up"?-1:action=="down"?1:action=="previous_section"?-8:8); dirty_=true; }
        else if(action=="accept") act({{"action","pick_apply"}});
        return;
    }
    if(screen_==Screen::Library) {
        const int count=1+int(library_entries().size());
        if(action=="up" || action=="down") { library_row_=std::clamp(library_row_+(action=="up"?-1:1),0,count-1); dirty_=true; }
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
    if(action=="secondary") { if(model_.is_object() && model_.contains("notice") && model_["notice"].is_object()) act({{"action","run"},{"id",model_["notice"].value("action",std::string{})}}); return; }
    if(action=="previous_section" || action=="next_section") { act({{"action","section_delta"},{"delta",action=="previous_section"?-1:1}}); return; }
    if(action=="up" || action=="down") { act({{"action","row_delta"},{"delta",action=="up"?-1:1}}); return; }
    if(action=="left" || action=="right") { act({{"action","adjust"},{"delta",action=="left"?-1:1}}); return; }
    if(action=="accept") act({{"action","activate"}});
}
void Menu::act(const Json& action) {
    const auto name=action.value("action",std::string{});
    if(name!="value") error_.clear();
    if(name=="press") { key(action.value("binding",std::string{})); return; }
    if(name=="close") { close(); return; }
    if(picker_) {
        if(name=="pick_cancel" || name=="library") { picker_=false; dirty_=true; return; }
        if(name=="pick_row") { options_.selected=std::min(action.at("row").get<size_t>(),options_.matches.empty()?size_t{}:options_.matches.size()-1); dirty_=true; return; }
        if(name=="pick_apply") {
            const auto value=options_.value(); if(value.is_null()) return;
            const auto* c=current_control(); if(!c) return;
            Json event={{"id",c->at("id")},{"value",value}};
            picker_=false;
            if(c->contains("confirm")) { confirm_={{"event",event},{"message",c->at("confirm")}}; dialog_focus_=0; }
            else send_event(event);
            dirty_=true; return;
        }
        return;
    }
    if(!confirm_.is_null()) {
        if(name=="confirm") { auto event=confirm_.at("event"); event["confirmed"]=true; confirm_=nullptr; try { dialog_close(); } catch(...) {} send_event(event); }
        else if(name=="cancel") { confirm_=nullptr; try { dialog_close(); } catch(...) {} dirty_=true; }
        return;
    }
    if(name=="details") { dirty_=true; return; }   // the details window scrolls; kept for tooling
    if(name=="library") { screen_=Screen::Library; extension_id_.clear(); model_=nullptr; dirty_=true; enter_=true; return; }
    if(name=="open") {
        const int row=action.value("row",library_row_); library_row_=row;
        if(row<=0) { act({{"action","framework_tab"},{"section",1}}); return; }   // the CSSX entry opens Settings
        const auto& entries=library_["extensions"];
        if(row-1>=int(entries.size())) return;
        const auto& entry=entries[row-1];
        if(!entry.value("available",false)) { error_="This extension is unavailable: "+entry.value("error",std::string{}); dirty_=true; return; }
        extension_id_=entry.at("id").get<std::string>(); screen_=Screen::Extension; section_=row_=0; confirm_=nullptr;
        refresh_model(); dirty_=true; enter_=true; return;
    }
    if(name=="framework_tab") { screen_=action.value("section",0)==1?Screen::Settings:Screen::Library; extension_id_.clear(); model_=nullptr; dirty_=true; enter_=true; return; }
    if(name=="settings_row" && screen_==Screen::Settings) { settings_row_=std::clamp(action.value("row",0),0,3); dirty_=true; return; }
    if(name=="library_row" && screen_==Screen::Library) { library_row_=std::max(0,action.value("row",0)); dirty_=true; return; }
    if(name=="run" && screen_==Screen::Extension && model_.is_object()) {
        // Activate a control by id from anywhere on the page (notice strip).
        const auto id=action.value("id",std::string{});
        for(const auto& section:model_.value("sections",Json::array())) for(const auto& c:section.value("controls",Json::array())) if(c.value("id",std::string{})==id) {
            if(!interactive(c)) { error_="That action is not available right now."; dirty_=true; return; }
            Json event={{"id",id}};
            if(c.contains("confirm")) { confirm_={{"event",event},{"message",c.at("confirm")}}; dialog_focus_=0; dirty_=true; }
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
        row_=0; dirty_=true; enter_=true; return;
    }
    if(!count) return;
    const auto& controls=sections[section_]["controls"]; const int rows=int(controls.size());
    if(name=="row_delta") { row_=std::clamp(row_+action.at("delta").get<int>(),0,std::max(0,rows-1)); dirty_=true; return; }
    if(name=="row") { row_=std::clamp(action.at("row").get<int>(),0,std::max(0,rows-1)); dirty_=true; return; }
    const auto* c=current_control(); if(!c || !interactive(*c)) return;
    const auto type=c->at("type").get<std::string>();
    if(type=="choice" && (name=="pick" || (name=="activate" && c->at("options").size()>8))) {
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
        else if(name=="value" && (type=="radio" || type=="choice" || type=="slider" || type=="number")) event["value"]=(type=="slider"||type=="number")?Json(snap_value(*c,action.at("value").get<double>())):action.at("value");
        else return;
    } else if(name=="activate" || name=="adjust") {
        if(type=="toggle") event["value"]=!c->at("value").get<bool>();
        else if(adjustable(*c) && (name=="adjust" || type=="choice" || type=="radio")) event["value"]=adjusted_value(*c,action.value("delta",1));
        else if(type=="text" && name=="activate") event["value"]=text_of(name_input_.Get(),4096);
        else if(type!="button" || name!="activate") return;
    } else return;
    if(c->contains("confirm")) { confirm_={{"event",event},{"message",c->at("confirm")}}; dialog_focus_=0; dirty_=true; return; }
    send_event(event);
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
Json Menu::diagnostics() const {
    Json bindings=Json::object(); for(const auto& b:bindings_) bindings[b.action]=b.keys;
    int page_widgets=0, nested=0;
    for(const auto* stack:{&tab_items_,&list_,&head_,&panel_,&actions_,&footer_}) { page_widgets+=int(stack->used); for(const auto& cell:stack->cells) nested+=int(cell.kinds.size()); }
    return {{"bindings",bindings},{"open",active_},{"attached",page_.Get()!=nullptr},{"skeleton",design_.Get()!=nullptr},{"tab_index",tab_index_},
            {"screen",screen_==Screen::Library?"library":screen_==Screen::Extension?"extension":"settings"},{"extension",extension_id_},
            {"section",section_},{"row",row_},{"library_row",library_row_},{"picker",picker_},{"confirm",!confirm_.is_null()},
            {"error",error_},{"hits",hits_.size()},{"widgets",cost_.widgets},{"page_widgets",page_widgets},{"nested_widgets",nested},{"created",cost_.created},
            {"builds",cost_.builds},{"last_build_us",cost_.last_build_us},{"max_build_us",cost_.max_build_us},{"viewport",viewport_},{"gamepad",gamepad_},{"textures",textures_.size()}};
}
}

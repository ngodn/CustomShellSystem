// Photo Mode for extensions: see photo.hpp. Every engine name used here was read live on the
// 23 Oct 2026 build (docs: extensions/cine/docs/PHOTO-MODE-RESEARCH.md, live facts).
#include "photo.hpp"
#include <windows.h>
#include <algorithm>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/FString.hpp>

namespace cssx {
using namespace engine;
namespace {
constexpr const char* system_row_class="/Game/Sparta/UI/Menu/WBP_NB_System.WBP_NB_System_C";
constexpr const char* tab_class="/Game/Sparta/UI/PhotoMode/Widgets/WBP_NB_Menu_Image.WBP_NB_Menu_Image_C";
// A generic page: the same 29-function template as the Lens, Brightness, Colour, FX, Visibility
// and Fog pages. Its rows are replaced; its navigation, highlight, scrolling and fade stay.
constexpr const char* page_class="/Game/Sparta/UI/PhotoMode/Widgets/WBP_MOT_Photomode_LensSettings.WBP_MOT_Photomode_LensSettings_C";
constexpr const char* row_class="/Game/Sparta/UI/PhotoMode/Widgets/WBP_NB_Photomode.WBP_NB_Photomode_C";
constexpr const char* slider_class="/Game/Sparta/UI/PhotoMode/Widgets/WBP_NB_Photomode_Slider.WBP_NB_Photomode_Slider_C";
constexpr uint8_t state_selected=2, state_confirmed=3;           // ENavigableState
constexpr uint8_t vis_collapsed=1, vis_hidden=2, vis_self_passive=4;  // ESlateVisibility
constexpr int rows_per_tick=4;                                   // a game widget costs about a millisecond to create
constexpr uint64_t model_every_ms=250, confirm_window_ms=3000;

uint64_t now_ms() { return GetTickCount64(); }
UClass* widget_class(const char* path) {
    auto* type=load(path);
    if(!type) throw std::runtime_error(std::string("Game widget class is missing: ")+path);
    return static_cast<UClass*>(type);
}
void visible(UObject* widget,uint8_t value) { if(widget) invoke(widget,L"SetVisibility",L"InVisibility",value); }
UObject* nav_of(UObject* panel) { return object_of(panel,L"NavigationObject"); }
void rescan(UObject* panel) { if(auto* nav=nav_of(panel)) invoke(nav,L"GetNavigableChildren"); }
int active_index(UObject* panel) { auto* nav=nav_of(panel); return nav?read<int32_t>(nav,L"ActiveIndex"):-1; }
void navigate(UObject* panel,int index) {
    auto* nav=nav_of(panel); if(!nav) return;
    Call c(nav,L"NavigateToCustomIndex",3); c.set(L"Index",int32_t{index}); c.set(L"SubIndex",int32_t{0}); c.run();
}
int child_index(UObject* panel,UObject* child) {
    Call c(panel,L"GetChildIndex",2); c.set(L"content",child); c.run(); return c.get<int32_t>();
}
void add_vertical(UObject* box,UObject* child) { Call c(box,L"AddChildToVerticalBox",2); c.set(L"content",child); c.run(); }
void add_horizontal(UObject* box,UObject* child) { Call c(box,L"AddChildToHorizontalBox",2); c.set(L"content",child); c.run(); }
void add_child(UObject* panel,UObject* child) { Call c(panel,L"AddChild",2); c.set(L"content",child); c.run(); }
void remove(UObject* widget) { if(widget) try { invoke(widget,L"RemoveFromParent"); } catch(...) {} }
bool visible_now(UObject* widget) { Call c(widget,L"IsVisible",1); c.run(); return c.get<bool>(); }
}

void PhotoMode::report(const std::string& message) {
    if(message==last_error_) return;
    last_error_=message;
    if(deps_.log) deps_.log("Photo Mode: "+message);
}

void PhotoMode::set_runtime(Runtime* runtime) {
    if(runtime==deps_.runtime) return;
    detach();
    deps_.runtime=runtime; library_revision_=0; declared_.clear();
}

void PhotoMode::refresh_declared() {
    if(!deps_.runtime || deps_.runtime->revision()==library_revision_) return;
    library_revision_=deps_.runtime->revision();
    declared_.clear();
    for(const auto* m:deps_.runtime->photo_extensions()) declared_.push_back({m->id,m->photo_entry,m->photo_tabs});
    if(!session_.empty() && std::none_of(declared_.begin(),declared_.end(),[&](const Declared& d){ return d.id==session_; })) end_session();
    // An extension that left (suspended, removed) loses its pause row.
    for(auto it=pause_rows_.begin();it!=pause_rows_.end();) {
        if(std::none_of(declared_.begin(),declared_.end(),[&](const Declared& d){ return d.id==it->id; })) { remove(it->row.Get()); it=pause_rows_.erase(it); }
        else ++it;
    }
}

void PhotoMode::tick(const PlayerContext& player,double) {
    if(!deps_.runtime) return;
    refresh_declared();
    if(declared_.empty() && session_.empty() && pause_rows_.empty()) return;
    if(!player.pc) { if(!session_.empty() || !pause_rows_.empty()) detach(); return; }
    if(pc_.Get()!=player.pc) {
        detach();
        pc_=player.pc;
        handler_=object_of(player.pc,L"User Interface Handler Component");
        component_=object_of(player.pc,L"Photography Mode Component");
        keys_.clear(); keys_retry_=0;
    }
    auto* handler=handler_.Get(); auto* component=component_.Get();
    if(!handler || !component) return;
    try {
        if(!declared_.empty() && bool_of(handler,L"bIsInGameMenu")) pause_tick(handler);
        if(!session_.empty()) panel_tick(component);
    } catch(const std::exception& e) {
        report(e.what());
        end_session();
    }
}

// ---------------------------------------------------------------- pause menu

UObject* PhotoMode::ensure_pause_row(PauseRow& p,UObject* box,UObject* reference,UObject* after) {
    auto* row=p.row.Get();
    if(!row) {
        row=create_widget(pc_.Get(),widget_class(system_row_class));
        if(!row) throw std::runtime_error("The game would not create a System row");
        for(auto name:{L"DefaultTextColor",L"HighlightTextColor",L"DefaultButtonColor",L"HighlightButtonColor"}) copy_property(row,reference,name);
        const auto& d=*std::find_if(declared_.begin(),declared_.end(),[&](const Declared& x){ return x.id==p.id; });
        text_property(row,L"Text",d.entry);
        invoke(row,L"InitText");
        p.row=row;
    }
    if(child_index(box,row)>=0) return row;
    // Re-add the column with this row after `after`. UPanelWidget.InsertChildAt is not callable
    // in shipping; the System rows' slots have no overrides, so re-adding keeps the layout.
    auto list=children(box);
    list.erase(std::remove(list.begin(),list.end(),row),list.end());
    auto at=std::find(list.begin(),list.end(),after);
    if(at==list.end()) throw std::runtime_error("The System page has no Activate Photo Mode row");
    list.insert(at+1,row);
    invoke(box,L"ClearChildren");
    for(auto* child:list) add_vertical(box,child);
    rescan(box);
    return row;
}

void PhotoMode::pause_tick(UObject* handler) {
    auto* page=object_of(object_of(object_of(handler,L"WBP_Menu_Game"),L"WBP_Menu_Options"),L"WBP_MOT_System");
    if(!page || !visible_now(page)) { for(auto& p:pause_rows_) p.down=false; return; }
    system_page_=page;
    auto* box=object_of(page,L"BP_VBC_System"); auto* photo=object_of(page,L"WBP_NBS_ActivatePhotomode");
    if(!box || !photo) throw std::runtime_error("The System page layout changed");
    if(keys_.empty() && now_ms()>=keys_retry_) {
        try { keys_=read_menu_bindings(pc_.Get(),handler); } catch(const std::exception& e) { keys_retry_=now_ms()+1000; report(e.what()); }
    }
    bool accept=false;
    for(const auto& b:keys_) if(b.action=="accept") for(const auto& k:b.keys) if(key_down(pc_.Get(),k)) { accept=true; break; }
    const bool enabled=bool_of(photo,L"bEnabled",true);
    UObject* after=photo;
    for(const auto& d:declared_) {
        if(d.entry.empty()) continue;
        auto it=std::find_if(pause_rows_.begin(),pause_rows_.end(),[&](const PauseRow& p){ return p.id==d.id; });
        if(it==pause_rows_.end()) { pause_rows_.push_back({d.id,{},false}); it=pause_rows_.end()-1; }
        auto* row=ensure_pause_row(*it,box,photo,after);
        after=row;
        // Available exactly when the game's own Photo Mode row is (IsPlayerInterfaceEnabled).
        if(bool_of(row,L"bEnabled",true)!=enabled) { raw_value(row,L"bEnabled",enabled); try { invoke(row,L"UpdateEnabledState"); } catch(...) {} }
        const auto state=read<uint8_t>(row,L"NavigableState");
        // The game's navigation confirms a row by state (keys and clicks); the accept key on the
        // selected row is read too, in case a state change is undone within the same frame.
        const bool pressed=state==state_confirmed || (state==state_selected && accept && !it->down);
        it->down=accept;
        if(enabled && pressed) { begin_session(d.id,page); return; }
    }
}

void PhotoMode::begin_session(const std::string& id,UObject* system_page) {
    auto* handler=handler_.Get(); auto* component=component_.Get();
    if(!handler || !component) return;
    // The game's own chain for Activate Photo Mode (WBP_MOT_System.HandlePhotomodeButton).
    invoke(system_page,L"OnMenuClose");
    { Call c(handler,L"HandleOptionsMenu",2); c.set(L"SubTabIndex",int32_t{0}); c.set(L"AllowClose",true); c.run(); }
    invoke(component,L"ActivatePlayerMode");
    session_=id; session_started_=now_ms(); session_live_=false; active_tab_=-1; model_=nullptr;
    if(deps_.log) deps_.log("Photo Mode opened for "+id);
}

void PhotoMode::end_session() noexcept {
    if(session_.empty()) return;
    const bool told=panel_.Get()!=nullptr || !tabs_.empty();
    detach_panel();
    const auto id=session_;
    session_.clear(); session_live_=false; model_=nullptr; pending_confirm_.clear();
    if(told && deps_.runtime) { try { deps_.runtime->host_event(id,"photo_mode","closed"); } catch(...) {} }
    if(deps_.log) try { deps_.log("Photo Mode closed for "+id); } catch(...) {}
}

// ---------------------------------------------------------------- panel

void PhotoMode::panel_tick(UObject* component) {
    if(!bool_of(component,L"bIsPhotoModeActive")) {
        // Closed, or never opened (the game refused): give it five seconds to start.
        if(session_live_ || now_ms()-session_started_>5000) end_session();
        return;
    }
    session_live_=true;
    auto* panel=object_of(component,L"PhotoModeUI");
    if(!panel) return;
    if(panel_.Get()!=panel || tabs_.empty() || !tabs_.front().tab.Get() || child_index(strip_.Get(),tabs_.front().tab.Get())<0) {
        const bool first=panel_.Get()==nullptr && tabs_.empty();
        detach_panel();
        if(!attach(panel)) { end_session(); return; }
        if(first) {
            try { deps_.runtime->host_event(session_,"photo_mode","open"); } catch(const std::exception& e) { report(e.what()); }
            navigate(strip_.Get(),game_tabs_);   // open on the extension's first tab
        }
    }
    auto* strip=strip_.Get(); if(!strip) return;
    const int index=active_index(strip);
    if(index!=active_tab_) {
        active_tab_=index; model_at_=0;
        if(auto* scroll=scroll_.Get()) if(index>=0) {
            const auto list=children(strip);
            if(index<int(list.size())) {
                Call c(scroll,L"ScrollWidgetIntoView",4); c.set(L"WidgetToFind",list[index]); c.set(L"AnimateScroll",true);
                c.set(L"ScrollDestination",uint8_t{0}); c.set(L"Padding",40.f); c.run();
            }
        }
    }
    const int ours=active_tab_-game_tabs_;
    if(ours<0 || ours>=int(tabs_.size())) return;
    auto& tab=tabs_[size_t(ours)];
    const auto now=now_ms();
    if(model_.is_null() || now-model_at_>=model_every_ms) {
        model_at_=now;
        try { model_=deps_.runtime->model(session_); } catch(const std::exception& e) { report(e.what()); return; }
    }
    build_page(tab);
    if(bool_of(component,L"bIsPlayerUserInterfaceInFocus")) poll_keys(tab);
    else for(auto& [k,v]:key_down_) v=true;   // a key held while switching focus must not fire
}

UObject* PhotoMode::icon_texture(const fs::path& file) {
    Call import(find_cached(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
    import.set(L"WorldContextObject",pc_.Get()); import.set(L"Filename",FString(file.c_str())); import.run();
    return import.get<UObject*>();
}

bool PhotoMode::attach(UObject* panel) {
    auto declared=std::find_if(declared_.begin(),declared_.end(),[&](const Declared& d){ return d.id==session_; });
    if(declared==declared_.end()) return false;
    auto* strip=object_of(panel,L"BP_HBC_Menu_Options"); auto* switcher=object_of(panel,L"WS_Tabs");
    if(!strip || !switcher) { report("the Photo Mode panel layout changed; tabs not added"); return false; }
    const auto game=children(strip);
    if(game.empty()) return false;
    auto* reference=game.front();
    Call desired(strip,L"GetDesiredSize",1); desired.run();
    const auto width=desired.get<Vec2>().x;   // the game's own strip width, kept when it scrolls
    panel_=panel; strip_=strip; switcher_=switcher; game_tabs_=int(game.size());
    auto* tab_type=widget_class(tab_class); auto* page_type=widget_class(page_class);
    for(const auto& t:declared->tabs) {
        Tab tab; tab.section=t.section;
        auto* icon=create_widget(pc_.Get(),tab_type);
        for(auto name:{L"ImageStretchMode",L"SelectedColor",L"DefaultColor",L"RootSize",L"RootScale",L"HighlightY",L"bUseHighlight",L"IconSizeX",L"IconSizeY",L"HighlightColor"})
            copy_property(icon,reference,name);
        if(auto* texture=icon_texture(t.icon)) { object_property(icon,L"IconTexture",texture); tab.icon=texture; }
        else report("tab icon did not load: "+path_utf8(t.icon));
        for(auto fn:{L"UpdateImage",L"CommitSize",L"CommitScale"}) invoke(icon,fn);
        add_horizontal(strip,icon);
        if(auto* slot=object_of(icon,L"Slot")) if(auto* ref_slot=object_of(reference,L"Slot"))
            invoke(slot,L"SetPadding",L"InPadding",read<std::array<float,4>>(ref_slot,L"Padding"));   // the game's own tab spacing
        auto* page=create_widget(pc_.Get(),page_type);
        auto* box=object_of(page,L"BP_VBC_PhotomodeGeneral");
        if(!box) { remove(icon); report("the Photo Mode page layout changed; tabs not added"); detach_panel(); return false; }
        invoke(box,L"ClearChildren");
        add_child(switcher,page);
        tab.tab=icon; tab.page=page; tab.box=box;
        tabs_.push_back(std::move(tab));
    }
    rescan(strip);
    wrap_strip(panel);
    if(auto* size=size_box_.Get()) invoke(size,L"SetWidthOverride",L"InWidthOverride",float(width));
    return true;
}

void PhotoMode::wrap_strip(UObject* panel) {
    auto* strip=strip_.Get(); auto* slot=object_of(strip,L"Slot"); auto* row=object_of(slot,L"Parent");
    auto* tree=object_of(panel,L"WidgetTree");
    if(!row || !tree) { report("the tab strip cannot scroll (layout changed)"); return; }
    strip_siblings_.clear();
    for(auto* child:children(row)) {
        auto* s=object_of(child,L"Slot"); SlotLook look;
        if(s) { look.padding=read<std::array<float,4>>(s,L"Padding"); look.size=read<std::array<std::byte,8>>(s,L"Size");
                look.horizontal=read<uint8_t>(s,L"HorizontalAlignment"); look.vertical=read<uint8_t>(s,L"VerticalAlignment"); }
        strip_siblings_.push_back({child,look});
    }
    auto* size=construct(L"/Script/UMG.SizeBox",tree); auto* scroll=construct(L"/Script/UMG.ScrollBox",tree);
    invoke(scroll,L"SetOrientation",L"NewOrientation",uint8_t{0});            // horizontal
    invoke(scroll,L"SetScrollBarVisibility",L"NewScrollBarVisibility",uint8_t{vis_collapsed});
    content(size,scroll);
    invoke(row,L"ClearChildren");
    for(const auto& [child,look]:strip_siblings_) {
        auto* widget=child.Get(); if(!widget) continue;
        auto* placed=widget==strip?size:widget;
        add_horizontal(row,placed);
        if(auto* s=object_of(placed,L"Slot")) {
            invoke(s,L"SetPadding",L"InPadding",look.padding); invoke(s,L"SetSize",L"InSize",look.size);
            invoke(s,L"SetHorizontalAlignment",L"InHorizontalAlignment",look.horizontal); invoke(s,L"SetVerticalAlignment",L"InVerticalAlignment",look.vertical);
        }
    }
    add_child(scroll,strip);
    strip_box_=row; scroll_=scroll; size_box_=size;
}

void PhotoMode::unwrap_strip() noexcept {
    auto* row=strip_box_.Get(); auto* strip=strip_.Get();
    if(row && strip && !strip_siblings_.empty()) try {
        remove(strip);
        invoke(row,L"ClearChildren");
        for(const auto& [child,look]:strip_siblings_) {
            auto* widget=child.Get(); if(!widget) continue;
            add_horizontal(row,widget);
            if(auto* s=object_of(widget,L"Slot")) {
                invoke(s,L"SetPadding",L"InPadding",look.padding); invoke(s,L"SetSize",L"InSize",look.size);
                invoke(s,L"SetHorizontalAlignment",L"InHorizontalAlignment",look.horizontal); invoke(s,L"SetVerticalAlignment",L"InVerticalAlignment",look.vertical);
            }
        }
    } catch(...) {}
    strip_siblings_.clear(); strip_box_.Reset(); scroll_.Reset(); size_box_.Reset();
}

void PhotoMode::detach_panel() noexcept {
    try {
        if(auto* strip=strip_.Get()) if(active_tab_>=game_tabs_ && game_tabs_>0) navigate(strip,0);
    } catch(...) {}
    for(auto& t:tabs_) { remove(t.tab.Get()); remove(t.page.Get()); }
    tabs_.clear();
    unwrap_strip();
    try { if(auto* strip=strip_.Get()) rescan(strip); } catch(...) {}
    panel_.Reset(); strip_.Reset(); switcher_.Reset(); game_tabs_=0; active_tab_=-1;
}

void PhotoMode::detach() noexcept {
    end_session();
    for(auto& p:pause_rows_) remove(p.row.Get());
    try { if(auto* page=system_page_.Get()) if(auto* box=object_of(page,L"BP_VBC_System")) rescan(box); } catch(...) {}
    pause_rows_.clear(); system_page_.Reset();
}

void PhotoMode::forget() noexcept {
    pause_rows_.clear(); system_page_.Reset(); tabs_.clear(); strip_siblings_.clear();
    panel_.Reset(); strip_.Reset(); switcher_.Reset(); strip_box_.Reset(); scroll_.Reset(); size_box_.Reset();
    pc_.Reset(); handler_.Reset(); component_.Reset(); session_.clear(); keys_.clear();
}

// ---------------------------------------------------------------- rows

void PhotoMode::build_page(Tab& tab) {
    auto* box=tab.box.Get(); if(!box) return;
    auto rows=photo_rows(model_,tab.section);
    // Pending confirmation shows on its row until it runs out.
    if(!pending_confirm_.empty() && now_ms()>confirm_until_) pending_confirm_.clear();
    for(auto& r:rows) if(r.id==pending_confirm_) r.value="Confirm again";
    // Pool: one widget per row and kind, created a few per tick so a big tab never hitches.
    std::vector<std::pair<bool,size_t>> order; size_t options=0, sliders=0; int budget=rows_per_tick;
    for(const auto& r:rows) {
        auto& pool=r.slider?tab.slider_rows:tab.option_rows; auto& n=r.slider?sliders:options;
        if(n>=pool.size()) {
            if(budget--<=0) break;
            Row row; row.slider=r.slider;
            row.widget=create_widget(pc_.Get(),widget_class(r.slider?slider_class:row_class));
            pool.push_back(std::move(row));
        }
        order.push_back({r.slider,n++});
    }
    if(order!=tab.order) {
        invoke(box,L"ClearChildren");
        for(const auto& [slider,i]:order) if(auto* w=(slider?tab.slider_rows:tab.option_rows)[i].widget.Get()) add_vertical(box,w);
        tab.order=order;
        rescan(box);
    }
    const int selected=active_index(box);
    for(size_t i=0;i<order.size();++i) {
        auto& row=(order[i].first?tab.slider_rows:tab.option_rows)[order[i].second];
        fill(row,rows[i],int(i)==selected);
    }
    tab.rows.assign(rows.begin(),rows.begin()+std::ptrdiff_t(order.size()));
    tab.selected=selected;
}

void PhotoMode::fill(Row& row,const PhotoRow& value,bool) {
    auto* w=row.widget.Get(); if(!w) return;
    if(row.fresh || row.shown.name!=value.name) {
        text_property(w,L"Text_OptionName",value.name);
        invoke(w,L"InitText");
    }
    if(row.fresh || row.shown.value!=value.value) if(auto* block=object_of(w,L"Text_Option_Value")) text_value(block,value.value);
    if(row.fresh || row.shown.arrows!=value.arrows) {
        // Hidden, not collapsed: the value keeps its place in the row.
        visible(object_of(w,L"WBP_ArrowButton_L"),value.arrows?vis_self_passive:vis_hidden);
        visible(object_of(w,L"WBP_ArrowButton_R"),value.arrows?vis_self_passive:vis_hidden);
    }
    if(row.slider && (row.fresh || row.shown.fill!=value.fill)) if(auto* bar=object_of(w,L"WBP_GenericBar")) {
        try { invoke(bar,L"SetPercent",L"InPercent",float(value.fill)); } catch(...) {}
    }
    if(row.fresh || row.shown.enabled!=value.enabled) { try { invoke(w,L"ToggleButtonGrayout",L"Grayout",!value.enabled); } catch(...) {} }
    row.shown=value; row.fresh=false;
}

void PhotoMode::poll_keys(Tab& tab) {
    if(keys_.empty()) {
        if(now_ms()<keys_retry_) return;
        try { keys_=read_menu_bindings(pc_.Get(),handler_.Get()); } catch(const std::exception& e) { keys_retry_=now_ms()+1000; report(e.what()); return; }
    }
    for(const auto& b:keys_) {
        if(b.action!="left" && b.action!="right" && b.action!="accept") continue;
        bool down=false;
        for(const auto& k:b.keys) if(key_down(pc_.Get(),k)) { down=true; break; }
        auto& was=key_down_[b.action];
        const auto now=now_ms();
        const bool repeat=b.action!="accept" && down && was && now>=repeat_at_;
        if(down && (!was || repeat)) { repeat_at_=now+(was?90:400); was=down; press(tab,b.action); return; }
        was=down;
    }
}

void PhotoMode::press(Tab& tab,const std::string& key) {
    if(tab.selected<0 || tab.selected>=int(tab.rows.size())) return;
    const auto& row=tab.rows[size_t(tab.selected)];
    const auto* control=photo_control(model_,tab.section,row.id);
    if(!control) return;
    bool confirmed=false;
    if(control->contains("confirm") && interactive(*control)) {
        const auto now=now_ms();
        const bool would_send=!photo_event(*control,key,true).is_null();
        if(!would_send) return;
        if(pending_confirm_!=row.id || now>confirm_until_) { pending_confirm_=row.id; confirm_until_=now+confirm_window_ms; model_at_=0; return; }
        confirmed=true; pending_confirm_.clear();
    }
    const auto event=photo_event(*control,key,confirmed);
    if(event.is_null()) return;
    try { deps_.runtime->event(session_,event); last_error_.clear(); }
    catch(const std::exception& e) { report(e.what()); }
    model_at_=0;   // show the result on the next tick
}

Json PhotoMode::diagnostics() const {
    Json tabs=Json::array();
    for(const auto& t:tabs_) tabs.push_back({{"section",t.section},{"rows",t.rows.size()},{"selected",t.selected},{"widgets",t.option_rows.size()+t.slider_rows.size()}});
    Json declared=Json::array();
    for(const auto& d:declared_) declared.push_back({{"id",d.id},{"entry",d.entry},{"tabs",d.tabs.size()}});
    return {{"declared",declared},{"pause_rows",pause_rows_.size()},{"session",session_},{"live",session_live_},
            {"attached",panel_.Get()!=nullptr},{"game_tabs",game_tabs_},{"active_tab",active_tab_},{"tabs",tabs},{"error",last_error_}};
}
}

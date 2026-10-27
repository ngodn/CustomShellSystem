// Photo Mode for extensions: see photo.hpp. Every engine name used here was read live on the
// 23 Oct 2026 build (docs: extensions/cine/docs/PHOTO-MODE-RESEARCH.md, live facts).
#include "photo.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
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
constexpr uint64_t check_every_ms=250, settle_ms=1000, confirm_window_ms=3000;

uint64_t now_ms() { return GetTickCount64(); }
uint64_t clock_us() {
    static const double per_us=[]{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return double(f.QuadPart)/1e6; }();
    LARGE_INTEGER t; QueryPerformanceCounter(&t); return uint64_t(double(t.QuadPart)/per_us);
}
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
    Call c(nav,L"NavigateToCustomIndex",4); c.set(L"Index",int32_t{index}); c.set(L"SubIndex",int32_t{0}); c.run();   // + Success, ReturnValue
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
        if(bool_of(row,L"bEnabled",true)!=enabled) { set_bool(row,L"bEnabled",enabled); try { invoke(row,L"UpdateEnabledState",L"bEnabled",enabled); } catch(const std::exception& e) { report(e.what()); } }
        const auto state=read<uint8_t>(row,L"NavigableState");
        // The game's navigation confirms a row by state (keys and clicks); the accept key on the
        // selected row is read too, in case a state change is undone within the same frame.
        const bool pressed=state==state_confirmed || (state==state_selected && accept && !it->down);
        it->down=accept;
        if(enabled && pressed) { begin_session(d.id,page); return; }
    }
    prefetch_icons();
}

void PhotoMode::begin_session(const std::string& id,UObject* system_page) {
    auto* handler=handler_.Get(); auto* component=component_.Get();
    if(!handler || !component) return;
    // The game's own chain for Activate Photo Mode (WBP_MOT_System.HandlePhotomodeButton).
    invoke(system_page,L"OnMenuClose");
    { Call c(handler,L"HandleOptionsMenu",2); c.set(L"SubTabIndex",int32_t{0}); c.set(L"AllowClose",true); c.run(); }
    invoke(component,L"ActivatePlayerMode");
    session_=id; session_started_=now_ms(); session_live_=false; announced_=false; active_tab_=-1; model_=nullptr; rows_dirty_=true;
    if(deps_.log) deps_.log("Photo Mode opened for "+id);
}

void PhotoMode::dev_open(const std::string& id) {
    if(std::none_of(declared_.begin(),declared_.end(),[&](const Declared& d){ return d.id==id; }))
        throw std::runtime_error("no loaded extension declares photo_mode: "+id);
    auto* component=component_.Get();
    if(!component) throw std::runtime_error("no player yet");
    end_session();
    // Joins a Photo Mode that is already open; otherwise opens it the way the game's camera key does.
    if(!bool_of(component,L"bIsPhotoModeActive")) invoke(component,L"ActivatePlayerMode");
    session_=id; session_started_=now_ms(); session_live_=false; announced_=false; active_tab_=-1; model_=nullptr; rows_dirty_=true;
    if(deps_.log) deps_.log("Photo Mode opened for "+id+" (dev)");
}

void PhotoMode::dev_press(const std::string& key) {
    if(key!="left" && key!="right" && key!="accept") throw std::runtime_error("key is left, right or accept");
    const int ours=active_tab_-game_tabs_;
    if(session_.empty() || ours<0 || ours>=int(tabs_.size())) throw std::runtime_error("no extension tab is showing");
    press(tabs_[size_t(ours)],key);
}

void PhotoMode::end_session() noexcept {
    if(session_.empty()) return;
    const bool told=announced_;
    announced_=false;
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
    const auto now=now_ms();
    const bool slow=now>=slow_at_;   // checks that only need to run four times a second
    if(slow) slow_at_=now+check_every_ms;
    // A new panel, or the game rebuilt the strip: start over.
    if(panel_.Get()!=panel || (!tabs_.empty() && (!tabs_.front().tab.Get() || (slow && child_index(strip_.Get(),tabs_.front().tab.Get())<0)))) {
        detach_panel();
        const auto attached=attach(panel);
        if(attached==Attach::failed) { end_session(); return; }
        if(attached==Attach::later) {
            if(now_ms()-session_started_>5000) { report("the Photo Mode tab strip never laid out; tabs not added"); end_session(); }
            return;
        }
    }
    {
        auto declared=std::find_if(declared_.begin(),declared_.end(),[&](const Declared& d){ return d.id==session_; });
        if(declared==declared_.end()) { end_session(); return; }
        if(!grow(*declared)) return;
    }
    if(!announced_) {
        announced_=true;
        try { deps_.runtime->host_event(session_,"photo_mode","open"); } catch(const std::exception& e) { report(e.what()); }
        try { navigate(strip_.Get(),game_tabs_); } catch(const std::exception& e) { report(e.what()); }   // open on the extension's first tab
    }
    auto* strip=strip_.Get(); if(!strip) return;
    const int index=active_index(strip);
    if(index!=active_tab_) {
        active_tab_=index; rows_dirty_=true;
        // Scroll by whole tabs so no icon is ever cut at the strip's edge: the window of tabs
        // that fit moves only as far as needed to show the active one.
        if(auto* scroll=scroll_.Get()) if(index>=0 && tab_pitch_>0) {
            const int fit=std::max(1,int(std::floor(strip_width_/tab_pitch_+0.01)));
            const int total=game_tabs_+int(tabs_.size());
            if(index<first_tab_) first_tab_=index;
            else if(index>=first_tab_+fit) first_tab_=index-fit+1;
            first_tab_=std::clamp(first_tab_,0,std::max(0,total-fit));
            invoke(scroll,L"SetScrollOffset",L"NewScrollOffset",float(first_tab_*tab_pitch_));
        }
    }
    const int ours=active_tab_-game_tabs_;
    const bool on_ours=ours>=0 && ours<int(tabs_.size());
    if(!on_ours) return;
    auto& tab=tabs_[size_t(ours)];
    // The rows follow the extension's model, which changes only when an extension says so.
    const auto revision=deps_.runtime->revision();
    if(model_.is_null() || revision!=model_revision_) {
        try { model_=deps_.runtime->model(session_); } catch(const std::exception& e) { report(e.what()); return; }
        model_revision_=revision; rows_dirty_=true;
    }
    if(!pending_confirm_.empty() && now>confirm_until_) { pending_confirm_.clear(); rows_dirty_=true; }   // "Confirm again" ran out
    build_page(tab,slow);
    if(bool_of(component,L"bIsPlayerUserInterfaceInFocus")) poll_keys(tab);
    else for(auto& [k,v]:key_down_) v=true;   // a key held while switching focus must not fire
}

// Tab icons are imported once and reused while the engine keeps them; a texture the garbage
// collector took is imported again.
UObject* PhotoMode::icon_texture(const fs::path& file) {
    auto& cached=icons_[file.native()];
    if(auto* texture=cached.Get()) return texture;
    Call import(find_cached(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
    import.set(L"WorldContextObject",pc_.Get()); import.set(L"Filename",FString(file.c_str())); import.run();
    auto* texture=import.get<UObject*>();
    if(texture) cached=texture;
    return texture;
}

// While the pause menu shows an extension's row, import one of its tab icons a frame, so opening
// the extension's Photo Mode does not decode them all at once.
void PhotoMode::prefetch_icons() {
    for(const auto& d:declared_) for(const auto& t:d.tabs) {
        if(auto it=icons_.find(t.icon.native()); it!=icons_.end() && it->second.Get()) continue;
        try { icon_texture(t.icon); } catch(const std::exception& e) { report(e.what()); }
        return;
    }
}

PhotoMode::Attach PhotoMode::attach(UObject* panel) {
    auto declared=std::find_if(declared_.begin(),declared_.end(),[&](const Declared& d){ return d.id==session_; });
    if(declared==declared_.end()) return Attach::failed;
    auto* strip=object_of(panel,L"BP_HBC_Menu_Options"); auto* switcher=object_of(panel,L"WS_Tabs");
    if(!strip || !switcher) { report("the Photo Mode panel layout changed; tabs not added"); return Attach::failed; }
    const auto game=children(strip);
    if(game.empty()) return Attach::later;
    // The game's own strip width, kept when it scrolls. A panel the game has just created has no
    // layout yet: measure it now, and if it still reads zero, try again next tick.
    attach_cost_={};
    const auto measure=clock_us();
    invoke(strip,L"ForceLayoutPrepass");
    Call desired(strip,L"GetDesiredSize",1); desired.run();
    const auto width=desired.get<Vec2>().x;
    attach_cost_.steps.push_back({"measure",clock_us()-measure});
    if(!(width>1)) return Attach::later;
    panel_=panel; strip_=strip; switcher_=switcher; game_tabs_=int(game.size());
    pages_ready_=0;
    const auto started=clock_us();
    wrap_strip(panel);
    attach_cost_.steps.push_back({"wrap",clock_us()-started});
    if(auto* size=size_box_.Get()) invoke(size,L"SetWidthOverride",L"InWidthOverride",float(width));
    strip_width_=float(width); tab_pitch_=float(width)/float(game_tabs_); first_tab_=0;   // the game's tabs are all alike
    return Attach::done;
}

// The extension's tabs go in one widget a frame (each costs a few milliseconds): first every
// tab icon, then every page. True once all are in.
bool PhotoMode::grow(const Declared& declared) {
    const auto started=clock_us();
    auto* strip=strip_.Get(); auto* switcher=switcher_.Get();
    if(!strip || !switcher) return false;
    if(tabs_.size()<declared.tabs.size()) {
        const auto& t=declared.tabs[tabs_.size()];
        auto* reference=children(strip).front();
        Tab tab; tab.section=t.section;
        auto* icon=create_widget(pc_.Get(),widget_class(tab_class));
        for(auto name:{L"ImageStretchMode",L"SelectedColor",L"DefaultColor",L"RootSize",L"RootScale",L"HighlightY",L"bUseHighlight",L"IconSizeX",L"IconSizeY",L"HighlightColor"})
            copy_property(icon,reference,name);
        if(auto* texture=icon_texture(t.icon)) { object_property(icon,L"IconTexture",texture); tab.icon=texture; }
        else report("tab icon did not load: "+path_utf8(t.icon));
        for(auto fn:{L"UpdateImage",L"CommitSize",L"CommitScale"}) invoke(icon,fn);
        add_horizontal(strip,icon);
        if(auto* slot=object_of(icon,L"Slot")) if(auto* ref_slot=object_of(reference,L"Slot"))
            invoke(slot,L"SetPadding",L"InPadding",read<std::array<float,4>>(ref_slot,L"Padding"));   // the game's own tab spacing
        tab.tab=icon;
        tabs_.push_back(std::move(tab));
        rescan(strip);
        attach_cost_.steps.push_back({"tab",clock_us()-started});
        return false;
    }
    if(pages_ready_<tabs_.size()) {
        auto& tab=tabs_[pages_ready_];
        auto* page=create_widget(pc_.Get(),widget_class(page_class));
        auto* box=object_of(page,L"BP_VBC_PhotomodeGeneral");
        if(!box) throw std::runtime_error("the Photo Mode page layout changed; tabs not added");
        if(pages_ready_==0) learn_row_looks(box);
        invoke(box,L"ClearChildren");
        add_child(switcher,page);
        // The player's text size setting, which the game applies to its own pages when they open.
        try { Call scale(page,L"RefreshUIScale",1); scale.set(L"bPreview",false); scale.run(); } catch(const std::exception& e) { report(e.what()); }
        tab.page=page; tab.box=box;
        ++pages_ready_;
        attach_cost_.steps.push_back({"page",clock_us()-started});
        return false;
    }
    return true;
}

// The game's own rows on the page CSSX clones carry per-row settings that differ from the row
// class defaults: highlight style and select-on-hover.
// Learned from those rows before they are cleared, so CSSX rows always match the shipped game.
void PhotoMode::learn_row_looks(UObject* box) {
    option_look_={}; slider_look_={};
    auto* option_type=widget_class(row_class); auto* slider_type=widget_class(slider_class);
    for(auto* r:children(box)) {
        auto* type=r->GetClassPrivate();
        RowLook* look=type==slider_type?&slider_look_:type==option_type?&option_look_:nullptr;
        if(!look || look->known) continue;
        look->type=read<uint8_t>(r,L"NavigableType");
        look->hover=bool_of(r,L"bSelectOnHover");
        look->known=true;
    }
    if(!option_look_.known) option_look_=slider_look_;
    if(!slider_look_.known) slider_look_=option_look_;
    if(!option_look_.known) report("the game's Photo Mode rows changed; CSSX rows use their defaults");
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
    strip_width_=0; tab_pitch_=0; first_tab_=0; pages_ready_=0;
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
    pc_.Reset(); handler_.Reset(); component_.Reset(); session_.clear(); keys_.clear(); icons_.clear(); announced_=false; pages_ready_=0;
}

// ---------------------------------------------------------------- rows

void PhotoMode::build_page(Tab& tab,bool slow) {
    auto* box=tab.box.Get(); if(!box) return;
    if(rows_dirty_ || !tab.complete) {
        rows_dirty_=false;
        auto rows=photo_rows(model_,tab.section);
        for(auto& r:rows) if(r.id==pending_confirm_) r.value="Confirm again";   // until it runs out
        // Pool: one widget per row and kind, created a few per tick so a big tab never hitches.
        std::vector<std::pair<bool,size_t>> order; size_t options=0, sliders=0; int budget=rows_per_tick;
        for(const auto& r:rows) {
            auto& pool=r.slider?tab.slider_rows:tab.option_rows; auto& n=r.slider?sliders:options;
            if(n>=pool.size()) {
                if(budget--<=0) break;
                pool.push_back(make_row(r.slider));
            }
            order.push_back({r.slider,n++});
        }
        if(order!=tab.order) {
            invoke(box,L"ClearChildren");
            for(const auto& [slider,i]:order) if(auto* w=(slider?tab.slider_rows:tab.option_rows)[i].widget.Get()) add_vertical(box,w);
            tab.order=order;
            rescan(box);
            // The page spaces its rows itself (they overlap by the highlight's margin).
            if(auto* page=tab.page.Get()) try { invoke(page,L"UpdateSelectionDistance"); } catch(const std::exception& e) { report(e.what()); }
            // The game selects a page's first row when its tab opens; this page's rows arrive a few
            // ticks later, so select it (or the row that was selected before the rows changed) here.
            if(!order.empty() && active_index(box)<0) navigate(box,std::clamp(tab.selected,0,int(order.size())-1));
        }
        for(size_t i=0;i<order.size();++i) fill(row_at(tab,i),rows[i]);
        tab.rows.assign(rows.begin(),rows.begin()+std::ptrdiff_t(order.size()));
        tab.complete=order.size()==rows.size();
    }
    tab.selected=active_index(box);
    if(slow) recheck(tab);
}

PhotoMode::Row& PhotoMode::row_at(Tab& tab,size_t i) {
    const auto& [slider,index]=tab.order[i];
    return (slider?tab.slider_rows:tab.option_rows)[index];
}

PhotoMode::Row PhotoMode::make_row(bool slider) {
    Row row; row.slider=slider; row.settle_until=now_ms()+settle_ms;
    row.widget=create_widget(pc_.Get(),widget_class(slider?slider_class:row_class));
    auto* w=row.widget.Get();
    // The settings the game gives its own rows on its pages (highlight style, hover).
    const auto& look=slider?slider_look_:option_look_;
    if(look.known) {
        Call type(w,L"SetNavigableType",1); type.set(L"NewParam",look.type); type.run();
        set_bool(w,L"bSelectOnHover",look.hover);
    }
    return row;
}

// The game rewrites a row when it builds it (a frame after CSSX first filled it) and on Reset
// All, with "- -" and an empty bar, since the row has no option logic of its own. New rows are
// read back for their first second; after that one row stands for the page, since Reset All
// rewrites them all at once, and only when it has changed is every row read back.
void PhotoMode::recheck(Tab& tab) {
    if(tab.order.empty()) return;
    const auto now=now_ms();
    bool settling=false;
    for(size_t i=0;i<tab.order.size();++i) { auto& row=row_at(tab,i); if(now<row.settle_until) { settling=true; recheck_row(row); } }
    if(!settling) {
        auto& sentinel=row_at(tab,0);
        recheck_row(sentinel);
        if(sentinel.fresh) for(size_t i=1;i<tab.order.size();++i) recheck_row(row_at(tab,i));
    }
    for(size_t i=0;i<tab.order.size() && i<tab.rows.size();++i) if(auto& row=row_at(tab,i); row.fresh) fill(row,tab.rows[i]);
}

void PhotoMode::recheck_row(Row& row) {
    auto* w=row.widget.Get(); if(!w || row.fresh) return;
    const auto& shown=row.shown;
    if(text_of(object_of(w,L"Text_Option_Value"),4096)!=shown.value) { row.fresh=true; return; }
    if(text_of(object_of(w,L"Text_Option"),4096)!=shown.name) { row.fresh=true; return; }
    if(auto* arrow=object_of(w,L"WBP_ArrowButton_L"))
        if(read<uint8_t>(arrow,L"Visibility")!=(shown.arrows?vis_self_passive:vis_hidden)) { row.fresh=true; return; }
    if(row.slider) if(auto* bar=object_of(w,L"WBP_GenericBar")) if(auto* fill=object_of(bar,L"ProgressBar_Generic"))
        if(std::abs(read<float>(fill,L"Percent")-float(shown.fill))>0.001f) row.fresh=true;
}

void PhotoMode::fill(Row& row,const PhotoRow& value) {
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
        invoke(bar,L"UpdateProgressBar",L"InPercent",float(value.fill));   // WBP_Player_Bar_Generic
    }
    if(row.fresh || row.shown.enabled!=value.enabled) invoke(w,L"ToggleButtonGrayout",L"bShouldGrayout",!value.enabled);
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
        if(pending_confirm_!=row.id || now>confirm_until_) { pending_confirm_=row.id; confirm_until_=now+confirm_window_ms; rows_dirty_=true; return; }
        confirmed=true; pending_confirm_.clear();
    }
    const auto event=photo_event(*control,key,confirmed);
    if(event.is_null()) return;
    try { deps_.runtime->event(session_,event); last_error_.clear(); }
    catch(const std::exception& e) { report(e.what()); }
    rows_dirty_=true;   // show the result on the next tick, even if the extension did not invalidate
}

Json PhotoMode::attach_steps() const {
    Json steps=Json::array();
    for(const auto& [name,us]:attach_cost_.steps) steps.push_back({name,us});
    return steps;
}

Json PhotoMode::diagnostics() const {
    Json tabs=Json::array();
    for(const auto& t:tabs_) tabs.push_back({{"section",t.section},{"rows",t.rows.size()},{"selected",t.selected},{"widgets",t.option_rows.size()+t.slider_rows.size()}});
    Json declared=Json::array();
    for(const auto& d:declared_) declared.push_back({{"id",d.id},{"entry",d.entry},{"tabs",d.tabs.size()}});
    return {{"declared",declared},{"pause_rows",pause_rows_.size()},{"session",session_},{"live",session_live_},
            {"attached",panel_.Get()!=nullptr},{"game_tabs",game_tabs_},{"active_tab",active_tab_},{"tabs",tabs},{"attach_us",attach_steps()},{"error",last_error_}};
}
}

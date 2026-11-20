// Photo Mode for extensions: see photo.hpp. Every engine name used here was read live on the
// 23 Oct 2026 build (docs: extensions/cine/docs/PHOTO-MODE-RESEARCH.md, live facts).
#include "photo.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>

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
// EInterfaceInput Confirm_Primary_Press (NewEnumerator8; DisplayNameMap of
// /Game/Sparta/UI/Core/Navigation/Enums/EInterfaceInput): Enter, Space, pad A.
constexpr uint8_t input_confirm=13;
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
void add_horizontal(UObject* box,UObject* child) { set_widget_child<Call>(box,L"AddChildToHorizontalBox",child); }
void add_child(UObject* panel,UObject* child) { set_widget_child<Call>(panel,L"AddChild",child); }
void remove(UObject* widget) { if(widget) try { invoke(widget,L"RemoveFromParent"); } catch(...) {} }
bool visible_now(UObject* widget) { Call c(widget,L"IsVisible",1); c.run(); return c.get<bool>(); }
}

void PhotoMode::report(const std::string& message) {
    if(message==last_error_) return;
    last_error_=message;
    if(deps_.log) deps_.log("Photo Mode: "+message);
}

void PhotoMode::apply_visual_bool(UObject* widget,const wchar_t* function,bool value) {
    try {
        Call call(widget,function,1);
        call.set_single_bool_input(value);
        call.run();
    } catch(const std::exception& e) { report(e.what()); }
}

void PhotoMode::set_runtime(Runtime* runtime) {
    if(runtime==deps_.runtime) return;
    detach();
    // A reloaded extension can replace a PNG at the same path while the old texture is alive.
    icons_.clear();
    deps_.runtime=runtime; photo_revision_=0; declared_.clear();
}

const PhotoMode::Declared* PhotoMode::declaration(const std::string& id) const {
    const auto found=std::ranges::find_if(declared_,[&](const auto& d){ return d.owner->id==id; });
    return found==declared_.end()?nullptr:&*found;
}

void PhotoMode::refresh_declared() {
    if(!deps_.runtime || deps_.runtime->photo_revision()==photo_revision_) return;
    photo_revision_=deps_.runtime->photo_revision();
    declared_=deps_.runtime->photo_modes();
    if(!session_.empty() || !parked_.empty()) {
        if(auto* mode=declaration(session_.empty()?parked_:session_)) {
            if(participants_.prune(*deps_.runtime,*mode) && !session_.empty()) {
                recover_owner_input_=true;
                open_provider_=session_;
                open_section_.clear();
            }
            size_t keep=0;
            while(keep<tabs_.size() && keep<mode->tabs.size() && tabs_[keep].provider==mode->tabs[keep].provider) ++keep;
            if(keep<tabs_.size()) trim_tabs(keep);
        } else end_session("detached");
    }
    for(auto it=pause_rows_.begin();it!=pause_rows_.end();) {
        if(!declaration(it->id)) { remove(it->row.Get()); it=pause_rows_.erase(it); }
        else ++it;
    }
}

void PhotoMode::tick(const PlayerContext& player,double) {
    if(!deps_.runtime) return;
    refresh_declared();
    if(declared_.empty() && session_.empty() && pause_rows_.empty()) return;
    if(!player.pc) { if(pc_.Get() || session() || !parked_.empty() || !pause_rows_.empty()) detach_player(); return; }
    if(pc_.Get()!=player.pc) {
        detach_player();
        pc_=player.pc;
        component_check_at_=0;
        keys_.clear(); keys_retry_=0;
    }
    if((!handler_.Get() || !component_.Get()) && now_ms()>=component_check_at_) {
        component_check_at_=now_ms()+check_every_ms;
        handler_=object_of(player.pc,L"User Interface Handler Component");
        component_=object_of(player.pc,L"Photography Mode Component");
    }
    auto* handler=handler_.Get(); auto* component=component_.Get();
    if(!handler || !component) return;
    try {
        run_step(component);
        if(!declared_.empty() && bool_of(handler,L"bIsInGameMenu")) pause_tick(handler);
        if(!session_.empty() && (activation_.live || (player.world && player.pawn))) panel_tick(component);
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
        text_property(row,L"Text",declaration(p.id)->owner->photo_entry);
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
    if(!page || !visible_now(page)) { for(auto& p:pause_rows_) p.down=p.confirmed=false; watch_confirms(nullptr,false); return; }
    system_page_=page;
    auto* box=object_of(page,L"BP_VBC_System"); auto* photo=object_of(page,L"WBP_NBS_ActivatePhotomode");
    if(!box || !photo || child_index(box,photo)<0) return;
    if(keys_.empty() && now_ms()>=keys_retry_) {
        try { keys_=read_menu_bindings(pc_.Get(),handler); } catch(const std::exception& e) { keys_retry_=now_ms()+1000; report(e.what()); }
    }
    bool accept=false;
    for(const auto& b:keys_) if(b.action=="accept") for(const auto& k:b.keys) if(key_down(pc_.Get(),k)) { accept=true; break; }
    const bool enabled=bool_of(photo,L"bEnabled",true);
    UObject* after=photo;
    for(const auto& d:declared_) {
        auto it=std::find_if(pause_rows_.begin(),pause_rows_.end(),[&](const PauseRow& p){ return p.id==d.owner->id; });
        if(it==pause_rows_.end()) { pause_rows_.push_back({d.owner->id,{},false}); it=pause_rows_.end()-1; }
        auto* row=ensure_pause_row(*it,box,photo,after);
        after=row;
        watch_confirms(row,true);
        // Available exactly when the game's own Photo Mode row is (IsPlayerInterfaceEnabled).
        if(bool_of(row,L"bEnabled",true)!=enabled) {
            set_bool(row,L"bEnabled",enabled);
            apply_visual_bool(row,L"UpdateEnabledState",enabled);
        }
        const auto state=read<uint8_t>(row,L"NavigableState");
        // The game's navigation confirms a row by state (keys and clicks); the accept key on the
        // selected row is read too, in case a state change is undone within the same frame.
        const bool pressed=it->confirmed || state==state_confirmed || (state==state_selected && accept && !it->down);
        it->down=accept; it->confirmed=false;
        if(enabled && pressed) { begin_session(d.owner->id,page); return; }
    }
    prefetch_icons();
}

// The System page acts on the confirm input only for the game's own rows (it compares the active
// row with each of them), so an extension row is never told. Two post-hooks report it: the page's
// input event (keys and pad: the input arrives with the active row still selected), and every
// row's OnConfirmedState (any path that does confirm a row). They live only while the System
// page shows, since a Blueprint hook makes every script call pay a lookup.
void PhotoMode::watch_confirms(UObject* row,bool on) noexcept {
    auto* hooks=deps_.hooks; if(!hooks) return;
    try {
        if(on && row && !confirm_hook_)
            if(auto* fn=row->GetFunctionByNameInChain(L"OnConfirmedState")) confirm_hook_=hooks->add(hooks->context,fn,&PhotoMode::on_confirmed,this);
        if(on && !input_hook_) if(auto* page=system_page_.Get()) {
            // The page's input listener event; its name carries an editor node number, so match it.
            for(auto* fn:page->GetClassPrivate()->ForEachFunction()) {
                const std::wstring name(fn->GetName());
                if(name.rfind(L"BndEvt__",0)!=0 || name.find(L"OnInputTriggered")==std::wstring::npos) continue;
                for(auto* p:fn->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm) && std::wstring(p->GetName())==L"Input" && p->GetElementSize()==1) input_offset_=p->GetOffset_Internal();
                if(input_offset_>=0) input_hook_=hooks->add(hooks->context,fn,&PhotoMode::on_page_input,this);
                break;
            }
        }
        if(on && (!confirm_hook_ || !input_hook_) && !watch_reported_) {
            watch_reported_=true;
            report("the pause row's confirm cannot be fully watched; the accept key still works");
        }
    } catch(const std::exception& e) { report(e.what()); }
    if(!on) {
        if(confirm_hook_ && hooks->remove(hooks->context,confirm_hook_)) confirm_hook_=0;
        if(input_hook_ && hooks->remove(hooks->context,input_hook_)) input_hook_=0;
    }
}

void PhotoMode::on_confirmed(void* user,void* object,void*,void*) {
    auto* self=static_cast<PhotoMode*>(user);
    for(auto& p:self->pause_rows_) if(p.row.Get()==object) p.confirmed=true;   // acted on next tick
}

void PhotoMode::on_page_input(void* user,void*,void* frame,void*) {
    auto* self=static_cast<PhotoMode*>(user);
    auto* locals=frame?static_cast<RC::Unreal::FFrame*>(frame)->Locals():nullptr;
    if(!locals || self->input_offset_<0) return;
    if(static_cast<const uint8_t*>(locals)[self->input_offset_]!=input_confirm) return;
    try { for(auto& p:self->pause_rows_) if(auto* row=p.row.Get()) if(read<uint8_t>(row,L"NavigableState")==state_selected) p.confirmed=true; } catch(...) {}
}

void PhotoMode::begin_session(const std::string& id,UObject* system_page) {
    auto* handler=handler_.Get(); auto* component=component_.Get();
    if(!handler || !component) return;
    // The game's own chain for Activate Photo Mode (WBP_MOT_System.HandlePhotomodeButton).
    invoke(system_page,L"OnMenuClose");
    { Call c(handler,L"HandleOptionsMenu",2); c.set(L"SubTabIndex",int32_t{0}); c.set(L"AllowClose",true); c.run(); }
    end_session("detached");
    queue_open(id);
}

void PhotoMode::queue_open(const std::string& id) {
    session_=id; activation_={}; announced_=false; active_tab_=-1; model_=nullptr; rows_dirty_=true;
    last_error_.clear();
    if(deps_.log) deps_.log("Photo Mode requested for "+id);
}

void PhotoMode::dev_open(const std::string& id) {
    if(!declaration(id))
        throw std::runtime_error("no loaded extension declares photo_mode: "+id);
    end_session("detached");
    queue_open(id);
}

void PhotoMode::dev_press(const std::string& key) {
    if(key!="left" && key!="right" && key!="accept") throw std::runtime_error("key is left, right or accept");
    const int ours=active_tab_-game_tabs_;
    if(session_.empty() || ours<0 || ours>=int(tabs_.size())) throw std::runtime_error("no extension tab is showing");
    auto& tab=tabs_[size_t(ours)];
    deps_.runtime->photo_tab(*tab.provider,[&]{ press(tab,key); });
}

void PhotoMode::end_session(const char* how) noexcept {
    if(session_.empty() && parked_.empty()) return;
    const auto id=session_.empty()?parked_:session_;
    const bool park=std::string_view(how)=="parked";
    announced_=false; recover_owner_input_=false;
    detach_panel();
    session_.clear(); activation_={}; model_=nullptr; model_provider_=nullptr; pending_confirm_.clear();
    open_provider_.clear(); open_section_.clear();
    if(!park) parked_.clear();
    try {
        if(park) participants_.park(*deps_.runtime);
        else participants_.end(*deps_.runtime,std::string_view(how)=="detached");
        if(deps_.log) deps_.log(std::string("Photo Mode ")+how+" for "+id);
    } catch(...) {}
}

// ---------------------------------------------------------------- stepping out and back in

// An extension in a session can leave Photo Mode for a while (to let the player move) and come
// back into the same session. Both happen on the next tick, never inside the extension's own call.
Json PhotoMode::request(const Json& r) {
    participants_.request(r);
    return true;
}

void PhotoMode::run_step(UObject* component) {
    if(!participants_.has_step()) return;
    const auto step=participants_.take_step();
    if(step.kind==PhotoSession::Step::none) return;
    if(step.kind==PhotoSession::Step::out) {
        if(session_.empty()) return;
        parked_=session_;
        end_session("parked");
        // The game's own way out of Photo Mode: the event its Esc / B binding runs.
        for(auto* fn:component->GetClassPrivate()->ForEachFunction()) {
            const std::wstring name(fn->GetName());
            if(name.rfind(L"InpActEvt_IA_PhotoMode_Deactivate_PlayerControls",0)!=0) continue;
            Call close(component,name.c_str(),fn->GetNumParms()); close.run();   // default input value, no source action
            return;
        }
        report("Photo Mode could not be closed for the extension (the game's exit event is missing)");
        return;
    }
    if(parked_.empty()) return;
    if(!declaration(parked_)) { end_session("detached"); return; }
    open_provider_=step.provider;
    open_section_=step.section;
    queue_open(std::exchange(parked_,{}));
}

// ---------------------------------------------------------------- panel

void PhotoMode::panel_tick(UObject* component) {
    const auto now=now_ms();
    switch(activation_.poll(now,bool_of(component,L"bIsPhotoModeActive"))) {
    case PhotoActivation::try_activate: {
        Call ready(component,L"IsPlayerInterfaceEnabled",1); ready.run();
        if(ready.get<bool>()) invoke(component,L"ActivatePlayerMode");
        return;
    }
    case PhotoActivation::wait: return;
    case PhotoActivation::closed: end_session(); return;
    case PhotoActivation::panel: break;
    }
    auto* panel=object_of(component,L"PhotoModeUI");
    if(!panel) return;
    const bool slow=now>=slow_at_;   // checks that only need to run four times a second
    if(slow) slow_at_=now+check_every_ms;
    // A new panel, or the game rebuilt the strip: start over.
    if(panel_.Get()!=panel || (!tabs_.empty() && (!tabs_.front().tab.Get() || (slow && child_index(strip_.Get(),tabs_.front().tab.Get())<0)))) {
        const int previous=active_tab_-game_tabs_;
        if(previous>=0 && size_t(previous)<tabs_.size() && open_provider_.empty()) {
            open_provider_=tabs_[size_t(previous)].provider->id;
            open_section_=tabs_[size_t(previous)].section;
        }
        detach_panel();
        const auto attached=attach(panel);
        if(attached==Attach::failed) { end_session(); return; }
        if(attached==Attach::later) {
            if(now_ms()-activation_.active_since>5000) { report("the Photo Mode tab strip never laid out; tabs not added"); end_session(); }
            return;
        }
    }
    const auto* mode=declaration(session_);
    if(!mode) { end_session("detached"); return; }
    const bool ready_at_start=owner_page_ready_;
    if(icons_ready_<mode->tabs.size() || pages_ready_<mode->tabs.size()) grow(*mode);
    if(pages_ready_<mode->owner->photo_tabs.size()) return;
    if(!announced_) {
        announced_=true;
        participants_.open(*deps_.runtime,*mode);
    }
    if(!owner_page_ready_ || !open_provider_.empty()) {
        const size_t first=owner_page_ready_ ? mode->focus_tab(open_provider_,open_section_) : 0;
        if(first<pages_ready_ && first<icons_ready_) {
            if(select_page(first)) {
                if(owner_page_ready_) { open_provider_.clear(); open_section_.clear(); }
            } else if(!owner_page_ready_) {
                return;
            }
        }
    }
    auto* strip=strip_.Get(); if(!strip) return;
    const int index=active_index(strip);
    const bool changed=index!=active_tab_;
    if(changed) {
        gestures_.clear();
        active_tab_=index; rows_dirty_=true; pending_confirm_.clear();
        // Scroll by whole tabs so no icon is ever cut at the strip's edge: the window of tabs
        // that fit moves only as far as needed to show the active one.
        if(auto* scroll=scroll_.Get()) if(index>=0 && tab_pitch_>0) {
            const int fit=std::max(1,int(std::floor(strip_width_/tab_pitch_+0.01)));
            const int total=game_tabs_+int(icons_ready_);
            first_tab_=photo_first_visible(first_tab_,index,total,fit);
            invoke(scroll,L"SetScrollOffset",L"NewScrollOffset",float(first_tab_*tab_pitch_));
        }
    }
    const int ours=active_tab_-game_tabs_;
    const bool on_ours=ours>=0 && ours<int(pages_ready_);
    if(on_ours) {
        auto& tab=tabs_[size_t(ours)];
        if(!deps_.runtime->photo_tab(*tab.provider,[&]{
            if(!tab.provider->photo_target.empty() && (changed || slow)) {
                if(bool_of(component,L"bHasPromptOnScreen")) return;
                if(!select_photo_page<Call>(panel,switcher_.Get(),tab.page.Get(),index))
                    throw std::runtime_error("Photo Mode could not select the contributor page");
            }
            update_active_page(tab,component,slow,now);
        })) return;
        if(tab.provider==mode->owner && model_provider_==mode->owner && (tab.complete || !tab.rows.empty())) owner_page_ready_=true;
    }
    if(!on_ours && !row_hooks_.empty()) unwatch_row_input();
    if(ready_at_start) participants_.open_next(*deps_.runtime);
}

bool PhotoMode::select_page(size_t index) {
    auto* strip=strip_.Get();
    auto* panel=panel_.Get();
    auto* page=tabs_[index].page.Get();
    if(!page || !strip || !panel) return false;
    try {
        auto* component=component_.Get();
        if(recover_owner_input_) {
            if(bool_of(component,L"bHasPromptOnScreen")) return false;
            Call debug(pc_.Get(),L"IsDebugWindowInFocus",1); debug.run();
            if(debug.get<bool>()) return false;
        }
        const int target=game_tabs_+int(index);
        if(active_index(strip)!=target) navigate(strip,target);
        if(active_index(strip)!=target || !select_photo_page<Call>(panel,switcher_.Get(),page,target)) return false;
        if(recover_owner_input_) {
            restore_photo_focus<Call>(panel,find_cached(L"/Script/UMG.Default__WidgetBlueprintLibrary"),[&]{
                set_bool(component,L"bIsPlayerUserInterfaceInFocus",true);
            });
            recover_owner_input_=false;
        }
        return true;
    } catch(const std::exception& error) {
        report(error.what());
        return false;
    }
}

void PhotoMode::update_active_page(Tab& tab,UObject* component,bool slow,uint64_t now) {
    // The rows follow the extension's model, which changes only when an extension says so.
    const auto revision=deps_.runtime->revision();
    if(model_provider_!=tab.provider || model_.is_null() || revision!=model_revision_) {
        try { model_=deps_.runtime->photo_model(*tab.provider); }
        catch(const std::exception& error) {
            if(!tab.provider->photo_target.empty()) throw;
            report(error.what());
            return;
        }
        model_provider_=tab.provider; model_revision_=revision; rows_dirty_=true;
    }
    if(!pending_confirm_.empty() && now>confirm_until_) { pending_confirm_.clear(); rows_dirty_=true; }   // "Confirm again" ran out
    build_page(tab,model_,std::exchange(rows_dirty_,false),true,slow);
    if(bool_of(component,L"bIsPlayerUserInterfaceInFocus")) {
        if(slow && !tab.provider->photo_target.empty() && !bool_of(component,L"bHasPromptOnScreen") &&
           visible_now(tab.page.Get()) && (tab.selected<0 || size_t(tab.selected)>=tab.rows.size()))
            throw std::runtime_error("Photo Mode could not select a contributor row");
    }
    dispatch_input(tab);
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

// Preload owner icons one per frame. A guest icon is decoded only after its session opens.
void PhotoMode::prefetch_icons() {
    for(const auto& d:declared_) for(const auto& t:d.tabs) {
        if(t.provider!=d.owner) continue;
        if(auto it=icons_.find(t.spec->icon.native()); it!=icons_.end() && it->second.Get()) continue;
        try { icon_texture(t.spec->icon); } catch(const std::exception& e) { report(e.what()); }
        return;
    }
}

PhotoMode::Attach PhotoMode::attach(UObject* panel) {
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
    icons_ready_=pages_ready_=0;
    const auto started=clock_us();
    wrap_strip(panel);
    attach_cost_.steps.push_back({"wrap",clock_us()-started});
    if(auto* size=size_box_.Get()) invoke(size,L"SetWidthOverride",L"InWidthOverride",float(width));
    strip_width_=float(width); tab_pitch_=float(width)/float(game_tabs_); first_tab_=0;   // the game's tabs are all alike
    return Attach::done;
}

// The owner's original strip is built icons first. Once its tabs are usable, give each
// guest a page with rows before making its icon navigable.
void PhotoMode::grow(const Declared& declared) {
    const bool icon=icons_ready_<declared.owner->photo_tabs.size() || icons_ready_<pages_ready_;
    const size_t index=icon?icons_ready_:pages_ready_;
    const auto& binding=declared.tabs[index];
    if(binding.provider!=declared.owner && (!owner_page_ready_ || !participants_.has_opened(binding.provider))) return;
    if(index==tabs_.size()) {
        auto& tab=tabs_.emplace_back();
        tab.provider=binding.provider;
        tab.section=binding.spec->section;
    }
    const auto started=clock_us();
    deps_.runtime->photo_tab(*binding.provider,[&]{
        auto& tab=tabs_[index];
        if(icon) {
            if(binding.provider!=declared.owner && tab.rows.empty()) {
                build_page(tab,deps_.runtime->photo_model(*tab.provider),true,false,false);
                attach_cost_.steps.push_back({"first row",clock_us()-started});
                return;
            }
            add_tab_widget(tab,*binding.spec);
            ++icons_ready_;
        } else {
            add_page_widget(tab);
            ++pages_ready_;
        }
        attach_cost_.steps.push_back({icon?"tab":"page",clock_us()-started});
    });
}

void PhotoMode::add_tab_widget(Tab& tab,const Manifest::PhotoTab& spec) {
    auto* strip=strip_.Get();
    const auto existing=children(strip);
    if(existing.empty()) throw std::runtime_error("the Photo Mode tab strip has no reference icon");
    auto* reference=existing.front();
    auto* icon=create_widget(pc_.Get(),widget_class(tab_class));
    if(!icon) throw std::runtime_error("Could not create Photo Mode tab icon");
    tab.tab=icon;
    for(auto name:{L"ImageStretchMode",L"SelectedColor",L"DefaultColor",L"RootSize",L"RootScale",L"HighlightY",L"bUseHighlight",L"IconSizeX",L"IconSizeY",L"HighlightColor"})
        copy_property(icon,reference,name);
    if(auto* texture=icon_texture(spec.icon)) { object_property(icon,L"IconTexture",texture); tab.icon=texture; }
    else if(!tab.provider->photo_target.empty()) throw std::runtime_error("Photo Mode tab icon could not load: "+path_utf8(spec.icon));
    else report("tab icon did not load: "+path_utf8(spec.icon));
    for(auto fn:{L"UpdateImage",L"CommitSize",L"CommitScale"}) invoke(icon,fn);
    if(!set_widget_child<Call>(strip,L"AddChildToHorizontalBox",icon))
        throw std::runtime_error("Photo Mode strip rejected the tab icon");
    if(!tab.provider->photo_target.empty()) {
        invoke(icon,L"ForceLayoutPrepass");
        Call size(icon,L"GetDesiredSize",1); size.run();
        const auto extent=size.get<Vec2>();
        if(!visible_now(icon) || !(extent.x>0 && extent.y>0))
            throw std::runtime_error("Photo Mode tab icon has no visible area");
    }
    if(auto* slot=object_of(icon,L"Slot")) if(auto* ref_slot=object_of(reference,L"Slot"))
        set_widget_option<Call>(slot,L"SetPadding",WidgetParamType::margin,read<Margin>(ref_slot,L"Padding"));
    rescan(strip);
}

void PhotoMode::add_page_widget(Tab& tab) {
    auto* page=create_widget(pc_.Get(),widget_class(page_class));
    if(!page) throw std::runtime_error("Could not create Photo Mode tab page");
    tab.page=page;
    auto* box=object_of(page,L"BP_VBC_PhotomodeGeneral");
    if(!box) throw std::runtime_error("the Photo Mode page layout changed; tabs not added");
    if(pages_ready_==0) learn_row_looks(box);
    invoke(box,L"ClearChildren");
    if(!set_widget_child<Call>(switcher_.Get(),L"AddChild",page)) throw std::runtime_error("Photo Mode switcher rejected the page");
    apply_visual_bool(page,L"RefreshUIScale",false);
    tab.box=box;
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
    unwatch_row_input();
    try {
        if(auto* strip=strip_.Get()) if(active_tab_>=game_tabs_ && game_tabs_>0) navigate(strip,0);
    } catch(...) {}
    for(auto& t:tabs_) { remove(t.tab.Get()); remove(t.page.Get()); }
    tabs_.clear(); owner_page_ready_=false;
    unwrap_strip();
    try { if(auto* strip=strip_.Get()) rescan(strip); } catch(...) {}
    panel_.Reset(); strip_.Reset(); switcher_.Reset(); game_tabs_=0; active_tab_=-1;
    strip_width_=0; tab_pitch_=0; first_tab_=0; icons_ready_=pages_ready_=0;
}

void PhotoMode::detach() noexcept {
    end_session("detached");
    detach_player();
}

void PhotoMode::detach_player() noexcept {
    watch_confirms(nullptr,false);
    end_session("detached");
    for(auto& p:pause_rows_) remove(p.row.Get());
    try { if(auto* page=system_page_.Get()) if(auto* box=object_of(page,L"BP_VBC_System")) rescan(box); } catch(...) {}
    pause_rows_.clear(); system_page_.Reset();
    pc_.Reset(); handler_.Reset(); component_.Reset();
}

void PhotoMode::trim_tabs(size_t keep) {
    gestures_.clear();
    auto* strip=strip_.Get();
    int selected=active_tab_;
    if(strip) try { selected=active_index(strip); } catch(const std::exception& error) { report(error.what()); }
    const int ours=selected-game_tabs_;
    if(ours>=0 && size_t(ours)>=keep && strip) {
        open_provider_=session_;
        open_section_.clear();
        try { navigate(strip,keep?game_tabs_:0); } catch(const std::exception& e) { report(e.what()); }
        active_tab_=-1;
    }
    for(size_t i=keep;i<tabs_.size();++i) {
        remove(tabs_[i].tab.Get());
        remove(tabs_[i].page.Get());
    }
    tabs_.resize(keep);
    icons_ready_=std::min(icons_ready_,keep);
    pages_ready_=std::min(pages_ready_,keep);
    pending_confirm_.clear(); rows_dirty_=true;
    if(strip) try { rescan(strip); } catch(const std::exception& e) { report(e.what()); }
}

// ---------------------------------------------------------------- rows

void PhotoMode::build_page(Tab& tab,const Json& model,bool rebuild,bool active,bool slow) {
    auto* box=tab.box.Get();
    if(!box) throw std::runtime_error("Photo Mode tab lost its row container");
    if(rebuild || !tab.complete) {
        auto rows=photo_tab_rows(model,*tab.provider,tab.section);
        if(active) for(auto& r:rows) if(r.id==pending_confirm_) r.value="Confirm again";   // until it runs out
        // Pool: one widget per row and kind, created a few per tick so a big tab never hitches.
        decltype(tab.order) order;
        std::array<size_t,PhotoRow::kind_count> used{};
        int budget=active?rows_per_tick:1;
        for(const auto& r:rows) {
            auto& pool=tab.pools[r.kind];
            auto& n=used[r.kind];
            if(n>=pool.size()) {
                if(budget--<=0) break;
                pool.push_back(make_row(r.kind));
            }
            order.push_back({r.kind,n++});
        }
        if(order!=tab.order) {
            invoke(box,L"ClearChildren");
            for(const auto& [kind,i]:order) {
                add_photo_row<Call>(box,tab.pools[kind][i].widget.Get());
            }
            tab.order=order;
            rescan(box);
            // The page spaces its rows itself (they overlap by the highlight's margin).
            if(auto* page=tab.page.Get()) try { invoke(page,L"UpdateSelectionDistance"); } catch(const std::exception& e) { report(e.what()); }
        }
        for(size_t i=0;i<order.size();++i) fill(row_at(tab,i),rows[i]);
        tab.rows.assign(rows.begin(),rows.begin()+std::ptrdiff_t(order.size()));
        tab.complete=order.size()==rows.size();
    }
    if(!active) return;
    if(!tab.order.empty() && active_index(box)<0) navigate(box,std::clamp(tab.selected,0,int(tab.order.size())-1));
    tab.selected=active_index(box);
    // Rows of a kind share their Blueprint functions, including the main button's class.
    for(const auto& pool:tab.pools) if(!pool.empty()) {
        const auto& row=pool.front();
        watch_row_input(row.widget.Get(),false);
        watch_row_input(row.main_button.Get(),true);
    }
    if(slow) recheck(tab);
}

PhotoMode::Row& PhotoMode::row_at(Tab& tab,size_t i) {
    const auto& [kind,index]=tab.order[i];
    return tab.pools[kind][index];
}

PhotoMode::Row PhotoMode::make_row(PhotoRow::Kind kind) {
    const bool slider=kind==PhotoRow::slider;
    Row row; row.slider=slider; row.settle_until=now_ms()+settle_ms;
    row.widget=create_widget(pc_.Get(),widget_class(slider?slider_class:row_class));
    auto* w=row.widget.Get();
    if(!w) throw std::runtime_error("Could not create Photo Mode row");
    row.main_button=object_of(w,L"WBP_NavButton_Main");
    if(!row.main_button.Get()) throw std::runtime_error("Photo Mode row has no main button");

    // The settings the game gives its own rows on its pages (highlight style, hover).
    const auto& look=slider?slider_look_:option_look_;
    if(look.known) {
        Call type(w,L"SetNavigableType",1); type.set(L"NewParam",look.type); type.run();
        set_bool(w,L"bSelectOnHover",look.hover);
    }
    if(kind==PhotoRow::information) {
        auto* value=object_of(w,L"Text_Option_Value");
        auto* column=object_of(w,L"SizeBox_OptionValue");
        if(column && value) {
            // Keep the game's font at its native size. fit_info measures it after the column is laid out.
            try { fit_information_value<Call>(column,value); }
            catch(const std::exception& e) { report(std::string("information row layout: ")+e.what()); }
        }
    }
    return row;
}

// The game rewrites a row when it builds it (a frame after CSSX first filled it) and on Reset
// All, with "- -" and an empty bar, since the row has no option logic of its own. New rows are
// read back for their first second. After that the first option row and the first slider row
// stand for the page (the game rewrites each kind as a group: Reset All, the UI coming back after
// a take), and only when one of them changed is every row read back.
void PhotoMode::recheck(Tab& tab) {
    if(tab.order.empty()) return;
    const auto now=now_ms();
    bool settling=false;
    for(size_t i=0;i<tab.order.size();++i) { auto& row=row_at(tab,i); if(now<row.settle_until) { settling=true; recheck_row(row); } }
    for(size_t i=0;i<tab.order.size();++i) {
        auto& row=row_at(tab,i);
        if(row.shown.kind==PhotoRow::information && !row.fresh) {
            recheck_row(row);
            if(!row.fresh) fit_info(row,row.shown,false);
        }
    }
    if(!settling) {
        // The game resets option rows and slider rows separately, so one of each stands for the page.
        bool changed=false; bool seen_option=false, seen_slider=false;
        for(size_t i=0;i<tab.order.size() && !(seen_option && seen_slider);++i) {
            auto& row=row_at(tab,i); bool& seen=row.slider?seen_slider:seen_option;
            if(seen) continue;
            seen=true; recheck_row(row); changed=changed || row.fresh;
        }
        if(changed) for(size_t i=0;i<tab.order.size();++i) recheck_row(row_at(tab,i));
    }
    for(size_t i=0;i<tab.order.size() && i<tab.rows.size();++i) if(auto& row=row_at(tab,i); row.fresh) fill(row,tab.rows[i]);
}

void PhotoMode::fit_info(Row& row,const PhotoRow& value,bool force) {
    auto* w=row.widget.Get();
    auto* column=object_of(w,L"SizeBox_OptionValue");
    auto* block=object_of(w,L"Text_Option_Value");
    if(!block) return;
    if(column) try {
        Call geometry(column,L"GetCachedGeometry",1); geometry.run();
        Call size(find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
        size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        const double width=size.get<Vec2>().x;
        if(!force && width==row.column_width && std::abs(desired_text_width<Call,Vec2>(block)-row.text_width)<0.01) return;
        std::tie(row.displayed_info,row.text_width)=display_information<Call,Vec2>(block,value.value,width,text_value);
        row.column_width=width;
        return;
    } catch(const std::exception& e) { report(std::string("information row measurement: ")+e.what()); }
    row.displayed_info=value.value;
    row.column_width=row.text_width=-1;
    text_value(block,row.displayed_info);
}

void PhotoMode::recheck_row(Row& row) {
    auto* w=row.widget.Get(); if(!w || row.fresh) return;
    const auto& shown=row.shown;
    // GetText returns the displayed text plus a terminator. The model retains the full label.
    if(text_of(object_of(w,L"Text_Option_Value"),1024*1024+1)!=(shown.kind==PhotoRow::information?row.displayed_info:shown.value)) { row.fresh=true; return; }
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
    if(row.fresh || row.shown.value!=value.value) {
        if(value.kind==PhotoRow::information) fit_info(row,value,true);
        else if(auto* block=object_of(w,L"Text_Option_Value")) text_value(block,value.value);
    }
    if(row.fresh || row.shown.arrows!=value.arrows) {
        // Hidden, not collapsed: the value keeps its place in the row.
        visible(object_of(w,L"WBP_ArrowButton_L"),value.arrows?vis_self_passive:vis_hidden);
        visible(object_of(w,L"WBP_ArrowButton_R"),value.arrows?vis_self_passive:vis_hidden);
    }
    if(row.slider && (row.fresh || row.shown.fill!=value.fill)) if(auto* bar=object_of(w,L"WBP_GenericBar")) {
        invoke(bar,L"UpdateProgressBar",L"InPercent",float(value.fill));   // WBP_Player_Bar_Generic
    }
    // The option and slider Blueprints spell the input differently.
    if(row.fresh || row.shown.enabled!=value.enabled) apply_visual_bool(w,L"ToggleButtonGrayout",!value.enabled);
    row.shown=value; row.fresh=false;
}

void PhotoMode::watch_row_input(UObject* widget,bool button) {
    if(!widget) throw std::runtime_error("Photo Mode row input widget expired");
    auto* type=widget->GetClassPrivate();
    if(watched_classes_.contains(type)) return;
    auto* hooks=deps_.hooks;
    if(!hooks || hooks->size<sizeof(CssxHookHost) || !hooks->add_script_pre)
        throw std::runtime_error("Photo Mode row input requires the matching CSSX loader");
    int count=0;
    for(auto* fn:type->ForEachFunction()) {
        const auto source=photo_input_source(fn->GetName());
        if(source==PhotoInputSource::unrelated || button!=(source==PhotoInputSource::button || source==PhotoInputSource::button_down || source==PhotoInputSource::button_up)) continue;
        ++count;
        int offset=-1;
        if(source==PhotoInputSource::navigation) {
            for(auto* p:fn->ForEachProperty()) {
                if(p->HasAnyPropertyFlags(CPF_Parm) && !p->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm) &&
                   p->GetElementSize()==1 && p->GetFName()==FName(L"Input")) offset=p->GetOffset_Internal();
            }
        }
        bind_photo_script(row_hooks_,fn,source,offset,[&](auto* target){
            return hooks->add_script_pre(hooks->context,target,&PhotoMode::on_row_input,this);
        });
    }
    if(count!=(button?3:5)) throw std::runtime_error("Photo Mode widget input events changed");
    watched_classes_.insert(type);
}

void PhotoMode::unwatch_row_input() noexcept {
    gestures_.clear();
    watched_classes_.clear();
    if(auto* hooks=deps_.hooks) std::erase_if(row_hooks_,[&](const auto& entry){
        return hooks->remove(hooks->context,entry.second.token)!=0;
    });
}

void PhotoMode::on_row_input(void* user,void* object,void* raw_frame,void*) noexcept {
    auto* self=static_cast<PhotoMode*>(user);
    auto* frame=static_cast<RC::Unreal::FFrame*>(raw_frame);
    if(!frame || !object) return;
    const auto it=self->row_hooks_.find(frame->Node());
    if(it==self->row_hooks_.end()) return;
    const auto& hook=it->second;
    if(hook.offset>=0 && !frame->Locals()) return;
    int32_t entry=0;
    if(!hook.route.entries.empty()) std::memcpy(&entry,frame->Locals(),sizeof(entry));
    const auto source=hook.route.source(entry);
    if(source==PhotoInputSource::unrelated) return;
    const auto input=source==PhotoInputSource::navigation?static_cast<const uint8_t*>(frame->Locals())[hook.offset]:0;
    try { self->remember_input(static_cast<UObject*>(object),source,uint8_t(input)); }
    catch(const std::exception& error) { try { self->report(error.what()); } catch(...) {} }
}

bool PhotoMode::body_click_allowed(UObject* button) {
    if(bool_of(button,L"IgnoreBlockAll")) return true;
    Call pawn(button,L"GetOwningPlayerPawn",1); pawn.run();
    auto* target=pawn.get<UObject*>();
    if(!target || !target->GetFunctionByNameInChain(L"HasMatchingGameplayTag")) return true;
    Call blocked(target,L"HasMatchingGameplayTag",2);
    auto* tag=blocked.param(L"TagToCheck");
    member(blocked.data(tag),tag->GetElementSize(),find_cached(L"/Script/GameplayTags.GameplayTag"),L"TagName",FName(L"UI.Input.Block.All"));
    blocked.run();
    return !blocked.get<bool>();
}

void PhotoMode::remember_input(UObject* object,PhotoInputSource source,uint8_t input) {
    const int ours=active_tab_-game_tabs_;
    if(ours<0 || size_t(ours)>=tabs_.size()) return;
    auto& tab=tabs_[size_t(ours)];
    for(size_t i=0;i<tab.rows.size();++i) {
        auto& row=row_at(tab,i);
        auto* widget=row.widget.Get();
        const bool body=source==PhotoInputSource::button || source==PhotoInputSource::button_down || source==PhotoInputSource::button_up;
        if((body?row.main_button.Get():widget)!=object) continue;
        const auto& shown=tab.rows[i];
        const auto identity=reinterpret_cast<uintptr_t>(widget);
        if(source==PhotoInputSource::button_down && shown.button) {
            Call subsystem(find_cached(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetLocalPlayerSubsystem",3);
            subsystem.set(L"ContextObject",object);
            subsystem.set(L"Class",find_cached(L"/Script/CommonInput.CommonInputSubsystem"));
            subsystem.run();
            gestures_.down(identity,shown.id,bool_of(subsystem.get<UObject*>(),L"bIsGamepadSimulatedClick"));
            return;
        }
        if(source==PhotoInputSource::button_up) { gestures_.up(identity,shown.id); return; }
        const auto command=photo_command(source,input,shown.button,shown.arrows);
        if(command.empty() || !shown.enabled || !visible_now(widget) || !bool_of(widget,L"bEnabled",true)) return;
        if(source==PhotoInputSource::button && !body_click_allowed(object)) return;
        gestures_.push({identity,shown.id,std::string(command),source});
        return;
    }
}

void PhotoMode::dispatch_input(Tab& tab) {
    auto batch=gestures_.take();
    if(batch.empty()) return;
    if(bool_of(component_.Get(),L"bHasPromptOnScreen") || !visible_now(object_of(panel_.Get(),L"Overlay_UI"))) return;
    Call active(switcher_.Get(),L"GetActiveWidget",1); active.run();
    if(active.get<UObject*>()!=tab.page.Get()) return;
    for(const auto& gesture:batch) {
        for(size_t i=0;i<tab.rows.size();++i) {
            if(!gesture.matches(reinterpret_cast<uintptr_t>(row_at(tab,i).widget.Get()),tab.rows[i].id)) continue;
            if(model_revision_!=deps_.runtime->revision()) {
                model_=deps_.runtime->photo_model(*tab.provider);
                model_revision_=deps_.runtime->revision();
                rows_dirty_=true;
            }
            press_row(tab,i,gesture.command);
            break;
        }
    }
}

void PhotoMode::press(Tab& tab,const std::string& key) {
    if(tab.selected>=0 && size_t(tab.selected)<tab.rows.size()) press_row(tab,size_t(tab.selected),key);
}

void PhotoMode::press_row(Tab& tab,size_t index,const std::string& key) {
    if(model_provider_!=tab.provider) return;
    const auto& row=tab.rows[index];
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
    try {
        deps_.runtime->photo_action(*tab.provider,event);
        ++delivered_;
        last_action_={{"extension",tab.provider->id},{"section",tab.section},{"event",event}};
        last_error_.clear();
    }
    catch(const std::exception& error) {
        if(!tab.provider->photo_target.empty()) throw;
        report(error.what());
    }
    rows_dirty_=true;   // show the result on the next tick, even if the extension did not invalidate
}

Json PhotoMode::attach_steps() const {
    Json steps=Json::array();
    for(const auto& [name,us]:attach_cost_.steps) steps.push_back({name,us});
    return steps;
}

namespace {
std::optional<PhotoBounds> pixel_bounds(UObject* widget,UObject* world) {
    if(!widget || !visible_now(widget)) return {};
    auto* slate=find_cached(L"/Script/UMG.Default__SlateBlueprintLibrary");
    Call geometry(widget,L"GetCachedGeometry",1); geometry.run();
    Call local(slate,L"GetLocalSize",2); local.copy(L"Geometry",geometry,L"ReturnValue"); local.run();
    const auto size=local.get<Vec2>();
    if(!(size.x>0 && size.y>0)) return {};
    auto pixel=[&](Vec2 point) {
        Call project(slate,L"LocalToViewport",5);
        project.set(L"WorldContextObject",world); project.copy(L"Geometry",geometry,L"ReturnValue");
        project.set(L"LocalCoordinate",point); project.run();
        return project.get<Vec2>(L"PixelPosition");
    };
    const auto a=pixel({0,0}), b=pixel(size);
    return PhotoBounds{a.x,a.y,b.x,b.y};
}

Json rect_json(std::optional<PhotoBounds> r) {
    if(!r) return nullptr;
    return {{"x",r->left},{"y",r->top},{"w",r->right-r->left},{"h",r->bottom-r->top}};
}
}

Json PhotoMode::input_layout() const {
    Json result={{"icons",Json::array()},{"rows",Json::array()}};
    auto* panel=panel_.Get();
    auto* pc=pc_.Get();
    if(!panel || !pc || !visible_now(object_of(panel,L"Overlay_UI"))) return result;
    auto bounds=[&](UObject* widget){return pixel_bounds(widget,pc);};
    auto viewport=bounds(panel);
    auto clipped=[&](UObject* widget,std::optional<PhotoBounds> clip) {
        auto rect=bounds(widget);
        return rect && clip?rect->clipped(*clip):std::optional<PhotoBounds>{};
    };
    const auto strip_clip=scroll_.Get()?clipped(scroll_.Get(),viewport):viewport;
    result["panel"]=rect_json(clipped(switcher_.Get(),viewport));
    result["strip"]=rect_json(strip_clip);
    auto scroll_state=[](UObject* widget)->Json {
        if(!widget) return nullptr;
        Call offset(widget,L"GetScrollOffset",1); offset.run();
        Call end(widget,L"GetScrollOffsetOfEnd",1); end.run();
        return {{"offset",offset.get<float>()},{"end",end.get<float>()}};
    };
    result["strip_scroll"]=scroll_state(scroll_.Get());
    result["hints"]=rect_json(clipped(object_of(panel,L"PhotoModeBottomPrompts"),viewport));
    int index=0;
    for(auto* icon:children(strip_.Get())) {
        const auto rect=clipped(icon,strip_clip);
        if(rect) result["icons"].push_back({{"index",index},{"rect",rect_json(rect)}});
        ++index;
    }
    Call current(switcher_.Get(),L"GetActiveWidget",1); current.run();
    auto* page=current.get<UObject*>();
    auto* box=object_of(page,L"BP_VBC_PhotomodeGeneral");
    if(!box) return result;
    auto* scroll=object_of(page,L"ScrollBox_DisplayOptions");
    result["row_scroll"]=scroll_state(scroll);
    result["selected"]=active_index(box);
    const auto row_clip=clipped(scroll?scroll:page,viewport);
    index=0;
    const int ours=active_tab_-game_tabs_;
    for(auto* row:children(box)) {
        const auto rect=clipped(row,row_clip);
        if(rect) {
            Json entry={{"index",index},{"rect",rect_json(rect)}};
            if(auto* label=object_of(row,L"Text_Option")) entry["label"]=text_of(label,4096);
            if(auto* value=object_of(row,L"Text_Option_Value")) entry["value"]=text_of(value,1024*1024+1);
            if(ours>=0 && size_t(ours)<tabs_.size() && page==tabs_[ours].page.Get() && size_t(index)<tabs_[ours].rows.size())
                entry["id"]=tabs_[ours].rows[index].id;
            for(const auto& [name,child]:{std::pair{"button",L"WBP_NavButton_Main"},{"left",L"WBP_ArrowButton_L"},
                                        {"right",L"WBP_ArrowButton_R"},{"bar",L"WBP_GenericBar"}})
                entry[name]=rect_json(clipped(object_of(row,child),rect));
            result["rows"].push_back(std::move(entry));
        }
        ++index;
    }
    return result;
}

Json PhotoMode::diagnostics() const {
    Json tabs=Json::array();
    for(const auto& t:tabs_) {
        size_t widgets=0;
        for(const auto& pool:t.pools) widgets+=pool.size();
        Json info=Json::array();
        for(const auto& [kind,index]:t.order) if(kind==PhotoRow::information) {
            const auto& row=t.pools[kind][index];
            info.push_back({{"id",row.shown.id},{"displayed",row.displayed_info},{"column_width",row.column_width},{"text_width",row.text_width}});
        }
        tabs.push_back({{"information",std::move(info)},{"extension",t.provider->id},{"section",t.section},{"rows",t.rows.size()},{"selected",t.selected},{"widgets",widgets}});
    }
    Json declared=Json::array();
    for(const auto& d:declared_) declared.push_back({{"id",d.owner->id},{"entry",d.owner->photo_entry},{"tabs",d.tabs.size()}});
    Json layout;
    try { layout=input_layout(); } catch(const std::exception& e) { layout={{"error",e.what()}}; }
    return {{"input_layout",std::move(layout)},{"menu_focus",bool_of(component_.Get(),L"bIsPlayerUserInterfaceInFocus")},
            {"delivered",delivered_},{"last_action",last_action_},{"declared",declared},{"pause_rows",pause_rows_.size()},{"session",session_},{"live",activation_.live},
            {"attached",panel_.Get()!=nullptr},{"game_tabs",game_tabs_},{"active_tab",active_tab_},{"tabs",tabs},{"attach_us",attach_steps()},{"error",last_error_}};
}
}

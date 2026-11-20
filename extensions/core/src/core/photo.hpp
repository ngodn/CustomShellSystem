#pragma once
// Photo Mode for extensions (CSSX 1.3.2). An extension that declares "photo_mode" in its manifest
// gets a row on the pause menu's System page, right under the game's Activate Photo Mode. The row
// opens the game's own Photo Mode the way the game's row does, then adds the extension's menu
// sections to the panel as tabs: the game's tab and page widgets, the game's option and slider
// rows. The tab strip scrolls once the extra tabs no longer fit.
//
// The game handles the panel itself: tab switching, up and down on a page, highlight, focus,
// Hide UI, Take Shot and closing. CSSX reads only left, right and confirm on the extension's
// rows and turns them into the extension's control events. Everything CSSX adds is removed when
// that Photo Mode session ends, so the game's Activate Photo Mode always opens the panel exactly
// as shipped. Since 1.3.3, other extensions can contribute tabs to the owner's session.
//
// Row events are observed only while an extension page is active.
#include "engine.hpp"
#include "core_abi.h"
#include "photo_session.hpp"
#include "menu_input.hpp"
#include "photo_rows.hpp"
#include "photo_input.hpp"
#include "photo_activation.hpp"
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace cssx {
class PhotoMode {
public:
    struct Deps {
        Runtime* runtime=nullptr;
        std::function<void(const std::string&)> log;
        const CssxHookHost* hooks=nullptr;   // the loader's hook service: tells when a row is confirmed
    };
    explicit PhotoMode(Deps deps):deps_(std::move(deps)) {}
    ~PhotoMode() { try { detach(); } catch(...) {} }
    PhotoMode(const PhotoMode&)=delete;
    PhotoMode& operator=(const PhotoMode&)=delete;

    void set_runtime(Runtime* runtime);
    // Once per engine tick from the core.
    void tick(const engine::PlayerContext& player,double delta);
    // Remove everything CSSX added (pause rows, tabs, the scroll box) and end a session.
    void detach() noexcept;
    bool session() const { return !session_.empty(); }
    Json diagnostics() const;
    // Host requests from extensions: photo.step_out, photo.step_in {section}.
    Json request(const Json& r);
    Json attach_steps() const;
    // Dev channel only: start a session for an extension without the pause menu (tests).
    void dev_open(const std::string& id);
    void dev_press(const std::string& key);   // a menu key on the selected row

private:
    using UObject=RC::Unreal::UObject;
    using WeakObject=engine::WeakObject;
    using Declared=PhotoModeDefinition;
    struct PauseRow { std::string id; WeakObject row; bool down=false, confirmed=false; };
    struct Row { WeakObject widget,main_button; bool slider=false; PhotoRow shown; std::string displayed_info; double column_width=-1, text_width=-1; bool fresh=true; uint64_t settle_until=0; };
    struct Tab {
        const Manifest* provider=nullptr;
        std::string section; WeakObject tab, page, box; WeakObject icon;
        std::array<std::vector<Row>,PhotoRow::kind_count> pools;
        std::vector<std::pair<PhotoRow::Kind,size_t>> order;
        std::vector<PhotoRow> rows;                  // what the page shows now
        int selected=-1;
        bool complete=false;                         // every row of the model has its widget
    };

    Deps deps_;
    uint64_t photo_revision_=0;
    std::vector<Declared> declared_;
    WeakObject pc_, handler_, component_;
    // pause menu
    std::vector<PauseRow> pause_rows_;
    WeakObject system_page_;
    uint64_t confirm_hook_=0, input_hook_=0;   // post-hooks while the System page shows (see watch_confirms)
    int32_t input_offset_=-1;               // the page input event's Input parameter
    bool watch_reported_=false;
    // session
    std::string session_;                   // extension id that owns the open Photo Mode session
    PhotoSession participants_;
    std::string open_provider_;
    const Manifest* model_provider_=nullptr;
    std::string parked_, open_section_;     // a session stepped out of; the tab to reopen on
    PhotoActivation activation_;
    uint64_t component_check_at_=0;
    WeakObject panel_, strip_, switcher_, strip_box_, scroll_, size_box_;
    std::vector<Tab> tabs_;
    int game_tabs_=0, active_tab_=-1;
    float strip_width_=0, tab_pitch_=0; int first_tab_=0;   // tab strip scrolling, whole tabs at a time
    struct SlotLook { std::array<float,4> padding{}; std::array<std::byte,8> size{}; uint8_t horizontal=0, vertical=0; };
    std::vector<std::pair<WeakObject,SlotLook>> strip_siblings_;   // the strip's row before CSSX wrapped it
    Json model_;
    uint64_t model_revision_=0, slow_at_=0;
    bool rows_dirty_=true;
    struct { std::vector<std::pair<const char*,uint64_t>> steps; } attach_cost_;   // last attach, microseconds a step
    size_t icons_ready_=0, pages_ready_=0;
    bool owner_page_ready_=false, recover_owner_input_=false;
    bool announced_=false;                  // the extension heard "open" this session
    std::map<std::wstring,WeakObject> icons_;   // imported tab icons
    std::string pending_confirm_; uint64_t confirm_until_=0;
    std::vector<MenuKeyBinding> keys_;
    uint64_t keys_retry_=0;
    std::map<RC::Unreal::UFunction*,PhotoInputHook> row_hooks_;
    std::set<RC::Unreal::UClass*> watched_classes_;
    PhotoInputQueue gestures_;
    uint64_t delivered_=0;
    Json last_action_;
    std::string last_error_;
    struct RowLook { uint8_t type=0; bool hover=false; bool known=false; };
    RowLook option_look_, slider_look_;

    Json input_layout() const;
    void refresh_declared();
    const Declared* declaration(const std::string&) const;
    void trim_tabs(size_t keep);
    void report(const std::string& message);
    void apply_visual_bool(UObject* widget,const wchar_t* function,bool value);
    // pause menu
    void pause_tick(UObject* handler);
    UObject* ensure_pause_row(PauseRow& p,UObject* box,UObject* reference,UObject* after);
    void detach_player() noexcept;
    void queue_open(const std::string& id);
    void begin_session(const std::string& id,UObject* system_page);
    void watch_confirms(UObject* row,bool on) noexcept;
    static void on_confirmed(void* user,void* object,void* frame,void* result);
    static void on_page_input(void* user,void* object,void* frame,void* result);
    void end_session(const char* how="closed") noexcept;
    void run_step(UObject* component);
    // panel
    void panel_tick(UObject* component);
    bool select_page(size_t index);
    enum class Attach { done, later, failed };
    Attach attach(UObject* panel);
    void detach_panel() noexcept;
    void wrap_strip(UObject* panel);
    void unwrap_strip() noexcept;
    void learn_row_looks(UObject* box);
    UObject* icon_texture(const fs::path& file);
    void prefetch_icons();
    void grow(const Declared& declared);
    void add_tab_widget(Tab&,const Manifest::PhotoTab&);
    void add_page_widget(Tab&);
    void update_active_page(Tab&,UObject* component,bool slow,uint64_t now);
    void build_page(Tab&,const Json& model,bool rebuild,bool active,bool slow);
    Row& row_at(Tab& tab,size_t i);
    Row make_row(PhotoRow::Kind kind);
    void recheck(Tab& tab);
    void recheck_row(Row& row);
    void fit_info(Row&,const PhotoRow&,bool force);
    void fill(Row& row,const PhotoRow& value);
    void watch_row_input(UObject* widget,bool button);
    void unwatch_row_input() noexcept;
    static void on_row_input(void*,void*,void*,void*) noexcept;
    void remember_input(UObject*,PhotoInputSource,uint8_t);
    bool body_click_allowed(UObject* button);
    void dispatch_input(Tab&);
    void press_row(Tab&,size_t,const std::string&);
    void press(Tab& tab,const std::string& key);
};
}

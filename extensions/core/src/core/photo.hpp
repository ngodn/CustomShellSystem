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
// as shipped. The extension hears {"id":"cssx.photo_mode","value":"open"|"closed"}.
//
// Cost: nothing unless an extension declares photo_mode. Then two cached flag reads a frame;
// while an extension tab is showing, a key poll and a model refresh four times a second.
#include "engine.hpp"
#include "extensions.hpp"
#include "menu_input.hpp"
#include "photo_rows.hpp"
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
    // Dev channel only: start a session for an extension without the pause menu (tests).
    void dev_open(const std::string& id);
    void dev_press(const std::string& key);   // a menu key on the selected row

private:
    using UObject=RC::Unreal::UObject;
    using WeakObject=engine::WeakObject;
    struct Declared { std::string id, entry; std::vector<Manifest::PhotoTab> tabs; };
    struct PauseRow { std::string id; WeakObject row; bool down=false; };
    struct Row { WeakObject widget; bool slider=false; PhotoRow shown; bool fresh=true; uint64_t settle_until=0; };
    struct Tab {
        std::string section; WeakObject tab, page, box; WeakObject icon;
        std::vector<Row> option_rows, slider_rows;   // pooled per kind
        std::vector<std::pair<bool,size_t>> order;  // row i of the page: (slider?, index in its pool)
        std::vector<PhotoRow> rows;                  // what the page shows now
        int selected=-1;
        bool complete=false;                         // every row of the model has its widget
    };

    Deps deps_;
    uint64_t library_revision_=0;
    std::vector<Declared> declared_;
    WeakObject pc_, handler_, component_;
    // pause menu
    std::vector<PauseRow> pause_rows_;
    WeakObject system_page_;
    // session
    std::string session_;                   // extension id that owns the open Photo Mode session
    uint64_t session_started_=0;
    bool session_live_=false;               // Photo Mode was seen active in this session
    WeakObject panel_, strip_, switcher_, strip_box_, scroll_, size_box_;
    std::vector<Tab> tabs_;
    int game_tabs_=0, active_tab_=-1;
    float strip_width_=0, tab_pitch_=0; int first_tab_=0;   // tab strip scrolling, whole tabs at a time
    struct SlotLook { std::array<float,4> padding{}; std::array<std::byte,8> size{}; uint8_t horizontal=0, vertical=0; };
    std::vector<std::pair<WeakObject,SlotLook>> strip_siblings_;   // the strip's row before CSSX wrapped it
    Json model_;
    uint64_t model_revision_=0, slow_at_=0;
    bool rows_dirty_=true;
    std::string pending_confirm_; uint64_t confirm_until_=0;
    std::vector<MenuKeyBinding> keys_;
    std::map<std::string,bool> key_down_;
    uint64_t keys_retry_=0, repeat_at_=0;
    std::string last_error_;
    struct RowLook { uint8_t type=0; bool hover=false; bool known=false; };
    RowLook option_look_, slider_look_;
    WeakObject note_box_, note_text_;   // the description line
    std::string note_shown_;
    bool note_styled_=false, note_visible_=false;

    void refresh_declared();
    void forget() noexcept;
    void report(const std::string& message);
    // pause menu
    void pause_tick(UObject* handler);
    UObject* ensure_pause_row(PauseRow& p,UObject* box,UObject* reference,UObject* after);
    void begin_session(const std::string& id,UObject* system_page);
    void end_session() noexcept;
    // panel
    void panel_tick(UObject* component);
    enum class Attach { done, later, failed };
    Attach attach(UObject* panel);
    void detach_panel() noexcept;
    void wrap_strip(UObject* panel);
    void unwrap_strip() noexcept;
    void learn_row_looks(UObject* box);
    void create_note(UObject* panel);
    void show_note(bool on);
    void update_note(const Tab& tab);
    UObject* icon_texture(const fs::path& file);
    void build_page(Tab& tab,bool slow);
    Row& row_at(Tab& tab,size_t i);
    Row make_row(bool slider);
    void recheck(Tab& tab);
    void recheck_row(Row& row);
    void fill(Row& row,const PhotoRow& value);
    void poll_keys(Tab& tab);
    void press(Tab& tab,const std::string& key);
};
}

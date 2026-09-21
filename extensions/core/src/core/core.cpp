#include "core.hpp"
#include "cssx_version.hpp"
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <Unreal/UnrealVersion.hpp>

namespace cssx {
using namespace engine;
namespace {
int64_t qpc() { LARGE_INTEGER v; QueryPerformanceCounter(&v); return v.QuadPart; }
struct Phase { FrameRing<4096>& ring; int64_t start; Phase(FrameRing<4096>& r):ring(r),start(qpc()) {} ~Phase() { ring.push(qpc()-start); } };
std::string lower(std::string s) { for(auto& c:s) c=char(std::tolower((unsigned char)c)); return s; }
}
Core::Core(const CssxLoaderHost& host):host_(host),root_(host.root),mods_root_(host.mods_root),storage_(root_) {
    std::string note;
    settings_=Settings::load(root_/"settings.json",&note);
    if(!note.empty()) log("info",note);
    // Diagnostic switches (dev/*.txt) isolate the two per-frame mechanisms.
    { std::error_code ec;
      const bool no_writer=fs::exists(root_/"dev/no-writer.txt",ec), no_cache=fs::exists(root_/"dev/no-find-cache.txt",ec);
      engine::set_find_cache(!no_cache);
      if(!no_writer) { writer_.start(); storage_.set_writer(&writer_); }   // log appends never block the game thread
      log("info","Core switches",{{"writer_thread",!no_writer},{"find_cache",!no_cache}}); }
    hotkey_down_.assign(2,true);   // require a release before the first open
    bridge_=std::make_unique<Bridge>(host.hooks);
    hud_.set_logger([this](const std::string& m){ log("warning",m); });
    hud_.set_object_minter([this](UObject* o){ return bridge_->track(o); });
    dev_=std::make_unique<DevChannel>(root_,[this](const Json& r){ return dev_request(r); });
    log("info","CSSX core " CSSX_VERSION " created",{{"dev_channel",dev_->enabled()},{"loader",host.loader_version?host.loader_version:""}});
}
void Core::report_tick_failure(const std::string& what) { try { log("error","Core tick failed: "+what); } catch(...) {} if(host_.log) host_.log(("error: core tick failed: "+what).c_str()); }
void Core::log(const std::string& level,const std::string& message,const Json& fields) {
    try { storage_.log("cssx",level,message,fields); } catch(...) {}
    if(level=="error" || level=="warning") host_.log((level+": "+message).c_str());
}
void Core::check_legacy() {
    std::error_code ec;
    const auto css=mods_root_/"CustomShellSystem";
    legacy_.activation_present=fs::exists(css/"cssx.json",ec) || fs::exists(css/"cores/cssx_core.dll",ec);
    Json mapped=Json::array();
    HMODULE modules[1024]; DWORD needed=0;
    if(EnumProcessModules(GetCurrentProcess(),modules,sizeof modules,&needed)) {
        const auto count=std::min<size_t>(needed/sizeof(HMODULE),1024);
        for(size_t i=0;i<count;++i) {
            wchar_t path[MAX_PATH*4]{};
            if(!GetModuleFileNameW(modules[i],path,MAX_PATH*4)) continue;
            const auto name=lower(path_utf8(fs::path(path).filename()));
            const auto folder=lower(path_utf8(fs::path(path).parent_path()));
            if(name.starts_with("cssx_core") && folder.ends_with("customshellsystem/cores")) mapped.push_back(path_utf8(fs::path(path)));
        }
    }
    legacy_.mapped=mapped;
    Json shared=Json::array();
    if(fs::is_directory(css/"extensions",ec)) for(const auto& entry:fs::directory_iterator(css/"extensions",ec)) {
        if(!entry.is_directory(ec)) continue;
        if(fs::exists(root_/"extensions"/entry.path().filename(),ec)) shared.push_back(path_utf8(entry.path().filename()));
    }
    legacy_.shared_ids=shared;
    ++legacy_.checks;
}
void Core::start_runtime() {
    if(runtime_) return;
    HostServices services;
    services.hud=hud_.api();
    services.request=[this](const Json& r){ return service(r); };
    services.log=[this](const std::string& level,const std::string& message,const Json& fields){ log(level,message,fields); };
    runtime_=std::make_unique<Runtime>(root_,std::move(services));
    if(menu_) menu_->set_runtime(runtime_.get());
    log("info","Extensions started",{{"count",runtime_->count()}});
}
Json Core::service(const Json& request) {
    const auto op=request.at("op").get<std::string>();
    struct Count { std::pair<uint64_t,uint64_t>& slot; uint64_t start; ~Count() { ++slot.first; slot.second+=monotonic_us()-start; } } count{op_stats_[op+(request.contains("function")?":"+request["function"].get<std::string>():request.contains("property")?":"+request["property"].get<std::string>():"")],monotonic_us()};
    if(op=="log") { log(request.value("level",std::string("info")),request.at("message").get<std::string>(),request.value("fields",Json::object())); return nullptr; }
    if(op=="menu.status") return {{"menu_open",menu_ && menu_->is_open()},{"game_menu_open",game_menu_open()}};
    if(op=="menu.close") { if(menu_) menu_->close(); return true; }
    if(op=="input.focus") { DWORD pid=0; const auto window=GetForegroundWindow(); if(window) GetWindowThreadProcessId(window,&pid); return window && pid==GetCurrentProcessId(); }
    if(!engine_) throw std::runtime_error("Game thread is not initialized");
    return bridge_->request(player_,request);
}
void Core::hotkey() {
    // On the CSSX page: close the Player Menu. Elsewhere: open it on the CSSX
    // tab, or switch to it when the Player Menu is already open.
    if(menu_->is_open()) { log("info","Hotkey: closing the Player Menu"); menu_->close(); return; }
    std::string reason; menu_->set_runtime(runtime_.get());
    if(menu_->open(player_,&reason)) log("info","Hotkey: opening the Player Menu on the CSSX tab");
    else log("info","Hotkey ignored: "+reason);
}
bool Core::game_menu_open() const {
    try {
        if(!player_.pc) return false;
        auto* handler=object_of(player_.pc,L"User Interface Handler Component");
        if(!handler) return false;
        // ActiveDisplayMenu is a world actor that also exists at rest points, so
        // it is not a menu-open signal. bIsInGameMenu and ActiveMenu are.
        if(bool_of(handler,L"bIsInGameMenu")) return true;
        if(object_of(handler,L"ActiveMenu")) return true;
        return false;
    } catch(...) { return false; }
}
bool Core::hotkey_pressed(const std::vector<std::string>& keys,size_t slot) {
    if(!player_.pc || keys.empty()) return false;
    bool down=true;
    for(const auto& name:keys) {
        Call call(player_.pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
        member(call.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(wide(name).c_str()));
        call.run(); if(!call.get<bool>()) { down=false; break; }
    }
    const bool fired=down && !hotkey_down_[slot];
    hotkey_down_[slot]=down;
    return fired;
}
void Core::tick(void* engine,float delta) {
    Phase whole(phase_tick_);
    engine_=engine; seconds_+=delta;
    const auto now=GetTickCount64();
    // Developer isolation: with idle set, nothing but the dev channel runs, so
    // a frame-rate comparison can separate the core's per-frame work from the
    // mere presence of the loaded DLLs.
    if(idle_) { if(dev_) dev_->poll(); return; }
    if(!started_) {
        started_=true; start_tick_=now;
        try { if(!Version::IsAtLeast(5,6) || !Version::IsBelow(5,7)) log("warning","CSSX targets UE 5.6; the detected engine version differs"); } catch(...) {}
        check_legacy();
        menu_=std::make_unique<Menu>(Menu::Deps{nullptr,&settings_,[this](const std::string& m){ log("warning",m); },
            [this]{ return status(); },[this]{ settings_.save(root_/"settings.json"); },CSSX_VERSION,root_,[this]{ return perf_brief(); }});
        // CSS is optional. When it is installed and enabled it adds its own Player
        // Menu tab first; CSSX waits briefly for that so CSS's page-count checks
        // keep passing. Without CSS, CSSX attaches at once.
        { std::error_code ec; const auto css=mods_root_/"CustomShellSystem"; css_present_=fs::exists(css/"enabled.txt",ec) && fs::exists(css/"dlls/main.dll",ec); menu_->set_css_present(css_present_); }
        if(extensions_allowed()) start_runtime();
        else log("warning","Legacy CSSX activation or runtime detected in CustomShellSystem; extensions are not loaded",{{"activation",legacy_.activation_present},{"mapped",legacy_.mapped}});
        legacy_check_after_=now+30000;
    }
    try { player_=player_context(engine); } catch(...) { player_={}; }
    if(dev_) dev_->poll();
    // Second legacy check after CSS has had time to start its host lazily.
    if(legacy_check_after_ && now>=legacy_check_after_) {
        legacy_check_after_=0; check_legacy();
        if(extensions_allowed() && !runtime_) start_runtime();
        else if(!extensions_allowed() && runtime_) log("error","A legacy CSSX runtime appeared after start; the standalone extensions stay loaded but hooks may conflict. Run the migration.");
    }
    if(runtime_) {
        if(runtime_->count()) { Phase p(phase_ext_); runtime_->tick(delta); }
        if(runtime_->needs_frame()) {
            Phase p(phase_hud_);
            CssxFrame frame; hud_.update(player_,(menu_ && menu_->is_open()) || game_menu_open(),frame); frame.seconds=delta;
            runtime_->render(frame);
        }
    }
    // Menu: hotkeys sampled at 30 Hz while a player controller exists.
    hotkey_accumulator_+=delta;
    if(player_.pc && hotkey_accumulator_>=1.0/30) {
        hotkey_accumulator_=0;
        bool pressed=false;
        try { const bool keyboard=hotkey_pressed(settings_.open_keyboard,0); const bool gamepad=hotkey_pressed(settings_.open_gamepad,1); pressed=keyboard || gamepad; } catch(...) {}
        // Second keyboard path: UE4SS's raw key polling through the loader.
        if(host_.size>=sizeof(CssxLoaderHost) && host_.take_hotkey && host_.take_hotkey(host_.hotkey_context)>0) { pressed=true; hotkey_down_[0]=true; }
        if(pressed) hotkey();
    }
    { Phase p(phase_menu_); menu_->tick(player_,delta); }
    if(!quiet_ && now>=status_after_) { status_after_=now+5000; publish_status(); }
}
Json Core::frame_stats(double seconds) const {
    const auto* ring=host_.frames;
    Json result={{"scope","Interval between consecutive engine tick callbacks on the game thread (loader ring); not GPU presentation timing."}};
    if(ring && ring->frequency>0) {
        // Take roughly `seconds` worth of the newest frames: bound by the ring.
        auto all=newest_of(ring->intervals,ring->capacity,ring->head,ring->total,ring->capacity);
        std::vector<int64_t> window; int64_t budget=int64_t(seconds*double(ring->frequency));
        for(auto it=all.rbegin();it!=all.rend() && budget>0;++it) { window.push_back(*it); budget-=*it; }
        std::reverse(window.begin(),window.end());
        result["engine"]=summarize(window,ring->frequency).json();
        result["frames_total"]=uint64_t(ring->total);
        auto phase=[&](const FrameRing<4096>& r){ auto v=r.newest(size_t(window.size())); auto s=summarize(v,ring->frequency); long double sum=0; for(auto x:v) sum+=(long double)x; return Json{{"count",v.size()},{"mean_us",v.empty()?0.0:double(sum)*1e6/double(ring->frequency)/double(v.size())},{"max_us",s.max_ms*1000},{"p99_us",s.p99_ms*1000}}; };
        result["phases"]={{"core_tick",phase(phase_tick_)},{"extensions",phase(phase_ext_)},{"hud",phase(phase_hud_)},{"menu",phase(phase_menu_)}};
        // Hitch attribution: for every frame above twice the median, the
        // core's own time in that frame. If the core share stays in
        // microseconds while the frame is tens of milliseconds, the hitch
        // is not CSSX's.
        const auto core=phase_tick_.newest(window.size());
        const double to_ms=1000.0/double(ring->frequency), to_us=1e6/double(ring->frequency);
        const auto engine=summarize(window,ring->frequency);
        Json worst=Json::array(); double core_at_hitches_max_us=0; uint64_t core_hitches=0;
        for(size_t i=0;i<window.size();++i) {
            const double ms=double(window[i])*to_ms; if(ms<=engine.median_ms*2) continue;
            const size_t k=core.size()>=window.size()?i+(core.size()-window.size()):i;
            const double us=k<core.size()?double(core[k])*to_us:0;
            core_at_hitches_max_us=std::max(core_at_hitches_max_us,us);
            if(us>ms*1000*0.25) ++core_hitches;
            worst.push_back({{"frame_ms",ms},{"core_us",us},{"age_frames",window.size()-1-i}});
        }
        std::sort(worst.begin(),worst.end(),[](const Json& a,const Json& b){ return a["frame_ms"].get<double>()>b["frame_ms"].get<double>(); });
        if(worst.size()>12) worst.erase(worst.begin()+12,worst.end());
        result["hitches"]={{"count",engine.hitches},{"core_share_max_us",core_at_hitches_max_us},{"frames_where_core_exceeds_quarter",core_hitches},{"worst",worst}};
    }
    result["hooks"]=bridge_->hook_stats();
    Json ops=Json::array();
    for(const auto& [op,v]:op_stats_) ops.push_back({{"op",op},{"calls",v.first},{"us",v.second}});
    std::sort(ops.begin(),ops.end(),[](const Json& a,const Json& b){ return a["us"].get<uint64_t>()>b["us"].get<uint64_t>(); });
    if(ops.size()>40) ops.erase(ops.begin()+40,ops.end());
    result["requests"]=ops;
    if(menu_) result["menu_builds"]={{"builds",menu_->cost().builds},{"build_us",menu_->cost().build_us},{"last_build_us",menu_->cost().last_build_us},{"widgets",menu_->cost().widgets}};
    return result;
}
Json Core::status() {
    Json s={{"version",CSSX_VERSION},{"pid",GetCurrentProcessId()},{"uptime_s",seconds_},{"player",player_.pawn!=nullptr},
            {"menu_open",menu_ && menu_->is_open()},{"menu_attached",menu_ && menu_->attached()},{"game_menu_open",game_menu_open()},{"css_present",css_present_},{"dev_channel",dev_ && dev_->enabled()},
            {"legacy",{{"activation_present",legacy_.activation_present},{"runtime_mapped",legacy_.mapped},{"shared_extension_ids",legacy_.shared_ids},{"checks",legacy_.checks}}},
            {"extensions_loaded",runtime_?runtime_->count():0},{"hud",hud_.diagnostics()}};
    if(!extensions_allowed()) s["notice"]="Legacy CSSX files are active inside CustomShellSystem. Close the game and run the CSSX migration; extensions stay unloaded until then.";
    return s;
}
void Core::publish_status() { try { writer_.replace(root_/"runtime/status.json",status().dump(2)+"\n"); } catch(...) {} }
Json Core::perf_brief() const {
    // Last ~10 s: engine frame time and the core's own share of it.
    Json out={{"hz",0.0},{"median_ms",0.0},{"core_mean_us",0.0},{"core_max_us",0.0},{"core_p99_us",0.0},{"frames",0}};
    const auto* ring=host_.frames; if(!ring || ring->frequency<=0) return out;
    auto all=newest_of(ring->intervals,ring->capacity,ring->head,ring->total,ring->capacity);
    std::vector<int64_t> window; int64_t budget=int64_t(10*ring->frequency);
    for(auto it=all.rbegin();it!=all.rend() && budget>0;++it) { window.push_back(*it); budget-=*it; }
    const auto engine=summarize(window,ring->frequency);
    const auto core=phase_tick_.newest(window.size()); const auto cs=summarize(core,ring->frequency);
    long double sum=0; for(auto v:core) sum+=(long double)v;
    out["hz"]=engine.hz; out["median_ms"]=engine.median_ms; out["frames"]=engine.count;
    out["core_mean_us"]=core.empty()?0.0:double(sum)*1e6/double(ring->frequency)/double(core.size());
    out["core_max_us"]=cs.max_ms*1000; out["core_p99_us"]=cs.p99_ms*1000;
    return out;
}
Json Core::dev_request(const Json& request) {
    const auto op=request.value("op",std::string{});
    if(op=="status") return status();
    if(op=="frame.stats") return frame_stats(request.value("seconds",10.0));
    if(op=="library" || op=="model" || op=="event") {
        if(!runtime_) throw std::runtime_error("Extensions are not loaded");
        Json forwarded=request; if(request.contains("extension")) forwarded["id"]=request.at("extension");   // "id" is the request id on this channel
        return runtime_->request(forwarded);
    }
#ifdef CSSX_DEV
    if(op=="screenshot") {
        // Steam captures the presented frame; no focus change, no input injection.
        auto module=GetModuleHandleW(L"steam_api64.dll");
        if(!module) throw std::runtime_error("Steam screenshot module unavailable");
        auto get=reinterpret_cast<void*(*)()>(GetProcAddress(module,"SteamAPI_SteamScreenshots_v003"));
        auto trigger=reinterpret_cast<void(*)(void*)>(GetProcAddress(module,"SteamAPI_ISteamScreenshots_TriggerScreenshot"));
        if(!get || !trigger) throw std::runtime_error("Steam screenshot API unavailable");
        auto* screenshots=get(); if(!screenshots) throw std::runtime_error("Steam screenshots interface unavailable");
        trigger(screenshots);
        return {{"screenshot_requested",true}};
    }
#endif
    if(op=="idle") { idle_=request.value("value",true); if(idle_ && menu_) menu_->close(); return {{"idle",idle_}}; }
    if(op=="quiet") { quiet_=request.value("value",true); return {{"quiet",quiet_}}; }
    if(op=="menu.open") { std::string reason; menu_->set_runtime(runtime_.get()); if(!menu_->is_open() && !menu_->open(player_,&reason)) throw std::runtime_error(reason); return true; }
    if(op=="menu.hotkey") { hotkey(); return true; }
    if(op=="menu.key") { menu_->drive_key(request.at("key").get<std::string>()); return true; }
    if(op=="menu.act") { menu_->drive(request.at("action")); return true; }
    if(op=="menu.close") { menu_->close(); return true; }
    if(op=="menu.diagnostics") return menu_->diagnostics();
    if(op=="engine") { if(!request.contains("request")) throw std::runtime_error("engine needs a request"); return service(request.at("request")); }
    if(op=="quit") { if(!player_.world) throw std::runtime_error("No world"); Call quit(find(L"/Script/Engine.Default__KismetSystemLibrary"),L"QuitGame",4); quit.set(L"WorldContextObject",player_.world); quit.set(L"SpecificPlayer",static_cast<UObject*>(nullptr)); quit.set(L"QuitPreference",uint8_t{0}); quit.set(L"bIgnorePlatformRestrictions",false); quit.run(); return true; }
    throw std::runtime_error("Unknown dev request: "+op);
}
bool Core::stop() {
    if(stopped_) return true;
    if(menu_) menu_->detach();
    if(runtime_ && !runtime_->stop()) { log("warning","Core stop deferred: an extension has not restored its changes"); return false; }
    if(!bridge_->stop_hooks()) { log("warning","Core stop deferred: hook cleanup pending"); return false; }
    hud_.release();
    stopped_=true; publish_status();
    log("info","CSSX core stopped");
    return true;
}
namespace {
void* create(const CssxLoaderHost* host) {
    try { if(!host || host->abi!=CSSX_CORE_ABI || host->size<sizeof(CssxLoaderHost) || !host->root) return nullptr; return new Core(*host); }
    catch(const std::exception& e) { if(host && host->log) host->log((std::string("CSSX core creation failed: ")+e.what()).c_str()); return nullptr; }
    catch(...) { return nullptr; }
}
void tick(void* core,void* engine,float delta) {
    // An exception here would otherwise vanish (OutputDebugString is invisible
    // under Proton); log each distinct message once through the loader.
    static std::string last;
    try { static_cast<Core*>(core)->tick(engine,delta); }
    catch(const std::exception& e) { if(last!=e.what()) { last=e.what(); static_cast<Core*>(core)->report_tick_failure(last); } }
    catch(...) { if(last!="unknown") { last="unknown"; static_cast<Core*>(core)->report_tick_failure("non-standard exception"); } }
}
int stop(void* core) { try { return static_cast<Core*>(core)->stop()?1:0; } catch(...) { return 0; } }
void destroy(void* core) { delete static_cast<Core*>(core); }
const CssxCoreApi api{CSSX_CORE_ABI,sizeof(CssxCoreApi),CSSX_VERSION,create,tick,stop,destroy};
}
}
extern "C" __declspec(dllexport) const CssxCoreApi* cssx_core_api() { return &cssx::api; }

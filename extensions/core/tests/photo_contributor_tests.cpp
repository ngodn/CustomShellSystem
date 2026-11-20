#include "photo_rows.hpp"
#include "photo_input.hpp"
#include "photo_session.hpp"
#include "../src/core/widget_calls.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace cssx;
namespace {
void check(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}
struct Directory {
    fs::path path=fs::temp_directory_path()/("cssx-photo-"+std::to_string(monotonic_us()));
    Directory() { if(!fs::create_directory(path)) throw std::runtime_error("Cannot create fixture directory"); }
    ~Directory() { std::error_code ec; fs::remove_all(path,ec); }
};

Json menu(const std::string& label) {
    Json control={{"id","go"},{"type","button"},{"label",label}};
    return {{"schema",2},{"sections",Json::array({{{"id","shared"},{"title","Shared"},{"controls",Json::array({control})}}})}};
}

void install(const fs::path& root,std::string id,std::string title,std::string target={},std::string floor={}) {
    auto dir=root/"extensions"/id;
    fs::create_directories(dir);
    Json manifest={{"schema",2},{"api",3},{"id",id},{"title",title},{"author","Test"},{"version","2.10.0"},
                   {"kind","lua"},{"entry","main.lua"},{"menu","menu.json"}};
    Json photo={{"tabs",Json::array({{{"section","shared"},{"icon","icon.png"}}})}};
    if(target.empty()) {
        photo["entry"]="Open "+title;
        manifest["photo_mode"]=photo;
    } else {
        photo["target"]=target;
        if(!floor.empty()) photo["min_version"]=floor;
        manifest["photo_mode_contribution"]=photo;
    }
    std::ofstream(dir/"extension.json")<<manifest;
    std::ofstream(dir/"menu.json")<<menu(id);
    std::ofstream(dir/"icon.png")<<"fixture";
    std::ofstream(dir/"main.lua")<<R"(
        return {
            model = function()
                local result = cssx.request {op="probe.model"}
                if result == "fail" then error("fixture model failure") end
                return {}
            end,
            event = function(e)
                local result = cssx.request {op="probe.event", value=e.value or e.id}
                if result == "fail" then error("fixture event failure") end
            end
        }
    )";
}

void open_all(PhotoSession& session,Runtime& runtime,const PhotoModeDefinition& mode) {
    session.open(runtime,mode);
    while(session.open_next(runtime)) {}
}

// The game can accept the reflected AddChild call but return no panel slot.
struct RejectRow {
    RejectRow(int*,const wchar_t* function,int count) {
        check(std::wstring_view(function)==L"AddChildToVerticalBox" && count==2,"wrong row function");
    }
    std::array<int,2> parameters() const { return {0,1}; }
    engine::WidgetParam describe_widget_parameter(int field) const {
        using namespace engine;
        return {field?L"ReturnValue":L"content",WidgetParamType::object,field?WidgetParamRole::result:WidgetParamRole::input};
    }
    void set(const wchar_t* name,int*) { check(std::wstring_view(name)==L"content","wrong row parameter"); }
    void run() {}
    template<class T> T get(const wchar_t*) const { return nullptr; }
};

void check_guest_failure_isolation() {
    Directory dir;
    const std::string owner="test.owner", guest="test.guest";
    install(dir.path,owner,owner);
    install(dir.path,guest,guest,owner);
    int warnings=0, owner_actions=0, owner_closes=0, guest_opens=0;
    bool throw_on_open=true;
    HostServices host;
    host.log=[&](const std::string&,const std::string& msg,const Json& fields) {
        if(msg=="Photo Mode contribution disabled") {
            ++warnings;
            check(fields.at("extension")==guest,"wrong failure attribution");
        }
    };
    host.request=[&](const Json& r)->Json {
        if(r.at("op")=="probe.event") {
            if(r.at("extension")==owner && r.at("value")=="go") ++owner_actions;
            if(r.at("extension")==owner && r.at("value")=="closed") ++owner_closes;
            if(r.at("extension")==guest && r.at("value")=="open") {
                ++guest_opens;
                if(throw_on_open) return "fail"; // fixture raises a Lua error
            }
        }
        return "ok";
    };
    Runtime runtime(dir.path,host);
    PhotoSession session;
    auto mode=runtime.photo_modes().at(0);
    auto& provider=*mode.contributors.at(0);
    auto owner_button=[&] {
        auto model=runtime.photo_model(*mode.owner);
        const auto* control=photo_control(model,"shared","go");
        check(control!=nullptr,"owner button lost");
        runtime.photo_action(*mode.owner,photo_event(*control,"accept",false));
    };
    for(int attempt=0;attempt<4;++attempt) {
        session.open(runtime,mode);
        owner_button();
        check(guest_opens==attempt && !session.includes(guest),"owner handles input before guest open");
        check(session.open_next(runtime) && !session.open_next(runtime),"guest opens once");
        if(attempt==1 || attempt==2) {
            session.request({{"op","photo.step_out"},{"extension",guest}});
            check(!runtime.photo_tab(provider,[&]{
                const auto model=runtime.photo_model(provider);
                const auto rows=photo_tab_rows(model,provider,attempt==1?"shared":"unknown");
                check(rows.size()==1,"fixture needs a row");
                int box=0,row=0;
                engine::add_photo_row<RejectRow>(&box,&row);
            }),"row failure must stay within the tab");
        }
        if(attempt<3) {
            auto remaining=runtime.photo_modes().at(0);
            check(session.prune(runtime,remaining) && !session.prune(runtime,remaining),"single owner focus recovery");
            check(!session.includes(guest) && remaining.tabs.size()==1 && session.includes(owner),"only the failed guest leaves");
            check(session.take_step().kind==PhotoSession::Step::none,"failed guest cannot park the owner");
            runtime.disable_photo_contribution(provider,"duplicate");
            check(warnings==attempt+1,"one reason per failed guest");
        } else check(session.includes(guest),"guest can join a fresh session");
        owner_button();
        check(owner_actions==2*(attempt+1) && owner_closes==attempt,"owner action still delivered without reopening");
        session.end(runtime,false);
        throw_on_open=false;
        mode=runtime.photo_modes().at(0);
        check(mode.contributors.size()==1,"next genuine open retries the guest");
    }
    check(runtime.stop(),"fixture unloaded");
}

void check_stalled_guest_tick() {
    Directory dir;
    const std::string owner="test.owner",guest="test.guest";
    install(dir.path,owner,owner);
    install(dir.path,guest,guest,owner);
    std::ofstream(dir.path/"extensions"/guest/"main.lua")<<R"(
        local active=false
        return { model=function() return {} end,
            event=function(e) active=e.value=="open" end,
            tick=function() if active then while true do end end end }
    )";
    int warnings=0,owner_actions=0;
    HostServices host;
    host.log=[&](const std::string&,const std::string& message,const Json&) {
        if(message=="Photo Mode contribution disabled") ++warnings;
    };
    host.request=[&](const Json& r)->Json {
        if(r.at("extension")==owner && r.value("value","")=="go") ++owner_actions;
        return "ok";
    };
    Runtime runtime(dir.path,host);
    auto mode=runtime.photo_modes().at(0);
    PhotoSession session;
    open_all(session,runtime,mode);
    runtime.tick(.2);
    check(session.prune(runtime,runtime.photo_modes().at(0)) && !session.includes(guest),"stalled Lua guest pruned after instruction limit");
    runtime.tick(.2);
    runtime.photo_action(*mode.owner,{{"id","go"}});
    check(warnings==1 && owner_actions==1 && session.includes(owner),"owner handles input after stalled guest tick");
    session.end(runtime,false);
    check(runtime.stop(),"stalled guest fixture unloaded");
}

void check_native_row_input() {
    using S=PhotoInputSource;
    struct Binding { const wchar_t* name; S source; };
    const Binding functions[]={
        {L"HandleNavigableInput",S::navigation},
        {L"BndEvt__WBP_NB_Photomode_WBP_ArrowButton_L_K2Node_ComponentBoundEvent_8_OnClicked__DelegateSignature",S::arrow_left},
        {L"BndEvt__WBP_NB_Photomode_WBP_ArrowButton_R_K2Node_ComponentBoundEvent_10_OnContinuousTrigger__DelegateSignature",S::arrow_right},
        {L"BndEvt__WBP_NB_Option_Slider_WBP_ArrowButton_R_K2Node_ComponentBoundEvent_7_OnClicked__DelegateSignature",S::arrow_right},
        {L"BndEvt__WBP_NB_Option_Slider_WBP_ArrowButton_L_K2Node_ComponentBoundEvent_10_OnContinuousTrigger__DelegateSignature",S::arrow_left},
        {L"BndEvt__WBP_NavigationButton_ButtonWidget_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature",S::button},
        {L"BndEvt__WBP_NavigationButton_ButtonWidget_K2Node_ComponentBoundEvent_4_OnButtonReleasedEvent__DelegateSignature",S::button_up},
        {L"BndEvt__WBP_NB_Photomode_WBP_ArrowButton_L_K2Node_ComponentBoundEvent_3_OnHovered__DelegateSignature",S::unrelated},
        {L"OnMouseWheel",S::unrelated},{L"OnMouseMove",S::unrelated}
    };
    for(auto binding:functions) check(photo_input_source(binding.name)==binding.source,"wrong native widget binding");
    struct Function {
        int bytes=0,params=0,entry=0;
        Function* graph=nullptr;
        int GetNumParms() const { return params; }
        int GetParmsSize() const { return bytes; }
        int GetEventGraphCallOffset() const { return entry; }
        Function* GetEventGraphFunction() const { return graph; }
    };
    Function graph{4,1},left{0,0,1466,&graph},right{0,0,1456,&graph},body,nav{2,2};
    std::map<Function*,PhotoInputHook> registered;
    unsigned subscriptions=0;
    auto subscribe=[&](Function*) { return ++subscriptions; };
    bind_photo_script(registered,&left,S::arrow_left,-1,subscribe);
    bind_photo_script(registered,&right,S::arrow_right,-1,subscribe);
    bind_photo_script(registered,&body,S::button,-1,subscribe);
    bind_photo_script(registered,&nav,S::navigation,0,subscribe);
    const auto& shared=registered.at(&graph);
    check(subscriptions==3 && !registered.contains(&left) && !registered.contains(&right),
          "optimized stubs share one real graph subscription");
    check(shared.offset==0 && shared.route.source(1466)==S::arrow_left && shared.route.source(1456)==S::arrow_right &&
          shared.route.source(200)==S::unrelated,"only the selected bound delegate reaches the row");
    check(registered.at(&body).route.source(0)==S::button && registered.at(&nav).offset==0,
          "ordinary button and parameterized navigation functions remain direct subscriptions");
    for(int i=0;i<256;++i) {
        std::string_view expected=i==5 || i==6?"left":i==7 || i==8?"right":i==13?"accept":"";
        check(photo_command(S::navigation,uint8_t(i),false,true)==expected,"stock input enum routed to wrong action");
    }
    check(photo_command(S::button,0,true,false)=="accept" && photo_command(S::button,0,false,true).empty(),
          "only an action button accepts a main-body click");
    check(photo_command(S::arrow_left,0,true,false).empty() && photo_command(S::arrow_right,0,false,true)=="right",
          "arrow clicks require an adjustable row");
    PhotoInputQueue queue;
    for(int i=0;i<3;++i) queue.push({11,"speed","right",S::navigation});
    check(queue.take().size()==3,"the native listener owns repeat cadence");
    PhotoGesture click{11,"play","accept",S::button},confirm{11,"play","accept",S::navigation};
    queue.down(11,"play",true);
    queue.push(confirm);
    check(queue.take().size()==1,"pad press delivers once");
    queue.up(11,"play");
    queue.push(click);
    check(queue.take().empty(),"the later simulated release cannot deliver a second press");
    queue.down(11,"play",true);
    queue.up(11,"play");
    queue.push(click);
    queue.push(confirm);
    check(queue.take().size()==1,"a synthetic click alone works, paired navigation does not duplicate it");
    queue.down(11,"play",false);
    queue.push(confirm);
    queue.push(click);
    check(queue.take().size()==2,"independent mouse and keyboard actions each deliver");
    queue.push(click);
    queue.clear();
    check(queue.take().empty(),"tab change discards stale gestures");
    check(!click.matches(12,"play") && !click.matches(11,"exit"),"pooled row or replaced widget took stale input");
    PhotoBounds icon{90,10,130,35},strip{0,0,100,40};
    auto clipped=icon.clipped(strip);
    check(clipped && clipped->left==90 && clipped->right==100 && !icon.clipped({0,0,40,40}),
          "diagnostic rectangle must exclude the scrolled-off portion");
}
struct StreamProbe {
    int starts=0,polls=0,resident=0,pending=0,late=0;
    bool finish=false,throw_start=false;
};
class FakeStream final : public AssetStream {
    StreamProbe& state_;
    std::string path_;
    int held_=0;
public:
    explicit FakeStream(StreamProbe& state):state_(state) {}
    ~FakeStream() override { cancel_path(); state_.resident-=held_; }
    void start(const std::string& path) override {
        check(path_.empty(),"only one outstanding path per stream");
        ++state_.starts; path_=path; ++state_.pending;
        if(state_.throw_start) throw std::runtime_error("fixture start failed");
    }
    std::optional<uint64_t> poll() override {
        ++state_.polls;
        if(!state_.finish) return {};
        bool missing=path_.ends_with("Missing");
        --state_.pending; path_.clear();
        if(missing) return 0;
        ++held_; ++state_.resident;
        return 100+held_;
    }
    void cancel_path() noexcept override {
        if(!path_.empty()) { --state_.pending; ++state_.late; path_.clear(); }
    }
};

void check_async_assets() {
    Directory dir;
    const std::string owner="test.owner",guest="test.guest";
    for(const auto& id:{owner,guest}) {
        install(dir.path,id,id,id==owner?"":owner);
        std::ofstream(dir.path/"extensions"/id/"main.lua")<<R"(
            return { model=function() return {} end, event=function(e)
                if e.id=="cssx.probe" then
                    local result,err=cssx.request(e.value)
                    cssx.request{op="test.reply",result=result,err=err}
                elseif e.id=="cssx.asset_load" then
                    local release=cssx.request{op="test.notice",value=e.value}
                    if release then cssx.request{op="asset.release",handle=e.value.handle} end
                end
            end }
        )";
    }
    StreamProbe probe;
    Json reply; std::vector<Json> events;
    bool release_in_callback=false;
    HostServices host;
    host.asset_stream=[&]{ return std::make_unique<FakeStream>(probe); };
    host.request=[&](const Json& r)->Json {
        if(r.at("op")=="test.notice") { events.push_back(r); return release_in_callback; }
        reply=r;
        return nullptr;
    };
    Runtime runtime(dir.path,host);
    PhotoSession session;
    auto mode=runtime.photo_modes().front();
    auto ask=[&](const std::string& who,Json args) {
        reply=nullptr; runtime.host_event(who,"probe",args); return reply;
    };
    const std::string a="/Game/A.A",b="/Game/B.B",missing="/Game/Absent.Missing";
    auto request=[&](const std::string& who,Json paths) { return ask(who,{{"op","asset.load_async"},{"paths",paths}}); };
    auto start=[&](const std::string& who) { return request(who,Json::array({a,b})).at("result").get<uint64_t>(); };
    auto release=[&](const std::string& who,uint64_t id) { return ask(who,{{"op","asset.release"},{"handle",id}}).at("result").get<bool>(); };
    auto tick=[&] {
        int before=probe.starts+probe.polls;
        runtime.tick(.016);
        check(probe.starts+probe.polls-before<=1 && probe.pending<=2,"global stream work exceeded per-frame cap");
    };
    auto empty=[&] { check(probe.resident==0 && probe.pending==0,"scope retained an asset or pending stream"); };
    check(request(owner,Json::array({"relative"})).contains("err"),"invalid paths rejected");
    check(request(owner,Json::array()).contains("err"),"empty batch rejected");
    check(request(owner,std::vector<std::string>(33,a)).contains("err"),"oversized batch rejected");
    auto id=request(guest,Json::array({a,b,a})).at("result").get<uint64_t>();
    check(!release(owner,id) && probe.starts==0,"queue returns immediately and caller owns the handle");
    tick(); tick();
    check(events.empty() && probe.pending==1,"pending IO does not deliver completion or block");
    probe.finish=true;
    tick(); tick(); tick();
    check(events.size()==2 && events[0]["extension"]==guest && events[0]["value"]["state"]=="progress" &&
          events[1]["value"]["state"]=="complete" && events[1]["value"]["finished"]==2 &&
          events[1]["value"]["total"]==2 && events[1]["value"]["results"][a]["$object"]==101 &&
          events[1]["value"]["results"][b]["$object"]==102 && probe.resident==2,"deduplicated paths complete and stay resident");
    tick(); check(events.size()==2,"terminal event delivered once");
    check(release(guest,id) && !release(guest,id),"double release is harmless"); empty();
    id=request(owner,Json::array({a,missing})).at("result").get<uint64_t>();
    for(int i=0;i<4;++i) tick();
    check(events.back()["value"]["state"]=="failed" && events.back()["value"]["results"][missing].is_null() &&
          events.back()["value"]["results"][a]["$object"]==101 && probe.resident==1,"partial results retained on failure");
    release(owner,id); empty();
    probe.throw_start=true; id=start(owner);
    tick(); tick();
    check(events.back()["value"]["state"]=="failed" && events.back()["value"]["errors"].size()==2,"submission errors finish the batch");
    release(owner,id); probe.throw_start=false; empty();
    id=start(owner); check(release(owner,id),"cancel queued request"); empty();
    for(int exit=0;exit<5;++exit) {
        open_all(session,runtime,mode); id=start(guest);
        tick(); tick(); tick();
        check(probe.resident==1 && probe.pending==1,"exit fixture holds a resident and a pending path");
        auto seen=events.size();
        if(exit==0) release(guest,id);
        if(exit==1) {
            session.park(runtime);
            check(probe.resident==1 && probe.pending==1,"park preserves the held batch");
            open_all(session,runtime,mode);
            check(probe.resident==1 && probe.pending==1,"resume preserves the same handle");
            session.end(runtime,false);
        }
        if(exit==2) session.end(runtime,false);
        if(exit==3) session.end(runtime,true);
        if(exit==4) runtime.world_changed();
        empty(); tick(); check(events.size()==seen && !release(guest,id),"released work cannot deliver a late event");
        session.end(runtime,true);
    }
    release_in_callback=true; id=start(owner);
    tick(); tick(); empty(); check(!release(owner,id),"release from progress callback is safe");
    release_in_callback=false;
    id=start(owner); auto second=start(owner); auto third=start(guest);
    check(request(owner,Json::array({a})).contains("err"),"per-owner hold limit");
    probe.finish=false;
    for(int i=0;i<12;++i) tick();
    check(probe.pending==2,"global in-flight limit");
    release(owner,id); release(owner,second); release(guest,third); empty();
    probe.finish=true; open_all(session,runtime,mode); start(guest); tick(); tick(); tick();
    runtime.disable_photo_contribution(*mode.contributors.front(),"fixture failure"); empty();
    start(owner); tick(); tick(); tick(); check(runtime.stop(),"unload releases pending and resident assets"); empty();
}

void check_pause_tick_scopes() {
    Directory dir;
    const std::string owner="test.owner",guest="test.guest";
    for(const auto& id:{owner,guest}) {
        install(dir.path,id,id,id==owner?"":owner);
        std::ofstream(dir.path/"extensions"/id/"main.lua")<<R"(
            return { model=function() return {} end, event=function(e)
                if e.id~="cssx.probe" then return end
                local result,err=cssx.request{op="component.pause_tick."..e.value.op,target={["$object"]=e.value.id}}
                cssx.request{op="test.reply",result=result,err=err}
            end }
        )";
    }
    std::map<uint64_t,bool> components{{11,false},{12,true},{14,false}};
    bool fail_restore=false;
    HostServices host;
    host.component_pause_tick=[&](uint64_t id,std::optional<bool> write)->std::optional<bool> {
        auto it=components.find(id);
        if(it==components.end()) return {};
        if(id==11 && write==false && fail_restore) throw std::runtime_error("transient restore failure");
        const bool original=it->second;
        if(write) it->second=*write;
        return original;
    };
    Json reply;
    host.request=[&](const Json& request)->Json { reply=request; return nullptr; };
    Runtime runtime(dir.path,host);
    PhotoSession session;
    auto mode=runtime.photo_modes().front();
    auto ask=[&](const std::string& caller,const char* op,int id=11) {
        reply=nullptr;
        runtime.host_event(caller,"probe",{{"op",op},{"id",id}});
        return reply;
    };
    auto acquire=[&] {
        check(ask(guest,"acquire").at("result")==false && components.at(11),"acquire records and enables a false bit");
        check(ask(guest,"acquire").at("result")==false,"repeat acquire preserves first baseline");
        check(ask(guest,"acquire",12).at("result")==true,"already-enabled bit preserved");
    };
    auto restored=[&] { check(!components.at(11) && components.at(12),"restore original false and true bits"); };
    check(ask(guest,"read").at("result")==false && ask(guest,"acquire").contains("err"),"read outside session, acquire restricted");
    open_all(session,runtime,mode);
    acquire();
    check(ask(guest,"read").at("result")==true,"read returns current held value");
    check(ask(owner,"acquire").contains("err") && ask(owner,"release").at("result")==false && components.at(11),"other provider cannot steal or release");
    check(ask(guest,"release").at("result")==true && ask(guest,"release").at("result")==false,"release is idempotent");
    restored();
    acquire(); session.park(runtime); restored();
    check(ask(guest,"acquire").contains("err"),"parked session cannot reacquire");
    open_all(session,runtime,mode); acquire(); session.end(runtime,false); restored();
    open_all(session,runtime,mode); acquire(); session.end(runtime,true); restored();
    open_all(session,runtime,mode); acquire();
    check(ask(owner,"acquire",14).at("result")==false,"second owner has an independent component");
    fail_restore=true;
    runtime.world_changed();
    check(components.at(11) && !components.at(14),"one failed restore cannot block another owner");
    fail_restore=false;
    runtime.tick(0.01); restored();
    check(ask(guest,"acquire").contains("err"),"world transition closes scope before callbacks");
    session.end(runtime,true); open_all(session,runtime,mode); acquire();
    components.erase(11);
    components[13]=false;
    check(ask(guest,"release").at("result")==true && !components.at(13),"expired handle cannot write its replacement");
    components[11]=false;
    acquire(); runtime.disable_photo_contribution(*mode.contributors.front(),"fixture failure"); restored();
    check(ask(owner,"acquire").at("result")==false,"owner can acquire after guest retires");
    check(runtime.stop(),"unload releases the outstanding owner override"); restored();
}

}

int main() try {
    check_native_row_input();
    check_guest_failure_isolation();
    check_stalled_guest_tick();
    check_async_assets();
    check_pause_tick_scopes();
    Directory dir;
    const std::string owner="test.host", guest="test.b", other="test.a";
    install(dir.path,owner,"Owner");
    install(dir.path,guest,"Alpha",owner,"2.9.9");
    install(dir.path,other,"Zulu",owner);
    install(dir.path,"test.old","Old",owner,"2.11.0");
    install(dir.path,"test.missing","Missing","test.absent");
    install(dir.path,"test.nonhost","Nonhost",guest);

    auto guest_dir=dir.path/"extensions"/guest;
    const auto manifest=read_json(guest_dir/"extension.json");
    auto parsed=Manifest::parse(manifest,guest_dir).json();
    check(!parsed.contains("photo_mode") && parsed["photo_mode_contribution"]["min_version"]=="2.9.9","library echoes the contributor's declaration");
    for(auto field:{"self","legacy","both","bad_floor","duplicate","escape"}) {
        auto bad=manifest;
        auto& declaration=bad["photo_mode_contribution"];
        const std::string kind=field;
        if(kind=="self") declaration["target"]=guest;
        if(kind=="legacy") { bad["schema"]=1; bad["api"]=1; }
        if(kind=="both") bad["photo_mode"]=Json::object();
        if(kind=="bad_floor") declaration["min_version"]="2.x";
        if(kind=="duplicate") declaration["tabs"].push_back(declaration["tabs"][0].get<Json>());
        if(kind=="escape") declaration["tabs"][0]["icon"]="../test.a/icon.png";
        bool rejected=false;
        try { (void)Manifest::parse(bad,guest_dir); } catch(const std::exception&) { rejected=true; }
        check(rejected,field);
    }

    std::map<std::string,std::vector<std::string>> events;
    std::map<std::string,int> models;
    int idle_logs=0, failure_logs=0;
    std::string fault, slow;
    HostServices host;
    host.log=[&](const std::string&,const std::string& message,const Json&){
        idle_logs+=message=="Photo Mode contribution idle";
        failure_logs+=message=="Photo Mode contribution disabled";
    };
    host.request=[&](const Json& r)->Json {
        const auto id=r.at("extension").get<std::string>();
        const auto op=r.at("op").get<std::string>();
        if(op=="probe.model") ++models[id];
        else events[id].push_back(r.at("value").get<std::string>());
        if(id==slow && op=="probe.model") std::this_thread::sleep_for(std::chrono::milliseconds(65));
        return fault==id+op?"fail":"ok";
    };
    {
        Runtime runtime(dir.path,host);
        auto modes=runtime.photo_modes();
        check(modes.size()==1 && idle_logs==3,"only the owner creates a mode and incompatible targets log once");
        const auto& mode=modes.front();
        check(mode.tabs.size()==3 && mode.tabs[0].provider->id==owner &&
              mode.tabs[1].provider->id==other && mode.tabs[2].provider->id==guest,"tabs ordered by owner, then id, independent of title");
        PhotoSession session;
        open_all(session,runtime,mode);
        check(session.includes(owner) && session.includes(guest) && !session.includes("test.old"),"only attached participants can step");
        const auto before=models;
        for(const auto& tab:mode.tabs) {
            auto model=runtime.photo_model(*tab.provider);
            auto rows=photo_rows(model,tab.spec->section);
            check(rows.size()==1 && rows.front().name==tab.provider->id,"each tab reads its own model with overlapping ids");
            if(tab.provider->id==guest)
                runtime.photo_action(*tab.provider,photo_event(*photo_control(model,tab.spec->section,"go"),"accept",false));
        }
        check(models[owner]==before.at(owner)+1 && models[guest]==before.at(guest)+1,"only the selected provider refreshes");
        bool refused=false;
        try { session.request({{"op","photo.step_out"},{"extension","test.old"}}); }
        catch(const std::exception&) { refused=true; }
        check(refused && !session.has_step(),"nonparticipant cannot change the owner's session");
        session.request({{"op","photo.step_out"},{"extension",guest}});
        check(session.take_step().kind==PhotoSession::Step::out,"attached guest can step out");
        session.park(runtime);
        session.request({{"op","photo.step_in"},{"extension",guest},{"section","shared"}});
        const auto step=session.take_step();
        check(step.kind==PhotoSession::Step::in && mode.focus_tab(step.provider,step.section)==2 &&
              mode.focus_tab(owner,step.section)==0 && mode.focus_tab(guest,"unknown")==0,"return focuses the caller's tab with fallback to owner");
        open_all(session,runtime,mode);
        session.end(runtime,false);
        check(events[owner]==std::vector<std::string>{"open","parked","open","closed"},"owner's original event sequence");
        check(events[guest]==std::vector<std::string>{"open","go","step_out","parked","step_in","open","closed"},"guest actions and full lifecycle");
        check(events[other]==std::vector<std::string>{"open","step_out","parked","step_in","open","closed"},"second guest receives no sibling action");

        open_all(session,runtime,mode);
        const auto topology=runtime.photo_revision();
        auto menu_path=dir.path/"extensions"/guest/"menu.json";
        const auto previous=fs::last_write_time(menu_path);
        std::ofstream(menu_path)<<menu("Changed");
        fs::last_write_time(menu_path,previous+std::chrono::seconds(2));
        for(int i=0;i<5;++i) runtime.tick(.25);
        check(photo_rows(runtime.photo_model(*mode.tabs[2].provider),"shared")[0].name=="Changed","menu reload updates its own page");
        check(topology==runtime.photo_revision() && events[guest].back()=="open","menu rebuild preserves session and tab membership");

        fault=other+"probe.event";
        try { runtime.photo_action(*mode.tabs[1].provider,{{"id","go"}}); } catch(const std::exception&) {}
        fault.clear();
        auto surviving=runtime.photo_modes();
        check(surviving[0].tabs.size()==2 && surviving[0].tabs[1].provider->id==guest,"failed callback removes only its own contribution");
        session.prune(runtime,surviving[0]);
        check(!session.includes(other) && events[other].back()=="detached","failed contributor detached from session");

        slow=guest;
        runtime.photo_action(*mode.tabs[2].provider,{{"id","go"}});
        const auto started=monotonic_us();
        try { (void)runtime.photo_model(*mode.tabs[2].provider); } catch(const std::exception&) {}
        std::cout<<"Slow contributor model returned after "<<monotonic_us()-started<<" us\n";
        surviving=runtime.photo_modes();
        session.prune(runtime,surviving[0]);
        check(surviving[0].tabs.size()==1 && !session.includes(guest) && failure_logs==2,"late model is retired once");
        const auto guest_calls=models[guest];
        runtime.photo_action(*mode.owner,{{"id","go"}});
        check(events[owner].back()=="go" && models[guest]==guest_calls && idle_logs==3,"owner actions remain usable without calling guests");
        session.end(runtime,true);
        check(runtime.stop(),"stop original runtime");
    }
    slow.clear();
    for(const auto& changed:{guest,owner}) {
        install(dir.path,changed,"Reloaded",changed==owner?"":owner);
        Runtime runtime(dir.path,host);
        auto mode=runtime.photo_modes().front();
        check(mode.tabs.size()==3,"reloading either manifest reattaches contributors");
        PhotoSession session;
        open_all(session,runtime,mode);
        session.park(runtime);
        session.end(runtime,true);
        check(!session.includes(owner) && events[guest].back()=="detached","reloading a parked session releases the guest");
        check(runtime.stop(),"stop reloaded runtime");
    }
    check(photo_first_visible(0,17,18,8)==10 && photo_first_visible(10,4,18,8)==4 &&
          photo_first_visible(4,7,18,8)==4,"18-tab strip scrolls both ways by whole tabs");
    check(!photo_version_matches("2.9.1-beta","2.0.0") && photo_version_matches("any",""),"opaque versions require an omitted floor");
    fs::remove_all(dir.path/"extensions");
    install(dir.path,owner,"Alone");
    Runtime alone(dir.path,host);
    auto mode=alone.photo_modes().front();
    auto before=models;
    auto topology=alone.photo_revision();
    PhotoSession solo;
    solo.open(alone,mode);
    for(int i=0;i<1000;++i) alone.invalidate(owner);
    solo.end(alone,false);
    check(mode.tabs.size()==1 && mode.contributors.empty() && topology==alone.photo_revision() && before[guest]==models[guest],"solo owner has no guest dispatch or layout rebuild on invalidation");
    check(alone.stop(),"stop solo runtime");
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }

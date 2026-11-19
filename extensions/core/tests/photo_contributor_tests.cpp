#include "photo_rows.hpp"
#include "photo_session.hpp"
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
    session.open(runtime,mode);
    acquire();
    check(ask(guest,"read").at("result")==true,"read returns current held value");
    check(ask(owner,"acquire").contains("err") && ask(owner,"release").at("result")==false && components.at(11),"other provider cannot steal or release");
    check(ask(guest,"release").at("result")==true && ask(guest,"release").at("result")==false,"release is idempotent");
    restored();
    acquire(); session.park(runtime); restored();
    check(ask(guest,"acquire").contains("err"),"parked session cannot reacquire");
    session.open(runtime,mode); acquire(); session.end(runtime,false); restored();
    session.open(runtime,mode); acquire(); session.end(runtime,true); restored();
    session.open(runtime,mode); acquire();
    check(ask(owner,"acquire",14).at("result")==false,"second owner has an independent component");
    fail_restore=true;
    runtime.world_changed();
    check(components.at(11) && !components.at(14),"one failed restore cannot block another owner");
    fail_restore=false;
    runtime.tick(0.01); restored();
    check(ask(guest,"acquire").contains("err"),"world transition closes scope before callbacks");
    session.end(runtime,true); session.open(runtime,mode); acquire();
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
        session.open(runtime,mode);
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
        session.open(runtime,mode);
        session.end(runtime,false);
        check(events[owner]==std::vector<std::string>{"open","parked","open","closed"},"owner's original event sequence");
        check(events[guest]==std::vector<std::string>{"open","go","step_out","parked","step_in","open","closed"},"guest actions and full lifecycle");
        check(events[other]==std::vector<std::string>{"open","step_out","parked","step_in","open","closed"},"second guest receives no sibling action");

        session.open(runtime,mode);
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
        session.open(runtime,mode);
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

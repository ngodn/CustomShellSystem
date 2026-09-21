// Portable checks: manifest schemas, model validation, storage layout,
// settings recovery, frame statistics. No engine, no Windows.
#include "manifest.hpp"
#include "controls.hpp"
#include "storage.hpp"
#include "writer.hpp"
#include "settings.hpp"
#include "frame_stats.hpp"
#include <fstream>
#include <iostream>
using namespace cssx;
int main() {
    unsigned checks=0;
    auto check=[&](bool ok,const char* what){if(!ok) {std::cerr<<"FAILED check "<<checks<<": "<<what<<"\n";std::exit(1);}++checks;};
    auto rejects=[&](auto operation,const char* what){bool threw=false;try{operation();}catch(const std::exception&){threw=true;}check(threw,what);};
    const auto root=fs::temp_directory_path()/"cssx-core-tests-\u6d4b\u8bd5";
    fs::remove_all(root);fs::create_directories(root/"valid");
    std::ofstream(root/"valid/main.lua")<<"return {}";
    Json legacy={{"schema",1},{"api",1},{"id","test.extension"},{"title","Test"},{"author","Author"},{"version","1.0.0"},{"kind","lua"},{"layout","tabs"},{"entry","main.lua"}};
    auto m=Manifest::parse(legacy,root/"valid");check(m.schema==1 && m.api==1 && m.layout=="tabs","legacy manifest accepted");
    Json current={{"schema",2},{"api",3},{"id","test.extension"},{"title","Test"},{"author","Author"},{"version","1.0.0"},{"kind","lua"},{"entry","main.lua"}};
    m=Manifest::parse(current,root/"valid");check(m.schema==2 && m.api==3 && m.layout=="tabs","schema 2 defaults layout");
    for(auto [schema,api]:{std::pair{1,3},std::pair{2,1},std::pair{3,3},std::pair{0,0}}) {
        auto bad=current;bad["schema"]=schema;bad["api"]=api;rejects([&]{Manifest::parse(bad,root/"valid");},"schema/api pair rejected");
    }
    for(const auto& id:{"CON","con.settings","nul","lpt1.logs","cssx","Case.Name","bad..id","trailing."}) {
        auto invalid=current;invalid["id"]=id;rejects([&]{Manifest::parse(invalid,root/"valid");},"invalid id rejected");
    }
    check(m.json()["schema"]==2 && m.json()["api"]==3,"manifest json echoes versions");
    rejects([&]{contained_file(root/"valid","../valid/main.lua");},"traversal rejected");
    rejects([&]{contained_file(root/"valid","C:/file.lua");},"absolute path rejected");
    fs::create_directories(root/"bad");std::ofstream(root/"bad/extension.json")<<"broken";
    atomic_json(root/"valid/extension.json",current);
    auto mixed=discover(root);check(mixed.entries.size()==1 && mixed.errors.size()==1,"discovery reports broken folder");

    // Menu model schema 1 and 2.
    Json model={{"sections",Json::array({{{"id","main"},{"title","Main"},{"controls",Json::array({{{"id","speed"},{"type","number"},{"label","Speed"},{"min",1},{"max",4},{"step",.1},{"value",2}}})}}})}};
    validate_model(model);++checks;
    Json definition1={{"schema",1},{"sections",model["sections"]}};
    auto bound=bind_menu(definition1,{{"values",{{"speed",3}}},{"status","ok"}});
    check(bound["sections"][0]["controls"][0]["value"]==3 && bound["status"]=="ok","schema 1 binding");
    Json definition2={{"schema",2},{"sections",Json::array({{{"id","main"},{"title","Main"},{"description","Section help"},{"controls",Json::array({
        {{"id","wipe"},{"type","button"},{"label","Wipe"},{"severity","danger"},{"effect","irreversible"},{"hint","Cannot be undone"},{"confirm","Really?"}},
        {{"id","hp"},{"type","slider"},{"label","Health"},{"min",0},{"max",100},{"step",1},{"value",50},{"unit","%"}}})}}})}};
    bound=bind_menu(definition2,{{"values",{{"hp",75}}}});
    check(bound["sections"][0]["controls"][1]["value"]==75,"schema 2 binding");
    { auto bad=definition2;bad["sections"][0]["controls"][0]["severity"]="loud";rejects([&]{bind_menu(bad,Json::object());},"bad severity rejected"); }
    { auto bad=definition2;bad["sections"][0]["controls"][0]["effect"]="maybe";rejects([&]{bind_menu(bad,Json::object());},"bad effect rejected"); }
    { auto bad=definition2;bad["schema"]=3;rejects([&]{bind_menu(bad,Json::object());},"schema 3 rejected"); }
    rejects([&]{validate_event(bound,{{"id","wipe"}});},"confirm required");
    validate_event(bound,{{"id","wipe"},{"confirmed",true}});++checks;
    rejects([&]{validate_event(bound,{{"id","hp"},{"value",101}});},"range enforced");
    check(display_value(bound["sections"][0]["controls"][1])=="75","display value");
    check(adjusted_value(bound["sections"][0]["controls"][1],1)==76,"step adjust");

    // Storage in the standalone layout.
    Storage storage(root,LogPolicy{256,2});
    storage.log("ext.one","info","hello",{{"k",1}});
    check(fs::exists(root/"logs/ext.one/current.jsonl"),"log under logs/<id>/");
    storage.log("cssx","info","framework");
    check(fs::exists(root/"logs/cssx.jsonl"),"framework log path");
    for(int i=0;i<12;++i) storage.log("ext.one","info",std::string(60,'x'));
    check(fs::exists(root/"logs/ext.one/current.jsonl.1") && fs::exists(root/"logs/ext.one/current.jsonl.2") && !fs::exists(root/"logs/ext.one/current.jsonl.3"),"rotation keeps two backups");
    auto out=storage.output("ext.one","notes/report.txt","data");
    check(out==root/"output/ext.one/notes/report.txt" && fs::exists(out),"output under output/<id>/");
    rejects([&]{storage.output("ext.one","../escape.txt","x");},"output traversal rejected");
    rejects([&]{storage.output("ext.one","con.txt","x");},"reserved name rejected");

    // Settings.
    std::string note;
    auto settings=Settings::load(root/"settings.json",&note);
    check(settings.open_keyboard==std::vector<std::string>{"F6"} && fs::exists(root/"settings.json"),"default settings written");
    settings.open_keyboard={"F7"};settings.extra["future"]=42;settings.save(root/"settings.json");
    auto again=Settings::load(root/"settings.json");
    check(again.open_keyboard==std::vector<std::string>{"F7"} && again.extra["future"]==42,"unknown keys round-trip");
    std::ofstream(root/"settings.json",std::ios::trunc)<<"{ broken";
    again=Settings::load(root/"settings.json",&note);
    check(again.open_keyboard==std::vector<std::string>{"F6"} && note.starts_with("Recovered"),"backup recovery restores the previous valid file");
    rejects([&]{Settings::parse({{"schema",1},{"ui_scale",9}});},"ui scale bounds");
    rejects([&]{Settings::parse({{"schema",1},{"open_keyboard",Json::array({"F 1"})}});},"key name validation");
    rejects([&]{Settings::parse({{"schema",1},{"open_gamepad",Json::array({"A","A"})}});},"duplicate keys rejected");

    // Frame statistics: synthetic 22 Hz with two hitches.
    FrameRing<64> ring;
    const int64_t hz=10'000'000;
    for(int i=0;i<40;++i) ring.push(hz/22);
    ring.push(hz/5);ring.push(hz/4);
    auto values=ring.newest(100);
    check(values.size()==42,"newest bounded by writes");
    auto s=summarize(values,hz);
    check(s.count==42 && s.hitches==2 && std::abs(s.median_ms-45.4545)<0.01 && s.max_ms>=250,"summary numbers");
    check(std::abs(s.hz-1000.0/s.mean_ms)<1e-9,"hz is 1/mean");
    for(int i=0;i<200;++i) ring.push(hz/60);
    values=ring.newest(10);check(values.size()==10 && values[0]==hz/60,"ring wraps");
    auto raw=newest_of(ring.data(),ring.capacity,ring.head(),ring.total(),64);
    check(raw.size()==64 && raw.back()==hz/60,"raw view matches");
    check(summarize({},hz).count==0,"empty summary safe");

    // Background writer: replace coalesces, append rotates, drain waits.
    {
        Writer writer;
        for(int i=0;i<50;++i) writer.replace(root/"runtime/status.json",std::to_string(i));
        writer.drain();
        std::ifstream in(root/"runtime/status.json");std::string last;in>>last;
        check(last=="49" && !fs::exists(root/"runtime/status.json.tmp"),"replace writes the newest bytes atomically");
        for(int i=0;i<40;++i) writer.append(root/"logs/x.jsonl",std::string(20,'a')+"\n",256,2);
        writer.drain();
        check(fs::exists(root/"logs/x.jsonl.1") && fs::exists(root/"logs/x.jsonl.2") && fs::file_size(root/"logs/x.jsonl")<=256,"append rotates at the byte limit");
        check(writer.failures()==0 && writer.queued()==0,"writer reports no failures");
        Storage queued(root,LogPolicy{256,2});queued.set_writer(&writer);
        queued.log("cssx","info","queued line");writer.drain();
        std::ifstream log(root/"logs/cssx.jsonl");std::string all((std::istreambuf_iterator<char>(log)),std::istreambuf_iterator<char>());
        check(all.find("queued line")!=std::string::npos,"storage log goes through the writer");
    }

    fs::remove_all(root);
    std::cout<<checks<<" runtime checks passed\n";
}

#include "extension_data.hpp"
#include "extension_storage.hpp"
#include <fstream>
#include <iostream>
using namespace css;
using namespace css::extensions;
int main() {
    unsigned checks=0;
    auto check=[&](bool ok){if(!ok) throw std::runtime_error("Extension check failed at "+std::to_string(checks));++checks;};
    auto rejects=[&](auto operation){bool threw=false;try{operation();}catch(const std::exception&){threw=true;}check(threw);};
    auto root=fs::temp_directory_path()/"cssx-tests-\u6d4b\u8bd5";
    fs::remove_all(root);fs::create_directories(root/"valid");
    std::ofstream(root/"valid/main.lua")<<"return {}";
    Json manifest={{"schema",1},{"api",1},{"id","test.extension"},{"title","Test"},{"author","Author"},{"version","1.0.0"},{"kind","lua"},{"layout","tabs"},{"entry","main.lua"}};
    atomic_json(root/"valid/extension.json",manifest);
    check(discover(root).entries.size()==1);
    for(const auto& id:{"CON","con.settings","nul","lpt1.logs","cssx","Case.Name","bad..id","trailing."}) {
        auto invalid=manifest;invalid["id"]=id;rejects([&]{Manifest::parse(invalid,root/"valid");});
    }
    rejects([&]{contained_file(root/"valid","../valid/main.lua");});
    rejects([&]{contained_file(root/"valid","C:/file.lua");});
    rejects([&]{contained_file(root/"valid","missing.lua");});
    fs::create_directories(root/"bad");std::ofstream(root/"bad/extension.json")<<"broken";
    auto mixed=discover(root);check(mixed.entries.size()==1);check(mixed.errors.size()==1);
    fs::create_directories(root/"duplicate");std::ofstream(root/"duplicate/main.lua")<<"return {}";atomic_json(root/"duplicate/extension.json",manifest);
    mixed=discover(root);check(mixed.entries.size()==1);check(mixed.errors.size()==2);
    Json model={{"sections",Json::array({{{"id","main"},{"title","Main"},{"controls",Json::array({{{"id","speed"},{"type","number"},{"label","Speed"},{"min",1},{"max",4},{"step",.1},{"value",2}}})}}})}};
    validate_model(model);++checks;
    auto definition=model;definition["schema"]=1;
    auto bound=bind_menu(definition,{{"values",{{"speed",3}}},{"enabled",{{"speed",false}}}});
    check(bound["sections"][0]["controls"][0]["value"]==3);
    check(bound["sections"][0]["controls"][0]["enabled"]==false);
    rejects([&]{bind_menu(definition,{{"values",Json::array({3})}});});
    rejects([&]{bind_menu(definition,{{"values",{{"speed",99}}}});});
    auto bad=model;bad["sections"][0]["controls"][0]["step"]=0;rejects([&]{validate_model(bad);});
    bad=model;bad["sections"][0]["controls"].push_back(bad["sections"][0]["controls"][0]);rejects([&]{validate_model(bad);});
    bad=model;bad["sections"][0]["controls"][0]["value"]=5;rejects([&]{validate_model(bad);});
    for(size_t count=0;count<=128;++count) {
        LibraryPage page{count};
        for(int i=0;i<200;++i) {page.move(1,0);check(page.page<page.pages());check(count?page.selected<count:page.selected==0);}
        for(int i=0;i<200;++i) {page.move(-1,0);check(page.page<page.pages());check(count?page.selected<count:page.selected==0);}
        page.slide(500);check(page.page==page.pages()-1);page.slide(-500);check(page.page==0);
    }
    Storage storage(root,{256,2});
    for(int i=0;i<12;++i) storage.log("test.extension","info","Line with newline\ninside",{{"index",i}});
    auto logs=root/"logs/extensions/test.extension";
    check(fs::exists(logs/"current.jsonl"));check(fs::exists(logs/"current.jsonl.1"));check(fs::exists(logs/"current.jsonl.2"));check(!fs::exists(logs/"current.jsonl.3"));
    for(const auto& f:fs::directory_iterator(logs)) { std::ifstream input(f.path());std::string line;while(std::getline(input,line)){auto j=Json::parse(line);check(j["extension"]=="test.extension");check(j["message"]=="Line with newline\ninside");}}
    auto output=storage.output("test.extension","report/测试.txt","Unicode output");check(fs::exists(output));
    rejects([&]{storage.output("test.extension","../escape.txt","bad");});
    rejects([&]{storage.output("test.extension","NUL.txt","bad");});
    rejects([&]{storage.log("../escape","info","bad");});
    fs::remove_all(root);std::cout<<checks<<" extension checks passed\n";
}

#include "extension_data.hpp"
#include "extension_search.hpp"
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
    OptionSearch search;
    search.reset(Json::array({{{"id","iron"},{"label","Iron Sword"}},{{"id","gold"},{"label","Gold Sword"}},{{"id","zh"},{"label","测试"}}}));
    check(search.matches.size()==3);check(search.filter("sword IRON"));check(search.matches.size()==1);check(search.value()=="iron");
    check(!search.filter("SWORD iron"));search.filter("测试");check(search.value()=="zh");
    search.filter("not found");check(search.matches.empty());check(search.value().is_null());search.move(100);check(search.selected==0);
    search.filter("");search.move(100);check(search.value()=="zh");search.move(-100);check(search.value()=="iron");
    for(size_t count=0;count<=128;++count) {
        LibraryPage page{count};
        for(int i=0;i<200;++i) {page.move(1,0);check(page.page<page.pages());check(count?page.selected<count:page.selected==0);}
        for(int i=0;i<200;++i) {page.move(-1,0);check(page.page<page.pages());check(count?page.selected<count:page.selected==0);}
        page.slide(500);check(page.page==page.pages()-1);page.slide(-500);check(page.page==0);
    }
    fs::remove_all(root);std::cout<<checks<<" extension checks passed\n";
}

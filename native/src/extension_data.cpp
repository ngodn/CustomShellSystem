#include "extension_data.hpp"
#include <algorithm>
#include <cmath>
namespace css::extensions {
bool valid_namespace(const std::string& id) {
    if(id.empty() || id.size()>96 || id.front()<'a' || id.front()>'z' || id.back()=='.') return false;
    for(char c:id) if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return false;
    if(id.find("..")!=std::string::npos) return false;
    const auto stem=id.substr(0,id.find('.'));
    return stem!="con" && stem!="prn" && stem!="aux" && stem!="nul" &&
        !(stem.size()==4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3]>='0' && stem[3]<='9');
}
static std::string text(const Json& j,const char* key,size_t limit,bool required=true) {
    auto v=j.value(key,std::string{});
    if((required && v.empty()) || v.size()>limit || v.find('\0')!=std::string::npos) throw std::runtime_error(std::string("Invalid extension ")+key);
    // Strict JSON serialization also checks UTF-8 without Windows ANSI conversion.
    (void)Json(v).dump();
    return v;
}
fs::path contained_file(const fs::path& root,const std::string& relative) {
    if(relative.empty() || relative.size()>2048 || relative.find('\\')!=std::string::npos || relative.find(':')!=std::string::npos || relative.find('\0')!=std::string::npos)
        throw std::runtime_error("Extension file must be a relative UTF-8 path using forward slashes");
    auto p=utf8_path(relative);
    if(p.is_absolute() || p.has_root_name()) throw std::runtime_error("Absolute extension file path");
    for(const auto& part:p) if(part==".." || part==".") throw std::runtime_error("Extension file path traversal");
    const auto base=fs::canonical(root), target=fs::canonical(base/p);
    auto a=base.begin(), b=target.begin();
    for(;a!=base.end();++a,++b) if(b==target.end() || *a!=*b) throw std::runtime_error("Extension file escapes its directory");
    if(b==target.end() || !fs::is_regular_file(target)) throw std::runtime_error("Extension file is not a regular file");
    return target;
}
Manifest Manifest::parse(const Json& j,const fs::path& dir) {
    if(!j.is_object() || j.value("schema",0)!=1 || j.value("api",0)!=1) throw std::runtime_error("Unsupported CSSX manifest/API version");
    Manifest m;
    m.id=text(j,"id",96); if(!valid_namespace(m.id) || m.id=="cssx") throw std::runtime_error("Extension ID must be a unique lowercase storage namespace");
    m.title=text(j,"title",96); m.version=text(j,"version",32); m.author=text(j,"author",96);
    m.description=text(j,"description",2048,false); m.kind=text(j,"kind",16); m.layout=text(j,"layout",16);
    if(m.kind!="native" && m.kind!="lua") throw std::runtime_error("Extension kind must be native or lua");
    if(m.layout!="inventory" && m.layout!="tabs") throw std::runtime_error("Extension layout must be inventory or tabs");
    m.directory=fs::canonical(dir); m.entry=contained_file(dir,text(j,"entry",2048));
    auto suffix=m.entry.extension();
    if((m.kind=="native" && suffix!=".dll") || (m.kind=="lua" && suffix!=".lua")) throw std::runtime_error("Extension entry does not match its kind");
    if(j.contains("banner")) m.banner=contained_file(dir,text(j,"banner",2048));
    if(j.contains("menu")) m.menu=contained_file(dir,text(j,"menu",2048));
    return m;
}
Json Manifest::json() const {
    return {{"id",id},{"title",title},{"version",version},{"author",author},{"description",description},{"kind",kind},{"layout",layout},{"banner",path_utf8(banner)}};
}
Discovery discover(const fs::path& root) {
    Discovery result; std::error_code ec;
    if(!fs::exists(root,ec)) { if(ec) result.errors.push_back({{"path",path_utf8(root)},{"error",ec.message()}}); return result; }
    std::vector<fs::path> paths;
    fs::directory_iterator it(root,ec),end;
    while(!ec && it!=end) {
        const auto entry=*it;
        if(!entry.is_symlink(ec) && entry.is_directory(ec)) paths.push_back(entry.path());
        if(paths.size()>max_extensions) { result.errors.push_back({{"path",path_utf8(root)},{"error","Too many extension directories (maximum 128)"}}); return result; }
        it.increment(ec);
    }
    if(ec) result.errors.push_back({{"path",path_utf8(root)},{"error",ec.message()}});
    std::sort(paths.begin(),paths.end()); std::set<std::string> ids;
    for(const auto& dir:paths) try {
        auto file=contained_file(dir,"extension.json");
        if(fs::file_size(file)>65536) throw std::runtime_error("Extension manifest exceeds 64 KiB");
        auto m=Manifest::parse(read_json(file),dir);
        if(!ids.insert(m.id).second) throw std::runtime_error("Duplicate extension ID: "+m.id);
        result.entries.push_back(std::move(m));
    } catch(const std::exception& e) { result.errors.push_back({{"path",path_utf8(dir)},{"error",e.what()}}); }
    std::sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b){return a.title==b.title?a.id<b.id:a.title<b.title;});
    return result;
}
void validate_model(const Json& j) {
    if(!j.is_object() || !j.contains("sections") || !j["sections"].is_array() || j["sections"].size()>max_sections) throw std::runtime_error("Invalid extension sections");
    std::set<std::string> sections,controls; size_t total=0;
    for(const auto& section:j["sections"]) {
        auto id=text(section,"id",96); text(section,"title",96);
        if(!valid_id(id) || !sections.insert(id).second) throw std::runtime_error("Invalid or duplicate section ID");
        if(!section.contains("controls") || !section["controls"].is_array()) throw std::runtime_error("Missing extension controls");
        for(const auto& c:section["controls"]) {
            if(++total>max_controls) throw std::runtime_error("Too many extension controls");
            auto cid=text(c,"id",96); text(c,"label",256); text(c,"description",4096,false);
            if(!valid_id(cid) || !controls.insert(cid).second) throw std::runtime_error("Invalid or duplicate control ID");
            auto kind=text(c,"type",16);
            if(kind!="button" && kind!="toggle" && kind!="number" && kind!="choice" && kind!="text" && kind!="label" && kind!="radio" && kind!="slider" && kind!="progress" && kind!="loading") throw std::runtime_error("Unsupported extension control");
            if(c.contains("enabled") && !c["enabled"].is_boolean()) throw std::runtime_error("Invalid enabled value");
            if(c.contains("confirm")) text(c,"confirm",2048);
            if(c.contains("disabled_label")) text(c,"disabled_label",96);
            if(kind=="number" || kind=="slider") {
                double low=c.at("min"),high=c.at("max"),step=c.at("step"),value=c.at("value");
                if(!std::isfinite(low) || !std::isfinite(high) || !std::isfinite(step) || !std::isfinite(value) || low>high || step<=0 || value<low || value>high) throw std::runtime_error("Invalid numeric control range");
            }
            if(kind=="progress") {
                const double value=c.at("value");
                if(!std::isfinite(value) || value<0 || value>1) throw std::runtime_error("Progress must be between zero and one");
            }
            if(kind=="loading" && !c.at("value").is_boolean()) throw std::runtime_error("Loading state must be boolean");
            if(c.contains("busy") && !c.at("busy").is_boolean()) throw std::runtime_error("Invalid busy state");
            if(kind=="toggle" && !c.at("value").is_boolean()) throw std::runtime_error("Invalid toggle value");
            if(kind=="text") text(c,"value",256,false);
            if(kind=="choice" || kind=="radio") {
                const auto& options=c.at("options");
                if(!options.is_array() || options.empty() || options.size()>(kind=="radio"?8:512)) throw std::runtime_error("Invalid choice options");
                std::set<std::string> values;
                for(const auto& o:options) { auto v=text(o,"id",256); text(o,"label",256); if(!values.insert(v).second) throw std::runtime_error("Duplicate choice option"); }
                if(!values.contains(text(c,"value",256))) throw std::runtime_error("Choice value is not an option");
            }
        }
    }
    if(j.dump().size()>1024*1024) throw std::runtime_error("Extension menu exceeds 1 MiB");
}
Json bind_menu(const Json& definition,const Json& model) {
    if(!definition.is_object() || definition.value("schema",0)!=1) throw std::runtime_error("Unsupported CSSX UI schema");
    if(!model.is_object()) throw std::runtime_error("CSSX menu bindings must be an object");
    Json result=definition;
    const auto values=model.value("values",Json::object()), choices=model.value("options",Json::object()), enabled=model.value("enabled",Json::object()), busy=model.value("busy",Json::object());
    if(!values.is_object() || !choices.is_object() || !enabled.is_object() || !busy.is_object()) throw std::runtime_error("CSSX value, option, enabled and busy bindings must be objects");
    const auto confirmations=model.value("confirmations",Json::object());
    if(!confirmations.is_object()) throw std::runtime_error("CSSX confirmations must be an object");
    for(auto& section:result.at("sections")) for(auto& control:section.at("controls")) {
        const auto binding=control.value("binding",control.at("id").get<std::string>());
        if(values.contains(binding)) control["value"]=values.at(binding);
        if(choices.contains(binding)) control["options"]=choices.at(binding);
        if(enabled.contains(binding)) control["enabled"]=enabled.at(binding);
        if(busy.contains(binding)) control["busy"]=busy.at(binding);
        if(confirmations.contains(binding)) control["confirm"]=confirmations.at(binding);
    }
    result["status"]=model.value("status",std::string{});
    result["error"]=text(model,"error",2048,false);
    validate_model(result);return result;
}
void LibraryPage::normalize() { page=std::min(page,pages()-1); selected=count?std::min(selected,count-1):0; }
void LibraryPage::slide(int direction) {
    normalize(); if(!count) return;
    auto next=std::clamp(static_cast<int>(page)+direction,0,static_cast<int>(pages())-1);
    const size_t slot=selected%9; page=static_cast<size_t>(next); selected=std::min(page*9+slot,count-1);
}
void LibraryPage::move(int x,int y) {
    normalize(); if(!count) return;
    const int slot=static_cast<int>(selected%9),col=slot%3,row=slot/3;
    if(x && ((col==0 && x<0) || (col==2 && x>0))) { slide(x); return; }
    int next=slot+x+3*y;
    if(next<0 || next>=9) return;
    selected=std::min(page*9+static_cast<size_t>(next),count-1);
}
}

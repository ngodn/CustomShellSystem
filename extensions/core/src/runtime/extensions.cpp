#include "extensions.hpp"
#include "controls.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

namespace cssx {
namespace {
void emit(CssxSink sink,void* output,const Json& value) { auto bytes=value.dump(); if(sink) sink(output,bytes.data(),bytes.size()); }
struct Response {
    std::string bytes; bool overflow=false;
    Json parse() const { if(overflow) throw std::runtime_error("CSSX response exceeds 1 MiB"); return bytes.empty()?Json::object():Json::parse(bytes); }
};
void collect(void* output,const char* bytes,size_t size) noexcept {
    auto& value=*static_cast<Response*>(output);
    if(value.overflow) return;
    if(size>1024*1024-value.bytes.size() || (!bytes && size)) {value.overflow=true;return;}
    try {if(size) value.bytes.append(bytes,size);} catch(...) {value.overflow=true;}
}
using Module=void*;
Module open_module(const fs::path& path) {
#ifdef _WIN32
    auto m=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!m) throw std::runtime_error("Cannot load extension DLL (Windows error "+std::to_string(GetLastError())+")");
    return m;
#else
    auto m=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL); if(!m) throw std::runtime_error(dlerror()); return m;
#endif
}
void* symbol(Module module,const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module),name));
#else
    return dlsym(module,name);
#endif
}
void close_module(Module module) {
    if(!module) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(module));
#else
    dlclose(module);
#endif
}
Json from_lua(lua_State* l,int index,int depth=0) {
    if(depth>16) throw std::runtime_error("Lua result is too deeply nested");
    if(!lua_checkstack(l,4)) throw std::runtime_error("Lua result exceeds available stack");
    index=lua_absindex(l,index);
    switch(lua_type(l,index)) {
    case LUA_TNIL: return nullptr;
    case LUA_TBOOLEAN: return lua_toboolean(l,index)!=0;
    case LUA_TNUMBER: if(lua_isinteger(l,index)) return lua_tointeger(l,index); else return lua_tonumber(l,index);
    case LUA_TSTRING: { size_t size=0;auto* p=lua_tolstring(l,index,&size);if(size>1024*1024) throw std::runtime_error("Lua string exceeds bound");return std::string(p,size); }
    case LUA_TTABLE: {
        auto length=lua_rawlen(l,index); if(length>4096) throw std::runtime_error("Lua array exceeds bound");
        bool array=length!=0;
        if(lua_getmetatable(l,index)) {lua_getfield(l,-1,"cssx_array");array=array || lua_toboolean(l,-1);lua_pop(l,2);}
        Json result=array?Json::array():Json::object(); size_t count=0;
        lua_pushnil(l);
        while(lua_next(l,index)) {
            if(++count>4096) throw std::runtime_error("Lua table exceeds bound");
            if(array) {
                if(!lua_isinteger(l,-2) || lua_tointeger(l,-2)<1 || static_cast<size_t>(lua_tointeger(l,-2))>length) throw std::runtime_error("Mixed Lua arrays are unsupported");
            } else {
                if(lua_type(l,-2)!=LUA_TSTRING) throw std::runtime_error("Lua object keys must be strings");
                result[lua_tostring(l,-2)]=from_lua(l,-1,depth+1);
            }
            lua_pop(l,1);
        }
        if(array) for(size_t i=1;i<=length;++i) { lua_rawgeti(l,index,static_cast<lua_Integer>(i)); result.push_back(from_lua(l,-1,depth+1)); lua_pop(l,1); }
        return result;
    }
    default: throw std::runtime_error("Unsupported value in Lua result");
    }
}
void to_lua(lua_State* l,const Json& j) {
    if(!lua_checkstack(l,4)) luaL_error(l,"CSSX Lua argument exceeds available stack");
    if(j.is_null()) lua_pushnil(l);
    else if(j.is_boolean()) lua_pushboolean(l,j.get<bool>());
    else if(j.is_number_integer()) lua_pushinteger(l,j.get<lua_Integer>());
    else if(j.is_number()) lua_pushnumber(l,j.get<double>());
    else if(j.is_string()) { const auto& s=j.get_ref<const std::string&>();lua_pushlstring(l,s.data(),s.size()); }
    else {
        lua_newtable(l);
        if(j.is_array()) { size_t i=0;for(const auto& v:j) { to_lua(l,v);lua_rawseti(l,-2,++i); } }
        else for(auto it=j.begin();it!=j.end();++it) { to_lua(l,it.value());lua_setfield(l,-2,it.key().c_str()); }
    }
}
struct Stopwatch {
    uint64_t& calls; uint64_t& us; uint64_t start;
    Stopwatch(uint64_t& c,uint64_t& u):calls(c),us(u),start(monotonic_us()) {}
    ~Stopwatch() { ++calls; us+=monotonic_us()-start; }
};
}
Json ExtensionCost::json() const {
    return {{"tick_calls",tick_calls},{"tick_us",tick_us},{"render_calls",render_calls},{"render_us",render_us},
            {"model_calls",model_calls},{"model_us",model_us},{"event_calls",event_calls},{"event_us",event_us},{"requests",requests}};
}
struct Runtime::Entry {
    Runtime* runtime=nullptr;
    Manifest manifest;
    CssxHost host{};
    Module module=nullptr;
    const CssxExtension* api=nullptr;
    void* instance=nullptr;
    lua_State* lua=nullptr;
    size_t memory=0; int instructions=0;
    int table=LUA_NOREF;
    std::string error;
    Json model=Json::object();
    Json status=Json::object();
    double status_age=1e9;
    bool suspended=false, stopped=false, dirty=true;
    bool used_hooks=false;
    unsigned requests=0;
    double elapsed=0, menu_check=0;
    fs::file_time_type menu_time{};
    Json definition;
    ExtensionCost cost;
    bool has_member(size_t member_end) const { return api && api->size>=member_end; }
    Json service(const Json&);
    void start();
    Json invoke_lua(const char*,const Json& argument=nullptr,bool optional=false);
    bool stop();
    ~Entry() { if(lua) lua_close(lua); if(api && instance && stopped) api->destroy(instance); if(stopped || !instance) close_module(module); }
    void refresh();
    void refresh_status();
    void event(const Json&);
    void tick(double);
    void render(const CssxFrame*);
    bool needs_frame() const {
        return !suspended && !stopped && api && instance && api->abi>=2 && has_member(CSSX_ABI2_EXTENSION_SIZE) && api->render;
    }
    void fail(const std::string& text);
};
namespace {
int entry_request(void* context,const char* data,CssxSink sink,void* output) {
    try { if(!data || std::strlen(data)>1024*1024) throw std::runtime_error("Extension request exceeds bound");emit(sink,output,static_cast<Runtime::Entry*>(context)->service(Json::parse(data)));return 1; }
    catch(const std::exception& e) { emit(sink,output,{{"error",e.what()}});return 0; }
}
void entry_log(void* context,int level,const char* utf8,size_t length) {
    auto* entry=static_cast<Runtime::Entry*>(context);
    try {
        const char* name=level<=CSSX_LOG_DEBUG?"debug":level==CSSX_LOG_INFO?"info":level==CSSX_LOG_WARNING?"warning":"error";
        entry->runtime->log(entry->manifest.id,name,std::string(utf8?utf8:"",utf8?std::min<size_t>(length,16384):0));
    } catch(...) {}
}
void entry_invalidate(void* context) { auto* entry=static_cast<Runtime::Entry*>(context); entry->dirty=true; entry->runtime->invalidate(entry->manifest.id); }
uint64_t entry_now(void*) { return monotonic_us(); }
void* lua_memory(void* context,void* ptr,size_t old,size_t size) {
    auto& entry=*static_cast<Runtime::Entry*>(context);
    if(!ptr) old=0;
    if(!size) { std::free(ptr);entry.memory-=old;return nullptr; }
    if(size>64*1024*1024 || entry.memory-old+size>64*1024*1024) return nullptr;
    auto* value=std::realloc(ptr,size);if(value) entry.memory=entry.memory-old+size;return value;
}
void lua_budget(lua_State* l,lua_Debug*) {
    auto* entry=*static_cast<Runtime::Entry**>(lua_getextraspace(l));
    if(++entry->instructions>200) {
        lua_sethook(l,lua_budget,LUA_MASKCOUNT,1);
        luaL_error(l,"CSSX Lua instruction budget exceeded");
    }
}
int lua_service(lua_State* l) {
    auto* entry=static_cast<Runtime::Entry*>(lua_touserdata(l,lua_upvalueindex(1)));
    try { auto request=from_lua(l,1);auto result=entry->service(request);to_lua(l,result);return 1; }
    catch(const std::exception& e) { lua_pushnil(l);lua_pushstring(l,e.what());return 2; }
}
int lua_array(lua_State* l) {
    if(lua_isnoneornil(l,1)) lua_newtable(l);
    else {luaL_checktype(l,1,LUA_TTABLE);lua_pushvalue(l,1);}
    lua_newtable(l);lua_pushboolean(l,1);lua_setfield(l,-2,"cssx_array");lua_setmetatable(l,-2);return 1;
}
struct LuaInvocation { Runtime::Entry* entry;const char* name;const Json* argument;bool optional; };
int lua_invoke(lua_State* l) {
    const auto* call=static_cast<LuaInvocation*>(lua_touserdata(l,1));
    lua_rawgeti(l,LUA_REGISTRYINDEX,call->entry->table);lua_getfield(l,-1,call->name);lua_remove(l,-2);
    if(lua_isnil(l,-1) && call->optional) return 1;
    if(!lua_isfunction(l,-1)) return luaL_error(l,"Lua extension requires function %s",call->name);
    to_lua(l,*call->argument);lua_call(l,1,1);return 1;
}
}
Json Runtime::Entry::service(const Json& j) {
    if(++requests>4096) throw std::runtime_error("Extension host request budget exceeded");
    ++cost.requests;
    const auto op=j.at("op").get<std::string>();
    const auto state=runtime->root()/"state"/utf8_path(manifest.id+".json");
    if(op=="state.load") {
        if(!fs::exists(state)) { atomic_json(state,Json::object());return Json::object(); }
        try { if(fs::file_size(state)>1024*1024) throw std::runtime_error("Extension state exceeds bound");return read_json(state); }
        catch(...) { auto backup=state;backup+=".bak";if(!fs::exists(backup)) throw;auto value=read_json(backup);atomic_json(state,value,false);return value; }
    }
    if(op=="state.save") { const auto& value=j.at("value");if(!value.is_object() || value.dump().size()>1024*1024) throw std::runtime_error("Invalid extension state");atomic_json(state,value);return {{"saved",true}}; }
    if(op=="log") { runtime->log(manifest.id,j.value("level",std::string("info")),j.at("message").get<std::string>(),j.value("fields",Json::object()));return nullptr; }
    if(op=="output.write") return path_utf8(runtime->storage().output(manifest.id,j.at("file").get<std::string>(),j.at("text").get<std::string>()));
    if(op=="asset") return path_utf8(contained_file(manifest.directory,j.at("file").get<std::string>()));
    if(op=="invalidate") { dirty=true;runtime->invalidate(manifest.id);return nullptr; }
    if(op=="extension.info") return {{"id",manifest.id},{"version",manifest.version},{"directory",path_utf8(manifest.directory)},{"abi",host.abi}};
    if(op.starts_with("hud.minimap")) throw std::runtime_error("hud.minimap operations belonged to CSS's engine; standalone CSSX does not provide them. Build the widget through hud.widget and drive it with request ops.");
    if(!runtime->services_.request) throw std::runtime_error("Engine services are unavailable in this host");
    auto request=j;request["extension"]=manifest.id;
    auto result=runtime->services_.request(request);
    if(op=="hooks.add") used_hooks=true;
    return result;
}
void Runtime::Entry::fail(const std::string& text) {
    error=text;suspended=true;++runtime->revision_;
    if(!stop()) error+="; cleanup incomplete, unload blocked";
    try { runtime->log(manifest.id,"error",text); runtime->log("cssx","error","Extension suspended",{{"id",manifest.id},{"error",text}}); } catch(...) {}
}
Json Runtime::Entry::invoke_lua(const char* name,const Json& argument,bool optional) {
    const int top=lua_gettop(lua); instructions=0;requests=0;
    if(!lua_checkstack(lua,8)) throw std::runtime_error("Lua callback has no stack space");
    LuaInvocation invocation{this,name,&argument,optional};
    lua_pushcfunction(lua,lua_invoke);lua_pushlightuserdata(lua,&invocation);
    lua_sethook(lua,lua_budget,LUA_MASKCOUNT,10000);
    int result=lua_pcall(lua,1,1,0);lua_sethook(lua,nullptr,0,0);
    if(result!=LUA_OK) { std::string text=lua_tostring(lua,-1)?lua_tostring(lua,-1):"Lua error";lua_settop(lua,top);throw std::runtime_error(text); }
    try { auto value=from_lua(lua,-1);lua_settop(lua,top);return value; }
    catch(...) { lua_settop(lua,top);throw; }
}
void Runtime::Entry::start() {
    host=CssxHost{};
    host.size=sizeof(CssxHost);host.context=this;host.request=entry_request;
    host.log=entry_log;host.invalidate=entry_invalidate;host.now_us=entry_now;
    if(manifest.kind=="native") {
        module=open_module(manifest.entry);
        auto get=reinterpret_cast<CssxGetExtension>(symbol(module,"cssx_get_extension"));
        if(!get) throw std::runtime_error("Missing cssx_get_extension export");
        const auto* candidate=get();
        if(!candidate) throw std::runtime_error("cssx_get_extension returned null");
        const uint32_t abi=candidate->abi;
        if(abi<1 || abi>CSSX_ABI) throw std::runtime_error("Unsupported extension ABI "+std::to_string(abi)+"; this CSSX supports ABI 1 to "+std::to_string(CSSX_ABI));
        const uint32_t min_size=abi==1?CSSX_ABI1_EXTENSION_SIZE:abi==2?CSSX_ABI2_EXTENSION_SIZE:uint32_t(sizeof(CssxExtension));
        if(candidate->size<min_size) throw std::runtime_error("Extension table is smaller than its declared ABI requires");
        if(!candidate->create || !candidate->model || !candidate->event || !candidate->stop || !candidate->destroy)
            throw std::runtime_error("Extension table is missing a required callback");
        // The manifest's api must agree with the binary: api 1 <-> ABI 1, api 2 <-> ABI 2, api 3 <-> ABI 3.
        if(manifest.api!=int(abi)) throw std::runtime_error("Manifest api "+std::to_string(manifest.api)+" does not match the DLL's ABI "+std::to_string(abi));
        api=candidate;
        // Present a host stamped with the ABI the extension declared. ABI-1
        // extensions see no HUD table; ABI-3 members are present but only an
        // ABI-3 extension knows to read them.
        host.abi=abi;
        host.hud=abi>=2?runtime->services_.hud:nullptr;
        instance=api->create(&host);if(!instance) throw std::runtime_error("Extension initialization failed");
    } else {
        host.abi=CSSX_ABI;
        if(fs::file_size(manifest.entry)>1024*1024) throw std::runtime_error("Lua entry exceeds 1 MiB");
        std::ifstream file(manifest.entry,std::ios::binary); std::string source((std::istreambuf_iterator<char>(file)),{});
        if(!file.good() && !file.eof()) throw std::runtime_error("Cannot read Lua entry");
        lua=lua_newstate(lua_memory,this);if(!lua) throw std::runtime_error("Cannot allocate Lua state");
        *static_cast<Entry**>(lua_getextraspace(lua))=this;
        for(auto lib: {std::pair{"_G",luaopen_base},std::pair{"table",luaopen_table},std::pair{"string",luaopen_string},std::pair{"math",luaopen_math},std::pair{"utf8",luaopen_utf8}}) { luaL_requiref(lua,lib.first,lib.second,1);lua_pop(lua,1); }
        for(auto name:{"dofile","loadfile","load","collectgarbage"}) {lua_pushnil(lua);lua_setglobal(lua,name);}
        lua_newtable(lua);lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,lua_service,1);lua_setfield(lua,-2,"request");lua_pushcfunction(lua,lua_array);lua_setfield(lua,-2,"array");lua_setglobal(lua,"cssx");
        const auto chunk="@"+manifest.id+"/entry.lua";
        int result=luaL_loadbufferx(lua,source.data(),source.size(),chunk.c_str(),"t");
        if(result==LUA_OK) { lua_sethook(lua,lua_budget,LUA_MASKCOUNT,10000);result=lua_pcall(lua,0,1,0);lua_sethook(lua,nullptr,0,0); }
        if(result!=LUA_OK) throw std::runtime_error(lua_tostring(lua,-1)?lua_tostring(lua,-1):"Cannot load Lua extension");
        if(!lua_istable(lua,-1)) throw std::runtime_error("Lua entry must return an extension table");
        table=luaL_ref(lua,LUA_REGISTRYINDEX);invoke_lua("start",nullptr,true);
    }
    if(!manifest.menu.empty()) {if(fs::file_size(manifest.menu)>1024*1024) throw std::runtime_error("Menu exceeds 1 MiB");definition=read_json(manifest.menu);menu_time=fs::last_write_time(manifest.menu);}
    refresh();
    runtime->log(manifest.id,"info","Extension initialized",{{"version",manifest.version},{"kind",manifest.kind},{"abi",host.abi}});
}
void Runtime::Entry::refresh() {
    requests=0;
    Stopwatch watch(cost.model_calls,cost.model_us);
    Json next;
    if(lua) next=invoke_lua("model");
    else { Response response;if(!api->model(instance,collect,&response)) throw std::runtime_error("Extension model callback failed");next=response.parse(); }
    if(!definition.is_null()) next=bind_menu(definition,next);
    validate_model(next);model=std::move(next);dirty=false;
}
void Runtime::Entry::refresh_status() {
    status=Json::object();
    if(suspended || stopped || !api || !instance || api->abi<3 || !has_member(sizeof(CssxExtension)) || !api->status) return;
    Response response;
    if(!api->status(instance,collect,&response)) return;
    try {
        auto value=response.parse();
        if(!value.is_object()) return;
        Json clean=Json::object();
        if(value.contains("summary") && value["summary"].is_string() && value["summary"].get_ref<const std::string&>().size()<=96) clean["summary"]=value["summary"];
        if(value.contains("active") && value["active"].is_boolean()) clean["active"]=value["active"];
        status=std::move(clean);
    } catch(...) {}
}
void Runtime::Entry::event(const Json& e) {
    if(dirty) refresh();
    validate_event(model,e);
    requests=0;
    Stopwatch watch(cost.event_calls,cost.event_us);
    if(lua) invoke_lua("event",e);
    else {
        auto data=e.dump();
        if(!api->event(instance,data.c_str())) {
            dirty=true;++runtime->revision_;refresh();
            throw std::runtime_error(model.value("error",std::string{}).empty()?"Extension event callback failed":model["error"].get<std::string>());
        }
    }
    dirty=true;++runtime->revision_;
}
void Runtime::Entry::tick(double delta) {
    elapsed+=std::clamp(delta,0.,.25);
    if(elapsed<.1) return;
    const auto step=elapsed;elapsed=0;requests=0;status_age+=step;
    menu_check+=step;
    if(!manifest.menu.empty() && menu_check>=1.) {
        menu_check=0;std::error_code ec;const auto time=fs::last_write_time(manifest.menu,ec);
        if(!ec && time!=menu_time) {
            try {if(fs::file_size(manifest.menu)>1024*1024) throw std::runtime_error("Menu exceeds 1 MiB");auto next=read_json(manifest.menu);auto before=definition;definition=next;try{refresh();}catch(...){definition=before;throw;}menu_time=time;++runtime->revision_;}
            catch(const std::exception& e) {runtime->log(manifest.id,"warning",std::string("UI reload rejected: ")+e.what());menu_time=time;}
        }
    }
    Stopwatch watch(cost.tick_calls,cost.tick_us);
    if(lua) invoke_lua("tick",step,true);
    else if(api && instance && api->tick && !api->tick(instance,step)) throw std::runtime_error("Extension tick callback failed");
}
void Runtime::Entry::render(const CssxFrame* frame) {
    if(!needs_frame()) return;
    Stopwatch watch(cost.render_calls,cost.render_us);
    CssxFrame stamped=*frame;stamped.abi=api->abi;
    if(!api->render(instance,&stamped)) throw std::runtime_error("Extension render callback failed");
}
bool Runtime::Entry::stop() {
    if(stopped) return true;requests=0;
    try {
        if(used_hooks && runtime->services_.request) {runtime->services_.request({{"op","hooks.clear"},{"extension",manifest.id}});used_hooks=false;}
        if(lua && table!=LUA_NOREF) { auto result=invoke_lua("stop",nullptr,true);if(result.is_boolean() && !result.get<bool>()) return false; }
        if(api && instance && !api->stop(instance)) return false;
        stopped=true;return true;
    } catch(const std::exception& e) {
        try {runtime->log(manifest.id,"error",std::string("Extension cleanup failed: ")+e.what());} catch(...) {}
        return false;
    }
}
Runtime::Runtime(fs::path root,HostServices services):root_(std::move(root)),services_(std::move(services)),storage_(root_) {
    fs::create_directories(root_/"extensions");
    log("cssx","info","Starting CSSX extension discovery");
    auto found=discover(root_/"extensions"); errors_=found.errors;
    for(const auto& error:errors_) log("cssx","warning","Extension discovery rejected a directory",error);
    for(auto& manifest:found.entries) {
        auto entry=std::make_unique<Entry>(); entry->runtime=this; entry->manifest=std::move(manifest);
        try { entry->start(); } catch(const std::exception& e) { entry->fail(e.what()); }
        entries_.push_back(std::move(entry));
    }
}
Runtime::~Runtime()=default;
void Runtime::log(const std::string& id,const std::string& level,const std::string& message,const Json& fields) {
    if(id=="cssx" && services_.log) { services_.log(level,message,fields); return; }
    storage_.log(id,level,message,fields);
}
void Runtime::invalidate(const std::string&) { ++revision_; }
Runtime::Entry& Runtime::find(const std::string& id) { for(auto& e:entries_) if(e->manifest.id==id) return *e;throw std::runtime_error("Extension is not installed"); }
Json Runtime::library() {
    auto result=Json{{"revision",revision_},{"extensions",Json::array()},{"errors",errors_}};
    for(auto& entry:entries_) {
        auto value=entry->manifest.json();
        value["error"]=entry->error;value["available"]=!entry->suspended;
        if(entry->status_age>=1.0) { entry->refresh_status(); entry->status_age=0; }
        value["status"]=entry->status;value["cost"]=entry->cost.json();
        result["extensions"].push_back(std::move(value));
    }
    return result;
}
Json Runtime::model(const std::string& id) { auto& e=find(id); if(e.suspended) throw std::runtime_error(e.error); if(e.dirty) e.refresh(); return e.model; }
void Runtime::event(const std::string& id,const Json& event) { auto& e=find(id); if(e.suspended) throw std::runtime_error(e.error); e.event(event); }
Json Runtime::request(const Json& j) {
    const auto op=j.at("op").get<std::string>();
    if(op=="library") return library();
    const auto id=j.at("id").get<std::string>();
    if(op=="model") return model(id);
    if(op=="event") { event(id,j.at("event"));return {{"revision",revision_}}; }
    throw std::runtime_error("Unknown CSSX runtime request");
}
void Runtime::tick(double seconds) { for(auto& e:entries_) if(!e->suspended) try { e->tick(seconds); } catch(const std::exception& error) { e->fail(error.what()); } }
void Runtime::render(const CssxFrame& frame) { for(auto& e:entries_) if(!e->suspended) try { e->render(&frame); } catch(const std::exception& error) { e->fail(error.what()); } }
bool Runtime::needs_frame() const { return !stopped_ && std::any_of(entries_.begin(),entries_.end(),[](const auto& e){return e->needs_frame();}); }
bool Runtime::stop() { bool ok=true;for(auto& e:entries_) if(!e->stop()) ok=false;stopped_=ok;return ok; }
}

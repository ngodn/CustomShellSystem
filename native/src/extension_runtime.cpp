#include "extension_controls.hpp"
#include "cssx/api.h"
#include "extension_data.hpp"
#include "extension_storage.hpp"
#include <fstream>
#include <memory>
#include <chrono>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
namespace css::extensions {
namespace {
void emit(CssxSink sink,void* output,const Json& value) { auto bytes=value.dump(); if(sink) sink(output,bytes.data(),bytes.size()); }
void collect(void* output,const char* bytes,size_t size) {
    auto& value=*static_cast<std::string*>(output);
    if(size>1024*1024 || value.size()+size>1024*1024) return;
    value.append(bytes,size);
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
struct Runtime;
struct Entry {
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
    bool suspended=false, stopped=false, dirty=true;
    unsigned requests=0;
    double elapsed=0;
    double menu_check=0;
    fs::file_time_type menu_time{};
    Json definition;
    Json service(const Json&);
    void start();
    Json invoke_lua(const char*,const Json& argument=nullptr,bool optional=false);
    bool stop();
    ~Entry() { if(lua) lua_close(lua); if(api && instance && stopped) api->destroy(instance); if(stopped || !instance) close_module(module); }
    void refresh();
    void event(const Json&);
    void tick(double);
    void fail(const std::string& text);
};
struct Runtime {
    CssxHost host;
    fs::path root;
    Storage storage;
    std::vector<std::unique_ptr<Entry>> entries;
    Json errors=Json::array();
    bool stopped=false;
    uint64_t revision=1;
    Runtime(const CssxHost& h,const fs::path& r):host(h),root(r),storage(r) {
        fs::create_directories(root/"extensions");
        storage.log("cssx","info","Starting CSSX extension discovery");
        auto found=discover(root/"extensions"); errors=found.errors;
        for(const auto& error:errors) storage.log("cssx","warning","Extension discovery rejected a directory",error);
        for(auto& manifest:found.entries) {
            auto entry=std::make_unique<Entry>(); entry->runtime=this; entry->manifest=std::move(manifest);
            try { entry->start(); } catch(const std::exception& e) { entry->fail(e.what()); }
            entries.push_back(std::move(entry));
        }
    }
    Json service(const Json& j) {
        auto bytes=j.dump(); std::string response;
        const int ok=host.request(host.context,bytes.c_str(),collect,&response);
        auto value=response.empty()?Json::object():Json::parse(response);
        if(!ok) throw std::runtime_error(value.value("error",std::string("CSSX host request failed")));
        return value;
    }
    Json library() const {
        auto result=Json{{"revision",revision},{"extensions",Json::array()},{"errors",errors}};
        for(const auto& entry:entries) { auto value=entry->manifest.json(); value["error"]=entry->error;value["available"]=!entry->suspended;result["extensions"].push_back(std::move(value)); }
        return result;
    }
    Entry& find(const std::string& id) { for(auto& e:entries) if(e->manifest.id==id) return *e;throw std::runtime_error("Extension is not installed"); }
    Json request(const Json& j) {
        auto op=j.at("op").get<std::string>();
        if(op=="library") return library();
        auto& e=find(j.at("id").get<std::string>());
        if(e.suspended) throw std::runtime_error(e.error);
        if(op=="model") { if(e.dirty) e.refresh(); return e.model; }
        if(op=="event") { e.event(j.at("event"));return {{"revision",revision}}; }
        throw std::runtime_error("Unknown CSSX request");
    }
    void tick(double seconds) { for(auto& e:entries) if(!e->suspended) try { e->tick(seconds); } catch(const std::exception& error) { e->fail(error.what()); } }
    bool stop() { bool ok=true;for(auto& e:entries) if(!e->stop()) ok=false;stopped=ok;return ok; }
};
int entry_service(void* context,const char* data,CssxSink sink,void* output) {
    try { if(!data || std::strlen(data)>1024*1024) throw std::runtime_error("Extension request exceeds bound");emit(sink,output,static_cast<Entry*>(context)->service(Json::parse(data)));return 1; }
    catch(const std::exception& e) { emit(sink,output,{{"error",e.what()}});return 0; }
}
Json Entry::service(const Json& j) {
    if(++requests>4096) throw std::runtime_error("Extension host request budget exceeded");
    auto op=j.at("op").get<std::string>();
    auto state=runtime->root/"state/extensions"/utf8_path(manifest.id+".json");
    if(op=="state.load") {
        if(!fs::exists(state)) { atomic_json(state,Json::object());return Json::object(); }
        try { if(fs::file_size(state)>1024*1024) throw std::runtime_error("Extension state exceeds bound");return read_json(state); }
        catch(...) { auto backup=state;backup+=".bak";if(!fs::exists(backup)) throw;auto value=read_json(backup);atomic_json(state,value,false);return value; }
    }
    if(op=="state.save") { const auto& value=j.at("value");if(!value.is_object() || value.dump().size()>1024*1024) throw std::runtime_error("Invalid extension state");atomic_json(state,value);return {{"saved",true}}; }
    if(op=="log") { runtime->storage.log(manifest.id,j.value("level",std::string("info")),j.at("message").get<std::string>(),j.value("fields",Json::object()));return nullptr; }
    if(op=="output.write") return path_utf8(runtime->storage.output(manifest.id,j.at("file").get<std::string>(),j.at("text").get<std::string>()));
    if(op=="asset") return path_utf8(contained_file(manifest.directory,j.at("file").get<std::string>()));
    if(op=="invalidate") { dirty=true;++runtime->revision;return nullptr; }
    auto request=j;request["extension"]=manifest.id;
    return runtime->service(request);
}
void Entry::fail(const std::string& text) {
    error=text;suspended=true;++runtime->revision;
    try { runtime->storage.log(manifest.id,"error",text); runtime->storage.log("cssx","error","Extension suspended",{{"id",manifest.id},{"error",text}}); } catch(...) {}
}
void* lua_memory(void* context,void* ptr,size_t old,size_t size) {
    auto& entry=*static_cast<Entry*>(context);
    if(!ptr) old=0;
    if(!size) { std::free(ptr);entry.memory-=old;return nullptr; }
    if(size>64*1024*1024 || entry.memory-old+size>64*1024*1024) return nullptr;
    auto* value=std::realloc(ptr,size);if(value) entry.memory=entry.memory-old+size;return value;
}
void lua_budget(lua_State* l,lua_Debug*) {
    auto* entry=*static_cast<Entry**>(lua_getextraspace(l));
    if(++entry->instructions>200) {
        // A script can catch an error with pcall. After exhaustion, check every
        // instruction so its catch handler cannot restart an unbounded loop.
        lua_sethook(l,lua_budget,LUA_MASKCOUNT,1);
        luaL_error(l,"CSSX Lua instruction budget exceeded");
    }
}
int lua_service(lua_State* l) {
    auto* entry=static_cast<Entry*>(lua_touserdata(l,lua_upvalueindex(1)));
    // Host errors return nil, message. Lua is compiled as C++ so allocation
    // failures also unwind these temporary JSON values before protected recovery.
    try { auto request=from_lua(l,1);auto result=entry->service(request);to_lua(l,result);return 1; }
    catch(const std::exception& e) { lua_pushnil(l);lua_pushstring(l,e.what());return 2; }
}
int lua_array(lua_State* l) {
    if(lua_isnoneornil(l,1)) lua_newtable(l);
    else {luaL_checktype(l,1,LUA_TTABLE);lua_pushvalue(l,1);}
    lua_newtable(l);lua_pushboolean(l,1);lua_setfield(l,-2,"cssx_array");lua_setmetatable(l,-2);return 1;
}
struct LuaInvocation { Entry* entry;const char* name;const Json* argument;bool optional; };
int lua_invoke(lua_State* l) {
    const auto* call=static_cast<LuaInvocation*>(lua_touserdata(l,1));
    lua_rawgeti(l,LUA_REGISTRYINDEX,call->entry->table);lua_getfield(l,-1,call->name);lua_remove(l,-2);
    if(lua_isnil(l,-1) && call->optional) return 1;
    if(!lua_isfunction(l,-1)) return luaL_error(l,"Lua extension requires function %s",call->name);
    to_lua(l,*call->argument);lua_call(l,1,1);return 1;
}
Json Entry::invoke_lua(const char* name,const Json& argument,bool optional) {
    const int top=lua_gettop(lua); instructions=0;requests=0;
    if(!lua_checkstack(lua,8)) throw std::runtime_error("Lua callback has no stack space");
    LuaInvocation invocation{this,name,&argument,optional};
    lua_pushcfunction(lua,lua_invoke);lua_pushlightuserdata(lua,&invocation);
    lua_sethook(lua,lua_budget,LUA_MASKCOUNT,10000);
    int result=lua_pcall(lua,1,1,0);lua_sethook(lua,nullptr,0,0);
    if(result!=LUA_OK) { std::string error=lua_tostring(lua,-1)?lua_tostring(lua,-1):"Lua error";lua_settop(lua,top);throw std::runtime_error(error); }
    try { auto value=from_lua(lua,-1);lua_settop(lua,top);return value; }
    catch(...) { lua_settop(lua,top);throw; }
}
void Entry::start() {
    host={CSSX_ABI,sizeof(CssxHost),this,entry_service};
    if(manifest.kind=="native") {
        module=open_module(manifest.entry);
        auto get=reinterpret_cast<CssxGetExtension>(symbol(module,"cssx_get_extension"));
        if(!get) throw std::runtime_error("Missing cssx_get_extension export");
        api=get();
        if(!api || api->abi!=CSSX_ABI || api->size<sizeof(CssxExtension) || !api->create || !api->model || !api->event || !api->stop || !api->destroy) throw std::runtime_error("Incompatible CSSX extension ABI");
        instance=api->create(&host);if(!instance) throw std::runtime_error("Extension initialization failed");
    } else {
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
    runtime->storage.log(manifest.id,"info","Extension initialized",{{"version",manifest.version},{"kind",manifest.kind}});
}
void Entry::refresh() {
    requests=0;
    Json next;
    if(lua) next=invoke_lua("model");
    else { std::string bytes;if(!api->model(instance,collect,&bytes)) throw std::runtime_error("Extension model callback failed");next=Json::parse(bytes); }
    if(!definition.is_null()) next=bind_menu(definition,next);
    validate_model(next);model=std::move(next);dirty=false;
}
void Entry::event(const Json& event) {
    if(dirty) refresh();
    css::extensions::validate_event(model,event);
    requests=0;
    if(lua) invoke_lua("event",event);
    else { auto data=event.dump();if(!api->event(instance,data.c_str())) throw std::runtime_error("Extension event callback failed"); }
    dirty=true;++runtime->revision;
}
void Entry::tick(double delta) {
    elapsed+=std::clamp(delta,0.,.25);
    if(elapsed<.1) return;
    const auto step=elapsed;elapsed=0;requests=0;
    menu_check+=step;
    if(!manifest.menu.empty() && menu_check>=1.) {
        menu_check=0;std::error_code ec;const auto time=fs::last_write_time(manifest.menu,ec);
        // Editors can briefly remove a file during atomic replacement.
        if(!ec && time!=menu_time) {
            try {if(fs::file_size(manifest.menu)>1024*1024) throw std::runtime_error("Menu exceeds 1 MiB");auto next=read_json(manifest.menu);auto before=definition;definition=next;try{refresh();}catch(...){definition=before;throw;}menu_time=time;++runtime->revision;}
            catch(const std::exception& e) {runtime->storage.log(manifest.id,"warning",std::string("UI reload rejected: ")+e.what());menu_time=time;}
        }
    }
    if(lua) invoke_lua("tick",step,true);
    else if(api && instance && api->tick && !api->tick(instance,step)) throw std::runtime_error("Extension tick callback failed");
}
bool Entry::stop() {
    if(stopped) return true;requests=0;
    try {
        if(lua && table!=LUA_NOREF) { auto result=invoke_lua("stop",nullptr,true);if(result.is_boolean() && !result.get<bool>()) return false; }
        if(api && instance && !api->stop(instance)) return false;
        stopped=true;return true;
    } catch(const std::exception& e) { fail(e.what());return false; }
}
void* create(const CssxHost* h,const wchar_t* root) {
    try { if(!h || h->abi!=CSSX_ABI || h->size<sizeof(CssxHost) || !h->request || !root) return nullptr;return new Runtime(*h,fs::path(root)); } catch(...) {return nullptr;}
}
int tick(void* p,double seconds) { try {static_cast<Runtime*>(p)->tick(seconds);return 1;} catch(...) {return 0;} }
int request(void* p,const char* j,CssxSink sink,void* output) {
    try {if(!j || std::strlen(j)>1024*1024) throw std::runtime_error("CSSX request exceeds bound");emit(sink,output,static_cast<Runtime*>(p)->request(Json::parse(j)));return 1;}
    catch(const std::exception& e) {emit(sink,output,{{"error",e.what()}});return 0;}
}
int stop(void* p) {try{return static_cast<Runtime*>(p)->stop()?1:0;}catch(...){return 0;}}
void destroy(void* p) {auto* runtime=static_cast<Runtime*>(p);if(runtime->stopped) delete runtime;}
}
}
extern "C" CSSX_EXPORT const CssxRuntime* cssx_get_runtime() {
    static const CssxRuntime api{CSSX_ABI,sizeof(CssxRuntime),css::extensions::create,css::extensions::tick,css::extensions::request,css::extensions::stop,css::extensions::destroy};return &api;
}

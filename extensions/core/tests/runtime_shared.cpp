// Shared-library shim so the Python lifecycle test can drive the portable
// Runtime through ctypes. The C table mirrors the runtime surface the core
// uses; it is a test harness, not a public ABI.
#include "extensions.hpp"
#include <cstring>

using namespace cssx;
namespace {
struct Harness {
    std::unique_ptr<Runtime> runtime;
    int (*host_request)(void*,const char*,CssxSink,void*)=nullptr;
    void* host_context=nullptr;
    CssxHudApi hud{};
};
void emit(CssxSink sink,void* output,const Json& value) { auto bytes=value.dump(); if(sink) sink(output,bytes.data(),bytes.size()); }
struct Response { std::string bytes; bool overflow=false; };
void collect(void* output,const char* bytes,size_t size) noexcept {
    auto& value=*static_cast<Response*>(output);
    if(value.overflow) return;
    if(size>1024*1024-value.bytes.size() || (!bytes && size)) {value.overflow=true;return;}
    try {if(size) value.bytes.append(bytes,size);} catch(...) {value.overflow=true;}
}
extern "C" {
void* cssx_test_create(int (*request)(void*,const char*,CssxSink,void*),void* context,const wchar_t* root) {
    try {
        auto* h=new Harness();h->host_request=request;h->host_context=context;
        h->hud.abi=2;h->hud.size=sizeof(CssxHudApi);
        HostServices services;
        services.hud=&h->hud;
        services.request=[h](const Json& j)->Json {
            auto bytes=j.dump();Response response;
            const int ok=h->host_request(h->host_context,bytes.c_str(),collect,&response);
            if(response.overflow) throw std::runtime_error("CSSX response exceeds 1 MiB");
            auto value=response.bytes.empty()?Json::object():Json::parse(response.bytes);
            if(!ok) throw std::runtime_error(value.value("error",std::string("host request failed")));
            return value;
        };
        h->runtime=std::make_unique<Runtime>(fs::path(root),std::move(services));
        return h;
    } catch(...) { return nullptr; }
}
int cssx_test_tick(void* p,double seconds) { try { static_cast<Harness*>(p)->runtime->tick(seconds); return 1; } catch(...) { return 0; } }
int cssx_test_render(void* p,const CssxFrame* frame) { try { static_cast<Harness*>(p)->runtime->render(*frame); return 1; } catch(...) { return 0; } }
int cssx_test_needs_frame(void* p) { return static_cast<Harness*>(p)->runtime->needs_frame()?1:0; }
int cssx_test_request(void* p,const char* json,CssxSink sink,void* output) {
    try { if(!json || std::strlen(json)>1024*1024) throw std::runtime_error("request exceeds bound"); emit(sink,output,static_cast<Harness*>(p)->runtime->request(Json::parse(json))); return 1; }
    catch(const std::exception& e) { emit(sink,output,{{"error",e.what()}}); return 0; }
}
int cssx_test_stop(void* p) { try { return static_cast<Harness*>(p)->runtime->stop()?1:0; } catch(...) { return 0; } }
void cssx_test_destroy(void* p) { auto* h=static_cast<Harness*>(p); if(h->runtime->stopped()) delete h; }
}
}

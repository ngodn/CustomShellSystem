// Native extension fixtures for the lifecycle test. One source, four binaries:
//   mode 1: ABI 1 table (no render member)
//   mode 2: ABI 2 table without render
//   mode 3: ABI 2 table whose render fails on the second frame
//   mode 4: ABI 3 table with status() and the direct host services
#include "cssx/abi.h"
#include "cssx/hud.h"
#include <cstring>
#include <string>

namespace {
struct Instance { unsigned frames=0; CssxHost host{}; bool logged=false; };
void* create(const CssxHost* host) {
    if(!host) return nullptr;
    // Every fixture checks that the host is stamped with its own ABI.
    const uint32_t expected=CSSX_TEST_MODE==1?1u:CSSX_TEST_MODE==4?3u:2u;
    if(host->abi!=expected) return nullptr;
    if(CSSX_TEST_MODE==1 && host->size<offsetof(CssxHost,hud)) return nullptr;
    if(CSSX_TEST_MODE==2 && host->hud==nullptr) return nullptr;   // ABI 2 must receive a HUD table
    auto* instance=new Instance();
    std::memcpy(&instance->host,host,host->size<sizeof(CssxHost)?host->size:sizeof(CssxHost));
    if(CSSX_TEST_MODE==4) {
        if(!host->log || !host->invalidate || !host->now_us) { delete instance; return nullptr; }
        static const char text[]="fixture created";
        host->log(host->context,CSSX_LOG_INFO,text,sizeof(text)-1);
        instance->logged=host->now_us(host->context)>0;
    }
    return instance;
}
int tick(void* p,double) { return p?1:0; }
int model(void*,CssxSink sink,void* output) {
    constexpr char json[]="{\"sections\":[{\"id\":\"main\",\"title\":\"Main\",\"controls\":[{\"id\":\"go\",\"type\":\"button\",\"label\":\"Go\"}]}]}";
    sink(output,json,sizeof(json)-1); return 1;
}
int event(void* p,const char* text) {
    if(CSSX_TEST_MODE==4) { auto* instance=static_cast<Instance*>(p); instance->host.invalidate(instance->host.context); }
    return text!=nullptr;
}
int stop(void*) { return 1; }
void destroy(void* p) { delete static_cast<Instance*>(p); }
int render(void* p,const CssxFrame* frame) {
    auto* instance=static_cast<Instance*>(p);
    if(frame->abi!=2) return 0;
    return ++instance->frames==1;   // second frame fails: exercises suspension
}
int status(void* p,CssxSink sink,void* output) {
    auto* instance=static_cast<Instance*>(p);
    const std::string json=std::string("{\"summary\":\"frames ")+std::to_string(instance->frames)+"\",\"active\":"+(instance->logged?"true":"false")+"}";
    sink(output,json.data(),json.size()); return 1;
}
}
extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{
        CSSX_TEST_MODE==1?1u:CSSX_TEST_MODE==4?3u:2u,
        CSSX_TEST_MODE==1?CSSX_ABI1_EXTENSION_SIZE:CSSX_TEST_MODE==4?uint32_t(sizeof(CssxExtension)):CSSX_ABI2_EXTENSION_SIZE,
        create,tick,model,event,stop,destroy,
        CSSX_TEST_MODE==3?render:nullptr,
        CSSX_TEST_MODE==4?status:nullptr};
    return &api;
}

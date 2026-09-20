#include "cssx/api.h"
#include <cstring>

namespace {
void* create(const CssxHost*) { return new unsigned(0); }
int model(void*, CssxSink sink, void* output) {
    constexpr char json[] = "{\"sections\":[]}";
    sink(output,json,sizeof(json)-1); return 1;
}
int event(void*,const char*) { return 1; }
int stop(void*) { return 1; }
void destroy(void* instance) { delete static_cast<unsigned*>(instance); }
int render(void* instance,const CssxFrame*) {
    // Fail on the second frame to exercise runtime suspension and cleanup.
    return ++*static_cast<unsigned*>(instance)==1;
}
}
extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{
        CSSX_TEST_MODE==1?1u:2u,
        CSSX_TEST_MODE==1?uint32_t(offsetof(CssxExtension,render)):uint32_t(sizeof(CssxExtension)),
        create,nullptr,model,event,stop,destroy,CSSX_TEST_MODE==3?render:nullptr};
    return &api;
}

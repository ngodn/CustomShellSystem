// CSSX ABI 3 table for CINE. Every entry is noexcept and catches everything: an exception
// must never cross the DLL boundary into the host.
#include "cine.hpp"
#include <cstring>

namespace {
void* create(const CssxHost* host) noexcept {
    try { return new cine::Extension(host); } catch (...) { return nullptr; }
}
int tick(void* instance, double seconds) noexcept {
    try { static_cast<cine::Extension*>(instance)->tick(seconds); return 1; } catch (...) { return 1; }   // tick handles its own failures
}
int model(void* instance, CssxSink sink, void* output) noexcept {
    try { const auto text = static_cast<cine::Extension*>(instance)->model().dump(); sink(output, text.data(), text.size()); return 1; }
    catch (...) { return 0; }
}
int event(void* instance, const char* text) noexcept {
    try {
        if (!text || std::strlen(text) > 1024 * 1024) return 0;
        static_cast<cine::Extension*>(instance)->event(cine::Json::parse(text));
        return 1;
    } catch (...) { return 0; }
}
int stop(void* instance) noexcept {
    try { return static_cast<cine::Extension*>(instance)->stop() ? 1 : 0; } catch (...) { return 1; }
}
void destroy(void* instance) noexcept { delete static_cast<cine::Extension*>(instance); }
int render(void* instance, const CssxFrame* frame) noexcept {
    try { return static_cast<cine::Extension*>(instance)->render(frame); } catch (...) { return 1; }
}
int status(void* instance, CssxSink sink, void* output) noexcept {
    try { const auto text = static_cast<cine::Extension*>(instance)->status().dump(); sink(output, text.data(), text.size()); return 1; }
    catch (...) { return 0; }
}
}

extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{CSSX_ABI, sizeof(CssxExtension), create, tick, model, event, stop, destroy, render, status};
    return &api;
}

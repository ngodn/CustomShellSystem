#include "teleport.hpp"
#include <cstring>

namespace {
void* create(const CssxHost* host) noexcept {
    try { return new teleport::Extension(host); } catch (...) { return nullptr; }
}
int tick(void* instance, double seconds) noexcept {
    try { static_cast<teleport::Extension*>(instance)->tick(seconds); return 1; } catch (...) { return 0; }
}
int model(void* instance, CssxSink sink, void* output) noexcept {
    try { const auto text = static_cast<teleport::Extension*>(instance)->model().dump(); sink(output, text.data(), text.size()); return 1; }
    catch (...) { return 0; }
}
int event(void* instance, const char* text) noexcept {
    try {
        if (!text || std::strlen(text) > 1024 * 1024) return 0;
        static_cast<teleport::Extension*>(instance)->event(teleport::Json::parse(text));
        return 1;
    } catch (...) { return 0; }
}
int stop(void* instance) noexcept {
    try { return static_cast<teleport::Extension*>(instance)->stop() ? 1 : 0; } catch (...) { return 0; }
}
void destroy(void* instance) noexcept { delete static_cast<teleport::Extension*>(instance); }
int render(void* instance, const CssxFrame* frame) noexcept {
    try { return static_cast<teleport::Extension*>(instance)->render(frame); } catch (...) { return 0; }
}
int status(void* instance, CssxSink sink, void* output) noexcept {
    try { const auto text = static_cast<teleport::Extension*>(instance)->status().dump(); sink(output, text.data(), text.size()); return 1; }
    catch (...) { return 0; }
}
}

extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{CSSX_ABI, sizeof(CssxExtension), create, tick, model, event, stop, destroy, render, status};
    return &api;
}

// The ABI table. Everything else lives in the tuner.
#include "tuner.hpp"
#include <cstring>

namespace {
void* create(const CssxHost* host) noexcept {
    try { return new tuner::Tuner(host); } catch (...) { return nullptr; }
}
int tick(void* instance, double seconds) noexcept {
    try { static_cast<tuner::Tuner*>(instance)->tick(seconds); return 1; } catch (...) { return 1; }
}
int model(void* instance, CssxSink sink, void* output) noexcept {
    try { const auto text = static_cast<tuner::Tuner*>(instance)->model().dump(); sink(output, text.data(), text.size()); return 1; }
    catch (...) { return 0; }
}
int event(void* instance, const char* text) noexcept {
    try {
        if (!text || std::strlen(text) > 64 * 1024) return 0;
        static_cast<tuner::Tuner*>(instance)->event(tuner::Json::parse(text));
        return 1;
    } catch (...) { return 0; }
}
int stop(void* instance) noexcept {
    try { return static_cast<tuner::Tuner*>(instance)->stop() ? 1 : 0; } catch (...) { return 0; }
}
void destroy(void* instance) noexcept { delete static_cast<tuner::Tuner*>(instance); }
int status(void* instance, CssxSink sink, void* output) noexcept {
    try { const auto text = static_cast<tuner::Tuner*>(instance)->status().dump(); sink(output, text.data(), text.size()); return 1; }
    catch (...) { return 0; }
}
}

extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{CSSX_ABI, sizeof(CssxExtension), create, tick, model, event, stop, destroy, nullptr, status};
    return &api;
}

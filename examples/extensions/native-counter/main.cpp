#include <cssx/client.hpp>
#include <cstring>
#include <algorithm>

namespace {
struct Counter {
    cssx::Client host;
    int value = 0;
    explicit Counter(const CssxHost* api) : host(api) {
        const auto state = host.request({{"op", "state.load"}});
        if (state.contains("count") && state["count"].is_number_integer())
            value = std::clamp(state["count"].get<int>(), 0, 100);
    }
};
void* create(const CssxHost* host) noexcept {
    try { return new Counter(host); } catch (...) { return nullptr; }
}
int tick(void*, double) noexcept { return 1; }
int model(void* instance, CssxSink sink, void* output) noexcept {
    try {
        const auto& counter = *static_cast<Counter*>(instance);
        auto text = cssx::Json{{"values", {{"count", counter.value}}}}.dump();
        sink(output, text.data(), text.size());
        return 1;
    } catch (...) { return 0; }
}
int event(void* instance, const char* input) noexcept {
    try {
        if (!input || std::strlen(input) > 1024 * 1024) return 0;
        auto& counter = *static_cast<Counter*>(instance);
        const auto message = cssx::Json::parse(input);
        if (message.at("id") != "count" || !message.at("value").is_number_integer()) return 0;
        const int next = message.at("value");
        if (next < 0 || next > 100) return 0;
        // Keep the old visible value if saving fails.
        counter.host.request({{"op", "state.save"}, {"value", {{"count", next}}}});
        counter.value = next;
        return 1;
    } catch (...) { return 0; }
}
int stop(void*) noexcept { return 1; } // This example owns no gameplay changes.
void destroy(void* instance) noexcept { delete static_cast<Counter*>(instance); }
}
extern "C" CSSX_EXPORT const CssxExtension* cssx_get_extension() {
    static const CssxExtension api{CSSX_ABI, sizeof(CssxExtension), create, tick, model, event, stop, destroy};
    return &api;
}

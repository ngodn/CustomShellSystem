#pragma once
#include "abi.h"
#include "hud.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <stdexcept>
#include <string>

namespace cssx {
using Json = nlohmann::json;

/* Thin C++ wrapper over CssxHost for native extensions. Owns nothing that
 * crosses the DLL boundary: requests are UTF-8 buffers, responses are copied
 * out of the sink before parsing. Accepts a host stamped ABI 1, 2 or 3 so one
 * extension source builds against any supported host; the ABI-3 direct
 * services are used only when the host provides them. */
class Client {
    CssxHost host_{};
    struct Response { std::string text; bool overflow = false; };
    static void receive(void* context, const char* bytes, size_t count) noexcept {
        auto& response = *static_cast<Response*>(context);
        if (response.overflow || count > 1024 * 1024 || response.text.size() + count > 1024 * 1024) { response.overflow = true; return; }
        try { response.text.append(bytes, count); } catch (...) { response.overflow = true; }
    }
    bool has(size_t member_end) const noexcept { return host_.size >= member_end; }
public:
    explicit Client(const CssxHost* host) {
        if (!host || host->abi < 1 || host->abi > CSSX_ABI || !host->request)
            throw std::runtime_error("Incompatible CSSX host");
        const uint32_t minimum = host->abi >= 3 ? uint32_t(sizeof(CssxHost)) : host->abi == 2 ? CSSX_ABI2_HOST_SIZE : uint32_t(offsetof(CssxHost, hud));
        if (host->size < minimum) throw std::runtime_error("Truncated CSSX host");
        std::memset(&host_, 0, sizeof host_);
        std::memcpy(&host_, host, host->size < sizeof(CssxHost) ? host->size : sizeof(CssxHost));
    }
    uint32_t abi() const noexcept { return host_.abi; }
    const CssxHost& raw_host() const noexcept { return host_; }
    void* context() const noexcept { return host_.context; }
    const CssxHudApi* hud() const noexcept { return host_.abi >= 2 && has(CSSX_ABI2_HOST_SIZE) ? host_.hud : nullptr; }
    Json request(const Json& value) const {
        auto bytes = value.dump(); Response response;
        if (bytes.size() > 1024 * 1024) throw std::runtime_error("CSSX request exceeds 1 MiB");
        const auto ok = host_.request(host_.context, bytes.c_str(), receive, &response);
        if (response.overflow) throw std::runtime_error("CSSX response exceeds 1 MiB");
        auto result = response.text.empty() ? Json() : Json::parse(response.text);
        if (!ok) throw std::runtime_error(result.is_object() ? result.value("error", std::string("CSSX host request failed")) : std::string("CSSX host request failed"));
        return result;
    }
    Json player() const { return request({{"op", "player"}}); }
    Json get(const Json& object, const std::string& property) const { return request({{"op", "get"}, {"target", object}, {"property", property}}); }
    Json set(const Json& object, const std::string& property, const Json& value) const { return request({{"op", "set"}, {"target", object}, {"property", property}, {"value", value}}); }
    Json call(const Json& object, const std::string& function, const Json& arguments = Json::array()) const {
        auto result = request({{"op", "call"}, {"target", object}, {"function", function}, {"args", arguments}});
        return result.is_object() && result.contains("ReturnValue") ? result.at("ReturnValue") : result;
    }
    Json find(const std::string& path) const { return request({{"op", "find"}, {"path", path}}); }
    void log(const std::string& message, const std::string& level = "info", const Json& fields = Json::object()) const {
        if (host_.abi >= 3 && has(offsetof(CssxHost, invalidate)) && host_.log && fields.empty()) {
            const int severity = level == "debug" ? CSSX_LOG_DEBUG : level == "warning" ? CSSX_LOG_WARNING : level == "error" ? CSSX_LOG_ERROR : CSSX_LOG_INFO;
            host_.log(host_.context, severity, message.data(), message.size());
            return;
        }
        request({{"op", "log"}, {"level", level}, {"message", message}, {"fields", fields}});
    }
    void invalidate() const {
        if (host_.abi >= 3 && has(offsetof(CssxHost, now_us)) && host_.invalidate) { host_.invalidate(host_.context); return; }
        request({{"op", "invalidate"}});
    }
    uint64_t now_us() const noexcept {
        return host_.abi >= 3 && has(sizeof(CssxHost)) && host_.now_us ? host_.now_us(host_.context) : 0;
    }
};
}

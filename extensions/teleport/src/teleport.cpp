#include "teleport.hpp"

namespace teleport {

Extension::Extension(const CssxHost* host) : host_(host) {
    hud_ = host_.hud();
    ctx_ = host_.context();
    load_settings();
}

void Extension::load_settings() {
    Json s;
    try { s = host_.request({{"op", "state.load"}}); } catch (...) { s = Json::object(); }
    if (!s.is_object()) s = Json::object();
    if (s.contains("enable") && s["enable"].is_boolean()) enable_ = s["enable"].get<bool>();
    if (s.contains("allow_locked") && s["allow_locked"].is_boolean()) allow_locked_ = s["allow_locked"].get<bool>();
    if (s.contains("confirmation") && s["confirmation"].is_string()) {
        const auto v = s["confirmation"].get<std::string>();
        if (v == "show" || v == "skip") confirmation_ = v;
    }
    if (s.contains("camera") && s["camera"].is_string()) {
        const auto v = s["camera"].get<std::string>();
        if (v == "front" || v == "default") camera_ = v;
    }
}

void Extension::save_settings() {
    const Json value = {
        {"enable", enable_},
        {"confirmation", confirmation_},
        {"allow_locked", allow_locked_},
        {"camera", camera_},
    };
    try { host_.request({{"op", "state.save"}, {"value", value}}); } catch (...) {}
}

void Extension::report(const std::string& text) {
    if (status_ == text) return;
    status_ = text;
    try { host_.log(text); } catch (...) {}
    try { host_.invalidate(); } catch (...) {}
}

Json Extension::model() {
    Json values = {
        {"enable", enable_},
        {"confirmation", confirmation_},
        {"allow_locked", allow_locked_},
        {"camera", camera_},
    };
    Json enabled = {
        {"enable", true},
        {"confirmation", enable_},
        {"allow_locked", enable_},
        {"camera", enable_},
    };
    std::string summary = enable_
        ? "On. Open the world map and hover a point, then press T or R3 to teleport."
        : "Off. The world map behaves normally.";
    return {
        {"values", values},
        {"enabled", enabled},
        {"status", error_.empty() ? summary + " " + status_ : summary},
        {"error", error_},
    };
}

void Extension::event(const Json& event) {
    error_.clear();
    try {
        const auto id = event.at("id").get<std::string>();
        if (id == "enable") {
            enable_ = event.at("value").get<bool>();
            if (!enable_) { hide_confirm(); hide_prompt(); confirming_ = false; }
            save_settings();
            report(enable_ ? "Map teleport on." : "Map teleport off.");
            return;
        }
        if (id == "allow_locked") {
            allow_locked_ = event.at("value").get<bool>();
            save_settings();
            report(allow_locked_ ? "Locked points can be teleported to." : "Only fast-travel points show the prompt.");
            return;
        }
        if (id == "confirmation") {
            const auto v = event.at("value").get<std::string>();
            if (v != "show" && v != "skip") throw std::runtime_error("Unknown confirmation option.");
            confirmation_ = v;
            save_settings();
            report(v == "show" ? "Teleport will ask before jumping." : "Teleport jumps on the first press.");
            return;
        }
        if (id == "camera") {
            const auto v = event.at("value").get<std::string>();
            if (v != "front" && v != "default") throw std::runtime_error("Unknown camera option.");
            camera_ = v;
            save_settings();
            report(v == "front" ? "Departure faces the character." : "Departure keeps the game camera.");
            return;
        }
        throw std::runtime_error("Unknown Teleport setting: " + id);
    } catch (const std::exception& e) {
        error_ = e.what();
        try { host_.invalidate(); } catch (...) {}
        throw;
    }
}

void Extension::tick(double seconds) {
    if (stopped_) return;
    // Release a pushed departure camera on a timer, so it self-heals even if the
    // teleport or the map close took an unexpected path. render() does the main work.
    if (camera_clear_in_ > 0) {
        camera_clear_in_ -= seconds;
        if (camera_clear_in_ <= 0) clear_camera();
    }
}

Json Extension::status() {
    return {{"summary", enable_ ? "Map teleport ready" : "Off"}, {"active", enable_}};
}

bool Extension::stop() {
    stopped_ = true;
    try { clear_camera(); } catch (...) {}
    try { drop_overlay(); } catch (...) {}
    return true;
}

}

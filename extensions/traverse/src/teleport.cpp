#include "teleport.hpp"

namespace teleport {

Extension::Extension(const CssxHost* host) : host_(host) {
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
    if (s.contains("meteor") && s["meteor"].is_boolean()) meteor_ = s["meteor"].get<bool>();
}

void Extension::save_settings() {
    const Json value = {
        {"enable", enable_},
        {"confirmation", confirmation_},
        {"allow_locked", allow_locked_},
        {"meteor", meteor_},
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
        {"meteor", meteor_},
    };
    Json enabled = {
        {"enable", true},
        {"confirmation", enable_},
        {"allow_locked", enable_},
        {"meteor", enable_},
    };
    std::string summary = enable_
        ? "On. Open the world map and hover a point, then press T or click the left stick (L3) to traverse."
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
            if (!enable_) { hide_prompt(); hide_dialog(); confirming_ = false; cleanup_meteor(false); }
            save_settings();
            report(enable_ ? "Map traverse on." : "Map traverse off.");
            return;
        }
        if (id == "allow_locked") {
            allow_locked_ = event.at("value").get<bool>();
            save_settings();
            report(allow_locked_ ? "Locked points can be traversed to." : "Only fast-travel points show the prompt.");
            return;
        }
        if (id == "confirmation") {
            const auto v = event.at("value").get<std::string>();
            if (v != "show" && v != "skip") throw std::runtime_error("Unknown confirmation option.");
            confirmation_ = v;
            save_settings();
            report(v == "show" ? "Traverse will ask before going." : "Traverse goes on the first press.");
            return;
        }
        if (id == "meteor") {
            meteor_ = event.at("value").get<bool>();
            if (!meteor_) cleanup_meteor(false);
            save_settings();
            report(meteor_ ? "Arrive as a falling meteor." : "Arrive with a plain streaming teleport.");
            return;
        }
        throw std::runtime_error("Unknown Traverse setting: " + id);
    } catch (const std::exception& e) {
        error_ = e.what();
        try { host_.invalidate(); } catch (...) {}
        throw;
    }
}

void Extension::tick(double) {
    // All timed work (the meteor descent, the landing-camera hold) is driven from
    // render(), which runs every frame; tick() has nothing to do.
}

Json Extension::status() {
    return {{"summary", enable_ ? "Map traverse ready" : "Off"}, {"active", enable_}};
}

bool Extension::stop() {
    stopped_ = true;
    try { cleanup_meteor(false); } catch (...) {}
    try { hide_dialog(); } catch (...) {}
    try { hide_prompt(); } catch (...) {}
    return true;
}

}

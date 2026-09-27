#pragma once
// Typed menu controls (the CSSX menu schema): what the page can show and how a value moves.
// Portable: no engine dependency, so the model logic is testable on the host.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ccs {
using Json = nlohmann::json;
inline bool adjustable(const Json& c) {
    const auto type = c.at("type");
    return type == "number" || type == "slider" || type == "choice" || type == "radio";
}
inline bool interactive(const Json& c) {
    const auto type = c.at("type");
    return type != "label" && type != "progress" && type != "loading" && c.value("enabled", true) && !c.value("busy", false);
}
inline std::string display_value(const Json& c) {
    const auto type = c.at("type");
    if (c.value("busy", false)) return "Working...";
    if (type == "toggle") return c.at("value").get<bool>() ? "On" : "Off";
    if (type == "loading") return c.at("value").get<bool>() ? "Loading..." : "Ready";
    if (type == "choice" || type == "radio") {
        for (const auto& option : c.at("options")) if (option.at("id") == c.at("value")) return option.at("label").get<std::string>();
    }
    if (type == "text") return c.at("value").get<std::string>();
    if (type == "number" || type == "slider" || type == "progress") {
        const auto value = c.at("value").get<double>() * (type == "progress" ? 100. : 1.);
        std::ostringstream out; out << std::fixed << std::setprecision(type == "progress" ? 0 : 4) << value;
        auto text = out.str();
        if (text.find('.') != std::string::npos) { while (text.ends_with('0')) text.pop_back(); if (text.ends_with('.')) text.pop_back(); }
        if (type == "progress") text += "%";
        return text;
    }
    return {};
}
inline double snap_value(const Json& c, double raw) {
    if (!std::isfinite(raw)) throw std::runtime_error("Value must be finite");
    const double low = c.at("min"), high = c.at("max"), step = c.at("step");
    return std::clamp(low + std::round((std::clamp(raw, low, high) - low) / step) * step, low, high);
}
inline Json adjusted_value(const Json& c, int direction) {
    if (!interactive(c) || !adjustable(c)) throw std::runtime_error("Control cannot be adjusted");
    const auto type = c.at("type");
    if (type == "number" || type == "slider") return snap_value(c, c.at("value").get<double>() + (direction < 0 ? -1 : 1) * c.at("step").get<double>());
    const auto& options = c.at("options");
    for (size_t i = 0; i < options.size(); ++i) if (options[i].at("id") == c.at("value"))
        return options[(i + options.size() + (direction < 0 ? -1 : 1)) % options.size()].at("id");
    throw std::runtime_error("Selected option no longer exists");
}
// Check an event against the control it targets: interactive, confirmed when required, and a
// value of the control's own kind and range.
inline void validate_event(const Json& model, const Json& event) {
    if (!event.is_object()) throw std::runtime_error("Control event must be an object");
    const auto id = event.at("id").get<std::string>();
    for (const auto& section : model.at("sections")) for (const auto& c : section.at("controls")) if (c.at("id") == id) {
        if (!interactive(c)) throw std::runtime_error("Control is disabled or read-only");
        if (c.contains("confirm") && !event.value("confirmed", false)) throw std::runtime_error("Control requires confirmation");
        const auto type = c.at("type").get<std::string>();
        if (type == "button") return;
        const auto& value = event.at("value");
        if (type == "toggle") { if (!value.is_boolean()) throw std::runtime_error("Toggle value must be a boolean"); }
        else if (type == "number" || type == "slider") {
            if (!value.is_number() || !std::isfinite(value.get<double>())) throw std::runtime_error("Value must be a finite number");
            const double v = value.get<double>();
            if (v < c.at("min").get<double>() - 1e-9 || v > c.at("max").get<double>() + 1e-9) throw std::runtime_error("Value is out of range");
        } else if (type == "choice" || type == "radio") {
            bool found = false;
            for (const auto& option : c.at("options")) if (option.at("id") == value) found = true;
            if (!found) throw std::runtime_error("Unknown option");
        } else if (type == "text") {
            if (!value.is_string() || value.get_ref<const std::string&>().size() > 256) throw std::runtime_error("Text must be at most 256 bytes");
        }
        return;
    }
    throw std::runtime_error("Unknown control ID");
}
}

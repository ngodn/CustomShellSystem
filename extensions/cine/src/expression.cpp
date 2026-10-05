#include "expression.hpp"
#include <algorithm>
#include <stdexcept>

namespace cine {
std::string expression_choice(const Json& value) {
    const auto id = value.get<std::string>();
    if (id.empty() || id.size() > 96 || !std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        })) throw std::runtime_error("Invalid facial expression ID");
    return id;
}
void ExpressionCatalog::refresh(const PerformanceCall& call) {
    entries_.clear();
    try {
        const auto result = call({{"action", "expressions"}});
        const auto& entries = result.at("entries");
        if (!entries.is_array() || entries.size() > 128) throw std::runtime_error("Invalid expression catalog size");
        std::map<std::string, std::string> parsed;
        for (const auto& entry : entries) {
            const auto id = expression_choice(entry.at("id"));
            const auto name = entry.at("name").get<std::string>();
            if (id == "unchanged" || id == "original" || name.empty() || name.size() > 160 ||
                name.find('\0') != std::string::npos || !parsed.emplace(id, name).second)
                throw std::runtime_error("Invalid or duplicate facial expression");
        }
        entries_ = std::move(parsed);
        error_ = entries_.empty() ? "No facial expressions are installed for this outfit." : "";
    } catch (const std::exception& e) { error_ = std::string("Expressions unavailable: ") + e.what(); }
}
Json ExpressionCatalog::options(const std::string& choice) const {
    Json result = Json::array({{{"id", "unchanged"}, {"label", "As set in CSS"}},
                               {{"id", "original"}, {"label", "Original face"}}});
    for (const auto& [id, name] : entries_) result.push_back({{"id", id}, {"label", name}});
    if (choice != "unchanged" && choice != "original" && !entries_.contains(choice))
        result.push_back({{"id", choice}, {"label", "Unavailable: " + choice}});
    return result;
}
std::string ExpressionCatalog::unavailable(const std::string& choice) const {
    if (choice == "unchanged" || choice == "original") return {};
    if (!error_.empty()) return error_;
    return entries_.contains(choice) ? "" : "This expression is unavailable for the worn outfit.";
}
std::string ExpressionCatalog::description(const std::string& choice) const {
    if (const auto why = unavailable(choice); !why.empty()) return why;
    if (choice == "unchanged") return "Keeps your CSS expression throughout the take.";
    return "Applies before the take; restores your previous expression when it ends.";
}
bool ExpressionTake::same_look(const Json& state) const {
    for (const auto* key : {"shell", "outfit", "variant"})
        if (!state.contains(key) || state.at(key) != scope_.at(key)) return false;
    return true;
}
void ExpressionTake::start(const PerformanceCall& call, const std::string& choice) {
    expression_choice(choice);
    if (owned()) throw std::runtime_error("Wait for the previous expression to be restored");
    if (choice == "unchanged") return;
    const auto state = call({{"action", "describe"}});
    if (state.at("busy").get<bool>()) throw std::runtime_error("Wait for CSS to finish applying your look");
    before_ = state.at("customize").value("expression", std::string());
    desired_ = choice == "original" ? "" : choice;
    restoring_ = false;
    if (before_ == desired_) {
        const auto& applied = state.at("expression");
        if (applied.at("applied") != desired_ || !applied.at("reason").get<std::string>().empty())
            throw std::runtime_error("The saved expression is not active. Check its shared assets in CSS.");
        return;
    }
    scope_ = {{"shell", state.at("shell")}, {"outfit", state.at("outfit")}, {"variant", state.at("variant")}};
    // Keep the snapshot even if the service throws after accepting the write.
    call({{"action", "apply"}, {"persist", false}, {"commands", Json::array({
        {{"action", "expression"}, {"expression", desired_}}})}});
}
bool ExpressionTake::ready(const PerformanceCall& call) const {
    if (!owned()) return true;
    const auto state = call({{"action", "describe"}});
    if (state.at("busy").get<bool>()) return false;
    if (!same_look(state) || state.at("customize").value("expression", std::string()) != desired_)
        throw std::runtime_error("The outfit or expression changed before the take");
    const auto& applied = state.at("expression");
    if (applied.at("applied") != desired_ || !applied.at("reason").get<std::string>().empty())
        throw std::runtime_error("CSS could not apply the expression: " + applied.at("reason").get<std::string>());
    return true;
}
bool ExpressionTake::stop(const PerformanceCall& call) {
    if (!owned()) return true;
    const auto state = call({{"action", "describe"}});
    if (state.at("busy").get<bool>()) return false;
    const auto current = state.at("customize").value("expression", std::string());
    if (!same_look(state) || current != desired_ || restoring_) {
        scope_ = nullptr;
        return true;
    }
    // Merge into the current customization, preserving any body/material edits made during the take.
    auto custom = state.at("customize");
    if (before_.empty()) custom.erase("expression"); else custom["expression"] = before_;
    call({{"action", "apply"}, {"persist", true}, {"commands", Json::array({
        {{"action", "restore"}, {"customize", custom}}})}});
    restoring_ = true;
    return false;
}
}

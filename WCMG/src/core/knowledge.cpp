#include "model.hpp"
#include "wcmg_knowledge_embedded.hpp"
#include <nlohmann/json.hpp>

namespace wcmg {
namespace {
Severity severity_from(const std::string& s) {
    auto l = lower(s);
    if (l == "critical") return Severity::Critical;
    if (l == "high") return Severity::High;
    if (l == "medium") return Severity::Medium;
    if (l == "low") return Severity::Low;
    return Severity::Info;
}
void merge(Knowledge& k, const nlohmann::json& j) {
    if (j.contains("version")) k.version = j["version"].get<std::string>();
    for (const auto& b : j.value("builds", nlohmann::json::array()))
        k.builds.push_back({b.value("build_id", ""), b.value("date", ""), b.value("note", "")});
    for (const auto& u : j.value("ue4ss", nlohmann::json::array()))
        k.ue4ss.push_back({lower(u.value("sha256", "")), u.value("version", ""), u.value("note", ""), u.value("supported", true)});
    for (const auto& m : j.value("mods", nlohmann::json::array()))
        k.mods.push_back({m.value("match", ""), m.value("display", ""), m.value("note", ""), m.value("affected_versions", ""),
                          severity_from(m.value("severity", "info"))});
    for (const auto& o : j.value("overlay_modules", nlohmann::json::array())) k.overlay_modules.push_back(o.get<std::string>());
    for (const auto& b : j.value("builtin_mods", nlohmann::json::array())) k.builtin_mods.push_back(b.get<std::string>());
}
}

Knowledge load_knowledge(const std::optional<fs::path>& override_file) {
    Knowledge k;
    try { merge(k, nlohmann::json::parse(kEmbeddedKnowledge)); } catch (...) {}
    if (override_file) {
        std::error_code ec;
        if (fs::is_regular_file(*override_file, ec)) {
            try {
                auto text = read_text(*override_file, 4 * 1024 * 1024);
                merge(k, nlohmann::json::parse(text));
                k.version += " + " + path_utf8(override_file->filename());
            } catch (...) {}
        }
    }
    return k;
}
}

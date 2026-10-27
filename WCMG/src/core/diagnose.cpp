#include "scanners.hpp"
#include "wcmg_version.hpp"
namespace wcmg {
Report diagnose(const Paths& paths, const std::optional<fs::path>& knowledge_override, const Progress& progress) {
    Report r;
    r.generated_by = "WCMG " WCMG_VERSION;
    r.evidence.paths = paths;
    r.evidence.scanned_at = std::chrono::system_clock::now();
    auto step = [&](const char* s) { if (progress) progress(s); };
    auto guard = [&](const char* what, auto&& fn) {
        try { fn(); } catch (const std::exception& e) { r.evidence.scan_errors.push_back(std::string(what) + ": " + e.what()); }
        catch (...) { r.evidence.scan_errors.push_back(std::string(what) + ": unknown error"); }
    };
    guard("knowledge", [&] { r.evidence.knowledge = load_knowledge(knowledge_override); });
    step("Reading crash reports"); guard("crash reports", [&] { r.evidence.crashes = scan_crash_reports(paths); });
    step("Reading UE4SS and its mods"); guard("ue4ss", [&] { r.evidence.ue4ss = scan_ue4ss(paths, r.evidence.knowledge); });
    step("Reading CSS, CSSX and CCS"); guard("our mods", [&] { r.evidence.ours = scan_our_mods(paths); });
    step("Reading pak containers"); guard("paks", [&] { r.evidence.paks = scan_paks(paths, progress); });
    step("Checking game files against the manifest"); guard("manifest", [&] { r.evidence.manifest = scan_manifest(paths, progress); });
    step("Reading system and config"); guard("system", [&] { r.evidence.system = scan_system(paths, r.evidence.crashes, r.evidence.knowledge); });
    step("Weighing the evidence"); guard("rules", [&] { r.findings = run_rules(r.evidence); r.verdict = write_verdict(r.evidence, r.findings); });
    return r;
}
}

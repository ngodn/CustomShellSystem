// Console runner: wcmg_cli <game dir or exe> [--json] [--md] [--zip out.zip] [--knowledge file]
// Used by the Linux tests against the real installation and reachable from the
// Windows exe as WCMG.exe --cli.
#include "scanners.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    using namespace wcmg;
    std::string target; bool json = false, md = false; fs::path zip; std::optional<fs::path> knowledge;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") json = true;
        else if (a == "--md") md = true;
        else if (a == "--zip" && i + 1 < argc) zip = argv[++i];
        else if (a == "--knowledge" && i + 1 < argc) knowledge = argv[++i];
        else target = a;
    }
    std::optional<Paths> paths;
    if (!target.empty()) paths = paths_from_user_pick(target);
    else { auto found = discover_installs(); if (!found.empty()) paths = found.front(); }
    if (!paths) { std::fprintf(stderr, "Game not found. Pass the Sparta folder, the MortalShell2 folder, the Win64 folder or the exe.\n"); return 2; }
    std::string why;
    if (!validate_paths(*paths, &why)) { std::fprintf(stderr, "%s\n", why.c_str()); return 2; }
    auto report = diagnose(*paths, knowledge, [](const std::string& s) { std::fprintf(stderr, "... %s\n", s.c_str()); });
    if (json) std::cout << report_json(report) << "\n";
    else if (md) std::cout << report_markdown(report) << "\n";
    else std::cout << report_text(report) << "\n";
    if (!zip.empty()) {
        try { auto out = collect_bundle(report, zip, [](const std::string& s) { std::fprintf(stderr, "... %s\n", s.c_str()); });
              std::fprintf(stderr, "Bundle written: %s\n", path_utf8(out).c_str()); }
        catch (const std::exception& e) { std::fprintf(stderr, "Bundle failed: %s\n", e.what()); return 3; }
    }
    return 0;
}

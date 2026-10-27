// CINE pack tests: folder matching, manifest validation and reading a pack through a mock CSSX
// bridge that answers load / table.rows / table.row like the real one (missing row = null).
#include "packs.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace cine;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
template<class F> void rejects(F f, const std::string& what) {
    bool failed = false; try { f(); } catch (...) { failed = true; } expect(failed, "expected rejection: " + what);
}
Json manifest(const std::string& pack = "IrisOut") {
    return {{"pack_schema", 1}, {"pack", pack}, {"name", "Iris Out"}, {"version", "1.0.0"}, {"song", "IRIS OUT"},
            {"presets", {"iris-out", "iris-out-9x16"}},
            {"audio", {{"iris-out-x4", {{"asset", "/Game/CSSX/CINE/" + pack + "/Audio/SW_iris_out_x4.SW_iris_out_x4"}, {"seconds", 89.1}}}}},
            {"requires", {{"cine", "2.0.0"}}}};
}

struct Host {
    std::map<std::string, Json> rows;   // row name -> DevComment JSON (dumped into the string)
    bool loads = true, wrong_type = false, bad_json = false;
    std::string bad_row;
    int requests = 0;
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};
    static int request(void* context, const char* value, CssxSink sink, void* output) {
        try { auto r = static_cast<Host*>(context)->handle(Json::parse(value)).dump(); sink(output, r.data(), r.size()); return 1; }
        catch (const std::exception& e) { auto r = Json{{"error", e.what()}}.dump(); sink(output, r.data(), r.size()); return 0; }
    }
    Json handle(const Json& j) {
        ++requests;
        const auto op = j.at("op").get<std::string>();
        if (op == "load") return loads ? Json{{"$object", 7}} : Json(nullptr);
        if (op == "table.rows") { Json names = Json::array(); for (const auto& [k, v] : rows) names.push_back(k); return names; }
        if (op == "table.row") {
            const auto it = rows.find(j.at("row").get<std::string>());
            if (it == rows.end()) return nullptr;
            if (wrong_type) return {{"RowName", "x"}};
            return {{"Tag", "None"}, {"DevComment", (bad_json || j.at("row") == bad_row) ? std::string("{oops") : it->second.dump()}};
        }
        throw std::runtime_error("unexpected op " + op);
    }
};
}

int main() {
    try {
        // Folder tokens.
        expect(pack_token("CINE_IrisOut_eins0fx_P") == "IrisOut", "token of a pack folder");
        expect(pack_token("CSSX_CINE_eins0fx_P").empty(), "the core pak is not a pack");
        expect(pack_token("CINE__eins0fx_P").empty(), "an empty token is refused");
        expect(pack_token("CINE_Iris-Out_eins0fx_P").empty(), "a token with punctuation is refused");
        expect(pack_token("CINE_IrisOut_someone_P").empty(), "another author's suffix is not ours");
        expect(table_path("IrisOut") == "/Game/CSSX/CINE/IrisOut/DT_CINE_Pack.DT_CINE_Pack", "table path");

        // Folder scan: complete trio only, sorted, broken ones named.
        const auto root = fs::temp_directory_path() / "cine_pack_tests";
        fs::remove_all(root); fs::create_directories(root);
        auto make = [&](const std::string& folder, std::initializer_list<const char*> exts) {
            fs::create_directories(root / folder);
            for (const char* e : exts) std::ofstream(root / folder / (folder + e)) << "x";
        };
        make("CINE_Walks_eins0fx_P", {".pak", ".ucas", ".utoc"});
        make("CINE_IrisOut_eins0fx_P", {".pak", ".ucas", ".utoc"});
        make("CINE_Broken_eins0fx_P", {".pak", ".utoc"});
        make("CSS_EvePeony_eins0fx_P", {".pak", ".ucas", ".utoc"});
        std::vector<std::string> problems;
        const auto found = find_pack_folders(root, problems);
        expect(found == std::vector<std::string>{"IrisOut", "Walks"}, "scan finds the complete packs in order");
        expect(problems.size() == 1 && problems[0].find("CINE_Broken_eins0fx_P") == 0, "the incomplete pack is named");
        problems.clear();
        expect(find_pack_folders(root / "missing", problems).empty() && problems.empty(), "no ~mods folder is no packs, quietly");
        fs::remove_all(root);

        // Manifest validation.
        const auto p = parse_manifest(manifest(), "IrisOut");
        expect(p.name == "Iris Out" && p.presets.size() == 2 && p.audio.at("iris-out-x4").seconds > 89, "manifest parsed");
        rejects([] { parse_manifest(manifest(), "Walks"); }, "manifest of another pack");
        rejects([] { auto m = manifest(); m["pack_schema"] = 2; parse_manifest(m, "IrisOut"); }, "newer pack format");
        rejects([] { auto m = manifest(); m["presets"] = Json::array(); parse_manifest(m, "IrisOut"); }, "no presets");
        rejects([] { auto m = manifest(); m["presets"] = {"a", "a"}; parse_manifest(m, "IrisOut"); }, "duplicate preset");
        rejects([] { auto m = manifest(); m["presets"] = {"Bad Id"}; parse_manifest(m, "IrisOut"); }, "invalid preset id");
        rejects([] { auto m = manifest(); m["audio"]["iris-out-x4"]["asset"] = "/Game/CSSX/CINE/Walks/Audio/SW_x.SW_x"; parse_manifest(m, "IrisOut"); },
                "audio in another pack's folder");
        rejects([] { auto m = manifest(); m["audio"]["iris-out-x4"]["seconds"] = -1; parse_manifest(m, "IrisOut"); }, "negative length");
        rejects([] { auto m = manifest(); m.erase("name"); parse_manifest(m, "IrisOut"); }, "missing name");

        // Reading a pack through the bridge.
        Host h;
        h.rows["manifest"] = manifest();
        h.rows["preset.iris-out"] = {{"id", "iris-out"}};
        h.rows["preset.iris-out-9x16"] = {{"id", "iris-out-9x16"}};
        h.rows["recipe.reel"] = {{"steps", Json::array()}};
        h.rows["junk"] = {{"x", 1}};
        cssx::Client client(&h.api);
        const auto pack = read_pack(client, "IrisOut");
        expect(pack.rows.size() == 3 && pack.rows.contains("recipe.reel") && !pack.rows.contains("junk"), "presets and recipes read, other rows ignored");
        expect(pack.rows.at("preset.iris-out").at("id") == "iris-out", "preset JSON round trip");

        Host bad_style = h; bad_style.api.context = &bad_style; bad_style.bad_row = "recipe.reel";
        cssx::Client style_client(&bad_style.api);
        const auto recovered = read_pack(style_client, "IrisOut");
        expect(recovered.rows.contains("preset.iris-out") && !recovered.rows.contains("recipe.reel"), "malformed optional style keeps the pack's scenes");
        expect(recovered.problems.size() == 1 && recovered.problems.front().find("recipe.reel") != std::string::npos, "malformed style warning names its row");

        Host missing = h; missing.api.context = &missing; missing.rows.erase("preset.iris-out-9x16");
        cssx::Client c2(&missing.api);
        rejects([&] { read_pack(c2, "IrisOut"); }, "manifest lists a preset the pack lacks");
        Host unloaded = h; unloaded.api.context = &unloaded; unloaded.loads = false;
        cssx::Client c3(&unloaded.api);
        rejects([&] { read_pack(c3, "IrisOut"); }, "the pak did not mount");
        Host typed = h; typed.api.context = &typed; typed.wrong_type = true;
        cssx::Client c4(&typed.api);
        rejects([&] { read_pack(c4, "IrisOut"); }, "a table of another row type");
        Host bad = h; bad.api.context = &bad; bad.bad_json = true;
        cssx::Client c5(&bad.api);
        rejects([&] { read_pack(c5, "IrisOut"); }, "a row that is not JSON");
        Host nomanifest = h; nomanifest.api.context = &nomanifest; nomanifest.rows.erase("manifest");
        cssx::Client c6(&nomanifest.api);
        rejects([&] { read_pack(c6, "IrisOut"); }, "no manifest row");

        std::cout << "cine pack tests: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
}

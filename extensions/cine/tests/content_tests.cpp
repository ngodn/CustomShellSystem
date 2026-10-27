#include "content.hpp"
#include "guides.hpp"
#include "layers.hpp"
#include "packs.hpp"
#include "preset.hpp"
#include <chrono>
#include <iostream>
#include <limits>

using namespace cine;
namespace {
size_t checks = 0;
void expect(bool value, const std::string& what) { ++checks; if (!value) throw std::runtime_error(what); }
void rejects(const Json& rows, const std::string& what) {
    bool failed = false;
    try { (void)parse_content(rows); } catch (const std::exception&) { failed = true; }
    expect(failed, "accepted invalid " + what);
}
void fields(Json& rows, Json& value, const Json& spec, const std::string& path) {
    const Json saved = value;
    const auto type = spec.at("type").get<std::string>();
    value = nullptr; rejects(rows, path + " null"); value = saved;
    if (type == "object") {
        for (const auto& [key, field] : spec.at("fields").items()) {
            value.erase(key); rejects(rows, path + "." + key + " missing"); value = saved;
            fields(rows, value[key], field, path + "." + key);
        }
        value["unexpected"] = 0; rejects(rows, path + " unknown field"); value = saved;
    } else if (type == "array") {
        value = Json::array();
        if (spec.at("min").get<size_t>() > 0) rejects(rows, path + " short array");
        value = saved;
        while (value.size() <= spec.at("max").get<size_t>()) value.push_back(saved.empty() ? Json("text") : saved.front());
        rejects(rows, path + " long array"); value = saved;
        for (size_t i = 0; i < value.size(); ++i) fields(rows, value[i], spec.at("items"), path + "[" + std::to_string(i) + "]");
    } else if (type == "number" || type == "integer") {
        value = "1"; rejects(rows, path + " text number");
        value = true; rejects(rows, path + " boolean number");
        value = spec.at("min").get<double>() - 1; rejects(rows, path + " below minimum");
        value = spec.at("max").get<double>() + 1; rejects(rows, path + " above maximum");
        value = std::numeric_limits<double>::infinity(); rejects(rows, path + " infinity");
        value = std::numeric_limits<double>::quiet_NaN(); rejects(rows, path + " NaN");
        if (type == "integer") { value = 1.5; rejects(rows, path + " fractional integer"); }
    } else if (type == "string") {
        value = 1; rejects(rows, path + " numeric text");
        value = std::string(spec.at("max").get<size_t>() + 1, 'x'); rejects(rows, path + " long text");
        value = std::string("a\0b", 3); rejects(rows, path + " NUL");
        if (spec.at("min").get<size_t>() > 0) { value = ""; rejects(rows, path + " empty text"); }
    } else if (type == "boolean") { value = 1; rejects(rows, path + " numeric boolean"); }
    value = saved;
}
struct Hud {
    CssxHudApi api{};
    CssxLayer made = 0;
    size_t colors = 0, fonts = 0;
    std::map<CssxLayer, std::string> texts;
    Hud() {
        api.image = api.text = [](void* p, CssxLayer) { return ++static_cast<Hud*>(p)->made; };
        api.layer_object = [](void*, CssxLayer l) { return uint64_t(l); };
        api.set_color = [](void* p, CssxLayer, float, float, float, float) { ++static_cast<Hud*>(p)->colors; };
        api.set_font = [](void* p, CssxLayer, float) { ++static_cast<Hud*>(p)->fonts; };
        api.set_visible = [](void*, CssxLayer, int32_t) {};
        api.set_rect = [](void*, CssxLayer, float, float, float, float, float, float, int32_t) {};
        api.set_angle = [](void*, CssxLayer, float) {};
        api.set_pivot = [](void*, CssxLayer, float, float) {};
        api.set_text = [](void* p, CssxLayer l, const char* data, size_t size) { static_cast<Hud*>(p)->texts[l] = std::string(data, size); };
    }
};
struct Host {
    Json rows = default_content_rows();
    size_t requests = 0, fail_at = 0, expire_at = 0;   // expire_at: that request answers "Object handle expired" once
    bool missing = false, malformed = false, wrong_type = false;
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};
    static int request(void* self, const char* data, CssxSink sink, void* output) {
        try {
            const auto result = static_cast<Host*>(self)->handle(Json::parse(data)).dump();
            sink(output, result.data(), result.size()); return 1;
        } catch (const std::exception& e) {
            const auto error = Json{{"error", e.what()}}.dump(); sink(output, error.data(), error.size()); return 0;
        }
    }
    Json handle(const Json& j) {
        if (++requests == fail_at) throw std::runtime_error("injected bridge failure");
        if (requests == expire_at) throw std::runtime_error("Object handle expired");
        const auto op = j.at("op").get<std::string>();
        if (op == "load") { expect(j.at("path") == core_table_path, "core object path"); return missing ? Json() : Json{{"$object", 12}}; }
        if (op == "table.rows") { Json out = Json::array(); for (const auto& [name, row] : rows.items()) { (void)row; out.push_back(name); } return out; }
        if (op == "table.row") {
            if (wrong_type) return {{"Other", "wrong struct"}};
            return {{"Tag", "None"}, {"DevComment", malformed ? std::string("{") : rows.at(j.at("row").get<std::string>()).dump()}};
        }
        throw std::runtime_error("unexpected core operation");
    }
};
void drain(ContentLoader& loader, Host& host) {
    cssx::Client client(&host.api);
    // The whole table is read within one advance: a garbage collection between ticks would expire
    // the handle mid-read. Only an expired handle makes another advance read again.
    for (size_t i = 0; !loader.done() && i < 8; ++i) loader.advance(client);
    expect(loader.done(), "loader terminates");
    const size_t before = host.requests;
    for (int i = 0; i < 100; ++i) loader.advance(client);
    expect(host.requests == before, "finished loader makes no requests");
}
}

int main() {
    try {
        auto rows = default_content_rows();
        (void)parse_content(rows);
        for (const auto& [name, spec] : content_schema().items()) {
            auto copy = rows; copy.erase(name); rejects(copy, name + " missing row");
            fields(rows, rows[name], spec, name);
        }
        expect(rows == default_content_rows(), "mutation tests restored input");
        auto invalid = rows;
        invalid["style.guides"]["catalog"][1]["id"] = invalid["style.guides"]["catalog"][0]["id"]; rejects(invalid, "duplicate guide");
        invalid = rows; invalid["recipe.body"]["targets"][1]["weights"][0] = 0.5; rejects(invalid, "unnormalized body weights");
        invalid = rows; invalid["recipe.reel"]["steps"][0]["layers"] = {"unknown"}; rejects(invalid, "unknown layer");
        invalid = rows; invalid["recipe.reel"]["steps"][1]["at"] = 0.01; rejects(invalid, "reel order");
        invalid = rows; invalid["style.geometry"]["radius_min"] = 2; rejects(invalid, "reversed geometry bounds");
        invalid = rows; invalid["recipe.palettes"]["exclude"] = {"other"}; rejects(invalid, "destructive palette reset");
        invalid = rows; invalid["style.breakdown"]["wire"] = "/Game/Elsewhere/M_Wire.M_Wire"; rejects(invalid, "foreign asset root");

        expect(content().body.envelope.size() == 19 && content().motion.walk_speed == 184, "legacy body and gait defaults");
        expect(content().reel.steps.size() == 10 && content().reel.steps.back().at == .92, "legacy reel defaults");
        expect(content().motion.wall_hold == 1 && content().callouts.label_font == 18, "legacy filter and font defaults");
        expect(annotation(content().annotations.skeleton, 24) == "Skeleton, 24 bones", "annotation tokens");

        Host host;
        host.rows["style.guides"]["catalog"].push_back({{"id", "cinema"}, {"label", "Cinema"}, {"width", 3.0}, {"height", 1.0}});
        host.rows["style.callouts"]["label_font"] = 22;
        host.rows["style.bones"]["spring_groups"][0]["prefixes"] = {"custom_"};
        ContentLoader loader;
        drain(loader, host);
        expect(loader.problem().empty() && host.requests == rows.size() + 2, "one load, one list, one read per row");
        expect(guide_aspect(guide_from("cinema")) == 3 && guide_options().back().at("label") == "Cinema", "custom guide reaches renderer and menu");
        expect(callout_font == 22 && spring_group("custom_left") == "chest", "typed consumers use loaded data");
        {
            Host expiring; expiring.expire_at = 5;   // mid-read, as a collection during a world load does
            ContentLoader retry; cssx::Client c(&expiring.api);
            expect(!retry.advance(c) && !retry.done(), "an expired handle is read again later, not given up");
            expect(retry.advance(c) && retry.problem().empty(), "the second read completes");
        }
        reset_content();
        for (size_t failure = 1; failure <= rows.size() + 2; ++failure) {
            Host bad_host; bad_host.fail_at = failure;
            bad_host.rows["style.callouts"]["label_font"] = 99;
            ContentLoader bad_loader; drain(bad_loader, bad_host);
            expect(!bad_loader.problem().empty() && content().callouts.label_font == 18, "failure keeps defaults atomically");
        }
        for (int mode = 0; mode < 4; ++mode) {
            Host bad_host;
            bad_host.missing = mode == 0; bad_host.malformed = mode == 1; bad_host.wrong_type = mode == 2;
            if (mode == 3) bad_host.rows.erase("recipe.reel");
            ContentLoader bad_loader; drain(bad_loader, bad_host);
            expect(!bad_loader.problem().empty(), "missing/invalid table falls back");
        }
        auto core = default_content_rows();
        core["style.callouts"]["label_font"] = 20;
        const std::map<std::string, Json> first_rows{
            {"style.callouts", {{"schema", 1}, {"label_font", 24}}},
            {"recipe.defaults", {{"duration", 7}}},
            {"preset.example", {{"id", "ignored"}}}};
        const std::map<std::string, Json> second_rows{{"style.callouts", {{"label_font", 30}}}};
        auto first = resolve_content(core, first_rows), second = resolve_content(core, second_rows);
        expect(first.problems.empty() && first.value.callouts.label_font == 24 && first.value.defaults.duration == 7, "partial style and recipe override");
        expect(first.value.callouts.line_height == content().callouts.line_height, "omitted fields inherit core");
        expect(second.problems.empty() && second.value.callouts.label_font == 30 && second.value.defaults.duration == content().defaults.duration, "two packs resolve independently");
        expect(core["style.callouts"]["label_font"] == 20, "merging never mutates the shared core");
        auto full = resolve_content(core, {{"style.callouts", rows.at("style.callouts")}});
        expect(full.problems.empty() && full.value.callouts.label_font == 18, "full row override");
        for (const Json& bad_patch : {Json{{"label_font", -1}}, Json{{"label_font", "large"}}, Json{{"label_font", nullptr}}, Json{{"schema", 2}}, Json{{"typo", 1}}, Json::array()}) {
            const auto bad = resolve_content(core, {{"style.callouts", bad_patch}, {"recipe.defaults", {{"duration", 7}}}});
            expect(bad.problems.size() == 1 && bad.problems.front().starts_with("style.callouts"), "bad override reports its row");
            expect(bad.value.callouts.label_font == 20 && bad.value.defaults.duration == 7, "bad row falls back to core without dropping valid rows");
        }
        auto invalid_bounds = resolve_content(core, {{"style.geometry", {{"radius_min", 9}}}});
        expect(invalid_bounds.problems.size() == 1 && invalid_bounds.value.geometry.radius_min == content().geometry.radius_min, "override uses core relational validation");
        auto relocated = resolve_content(core, {{"style.materials", {{"root", "/Game/Other"}}}});
        expect(relocated.problems.size() == 1, "core ABI root cannot be relocated by a pack");
        auto unknown = resolve_content(core, {{"style.unknown", {{"schema", 1}}}});
        expect(unknown.problems.size() == 1, "unknown override row rejected");
        auto guide_row = rows.at("style.guides").at("catalog");
        guide_row.push_back({{"id", "pack-frame"}, {"label", "Pack frame"}, {"width", 4}, {"height", 1}});
        auto framed = resolve_content(core, {{"style.guides", {{"catalog", guide_row}}}});
        install_content(framed.value);
        expect(guide_aspect(guide_from("pack-frame")) == 4, "pack catalog replaces the whole list");
        install_content(first.value);
        expect(callout_font == 24 && guide_from("pack-frame") == Guide::Off, "switching packs loses the previous catalog");
        install_content(second.value);
        expect(callout_font == 30, "second pack binds its own typed values");
        install_content(parse_content(core));
        expect(callout_font == 20, "outside a take the loaded core style returns");
        reset_content();

        Hud hud;
        Guides guides; guides.attach(&hud.api, &hud, nullptr);
        Callouts labels; labels.attach(&hud.api, &hud, nullptr);
        CssxFrame frame{}; frame.world_generation = 1; frame.world_ready = 1; frame.viewport_w = 1920; frame.viewport_h = 1080;
        Overlay overlay; overlay.visible = true; overlay.guide = guide_from("16:9");
        const auto draw = [&] { guides.update(frame, overlay); labels.update(frame, {0, 0, 1920, 1080}, {}, "Caption", {"Name"}); };
        draw();
        const auto layer_count = hud.made, color_count = hud.colors, font_count = hud.fonts;
        install_content(first.value);
        guides.refresh_style(); labels.refresh_style(); draw();
        expect(hud.made == layer_count && hud.colors > color_count && hud.fonts > font_count, "style switch recolors and re-fonts existing layers without allocating");
        install_content(second.value);
        guides.refresh_style(); labels.refresh_style(); draw();
        expect(hud.made == layer_count, "a second pack does not leak HUD layers");
        reset_content();

        auto manifest = Json{{"pack_schema", 1}, {"pack", "Test"}, {"name", "Test"}, {"version", "1"}, {"presets", {"second", "first"}}};
        expect(parse_manifest(manifest, "Test").default_preset == "second", "manifest order supplies default");
        manifest["default_preset"] = "first";
        expect(parse_manifest(manifest, "Test").default_preset == "first", "manifest preference supplies default");
        manifest["default_preset"] = "absent";
        bool rejected = false; try { (void)parse_manifest(manifest, "Test"); } catch (...) { rejected = true; }
        expect(rejected, "default must belong to pack");

        const auto start = std::chrono::steady_clock::now();
        double sum = 0;
        for (size_t i = 0; i < 1000000; ++i) sum += content().motion.walk_speed;
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        expect(sum == 184000000, "typed read benchmark result");
        std::cout << checks << " core content checks passed; 1000000 typed reads: " << ns << " ns\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

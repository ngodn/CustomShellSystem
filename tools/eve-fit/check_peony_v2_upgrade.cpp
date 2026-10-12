#include "data.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace css;

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

Json read(const char* path) {
    std::ifstream input(path);
    return Json::parse(input);
}

const Json& variant(const Json& manifest, const std::string& id) {
    for (const auto& outfit : manifest.at("catalog").at("outfits"))
        if (outfit.at("id") == "eins0fx.seduxtress")
            for (const auto& value : outfit.at("variants"))
                if (value.at("id") == id) return value;
    throw std::runtime_error("Missing Eve variant: " + id);
}

const std::set<std::string> retired{
    "fabric_roughness", "garment_color", "metal_color", "trim_color", "trim_roughness"
};

Customization restore_profile(const ControlSet& model, const Customization& old) {
    State state;
    const std::string shell = "CharacterId.Player.Shell.Genessa";
    state.selections[shell] = {"eins0fx.seduxtress", "peony", old};
    state.remembered_custom["eins0fx.seduxtress/peony"] = old;
    state.walk_animation = "feminine";
    state.misc_rules["sidearm"] = {"in_use"};
    state.doubles = {true, false};
    save_profile_snapshot(state, "Peony.old");
    const auto encoded = state.json();
    const auto loaded = State::parse(encoded);
    require(loaded.json() == encoded, "State/profile serialization loses data");
    const auto& selection = loaded.presets.at("Peony.old").selections.at(shell);
    require(selection.outfit == "eins0fx.seduxtress" && selection.variant == "peony",
            "Profile loses the stable outfit/variant ID");
    const auto result = restore_customization(model, selection.custom.json());
    require(result == compatible_values(model, loaded.selections.at(shell).custom),
            "Profile and automatic selection restore disagree");
    require(result == compatible_values(model, loaded.remembered_custom.at("eins0fx.seduxtress/peony")),
            "Remembered customization and profile restore disagree");
    require(result.palette == "original", "Retired palette does not fall back to Original");
    require(result.ground_offset_cm == old.ground_offset_cm && result.expression == old.expression,
            "Upgrade loses placement or expression");
    for (const auto& [id, value] : old.values) {
        if (retired.contains(id)) require(!result.values.contains(id), "Retired control survives: " + id);
        else require(result.values.contains(id) && result.values.at(id) == value,
                     "Compatible setting is lost or changed: " + id);
    }
    require(result.values.size() == old.values.size() - retired.size(),
            "Upgrade introduces unexpected explicit overrides");
    require(result.tints == old.tints, "Upgrade changes group tints");
    control_values(model, result);
    require(restore_customization(model, result.json()) == result, "Second restore changes the look");
    return result;
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected old and new Eve package manifests");
        const auto old_manifest = read(argv[1]);
        const auto new_manifest = read(argv[2]);
        const auto old = ControlSet::parse(variant(old_manifest, "peony").at("customize"));
        const auto next = ControlSet::parse(variant(new_manifest, "peony").at("customize"));
        std::set<std::string> removed;
        for (const auto& control : old.controls)
            if (!next.find(control.id)) removed.insert(control.id);
            else require(next.find(control.id)->kind == control.kind, "Retained control changes kind: " + control.id);
        require(removed == retired, "Unexpected removed controls");
        require(old.palettes.size() == 5 && next.palettes.size() == 4, "Unexpected palette counts");
        for (const auto& palette : old.palettes)
            for (const auto& target : next.palettes)
                require(palette.id != target.id, "Old palette ID unexpectedly reused");
        for (const auto* id : {"black_pearl", "prototype", "skin_suit", "bikini", "casual_sweater", "midsummer_alice"})
            require(variant(old_manifest, id).at("customize") == variant(new_manifest, id).at("customize"),
                    std::string("Existing outfit customization changed: ") + id);

        Customization base;
        for (const auto& control : old.controls) base.values[control.id] = control.value;
        base.ground_offset_cm = 1.25;
        base.expression = "smile";
        base.tints["body"] = {15.f, .9f, .8f};
        base.tints["outfit"] = {-20.f, .7f, .9f};
        unsigned saved_cases = 0;
        std::vector<std::string> palettes{"original"};
        for (const auto& palette : old.palettes) palettes.push_back(palette.id);
        for (const auto& palette : palettes) {
            auto saved = base;
            saved.palette = palette;
            restore_profile(next, saved);
            ++saved_cases;
            for (const auto& control : old.controls) {
                if (retired.contains(control.id)) continue;
                for (int channel = 0; channel < control_channel_count(control); ++channel) {
                    const auto range = control_channel(control, channel);
                    for (const auto endpoint : {range.minimum, range.maximum}) {
                        auto boundary = saved;
                        boundary.values[control.id][channel] = endpoint;
                        control_values(old, boundary);
                        restore_profile(next, boundary);
                        ++saved_cases;
                    }
                }
            }
        }
        // Expected sections come from the V2 cooked mesh's material order.
        const std::map<std::string, std::set<int>> pieces{
            {"toggle_jacket", {26,33,34}}, {"toggle_corset", {28,32,35,36,37}},
            {"toggle_choker", {27}}, {"toggle_gloves", {20,30}},
            {"toggle_underwear", {24}}, {"toggle_leg_panels", {22}},
            {"toggle_boots", {21,29,31}}, {"toggle_stockings", {41,42}},
        };
        unsigned visibility_cases = 0;
        for (unsigned mask = 0; mask < (1u << pieces.size()); ++mask) {
            auto saved = base;
            std::set<int> expected{43,44,45};
            unsigned bit = 0;
            for (const auto& [id, sections] : pieces) {
                const bool shown = (mask & (1u << bit++)) != 0;
                saved.values[id] = {shown ? 1.f : 0.f, 0, 0, 1};
                if (!shown) expected.insert(sections.begin(), sections.end());
                if (id == "toggle_boots" && shown) expected.insert({38,39,40,42});
            }
            const auto restored = restore_profile(next, saved);
            require(hidden_control_sections(next, control_values(next, restored)) == expected,
                    "Old visibility settings use incorrect V2 sections");
            ++visibility_cases;
        }
        std::cout << Json{{"passed",true}, {"saved_cases",saved_cases},
                          {"visibility_cases",visibility_cases}, {"retained_controls",old.controls.size()-retired.size()},
                          {"retired_controls",retired}, {"unchanged_outfits",6},
                          {"live_acceptance",false}}.dump(2) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

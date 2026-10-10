#include "controls.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace css;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected wardrobe-customize.json");
        std::ifstream input(argv[1]);
        require(input.good(), "Cannot open wardrobe recipe");
        auto recipe = nlohmann::json::parse(input);
        const auto model = ControlSet::parse(recipe);
        require(model.controls.size() == 22 && model.palettes.size() == 5,
                "Expected 22 wardrobe controls and five palettes");
        // Independent expectations from the final slot and covered-foot audit.
        const std::map<std::string, std::set<int>> pieces{
            {"toggle_jacket", {20}}, {"toggle_corset", {21}},
            {"toggle_choker", {22}}, {"toggle_gloves", {23}},
            {"toggle_arm_ornaments", {24, 31}}, {"toggle_hip_accessories", {25, 29}},
            {"toggle_underwear", {32}}, {"toggle_leg_panels", {34, 38}},
            {"toggle_stockings", {35}}, {"toggle_boots", {26, 28, 33, 36}},
        };
        const std::set<int> covered_feet{18, 19, 37};
        require(hidden_control_sections(model, control_values(model, {})) == covered_feet,
                "Original must show boots and hide only their covered skin");
        unsigned combinations = 0;
        for (unsigned mask = 0; mask < (1u << pieces.size()); ++mask) {
            Customization user;
            std::set<int> expected;
            unsigned bit = 0;
            for (const auto& [id, sections] : pieces) {
                const bool shown = (mask & (1u << bit++)) != 0;
                user = apply_customize(model, user, {{"action", "control"},
                    {"control", id}, {"channel", 0}, {"value", shown ? 1 : 0}});
                if (!shown) expected.insert(sections.begin(), sections.end());
                if (id == "toggle_boots" && shown)
                    expected.insert(covered_feet.begin(), covered_feet.end());
            }
            require(hidden_control_sections(model, control_values(model, user)) == expected,
                    "Visibility combination hides an unrelated part or leaves covered feet visible");
            require(Customization::parse(user.json()) == user, "Visibility did not persist");
            for (const auto& palette : model.palettes) {
                auto styled = choose_palette(model, user, palette.id);
                require(hidden_control_sections(model, control_values(model, styled)) == expected,
                        "Palette changes garment visibility");
            }
            ++combinations;
        }
        // Palette application must also preserve controls supplied by the later body merge.
        recipe["controls"].push_back({{"id", "shape_PBMBreastsSize"}, {"name", "Chest size"},
            {"kind", "shape"}, {"group", "body"}, {"morph", "PBMBreastsSize"},
            {"default", {0, 0, 0, 1}}});
        const auto combined = ControlSet::parse(recipe);
        Customization user;
        user.values["shape_PBMBreastsSize"] = {.63f, 0, 0, 1};
        user.values["stocking_opacity"] = {.42f, 0, 0, 1};
        user.values["trim_glow"] = {3.2f, 0, 0, 1};
        user.values["flow_speed"] = {.7f, 0, 0, 1};
        user.ground_offset_cm = 1.25;
        for (const auto& palette : combined.palettes) {
            const auto styled = choose_palette(combined, user, palette.id);
            const auto values = control_values(combined, styled);
            for (const auto& [id, value] : user.values)
                require(values.at(id) == value, "Palette overwrites shape, opacity or glow settings");
            for (const auto& [id, value] : palette.values)
                require(values.at(id) == value, "Palette color differs from authored sRGB value");
            require(styled.ground_offset_cm == user.ground_offset_cm, "Palette changes placement");
            require(restore_customization(combined, styled.json()) == styled,
                    "Saved palette and custom settings did not restore");
        }
        std::cout << "{\"passed\":true,\"visibility_combinations\":" << combinations
                  << ",\"palette_visibility_checks\":" << combinations * model.palettes.size()
                  << ",\"palette_profile_checks\":5}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

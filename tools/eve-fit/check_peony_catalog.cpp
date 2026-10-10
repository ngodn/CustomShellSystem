#include "data.hpp"
#include "ground_offset.hpp"
#include <cmath>
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
        require(argc == 2, "Expected a directory containing the candidate .css.json catalog");
        const auto catalog = Catalog::load(argv[1]);
        require(catalog.outfits.size() == 1, "Expected only the Eve outfit");
        const auto& outfit = catalog.outfits.front();
        require(outfit.id == "eins0fx.seduxtress" && outfit.variants.size() == 7,
                "Expected the six retained variants and Peony");
        const auto* variant = catalog.find(outfit.id, "peony");
        require(variant && variant->controls && variant->ground_offset_toggle,
                "Peony controls or footwear rule missing");
        const auto& controls = *variant->controls;
        require(controls.controls.size() == 40 && controls.surfaces.size() == 9 && controls.palettes.size() == 5,
                "Merged controls, dye surfaces or palettes were lost");
        const auto* donor = catalog.find(outfit.id, "skin_suit");
        require(donor && donor->controls, "Skin Suit donor missing");
        Customization body;
        unsigned inherited = 0;
        for (const auto& control : donor->controls->controls) {
            if (control.group != ControlGroup::Body) continue;
            const auto* transferred = controls.find(control.id);
            require(transferred && transferred->kind == control.kind, "A body control changed kind or disappeared");
            auto value = control.value;
            if (control.kind == ControlKind::Shape) value[0] = .65f;
            if (control.kind == ControlKind::Color) value = {.4f, .5f, .6f, 1};
            body.values[control.id] = value;
            ++inherited;
        }
        require(inherited == 17, "Expected all six shapes, six colors and five physics controls");
        require(compatible_values(controls, body).values == body.values,
                "Compatible body customization was not retained");
        const std::map<std::string, std::set<int>> switches{
            {"toggle_jacket", {20}}, {"toggle_corset", {21}}, {"toggle_choker", {22}},
            {"toggle_gloves", {23}}, {"toggle_arm_ornaments", {24, 31}},
            {"toggle_hip_accessories", {25, 29}}, {"toggle_underwear", {32}},
            {"toggle_leg_panels", {34, 38}}, {"toggle_stockings", {35}},
            {"toggle_boots", {26, 28, 33, 36}}, {"hair", {15, 16, 17}},
        };
        unsigned cases = 0;
        for (unsigned mask = 0; mask < (1u << switches.size()); ++mask) {
            auto user = body;
            std::set<int> hidden;
            bool boots = false;
            unsigned bit = 0;
            for (const auto& [id, sections] : switches) {
                const bool shown = (mask & (1u << bit++)) != 0;
                user = apply_customize(controls, user, {{"action", "control"},
                    {"control", id}, {"channel", 0}, {"value", shown ? 1 : 0}});
                if (!shown) hidden.insert(sections.begin(), sections.end());
                if (id == "toggle_boots") boots = shown;
            }
            if (boots) hidden.insert({18, 19, 37});
            const double expected_height = boots ? 9.769743204116821 : 0;
            require(hidden_control_sections(controls, control_values(controls, user)) == hidden,
                    "Merged visibility hides the wrong sections");
            require(authored_ground_offset(outfit, *variant, user) == expected_height,
                    "Boot visibility and authored height disagree");
            for (const auto& palette : controls.palettes) {
                const auto styled = choose_palette(controls, user, palette.id);
                const auto restored = restore_customization(controls, styled.json());
                require(restored == styled, "Merged profile failed to restore");
                const auto values = control_values(controls, restored);
                require(hidden_control_sections(controls, values) == hidden, "Palette changes visibility");
                for (const auto& [id, value] : body.values)
                    require(values.at(id) == value, "Palette changes inherited body settings");
                require(authored_ground_offset(outfit, *variant, restored) == expected_height,
                        "Palette or profile changes automatic footwear height");
            }
            ++cases;
        }
        Customization look;
        look.ground_offset_cm = -2;
        require(look.ground_offset_cm.value_or(authored_ground_offset(outfit, *variant, look)) == -2,
                "Manual ground height did not keep precedence");
        look.ground_offset_cm.reset();
        GroundOffset placement;
        const auto lifted = placement.set(-96, authored_ground_offset(outfit, *variant, look));
        look.values["toggle_boots"] = {0, 0, 0, 1};
        require(placement.set(lifted, authored_ground_offset(outfit, *variant, look)) == -96,
                "Removing boots did not restore the baseline height");
        std::cout << "{\"passed\":true,\"visibility_height_cases\":" << cases
                  << ",\"palette_profile_cases\":" << cases * controls.palettes.size()
                  << ",\"inherited_body_controls\":" << inherited << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

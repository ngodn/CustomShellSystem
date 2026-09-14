#include "data.hpp"
#include <iostream>
#include <stdexcept>
#include <functional>

using namespace css;
static unsigned checks;
static void expect(bool value, const char* label) { ++checks; if (!value) throw std::runtime_error(label); }
static void rejects(const std::function<void()>& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    expect(rejected, "Invalid data was accepted");
}
int main() {
    auto dir = fs::temp_directory_path() / "css-data-tests";
    fs::create_directories(dir);
    try {
        expect(valid_id("beaute.genessa"), "Valid id rejected");
        expect(!valid_id("../escape"), "Traversal accepted");
        expect(!valid_id(".."), "Dot traversal accepted");
        expect(valid_asset("/Game/CSS/Mesh.Mesh"), "Valid asset rejected");
        expect(!valid_asset("/Game/CSS/../Mesh.Mesh"), "Asset traversal accepted");
        State state;
        expect(!state.invert_orbit_x && state.invert_orbit_y, "Default orbit must invert vertical only");
        auto legacy=state.json(); legacy.erase("invert_orbit_x"); legacy.erase("invert_orbit_y");
        expect(!State::parse(legacy).invert_orbit_x && State::parse(legacy).invert_orbit_y, "Old saves lost vertical inversion default");
        state.invert_orbit_x=true; state.invert_orbit_y=false;
        auto settings=State::parse(state.json());
        expect(settings.invert_orbit_x && !settings.invert_orbit_y, "Orbit preferences did not persist");
        state.enabled = true;
        state.selections["CharacterId.Player.Shell.Genessa"] = {"beaute.genessa", "regular"};
        state.favorites.insert("beaute.genessa");
        state.presets["my-outfit"] = state.selections;
        expect(State::parse(state.json()).json() == state.json(), "State round trip failed");
        auto invalid = state.json(); invalid["schema"] = 2;
        rejects([&] { State::parse(invalid); });
        invalid = state.json(); invalid["enabled"] = "true";
        rejects([&] { State::parse(invalid); });
        invalid = state.json(); invalid["selections"]["../bad"] = {{"outfit", "x"}, {"variant", "x"}};
        rejects([&] { State::parse(invalid); });
        auto file = dir / "state.json";
        atomic_json(file, state.json());
        auto original = state.json(); state.enabled = false;
        atomic_json(file, state.json());
        expect(read_json(file) == state.json(), "New state not persisted");
        expect(read_json(file.string() + ".bak") == original, "Backup not preserved");
        auto catalog_dir = dir / "catalog";
        fs::create_directories(catalog_dir);
        Json item = {{"id", "test"}, {"name", "Test"}, {"shells", {"CharacterId.Player.Shell.Genessa"}},
                     {"variants", {{{"id", "a"}, {"name", "A"}, {"mesh", "/Game/CSS/Mesh.Mesh"}}}}};
        Json catalog = {{"schema", 1}, {"outfits", {item}}};
        auto catalog_file = catalog_dir / "test.css.json";
        atomic_json(catalog_file, catalog, false);
        auto loaded = Catalog::load(catalog_dir);
        expect(loaded.find("test", "a") != nullptr, "Variant missing");
        expect(loaded.find("test", "missing") == nullptr, "Missing variant accepted");
        auto material_catalog=catalog;
        material_catalog["outfits"][0]["variants"][0]["materials"]={{"0","/Game/CSS/Face.Face"},{"2","/Game/CSS/Hair.Hair"}};
        atomic_json(catalog_file,material_catalog,false);
        auto with_materials=Catalog::load(catalog_dir);
        expect(with_materials.find("test","a")->materials.at(2)=="/Game/CSS/Hair.Hair","Package material recipe lost");
        for(const auto& bad: {"-1","01","128","0x2"}) {
            auto bad_materials=material_catalog;
            bad_materials["outfits"][0]["variants"][0]["materials"]={{bad,"/Game/CSS/Face.Face"}};
            atomic_json(catalog_file,bad_materials,false);
            rejects([&]{Catalog::load(catalog_dir);});
        }
        atomic_json(catalog_file,catalog,false);
        expect(loaded.compatible("test", "CharacterId.Player.Shell.Genessa"), "Compatible shell rejected");
        expect(!loaded.compatible("test", "CharacterId.Player.Shell.KnightLady"), "Wrong shell accepted");
        catalog["outfits"][0]["compatibility"] = "same_skeleton";
        atomic_json(catalog_file, catalog, false);
        loaded = Catalog::load(catalog_dir);
        expect(loaded.compatible("test", "CharacterId.Player.Shell.KnightLady"), "Cross-shell appearance rejected");
        expect(loaded.compatible("test", "CharacterId.Player.Darkform.CorruptedGenessa"), "Dark form appearance rejected");
        expect(!loaded.compatible("test", "CharacterId.NPC.Genessa"), "Non-player accepted");
        expect(!loaded.compatible("test", ""), "Title screen accepted");
        catalog["outfits"][0]["compatibility"] = "anything";
        atomic_json(catalog_file, catalog, false);
        rejects([&] { Catalog::load(catalog_dir); });
        catalog["outfits"] = {item};
        catalog["outfits"].push_back(item); atomic_json(catalog_file, catalog, false);
        rejects([&] { Catalog::load(catalog_dir); });
        catalog["outfits"] = {item}; catalog["outfits"][0]["variants"][0]["mesh"] = "/Script/Engine.Object";
        atomic_json(catalog_file, catalog, false); rejects([&] { Catalog::load(catalog_dir); });
        fs::remove_all(dir);
        std::cout << checks << " behavioral checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

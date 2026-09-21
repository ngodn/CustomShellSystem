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
        {
            Catalog list;
            for(const auto* id:{"ordinary.a","favorite.a","equipped","ordinary.b","favorite.b"}) {
                Outfit outfit;outfit.id=id;list.outfits.push_back(std::move(outfit));
            }
            auto ids=[](const auto& order) {
                std::vector<std::string> result;for(auto* outfit:order) result.push_back(outfit->id);return result;
            };
            expect(ids(list.display_order("equipped",{"favorite.a","favorite.b","equipped","missing"}))==
                std::vector<std::string>{"equipped","favorite.a","favorite.b","ordinary.a","ordinary.b"},
                "Equipped/favorite groups lost priority, stability or uniqueness");
            expect(ids(list.display_order("missing",{"favorite.b"}))==
                std::vector<std::string>{"favorite.b","ordinary.a","favorite.a","equipped","ordinary.b"},
                "Missing equipped outfit or changed favorites broke order");
            expect(ids(list.display_order("",{}))==
                std::vector<std::string>{"ordinary.a","favorite.a","equipped","ordinary.b","favorite.b"},
                "Unranked catalog order changed");
            expect(Catalog{}.display_order("",{}).empty(),"Empty catalog gained an outfit");
        }
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
        state.harbinger_mirror=false;
        auto settings=State::parse(state.json());
        expect(settings.invert_orbit_x && !settings.invert_orbit_y, "Orbit preferences did not persist");
        expect(!settings.harbinger_mirror, "Harbinger preference lost during customization migration");
        state.enabled = true;
        state.selections["CharacterId.Player.Shell.Genessa"] = {"beaute.genessa", "regular"};
        state.favorites.insert("beaute.genessa");
        state.presets["my-outfit"] = Preset{state.selections, "feminine"};
        expect(State::parse(state.json()).json() == state.json(), "State round trip failed");
        state.walk_animation = "feminine";
        {
            const auto back = State::parse(state.json());
            expect(back.walk_animation == "feminine", "Walk animation setting lost");
            expect(back.presets.at("my-outfit").walk_animation == "feminine", "Template walk setting lost");
        }
        {   // The 0.3.3 preview offered jogging and sprinting. Both are gone: a state
            // written by it still loads, and neither setting comes back.
            auto preview = state.json();
            preview["run_animation"] = "feminine";
            preview["jog_animation"] = "feminine"; preview["sprint_animation"] = "feminine";
            preview["presets"]["my-outfit"]["sprint_animation"] = "feminine";
            const auto back = State::parse(preview);
            expect(back.walk_animation == "feminine", "Preview state did not load");
            expect(!back.json().contains("jog_animation") && !back.json().contains("sprint_animation"),
                   "Jog and sprint settings were written back");
        }
        auto old_template = state.json(); old_template["presets"]["my-outfit"] = old_template["presets"]["my-outfit"]["selections"]; old_template.erase("walk_animation");
        auto migrated = State::parse(old_template);
        expect(migrated.walk_animation == "normal" && migrated.presets.at("my-outfit").walk_animation == "normal" && migrated.presets.at("my-outfit").selections.size() == 1, "Legacy template migration failed");
        auto bad = state.json(); bad["walk_animation"] = "sideways";
        rejects([&] { State::parse(bad); });
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
        {   // 1.0: a variant is a list of items. A package written before this says `mesh`
            // and reads as one body item, which is what every published package does.
            const auto* plain=with_materials.find("test","a");
            expect(plain->items.size()==1 && plain->items[0].slot==ItemSlot::Body,
                   "A variant with a mesh must read as one body item");
            expect(plain->items[0].mesh==plain->mesh && plain->items[0].materials==plain->materials,
                   "The implied body item must carry the variant's mesh and materials");
            expect(std::string(item_slot_name(ItemSlot::Neck))=="neck","Slot name missing");
            ItemSlot parsed{};
            expect(item_slot_from_name("trinket4",parsed) && parsed==ItemSlot::Trinket4,"Slot lookup failed");
            expect(!item_slot_from_name("elbow",parsed),"Unknown slot accepted");

            // Built field by field: nested brace lists are ambiguous between an object
            // and an array in nlohmann, and a materials map read as an array would make
            // this test lie about what the parser accepts.
            Json body_item=Json::object();
            body_item["id"]="body"; body_item["name"]="Body"; body_item["slot"]="body";
            body_item["mesh"]="/Game/CSS/Body.Body";
            body_item["materials"]=Json::object(); body_item["materials"]["0"]="/Game/CSS/Skin.Skin";
            Json collar=Json::object();
            collar["id"]="collar"; collar["name"]="Collar"; collar["slot"]="neck"; collar["order"]=20;
            collar["mesh"]="/Game/CSS/Collar.Collar";
            collar["hides"]=Json::object(); collar["hides"]["sections"]=Json::array({3,4});
            Json items=Json::array({body_item,collar});
            auto layered=catalog;
            layered["outfits"][0]["variants"][0].erase("mesh");
            layered["outfits"][0]["variants"][0]["items"]=items;
            atomic_json(catalog_file,layered,false);
            // Hold the catalog: find() returns a pointer into it, and a temporary would
            // be gone by the semicolon.
            const auto loaded_items=Catalog::load(catalog_dir);
            const auto* worn=loaded_items.find("test","a");
            expect(worn->items.size()==2,"Both items lost");
            expect(worn->mesh=="/Game/CSS/Body.Body" && worn->materials.at(0)=="/Game/CSS/Skin.Skin",
                   "The body item must be mirrored onto the variant");
            expect(worn->items[1].slot==ItemSlot::Neck && worn->items[1].order==20,"Accessory slot or order lost");
            expect(worn->items[1].hides_sections==std::vector<int>{3,4},"Hidden sections lost");

            // A variant says where its body is once, and has exactly one.
            auto both=layered; both["outfits"][0]["variants"][0]["mesh"]="/Game/CSS/Mesh.Mesh";
            atomic_json(catalog_file,both,false); rejects([&]{Catalog::load(catalog_dir);});
            auto bodiless=layered; bodiless["outfits"][0]["variants"][0]["items"][0]["slot"]="chest";
            atomic_json(catalog_file,bodiless,false); rejects([&]{Catalog::load(catalog_dir);});
            auto twice=layered; twice["outfits"][0]["variants"][0]["items"][1]["slot"]="body";
            atomic_json(catalog_file,twice,false); rejects([&]{Catalog::load(catalog_dir);});
            auto same_slot=layered;
            same_slot["outfits"][0]["variants"][0]["items"][1]["slot"]="body";
            same_slot["outfits"][0]["variants"][0]["items"][0]["slot"]="body";
            atomic_json(catalog_file,same_slot,false); rejects([&]{Catalog::load(catalog_dir);});
            auto duplicate_id=layered; duplicate_id["outfits"][0]["variants"][0]["items"][1]["id"]="body";
            atomic_json(catalog_file,duplicate_id,false); rejects([&]{Catalog::load(catalog_dir);});
            auto unknown=layered; unknown["outfits"][0]["variants"][0]["items"][1]["slot"]="elbow";
            atomic_json(catalog_file,unknown,false); rejects([&]{Catalog::load(catalog_dir);});
            auto far=layered; far["outfits"][0]["variants"][0]["items"][1]["order"]=1000;
            atomic_json(catalog_file,far,false); rejects([&]{Catalog::load(catalog_dir);});
            auto bad_section=layered; bad_section["outfits"][0]["variants"][0]["items"][1]["hides"]["sections"]={128};
            atomic_json(catalog_file,bad_section,false); rejects([&]{Catalog::load(catalog_dir);});
            auto empty=layered; empty["outfits"][0]["variants"][0]["items"]=Json::array();
            atomic_json(catalog_file,empty,false); rejects([&]{Catalog::load(catalog_dir);});
            atomic_json(catalog_file,material_catalog,false);
        }
        {   // 1.0.0-beta: Templates (combinations, palettes, archetypes, etc.)
            auto templated = catalog;
            templated["outfits"][0]["templates"] = {
                {"combinations", {{{"id", "harness_style"}, {"name", "Harness Set"}}}},
                {"palettes", {{{"id", "crimson_vow"}, {"name", "Crimson Vow"}}}},
                {"archetypes", {{{"id", "seductress_petite"}, {"name", "Petite Seductress"}}}},
                {"physics", {{{"id", "jiggle_soft"}, {"name", "Soft Tissue"}}}},
                {"accessories", {{{"id", "choker_set"}, {"name", "Gothic Choker"}}}},
                {"fabrics", {{{"id", "sheer_gown"}, {"name", "Sheer Silk"}}}},
                {"anatomy", {{{"id", "sensual_curves"}, {"name", "Hourglass"}}}},
            };
            atomic_json(catalog_file, templated, false);
            auto with_templates = Catalog::load(catalog_dir);
            expect(with_templates.outfits[0].templates.size() == 7, "Templates parsing failed");
            expect(with_templates.outfits[0].templates[0].kind == TemplateKind::Combination, "Combination template kind wrong");
            expect(with_templates.outfits[0].templates[1].kind == TemplateKind::Palette, "Palette template kind wrong");
            expect(with_templates.outfits[0].templates[2].kind == TemplateKind::Archetype, "Archetype template kind wrong");
            expect(with_templates.outfits[0].templates[3].kind == TemplateKind::Physics, "Physics template kind wrong");
            expect(with_templates.outfits[0].templates[4].kind == TemplateKind::Accessory, "Accessory template kind wrong");
            expect(with_templates.outfits[0].templates[5].kind == TemplateKind::Fabric, "Fabric template kind wrong");
            expect(with_templates.outfits[0].templates[6].kind == TemplateKind::Anatomy, "Anatomy template kind wrong");
            atomic_json(catalog_file, catalog, false);
        }
        {   // 0.4: the correction that keeps a stowed seal out of the hips. CSS measures
            // the seal against the body's live physics asset and holds it `clearance` off;
            // `location` is only the fallback for a mesh with no collision to measure.
            Json collision = {{"clearance", 3.0}, {"max_push", 8.0}, {"anchor", "pelvis"},
                              {"direction", {0.29066, -0.82883, 0.47807}}};
            auto seals = catalog;
            seals["outfits"][0]["variants"][0]["attachments"] =
                {{"Socket_Prop_Stowed_InfiniteSeal_Right", {{"location", {0.2746, 0.369, 2.9645}}, {"rotation", {0, 0, 0}}, {"collision", collision}}}};
            atomic_json(catalog_file, seals, false);
            const auto with_seals = Catalog::load(catalog_dir);
            const auto& offset = with_seals.find("test", "a")->attachments.at("Socket_Prop_Stowed_InfiniteSeal_Right");
            expect(offset.location[2] == 2.9645, "Attachment offset lost");
            expect(offset.collision.active() && offset.collision.anchor == "pelvis", "Attachment collision lost");
            expect(offset.collision.clearance == 3.0 && offset.collision.max_push == 8.0, "Collision limits lost");

            auto reject_with = [&](auto mutate) {
                auto bad = seals; mutate(bad["outfits"][0]["variants"][0]["attachments"]["Socket_Prop_Stowed_InfiniteSeal_Right"]["collision"]);
                atomic_json(catalog_file, bad, false);
                rejects([&] { Catalog::load(catalog_dir); });
            };
            reject_with([](Json& c) { c["direction"] = {1, 1, 0}; });    // not a unit vector
            reject_with([](Json& c) { c["clearance"] = 500; });          // absurd hold-off
            reject_with([](Json& c) { c["max_push"] = 0; });             // no room to correct
            reject_with([](Json& c) { c["max_push"] = 500; });           // absurd correction

            // A block written for an older CSS, or one with nothing to hold, is ignored
            // rather than rejecting the package: the fixed offset still applies.
            auto legacy = seals;
            legacy["outfits"][0]["variants"][0]["attachments"]["Socket_Prop_Stowed_InfiniteSeal_Right"]["collision"] =
                {{"anchor", "pelvis"}, {"max_push", 7.0}, {"direction", {0.29066, -0.82883, 0.47807}},
                 {"probes", {{{"clearance", 2.4}, {"drivers", {{{"bone", "pelvis"}, {"weight", 1.0}, {"point", {0, 0, 0}}}}}}}}};
            atomic_json(catalog_file, legacy, false);
            const auto older = Catalog::load(catalog_dir);
            const auto& ignored = older.find("test", "a")->attachments.at("Socket_Prop_Stowed_InfiniteSeal_Right");
            expect(!ignored.collision.active(), "A collision block with no clearance should be ignored");
            expect(ignored.location[2] == 2.9645, "The fixed offset should survive an ignored collision block");
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

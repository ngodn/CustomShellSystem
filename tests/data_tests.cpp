#include "data.hpp"
#include "file_writer.hpp"
#include <chrono>
#include <fstream>
#include <thread>
#include "ground_offset.hpp"
#include <iostream>
#include <stdexcept>
#include <functional>
#include <limits>
#include <chrono>
#include <fstream>
#include <thread>

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
            GroundOffset height;
            expect(height.set(-96,-3)==-99,"Ground correction has wrong sign");
            expect(height.set(-99,-3)==-99,"Repeated application accumulated height");
            expect(height.set(-99,-2)==-98,"Variant change used the already offset height");
            expect(height.set(-96,-3)==-99,"Native baseline reset was not recovered");
            expect(height.restore(-99)==-96,"Original world height was not restored");
            expect(!height.restore(-96),"Restore was not idempotent");
            expect(height.set(-90,-3)==-93,"New pawn retained old height");
            rejects([&]{height.set(-92,-3);});
            expect(!height.restore(-92),"Cleanup overwrote an external height change");
            rejects([&]{height.set(-96,11);});
            rejects([&]{height.set(-96,std::numeric_limits<double>::quiet_NaN());});
            // A fresh core finds the mesh where the last one left it: anchored to the authored
            // height, the offset is recognised instead of stacked, and restore returns to it.
            // A shell item's own MISC rule survives a save; junk keys and extra items are dropped.
            // The loader's writer: writes land in order, the last one wins, .bak keeps the
            // previous content when asked, and a missing directory is created.
            const auto dir=std::filesystem::temp_directory_path()/("css-writer-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            const auto file=dir/"nested"/"state.json";
            auto contents=[&](const std::filesystem::path& p){ std::ifstream in(p,std::ios::binary); return std::string((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()); };
            { FileWriter writer; writer.post(file,"one",true); }   // destruction drains the queue
            expect(contents(file)=="one","Writer did not create the directory and file");
            {
                FileWriter writer;
                writer.post(file,"two",true);
                writer.post(file,"three",true);   // supersedes "two": one write, not two
            }
            expect(contents(file)=="three","Writer lost the last write");
            expect(contents(dir/"nested"/"state.json.bak")=="one","Backup is not the content the write replaced");
            expect(!std::filesystem::exists(dir/"nested"/"state.json.tmp"),"Writer left its temp file");
            std::filesystem::remove_all(dir);
            State severed; severed.last_living_shell="CharacterId.Player.Shell.Genessa";
            expect(State::parse(severed.json()).last_living_shell=="CharacterId.Player.Shell.Genessa","Last living shell not saved");
            auto odd=severed.json(); odd["last_living_shell"]="CharacterId.Player.Darkform.StrongOne";
            expect(State::parse(odd).last_living_shell.empty(),"A Darkform tag must not pass as the last living shell");
            State with_item;
            with_item.misc_rules["item:wp_alienheart"]=MiscRule{"hidden"};
            with_item.misc_rules["seal"]=MiscRule{"in_use"};
            auto back=State::parse(with_item.json());
            expect(back.misc_rules.at("item:wp_alienheart").mode=="hidden" && back.misc_rules.at("seal").mode=="in_use","Shell item rule not saved");
            auto junk=with_item.json(); junk["misc_rules"]["item:Bad Key!"]={{"mode","hidden"}}; junk["misc_rules"]["hat"]={{"mode","hidden"}};
            expect(State::parse(junk).misc_rules.size()==2,"Unknown MISC rule keys were kept");
            expect(misc_item_rule_key("item:wp_x") && !misc_item_rule_key("item:") && !misc_item_rule_key("wp_x"),"Item rule key check wrong");
            auto shown=with_item.json(); shown["misc_rules"]["item:wp_alienheart"]={{"mode","shown"}}; shown["misc_rules"]["seal"]={{"mode","shown"}};
            auto kept=State::parse(shown);
            expect(kept.misc_rules.at("item:wp_alienheart").mode=="shown" && kept.misc_rules.at("seal").mode=="default","Always Shown must be an item-only mode");
            GroundOffset resumed;
            expect(resumed.set(-99,-3,-96)==-99,"Resumed offset stacked on itself");
            expect(resumed.restore(-99)==-96,"Resumed offset did not restore the authored height");
            GroundOffset fresh;
            expect(fresh.set(-96,-3,-96)==-99,"Authored baseline changed the correction");
            GroundOffset foreign;
            expect(foreign.set(-90,-3,-96)==-93,"A height another owner set was not respected");
            expect(foreign.restore(-93)==-90,"Restore did not return the other owner's height");
        }
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
            Outfit originals;originals.id=original_shells_id;originals.same_skeleton=true;
            Variant official;official.id="tiel";official.name="Tiel";official.mesh="/Game/Stock/Tiel.Tiel";
            originals.variants.push_back(official);list.outfits.push_back(originals);
            expect(list.find(original_shells_id,"tiel")!=nullptr,"Official shell is not selectable");
            expect(list.compatible(original_shells_id,"CharacterId.Player.Shell.Genessa"),"Official appearance cannot overlay another shell");
            expect(!list.compatible(original_shells_id,"CharacterId.Enemy.Tiel"),"Official appearance accepted an enemy");
            expect(ids(list.display_order(original_shells_id,{original_shells_id,"favorite.b"}))==
                std::vector<std::string>{"favorite.b","ordinary.a","favorite.a","equipped","ordinary.b"},
                "Official shell duplicated its dedicated row or changed favorite ordering");
            State stock_state;stock_state.selections["CharacterId.Player.Shell.Genessa"]={original_shells_id,"tiel"};
            stock_state.presets["stock-look"]={stock_state.selections,"normal"};
            expect(State::parse(stock_state.json()).json()==stock_state.json(),"Official visual selection lost its gameplay-shell key or profile");
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
        // An invalid saved selection is dropped on its own; the rest of the state still loads.
        invalid = state.json(); invalid["selections"]["../bad"] = {{"outfit", "x"}, {"variant", "x"}};
        auto dropped = State::parse(invalid);
        expect(!dropped.selections.contains("../bad") && dropped.selections.size() == state.selections.size(), "Invalid selection key was kept");
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
        expect(loaded.find("test","a")->ground_offset_cm==0,"Legacy package acquired a ground offset");
        {
            auto spoof=catalog;spoof["outfits"][0]["id"]=original_shells_id;
            atomic_json(catalog_file,spoof,false);
            rejects([&]{Catalog::load(catalog_dir);});
            atomic_json(catalog_file,catalog,false);
        }
        {
            auto grounded=catalog;
            grounded["outfits"][0]["variants"][0]["ground_offset_cm"]=-3;
            atomic_json(catalog_file,grounded,false);
            expect(Catalog::load(catalog_dir).find("test","a")->ground_offset_cm==-3,"Ground offset was lost");
            for(const Json& value:Json::array({-11,11,"-3",nullptr,true})) {
                grounded["outfits"][0]["variants"][0]["ground_offset_cm"]=value;
                atomic_json(catalog_file,grounded,false);
                rejects([&]{Catalog::load(catalog_dir);});
            }
        }
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
        {
            // runtime/*.json: created with its folder, replaced in place, no temp file left behind.
            auto runtime = dir / "runtime" / "status.json";
            write_runtime_json(runtime, {{"a", 1}});
            write_runtime_json(runtime, {{"a", 2}});
            expect(read_json(runtime).at("a") == 2 && !fs::exists(runtime.string() + ".tmp"), "Runtime JSON was not replaced");
            write_runtime_json(runtime, {{"text", std::string("bad \xff byte")}});
            expect(read_json(runtime).contains("text"), "Runtime JSON rejected invalid UTF-8 instead of replacing it");
            // A damaged state file is archived, but only the newest three archives are kept.
            auto states = dir / "damaged"; fs::create_directories(states);
            auto primary = states / "state.json";
            atomic_json(primary, State{}.json()); atomic_json(primary, State{}.json());   // second write leaves a .bak
            for (int i = 0; i < 5; ++i) {
                { std::ofstream(primary, std::ios::trunc) << "broken"; }
                load_state(primary);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            size_t archived = 0;
            for (const auto& entry : fs::directory_iterator(states))
                archived += entry.path().filename().string().starts_with("state.json.corrupt-");
            expect(archived == 3, "Damaged state archives were not pruned to three");
        }
        fs::remove_all(dir);
        std::cout << checks << " behavioral checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

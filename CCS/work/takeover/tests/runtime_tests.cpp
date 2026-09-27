#include "storage.hpp"
#include "settings.hpp"
#include "common.hpp"
#include "catalog.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using ccs::runtime::Storage;
using ccs::runtime::Settings;
using nlohmann::json;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const std::filesystem::path root = argv[1];
        std::filesystem::remove_all(root);
        Storage storage(root / "presets");
        ccs::PresetData preset;
        preset.name = "../escape";
        check(!storage.save_preset(preset), "preset write escaped its directory");
        check(!storage.delete_preset("../escape"), "unsafe delete accepted");
        check(!storage.load_preset("../escape"), "unsafe load accepted");
        for (const auto* name : {"CON", "nul.txt", "LPT1", "COM9", ".hidden", "trailing.", "a:b", "a\\b"}) {
            preset.name = name;
            check(!storage.save_preset(preset), "invalid Windows filename accepted");
        }
        preset.name = "custom_1";
        preset.slots[0].origin = ccs::MoveOrigin::EnemyHumanoid;
        preset.slots[0].move_id = "BrigBase_Attack_Stab";
        check(storage.save_preset(preset), "preset save failed");
        auto duplicate = preset; duplicate.slots[0].move_id = "unconfirmed_change";
        check(!storage.save_preset(duplicate), "Default storage save replaced an existing preset");
        const auto loaded = storage.load_preset(preset.name);
        check(loaded && loaded->slots[0].origin == preset.slots[0].origin, "move origin lost during roundtrip");
        check(loaded->slots[0].move_id == preset.slots[0].move_id, "Rejected duplicate changed saved bindings");
        auto encoded = Storage::preset_to_json(preset);
        encoded["schema_version"] = 2;
        check(!Storage::json_to_preset(encoded), "future schema accepted");
        encoded["schema_version"] = 1;
        encoded["light_chain"]["H1"] = json::object();
        check(!Storage::json_to_preset(encoded), "heavy slot accepted in light chain");
        check(!Storage::json_to_preset(json::array()), "non-object accepted as preset");
        auto legacy = Storage::preset_to_json(preset);
        legacy["light_chain"]["L1"].erase("move_id");
        legacy["light_chain"]["L1"]["ability"] = "legacy_move";
        check(Storage::json_to_preset(legacy)->slots[0].move_id == "legacy_move", "bundled legacy preset lost its move id");
        check(ccs::runtime::write_file_atomic(root / "replace.json", "first"), "first write failed");
        check(ccs::runtime::write_file_atomic(root / "replace.json", "second"), "replacement failed");
        check(ccs::runtime::read_file_text(root / "replace.json") == "second", "replacement not visible");
        std::filesystem::create_directory(root / "directory.json");
        check(!ccs::runtime::write_file_atomic(root / "directory.json", "invalid"), "failed replacement reported success");
        Settings settings(root / "settings.json");
        check(!settings.enabled(), "unverified combat enabled at boot");
        const auto before = settings.to_json();
        try { settings.from_json({{"enabled", true}, {"damage_scale", -1.0}}); } catch (...) {}
        check(settings.to_json() == before, "invalid settings partially applied");
        check(settings.save() && settings.load(), "settings roundtrip failed");
        ccs::runtime::Catalog catalog;
        check(catalog.load(argv[2]), "generated catalog rejected");
        const auto snapshot = json::parse(ccs::runtime::read_file_text(argv[2]));
        check(!catalog.moves().empty() && catalog.moves().size() == snapshot.at("moves").size(),
              "catalog lost selector-linked moves");
        check(!catalog.find_move("GA_Player_Attack_ClockworkScythe_A3_Finisher_C"),
              "untagged additional attack received guessed finisher semantics");
        const auto* opener = catalog.find_move("GA_Player_HadernsSword_A1_C");
        check(opener && opener == catalog.find_move("HadernsSword_A1"), "legacy preset alias lost");
        check(opener->compatible_slots == 1 && !opener->runtime_verified, "opener evidence replaced by guessed slots");
        const auto* critical = catalog.find_tarstone("ID_Melee_HeavyAttackFinisher_Critical");
        check(critical && critical->levels == std::vector<double>{0.3, 0.35, 0.4}, "Clerik cooked stats altered");
        const auto* resolve = catalog.find_tarstone("ID_Melee_LightAttackFinisher_Resolve");
        check(resolve && resolve->levels == std::vector<double>{1.4, 1.7, 2.5}, "Zealot cooked stats altered");
        check(ccs::runtime::write_file_atomic(root / "bad-catalog.json", "{}"), "bad catalog fixture failed");
        check(!catalog.load(root / "bad-catalog.json") && catalog.find_move(opener->id), "bad reload destroyed the valid catalog");
        std::filesystem::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

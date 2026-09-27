#include "persistence.hpp"
#include "common.hpp"
#include "storage.hpp"
#include <chrono>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace ccs::runtime;
using namespace std::chrono_literals;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

FileResult receive(Persistence& files) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = files.poll()) return std::move(*result);
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("Persistence worker stopped returning accepted jobs");
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        const std::filesystem::path root = argv[1];
        std::filesystem::remove_all(root);
        ccs::PresetData preset;
        preset.name = "custom";
        preset.slots[0].move_id = "recorded_move";
        const auto payload = Storage::preset_to_json(preset).dump();
        {
            Persistence files(root);
            check(!files.submit({FileOperation::LoadPreset, "../escape", {}}), "Unsafe path was queued");
            check(!files.submit({FileOperation::SavePreset, preset.name, std::string(Persistence::payload_limit + 1, 'x')}), "Unbounded payload was queued");
            check(!files.submit({static_cast<FileOperation>(255), {}, {}}), "Unknown operation was queued");
            check(!files.submit({FileOperation::LoadPreset, preset.name, "unexpected"}), "Read request accepted a payload");
            check(!files.submit({FileOperation::SaveSettings, "unexpected", "{}"}), "Settings accepted a filename override");
            check(!files.submit({FileOperation::DeletePreset, preset.name, {}, true}), "Delete accepted overwrite authorization");
            const auto save = files.submit({FileOperation::SavePreset, preset.name, payload});
            const auto load = files.submit({FileOperation::LoadPreset, preset.name, {}});
            const auto remove = files.submit({FileOperation::DeletePreset, preset.name, {}});
            const auto missing = files.submit({FileOperation::LoadPreset, preset.name, {}});
            check(save && load && remove && missing, "Normal requests were rejected");
            auto result = receive(files);
            check(result.id == *save && result.success, "Preset save failed or FIFO order changed");
            result = receive(files);
            check(result.id == *load && result.success && result.preset && result.preset->slots[0].move_id == "recorded_move", "Load did not observe preceding save");
            result = receive(files);
            check(result.id == *remove && result.success, "Delete failed");
            result = receive(files);
            check(result.id == *missing && !result.success && !result.error.empty(), "Missing preset was treated as successful");

            check(files.submit({FileOperation::SavePreset, preset.name, payload}).has_value(), "Create save rejected");
            check(receive(files).success, "Create save failed");
            const auto saved = read_file_text(root / "presets/custom.json");
            auto changed = preset;
            changed.slots[0].move_id = "changed_move";
            const auto changed_payload = Storage::preset_to_json(changed).dump();
            check(files.submit({FileOperation::SavePreset, preset.name, changed_payload}).has_value(), "Conflict request rejected");
            result = receive(files);
            check(!result.success && result.exists && read_file_text(root / "presets/custom.json") == saved,
                "Unconfirmed save replaced existing content");
            check(files.submit({FileOperation::SavePreset, preset.name, changed_payload, true}).has_value(), "Confirmed overwrite rejected");
            check(receive(files).success && Storage(root / "presets").load_preset(preset.name)->slots[0].move_id == "changed_move",
                "Confirmed overwrite did not replace the preset");

            check(files.submit({FileOperation::SaveSettings, {}, R"({"enabled":false,"damage_scale":2.0})"}).has_value(), "Settings request rejected");
            check(receive(files).success, "Settings write failed");
            const auto before = read_file_text(root / "settings.json");
            check(files.submit({FileOperation::SaveSettings, {}, R"({"enabled":true,"damage_scale":-1.0})"}).has_value(), "Invalid settings did not reach validation");
            check(!receive(files).success && read_file_text(root / "settings.json") == before, "Invalid settings replaced valid data");

            auto mismatch = Storage::preset_to_json(preset);
            mismatch["preset_name"] = "different";
            check(files.submit({FileOperation::SavePreset, preset.name, mismatch.dump()}).has_value(), "Mismatched name request rejected before validation");
            check(!receive(files).success && !std::filesystem::exists(root / "presets/different.json"), "Payload redirected preset output");

            for (size_t i = 0; i < Persistence::capacity; ++i)
                check(files.submit({FileOperation::ListPresets, {}, {}}).has_value(), "Capacity consumed before its documented bound");
            std::this_thread::sleep_for(20ms);
            check(!files.submit({FileOperation::ListPresets, {}, {}}), "Unread results allowed unbounded growth");
            for (size_t i = 0; i < Persistence::capacity; ++i) check(receive(files).success, "Accepted list request lost");
            check(files.submit({FileOperation::ListPresets, {}, {}}).has_value(), "Consuming results did not release capacity");
            files.close();
            check(!files.submit({FileOperation::ListPresets, {}, {}}), "Closed worker accepted a request");
            check(receive(files).success, "Closing discarded an accepted job");
        }
        {
            Persistence files(root);
            auto shutdown_preset = preset; shutdown_preset.name = "shutdown";
            check(files.submit({FileOperation::SavePreset, shutdown_preset.name, Storage::preset_to_json(shutdown_preset).dump()}).has_value(), "Shutdown save request rejected");
        }
        {
            Persistence files(root);
            std::atomic<size_t> accepted{};
            std::array<std::jthread, 4> producers;
            for (auto& thread : producers) thread = std::jthread([&] {
                for (size_t i = 0; i < 8; ++i)
                    if (files.submit({FileOperation::ListPresets, {}, {}})) ++accepted;
            });
            for (auto& thread : producers) thread.join();
            check(accepted == Persistence::capacity, "Concurrent producers lost requests below capacity");
            uint64_t previous{};
            for (size_t i = 0; i < Persistence::capacity; ++i) {
                const auto result = receive(files);
                check(result.success && result.id > previous, "Concurrent requests duplicated or reordered results");
                previous = result.id;
            }
        }
        Storage storage(root / "presets");
        check(storage.load_preset(preset.name).has_value(), "Worker destruction lost accepted writes");
        check(storage.load_preset("shutdown").has_value(), "Worker destruction discarded an accepted create");
        {
            const auto target = root / "race.json";
            const std::string a(131072, 'a'), b(131072, 'b');
            std::atomic<unsigned> winners{}, conflicts{}, failed{};
            std::array<std::jthread, 8> writers;
            for (size_t i = 0; i < writers.size(); ++i) writers[i] = std::jthread([&, i] {
                const auto result = write_file_atomic(target, i % 2 ? a : b, false);
                if (result == FileWriteResult::Success) ++winners;
                else if (result == FileWriteResult::Exists) ++conflicts;
                else ++failed;
            });
            for (auto& writer : writers) writer.join();
            const auto content = read_file_text(target);
            check(winners == 1 && conflicts == 7 && failed == 0 && (content == a || content == b),
                "Concurrent create published multiple winners or partial content");
            const auto legacy_temp = root / "race.json.tmp";
            std::ofstream(legacy_temp) << "other writer";
            check(write_file_atomic(target, "replacement"), "Confirmed replacement failed");
            check(read_file_text(target) == "replacement" && read_file_text(legacy_temp) == "other writer",
                "Atomic replacement damaged another writer's temporary file");
            std::filesystem::create_directory(root / "directory.json");
            check(write_file_atomic(root / "directory.json", "data", true) == FileWriteResult::Failed,
                "A directory was replaced as a file");
            for (const auto& entry : std::filesystem::directory_iterator(root))
                check(entry.path().filename().string().find(".tmp.") == std::string::npos, "Temporary output leaked");
        }
        Storage many(root / "many");
        for (size_t i = 0; i < 1024; ++i) std::ofstream(root / "many" / ("item_" + std::to_string(i) + ".json")) << "{}";
        std::vector<std::string> names;
        check(many.list_presets(names) && names.size() == 1024, "Exact list limit rejected");
        std::ofstream(root / "many/overflow.json") << "{}";
        check(!many.list_presets(names) && names.empty(), "Oversized library silently returned a partial list");
        std::filesystem::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

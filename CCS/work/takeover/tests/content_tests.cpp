#include "content_monitor.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace ccs::runtime;
using namespace std::chrono_literals;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void file(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents; output.close(); check(static_cast<bool>(output), "Fixture write failed");
}
template<class F> void rejects(F operation) {
    bool failed{}; try { operation(); } catch (const std::exception&) { failed = true; }
    check(failed, "Unsafe or incomplete content inspection accepted");
}
std::shared_ptr<const ContentObservation> observe(ContentMonitor& monitor, uint64_t after) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    bool requested{};
    while (std::chrono::steady_clock::now() < deadline) {
        if (!requested) requested = monitor.request();
        if (auto result = monitor.poll(); result && result->sequence > after) return result;
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("Content monitor did not publish requested inspection");
}
int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--inspect") {
            const auto began = std::chrono::steady_clock::now();
            const auto manifest = read_content_manifest(fs::path(argv[2]));
            nlohmann::json output{{"scope", "disk_metadata_change_detector"}, {"mounted_content_verified", false},
                {"executable", manifest.executable}, {"packages", manifest.packages}, {"files", nlohmann::json::array()},
                {"file_clock_period_num", fs::file_time_type::duration::period::num},
                {"file_clock_period_den", fs::file_time_type::duration::period::den},
                {"elapsed_us", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - began).count()}};
            for (const auto& entry : manifest.files)
                output["files"].push_back({{"path", entry.path}, {"bytes", entry.bytes}, {"modified_ticks", entry.modified}});
            std::cout << output.dump(2) << '\n'; return 0;
        }
        check(argc == 2, "Expected fixture directory or --inspect executable");
        const auto root = fs::absolute(argv[1]);
        fs::remove_all(root);
        const auto executable = root / "Game/Binaries/Win64/Game.exe";
        const auto packages = root / "Game/Content/Paks";
        file(executable, "executable");
        file(packages / "Base.PAK", "pak");
        file(packages / "base.utoc", "toc");
        file(packages / "base.ucas", "data");
        file(packages / "~mods/mod.pak", "mod");
        file(packages.parent_path() / "AssetRegistry.bin", "registry");
        const auto initial = read_content_manifest(executable);
        check(initial.files.size() == 6, "Executable, containers, nested mod or registry omitted");
        check(initial == read_content_manifest(executable), "Stable installation changed identity");
        file(packages / "readme.txt", "ignored");
        check(initial == read_content_manifest(executable), "Unrelated file changed content identity");
        const auto old_time = fs::last_write_time(packages / "base.ucas");
        fs::last_write_time(packages / "base.ucas", old_time + 1s);
        check(initial != read_content_manifest(executable), "Content timestamp change missed");
        fs::last_write_time(packages / "base.ucas", old_time);
        file(packages / "base.ucas", "DATA");
        fs::last_write_time(packages / "base.ucas", old_time);
        check(initial == read_content_manifest(executable), "Metadata detector falsely claims byte identity");
        rejects([&] { read_content_manifest(fs::path("relative.exe")); });
        std::stop_source canceled; canceled.request_stop();
        rejects([&] { read_content_manifest(executable, canceled.get_token()); });
        fs::create_directory_symlink(packages / "~mods", packages / "linked-mod");
        rejects([&] { read_content_manifest(executable); });
        fs::remove(packages / "linked-mod");
        {
            ContentMonitor monitor(executable);
            check(!monitor.poll(), "Idle monitor performed unsolicited disk inspection");
            auto result = observe(monitor, 0);
            check(result->available && !result->restart_required, "Initial observation rejected");
            result = observe(monitor, result->sequence);
            check(result->available && !result->restart_required, "Unchanged files require restart");
            file(packages / "~mods/new.utoc", "new mod");
            result = observe(monitor, result->sequence);
            check(result->available && result->restart_required, "Installed content change was not latched");
            fs::remove(packages / "~mods/new.utoc");
            result = observe(monitor, result->sequence);
            check(result->restart_required, "Returning files to baseline cleared restart requirement");
            fs::remove(executable);
            result = observe(monitor, result->sequence);
            check(!result->available && !result->manifest && !result->error.empty() && result->restart_required,
                "Failed read retained a usable stale manifest");
            file(executable, "executable");
            monitor.close();
            check(!monitor.request(), "Closed monitor accepted work");
            const auto deadline = std::chrono::steady_clock::now() + 3s;
            while (!monitor.stopped() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
            check(monitor.stopped(), "Closed monitor did not quiesce");
        }
        fs::remove_all(packages);
        fs::create_directories(packages);
        rejects([&] { read_content_manifest(executable); });
        {
            ContentMonitor monitor(executable);
            auto result = observe(monitor, 0);
            check(!result->available, "Empty installation accepted");
            file(packages / "repaired.pak", "repair");
            result = observe(monitor, result->sequence);
            check(result->available && !result->restart_required, "Initial failed inspection cannot recover");
        }
        for (unsigned i = 0; i < 16; ++i) {
            ContentMonitor monitor(executable);
            check(monitor.request(), "Fresh monitor rejected inspection");
            monitor.close();
            check(!monitor.request(), "Closing queued/running monitor accepted work");
        }
        auto deep = packages;
        for (unsigned i = 0; i <= ContentMonitor::depth_limit; ++i) deep /= "nested";
        file(deep / "hidden.pak", "hidden");
        rejects([&] { read_content_manifest(executable); });
        fs::remove_all(packages / "nested");
        for (size_t i = 0; i < ContentMonitor::entry_limit; ++i) file(packages / (std::to_string(i) + ".txt"), "");
        rejects([&] { read_content_manifest(executable); });
        std::cout << "Content change, failure, cancellation and lifetime checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

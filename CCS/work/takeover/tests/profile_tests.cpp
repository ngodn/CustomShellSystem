#include "frame_profile.hpp"
#include "common.hpp"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

using Profile = ccs::runtime::FrameProfile;
using Json = nlohmann::json;
void check(bool value) { if (!value) throw std::runtime_error("Frame profile invariant failed"); }
Json read(const std::filesystem::path& path) {
    std::ifstream input(path);
    return Json::parse(input);
}
bool arm(Profile& profile, uint64_t now, uint64_t duration = 30'000'000'000ull) {
    const auto deadline = Profile::Clock::now() + std::chrono::seconds(3);
    while (Profile::Clock::now() < deadline) {
        if (profile.arm(now, duration)) return true;
        std::this_thread::yield();
    }
    return false;
}
int main(int argc, char** argv) {
    check(argc == 2);
    const std::filesystem::path root(argv[1]);
    std::filesystem::remove_all(root);
    {
        Profile profile(root / "statistics", "test", 100);
        check(!profile.arm(0, 0));
        check(!profile.arm(std::numeric_limits<uint64_t>::max()));
        check(arm(profile, 1000));
        check(!profile.arm(1001));
        for (uint64_t i = 0; i < 100; ++i) {
            const auto start = 1000 + i * 1'000'000;
            profile.begin(i == 10 ? std::numeric_limits<double>::quiet_NaN() : .001, start);
            if (i < 50) profile.record(Profile::Phase::Menu, i * 1000);
            profile.record(Profile::Phase::Context, 1000);
            profile.end(start + (i + 1) * 1000, i == 20);
        }
        const auto deadline = Profile::Clock::now() + std::chrono::seconds(3);
        while (profile.active() && Profile::Clock::now() < deadline) { profile.begin(0, 101'000'000); std::this_thread::yield(); }
        check(!profile.active());
    }
    const auto report = read(root / "statistics/test-1000.json");
    check(report.at("frames") == 100 && report.at("failed_frames") == 1);
    check(report.at("stop_reason") == "frame_capacity");
    check(report.at("elapsed").at("mean_us") == 50.5);
    check(report.at("elapsed").at("p95_us") == 95.0);
    check(report.at("elapsed").at("p99_us") == 99.0);
    check(report.at("elapsed").at("maximum_us") == 100.0);
    check(report.at("engine_delta").at("count") == 99);
    check(report.at("tick_interval").at("count") == 99);
    check(report.at("phases").at("menu").at("count") == 50);
    check(report.at("phases").at("probe").at("count") == 0);
    check(report.at("phases").at("context").at("count") == 100);
    check(std::filesystem::file_size(root / "statistics/test-1000.csv") > 1000);
    {
        Profile profile(root / "hitch", "test", 4);
        check(arm(profile, 100));
        profile.begin(.016, 100); profile.record(Profile::Phase::Menu, 10); profile.end(150);
        profile.begin(.016, 1'000'100); profile.record(Profile::Phase::Menu, 20); profile.end(1'000'170);
        profile.begin(-1, 1'000'200); profile.end(1'000'210);
        profile.cancel(1'000'211);
    }
    const auto hitch = read(root / "hitch/test-100.json");
    check(hitch.at("stop_reason") == "cancelled");
    const auto& largest = hitch.at("largest_intervals").at(0);
    check(largest.at("frame") == 1 && largest.at("interval_ns") == 1'000'000);
    check(largest.at("preceding_callback_ns") == 50 && largest.at("current_callback_ns") == 70);
    check(largest.at("preceding_phases_ns").at(static_cast<size_t>(Profile::Phase::Menu)) == 10);
    {
        Profile profile(root / "drain", "test", 8);
        check(arm(profile, 200));
        profile.begin(.1, 200); profile.end(300);
        // Destruction drains a partially filled capture without any engine call.
    }
    check(read(root / "drain/test-200.json").at("stop_reason") == "shutdown");
    {
        Profile profile(root / "duration", "test", 8);
        check(arm(profile, 400, 3'000'000'000));
        profile.begin(0, 400); profile.end(500);
        profile.begin(.1, 3'000'000'401); profile.end(3'000'000'402);
    }
    check(read(root / "duration/test-400.json").at("stop_reason") == "duration");
    const auto capture_start = Profile::now_ns();
    {
        Profile profile(root / "exception", "test", 8);
        check(arm(profile, capture_start));
        try {
            Profile::Scope frame(profile, .016);
            profile.measure(Profile::Phase::Menu, [] { throw std::runtime_error("measured failure"); });
        } catch (const std::runtime_error&) {}
        profile.cancel(Profile::now_ns());
    }
    const auto exception = read(root / "exception" / ("test-" + std::to_string(capture_start) + ".json"));
    check(exception.at("frames") == 1 && exception.at("failed_frames") == 1);
    check(exception.at("phases").at("menu").at("count") == 1);
    {
        std::ofstream(root / "not-a-directory") << "occupied";
        Profile profile(root / "not-a-directory", "test", 1);
        check(arm(profile, 500)); profile.begin(.1, 500); profile.end(600);
        const auto submit_deadline = Profile::Clock::now() + std::chrono::seconds(3);
        while (profile.active() && Profile::Clock::now() < submit_deadline) { profile.begin(0, 700); std::this_thread::yield(); }
        const auto deadline = Profile::Clock::now() + std::chrono::seconds(3);
        while (!profile.failures() && Profile::Clock::now() < deadline) std::this_thread::yield();
        check(profile.failures() == 1 && profile.completed() == 0);
    }
}

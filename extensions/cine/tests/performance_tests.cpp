#include "performance.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace cine;
namespace {
int checks = 0;
void expect(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (...) { failed = true; } expect(failed, "Expected rejection"); }
struct Host {
    Json state{{"id", ""}, {"token", uint64_t(0)}, {"active", false}, {"reason", ""}};
    bool stop_fails = false;
    int starts = 0, stops = 0;
    Json last_start;
    Json call(const Json& r) {
        const auto op = r.at("op");
        if (op == "status") return state;
        if (op == "start") {
            ++starts; last_start = r;
            state["token"] = state.at("token").get<uint64_t>() + 1;
            state["id"] = r.at("id"); state["active"] = true; state["reason"] = ""; return state;
        }
        if (op == "stop") {
            if (stop_fails) throw std::runtime_error("service busy");
            expect(r.at("token") == state.at("token"), "Stop reached someone else's performance");
            ++stops; state["active"] = false; return state;
        }
        throw std::runtime_error("Unknown request");
    }
};
Json entry(const char* id, const char* library, bool available = true) {
    return {{"id", id}, {"name", id}, {"library", library}, {"library_name", library},
        {"playback", "once"}, {"available", available}, {"reason", available ? "" : "Unsupported outfit"}};
}
}
int main() {
    try {
        PerformanceChoice c; c.id = "css.dance"; c.rate = .6; c.seconds = 2.4;
        expect(PerformanceChoice::parse(c.json()).json() == c.json(), "Saved selection did not round-trip");
        for (const auto rate : {0., 4.1, std::numeric_limits<double>::infinity()}) rejects([&] { auto j = c.json(); j["rate"] = rate; PerformanceChoice::parse(j); });
        rejects([&] { auto j = c.json(); j["seconds"] = -1.; PerformanceChoice::parse(j); });
        rejects([&] { auto j = c.json(); j["id"] = std::string(97, 'x'); PerformanceChoice::parse(j); });
        expect(!c.json().contains("token"), "Ownership leaked into persistence");
        Host h; auto call = [&](const Json& r) { return h.call(r); };
        PerformanceTake take;
        take.start(call, {}); expect(h.starts == 0 && !take.owned(), "Off called CSS");
        take.start(call, c);
        expect(h.last_start.at("rate") == .6 && h.last_start.at("seconds") == 2.4 && h.last_start.at("id") == c.id, "Timing or canonical ID changed");
        rejects([&] { take.start(call, c); });
        expect(take.poll(call).empty() && take.owned(), "A live take lost ownership");
        h.stop_fails = true; rejects([&] { take.stop(call); });
        expect(take.owned(), "Failed cleanup forgot ownership");
        h.stop_fails = false; take.stop(call);
        expect(h.stops == 1 && !take.owned(), "Cleanup did not retry");
        take.stop(call); expect(h.stops == 1, "Repeated stop mutated the game");
        take.start(call, c); h.state["active"] = false;
        expect(take.poll(call).empty() && !take.owned(), "Natural end was treated as a failed take");
        take.start(call, c); h.state["active"] = false; h.state["reason"] = "Player moved";
        expect(take.poll(call) == "Player moved", "Gameplay interruption was hidden");
        take.start(call, c); h.state["token"] = uint64_t(99);
        take.stop(call); expect(h.stops == 1 && h.state.at("active") == true, "Stale stop ended another take");
        take.start(call, c); h.state["id"] = "someone.else";
        expect(!take.poll(call).empty() && !take.owned(), "Replacement was not detected");

        PerformanceCatalog catalog;
        expect(!catalog.unavailable(c).empty(), "Unread library appeared available");
        expect(catalog.options(c).back().at("id") == c.id, "Missing library discarded saved choice");
        Json data{{"entries", Json::array({entry("css.dance", "a"), entry("css.pose", "b", false)})}};
        auto refresh = [&] { catalog.refresh([&](const Json&) { return data; }); };
        refresh(); expect(catalog.unavailable(c).empty(), "Supported clip unavailable");
        expect(catalog.libraries(c).size() == 2 && catalog.options(c).size() == 2, "Library groups were mixed");
        c.id = "css.pose";
        expect(catalog.unavailable(c) == "Unsupported outfit", "Unavailable reason lost");
        data["entries"].push_back(data["entries"][0]); refresh();
        expect(!catalog.find("css.dance") && !catalog.unavailable(c).empty(), "Invalid catalog retained partial entries");
        catalog.refresh([](const Json&) -> Json { throw std::runtime_error("CSS unavailable"); });
        expect(catalog.description(c).find("CSS unavailable") != std::string::npos, "Service failure not shown");
        data["entries"] = Json::array(); refresh();
        expect(catalog.description({}).find("Install") != std::string::npos, "Empty library lacks installation guidance");
        for (int i = 0; i < 257; ++i) { auto e = entry("id", "a"); e["id"] = "id" + std::to_string(i); data["entries"].push_back(e); }
        refresh(); expect(!catalog.find("id0"), "Oversized library was silently truncated");
        std::cout << checks << " CINE performance checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

#include "expression.hpp"
#include <iostream>

using namespace cine;
namespace {
int checks = 0;
void expect(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (...) { failed = true; } expect(failed, "Expected rejection"); }
struct Host {
    Json state{{"shell", "genessa"}, {"outfit", "eve"}, {"variant", "pearl"}, {"busy", false},
        {"customize", {{"expression", "css.face.neutral"}, {"body", 0.6}}},
        {"expression", {{"applied", "css.face.neutral"}, {"reason", ""}}}};
    int reads = 0, writes = 0;
    bool throw_before = false, throw_after = false;
    Json last;
    void settle() {
        state["busy"] = false;
        state["expression"] = {{"applied", state.at("customize").value("expression", std::string())}, {"reason", ""}};
    }
    Json call(const Json& r) {
        if (r.at("action") == "describe") { ++reads; return state; }
        if (r.at("action") != "apply") throw std::runtime_error("Unexpected request");
        if (throw_before) throw std::runtime_error("Before accepting write");
        ++writes; last = r;
        const auto& command = r.at("commands").at(0);
        if (command.at("action") == "restore") state["customize"] = command.at("customize");
        else state["customize"]["expression"] = command.at("expression");
        state["busy"] = true;
        if (throw_after) throw std::runtime_error("After accepting write");
        return state;
    }
};
}
int main() {
    try {
        Host h; auto call = [&](const Json& r) { return h.call(r); };
        ExpressionTake take;
        take.start(call, "unchanged"); expect(take.ready(call) && take.stop(call) && h.reads == 0 && h.writes == 0, "Default called CSS");
        take.start(call, "css.face.happy");
        expect(take.owned() && !take.ready(call) && !h.last.at("persist").get<bool>(), "Take did not wait for transient write");
        rejects([&] { take.start(call, "css.face.happy"); });
        h.settle(); expect(take.ready(call), "Applied face not ready");
        h.state["customize"]["body"] = .9;
        expect(!take.stop(call) && take.owned(), "Restore did not wait for CSS");
        expect(h.last.at("persist") == true && h.state["customize"]["body"] == .9 &&
               h.state["customize"]["expression"] == "css.face.neutral", "Restore clobbered body edits or original face");
        expect(!take.stop(call) && h.writes == 2, "Busy restore issued another write");
        h.settle(); expect(take.stop(call) && !take.owned(), "Completed restore retained ownership");
        const auto writes = h.writes;
        take.start(call, "css.face.neutral"); expect(!take.owned() && h.writes == writes, "Same face was rewritten");
        h.state["expression"]["reason"] = "Missing shared asset";
        rejects([&] { take.start(call, "css.face.neutral"); });
        h.settle(); take.start(call, "original"); h.settle(); expect(take.ready(call), "Original face rejected");
        take.stop(call); h.settle(); take.stop(call);
        take.start(call, "css.face.happy"); h.settle();
        h.state["customize"]["expression"] = "css.face.wink";
        const auto changed_writes = h.writes;
        rejects([&] { take.ready(call); });
        expect(take.stop(call) && h.writes == changed_writes, "Cleanup overwrote another writer's face");
        take.start(call, "css.face.happy"); h.settle(); h.state["variant"] = "other";
        const auto variant_writes = h.writes;
        rejects([&] { take.ready(call); });
        expect(take.stop(call) && h.writes == variant_writes, "Cleanup wrote to a different variant");
        h.state["customize"].erase("expression"); h.settle();
        take.start(call, "css.face.happy"); h.settle(); take.stop(call);
        expect(!h.state["customize"].contains("expression"), "Original reset retained a selected face");
        h.settle(); take.stop(call);
        h.throw_before = true; rejects([&] { take.start(call, "css.face.happy"); });
        h.throw_before = false; expect(take.stop(call), "Failed first write leaked ownership");
        h.throw_after = true; rejects([&] { take.start(call, "css.face.happy"); });
        h.throw_after = false; h.settle();
        expect(!take.stop(call), "Accepted write exception lost restoration"); h.settle(); take.stop(call);
        take.start(call, "css.face.happy"); h.settle();
        h.state["expression"] = {{"applied", ""}, {"reason", "Missing facial mesh"}};
        rejects([&] { take.ready(call); }); take.stop(call); h.settle(); take.stop(call);
        take.start(call, "css.face.happy"); h.settle(); h.throw_before = true;
        rejects([&] { take.stop(call); }); expect(take.owned(), "Failed cleanup forgot ownership");
        h.throw_before = false; take.stop(call); h.settle(); take.stop(call);
        h.state["busy"] = true; rejects([&] { take.start(call, "original"); });
        expect(!take.owned(), "Busy CSS acquired face ownership");
        for (const auto* id : {"", "white space", "bad/path"}) rejects([&] { expression_choice(id); });

        ExpressionCatalog catalog;
        expect(catalog.unavailable("unchanged").empty(), "No pack blocks ordinary takes");
        expect(catalog.options("css.face.happy").back().at("id") == "css.face.happy", "Unavailable saved ID lost");
        Json entries = Json::array({{{"id", "css.face.happy"}, {"name", "Happy"}}});
        auto refresh = [&] { catalog.refresh([&](const Json&) { return Json{{"entries", entries}}; }); };
        refresh(); expect(catalog.unavailable("css.face.happy").empty(), "Catalog lost supported face");
        expect(!catalog.unavailable("css.face.wink").empty(), "Unknown face allowed");
        entries.push_back(entries.front()); refresh();
        expect(!catalog.unavailable("css.face.happy").empty() && catalog.options("unchanged").size() == 2, "Invalid catalog partially accepted");
        catalog.refresh([](const Json&) -> Json { throw std::runtime_error("Offline"); });
        expect(catalog.description("css.face.happy").find("Offline") != std::string::npos, "Missing service error hidden");
        std::cout << checks << " CINE expression checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

// Service bus tests: fake provider modules behind the injected scan/alive functions.
#include "services.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using cssx::Json;
namespace {
unsigned checks = 0;
void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
template <class F> void rejects(F&& f, const std::string& needle, const std::string& what) {
    ++checks;
    try { f(); } catch (const std::exception& e) {
        if (std::string(e.what()).find(needle) == std::string::npos) throw std::runtime_error(what + ": wrong error: " + e.what());
        return;
    }
    throw std::runtime_error(what + ": accepted");
}
void send(CssxServiceSink sink, void* ctx, const std::string& s) { sink(ctx, s.data(), s.size()); }

int calls_echo = 0;
int echo(void* provider, const char* request, size_t size, CssxServiceSink sink, void* ctx) {
    ++calls_echo;
    auto r = Json::parse(std::string(request, size));
    send(sink, ctx, Json{{"echo", r}, {"tag", static_cast<const char*>(provider)}}.dump());
    return 1;
}
int refuse(void*, const char*, size_t, CssxServiceSink sink, void* ctx) { send(sink, ctx, R"({"error":"busy, try later"})"); return 0; }
int refuse_bare(void*, const char*, size_t, CssxServiceSink, void*) { return 0; }
int silent(void*, const char*, size_t, CssxServiceSink, void*) { return 1; }
int twice(void*, const char*, size_t, CssxServiceSink sink, void* ctx) { send(sink, ctx, "1"); send(sink, ctx, "2"); return 1; }
int garbage(void*, const char*, size_t, CssxServiceSink sink, void* ctx) { send(sink, ctx, "{not json"); return 1; }
int huge(void*, const char*, size_t, CssxServiceSink sink, void* ctx) { sink(ctx, "x", cssx::max_service_reply + 1); return 1; }

char tag_a[] = "A", tag_b[] = "B";
const CssxService a_services[]{
    {"css.customize", 2, "look", echo, tag_a},
    {"css.refuse", 1, nullptr, refuse, nullptr},
    {"css.refuse-bare", 1, nullptr, refuse_bare, nullptr},
    {"css.silent", 1, nullptr, silent, nullptr},
    {"css.twice", 1, nullptr, twice, nullptr},
    {"css.garbage", 1, nullptr, garbage, nullptr},
    {"css.huge", 1, nullptr, huge, nullptr},
    {"Bad Name", 1, nullptr, echo, nullptr},
    {"nodot", 1, nullptr, echo, nullptr},
    {"css.nocall", 1, nullptr, nullptr, nullptr},
    {"css.v0", 0, nullptr, echo, nullptr},
    {"css.customize", 9, "repeat", echo, nullptr},
};
const CssxServiceTable a_table{CSSX_SERVICE_ABI, sizeof(CssxServiceTable), uint32_t(std::size(a_services)), a_services};
const CssxService b_services[]{{"css.customize", 1, "new core", echo, tag_b}, {"other.ping", 1, "ping", echo, tag_b}};
const CssxServiceTable b_table{CSSX_SERVICE_ABI, sizeof(CssxServiceTable), 2, b_services};
const CssxServiceTable future_table{2, sizeof(CssxServiceTable), 2, b_services};
const CssxServiceTable small_table{CSSX_SERVICE_ABI, 8, 2, b_services};
const CssxServiceTable big_table{CSSX_SERVICE_ABI, sizeof(CssxServiceTable), 65, b_services};

bool a_active = true;
const CssxServiceTable* get_a() { return a_active ? &a_table : nullptr; }
const CssxServiceTable* get_b() { return &b_table; }

struct World {
    std::vector<cssx::ServiceProvider> loaded;
    std::vector<std::string> unloaded;
    int scans = 0, warnings = 0;
    bool fail_scan = false;
    cssx::ServiceBus bus{
        [this] { ++scans; if (fail_scan) throw std::runtime_error("EnumProcessModules failed"); return loaded; },
        [this](const cssx::ServiceProvider& p) { for (const auto& u : unloaded) if (u == p.module) return false; return true; },
        [this](const std::string& level, const std::string&, const Json&) { if (level == "warning") ++warnings; }};
};
}

int main() {
    try {
        // Names.
        for (const auto* ok : {"css.customize", "a.b", "my-mod.do_it2"}) expect(cssx::valid_service_name(ok), std::string("valid name ") + ok);
        for (const auto* bad : {"", "ab", "nodot", ".lead", "trail.", "Upper.case", "sp ace.x", "a/b.c"}) expect(!cssx::valid_service_name(bad), std::string("invalid name ") + bad);
        expect(!cssx::valid_service_name(std::string(65, 'a') + ".b"), "names are at most 64 bytes");

        // Table reading.
        std::vector<std::string> problems;
        auto entries = cssx::read_service_table(&a_table, &problems);
        expect(entries.size() == 7, "seven valid entries, got " + std::to_string(entries.size()));
        expect(entries[0].name == "css.customize" && entries[0].version == 2 && entries[0].description == "look", "first customize entry kept");
        expect(problems.size() == 5, "bad name, no dot, no call, version 0, repeated name reported");
        expect(cssx::read_service_table(nullptr).empty(), "null table: nothing");
        problems.clear(); expect(cssx::read_service_table(&future_table, &problems).empty() && !problems.empty(), "future ABI refused with a reason");
        expect(cssx::read_service_table(&small_table).empty(), "short table refused");
        expect(cssx::read_service_table(&big_table).empty(), "more than 64 entries refused");
        const CssxServiceTable empty{CSSX_SERVICE_ABI, sizeof(CssxServiceTable), 0, nullptr};
        expect(cssx::read_service_table(&empty).empty(), "empty table is fine");
        const CssxServiceTable dangling{CSSX_SERVICE_ABI, sizeof(CssxServiceTable), 1, nullptr};
        expect(cssx::read_service_table(&dangling).empty(), "count without entries refused");
        char unterminated[80]; std::fill(std::begin(unterminated), std::end(unterminated), 'a');
        const CssxService runaway[]{{unterminated, 1, nullptr, echo, nullptr}};
        const CssxServiceTable runaway_table{CSSX_SERVICE_ABI, sizeof(CssxServiceTable), 1, runaway};
        expect(cssx::read_service_table(&runaway_table).empty(), "an unterminated name is refused without reading past 65 bytes");

        // Calls and reply handling.
        auto find = [&](const std::string& n) { for (const auto& e : entries) if (e.name == n) return e; throw std::runtime_error("no " + n); };
        auto r = cssx::call_service(find("css.customize"), {{"action", "describe"}});
        expect(r["echo"]["action"] == "describe" && r["tag"] == "A", "request reaches the provider and the reply comes back");
        rejects([&] { cssx::call_service(find("css.refuse"), {}); }, "busy, try later", "provider error text is passed on");
        rejects([&] { cssx::call_service(find("css.refuse-bare"), {}); }, "refused the call", "a bare refusal is described");
        rejects([&] { cssx::call_service(find("css.silent"), {}); }, "sent no reply", "success without a reply is an error");
        expect(cssx::call_service(find("css.twice"), {}) == 1, "only the first reply counts");
        rejects([&] { cssx::call_service(find("css.garbage"), {}); }, "not JSON", "non-JSON reply is an error");
        rejects([&] { cssx::call_service(find("css.huge"), {}); }, "larger than 4 MiB", "oversized reply is an error");

        // The bus: discovery, version checks, rate-limited rescans, hot swap.
        {
            World w;
            rejects([&] { w.bus.call("css.customize", 1, {}, 0.0); }, "No service named css.customize is loaded", "nothing loaded");
            expect(w.scans == 1, "a miss scans once");
            rejects([&] { w.bus.call("css.customize", 1, {}, 0.5); }, "No service named", "still nothing");
            expect(w.scans == 1, "misses within a second do not rescan");
            w.loaded.push_back({nullptr, "C:/mods/css_core-1.dll", get_a});
            rejects([&] { w.bus.call("css.customize", 1, {}, 0.9); }, "No service named", "rate limit holds");
            expect(w.bus.call("css.customize", 1, {{"x", 1}}, 1.1)["tag"] == "A", "found after the interval");
            expect(w.scans == 2, "one more scan");
            for (int i = 0; i < 100; ++i) w.bus.call("css.customize", 2, {}, 2.0 + i);
            expect(w.scans == 2, "hits never rescan");
            rejects([&] { w.bus.call("css.customize", 3, {}, 200); }, "is version 2; this caller needs 3", "too old a provider is explained");
            expect(w.bus.call("css.customize", 0, {}, 200)["tag"] == "A", "version 0 accepts any");
            rejects([&] { w.bus.call("Bad", 1, {}, 200); }, "Invalid service name", "bad names are refused before any scan");
            expect(w.warnings == 1, "a provider's table problems are logged once, not per call");

            // Hot swap: the old copy withdraws its table, then unloads; the new copy is found.
            a_active = false;
            w.loaded.push_back({nullptr, "C:/mods/css_core-2.dll", get_b});
            expect(w.bus.call("css.customize", 1, {}, 300)["tag"] == "B", "a withdrawn table hands over to the new module");
            w.unloaded.push_back("C:/mods/css_core-1.dll");
            w.loaded.erase(w.loaded.begin());
            expect(w.bus.call("css.customize", 1, {}, 301)["tag"] == "B", "the unloaded module is never touched");
            a_active = true;

            auto list = w.bus.list(400);
            expect(list.size() == 2 && list[0]["name"] == "css.customize" && list[0]["module"] == "C:/mods/css_core-2.dll" && list[1]["name"] == "other.ping", "list shows live services");

            // Duplicate names across modules: the first live provider wins, list agrees.
            w.loaded.insert(w.loaded.begin(), {nullptr, "C:/mods/css_core-3.dll", get_a});
            w.bus.reset();
            expect(w.bus.call("css.customize", 1, {}, 500)["tag"] == "A", "first live provider wins");
            list = w.bus.list(500);
            size_t customize = 0; for (const auto& e : list) if (e["name"] == "css.customize") { ++customize; expect(e["version"] == 2, "list shows the winner"); }
            expect(customize == 1, "a name appears once in the list");

            // A failing scan is logged and reads as "nothing loaded"; it never throws through.
            w.fail_scan = true; w.bus.reset();
            rejects([&] { w.bus.call("css.customize", 1, {}, 600); }, "No service named", "failed scan: nothing loaded");
            expect(w.warnings >= 2, "the failed scan is logged");
        }
        std::cout << checks << " service bus checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}

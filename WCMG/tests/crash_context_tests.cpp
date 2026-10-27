#include "check.hpp"
#include "scanners.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>

using namespace wcmg;
namespace fs = std::filesystem;

namespace {

void write_text(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << s;
}

const char* kXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<FGenericCrashContext>
	<RuntimeProperties>
		<CrashVersion>3</CrashVersion>
		<CrashGUID>UECC-Windows-TEST_0000</CrashGUID>
		<IsEnsure>false</IsEnsure>
		<IsStall>false</IsStall>
		<IsAssert>true</IsAssert>
		<CrashType>Assert</CrashType>
		<ErrorMessage>Assertion failed: A &amp;&amp; B &lt;x&gt; &quot;q&quot; &apos;s&apos; &#65;&#x42;</ErrorMessage>
		<CrashReporterMessage />
		<CrashReporterMessage>Attended</CrashReporterMessage>
		<CrashReporterMessage />
		<SecondsSinceStart>413</SecondsSinceStart>
		<EngineVersion>5.6.1-0+++Sparta-Depot+Main+CL97404+1875</EngineVersion>
		<BuildVersion>++Sparta-Depot+Main+CL97404+1875-CL-0</BuildVersion>
		<!-- a comment <with> tags -->
		<Misc.CPUBrand>12th Gen Intel(R) Core(TM) i5-12400F</Misc.CPUBrand>
		<MemoryStats.bIsOOM>1</MemoryStats.bIsOOM>
		<CallStack />
		<PCallStack>

MortalShell2-Win64-Shipping 0x00006ffff05a0000 + 39ff154
UE4SS 0x00006fffef020000 + 446175
css_core-1.2.0 0x0000000180000000 + 1a2b
kernel32 0x00006fffffe70000 + 11669
</PCallStack>
		<PCallStackHash>3E908804A981C8A8F1F84CC40CE9BC003C69DCE6</PCallStackHash>
		<Threads>
			<Thread>
				<CallStack>ntdll 0x00006fffffef0000 + ea94
</CallStack>
				<IsCrashed>true</IsCrashed>
				<ThreadID>376</ThreadID>
				<ThreadName>GameThread</ThreadName>
			</Thread>
		</Threads>
		<GPUBreadcrumbs>
			<Source>RHI</Source>
			<Queue>
				<Name>Graphics Queue 0</Name>
				<Breadcrumbs>{{Frame 1},F,{{{Scene},F}}}{{Frame 2},N}</Breadcrumbs>
			</Queue>
			<Queue>
				<Name>AsyncCompute Queue 0</Name>
				<Breadcrumbs>{{Frame 22202},A,{{{SceneRender - ViewFamilies},A,{{{Done},F},{{Scene},A,{{{Nanite::VisBuffer},A,{{{Nanite::DrawGeometry},A,{{{MainPass},A},{{PostPass},A}}}}},{{NaniteBasePass},A},{{GPU KeyGen &amp; Sort},N}}}}}}}{{BufferPoolCopyOps},N}{{Frame 22203},N,{{{WorldTick},N}}}</Breadcrumbs>
			</Queue>
			<Queue>
				<Name>Copy Queue</Name>
				<Breadcrumbs>{{Frame 9},A,{{{Copy},A}}}</Breadcrumbs>
			</Queue>
		</GPUBreadcrumbs>
	</RuntimeProperties>
	<PlatformProperties>
		<PlatformIsRunningWine>true</PlatformIsRunningWine>
	</PlatformProperties>
	<EngineData>
		<RHI.RHIName>D3D12</RHI.RHIName>
		<GPUCrash.D3DDeviceRemovedReasonString>DXGI_ERROR_DEVICE_REMOVED</GPUCrash.D3DDeviceRemovedReasonString>
	</EngineData>
	<GameData>
		<DLSSG.Enable>1</DLSSG.Enable>
		<Settings.Upscaler>DLSS+NIS</Settings.Upscaler>
		<Player.Shell>CharacterId.Player.Darkform.CorruptedGenessa</Player.Shell>
	</GameData>
</FGenericCrashContext>
)";

void synthetic_xml() {
    fs::path p = wcmg_test::scratch_dir() / "ctx" / "CrashContext.runtime-xml";
    write_text(p, kXml);
    CrashReport r = parse_crash_context(p);
    CHECK_EQ(r.guid, std::string("UECC-Windows-TEST_0000"));
    CHECK_EQ(r.crash_type, std::string("Assert"));
    CHECK_EQ(r.error_message, std::string("Assertion failed: A && B <x> \"q\" 's' AB"));
    CHECK(r.is_assert);
    CHECK(!r.is_ensure);
    CHECK(!r.is_stall);
    CHECK(r.is_oom);
    CHECK_EQ(r.seconds_since_start, 413);
    CHECK_EQ(r.engine_version, std::string("5.6.1-0+++Sparta-Depot+Main+CL97404+1875"));
    CHECK_EQ(r.build_version, std::string("++Sparta-Depot+Main+CL97404+1875-CL-0"));
    CHECK_EQ(r.callstack_hash, std::string("3E908804A981C8A8F1F84CC40CE9BC003C69DCE6"));
    CHECK_EQ(r.frames.size(), size_t(4));
    if (r.frames.size() == 4) {
        CHECK_EQ(r.frames[0].module, std::string("MortalShell2-Win64-Shipping"));
        CHECK_EQ(r.frames[0].base, 0x00006ffff05a0000ull);
        CHECK_EQ(r.frames[0].offset, 0x39ff154ull);
        CHECK_EQ(r.frames[1].module, std::string("UE4SS"));
        CHECK_EQ(r.frames[2].module, std::string("css_core-1.2.0"));
        CHECK_EQ(r.frames[3].module, std::string("kernel32"));
        CHECK(!r.frames[0].from_stack_scan);
    }
    CHECK_EQ(r.props["CrashReporterMessage"], std::string("Attended"));
    CHECK_EQ(r.props["Misc.CPUBrand"], std::string("12th Gen Intel(R) Core(TM) i5-12400F"));
    CHECK_EQ(r.props["PlatformIsRunningWine"], std::string("true"));
    CHECK_EQ(r.props["RHI.RHIName"], std::string("D3D12"));
    CHECK_EQ(r.props["DLSSG.Enable"], std::string("1"));
    CHECK_EQ(r.props["Settings.Upscaler"], std::string("DLSS+NIS"));
    CHECK_EQ(r.props["Player.Shell"], std::string("CharacterId.Player.Darkform.CorruptedGenessa"));
    CHECK(r.props.count("CrashType") == 0);
    CHECK(r.props.count("PCallStack") == 0);
    CHECK(r.props.count("ThreadName") == 0);      // per-thread data stays out
    CHECK(r.props.count("CallStack") == 1);       // the empty crash-level one
    CHECK_EQ(r.props["GPUBreadcrumbs.ActiveQueue"], std::string("AsyncCompute Queue 0"));
    // First queue has nothing active; the second's oldest unfinished chain wins.
    std::vector<std::string> want = {"Frame 22202", "SceneRender - ViewFamilies", "Scene", "Nanite::VisBuffer",
                                     "Nanite::DrawGeometry", "MainPass"};
    CHECK(r.gpu_breadcrumb_tail == want);

    // Garbage and truncation must not throw and must not invent data.
    std::string full = kXml;
    for (size_t cut : {size_t(0), size_t(10), full.size() / 3, full.size() / 2, full.size() - 30}) {
        write_text(p, full.substr(0, cut));
        CrashReport t = parse_crash_context(p);
        (void)t;
    }
    write_text(p, "<a><b>{{x},A,{{{y},A,{{{z}");
    CrashReport junk = parse_crash_context(p);
    CHECK(junk.crash_type.empty());
    write_text(p, "<GPUBreadcrumbs><Queue><Breadcrumbs>{{x},A,{{{y},A,{{{z},A</Breadcrumbs></Queue></GPUBreadcrumbs>");
    CrashReport partial = parse_crash_context(p);
    CHECK((partial.gpu_breadcrumb_tail == std::vector<std::string>{"x", "y", "z"}));
    CHECK(parse_crash_context(wcmg_test::scratch_dir() / "nope.xml").crash_type.empty());
}

void synthetic_scan() {
    fs::path saved = wcmg_test::scratch_dir() / "Saved";
    std::error_code ec;
    fs::remove_all(saved, ec);
    fs::path older = saved / "Crashes" / "UECC-Windows-OLD_0000";
    fs::path newer = saved / "Crashes" / "UECC-Windows-NEW_0000";
    fs::path broken = saved / "Crashes" / "UECC-Windows-BROKEN_0000";
    write_text(older / "CrashContext.runtime-xml", kXml);
    write_text(newer / "CrashContext.runtime-xml",
               "<FGenericCrashContext><RuntimeProperties><CrashType>GPUCrash</CrashType></RuntimeProperties></FGenericCrashContext>");
    write_text(newer / "UEMinidump.dmp", "not a dump at all");
    fs::create_directories(broken);
    fs::create_directories(saved / "Crashes" / "SomethingElse");
    auto now = fs::file_time_type::clock::now();
    fs::last_write_time(older, now - std::chrono::hours(48));
    fs::last_write_time(broken, now - std::chrono::hours(24));
    fs::last_write_time(newer, now);

    Paths paths;
    paths.saved = saved;
    auto list = scan_crash_reports(paths, 20);
    CHECK_EQ(list.size(), size_t(3));
    if (list.size() == 3) {
        CHECK_EQ(list[0].crash_type, std::string("GPUCrash"));
        CHECK(list[0].has_minidump);
        CHECK_EQ(list[0].minidump_bytes, uint64_t(17));
        CHECK(!list[0].minidump_error.empty());
        CHECK_EQ(list[0].guid, std::string("UECC-Windows-NEW_0000"));
        CHECK_EQ(list[1].folder, broken);
        CHECK(!list[1].minidump_error.empty());
        CHECK_EQ(list[2].crash_type, std::string("Assert"));
        CHECK(!list[2].has_minidump);
        CHECK(list[0].when > list[2].when);
    }
    CHECK_EQ(scan_crash_reports(paths, 1).size(), size_t(1));
    Paths none;
    CHECK(scan_crash_reports(none).empty());
    none.saved = saved / "missing";
    CHECK(scan_crash_reports(none).empty());
}

std::string fr(const Frame& f) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s+%llx%s", f.module.empty() ? "?" : f.module.c_str(),
                  static_cast<unsigned long long>(f.offset), f.from_stack_scan ? "" : "*");
    return buf;
}

std::string short_mod(std::string m) {
    if (m == "MortalShell2-Win64-Shipping") return "Game";
    return m;
}

bool same(const Frame& a, const Frame& b) { return a.module == b.module && a.offset == b.offset; }

void real_reports() {
    auto saved = wcmg_test::env_path("WCMG_SAVED_DIR");
    if (!saved) { std::printf("crash_context: WCMG_SAVED_DIR unset, skipping real reports\n"); return; }
    size_t folders = 0;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(*saved / "Crashes", ec))
        if (e.is_directory() && e.path().filename().string().starts_with("UECC-")) ++folders;
    Paths paths;
    paths.saved = *saved;
    auto list = scan_crash_reports(paths, 1000);
    CHECK_EQ(list.size(), folders);
    for (size_t i = 1; i < list.size(); ++i) CHECK(list[i - 1].when >= list[i].when);

    std::map<std::string, int> types;
    int crash_outside = 0;
    int crash_n = 0, crash_match = 0, rip_exact = 0, with_frames = 0, gpu_with_tail = 0, gpu_n = 0;
    struct Agree { size_t xml_total = 0, xml_found = 0, scan_total = 0, scan_in_xml = 0; };
    std::map<std::string, Agree> by_type;
    size_t xml_total = 0, xml_found = 0, scan_total = 0, scan_in_xml = 0;
    int top3_any = 0, top3_n = 0;
    std::printf("\n%-10s %-8s %-28s | %-62s | %s\n", "type", "folder", "exception module", "XML frames 0..2",
                "scanned frames 0..2 (* = RIP)");
    for (const CrashReport& r : list) {
        CHECK(!r.crash_type.empty());
        ++types[r.crash_type];
        std::string xml3, scan3;
        for (size_t i = 0; i < 3 && i < r.frames.size(); ++i) {
            Frame f = r.frames[i];
            f.module = short_mod(f.module);
            xml3 += (i ? " " : "") + fr(f);
        }
        for (size_t i = 0; i < 3 && i < r.minidump_frames.size(); ++i) {
            Frame f = r.minidump_frames[i];
            f.module = short_mod(f.module);
            scan3 += (i ? " " : "") + fr(f);
        }
        std::string id = r.folder.filename().string().substr(13, 8);
        std::printf("%-10s %-8s %-28s | %-62s | %s%s\n", r.crash_type.c_str(), id.c_str(),
                    r.exception_module.empty() ? "-" : r.exception_module.c_str(), xml3.c_str(), scan3.c_str(),
                    r.minidump_error.empty() ? "" : ("  [" + r.minidump_error + "]").c_str());

        if (r.crash_type == "GPUCrash") {
            ++gpu_n;
            if (!r.gpu_breadcrumb_tail.empty()) ++gpu_with_tail;
        }
        if (r.frames.empty() || r.minidump_frames.empty()) continue;
        ++with_frames;
        if (!r.minidump_frames[0].from_stack_scan && same(r.minidump_frames[0], r.frames[0])) ++rip_exact;
        // Recall: XML frames (after the faulting one) that the scan also found, anywhere.
        Agree& a = by_type[r.crash_type];
        for (size_t i = 1; i < r.frames.size(); ++i) {
            ++xml_total;
            ++a.xml_total;
            for (const Frame& s : r.minidump_frames)
                if (same(s, r.frames[i])) { ++xml_found; ++a.xml_found; break; }
        }
        for (const Frame& s : r.minidump_frames) {
            if (!s.from_stack_scan) continue;
            ++scan_total;
            ++a.scan_total;
            for (const Frame& x : r.frames)
                if (same(s, x)) { ++scan_in_xml; ++a.scan_in_xml; break; }
        }
        ++top3_n;
        bool any = false;
        for (size_t i = 0; i < 3 && i < r.minidump_frames.size(); ++i)
            for (size_t j = 0; j < 3 && j < r.frames.size(); ++j)
                if (same(r.minidump_frames[i], r.frames[j])) any = true;
        if (any) ++top3_any;

        if (r.crash_type == "Crash") {
            ++crash_n;
            const std::string& first = r.frames[0].module;
            bool ok = r.exception_module == first ||
                      (first == "VCRUNTIME140" && r.frames.size() > 1 &&
                       (r.exception_module == "VCRUNTIME140" || r.exception_module == r.frames[1].module));
            // UE names an address past the end of every image after the nearest module below it
            // (seen: "Normaliz+6f00" for a 0x2000-byte Normaliz.dll). The dump has to prove that case.
            bool outside_every_image = false;
            if (!ok && r.exception_module.empty()) {
                Minidump m = read_minidump(r.minidump);
                uint64_t addr = r.frames[0].base + r.frames[0].offset;
                for (const DumpModule& d : m.modules)
                    if (d.base == r.frames[0].base && addr >= d.base + d.size && !m.module_at(addr) &&
                        r.exception_address == addr)
                        outside_every_image = true;
                if (outside_every_image) {
                    ++crash_outside;
                    std::printf("  %s: fault at %llx is outside every image; UE calls it %s+%llx\n",
                                r.folder.filename().string().c_str(), static_cast<unsigned long long>(addr),
                                first.c_str(), static_cast<unsigned long long>(r.frames[0].offset));
                }
            }
            ok = ok || outside_every_image;
            if (ok) ++crash_match;
            CHECK(ok);
            if (!ok)
                std::fprintf(stderr, "  %s: exception in %s, XML top frame %s\n", r.folder.filename().string().c_str(),
                             r.exception_module.c_str(), first.c_str());
        }
    }
    std::printf("\ncrash types:");
    for (auto& [k, v] : types) std::printf(" %s=%d", k.c_str(), v);
    std::printf("\nreports %zu, with both XML and scanned frames %d\n", list.size(), with_frames);
    std::printf("Crash: exception module agrees with XML top frame %d/%d (%d of them outside every image)\n", crash_match, crash_n, crash_outside);
    std::printf("RIP equals XML frame 0 (module+offset) %d/%d\n", rip_exact, with_frames);
    std::printf("XML frames 1..n found anywhere in the 64-frame scan: %zu/%zu\n", xml_found, xml_total);
    std::printf("scanned candidates that are real XML frames: %zu/%zu\n", scan_in_xml, scan_total);
    for (auto& [k, a] : by_type)
        std::printf("  %s: XML frames found by the scan %zu/%zu, scan candidates that are XML frames %zu/%zu\n",
                    k.c_str(), a.xml_found, a.xml_total, a.scan_in_xml, a.scan_total);
    std::printf("scanned top 3 share a frame with XML top 3: %d/%d\n", top3_any, top3_n);
    std::printf("GPUCrash with an active breadcrumb chain: %d/%d\n", gpu_with_tail, gpu_n);
    for (const CrashReport& r : list) {
        if (r.gpu_breadcrumb_tail.empty()) continue;
        std::printf("example breadcrumb tail (%s):", r.folder.filename().string().c_str());
        for (auto& s : r.gpu_breadcrumb_tail) std::printf(" > %s", s.c_str());
        std::printf("\n");
        break;
    }
}

}  // namespace

int main() {
    synthetic_xml();
    synthetic_scan();
    real_reports();
    return wcmg_test::finish("crash_context");
}

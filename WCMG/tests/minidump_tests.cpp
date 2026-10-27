#include "check.hpp"
#include "scanners.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

using namespace wcmg;
namespace fs = std::filesystem;

namespace {

// Builds a small but valid minidump in memory, the same shape dbghelp writes.
struct DumpWriter {
    std::vector<uint8_t> b;
    void u8(uint8_t v) { b.push_back(v); }
    void u16(uint16_t v) { for (int i = 0; i < 2; ++i) b.push_back(uint8_t(v >> (8 * i))); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }
    void u64(uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(uint8_t(v >> (8 * i))); }
    void zeros(size_t n) { b.insert(b.end(), n, 0); }
    uint32_t here() const { return uint32_t(b.size()); }
    void put32(uint32_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b[at + i] = uint8_t(v >> (8 * i)); }
    void put64(uint32_t at, uint64_t v) { for (int i = 0; i < 8; ++i) b[at + i] = uint8_t(v >> (8 * i)); }
    uint32_t wstr(const std::string& s) {  // MINIDUMP_STRING, ASCII only here
        uint32_t at = here();
        u32(uint32_t(s.size() * 2));
        for (char c : s) u16(uint16_t(c));
        u16(0);
        return at;
    }
};

constexpr uint64_t kExeBase = 0x140000000ull, kExeSize = 0x1000000;
constexpr uint64_t kSsBase = 0x7ff800000000ull, kSsSize = 0x200000;
constexpr uint64_t kStackStart = 0x10000, kRsp = kStackStart + 0x10;

struct Built { std::vector<uint8_t> bytes; uint32_t module_list_rva = 0; uint32_t stack_rva = 0; };

// with_thread_stack_rva=false leaves the thread's own descriptor empty so the reader
// has to find the stack through the memory list.
Built build_dump(bool with_thread_stack_rva = true, bool with_context = true, bool mem64 = false) {
    DumpWriter w;
    const uint32_t nstreams = 5;
    w.u32(0x504D444D);
    w.u32(0x0000A793);
    w.u32(nstreams);
    w.u32(32);
    w.u32(0); w.u32(0); w.u64(0);
    const uint32_t dir = w.here();
    w.zeros(nstreams * 12);
    auto set_dir = [&](uint32_t i, uint32_t type, uint32_t size, uint32_t rva) {
        w.put32(dir + i * 12, type);
        w.put32(dir + i * 12 + 4, size);
        w.put32(dir + i * 12 + 8, rva);
    };

    uint32_t exe_name = w.wstr("C:\\Games\\Sparta\\MortalShell2\\Binaries\\Win64\\MortalShell2-Win64-Shipping.exe");
    uint32_t ss_name = w.wstr("C:\\Games\\Sparta\\MortalShell2\\Binaries\\Win64\\ue4ss\\UE4SS.dll");

    // Module list
    uint32_t ml = w.here();
    w.u32(2);
    auto module = [&](uint64_t base, uint32_t size, uint32_t name_rva, bool version) {
        uint32_t at = w.here();
        w.u64(base); w.u32(size); w.u32(0x1234); w.u32(0x5F000000); w.u32(name_rva);
        w.zeros(52);
        if (version) {
            w.put32(at + 24, 0xFEEF04BD);
            w.put32(at + 32, (3u << 16) | 0u);
            w.put32(at + 36, (1u << 16) | 0u);
        }
        w.zeros(8 + 8 + 8 + 8);
    };
    module(kExeBase, uint32_t(kExeSize), exe_name, false);
    module(kSsBase, uint32_t(kSsSize), ss_name, true);
    set_dir(0, 4, w.here() - ml, ml);

    // Thread context (x64 CONTEXT is 0x4D0 bytes)
    uint32_t ctx = w.here();
    w.zeros(0x4D0);
    w.put64(ctx + 0x98, kRsp);
    w.put64(ctx + 0xF8, kSsBase + 0x10);

    // Stack bytes: the two qwords below RSP must be ignored, the rest is the planted chain.
    std::vector<uint64_t> stack(64, 0);
    stack[0] = kExeBase + 0x111;        // below rsp
    stack[1] = kExeBase + 0x222;        // below rsp
    stack[2] = 0x1234;                  // junk at rsp
    stack[3] = kExeBase + 0x500;
    stack[4] = kExeBase + 0x500;        // consecutive duplicate
    stack[5] = kSsBase + 0x40;
    stack[6] = kExeBase + kExeSize;     // one past the image: not a frame
    stack[7] = kExeBase + 0x900;
    uint32_t stack_rva = w.here();
    for (uint64_t q : stack) w.u64(q);
    const uint32_t stack_size = uint32_t(stack.size() * 8);

    // Thread list
    uint32_t tl = w.here();
    w.u32(1);
    w.u32(42); w.u32(0); w.u32(0x20); w.u32(0); w.u64(0x7000);
    w.u64(kStackStart); w.u32(with_thread_stack_rva ? stack_size : 0); w.u32(with_thread_stack_rva ? stack_rva : 0);
    w.u32(with_context ? 0x4D0 : 0); w.u32(with_context ? ctx : 0);
    set_dir(1, 3, w.here() - tl, tl);

    // Memory list, or the full-memory Memory64 form UE4SS writes
    uint32_t mem = w.here();
    if (mem64) {
        w.u64(1); w.u64(stack_rva);
        w.u64(kStackStart); w.u64(stack_size);
        set_dir(2, 9, w.here() - mem, mem);
    } else {
        w.u32(1);
        w.u64(kStackStart); w.u32(stack_size); w.u32(stack_rva);
        set_dir(2, 5, w.here() - mem, mem);
    }

    // Exception stream
    uint32_t ex = w.here();
    w.u32(42); w.u32(0);
    w.u32(0xC0000005); w.u32(0); w.u64(0); w.u64(kSsBase + 0x10); w.u32(2); w.u32(0);
    w.u64(0); w.u64(0xa8);
    w.zeros(13 * 8);
    w.u32(with_context ? 0x4D0 : 0); w.u32(with_context ? ctx : 0);
    set_dir(3, 6, w.here() - ex, ex);

    // System info
    uint32_t si = w.here();
    w.u16(9); w.u16(6); w.u16(0); w.u8(12); w.u8(1);
    w.u32(10); w.u32(0); w.u32(19045); w.u32(2); w.u32(0);
    w.zeros(32);
    set_dir(4, 7, w.here() - si, si);

    return {w.b, ml, stack_rva};
}

fs::path write_file(const std::string& name, const std::vector<uint8_t>& bytes, size_t len) {
    fs::path p = wcmg_test::scratch_dir() / name;
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(std::min(len, bytes.size())));
    return p;
}

void synthetic() {
    Built d = build_dump();
    fs::path p = write_file("synthetic.dmp", d.bytes, d.bytes.size());
    Minidump m = read_minidump(p);
    CHECK(m.ok());
    CHECK_EQ(m.modules.size(), size_t(2));
    if (m.modules.size() == 2) {
        CHECK_EQ(m.modules[0].base, kExeBase);
        CHECK_EQ(m.modules[1].size, kSsSize);
        CHECK(m.modules[1].name.ends_with("\\UE4SS.dll"));
        CHECK_EQ(m.modules[1].file_version, std::string("3.0.1.0"));
        CHECK(m.modules[0].file_version.empty());
        CHECK_EQ(m.modules[0].timestamp, 0x5F000000u);
    }
    CHECK_EQ(m.exception_thread_id, 42u);
    CHECK_EQ(m.exception_code, 0xC0000005u);
    CHECK_EQ(m.exception_address, kSsBase + 0x10);
    CHECK_EQ(m.exception_rip, kSsBase + 0x10);
    CHECK_EQ(m.exception_rsp, kRsp);
    CHECK_EQ(m.exception_params.size(), size_t(2));
    CHECK_EQ(m.system_info, std::string("Windows 10.0.19045 x64"));

    CHECK(m.module_at(kExeBase) != nullptr);
    CHECK(m.module_at(kExeBase + kExeSize) == nullptr);
    CHECK(m.module_at(kExeBase - 1) == nullptr);
    Frame f = m.frame_for(kSsBase + 0x1234);
    CHECK_EQ(f.module, std::string("UE4SS"));
    CHECK_EQ(f.offset, 0x1234ull);
    CHECK_EQ(m.frame_for(kExeBase).module, std::string("MortalShell2-Win64-Shipping"));
    CHECK(m.frame_for(0x42).module.empty());

    auto check_frames = [&](const std::vector<Frame>& fr) {
        CHECK_EQ(fr.size(), size_t(4));
        if (fr.size() != 4) return;
        CHECK(!fr[0].from_stack_scan);
        CHECK_EQ(fr[0].module, std::string("UE4SS"));
        CHECK_EQ(fr[0].offset, 0x10ull);
        CHECK(fr[1].from_stack_scan);
        CHECK_EQ(fr[1].module, std::string("MortalShell2-Win64-Shipping"));
        CHECK_EQ(fr[1].offset, 0x500ull);
        CHECK_EQ(fr[2].module, std::string("UE4SS"));
        CHECK_EQ(fr[2].offset, 0x40ull);
        CHECK_EQ(fr[3].offset, 0x900ull);
    };
    check_frames(scan_stack(m, p, 64));
    CHECK_EQ(scan_stack(m, p, 2).size(), size_t(2));
    CHECK(scan_stack(m, p, 0).empty());

    // Stack found through the memory list instead of the thread descriptor.
    Built d2 = build_dump(false);
    fs::path p2 = write_file("synthetic-memlist.dmp", d2.bytes, d2.bytes.size());
    Minidump m2 = read_minidump(p2);
    CHECK(m2.ok());
    check_frames(scan_stack(m2, p2, 64));

    // Same, through a Memory64 list.
    Built d4 = build_dump(false, true, true);
    fs::path p4 = write_file("synthetic-mem64.dmp", d4.bytes, d4.bytes.size());
    Minidump m4 = read_minidump(p4);
    CHECK(m4.ok());
    check_frames(scan_stack(m4, p4, 64));

    // No context: no RIP frame, scan from the start of the stack (so the two low qwords count).
    Built d3 = build_dump(true, false);
    fs::path p3 = write_file("synthetic-noctx.dmp", d3.bytes, d3.bytes.size());
    Minidump m3 = read_minidump(p3);
    CHECK(m3.ok());
    auto fr3 = scan_stack(m3, p3, 64);
    CHECK_EQ(fr3.size(), size_t(5));
    if (!fr3.empty()) {
        CHECK(fr3[0].from_stack_scan);
        CHECK_EQ(fr3[0].offset, 0x111ull);
    }

    CrashReport r;
    attach_minidump(r, p);
    CHECK(r.minidump_error.empty());
    CHECK_EQ(r.loaded_modules.size(), size_t(2));
    if (r.loaded_modules.size() == 2) {
        CHECK_EQ(r.loaded_modules[0], std::string("MortalShell2-Win64-Shipping.exe"));
        CHECK_EQ(r.loaded_modules[1], std::string("UE4SS.dll"));
    }
    CHECK_EQ(r.exception_module, std::string("UE4SS"));
    CHECK_EQ(r.exception_code, 0xC0000005ull);
    CHECK_EQ(r.minidump_frames.size(), size_t(4));
}

void truncated_and_garbage() {
    Built d = build_dump();
    // Every prefix of the file: nothing may crash, and any cut inside the module list must say so.
    int cut_errors = 0;
    for (size_t len = 0; len < d.bytes.size(); ++len) {
        fs::path p = write_file("cut.dmp", d.bytes, len);
        Minidump m = read_minidump(p);
        auto fr = scan_stack(m, p, 64);
        CrashReport r;
        attach_minidump(r, p);
        if (len < 32) CHECK(!m.ok());
        if (len > d.module_list_rva && len < d.module_list_rva + 4 + 2 * 108) {
            CHECK(!m.ok());
            CHECK(!r.minidump_error.empty());
        }
        if (!m.ok()) ++cut_errors;
        (void)fr;
    }
    CHECK(cut_errors > 0);

    CHECK(!read_minidump(wcmg_test::scratch_dir() / "does-not-exist.dmp").ok());
    {
        CrashReport r;
        attach_minidump(r, wcmg_test::scratch_dir() / "does-not-exist.dmp");
        CHECK(!r.minidump_error.empty());
    }
    std::vector<uint8_t> text = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!', 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(!read_minidump(write_file("text.dmp", text, text.size())).ok());

    // Random bytes behind a real header and directory, and random directories.
    std::mt19937 rng(1234);
    for (int round = 0; round < 400; ++round) {
        std::vector<uint8_t> g = d.bytes;
        size_t from = round % 2 ? 32 : 92;
        for (size_t i = from; i < g.size(); ++i)
            if (rng() % 7 == 0) g[i] = uint8_t(rng());
        fs::path p = write_file("garbage.dmp", g, g.size());
        Minidump m = read_minidump(p);
        auto fr = scan_stack(m, p, 64);
        CrashReport r;
        attach_minidump(r, p);
        CHECK(fr.size() <= 64);
    }
}

void real_dumps() {
    auto saved = wcmg_test::env_path("WCMG_SAVED_DIR");
    if (!saved) { std::printf("minidump: WCMG_SAVED_DIR unset, skipping real dumps\n"); return; }
    fs::path crashes = *saved / "Crashes";
    std::error_code ec;
    int n = 0, empty = 0, parsed = 0, with_exc = 0, with_ctx = 0, with_ss = 0;
    for (auto& e : fs::directory_iterator(crashes, ec)) {
        fs::path dmp = e.path() / "UEMinidump.dmp";
        if (!fs::is_regular_file(dmp)) continue;
        ++n;
        Minidump m = read_minidump(dmp);
        // A crash reporter killed mid-write leaves a 0-byte dump; that must be an error, not a parse.
        if (fs::file_size(dmp) == 0) {
            ++empty;
            CHECK(!m.ok());
            continue;
        }
        CHECK(m.ok());
        if (!m.ok()) { std::fprintf(stderr, "  %s: %s\n", dmp.string().c_str(), m.error.c_str()); continue; }
        ++parsed;
        bool exe = false, ss = false;
        for (auto& mod : m.modules) {
            std::string f = mod.name.substr(mod.name.find_last_of("\\/") + 1);
            if (f == "MortalShell2-Win64-Shipping.exe") exe = true;
            if (f == "UE4SS.dll") ss = true;
        }
        CHECK(exe);
        if (ss) ++with_ss;
        else std::printf("  no UE4SS.dll loaded: %s\n", e.path().filename().string().c_str());
        if (m.exception_code) ++with_exc;
        if (m.exception_rip) ++with_ctx;
        CHECK(!m.system_info.empty());
        CHECK(!scan_stack(m, dmp, 64).empty());
    }
    std::printf("minidump: %d real dumps, %d empty, %d parsed, %d with UE4SS.dll, %d with an exception record, "
                "%d with a register context\n", n, empty, parsed, with_ss, with_exc, with_ctx);
    CHECK(parsed > 0);
    // A vanilla run without UE4SS is allowed, but it should be the exception.
    CHECK(with_ss * 10 >= parsed * 9);
}

}  // namespace

int main() {
    synthetic();
    truncated_and_garbage();
    real_dumps();
    return wcmg_test::finish("minidump");
}

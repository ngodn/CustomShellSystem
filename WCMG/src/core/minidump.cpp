// Windows minidump reader (UEMinidump.dmp, UE4SS crash_*.dmp).
//
// Layouts follow the public dbghelp structs (minidumpapiset.h). Every read goes
// through Blob::get, which checks the range first, so a truncated or random file
// ends in Minidump::error and never in an out-of-range read.
#include "scanners.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>

namespace wcmg {
namespace {

constexpr uint32_t kSignature = 0x504D444D;  // "MDMP"
constexpr uint32_t kVersionLow = 0xA793;
constexpr uint64_t kMaxDumpBytes = 512ull * 1024 * 1024;

enum : uint32_t {
    ThreadListStream = 3,
    ModuleListStream = 4,
    MemoryListStream = 5,
    ExceptionStream = 6,
    SystemInfoStream = 7,
    Memory64ListStream = 9,
};

constexpr uint32_t kModuleSize = 108;
constexpr uint32_t kThreadSize = 48;
constexpr uint32_t kCtxRsp = 0x98;
constexpr uint32_t kCtxRip = 0xF8;

struct Blob {
    const uint8_t* p = nullptr;
    uint64_t n = 0;
    bool has(uint64_t off, uint64_t len) const { return off <= n && len <= n - off; }
    template <class T>
    bool get(uint64_t off, T& v) const {
        if (!has(off, sizeof(T))) return false;
        std::memcpy(&v, p + off, sizeof(T));  // dumps are little-endian, like every target we build for
        return true;
    }
};

struct Range { uint64_t start = 0, size = 0, rva = 0; };

struct ThreadRec {
    uint32_t id = 0;
    Range stack;
    uint32_t ctx_size = 0, ctx_rva = 0;
};

struct Parsed {
    Minidump md;
    std::vector<ThreadRec> threads;
    std::vector<Range> memory;   // both memory lists, each with its file offset
    uint32_t exc_ctx_size = 0, exc_ctx_rva = 0;
    bool have_exception = false;
    bool have_context = false;
};

void note(Minidump& m, const std::string& what) {
    if (m.error.empty()) m.error = what;
}

// How many fixed-size entries after a header of `header` bytes really fit, both in
// the stream's declared size and in the file.
uint64_t entries_that_fit(const Blob& b, uint64_t rva, uint64_t size, uint64_t header, uint64_t entry) {
    if (rva > b.n) return 0;
    uint64_t avail = std::min(size, b.n - rva);
    return avail < header ? 0 : (avail - header) / entry;
}

std::string utf16_to_utf8(const uint8_t* p, size_t bytes) {
    std::string out;
    size_t n = bytes / 2;
    auto unit = [&](size_t i) { return static_cast<uint32_t>(p[2 * i] | (p[2 * i + 1] << 8)); };
    for (size_t i = 0; i < n; ++i) {
        uint32_t c = unit(i);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < n) {
            uint32_t d = unit(i + 1);
            if (d >= 0xDC00 && d <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00);
                ++i;
            } else {
                c = 0xFFFD;
            }
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            c = 0xFFFD;
        }
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

bool read_string(const Blob& b, uint32_t rva, std::string& out) {
    uint32_t len = 0;
    if (!b.get(rva, len)) return false;
    if (len > 64 * 1024 || !b.has(uint64_t(rva) + 4, len)) return false;
    out = utf16_to_utf8(b.p + rva + 4, len);
    return true;
}

// "C:\\Games\\x\\UE4SS.dll" -> "UE4SS.dll"
std::string file_name_of(const std::string& full) {
    size_t s = full.find_last_of("\\/");
    return s == std::string::npos ? full : full.substr(s + 1);
}

// "UE4SS.dll" -> "UE4SS", matching what UE prints in PCallStack.
std::string module_stem(const std::string& full) {
    std::string f = file_name_of(full);
    size_t dot = f.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        std::string ext = f.substr(dot + 1);
        for (char& c : ext) c = static_cast<char>((c >= 'A' && c <= 'Z') ? c + 32 : c);
        if (ext == "dll" || ext == "exe" || ext == "drv" || ext == "sys" || ext == "ocx") f.resize(dot);
    }
    return f;
}

void parse_modules(const Blob& b, uint64_t rva, uint64_t size, Minidump& m) {
    uint32_t count = 0;
    if (!b.get(rva, count)) { note(m, "module list is outside the file"); return; }
    uint64_t need = 4 + uint64_t(count) * kModuleSize;
    if (need > size || !b.has(rva, need)) {
        note(m, "module list is truncated");
        count = static_cast<uint32_t>(std::min<uint64_t>(count, entries_that_fit(b, rva, size, 4, kModuleSize)));
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t at = rva + 4 + uint64_t(i) * kModuleSize;
        DumpModule mod;
        uint32_t size32 = 0, name_rva = 0;
        if (!b.get(at, mod.base) || !b.get(at + 8, size32) || !b.get(at + 12, mod.checksum) ||
            !b.get(at + 16, mod.timestamp) || !b.get(at + 20, name_rva))
            break;
        mod.size = size32;
        if (!read_string(b, name_rva, mod.name)) note(m, "a module name is outside the file");
        uint32_t sig = 0, ms = 0, ls = 0;
        if (b.get(at + 24, sig) && sig == 0xFEEF04BD && b.get(at + 32, ms) && b.get(at + 36, ls)) {
            mod.file_version = std::to_string(ms >> 16) + "." + std::to_string(ms & 0xFFFF) + "." +
                               std::to_string(ls >> 16) + "." + std::to_string(ls & 0xFFFF);
        }
        m.modules.push_back(std::move(mod));
    }
}

void parse_threads(const Blob& b, uint64_t rva, uint64_t size, Parsed& out) {
    uint32_t count = 0;
    if (!b.get(rva, count)) { note(out.md, "thread list is outside the file"); return; }
    uint64_t need = 4 + uint64_t(count) * kThreadSize;
    if (need > size || !b.has(rva, need)) {
        note(out.md, "thread list is truncated");
        count = static_cast<uint32_t>(std::min<uint64_t>(count, entries_that_fit(b, rva, size, 4, kThreadSize)));
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t at = rva + 4 + uint64_t(i) * kThreadSize;
        ThreadRec t;
        uint32_t ssize = 0, srva = 0;
        if (!b.get(at, t.id) || !b.get(at + 24, t.stack.start) || !b.get(at + 32, ssize) ||
            !b.get(at + 36, srva) || !b.get(at + 40, t.ctx_size) || !b.get(at + 44, t.ctx_rva))
            break;
        t.stack.size = ssize;
        t.stack.rva = srva;
        out.threads.push_back(t);
    }
}

void parse_memory_list(const Blob& b, uint64_t rva, uint64_t size, Parsed& out) {
    uint32_t count = 0;
    if (!b.get(rva, count)) { note(out.md, "memory list is outside the file"); return; }
    uint64_t need = 4 + uint64_t(count) * 16;
    if (need > size || !b.has(rva, need)) {
        note(out.md, "memory list is truncated");
        count = static_cast<uint32_t>(std::min<uint64_t>(count, entries_that_fit(b, rva, size, 4, 16)));
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t at = rva + 4 + uint64_t(i) * 16;
        Range r;
        uint32_t ds = 0, drva = 0;
        if (!b.get(at, r.start) || !b.get(at + 8, ds) || !b.get(at + 12, drva)) break;
        r.size = ds;
        r.rva = drva;
        out.memory.push_back(r);
    }
}

void parse_memory64_list(const Blob& b, uint64_t rva, uint64_t size, Parsed& out) {
    uint64_t count = 0, base_rva = 0;
    if (!b.get(rva, count) || !b.get(rva + 8, base_rva)) { note(out.md, "memory64 list is outside the file"); return; }
    uint64_t max_fit = entries_that_fit(b, rva, size, 16, 16);
    if (count > max_fit) {
        note(out.md, "memory64 list is truncated");
        count = max_fit;
    }
    uint64_t data = base_rva;
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t at = rva + 16 + i * 16;
        Range r;
        if (!b.get(at, r.start) || !b.get(at + 8, r.size)) break;
        r.rva = data;
        out.memory.push_back(r);
        if (r.size > UINT64_MAX - data) break;
        data += r.size;
    }
}

void parse_exception(const Blob& b, uint64_t rva, uint64_t size, Parsed& out) {
    Minidump& m = out.md;
    if (size < 168 || !b.has(rva, 168)) { note(m, "exception stream is truncated"); return; }
    uint32_t nparams = 0;
    b.get(rva, m.exception_thread_id);
    b.get(rva + 8, m.exception_code);
    b.get(rva + 24, m.exception_address);
    b.get(rva + 32, nparams);
    nparams = std::min<uint32_t>(nparams, 15);
    for (uint32_t i = 0; i < nparams; ++i) {
        uint64_t v = 0;
        b.get(rva + 40 + 8ull * i, v);
        m.exception_params.push_back(v);
    }
    b.get(rva + 160, out.exc_ctx_size);
    b.get(rva + 164, out.exc_ctx_rva);
    out.have_exception = true;
}

void parse_system_info(const Blob& b, uint64_t rva, uint64_t size, Minidump& m) {
    if (size < 24 || !b.has(rva, 24)) { note(m, "system info stream is truncated"); return; }
    uint16_t arch = 0;
    uint32_t major = 0, minor = 0, build = 0;
    b.get(rva, arch);
    b.get(rva + 8, major);
    b.get(rva + 12, minor);
    b.get(rva + 16, build);
    const char* a = "unknown arch";
    switch (arch) {
        case 9: a = "x64"; break;
        case 0: a = "x86"; break;
        case 12: a = "arm64"; break;
        case 5: a = "arm"; break;
        default: break;
    }
    m.system_info = "Windows " + std::to_string(major) + "." + std::to_string(minor) + "." +
                    std::to_string(build) + " " + a;
}

bool read_context(const Blob& b, uint32_t size, uint32_t rva, uint64_t& rip, uint64_t& rsp) {
    if (size < kCtxRip + 8 || !b.has(rva, size)) return false;
    return b.get(uint64_t(rva) + kCtxRip, rip) && b.get(uint64_t(rva) + kCtxRsp, rsp);
}

Parsed parse_blob(const Blob& b) {
    Parsed out;
    Minidump& m = out.md;
    uint32_t sig = 0, version = 0, nstreams = 0, dir_rva = 0;
    if (!b.get(0, sig) || !b.get(4, version) || !b.get(8, nstreams) || !b.get(12, dir_rva)) {
        m.error = "file is too small to be a minidump";
        return out;
    }
    if (sig != kSignature) { m.error = "not a minidump (no MDMP signature)"; return out; }
    if ((version & 0xFFFF) != kVersionLow) { m.error = "unexpected minidump version"; return out; }
    if (nstreams > 4096 || !b.has(dir_rva, uint64_t(nstreams) * 12)) {
        m.error = "minidump stream directory is truncated";
        return out;
    }
    for (uint32_t i = 0; i < nstreams; ++i) {
        uint64_t at = dir_rva + uint64_t(i) * 12;
        uint32_t type = 0, size = 0, rva = 0;
        b.get(at, type);
        b.get(at + 4, size);
        b.get(at + 8, rva);
        if (type == 0) continue;  // UnusedStream
        bool known = type == ThreadListStream || type == ModuleListStream || type == MemoryListStream ||
                     type == ExceptionStream || type == SystemInfoStream || type == Memory64ListStream;
        if (!known) continue;
        // Memory64List gives only its header size in the directory; data follows elsewhere.
        if (!b.has(rva, std::min<uint64_t>(size, 4))) {
            note(m, "a minidump stream lies past the end of the file");
            continue;
        }
        switch (type) {
            case ThreadListStream: parse_threads(b, rva, size, out); break;
            case ModuleListStream: parse_modules(b, rva, size, m); break;
            case MemoryListStream: parse_memory_list(b, rva, size, out); break;
            case ExceptionStream: parse_exception(b, rva, size, out); break;
            case SystemInfoStream: parse_system_info(b, rva, size, m); break;
            case Memory64ListStream: parse_memory64_list(b, rva, size, out); break;
            default: break;
        }
    }
    // Register context: the exception record's copy first, then the thread's own.
    uint64_t rip = 0, rsp = 0;
    if (out.have_exception && read_context(b, out.exc_ctx_size, out.exc_ctx_rva, rip, rsp)) {
        out.have_context = true;
    } else {
        for (const ThreadRec& t : out.threads) {
            if (out.have_exception && t.id != m.exception_thread_id) continue;
            if (read_context(b, t.ctx_size, t.ctx_rva, rip, rsp)) out.have_context = true;
            break;
        }
    }
    if (out.have_context) {
        m.exception_rip = rip;
        m.exception_rsp = rsp;
    }
    return out;
}

bool load_file(const fs::path& path, std::vector<uint8_t>& buf, std::string& error) {
    std::error_code ec;
    uint64_t size = fs::file_size(path, ec);
    if (ec) { error = "cannot read the minidump: " + ec.message(); return false; }
    if (size == 0) { error = "minidump is empty (0 bytes), the crash reporter did not finish writing it"; return false; }
    if (size > kMaxDumpBytes) { error = "minidump is larger than 512 MB, not read"; return false; }
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open the minidump"; return false; }
    buf.resize(static_cast<size_t>(size));
    if (size && !f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size))) {
        // The file may have shrunk while we read it; keep what arrived.
        buf.resize(static_cast<size_t>(f.gcount()));
    }
    return true;
}

std::vector<Frame> scan_parsed(const Minidump& md, const Parsed& p, const Blob& b, size_t max_frames) {
    std::vector<Frame> out;
    if (max_frames == 0) return out;
    uint64_t last = 0;
    bool have_last = false;
    auto push = [&](uint64_t v, bool scanned) {
        if (have_last && v == last) return;
        Frame f = md.frame_for(v);
        f.from_stack_scan = scanned;
        out.push_back(std::move(f));
        last = v;
        have_last = true;
    };
    if (p.have_context) push(md.exception_rip, false);

    const ThreadRec* t = nullptr;
    for (const ThreadRec& tr : p.threads)
        if (tr.id == md.exception_thread_id) { t = &tr; break; }
    if (!t && !p.threads.empty()) t = &p.threads.front();
    if (!t) return out;

    // The thread's own descriptor usually points straight at its stack bytes; when it
    // does not (full-memory dumps), look the range up in the memory lists.
    Range stack;
    bool found = false;
    uint64_t anchor = p.have_context ? md.exception_rsp : t->stack.start;
    if (t->stack.size && t->stack.rva && b.has(t->stack.rva, t->stack.size) &&
        (!p.have_context || (anchor >= t->stack.start && anchor - t->stack.start < t->stack.size))) {
        stack = t->stack;
        found = true;
    }
    if (!found) {
        uint64_t want = anchor ? anchor : t->stack.start;
        for (const Range& r : p.memory) {
            if (want >= r.start && want - r.start < r.size) { stack = r; found = true; break; }
        }
    }
    if (!found && t->stack.size && b.has(t->stack.rva, t->stack.size)) {
        stack = t->stack;
        found = true;
    }
    if (!found) return out;

    uint64_t avail = b.has(stack.rva, 0) ? std::min<uint64_t>(stack.size, b.n - stack.rva) : 0;
    uint64_t off = 0;
    if (p.have_context && md.exception_rsp >= stack.start && md.exception_rsp - stack.start < stack.size)
        off = md.exception_rsp - stack.start;
    for (; off + 8 <= avail && out.size() < max_frames; off += 8) {
        uint64_t v = 0;
        if (!b.get(stack.rva + off, v)) break;
        if (md.module_at(v)) push(v, true);
    }
    if (out.size() > max_frames) out.resize(max_frames);
    return out;
}

}  // namespace

const DumpModule* Minidump::module_at(uint64_t address) const {
    for (const DumpModule& m : modules)
        if (address >= m.base && address - m.base < m.size) return &m;
    return nullptr;
}

Frame Minidump::frame_for(uint64_t address) const {
    Frame f;
    if (const DumpModule* m = module_at(address)) {
        f.module = module_stem(m->name);
        f.base = m->base;
        f.offset = address - m->base;
    } else {
        f.offset = address;
    }
    return f;
}

Minidump read_minidump(const fs::path& path) {
    try {
        std::vector<uint8_t> buf;
        std::string err;
        if (!load_file(path, buf, err)) {
            Minidump m;
            m.error = err;
            return m;
        }
        Blob b{buf.data(), buf.size()};
        return parse_blob(b).md;
    } catch (const std::exception& e) {
        Minidump m;
        m.error = std::string("minidump read failed: ") + e.what();
        return m;
    } catch (...) {
        Minidump m;
        m.error = "minidump read failed";
        return m;
    }
}

std::vector<Frame> scan_stack(const Minidump& dump, const fs::path& dump_file, size_t max_frames) {
    try {
        std::vector<uint8_t> buf;
        std::string err;
        if (!load_file(dump_file, buf, err)) return {};
        Blob b{buf.data(), buf.size()};
        Parsed p = parse_blob(b);
        // Use the caller's module list and exception facts; the rest comes from the file.
        const Minidump& md = dump.modules.empty() ? p.md : dump;
        return scan_parsed(md, p, b, max_frames);
    } catch (...) {
        return {};
    }
}

void attach_minidump(CrashReport& report, const fs::path& dump_file) {
    try {
        std::vector<uint8_t> buf;
        std::string err;
        if (!load_file(dump_file, buf, err)) {
            report.minidump_error = err;
            return;
        }
        Blob b{buf.data(), buf.size()};
        Parsed p = parse_blob(b);
        const Minidump& md = p.md;
        report.loaded_modules.clear();
        for (const DumpModule& m : md.modules) report.loaded_modules.push_back(file_name_of(m.name));
        report.exception_code = md.exception_code;
        report.exception_address = md.exception_address;
        uint64_t where = md.exception_address ? md.exception_address : md.exception_rip;
        if (const DumpModule* m = md.module_at(where)) report.exception_module = module_stem(m->name);
        report.minidump_frames = scan_parsed(md, p, b, 64);
        if (!md.ok()) report.minidump_error = md.error;
    } catch (const std::exception& e) {
        report.minidump_error = std::string("minidump read failed: ") + e.what();
    } catch (...) {
        report.minidump_error = "minidump read failed";
    }
}

}  // namespace wcmg

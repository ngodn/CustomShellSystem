// Saved/Crashes/UECC-* folders: CrashContext.runtime-xml plus the minidump beside it.
//
// The crash context is a flat FGenericCrashContext XML written by the engine. It is
// simple enough that a tiny hand-rolled reader is safer than pulling in a library:
// no attributes we care about, no namespaces, text-only leaves.
#include "scanners.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <system_error>

namespace wcmg {
namespace {

// ---------------------------------------------------------------------------
// Text helpers

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string trim_copy(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return std::string(s.substr(a, b - a));
}

void append_utf8(std::string& out, uint32_t cp) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

std::string unescape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') { out += s[i]; continue; }
        size_t semi = s.find(';', i + 1);
        if (semi == std::string_view::npos || semi - i > 12) { out += s[i]; continue; }
        std::string_view ent = s.substr(i + 1, semi - i - 1);
        if (ent == "amp") out += '&';
        else if (ent == "lt") out += '<';
        else if (ent == "gt") out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (ent.size() > 1 && ent[0] == '#') {
            uint32_t cp = 0;
            const char* b = ent.data() + 1;
            const char* e = ent.data() + ent.size();
            int base = 10;
            if (b < e && (*b == 'x' || *b == 'X')) { ++b; base = 16; }
            auto r = std::from_chars(b, e, cp, base);
            if (r.ec != std::errc() || r.ptr != e) { out += s[i]; continue; }
            append_utf8(out, cp);
        } else {
            out += s[i];
            continue;
        }
        i = semi;
    }
    return out;
}

bool parse_hex(std::string_view s, uint64_t& v) {
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.remove_prefix(2);
    if (s.empty()) return false;
    auto r = std::from_chars(s.data(), s.data() + s.size(), v, 16);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

bool truthy(const std::string& v) {
    if (v == "1") return true;
    if (v.size() == 4) {
        std::string l = v;
        for (char& c : l) c = static_cast<char>((c >= 'A' && c <= 'Z') ? c + 32 : c);
        return l == "true";
    }
    return false;
}

std::string read_file_capped(const fs::path& p, size_t cap) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::string s;
    char buf[64 * 1024];
    while (f && s.size() < cap) {
        f.read(buf, static_cast<std::streamsize>(std::min(sizeof buf, cap - s.size())));
        s.append(buf, static_cast<size_t>(f.gcount()));
    }
    return s;
}

// ---------------------------------------------------------------------------
// Minimal XML walk: calls on_leaf(ancestors, name, text) for every element that
// has no child elements. Attributes are skipped, comments and PIs ignored, CDATA
// kept as text. Malformed input just ends the walk early.

struct XmlLeafSink {
    virtual ~XmlLeafSink() = default;
    virtual void leaf(const std::vector<std::string>& ancestors, const std::string& name, std::string text) = 0;
};

void walk_xml(std::string_view x, XmlLeafSink& sink) {
    struct Open { std::string name; bool has_children = false; std::string text; };
    std::vector<Open> stack;
    std::vector<std::string> ancestors;
    size_t i = 0;
    const size_t n = x.size();
    if (n >= 3 && static_cast<unsigned char>(x[0]) == 0xEF && static_cast<unsigned char>(x[1]) == 0xBB &&
        static_cast<unsigned char>(x[2]) == 0xBF)
        i = 3;
    auto ancestors_of_top = [&]() {
        ancestors.clear();
        for (size_t k = 0; k + 1 < stack.size(); ++k) ancestors.push_back(stack[k].name);
    };
    while (i < n) {
        size_t lt = x.find('<', i);
        if (lt == std::string_view::npos) break;
        if (!stack.empty() && lt > i) stack.back().text.append(x.substr(i, lt - i));
        i = lt;
        if (x.compare(i, 4, "<!--") == 0) {
            size_t e = x.find("-->", i + 4);
            if (e == std::string_view::npos) break;
            i = e + 3;
            continue;
        }
        if (x.compare(i, 9, "<![CDATA[") == 0) {
            size_t e = x.find("]]>", i + 9);
            if (e == std::string_view::npos) break;
            // CDATA content is literal; escape '&' so the later unescape leaves it alone.
            if (!stack.empty()) {
                for (char c : x.substr(i + 9, e - i - 9)) {
                    if (c == '&') stack.back().text += "&amp;";
                    else stack.back().text += c;
                }
            }
            i = e + 3;
            continue;
        }
        if (x.compare(i, 2, "<?") == 0 || x.compare(i, 2, "<!") == 0) {
            size_t e = x.find('>', i + 2);
            if (e == std::string_view::npos) break;
            i = e + 1;
            continue;
        }
        size_t gt = x.find('>', i + 1);
        if (gt == std::string_view::npos) break;
        std::string_view inner = x.substr(i + 1, gt - i - 1);
        i = gt + 1;
        if (!inner.empty() && inner[0] == '/') {
            std::string name = trim_copy(inner.substr(1));
            // Pop to the matching open tag; tolerate a stray close.
            auto it = std::find_if(stack.rbegin(), stack.rend(), [&](const Open& o) { return o.name == name; });
            if (it == stack.rend()) continue;
            while (!stack.empty()) {
                Open top = std::move(stack.back());
                ancestors_of_top();
                stack.pop_back();
                bool match = top.name == name;
                if (!top.has_children) sink.leaf(ancestors, top.name, unescape(trim_copy(top.text)));
                if (match) break;
            }
            continue;
        }
        bool self_closing = !inner.empty() && inner.back() == '/';
        if (self_closing) inner.remove_suffix(1);
        size_t name_end = 0;
        while (name_end < inner.size() && !is_space(inner[name_end])) ++name_end;
        std::string name(inner.substr(0, name_end));
        if (name.empty()) continue;
        if (!stack.empty()) stack.back().has_children = true;
        if (self_closing) {
            stack.push_back({name, false, {}});
            ancestors_of_top();
            stack.pop_back();
            sink.leaf(ancestors, name, std::string());
        } else {
            stack.push_back({name, false, {}});
        }
    }
}

// ---------------------------------------------------------------------------
// GPU breadcrumbs: {{name},state[,{child,child,...}]} nodes, siblings at the top
// level are concatenated without commas.

struct Crumb {
    std::string name;
    char state = 0;
    std::vector<Crumb> children;
};

class CrumbParser {
public:
    explicit CrumbParser(std::string_view s) : s_(s) {}

    std::vector<Crumb> parse_top() {
        std::vector<Crumb> out;
        while (true) {
            skip_sep();
            if (pos_ >= s_.size() || s_[pos_] != '{') break;
            Crumb c;
            bool ok = node(c, 0);
            out.push_back(std::move(c));
            if (!ok) break;
        }
        return out;
    }

private:
    std::string_view s_;
    size_t pos_ = 0;

    void skip_ws() { while (pos_ < s_.size() && is_space(s_[pos_])) ++pos_; }
    void skip_sep() { while (pos_ < s_.size() && (is_space(s_[pos_]) || s_[pos_] == ',')) ++pos_; }
    bool eat(char c) {
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    // Fills what it can; returns false on malformed input so the caller stops.
    bool node(Crumb& c, int depth) {
        if (depth > 512) return false;
        if (!eat('{') || !eat('{')) return false;
        // The name runs to its closing brace; names with balanced braces inside survive.
        int d = 0;
        size_t start = pos_;
        while (pos_ < s_.size()) {
            char ch = s_[pos_];
            if (ch == '{') ++d;
            else if (ch == '}') { if (d == 0) break; --d; }
            ++pos_;
        }
        if (pos_ >= s_.size()) return false;
        c.name = trim_copy(s_.substr(start, pos_ - start));
        ++pos_;
        if (!eat(',')) return false;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] != ',' && s_[pos_] != '}') c.state = s_[pos_++];
        while (pos_ < s_.size() && s_[pos_] != ',' && s_[pos_] != '}') ++pos_;
        if (eat(',')) {
            if (!eat('{')) return false;
            while (true) {
                skip_ws();
                if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; break; }
                Crumb child;
                bool ok = node(child, depth + 1);
                c.children.push_back(std::move(child));
                if (!ok) return false;
                skip_ws();
                if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
                if (!eat('}')) return false;
                break;
            }
        }
        return eat('}');
    }
};

// Several siblings can be active at once because the GPU pipelines work: a later
// pass writes its begin marker before an earlier one retires. The first active
// sibling is the oldest unfinished work, which is where the GPU was stuck.
std::vector<std::string> active_chain(const std::vector<Crumb>& roots) {
    std::vector<std::string> chain;
    const std::vector<Crumb>* level = &roots;
    while (level) {
        const Crumb* hit = nullptr;
        for (const Crumb& c : *level)
            if (c.state == 'A') { hit = &c; break; }
        if (!hit) break;
        chain.push_back(hit->name);
        level = &hit->children;
    }
    return chain;
}

// "ModuleName 0x00006ffff0530000 + 2f9cfeb"
bool parse_frame_line(std::string_view line, Frame& f) {
    std::string t = trim_copy(line);
    size_t plus = t.rfind(" + ");
    if (plus == std::string::npos) return false;
    std::string off = trim_copy(std::string_view(t).substr(plus + 3));
    std::string head = trim_copy(std::string_view(t).substr(0, plus));
    size_t sp = head.find_last_of(" \t");
    std::string base_s = sp == std::string::npos ? head : head.substr(sp + 1);
    std::string module = sp == std::string::npos ? std::string() : trim_copy(std::string_view(head).substr(0, sp));
    uint64_t base = 0, offset = 0;
    if (!parse_hex(base_s, base) || !parse_hex(off, offset)) return false;
    f = Frame{};
    f.module = module;
    f.base = base;
    f.offset = offset;
    return true;
}

struct ContextSink : XmlLeafSink {
    CrashReport& r;
    bool have_crumbs = false;
    std::string queue_name;
    explicit ContextSink(CrashReport& rep) : r(rep) {}

    static bool under(const std::vector<std::string>& anc, const char* tag) {
        return std::find(anc.begin(), anc.end(), tag) != anc.end();
    }

    void prop(const std::string& key, std::string text) {
        auto it = r.props.find(key);
        if (it != r.props.end() && text.empty()) return;  // keep the last non-empty
        r.props[key] = std::move(text);
    }

    void leaf(const std::vector<std::string>& anc, const std::string& name, std::string text) override {
        // Per-thread stacks repeat the same tag names; they are not crash-level facts.
        if (under(anc, "Threads")) return;
        if (under(anc, "GPUBreadcrumbs")) {
            if (under(anc, "Queue")) {
                if (name == "Name") queue_name = text;
                else if (name == "Breadcrumbs" && !have_crumbs) {
                    auto chain = active_chain(CrumbParser(text).parse_top());
                    if (!chain.empty()) {
                        r.gpu_breadcrumb_tail = std::move(chain);
                        have_crumbs = true;
                        if (!queue_name.empty()) r.props["GPUBreadcrumbs.ActiveQueue"] = queue_name;
                    }
                }
            } else {
                prop("GPUBreadcrumbs." + name, std::move(text));
            }
            return;
        }
        if (name == "CrashGUID") { if (!text.empty()) r.guid = text; return; }
        if (name == "CrashType") { if (!text.empty()) r.crash_type = text; return; }
        if (name == "ErrorMessage") { if (!text.empty()) r.error_message = text; return; }
        if (name == "IsAssert") { r.is_assert = truthy(text); return; }
        if (name == "IsEnsure") { r.is_ensure = truthy(text); return; }
        if (name == "IsStall") { r.is_stall = truthy(text); return; }
        if (name == "MemoryStats.bIsOOM") { r.is_oom = truthy(text); return; }
        if (name == "SecondsSinceStart") {
            int v = -1;
            auto res = std::from_chars(text.data(), text.data() + text.size(), v);
            if (res.ec == std::errc()) r.seconds_since_start = v;
            return;
        }
        if (name == "EngineVersion") { if (!text.empty()) r.engine_version = text; return; }
        if (name == "BuildVersion") { if (!text.empty()) r.build_version = text; return; }
        if (name == "PCallStackHash") { if (!text.empty()) r.callstack_hash = text; return; }
        if (name == "PCallStack") {
            if (text.empty()) return;
            r.frames.clear();
            size_t a = 0;
            while (a <= text.size()) {
                size_t b = text.find('\n', a);
                if (b == std::string::npos) b = text.size();
                Frame f;
                if (parse_frame_line(std::string_view(text).substr(a, b - a), f)) r.frames.push_back(std::move(f));
                a = b + 1;
            }
            return;
        }
        prop(name, std::move(text));
    }
};

FileTime mtime_or_zero(const fs::path& p) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    return ec ? FileTime{} : t;
}

bool starts_with_icase(const std::string& s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        char a = s[i], b = prefix[i];
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 32);
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 32);
        if (a != b) return false;
    }
    return true;
}

}  // namespace

CrashReport parse_crash_context(const fs::path& runtime_xml) {
    CrashReport r;
    std::string xml = read_file_capped(runtime_xml, 64u * 1024 * 1024);
    ContextSink sink(r);
    walk_xml(xml, sink);
    return r;
}

std::vector<CrashReport> scan_crash_reports(const Paths& paths, size_t limit) {
    std::vector<CrashReport> out;
    try {
        const fs::path root = paths.saved / "Crashes";
        std::error_code ec;
        if (paths.saved.empty() || !fs::is_directory(root, ec)) return out;

        struct Entry { fs::path dir; FileTime when; };
        std::vector<Entry> dirs;
        for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            std::error_code e2;
            if (!it->is_directory(e2)) continue;
            std::string name;
            try { name = it->path().filename().string(); } catch (...) { continue; }
            if (!starts_with_icase(name, "UECC-")) continue;
            dirs.push_back({it->path(), mtime_or_zero(it->path())});
        }
        std::sort(dirs.begin(), dirs.end(), [](const Entry& a, const Entry& b) {
            if (a.when != b.when) return a.when > b.when;
            return a.dir > b.dir;
        });
        if (dirs.size() > limit) dirs.resize(limit);

        for (const Entry& d : dirs) {
            CrashReport r;
            const fs::path xml = d.dir / "CrashContext.runtime-xml";
            std::string problem;
            try {
                std::error_code e2;
                if (fs::is_regular_file(xml, e2)) r = parse_crash_context(xml);
                else problem = "no CrashContext.runtime-xml in the crash folder";
            } catch (const std::exception& e) {
                r = CrashReport{};
                problem = std::string("crash context did not parse: ") + e.what();
            } catch (...) {
                r = CrashReport{};
                problem = "crash context did not parse";
            }
            r.folder = d.dir;
            r.when = d.when;
            if (r.guid.empty()) {
                try { r.guid = d.dir.filename().string(); } catch (...) {}
            }
            const fs::path dmp = d.dir / "UEMinidump.dmp";
            std::error_code e3;
            if (fs::is_regular_file(dmp, e3)) {
                r.has_minidump = true;
                r.minidump = dmp;
                auto sz = fs::file_size(dmp, e3);
                r.minidump_bytes = e3 ? 0 : static_cast<uint64_t>(sz);
                attach_minidump(r, dmp);
            }
            if (!problem.empty()) {
                r.minidump_error = r.minidump_error.empty() ? problem : problem + "; " + r.minidump_error;
            }
            out.push_back(std::move(r));
        }
    } catch (...) {
        // Keep whatever was collected; the orchestrator shows an empty list as "no crashes".
    }
    return out;
}

}  // namespace wcmg

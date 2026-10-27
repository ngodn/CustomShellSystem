// The support bundle: one zip a player can attach to a bug report. It holds the
// WCMG report, the newest crash reports, UE4SS and mod logs, our own mods' state
// files and the game's config. Save games never go in. One unreadable file only
// adds a line to bundle-notes.txt; the bundle itself fails only when the zip
// cannot be written.
#include "scanners.hpp"
#include "miniz/miniz.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wcmg {
namespace {
constexpr uint64_t kMB = 1024ull * 1024;
constexpr uint64_t kFileCap = 64 * kMB;
constexpr uint64_t kTotalCap = 512 * kMB;
constexpr uint64_t kOurLogCap = 5 * kMB;
constexpr size_t kOurLogFiles = 20;
constexpr size_t kCrashFolders = 5;
constexpr size_t kUe4ssDumps = 3;
constexpr size_t kModFilesPerKind = 10;

bool is_file(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }
bool is_dir(const fs::path& p) { std::error_code ec; return fs::is_directory(p, ec); }
FileTime mtime(const fs::path& p) { std::error_code ec; auto t = fs::last_write_time(p, ec); return ec ? FileTime{} : t; }
uint64_t fsize(const fs::path& p) { std::error_code ec; auto s = fs::file_size(p, ec); return ec ? 0 : (uint64_t)s; }

std::time_t to_time_t(FileTime t) {
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        t - FileTime::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sys);
}

std::FILE* open_for_write(const fs::path& p) {
#ifdef _WIN32
    return _wfopen(p.c_str(), L"wb");
#else
    return std::fopen(p.c_str(), "wb");
#endif
}

// Regular files directly in `dir` (or below it), newest first.
std::vector<fs::path> files_in(const fs::path& dir, bool recursive = false) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!is_dir(dir)) return out;
    if (recursive) {
        for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
             it.increment(ec))
            if (it->is_regular_file(ec)) out.push_back(it->path());
    } else {
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
             it.increment(ec))
            if (it->is_regular_file(ec)) out.push_back(it->path());
    }
    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) { return mtime(a) > mtime(b); });
    return out;
}

std::vector<fs::path> dirs_in(const fs::path& dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!is_dir(dir)) return out;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec)) out.push_back(it->path());
    return out;
}

std::string name_of(const fs::path& p) { return path_utf8(p.filename()); }

std::string ext_lower(const fs::path& p) { return lower(path_utf8(p.extension())); }

std::string rel_utf8(const fs::path& p, const fs::path& base) {
    std::error_code ec;
    auto r = fs::relative(p, base, ec);
    return path_utf8(ec || r.empty() ? p.filename() : r);
}

class Bundle {
public:
    explicit Bundle(const fs::path& out) : file_(open_for_write(out)) {
        if (!file_) throw std::runtime_error("Cannot create " + path_utf8(out));
        if (!mz_zip_writer_init_cfile(&zip_, file_, 0)) {
            std::fclose(file_);
            file_ = nullptr;
            throw std::runtime_error("Cannot start a zip in " + path_utf8(out));
        }
    }
    ~Bundle() {
        if (open_) mz_zip_writer_end(&zip_);
        if (file_) std::fclose(file_);
    }

    void note(std::string s) { notes_.push_back(std::move(s)); }
    size_t count() const { return names_.size(); }
    uint64_t total() const { return total_; }

    bool add_text(const std::string& name, const std::string& data) { return add_mem(name, data, std::time(nullptr), false); }

    // Text files over the cap keep their tail (the end of a log is what matters);
    // binary files over the cap are skipped.
    bool add_file(const std::string& name, const fs::path& src, uint64_t cap = kFileCap, bool text = true) {
        if (!is_file(src)) return false;
        auto size = fsize(src);
        std::ifstream in(src, std::ios::binary);
        if (!in) { note("Could not open " + path_utf8(src)); return false; }
        std::string data;
        if (size > cap) {
            if (!text) {
                note("Skipped " + name + ": " + format_bytes(size) + " is over the " + format_bytes(cap) + " per-file limit");
                return false;
            }
            in.seekg((std::streamoff)(size - cap));
            data = "[WCMG: this file is " + format_bytes(size) + "; only the last " + format_bytes(cap) + " are included]\n";
            size = cap;
        }
        auto head = data.size();
        data.resize(head + (size_t)size);
        in.read(data.data() + head, (std::streamsize)size);
        data.resize(head + (size_t)in.gcount());
        if (data.size() == head && size > 0) { note("Could not read " + path_utf8(src)); return false; }
        bool compressed_already = ext_lower(src) == ".zip";
        return add_mem(name, data, to_time_t(mtime(src)), !text || compressed_already);
    }

    bool would_fit(uint64_t bytes) const { return total_ + bytes <= kTotalCap; }

    void finish() {
        if (!mz_zip_writer_finalize_archive(&zip_))
            throw std::runtime_error(std::string("Could not finish the zip: ") + mz_zip_get_error_string(mz_zip_get_last_error(&zip_)));
        mz_zip_writer_end(&zip_);
        open_ = false;
        bool bad = std::ferror(file_) != 0;
        bad |= std::fclose(file_) != 0;
        file_ = nullptr;
        if (bad) throw std::runtime_error("Could not write the zip to disk (is the drive full?)");
    }

private:
    bool add_mem(std::string name, const std::string& data, std::time_t when, bool fast) {
        std::replace(name.begin(), name.end(), '\\', '/');
        if (!names_.insert(name).second) return false;
        if (name != "bundle-notes.txt" && !would_fit(data.size())) {
            note("Skipped " + name + ": the bundle reached its " + format_bytes(kTotalCap) + " limit");
            names_.erase(name);
            return false;
        }
        MZ_TIME_T t = when;
        mz_uint level = fast ? MZ_BEST_SPEED : MZ_DEFAULT_LEVEL;
        if (!mz_zip_writer_add_mem_ex_v2(&zip_, name.c_str(), data.data(), data.size(), nullptr, 0, level, 0, 0, &t, nullptr, 0,
                                         nullptr, 0)) {
            note("Could not add " + name + ": " + mz_zip_get_error_string(mz_zip_get_last_error(&zip_)));
            names_.erase(name);
            return false;
        }
        total_ += data.size();
        return true;
    }

    std::FILE* file_ = nullptr;
    mz_zip_archive zip_{};
    bool open_ = true;
    uint64_t total_ = 0;
    std::set<std::string> names_;
    std::vector<std::string> notes_;

public:
    const std::vector<std::string>& notes() const { return notes_; }
};

bool our_mod(const std::string& name) {
    return name == "CustomShellSystem" || name == "CSSX" || name == "CCS";
}

std::string one_line(std::string s) {
    for (auto& c : s) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    return trim(s);
}

// Directory listing with sizes and dates, for folders we do not copy wholesale.
std::string listing(const fs::path& dir, int max_depth, size_t max_lines = 5000) {
    std::ostringstream o;
    o << path_utf8(dir) << "\n";
    if (!is_dir(dir)) { o << "  (not found)\n"; return o.str(); }
    std::error_code ec;
    size_t lines = 0;
    for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (it.depth() >= max_depth) it.disable_recursion_pending();
        if (++lines > max_lines) { o << "  ... listing cut at " << max_lines << " entries\n"; break; }
        auto rel = rel_utf8(it->path(), dir);
        std::error_code e2;
        if (it->is_directory(e2)) o << "  " << rel << "/\n";
        else o << "  " << rel << "  " << format_bytes(fsize(it->path())) << "  " << format_time(mtime(it->path())) << "\n";
    }
    return o.str();
}

std::string crash_index(const Report& report, const std::vector<fs::path>& folders) {
    std::ostringstream o;
    o << "Crash report folders in " << path_utf8(report.evidence.paths.saved / "Crashes") << ", newest first\n";
    o << "The newest " << kCrashFolders << " are included under crashes/.\n\n";
    if (folders.empty()) o << "(none)\n";
    for (auto& f : folders) {
        std::string type, error;
        bool known = false;
        for (auto& c : report.evidence.crashes)
            if (c.folder.filename() == f.filename()) { type = c.crash_type; error = c.error_message; known = true; break; }
        if (!known && is_file(f / "CrashContext.runtime-xml")) {
            try {
                auto c = parse_crash_context(f / "CrashContext.runtime-xml");
                type = c.crash_type;
                error = c.error_message;
            } catch (...) {}
        }
        o << format_time(mtime(f)) << "  " << name_of(f) << "  " << (type.empty() ? "?" : type) << "  "
          << (error.empty() ? "" : one_line(error).substr(0, 300)) << "\n";
    }
    return o.str();
}

std::string paks_listing(const PakInfo& p) {
    std::ostringstream o;
    o << "Pak containers (" << p.containers.size() << "), game package ids: " << p.game_package_ids << "\n";
    if (!p.error.empty()) o << "Scan error: " << p.error << "\n";
    o << "\n";
    for (auto& c : p.containers) {
        o << c.stem << (c.is_patch ? "  [patch]" : "") << (c.is_game ? "  [game]" : "") << (c.is_mod ? "  [mod]" : "")
          << (c.css_package ? "  [CSS]" : "") << (c.cssx_package ? "  [CSSX]" : "") << (c.complete ? "" : "  [INCOMPLETE]") << "\n";
        o << "  folder: " << path_utf8(c.folder) << "\n";
        if (!c.pak.empty()) o << "  pak:  " << format_bytes(c.pak_bytes) << "\n";
        if (!c.utoc.empty()) o << "  utoc: " << format_bytes(c.utoc_bytes) << "\n";
        if (!c.ucas.empty()) o << "  ucas: " << format_bytes(c.ucas_bytes) << "\n";
        o << "  date: " << format_time(c.when) << "  chunks: " << c.chunks << "  packages: " << c.package_ids.size() << "\n";
        if (c.overrides_game) {
            o << "  overrides " << c.overrides_game << " game packages";
            for (size_t i = 0; i < c.override_names.size() && i < 20; ++i) o << (i ? ", " : ": ") << c.override_names[i];
            o << "\n";
            for (size_t i = 0; i < c.override_hints.size() && i < 10; ++i) o << "    in " << c.override_hints[i] << "\n";
        }
        if (!c.problem.empty()) o << "  problem: " << c.problem << "\n";
    }
    if (!p.mod_collisions.empty()) {
        o << "\nPackages carried by two or more mod containers (" << p.mod_collisions.size() << ")\n";
        for (auto& c : p.mod_collisions) {
            char id[32];
            std::snprintf(id, sizeof id, "%016llx", (unsigned long long)c.package_id);
            o << "  " << id << (c.name.empty() ? "" : "  " + c.name) << ":";
            for (auto& s : c.containers) o << " " << s;
            o << "\n";
        }
    }
    if (!p.stray_files.empty()) {
        o << "\nStray files under the mod folders\n";
        for (auto& s : p.stray_files) o << "  " << s << "\n";
    }
    if (!p.mods_outside_mods_folder.empty()) {
        o << "\nMod containers directly in Paks/\n";
        for (auto& s : p.mods_outside_mods_folder) o << "  " << s << "\n";
    }
    return o.str();
}

std::string manifest_summary(const ManifestInfo& m) {
    std::ostringstream o;
    o << "Manifest present: " << (m.present ? "yes" : "no") << ", entries: " << m.manifest_entries << "\n";
    o << "Steam build id: " << (m.steam_build_id.empty() ? "?" : m.steam_build_id) << "\n";
    if (!m.steam_last_updated.empty()) {
        std::time_t t = (std::time_t)std::strtoll(m.steam_last_updated.c_str(), nullptr, 10);
        char buf[32] = "";
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
        o << "Steam last updated: " << m.steam_last_updated << " (" << buf << ")\n";
    }
    o << "Exe: " << format_bytes(m.exe_bytes) << ", " << format_time(m.exe_when) << "\n";
    o << "Exe sha256: " << (m.exe_sha256.empty() ? "?" : m.exe_sha256) << "\n";
    auto list = [&](const char* title, const std::vector<std::string>& v, size_t total) {
        o << "\n" << title << " (" << std::max(total, v.size()) << ")\n";
        for (auto& s : v) o << "  " << s << "\n";
        if (total > v.size()) o << "  ... and " << (total - v.size()) << " more\n";
    };
    list("Missing from disk", m.missing, m.missing_total);
    list("Not in the manifest", m.foreign, m.foreign_total);
    list("Replaced engine or game binaries", m.replaced_binaries, m.replaced_binaries.size());
    return o.str();
}

std::string system_text(const Report& r) {
    auto& s = r.evidence.system;
    auto& p = r.evidence.paths;
    std::ostringstream o;
    o << r.generated_by << "\n";
    o << "OS: " << s.os << "\n";
    o << "Game ran under Wine/Proton: " << (s.game_ran_under_wine ? "yes" : "no") << "\n";
    o << "WCMG ran under Wine: " << (p.running_under_wine ? "yes" : "no") << "\n";
    o << "CPU: " << s.cpu << "\nGPU: " << s.gpu << "\nGPU driver: " << s.gpu_driver << "\nRHI: " << s.rhi << "\n";
    o << "RAM: " << (s.total_ram ? format_bytes(s.total_ram) : std::string("?")) << "\n";
    o << "DLSS frame generation forced: " << (s.dlss_frame_gen_forced ? "yes" : "no") << "\n";
    o << "FSR frame interpolation: " << (s.fsr_frame_interp ? "yes" : "no") << "\n";
    if (!s.overlays_seen.empty()) {
        o << "Overlays seen in crash dumps:";
        for (auto& x : s.overlays_seen) o << " " << x;
        o << "\n";
    }
    for (auto& n : s.notes) o << "Note: " << n << "\n";
    o << "\nPaths\n";
    o << "  game root: " << path_utf8(p.game_root) << "\n  win64: " << path_utf8(p.win64) << "\n  ue4ss: " << path_utf8(p.ue4ss)
      << "\n  paks: " << path_utf8(p.paks) << "\n  saved: " << path_utf8(p.saved) << "\n  app manifest: " << path_utf8(p.steam_app_manifest)
      << "\n  found by: " << p.how_found << "\n";
    auto kv = [&](const char* title, const std::map<std::string, std::string>& m) {
        if (m.empty()) return;
        o << "\n" << title << "\n";
        for (auto& [k, v] : m) o << "  " << k << " = " << v << "\n";
    };
    kv("Engine.ini", s.engine_ini);
    kv("GameUserSettings.ini", s.game_user_settings);
    if (!r.evidence.scan_errors.empty()) {
        o << "\nScanner problems\n";
        for (auto& e : r.evidence.scan_errors) o << "  " << e << "\n";
    }
    return o.str();
}

struct Deferred { std::string name; fs::path src; };

// Report text can come from rules still being written; never let it sink the bundle.
template <class F>
std::string safe(Bundle& b, const char* what, F&& f) {
    try { return f(); }
    catch (const std::exception& e) { b.note(std::string("Could not build ") + what + ": " + e.what()); }
    catch (...) { b.note(std::string("Could not build ") + what); }
    return {};
}
}  // namespace

fs::path collect_bundle(const Report& report, const fs::path& zip_out, const Progress& progress) {
    auto step = [&](const char* s) { if (progress) progress(s); };
    const auto& P = report.evidence.paths;
    auto part = zip_out;
    part += ".part";
    std::error_code ec;
    if (zip_out.has_parent_path()) fs::create_directories(zip_out.parent_path(), ec);

    std::vector<Deferred> dumps;  // added last so the size cap lands on them, not on logs
    try {
        Bundle b(part);

        step("Packing the report");
        b.add_text("WCMG-report.md", safe(b, "the markdown report", [&] { return report_markdown(report); }));
        b.add_text("WCMG-report.txt", safe(b, "the text report", [&] { return report_text(report); }));
        b.add_text("WCMG-report.json", safe(b, "the json report", [&] { return report_json(report); }));
        b.add_text("system.txt", safe(b, "system.txt", [&] { return system_text(report); }));

        step("Packing crash reports");
        std::vector<fs::path> crash_dirs = dirs_in(P.saved / "Crashes");
        std::sort(crash_dirs.begin(), crash_dirs.end(), [](const fs::path& a, const fs::path& b) { return mtime(a) > mtime(b); });
        b.add_text("crashes/index.txt", safe(b, "the crash index", [&] { return crash_index(report, crash_dirs); }));
        for (size_t i = 0; i < crash_dirs.size() && i < kCrashFolders; ++i) {
            auto& d = crash_dirs[i];
            auto base = "crashes/" + name_of(d) + "/";
            b.add_file(base + "CrashContext.runtime-xml", d / "CrashContext.runtime-xml");
            // The engine copies its log into the crash folder; it is the best record of the last seconds.
            for (auto& f : files_in(d))
                if (ext_lower(f) == ".log") b.add_file(base + name_of(f), f);
            if (is_file(d / "UEMinidump.dmp")) dumps.push_back({base + "UEMinidump.dmp", d / "UEMinidump.dmp"});
        }
        if (!is_dir(P.saved / "Crashes")) b.note("No Crashes folder at " + path_utf8(P.saved / "Crashes"));

        step("Packing UE4SS files");
        for (auto& f : files_in(P.ue4ss)) {
            auto n = name_of(f);
            auto ln = lower(n);
            if (ln.rfind("ue4ss", 0) == 0 && (ln.ends_with(".log") || ln.find(".log.") != std::string::npos)) b.add_file("ue4ss/" + n, f);
        }
        b.add_file("ue4ss/UE4SS-settings.ini", P.ue4ss / "UE4SS-settings.ini");
        b.add_file("ue4ss/Mods/mods.txt", P.mods / "mods.txt");
        b.add_file("ue4ss/Mods/mods.json", P.mods / "mods.json");
        for (auto& mod : dirs_in(P.mods)) {
            auto name = name_of(mod);
            auto base = "ue4ss/Mods/" + name + "/";
            b.add_file(base + "enabled.txt", mod / "enabled.txt");
            if (our_mod(name)) continue;  // covered in ours/
            size_t logs = 0, jsons = 0, skipped = 0;
            for (auto& f : files_in(mod)) {  // newest first
                auto e = ext_lower(f);
                size_t& n = e == ".log" ? logs : jsons;
                if (e != ".log" && e != ".json") continue;
                if (n >= kModFilesPerKind) { ++skipped; continue; }
                if (b.add_file(base + name_of(f), f)) ++n;
            }
            if (skipped)
                b.note(name + ": " + std::to_string(skipped) + " older .log/.json files at the mod root were left out");
        }
        {
            std::vector<fs::path> ue_dumps;
            for (auto& dir : {P.ue4ss, P.win64})
                for (auto& f : files_in(dir)) {
                    auto n = lower(name_of(f));
                    if (n.rfind("crash_", 0) == 0 && n.ends_with(".dmp")) ue_dumps.push_back(f);
                }
            std::sort(ue_dumps.begin(), ue_dumps.end(), [](const fs::path& a, const fs::path& b) { return mtime(a) > mtime(b); });
            ue_dumps.erase(std::unique(ue_dumps.begin(), ue_dumps.end()), ue_dumps.end());
            for (size_t i = 0; i < ue_dumps.size() && i < kUe4ssDumps; ++i) dumps.push_back({"ue4ss/" + name_of(ue_dumps[i]), ue_dumps[i]});
            if (ue_dumps.size() > kUe4ssDumps)
                b.note("UE4SS has " + std::to_string(ue_dumps.size()) + " crash dumps; only the newest " + std::to_string(kUe4ssDumps) + " are included");
        }
        b.add_text("files/win64.txt", listing(P.win64, 1));
        b.add_text("files/ue4ss-mods.txt", listing(P.mods, 3));

        step("Packing CSS, CSSX and CCS files");
        struct Ours { const char* name; std::vector<const char*> files; };
        const Ours ours[] = {
            {"CustomShellSystem", {"CSS.log", "CSS.log.1", "release.json", "core.json", "loader-contract.json", "state/state.json"}},
            {"CSSX", {"CSSX.log", "release.json", "core.json", "settings.json", "dev/enabled.txt"}},
            {"CCS", {"CCS.log", "core.json", "settings.json"}},
        };
        for (auto& o : ours) {
            auto dir = P.mods / o.name;
            if (!is_dir(dir)) continue;
            auto base = std::string("ours/") + o.name + "/";
            for (auto* f : o.files) b.add_file(base + f, dir / fs::path(f));
            auto logs = files_in(dir / "logs", true);
            for (size_t i = 0; i < logs.size() && i < kOurLogFiles; ++i)
                b.add_file(base + "logs/" + rel_utf8(logs[i], dir / "logs"), logs[i], kOurLogCap);
            if (logs.size() > kOurLogFiles)
                b.note(std::string(o.name) + "/logs has " + std::to_string(logs.size()) + " files; only the newest " +
                       std::to_string(kOurLogFiles) + " are included");
        }

        step("Packing config and the game log");
        b.add_file("config/Engine.ini", P.saved / "Config" / "Windows" / "Engine.ini");
        b.add_file("config/GameUserSettings.ini", P.saved / "Config" / "Windows" / "GameUserSettings.ini");
        {
            auto logs = files_in(P.saved / "Logs");
            size_t taken = 0;
            for (auto& f : logs) {
                if (ext_lower(f) != ".log" || taken >= 3) continue;
                if (b.add_file("logs/" + name_of(f), f)) ++taken;
            }
        }

        step("Packing pak and manifest summaries");
        b.add_text("paks/listing.txt", safe(b, "the pak listing", [&] { return paks_listing(report.evidence.paks); }) + "\nFiles on disk\n" +
                                           listing(P.paks, 2));
        b.add_text("manifest/summary.txt", safe(b, "the manifest summary", [&] { return manifest_summary(report.evidence.manifest); }));

        step("Packing crash dumps");
        for (auto& d : dumps) {
            auto size = fsize(d.src);
            if (!b.would_fit(std::min(size, kFileCap))) {
                b.note("Skipped " + d.name + " (" + format_bytes(size) + "): the bundle reached its " + format_bytes(kTotalCap) + " limit");
                continue;
            }
            b.add_file(d.name, d.src, kFileCap, false);
        }

        step("Finishing the zip");
        std::ostringstream notes;
        notes << "WCMG support bundle\n" << report.generated_by << "\n";
        {
            std::time_t now = std::time(nullptr);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &now);
#else
            localtime_r(&now, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
            notes << "Made " << buf << " from " << path_utf8(P.game_root) << "\n";
        }
        notes << b.count() << " files, " << format_bytes(b.total()) << " before compression.\n";
        notes << "Save games are never included.\n";
        if (b.notes().empty()) notes << "\nNothing was skipped.\n";
        else {
            notes << "\nNotes\n";
            for (auto& n : b.notes()) notes << "  " << n << "\n";
        }
        b.add_text("bundle-notes.txt", notes.str());
        b.finish();
    } catch (...) {
        fs::remove(part, ec);
        throw;
    }
    fs::remove(zip_out, ec);
    fs::rename(part, zip_out, ec);
    if (ec) {
        auto why = ec.message();
        fs::remove(part, ec);
        throw std::runtime_error("Could not move the zip into place at " + path_utf8(zip_out) + ": " + why);
    }
    return zip_out;
}
}  // namespace wcmg

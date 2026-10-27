// Manifest scanner: the game's shipped file lists against what is on disk, the Steam
// build id and the shipping exe's hash.
//
// Manifest_UFSFiles_Win64.txt lists every file that went INTO the containers, so almost
// none of it exists loose (77 of 199,615 on 2026-10-27, the movies and the pakchunk .pak
// files). Manifest_NonUFSFiles_Win64.txt lists the loose files: exes, DLLs, movies. So
// "missing" is the NonUFS list plus UFS entries in the folders we walk, and "foreign" is
// what sits in Binaries/ and Paks/ that neither list knows.
//
// The manifest timestamps are build times, but Steam writes its own mtime on install
// (the shipping exe is about 22 hours off, tbb12.dll 11 days), so mtimes cannot tell a
// replaced binary from a shipped one and replaced_binaries stays empty.
#include "scanners.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>

namespace wcmg {
namespace {

constexpr size_t max_missing = 50;
constexpr size_t max_foreign = 100;
constexpr size_t max_walk = 100000;

fs::path from_utf8(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }

// The folders walked for foreign files, lower case, relative to Sparta.
constexpr std::string_view walk_roots[] = {"mortalshell2/binaries/", "mortalshell2/content/paks/", "engine/binaries/"};

bool under_walk_roots(const std::string& lowered) {
    for (auto r : walk_roots) if (lowered.starts_with(r)) return true;
    return false;
}

// Mod locations and runtime leftovers that are expected to be "not shipped".
bool excluded(const std::string& lowered) {
    if (lowered.starts_with("mortalshell2/binaries/win64/ue4ss/") || lowered.starts_with("mortalshell2/content/paks/~mods/") ||
        lowered.starts_with("mortalshell2/content/paks/logicmods/") || lowered.starts_with("mortalshell2/saved/"))
        return true;
    const auto slash = lowered.rfind('/');
    const std::string name = slash == std::string::npos ? lowered : lowered.substr(slash + 1);
    return name.starts_with("vkd3d-proton.cache") || name.ends_with(".pdb");
}

struct ManifestFile {
    std::vector<std::string> paths;  // as written, forward slashes
    bool present = false;
};

ManifestFile read_manifest(const fs::path& file) {
    ManifestFile m;
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return m;
    std::ifstream in(file, std::ios::binary);
    if (!in) return m;
    m.present = true;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto tab = line.find('\t');
        std::string path = tab == std::string::npos ? line : line.substr(0, tab);
        std::replace(path.begin(), path.end(), '\\', '/');
        path = trim(path);
        if (!path.empty()) m.paths.push_back(std::move(path));
    }
    return m;
}

// Valve KeyValues text: one "key" "value" pair per line in the AppState block.
std::string acf_value(const std::string& text, const std::string& key) {
    size_t pos = 0;
    while (pos < text.size()) {
        const auto end = std::min(text.find('\n', pos), text.size());
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        std::vector<std::string> tokens;
        for (size_t i = 0; i < line.size();) {
            const auto open = line.find('"', i);
            if (open == std::string::npos) break;
            const auto close = line.find('"', open + 1);
            if (close == std::string::npos) break;
            tokens.push_back(line.substr(open + 1, close - open - 1));
            i = close + 1;
        }
        if (tokens.size() == 2 && lower(tokens[0]) == lower(key)) return tokens[1];
    }
    return {};
}

}  // namespace

ManifestInfo scan_manifest(const Paths& paths, const Progress& progress) {
    ManifestInfo info;
    try {
        const fs::path root = paths.game_root;
        std::error_code ec;
        if (progress) progress("Reading the game's file manifests");
        const ManifestFile ufs = read_manifest(root / "Manifest_UFSFiles_Win64.txt");
        const ManifestFile non_ufs = read_manifest(root / "Manifest_NonUFSFiles_Win64.txt");
        const ManifestFile debug = read_manifest(root / "Manifest_DebugFiles_Win64.txt");
        info.present = ufs.present;
        info.manifest_entries = ufs.paths.size() + non_ufs.paths.size();

        // Files the game shipped inside the walked folders, lower case: Windows does not
        // care about case and neither does Proton.
        std::unordered_set<std::string> known;
        std::unordered_set<std::string> shipped_pak_stems;  // "pakchunk0-windows"
        std::vector<std::pair<std::string, std::string>> expect_loose;  // (lowered, as written)
        // Debug files (PDBs) are listed but Steam does not ship them: known, never expected.
        auto take = [&](const ManifestFile& m, bool loose_everywhere, bool expected = true) {
            for (const auto& p : m.paths) {
                std::string l = lower(p);
                const bool in_walk = under_walk_roots(l);
                if (expected && (loose_everywhere || in_walk)) expect_loose.emplace_back(l, p);
                if (l.starts_with("mortalshell2/content/paks/") && l.ends_with(".pak") && l.find('/', 26) == std::string::npos)
                    shipped_pak_stems.insert(l.substr(26, l.size() - 26 - 4));
                if (in_walk) known.insert(std::move(l));
            }
        };
        take(non_ufs, true);
        take(ufs, false);
        take(debug, false, false);

        if (progress) progress("Walking Binaries and Paks for files the game did not ship");
        std::unordered_set<std::string> on_disk;  // lowered, within the walk roots
        std::vector<std::pair<size_t, std::string>> foreign;  // (depth, relative path)
        size_t walked = 0;
        for (const auto r : {"MortalShell2/Binaries", "MortalShell2/Content/Paks", "Engine/Binaries"}) {
            const fs::path start = root / r;
            if (!fs::is_directory(start, ec)) { ec.clear(); continue; }
            fs::recursive_directory_iterator it(start, fs::directory_options::skip_permission_denied, ec), end;
            for (; it != end && walked < max_walk; it.increment(ec)) {
                if (ec) { ec.clear(); break; }
                const auto st = it->symlink_status(ec);
                if (ec) { ec.clear(); continue; }
                const std::string rel = path_utf8(it->path().lexically_relative(root));
                const std::string l = lower(rel);
                if (fs::is_directory(st)) {
                    // Mod folders hold thousands of files we are told to ignore; do not descend.
                    if (excluded(l + "/")) it.disable_recursion_pending();
                    continue;
                }
                ++walked;
                on_disk.insert(l);
                if (excluded(l) || known.contains(l)) continue;
                // The pakchunk .utoc/.ucas/.sig beside a shipped .pak and the global container
                // are not in any manifest but are the game's own.
                if (l.starts_with("mortalshell2/content/paks/") && l.find('/', 26) == std::string::npos) {
                    const std::string name = l.substr(26);
                    const auto dot = name.rfind('.');
                    const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
                    const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
                    if ((ext == ".utoc" || ext == ".ucas" || ext == ".sig") && shipped_pak_stems.contains(stem)) continue;
                    if (stem == "global" && (ext == ".utoc" || ext == ".ucas")) continue;
                }
                foreign.emplace_back(size_t(std::count(rel.begin(), rel.end(), '/')), rel);
            }
            ec.clear();
        }
        info.foreign_total = foreign.size();
        // Shallow first: a DLL beside the exe matters more than a file deep in a backup folder.
        std::sort(foreign.begin(), foreign.end());
        for (size_t i = 0; i < foreign.size() && i < max_foreign; ++i) info.foreign.push_back(foreign[i].second);

        if (progress) progress("Checking shipped files are on disk");
        for (const auto& [l, p] : expect_loose) {
            bool exists;
            if (under_walk_roots(l)) exists = on_disk.contains(l) || (walked >= max_walk && fs::exists(root / from_utf8(p), ec));
            else exists = fs::exists(root / from_utf8(p), ec);
            ec.clear();
            if (exists) continue;
            ++info.missing_total;
            if (info.missing.size() < max_missing) info.missing.push_back(p);
        }

        fs::path acf = paths.steam_app_manifest;
        if (acf.empty() && !root.empty()) acf = root.parent_path().parent_path() / "appmanifest_2584270.acf";
        if (!acf.empty() && fs::is_regular_file(acf, ec)) {
            const std::string text = read_text(acf, 1024 * 1024);
            info.steam_build_id = acf_value(text, "buildid");
            info.steam_last_updated = acf_value(text, "LastUpdated");
        }
        ec.clear();

        fs::path win64 = paths.win64;
        if (win64.empty()) win64 = (paths.project.empty() ? root / "MortalShell2" : paths.project) / "Binaries/Win64";
        const fs::path exe = win64 / "MortalShell2-Win64-Shipping.exe";
        if (fs::is_regular_file(exe, ec)) {
            if (progress) progress("Hashing MortalShell2-Win64-Shipping.exe");
            info.exe_bytes = fs::file_size(exe, ec);
            if (ec) { info.exe_bytes = 0; ec.clear(); }
            info.exe_when = fs::last_write_time(exe, ec);
            if (ec) { info.exe_when = {}; ec.clear(); }
            info.exe_sha256 = sha256_file(exe);
        }
    } catch (...) {
        // Partial results are still useful; the orchestrator notes nothing here because
        // ManifestInfo has no error field.
    }
    return info;
}

}  // namespace wcmg

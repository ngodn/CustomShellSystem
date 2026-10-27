// The Windows half of discovery: Steam's registry keys, the Steam uninstall
// entry, Epic Games manifests, a shallow probe of fixed drives, Wine detection
// and the folder picker. The portable half is src/core/discovery.cpp.
#include "win_common.hpp"
#include "discovery_private.hpp"
#include <shlobj.h>
#include <shobjidl.h>
#include "nlohmann/json.hpp"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {
using wcmg::fs::path;
using wcmg::detail::Candidate;

std::wstring reg_string(HKEY root, const wchar_t* sub, const wchar_t* value, DWORD view = 0) {
    wchar_t buf[4096];
    DWORD bytes = sizeof buf;
    if (RegGetValueW(root, sub, value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | view, nullptr, buf, &bytes) != ERROR_SUCCESS)
        return {};
    return buf;
}

bool dir_exists(const path& p) { std::error_code ec; return wcmg::fs::is_directory(p, ec); }

std::wstring known_folder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = p;
    CoTaskMemFree(p);
    return out;
}
}  // namespace

extern "C" void wcmg_win_steam_roots(std::vector<path>* out) {
    auto add = [&](const std::wstring& s) { if (!s.empty()) out->push_back(path(s)); };
    add(reg_string(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"));
    add(reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"));
    add(reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath", RRF_SUBKEY_WOW6464KEY));
    auto x86 = known_folder(FOLDERID_ProgramFilesX86);
    if (!x86.empty()) add(x86 + L"\\Steam");
}

extern "C" void wcmg_win_extra_candidates(std::vector<Candidate>* out) {
    const wchar_t* uninstall = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 2584270";
    for (DWORD view : {(DWORD)RRF_SUBKEY_WOW6432KEY, (DWORD)RRF_SUBKEY_WOW6464KEY}) {
        auto loc = reg_string(HKEY_LOCAL_MACHINE, uninstall, L"InstallLocation", view);
        if (!loc.empty()) out->push_back({path(loc), "Steam uninstall entry"});
    }

    // Epic writes one JSON .item per installed game.
    auto programdata = known_folder(FOLDERID_ProgramData);
    if (!programdata.empty()) {
        path manifests = path(programdata) / L"Epic" / L"EpicGamesLauncher" / L"Data" / L"Manifests";
        std::error_code ec;
        if (dir_exists(manifests))
            for (wcmg::fs::directory_iterator it(manifests, ec), end; !ec && it != end; it.increment(ec)) {
                if (wcmg::lower(wcmg::path_utf8(it->path().extension())) != ".item") continue;
                auto j = nlohmann::json::parse(wcmg::read_text(it->path(), 4 * 1024 * 1024), nullptr, false);
                if (!j.is_object()) continue;
                auto name = j.value("DisplayName", std::string());
                auto loc = j.value("InstallLocation", std::string());
                if (loc.empty() || !wcmg::icontains(name, "Mortal Shell")) continue;
                out->push_back({wcmg::detail::path_from_utf8(loc), "Epic Games manifest " + wcmg::path_utf8(it->path().filename())});
            }
    }

    // Last resort: the usual folder names on every fixed drive.
    const wchar_t* guesses[] = {L"Program Files (x86)\\Steam\\steamapps\\common\\Sparta", L"Program Files\\Steam\\steamapps\\common\\Sparta",
                                L"SteamLibrary\\steamapps\\common\\Sparta", L"Steam\\steamapps\\common\\Sparta",
                                L"Games\\SteamLibrary\\steamapps\\common\\Sparta", L"Games\\Sparta", L"Games\\Mortal Shell II",
                                L"Epic Games\\MortalShell2", L"Program Files\\Epic Games\\MortalShell2"};
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(drives & (1u << i))) continue;
        wchar_t root[] = {wchar_t(L'A' + i), L':', L'\\', 0};
        if (GetDriveTypeW(root) != DRIVE_FIXED) continue;
        for (auto* g : guesses) {
            path p = path(root) / g;
            if (dir_exists(p)) out->push_back({p, std::string("found on drive ") + char('A' + i) + ":"});
        }
    }
}

extern "C" int wcmg_win_under_wine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") ? 1 : 0;
}

namespace wcmg::win {

std::optional<std::wstring> pick_folder(HWND owner, const std::wstring& start) {
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        std::optional<std::wstring> result;
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dlg->SetTitle(L"Pick the Mortal Shell II game folder");
        if (!start.empty()) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&item)))) {
                dlg->SetFolder(item);
                item->Release();
            }
        }
        HRESULT hr = dlg->Show(owner);
        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) result = p;
                CoTaskMemFree(p);
                item->Release();
            }
        }
        dlg->Release();
        if (result || hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return result;
    }

    BROWSEINFOW bi{};
    bi.hwndOwner = owner;
    bi.lpszTitle = L"Pick the Mortal Shell II game folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return std::nullopt;
    wchar_t buf[MAX_PATH * 4] = L"";
    bool ok = SHGetPathFromIDListEx(pidl, buf, (DWORD)std::size(buf), GPFIDL_DEFAULT) != FALSE;
    CoTaskMemFree(pidl);
    if (!ok) return std::nullopt;
    return std::wstring(buf);
}
}  // namespace wcmg::win

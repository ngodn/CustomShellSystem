// WCMG.exe entry point. With --cli it runs the console report like wcmg_cli.exe;
// otherwise it opens the window, optionally on a path from the command line.
#include "win_common.hpp"
#include "scanners.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <cstdio>
#include <iostream>
#include <objbase.h>

namespace {
using namespace wcmg;

bool usable(DWORD which) {
    HANDLE h = GetStdHandle(which);
    return h && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN;
}

// A GUI exe has no console of its own. Keep redirected handles (> file, | more)
// and borrow the parent's console for the rest. Returns true when we had to make
// a console, so the caller can wait before it vanishes.
bool hook_up_console() {
    bool out = usable(STD_OUTPUT_HANDLE), err = usable(STD_ERROR_HANDLE);
    bool made = false;
    if (!out || !err) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) made = AllocConsole() != FALSE;
        FILE* f = nullptr;
        if (!out) freopen_s(&f, "CONOUT$", "w", stdout);
        if (!err) freopen_s(&f, "CONOUT$", "w", stderr);
        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::cerr.clear();
    }
    SetConsoleOutputCP(CP_UTF8);
    return made;
}

// Same behaviour as src/cli/main.cpp, with wide arguments.
int run_cli(int argc, wchar_t** argv) {
    bool made_console = hook_up_console();
    auto finish = [&](int code) {
        std::fflush(stdout);
        std::fflush(stderr);
        if (made_console) {
            std::fprintf(stderr, "\nPress Enter to close.\n");
            (void)std::getchar();
        }
        return code;
    };
    fs::path target, zip;
    bool json = false, md = false;
    std::optional<fs::path> knowledge;
    for (int i = 2; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--json") json = true;
        else if (a == L"--md") md = true;
        else if (a == L"--zip" && i + 1 < argc) zip = argv[++i];
        else if (a == L"--knowledge" && i + 1 < argc) knowledge = fs::path(argv[++i]);
        else target = a;
    }
    std::optional<Paths> paths;
    if (!target.empty()) paths = paths_from_user_pick(target);
    else if (auto found = discover_installs(); !found.empty()) paths = found.front();
    if (!paths) {
        std::fprintf(stderr, "Game not found. Pass the Sparta folder, the MortalShell2 folder, the Win64 folder or the exe.\n");
        return finish(2);
    }
    std::string why;
    if (!validate_paths(*paths, &why)) { std::fprintf(stderr, "%s\n", why.c_str()); return finish(2); }
    auto progress = [](const std::string& s) { std::fprintf(stderr, "... %s\n", s.c_str()); };
    auto report = diagnose(*paths, knowledge, progress);
    std::string out = json ? report_json(report) : md ? report_markdown(report) : report_text(report);
    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
    if (!zip.empty()) {
        try {
            auto written = collect_bundle(report, zip, progress);
            std::fprintf(stderr, "Bundle written: %s\n", path_utf8(written).c_str());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Bundle failed: %s\n", e.what());
            return finish(3);
        }
    }
    return finish(0);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 2 && std::wstring(argv[1]) == L"--cli") {
        int code = run_cli(argc, argv);
        LocalFree(argv);
        return code;
    }
    std::wstring initial = argv && argc >= 2 ? argv[1] : L"";
    if (argv) LocalFree(argv);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES};
    InitCommonControlsEx(&icc);
    int code = wcmg::win::run_gui(inst, show, initial);
    CoUninitialize();
    return code;
}

#pragma once
// Shared between the Win32 files of WCMG.exe: string conversion and the few
// entry points each file offers the others.
#include <windows.h>
#include <optional>
#include <string>

namespace wcmg::win {

inline std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

inline std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

// discovery_win.cpp: the folder picker (IFileOpenDialog, SHBrowseForFolder as fallback).
std::optional<std::wstring> pick_folder(HWND owner, const std::wstring& start);

// gui.cpp: the main window. `initial_path` non-empty means start a diagnosis on it.
int run_gui(HINSTANCE inst, int show, const std::wstring& initial_path);
}

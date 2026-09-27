#include "common.hpp"
#include <fstream>
#include <sstream>
#include <windows.h>

namespace ccs::runtime {

std::string narrow(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring widen(const std::string& narrow_str) {
    if (narrow_str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, narrow_str.data(), (int)narrow_str.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrow_str.data(), (int)narrow_str.size(), out.data(), size);
    return out;
}

std::string read_file_text(const fs::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream.is_open()) return {};
    std::ostringstream ss;
    ss << stream.rdbuf();
    return ss.str();
}

bool write_file_atomic(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp_path = path;
    tmp_path += ".tmp";

    {
        std::ofstream stream(tmp_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!stream.is_open()) return false;
        stream.write(content.data(), content.size());
        stream.flush();
    }

    fs::rename(tmp_path, path, ec);
    if (ec) {
        fs::copy_file(tmp_path, path, fs::copy_options::overwrite_existing, ec);
        fs::remove(tmp_path, ec);
    }
    return !ec;
}

} // namespace ccs::runtime

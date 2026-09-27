#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

namespace ccs::runtime {

namespace fs = std::filesystem;

inline uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string narrow(const std::wstring& wide);
std::wstring widen(const std::string& narrow);

std::string read_file_text(const fs::path& path);
bool write_file_atomic(const fs::path& path, const std::string& content);

} // namespace ccs::runtime

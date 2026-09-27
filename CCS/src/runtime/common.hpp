#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <cstdint>

namespace ccs::runtime {

namespace fs = std::filesystem;

inline uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool valid_preset_name(const std::string& name);

std::string read_file_text(const fs::path& path);
bool write_file_atomic(const fs::path& path, const std::string& content);
enum class FileWriteResult { Success, Exists, Failed };
FileWriteResult write_file_atomic(const fs::path& path, const std::string& content, bool replace_existing);

} // namespace ccs::runtime

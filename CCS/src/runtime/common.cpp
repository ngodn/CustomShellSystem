#include "common.hpp"
#include <fstream>
#include <atomic>
#include <algorithm>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace ccs::runtime {

bool valid_preset_name(const std::string& name) {
    if (name.empty() || name.size() > 96 || name.front() == '.' || name.back() == '.') return false;
    for (unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    auto stem = name.substr(0, name.find('.'));
    for (char& c : stem) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") return false;
    return !(stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
             stem[3] >= '1' && stem[3] <= '9');
}

std::string read_file_text(const fs::path& path) {
    constexpr size_t limit = 1024 * 1024;
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return {};
    const auto size = stream.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(limit)) return {};
    std::string text(static_cast<size_t>(size), '\0');
    stream.seekg(0);
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    return stream ? text : std::string{};
}

namespace {
struct TemporaryFile {
    fs::path path;
    bool owned{};
#ifdef _WIN32
    HANDLE handle{INVALID_HANDLE_VALUE};
    bool close() {
        const auto value = std::exchange(handle, INVALID_HANDLE_VALUE);
        return value == INVALID_HANDLE_VALUE || CloseHandle(value) != 0;
    }
#else
    int handle{-1};
    bool close() {
        const auto value = std::exchange(handle, -1);
        return value < 0 || ::close(value) == 0;
    }
#endif
    ~TemporaryFile() {
        close();
        if (owned) { std::error_code ec; fs::remove(path, ec); }
    }
};
}

bool write_file_atomic(const fs::path& path, const std::string& content) {
    return write_file_atomic(path, content, true) == FileWriteResult::Success;
}

FileWriteResult write_file_atomic(const fs::path& path, const std::string& content, bool replace_existing) {
    static std::atomic<uint64_t> sequence{};
    std::error_code ec;
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path(), ec);
    if (ec) return FileWriteResult::Failed;
    TemporaryFile temporary;
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    for (unsigned attempt = 0; attempt < 8 && !temporary.owned; ++attempt) {
        temporary.path = path;
        temporary.path += ".tmp." + std::to_string(pid) + "." + std::to_string(sequence.fetch_add(1));
#ifdef _WIN32
        temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        temporary.owned = temporary.handle != INVALID_HANDLE_VALUE;
        if (!temporary.owned && GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
            return FileWriteResult::Failed;
#else
        temporary.handle = ::open(temporary.path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        temporary.owned = temporary.handle >= 0;
        if (!temporary.owned && errno != EEXIST) return FileWriteResult::Failed;
#endif
    }
    if (!temporary.owned) return FileWriteResult::Failed;
    size_t offset{};
    while (offset < content.size()) {
        const auto size = std::min(content.size() - offset, size_t{65536});
#ifdef _WIN32
        DWORD written{};
        if (!WriteFile(temporary.handle, content.data() + offset, static_cast<DWORD>(size), &written, nullptr) || !written)
            return FileWriteResult::Failed;
#else
        const auto written = ::write(temporary.handle, content.data() + offset, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return FileWriteResult::Failed;
#endif
        offset += static_cast<size_t>(written);
    }
    if (!temporary.close()) return FileWriteResult::Failed;
#ifdef _WIN32
    if (MoveFileExW(temporary.path.c_str(), path.c_str(), replace_existing ? MOVEFILE_REPLACE_EXISTING : 0))
        return FileWriteResult::Success;
    const auto error = GetLastError();
    return !replace_existing && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) ? FileWriteResult::Exists : FileWriteResult::Failed;
#else
    if (replace_existing) fs::rename(temporary.path, path, ec);
    else fs::create_hard_link(temporary.path, path, ec);
    if (!ec) return FileWriteResult::Success;
    return !replace_existing && ec == std::errc::file_exists ? FileWriteResult::Exists : FileWriteResult::Failed;
#endif
}

} // namespace ccs::runtime

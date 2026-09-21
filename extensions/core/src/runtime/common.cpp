#include "common.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace cssx {
bool valid_id(const std::string& s) {
    return !s.empty() && s.size() <= 96 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    }) && s != "." && s != "..";
}
Json read_json(const fs::path& path) {
    if (fs::file_size(path) > 1024 * 1024) throw std::runtime_error("JSON exceeds 1 MiB: " + path_utf8(path));
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read: " + path_utf8(path));
    return Json::parse(in);
}
void atomic_json(const fs::path& path, const Json& data, bool backup, bool verify) {
    fs::create_directories(path.parent_path());
    auto temp = path; temp += ".tmp";
    const auto bytes = data.dump(2) + "\n";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) throw std::runtime_error("Cannot write " + path_utf8(path));
    }
    if (verify && read_json(temp) != data) throw std::runtime_error("Read-back mismatch writing " + path_utf8(path));
    if (backup && fs::exists(path)) {
        auto previous = path; previous += ".bak";
        fs::copy_file(path, previous, fs::copy_options::overwrite_existing);
    }
#ifdef _WIN32
    auto file = CreateFileW(temp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot flush " + path_utf8(path));
    const bool flushed = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (!flushed || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot atomically replace " + path_utf8(path));
#else
    fs::rename(temp, path);
#endif
}
std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm time{};
#ifdef _WIN32
    gmtime_s(&time, &seconds);
#else
    gmtime_r(&seconds, &time);
#endif
    std::ostringstream out;
    out << std::put_time(&time, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
        << (std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000) << 'Z';
    return out.str();
}
uint64_t monotonic_us() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

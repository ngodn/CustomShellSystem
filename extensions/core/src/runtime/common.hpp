#pragma once
// Portable helpers shared by the runtime, the loader, the tests and the tools.
// No engine or Windows-only dependency belongs here.
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <stdexcept>

namespace cssx {
using Json = nlohmann::json;
namespace fs = std::filesystem;

inline std::string path_utf8(const fs::path& path) {
    auto text = path.u8string();
    return std::string(text.begin(), text.end());
}
inline fs::path utf8_path(const std::string& text) {
    return fs::path(std::u8string(text.begin(), text.end()));
}
// Identifiers used in file names and JSON keys: letters, digits, '.', '_', '-'.
bool valid_id(const std::string&);
// Read a JSON file of at most 1 MiB.
Json read_json(const fs::path&);
// Write JSON through a temp file, read it back, keep an optional .bak of the
// previous file, then replace atomically (MoveFileEx on Windows, rename elsewhere).
void atomic_json(const fs::path&, const Json&, bool backup = true, bool verify = true);
// UTC ISO-8601 timestamp with milliseconds.
std::string utc_timestamp();
// Monotonic microseconds.
uint64_t monotonic_us();
}

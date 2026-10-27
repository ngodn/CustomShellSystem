#pragma once
// Minimal test harness: CHECK(cond) counts failures and keeps going; main() in
// each test file reports the total. Real-installation tests read WCMG_GAME_DIR
// and WCMG_SAVED_DIR from the environment and skip quietly when unset, so the
// suite also passes on a machine without the game.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace wcmg_test {
inline int failures = 0;
inline int checks = 0;
#define CHECK(cond) do { ++wcmg_test::checks; if (!(cond)) { ++wcmg_test::failures; std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { ++wcmg_test::checks; if (!((a) == (b))) { ++wcmg_test::failures; std::fprintf(stderr, "FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); } } while (0)
inline int finish(const char* name) {
    std::printf("%s: %d checks, %d failures\n", name, checks, failures);
    return failures ? 1 : 0;
}
inline std::optional<std::filesystem::path> env_path(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) return std::nullopt;
    std::filesystem::path p(v);
    if (!std::filesystem::exists(p)) return std::nullopt;
    return p;
}
// Writes a scratch file under the test's temp dir and returns its path.
inline std::filesystem::path scratch_dir() {
    auto d = std::filesystem::temp_directory_path() / "wcmg-tests";
    std::filesystem::create_directories(d);
    return d;
}
}

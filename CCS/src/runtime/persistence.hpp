#pragma once
#include "ccs_types.hpp"
#include <array>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ccs::runtime {
enum class FileOperation : uint8_t { ListPresets, LoadPreset, SavePreset, DeletePreset, SaveSettings };

struct FileRequest {
    FileOperation operation{FileOperation::ListPresets};
    std::string name;
    std::string payload;
    bool replace_existing{};
};

struct FileResult {
    uint64_t id{};
    FileOperation operation{};
    std::string name;
    bool success{};
    bool exists{};
    std::string error;
    std::vector<std::string> names;
    std::optional<PresetData> preset;
};

class Persistence {
public:
    static constexpr size_t capacity = 32;
    static constexpr size_t payload_limit = 64 * 1024;
    explicit Persistence(std::filesystem::path root);
    ~Persistence();
    Persistence(const Persistence&) = delete;
    Persistence& operator=(const Persistence&) = delete;

    // Completed results occupy capacity until consumed. Disk work never holds the gate.
    std::optional<uint64_t> submit(FileRequest request);
    std::optional<FileResult> poll();
    void close();

private:
    enum class State { Free, Queued, Running, Complete };
    struct Entry {
        State state{State::Free};
        uint64_t id{};
        FileRequest request;
        FileResult result;
    };
    void run();
    FileResult execute(uint64_t id, const FileRequest& request);
    std::filesystem::path root_;
    std::mutex gate_;
    std::condition_variable ready_;
    std::array<Entry, capacity> entries_;
    uint64_t next_id_{1};
    bool closed_{};
    // Joined before queue, condition variable and mutex destruction.
    std::jthread worker_;
};
}

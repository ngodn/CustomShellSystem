#pragma once
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <filesystem>
#include <cstdint>

namespace ccs::runtime {

class Writer {
public:
    explicit Writer(std::filesystem::path log_file);
    ~Writer();

    bool write(std::string message);
    bool flush();
    enum class DrainState { Pending, Complete, Failed };
    DrainState drain_state();

private:
    void worker_loop();

    std::filesystem::path path_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
    bool stop_{false};
    bool failed_{false};
    uint64_t submitted_{0};
    uint64_t completed_{0};
    static constexpr size_t queue_limit = 1024;
    static constexpr size_t message_limit = 64 * 1024;
    std::thread thread_;
};

} // namespace ccs::runtime

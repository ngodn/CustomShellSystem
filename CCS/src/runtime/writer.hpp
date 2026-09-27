#pragma once
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <filesystem>

namespace ccs::runtime {

class Writer {
public:
    explicit Writer(std::filesystem::path log_file);
    ~Writer();

    void write(std::string message);
    void flush();

private:
    void worker_loop();

    std::filesystem::path path_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
    bool stop_{false};
    std::thread thread_;
};

} // namespace ccs::runtime

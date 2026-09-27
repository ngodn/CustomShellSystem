#pragma once
// A coalescing status file: the game thread hands over the latest text, a worker replaces the
// file atomically. Only the newest snapshot is ever written; the game thread never touches disk.
#include "common.hpp"
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace ccs::runtime {
class StatusWriter {
public:
    explicit StatusWriter(std::filesystem::path path) : path_(std::move(path)), worker_([this](std::stop_token stop) { run(stop); }) {}
    ~StatusWriter() { { std::lock_guard lock(mutex_); worker_.request_stop(); } ready_.notify_all(); }
    StatusWriter(const StatusWriter&) = delete;
    StatusWriter& operator=(const StatusWriter&) = delete;
    void publish(std::string text) {
        if (text.size() > 1024 * 1024) return;
        { std::lock_guard lock(mutex_); pending_ = std::move(text); has_pending_ = true; }
        ready_.notify_one();
    }
private:
    void run(std::stop_token stop) {
        while (true) {
            std::string text;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [&] { return has_pending_ || stop.stop_requested(); });
                if (!has_pending_) return;
                text = std::move(pending_); has_pending_ = false;
            }
            write_file_atomic(path_, text, true);
        }
    }
    std::filesystem::path path_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::string pending_;
    bool has_pending_{};
    std::jthread worker_;
};
}

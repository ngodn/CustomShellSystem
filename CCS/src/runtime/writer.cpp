#include "writer.hpp"
#include <fstream>

namespace ccs::runtime {

Writer::Writer(std::filesystem::path log_file) : path_(std::move(log_file)) {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    thread_ = std::thread(&Writer::worker_loop, this);
}

Writer::~Writer() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool Writer::write(std::string message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_ || queue_.size() >= queue_limit || message.size() > message_limit) return false;
        queue_.push(std::move(message));
        ++submitted_;
    }
    cv_.notify_one();
    return true;
}

Writer::DrainState Writer::drain_state() {
    std::lock_guard lock(mutex_);
    if (failed_) return DrainState::Failed;
    return completed_ >= submitted_ ? DrainState::Complete : DrainState::Pending;
}

bool Writer::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto target = submitted_;
    cv_.wait(lock, [this, target] { return completed_ >= target; });
    return !failed_;
}

void Writer::worker_loop() {
    std::ofstream out;
    while (true) {
        std::string item;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) break;
            item = std::move(queue_.front());
            queue_.pop();
        }

        bool written = false;
        try {
            if (!out.is_open()) out.open(path_, std::ios::out | std::ios::app);
            if (out.is_open()) {
                out << item << "\n";
                out.flush();
                written = static_cast<bool>(out);
            }
        } catch (...) {}
        {
            std::lock_guard lock(mutex_);
            failed_ = failed_ || !written;
            ++completed_;
        }
        cv_.notify_all();
    }
}

} // namespace ccs::runtime

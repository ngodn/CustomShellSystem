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

// The log rotates at four megabytes: the current file becomes ".1" (replacing the previous
// one) and a fresh file starts, so a long session or a repeating warning cannot grow it forever.
void Writer::worker_loop() {
    std::ofstream out;
    constexpr uintmax_t rotate_bytes = uintmax_t(4) << 20;
    uintmax_t bytes = 0;
    auto rotate = [&] {
        out.close();
        std::error_code ec;
        auto previous = path_; previous += ".1";
        std::filesystem::remove(previous, ec);
        std::filesystem::rename(path_, previous, ec);
        bytes = 0;
    };
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
            if (!out.is_open()) {
                std::error_code ec;
                bytes = std::filesystem::file_size(path_, ec);
                if (ec) bytes = 0;
                if (bytes > rotate_bytes) rotate();
                out.open(path_, std::ios::out | std::ios::app);
            }
            if (out.is_open()) {
                out << item << "\n";
                out.flush();
                written = static_cast<bool>(out);
                bytes += item.size() + 1;
                if (bytes > rotate_bytes) rotate();
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

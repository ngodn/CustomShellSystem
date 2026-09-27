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

void Writer::write(std::string message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(message));
    }
    cv_.notify_one();
}

void Writer::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return queue_.empty(); });
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

        if (!out.is_open()) {
            out.open(path_, std::ios::out | std::ios::app);
        }
        if (out.is_open()) {
            out << item << "\n";
            out.flush();
        }
    }
}

} // namespace ccs::runtime

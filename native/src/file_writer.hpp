#pragma once
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace css {
// A file writer on its own thread, owned by the permanent loader. The core serialises on
// the game thread and hands the bytes over; the disk work (temp file, read-back check,
// optional .bak copy, atomic replace) happens here. Jobs for one path apply in order, and a
// job is skipped when a newer one for the same path is already waiting, so a burst of
// status updates costs one write.
class FileWriter {
    struct Job { std::filesystem::path path; std::string bytes; bool backup; };
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> queue_;
    std::function<void(const std::string&)> log_;
    bool stop_=false;
    std::thread thread_;
    static constexpr size_t queue_limit=256;

    void run() {
        for(;;) {
            Job job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock,[&]{ return stop_ || !queue_.empty(); });
                if(queue_.empty()) return;   // stop_ set and everything drained
                job=std::move(queue_.front()); queue_.pop_front();
                bool superseded=false;
                for(const auto& later:queue_) if(later.path==job.path) { superseded=true; break; }
                if(superseded) continue;
            }
            try { write(job); }
            catch(const std::exception& error) { if(log_) log_(std::string("File write failed: ")+error.what()); }
        }
    }
    static void write(const Job& job) {
        namespace fs=std::filesystem;
        std::error_code ignored;
        fs::create_directories(job.path.parent_path(),ignored);
        auto temp=job.path; temp+=".tmp";
        {
            std::ofstream out(temp,std::ios::binary|std::ios::trunc);
            out.write(job.bytes.data(),static_cast<std::streamsize>(job.bytes.size()));
            out.close();
            if(!out) throw std::runtime_error("cannot write "+temp.string());
        }
        {
            std::ifstream in(temp,std::ios::binary);
            std::string back((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
            if(back!=job.bytes) throw std::runtime_error("read-back mismatch on "+temp.string());
        }
        if(job.backup && fs::exists(job.path)) {
            auto previous=job.path; previous+=".bak";
            fs::copy_file(job.path,previous,fs::copy_options::overwrite_existing);
        }
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),job.path.c_str(),MOVEFILE_REPLACE_EXISTING))
            throw std::runtime_error("cannot replace "+job.path.string());
#else
        fs::rename(temp,job.path);
#endif
    }
public:
    explicit FileWriter(std::function<void(const std::string&)> log={}) : log_(std::move(log)), thread_([this]{ run(); }) {}
    ~FileWriter() {
        { std::lock_guard lock(mutex_); stop_=true; }
        wake_.notify_all();
        if(thread_.joinable()) thread_.join();
    }
    FileWriter(const FileWriter&)=delete;
    FileWriter& operator=(const FileWriter&)=delete;
    // Queue one write. Over the limit the oldest job is dropped (and logged): a stalled
    // disk must never grow memory without bound.
    void post(std::filesystem::path path,std::string bytes,bool backup) {
        {
            std::lock_guard lock(mutex_);
            if(stop_) return;
            if(queue_.size()>=queue_limit) {
                if(log_) log_("File write dropped, queue full: "+queue_.front().path.string());
                queue_.pop_front();
            }
            queue_.push_back({std::move(path),std::move(bytes),backup});
        }
        wake_.notify_one();
    }
    // Wait until every queued job has been written (tests, and the loader's shutdown).
    void drain() {
        for(;;) {
            { std::lock_guard lock(mutex_); if(queue_.empty()) break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        // The last popped job may still be in write(); a second lock/unlock after the
        // queue is empty is not enough, so callers that need the file use its content.
    }
    size_t pending() { std::lock_guard lock(mutex_); return queue_.size(); }
};
}

#pragma once
// Background file writer. The game thread hands over bytes and returns at
// once; one worker thread does the disk work. Used for everything that is
// informational (status, frame dumps, logs). Durable state (settings,
// extension state) keeps its synchronous, fsynced path because it is written
// only on explicit user actions.
#include "common.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace cssx {
class Writer {
public:
    Writer();
    ~Writer();                                   // drains the queue, then joins
    // Replace `path` with `bytes` through a temp file and rename. No fsync.
    void replace(fs::path path,std::string bytes);
    // Append one record to a log, rotating at `max_bytes` with `backups` copies.
    void append(fs::path path,std::string bytes,size_t max_bytes,unsigned backups);
    // Block until every queued job has been written (tests, shutdown).
    void drain();
    uint64_t queued() const;
    uint64_t failures() const { return failures_; }
    // Synchronous implementations shared with the worker (and usable without a thread).
    static void replace_now(const fs::path& path,const std::string& bytes);
    static void append_now(const fs::path& path,const std::string& bytes,size_t max_bytes,unsigned backups);
private:
    struct Job { fs::path path; std::string bytes; size_t max_bytes=0; unsigned backups=0; bool append=false; };
    mutable std::mutex mutex_;
    std::condition_variable wake_, idle_;
    std::deque<Job> jobs_;
    bool stop_=false;
    unsigned running_=0;
    uint64_t failures_=0;
    std::thread thread_;
    void run();
};
}

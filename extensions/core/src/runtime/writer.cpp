#include "writer.hpp"
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace cssx {
Writer::Writer() {}
void Writer::start() { if(!thread_.joinable()) thread_=std::thread([this]{ run(); }); }
Writer::~Writer() {
    { std::lock_guard lock(mutex_); stop_=true; }
    wake_.notify_all();
    if(thread_.joinable()) thread_.join();
}
void Writer::replace(fs::path path,std::string bytes) {
    if(!running()) { replace_now(path,bytes); return; }
    { std::lock_guard lock(mutex_); if(stop_) return;
      // Coalesce: a newer replace of the same file supersedes a queued one.
      for(auto& job:jobs_) if(!job.append && job.path==path) { job.bytes=std::move(bytes); return; }
      jobs_.push_back({std::move(path),std::move(bytes),0,0,false}); }
    wake_.notify_one();
}
void Writer::append(fs::path path,std::string bytes,size_t max_bytes,unsigned backups) {
    if(!running()) { append_now(path,bytes,max_bytes,backups); return; }
    { std::lock_guard lock(mutex_); if(stop_) return; jobs_.push_back({std::move(path),std::move(bytes),max_bytes,backups,true}); }
    wake_.notify_one();
}
void Writer::drain() {
    if(!running()) return;
    std::unique_lock lock(mutex_);
    idle_.wait(lock,[this]{ return jobs_.empty() && running_==0; });
}
uint64_t Writer::queued() const { std::lock_guard lock(mutex_); return jobs_.size()+running_; }
void Writer::run() {
    for(;;) {
        Job job;
        { std::unique_lock lock(mutex_);
          wake_.wait(lock,[this]{ return stop_ || !jobs_.empty(); });
          if(jobs_.empty()) { if(stop_) return; continue; }
          job=std::move(jobs_.front()); jobs_.pop_front(); ++running_; }
        try { if(job.append) append_now(job.path,job.bytes,job.max_bytes,job.backups); else replace_now(job.path,job.bytes); }
        catch(...) { std::lock_guard lock(mutex_); ++failures_; }
        { std::lock_guard lock(mutex_); --running_; }
        idle_.notify_all();
    }
}
void Writer::replace_now(const fs::path& path,const std::string& bytes) {
    fs::create_directories(path.parent_path());
    auto temp=path; temp+=".tmp";
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out.write(bytes.data(),std::streamsize(bytes.size())); out.flush(); if(!out) throw std::runtime_error("Cannot write "+path_utf8(path)); }
#ifdef _WIN32
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)) throw std::runtime_error("Cannot replace "+path_utf8(path));
#else
    fs::rename(temp,path);
#endif
}
void Writer::append_now(const fs::path& path,const std::string& line,size_t max_bytes,unsigned backups) {
    fs::create_directories(path.parent_path());
    std::error_code ec;
    if(max_bytes && backups && fs::exists(path,ec) && fs::file_size(path,ec)>0 && fs::file_size(path,ec)+line.size()>max_bytes) {
        auto rotated=[&](unsigned index){ auto p=path; p+="."+std::to_string(index); return p; };
        for(unsigned i=backups;i>1;--i) if(fs::exists(rotated(i-1),ec)) { if(fs::exists(rotated(i),ec)) fs::remove(rotated(i),ec); fs::rename(rotated(i-1),rotated(i),ec); }
        if(fs::exists(rotated(1),ec)) fs::remove(rotated(1),ec);
        fs::rename(path,rotated(1),ec);
    }
    std::ofstream file(path,std::ios::binary|std::ios::app); file.write(line.data(),std::streamsize(line.size())); file.flush();
    if(!file) throw std::runtime_error("Cannot append "+path_utf8(path));
}
}

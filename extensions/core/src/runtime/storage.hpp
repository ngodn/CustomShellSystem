#pragma once
#include "common.hpp"
namespace cssx {
class Writer;
struct LogPolicy { size_t max_bytes=1024*1024; unsigned backups=5; };
// One host thread owns this service. Every append is one escaped JSON record.
class Storage {
    fs::path root_;
    LogPolicy policy_;
    Writer* writer_=nullptr;
public:
    explicit Storage(fs::path root,LogPolicy policy={}):root_(std::move(root)),policy_(policy) {}
    // With a writer, log appends are queued and the calling thread never touches the disk.
    void set_writer(Writer* writer) { writer_=writer; }
    void log(const std::string& id,const std::string& level,const std::string& message,const Json& fields=Json::object());
    fs::path output(const std::string& id,const std::string& filename,const std::string& bytes);
};
}

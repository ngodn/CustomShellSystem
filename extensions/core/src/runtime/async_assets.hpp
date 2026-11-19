#pragma once
#include "common.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>

namespace cssx {
// One engine-owned strong-reference collection per extension request.
class AssetStream {
public:
    virtual ~AssetStream()=default;
    virtual void start(const std::string& path)=0;
    virtual std::optional<uint64_t> poll()=0;  // pending, missing (0), or a resident object handle
    virtual void cancel_path() noexcept=0;
};

class AsyncAssets {
public:
    using Factory=std::function<std::unique_ptr<AssetStream>()>;
    struct Notice { std::string owner; Json value; };
    uint64_t request(const std::string& owner,const Json& paths) {
        if(!paths.is_array() || paths.empty() || paths.size()>32) throw std::runtime_error("Expected 1 to 32 asset paths");
        if(jobs_.size()>=8) throw std::runtime_error("Asset request limit reached");
        size_t owned=0;
        for(const auto& [id,job]:jobs_) if(job.owner==owner) ++owned;
        if(owned>=2) throw std::runtime_error("Extension already holds two asset requests");
        Job job; job.owner=owner;
        std::set<std::string> seen;
        for(const auto& item:paths) {
            auto path=item.get<std::string>();
            auto slash=path.rfind('/'),dot=path.find('.',slash);
            if(path.empty() || path.size()>1024 || path.front()!='/' || slash==0 || dot==path.npos ||
               dot==slash+1 || dot+1==path.size() || path.find("//")!=path.npos ||
               path.find_first_of(R"(\"')")!=path.npos || std::any_of(path.begin(),path.end(),[](unsigned char c){ return c<=32; }))
                throw std::runtime_error("Expected an absolute soft object path");
            if(seen.insert(path).second) job.paths.push_back(std::move(path));
        }
        const auto id=next_++;
        jobs_.emplace(id,std::move(job));
        return id;
    }
    bool release(const std::string& owner,uint64_t id) {
        auto it=jobs_.find(id);
        if(it==jobs_.end() || it->second.owner!=owner) return false;
        jobs_.erase(it);
        return true;
    }
    void clear(const std::string& owner) { std::erase_if(jobs_,[&](const auto& p){ return p.second.owner==owner; }); }
    void clear() { jobs_.clear(); }

    // Visit at most eight small records. Start or poll one path globally per engine frame.
    std::optional<Notice> tick(const Factory& make) {
        auto it=jobs_.upper_bound(last_);
        size_t active=0;
        for(const auto& [id,job]:jobs_) active+=job.loading;
        for(size_t i=0;i<jobs_.size();++i,++it) {
            if(it==jobs_.end()) it=jobs_.begin();
            auto& job=it->second;
            if(job.finished==job.paths.size() || (!job.loading && active>=2)) continue;
            last_=it->first;
            const auto& path=job.paths[job.finished];
            uint64_t object=0;
            try {
                if(!job.loading) {
                    if(!job.stream) job.stream=make();
                    if(!job.stream) throw std::runtime_error("Async asset loader unavailable");
                    job.stream->start(path);
                    job.loading=true;
                    return {};
                }
                auto result=job.stream->poll();
                if(!result) return {};
                object=*result;
            } catch(const std::exception& e) {
                if(job.stream) job.stream->cancel_path();
                job.errors[path]=e.what();
            }
            job.loading=false;
            job.results[path]=object?Json{{"$object",object}}:Json(nullptr);
            if(!object && !job.errors.contains(path)) job.errors[path]="Asset did not load";
            ++job.finished;
            const char* state="progress";
            if(job.finished==job.paths.size()) state=job.errors.empty()?"complete":"failed";
            return Notice{job.owner,{{"handle",last_},{"state",state},{"finished",job.finished},{"total",job.paths.size()},
                                     {"results",job.results},{"errors",job.errors}}};
        }
        return {};
    }
private:
    struct Job {
        std::string owner;
        std::vector<std::string> paths;
        std::unique_ptr<AssetStream> stream;
        size_t finished=0;
        bool loading=false;
        Json results=Json::object(),errors=Json::object();
    };
    std::map<uint64_t,Job> jobs_;
    inline static uint64_t next_=1; // game thread; runtime reload invalidates the old handles
    uint64_t last_=0;
};
}

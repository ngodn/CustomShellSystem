#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

namespace css {
class OwnedMorphs {
    std::map<std::string,float> baseline_, written_;
public:
    bool matches(const std::map<std::string,float>& values) const { return written_==values; }
    void forget() {baseline_.clear();written_.clear();}
    template<class Read,class Write> void restore(Read read,Write write) {
        for(auto it=written_.begin();it!=written_.end();) {
            if(read(it->first)==it->second && baseline_.at(it->first)!=it->second)
                write(it->first,baseline_.at(it->first));
            it=written_.erase(it);
        }
        baseline_.clear();
    }
    // `verify` reads every written weight back (one reflected call per morph). A one-shot
    // expression keeps it; a performance sampled every frame passes it only now and then,
    // since the read-back was most of the per-frame cost on a 128-morph face.
    template<class Read,class Write> void apply(const std::map<std::string,float>& values,Read read,Write write,bool verify=true) {
        if(matches(values)) return;
        for(const auto& [name,weight]:values)
            if(name.empty() || !std::isfinite(weight) || std::abs(weight)>4.f)
                throw std::runtime_error("Invalid facial weight");
        if(baseline_.size()!=values.size() || std::any_of(values.begin(),values.end(),
            [&](const auto& value){return !baseline_.contains(value.first);})) {
            restore(read,write);
            std::map<std::string,float> captured;
            for(const auto& [name,weight]:values) {
                const float current=read(name);
                if(!std::isfinite(current)) throw std::runtime_error("Invalid original facial weight");
                captured.emplace(name,current);
            }
            baseline_=std::move(captured);
        }
        try {
            for(const auto& [name,weight]:values) {
                if(const auto prior=written_.find(name);prior!=written_.end() && prior->second==weight) continue;
                // Read the live weight only for a shape this owner has not written yet; the
                // argument of try_emplace would be evaluated (one reflected call) either way.
                auto entry=written_.find(name);
                if(entry==written_.end()) entry=written_.emplace(name,read(name)).first;
                const auto previous=entry->second;
                if(previous==weight) continue;
                entry->second=weight;
                try {write(name,weight);}
                catch(...) {
                    if(read(name)!=weight) entry->second=previous;
                    throw;
                }
                if(!verify) continue;
                const auto actual=read(name);
                if(actual!=weight) {
                    entry->second=actual;
                    throw std::runtime_error("Facial weight read-back failed");
                }
            }
        } catch(...) {restore(read,write);throw;}
    }
};
}

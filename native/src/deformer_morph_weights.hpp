#pragma once
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

namespace css {
// A graph opts in with zero-default float variables named CSS_Morph_<morph>.
// Missing variables are remembered until its instance changes.
class DeformerMorphWeights {
    std::map<std::string,std::optional<float>> applied_;
public:
    void forget() { applied_.clear(); }
    template<class Write>
    void sync(const std::map<std::string,float>& desired,Write write) {
        for(const auto& [name,value]:desired)
            if(name.empty() || !std::isfinite(value))
                throw std::runtime_error("Invalid mesh deformer morph weight");
        for(auto it=applied_.begin();it!=applied_.end();) {
            if(desired.contains(it->first)) { ++it;continue; }
            if(it->second && !write(it->first,0.f))
                throw std::runtime_error("Mesh deformer morph reset failed");
            it=applied_.erase(it);
        }
        for(const auto& [name,value]:desired) {
            auto it=applied_.find(name);
            if(it!=applied_.end() && (!it->second || *it->second==value)) continue;
            if(write(name,value)) applied_.insert_or_assign(name,value);
            else if(it==applied_.end()) applied_.emplace(name,std::nullopt);
            else throw std::runtime_error("Mesh deformer morph update failed");
        }
    }
};
}

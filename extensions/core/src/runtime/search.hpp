#pragma once
#include "manifest.hpp"
#include <functional>
#include <sstream>

namespace cssx {
// Build once per options snapshot. Query changes scan cached labels, never
// create engine widgets or request extension data from the filter loop.
class OptionSearch {
    std::vector<std::string> keys_;
    std::function<std::string(const std::string&)> fold_;
    std::string query_;
public:
    Json options=Json::array();
    std::vector<size_t> matches;
    size_t selected=0;
    static std::string ascii_fold(std::string text) {
        for(auto& c:text) if(c>='A' && c<='Z') c+=32;
        return text;
    }
    void reset(const Json& source,std::function<std::string(const std::string&)> fold=ascii_fold) {
        if(!source.is_array() || source.size()>512) throw std::runtime_error("Option search exceeds 512 choices");
        options=source;fold_=std::move(fold);keys_.clear();keys_.reserve(source.size());
        for(const auto& item:source) keys_.push_back(fold_(item.at("label").get<std::string>()+" "+item.at("id").get<std::string>()));
        query_="\xff";filter("");
    }
    bool filter(const std::string& query) {
        if(query.size()>1024) throw std::runtime_error("Search text exceeds 1024 bytes");
        const auto folded=fold_(query);if(folded==query_) return false;
        query_=folded;std::istringstream input(folded);std::vector<std::string> words;
        for(std::string word;input>>word;) words.push_back(std::move(word));
        matches.clear();
        for(size_t i=0;i<keys_.size();++i) {
            bool matched=true;for(const auto& word:words) if(keys_[i].find(word)==std::string::npos) {matched=false;break;}
            if(matched) matches.push_back(i);
        }
        selected=0;return true;
    }
    void move(int delta) {selected=matches.empty()?0:size_t(std::clamp(int(selected)+delta,0,int(matches.size())-1));}
    Json value() const {return matches.empty()?Json{}:options[matches.at(selected)].at("id");}
};
}

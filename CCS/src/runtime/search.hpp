#pragma once
// The picker's search: built once per options snapshot; query changes scan cached labels and
// never touch engine widgets.
#include <nlohmann/json.hpp>
#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ccs {
class OptionSearch {
    using Json = nlohmann::json;
    std::vector<std::string> keys_;
    std::function<std::string(const std::string&)> fold_;
    std::string query_;
public:
    Json options = Json::array();
    std::vector<size_t> matches;
    size_t selected = 0;
    static std::string ascii_fold(std::string text) {
        for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        return text;
    }
    void reset(const Json& source, std::function<std::string(const std::string&)> fold = ascii_fold) {
        if (!source.is_array() || source.size() > 4096) throw std::runtime_error("Option search exceeds 4096 choices");
        options = source; fold_ = std::move(fold); keys_.clear(); keys_.reserve(source.size());
        for (const auto& item : source) keys_.push_back(fold_(item.at("label").get<std::string>() + " " + item.at("id").get<std::string>()));
        query_ = "\xff"; filter("");
    }
    bool filter(const std::string& query) {
        if (query.size() > 1024) throw std::runtime_error("Search text exceeds 1024 bytes");
        const auto folded = fold_(query); if (folded == query_) return false;
        query_ = folded; std::istringstream input(folded); std::vector<std::string> words;
        for (std::string word; input >> word;) words.push_back(std::move(word));
        matches.clear();
        for (size_t i = 0; i < keys_.size(); ++i) {
            bool matched = true; for (const auto& word : words) if (keys_[i].find(word) == std::string::npos) { matched = false; break; }
            if (matched) matches.push_back(i);
        }
        selected = 0; return true;
    }
    void move(int delta) { selected = matches.empty() ? 0 : size_t(std::clamp(int(selected) + delta, 0, int(matches.size()) - 1)); }
    Json value() const { return matches.empty() ? Json{} : options[matches.at(selected)].at("id"); }
};
}

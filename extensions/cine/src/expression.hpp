#pragma once
#include "performance.hpp"
#include <map>

namespace cine {
std::string expression_choice(const Json&);
class ExpressionCatalog {
    std::map<std::string, std::string> entries_;
    std::string error_ = "Refresh expressions after choosing your outfit in CSS.";
public:
    void refresh(const PerformanceCall&);
    Json options(const std::string&) const;
    std::string unavailable(const std::string&) const;
    std::string description(const std::string&) const;
};

class ExpressionTake {
    Json scope_;
    std::string before_, desired_;
    bool restoring_ = false;
    bool same_look(const Json&) const;
public:
    void start(const PerformanceCall&, const std::string&);
    bool ready(const PerformanceCall&) const;
    // False means CSS is still applying a change. Keep ownership and retry later.
    bool stop(const PerformanceCall&);
    bool owned() const { return !scope_.is_null(); }
};
}

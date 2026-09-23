#pragma once
#include <cmath>
#include <optional>
#include <stdexcept>

namespace css {
// Preserve the first height so outfit changes cannot accumulate an offset.
class GroundOffset {
    std::optional<double> original_, applied_;
public:
    static bool valid(double offset) { return std::isfinite(offset) && std::abs(offset)<=10; }
    double set(double current,double offset) {
        if(!std::isfinite(current) || !valid(offset)) throw std::runtime_error("Invalid ground offset");
        if(!original_) original_=current;
        if(applied_ && std::abs(current-*applied_)>0.1 && std::abs(current-*original_)>0.1)
            throw std::runtime_error("World mesh height changed outside CSS");
        applied_=*original_+offset;
        return *applied_;
    }
    std::optional<double> restore(double current) {
        const auto result=(applied_ && std::abs(current-*applied_)<=0.1)?original_:std::nullopt;
        original_.reset();applied_.reset();
        return result;
    }
};
}

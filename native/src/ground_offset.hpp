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
    // `authored` is the height the pawn's class gives its mesh, when known. A fresh
    // baseline anchors to it when the mesh sits there, or exactly one `offset` from it
    // (a previous core already applied this offset), so nothing ever stacks.
    double set(double current,double offset,std::optional<double> authored=std::nullopt) {
        if(!std::isfinite(current) || !valid(offset)) throw std::runtime_error("Invalid ground offset");
        if(!original_) {
            const bool anchored=authored && std::isfinite(*authored) &&
                (std::abs(current-*authored)<=0.1 || std::abs(current-(*authored+offset))<=0.1);
            original_=anchored?*authored:current;
        }
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

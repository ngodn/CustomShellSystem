#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace css {
class MeshScale {
    using Vector = std::array<double,3>;
    std::optional<Vector> baseline_;
    Vector applied_{};
    Vector apply(const Vector& current,double factor) {
        for(double value:current) if(!std::isfinite(value) || value<=0 || value>100)
            throw std::runtime_error("Invalid mesh scale baseline");
        if(!baseline_ || !same(current,applied_)) baseline_=current;
        for(size_t i=0;i<3;++i) applied_[i]=(*baseline_)[i]*factor;
        return applied_;
    }
public:
    static bool valid(double scale) { return std::isfinite(scale) && scale>=0.1 && scale<=4.; }
    static bool same(const Vector& a,const Vector& b) {
        for(size_t i=0;i<3;++i) if(!std::isfinite(a[i]) || !std::isfinite(b[i]) || std::abs(a[i]-b[i])>1e-6) return false;
        return true;
    }
    Vector set(const Vector& current,double factor) {
        if(!valid(factor)) throw std::runtime_error("Invalid appearance scale");
        return apply(current,factor);
    }
    Vector compensate(const Vector& current,double body_factor) {
        if(!valid(body_factor)) throw std::runtime_error("Invalid appearance scale");
        return apply(current,1./body_factor);
    }
    std::optional<Vector> restore(const Vector& current) {
        auto result=baseline_ && same(current,applied_)?baseline_:std::nullopt;
        baseline_.reset();
        return result;
    }
};
}

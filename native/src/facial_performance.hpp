#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace css {
struct FacialCurveKey {
    double seconds = 0.;
    float value = 0.f;
};
struct FacialCurve {
    std::string morph;
    std::vector<FacialCurveKey> keys;
};
struct FacialPerformance {
    std::string clip;
    double duration = 0.;
    std::vector<FacialCurve> curves;
    static FacialPerformance parse(const nlohmann::json&, std::span<const std::string> morphs);
    size_t key_count() const noexcept;
    // Caller supplies the body clock and reusable buffers, in curve order.
    // Zero weight restores the baseline exactly; endpoint extrapolation is constant.
    void sample(double seconds, double weight, std::span<const float> baseline,
                std::span<float> output) const;
};
}

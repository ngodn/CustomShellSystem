#pragma once
#include <array>
#include <string>
#include <nlohmann/json.hpp>

namespace css {
using BodyVector = std::array<double,3>;
struct BodyGeometry {
    std::array<BodyVector,7> offsets{}, contact_centers{};
    std::array<float,7> moments{};
    // Per region, three parent-local columns of the contact ellipsoid basis.
    std::array<std::array<BodyVector,3>,7> contact_axes{};
    bool operator==(const BodyGeometry&) const = default;
};
struct BodyGeometryModel {
    struct Helper {
        std::string contact_bone;
        BodyVector offset{};
        std::array<BodyVector,6> offset_deltas{};
        double moment=0;
        std::array<double,6> moment_linear{};
        std::array<std::array<double,6>,6> moment_quadratic{};
        std::array<BodyVector,64> contact_centers{};
        std::array<double,64> contact_scales{};
        std::array<BodyVector,3> contact_axes{};
    };
    std::array<std::string,6> morphs;
    std::array<Helper,7> helpers;
    static BodyGeometryModel parse(const nlohmann::json&);
    BodyGeometry evaluate(const std::array<float,6>&) const;
};
}

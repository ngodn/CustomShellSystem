#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
using BodyVector = std::array<double,3>;
// The measured body model is multilinear in 6 to 8 morph axes (2^n corners).
// Schema 1 is the six-axis model cooked before the axis list was declared;
// schema 2 records the axes ("morphs") and the corner count ("corners").
inline constexpr std::size_t body_geometry_min_axes=6, body_geometry_max_axes=8;
struct BodyGeometry {
    // The morph values the geometry was evaluated at, one per model axis. Empty when
    // the AnimBP has no CSSBodyShape inputs.
    std::vector<float> shape;
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
        std::vector<BodyVector> offset_deltas;                 // one per axis
        double moment=0;
        std::vector<double> moment_linear;                     // one per axis
        std::vector<std::vector<double>> moment_quadratic;     // axes x axes, symmetric
        std::vector<BodyVector> contact_centers;               // one per corner
        std::vector<double> contact_scales;                    // one per corner
        std::array<BodyVector,3> contact_axes{};
        bool operator==(const Helper&) const = default;
    };
    int schema=2;
    std::vector<std::string> morphs;
    std::array<Helper,7> helpers;
    std::size_t axes() const { return morphs.size(); }
    std::size_t corners() const { return std::size_t{1}<<morphs.size(); }
    bool operator==(const BodyGeometryModel&) const = default;
    static BodyGeometryModel parse(const nlohmann::json&);
    BodyGeometry evaluate(const std::vector<float>&) const;
};
}

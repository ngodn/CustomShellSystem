#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
using BodyVector = std::array<double,3>;
// The measured body model is multilinear in 6 to 8 morph axes (2^n corners).
// Schema 1 stores six axes, schema 2 declares the axis count, and schema 3
// gives each axis its own contact-table domain.
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
// Highest body morph weight the geometry model evaluates; outfits may extend a slider past its authored morph.
inline constexpr float max_body_morph_weight=3.0f;

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
    std::vector<float> axis_maxima;
    std::array<Helper,7> helpers;
    std::size_t axes() const { return morphs.size(); }
    std::size_t corners() const { return std::size_t{1}<<morphs.size(); }
    bool operator==(const BodyGeometryModel&) const = default;
    static BodyGeometryModel parse(const nlohmann::json&);
    std::vector<float> bounded_weights(std::vector<float>) const;
    BodyGeometry evaluate(const std::vector<float>&) const;
private:
    float axis_maximum(std::size_t axis) const;
};
}

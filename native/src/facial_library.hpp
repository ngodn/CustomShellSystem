#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace css {
struct FacialExpression {
    std::string id, name;
    std::map<std::string,float> weights;
};
struct FacialMeshAdapter {
    std::string source_outfit, source_mesh, target_mesh, skeleton, source_toc_sha256;
    uint64_t source_bulk_bytes = 0;
};
struct FacialRig {
    std::string id;
    std::vector<std::string> morphs;
    std::vector<FacialExpression> expressions;
    std::vector<FacialMeshAdapter> adapters;
    static FacialRig parse(const nlohmann::json&);
    const FacialExpression* find(const std::string&) const;
    std::map<std::string,float> weights(const std::string&) const;
};
struct FacialMatch {
    const FacialRig* rig = nullptr;
    const FacialMeshAdapter* adapter = nullptr;
};
FacialMatch resolve_facial_rig(const std::vector<FacialRig>&, const std::string& outfit,
    const std::string& toc_sha256, uint64_t bulk_bytes, const std::string& source_mesh);
}

#pragma once
#include "facial_performance.hpp"
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
    std::vector<FacialPerformance> performances;
    static FacialRig parse(const nlohmann::json&);
    const FacialExpression* find(const std::string&) const;
    const FacialPerformance* find_performance(const std::string&) const;
    std::map<std::string,float> weights(const std::string&) const;
};
struct FacialMatch {
    const FacialRig* rig = nullptr;
    const FacialMeshAdapter* adapter = nullptr;
};
struct FacialPlan {
    std::string source_mesh, mesh, skeleton, expression, reason;
    std::map<std::string,float> weights;
};
FacialPlan plan_expression(FacialMatch,const std::string& source_mesh,const std::string& expression);
FacialMatch resolve_facial_rig(const std::vector<FacialRig>&, const std::string& outfit,
    const std::string& toc_sha256, uint64_t bulk_bytes, const std::string& source_mesh);
}

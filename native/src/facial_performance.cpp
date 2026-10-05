#include "facial_performance.hpp"
#include "data.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace css {
namespace {
void fields(const Json& value, const std::set<std::string>& allowed) {
    if(!value.is_object()) throw std::runtime_error("Facial performance must be an object");
    for(const auto& [key,_]:value.items()) if(!allowed.contains(key))
        throw std::runtime_error("Unknown facial performance field: "+key);
}
double number(const Json& value) {
    if(!value.is_number()) throw std::runtime_error("Expected a numeric facial key");
    const auto result=value.get<double>();
    if(!std::isfinite(result)) throw std::runtime_error("Nonfinite facial key");
    return result;
}
float evaluate(const FacialCurve& curve, double seconds) {
    const auto next=std::upper_bound(curve.keys.begin(),curve.keys.end(),seconds,
        [](double time,const FacialCurveKey& key){return time<key.seconds;});
    if(next==curve.keys.begin()) return next->value;
    if(next==curve.keys.end()) return curve.keys.back().value;
    const auto& prior=*(next-1);
    const double alpha=(seconds-prior.seconds)/(next->seconds-prior.seconds);
    return static_cast<float>(std::lerp(double(prior.value),double(next->value),alpha));
}
}
FacialPerformance FacialPerformance::parse(const Json& value,std::span<const std::string> morphs) {
    fields(value,{"clip","duration","curves"});
    FacialPerformance result;
    result.clip=value.at("clip").get<std::string>();
    if(result.clip.empty() || result.clip.size()>96 || !valid_id(result.clip) ||
       result.clip=="original" || result.clip=="off")
        throw std::runtime_error("Invalid facial performance identity");
    result.duration=number(value.at("duration"));
    if(result.duration<=0. || result.duration>86400.) throw std::runtime_error("Invalid facial performance duration");
    const auto& curves=value.at("curves");
    if(!curves.is_array() || curves.empty() || curves.size()>128)
        throw std::runtime_error("Invalid facial performance curve count");
    std::set<std::string> used;
    size_t total=0;
    for(const auto& item:curves) {
        fields(item,{"morph","keys"});
        FacialCurve curve;
        curve.morph=item.at("morph").get<std::string>();
        if(std::find(morphs.begin(),morphs.end(),curve.morph)==morphs.end() ||
           !used.insert(curve.morph).second)
            throw std::runtime_error("Unknown or repeated performance morph");
        const auto& keys=item.at("keys");
        if(!keys.is_array() || keys.empty() || keys.size()>16384 || (total+=keys.size())>65536)
            throw std::runtime_error("Invalid facial performance key count");
        curve.keys.reserve(keys.size());
        for(const auto& key:keys) {
            if(!key.is_array() || key.size()!=2) throw std::runtime_error("Expected facial key time and value");
            const auto seconds=number(key[0]), weight=number(key[1]);
            if(seconds<0. || seconds>result.duration+1.e-5 || std::abs(weight)>4. ||
               (!curve.keys.empty() && seconds<=curve.keys.back().seconds))
                throw std::runtime_error("Unordered or out-of-range facial key");
            curve.keys.push_back({seconds,static_cast<float>(weight)});
        }
        result.curves.push_back(std::move(curve));
    }
    return result;
}
size_t FacialPerformance::key_count() const noexcept {
    size_t result=0;
    for(const auto& curve:curves) result+=curve.keys.size();
    return result;
}
void FacialPerformance::sample(double seconds,double weight,std::span<const float> baseline,
                              std::span<float> output) const {
    if(!std::isfinite(seconds) || !std::isfinite(weight) || weight<0. || weight>1. ||
       baseline.size()!=curves.size() || output.size()!=curves.size() ||
       std::any_of(baseline.begin(),baseline.end(),[](float v){return !std::isfinite(v) || std::abs(v)>4.f;}))
        throw std::invalid_argument("Invalid facial sampling buffers, time or weight");
    for(size_t i=0;i<curves.size();++i)
        output[i]=weight==0. ? baseline[i] :
            static_cast<float>(std::lerp(double(baseline[i]),double(evaluate(curves[i],seconds)),weight));
}
FacialPerformancePose::FacialPerformancePose(const FacialPerformance& performance,
    const std::map<std::string,float>& baseline):performance_(performance),baseline_(baseline),values_(baseline) {
    if(baseline.empty() || baseline.size()>128 || std::any_of(baseline.begin(),baseline.end(),
        [](const auto& pair){return pair.first.empty() || !std::isfinite(pair.second) || std::abs(pair.second)>4.f;}))
        throw std::invalid_argument("Invalid facial performance baseline");
    for(const auto& curve:performance_.curves) curve_baseline_.push_back(baseline_.at(curve.morph));
    curve_values_.resize(curve_baseline_.size());
}
const std::map<std::string,float>& FacialPerformancePose::sample(double seconds,double weight) {
    performance_.sample(seconds,weight,curve_baseline_,curve_values_);
    // Shapes absent from this clip fade to zero, rather than retaining part of
    // the selected static expression throughout the authored performance.
    for(auto& [name,value]:values_) value=static_cast<float>(double(baseline_.at(name))*(1.-weight));
    for(size_t i=0;i<performance_.curves.size();++i) values_.at(performance_.curves[i].morph)=curve_values_[i];
    return values_;
}
}

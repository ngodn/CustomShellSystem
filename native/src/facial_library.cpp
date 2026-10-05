#include "facial_library.hpp"
#include "data.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>

namespace css {
namespace {
void fields(const Json& value, const std::set<std::string>& allowed) {
    if(!value.is_object()) throw std::runtime_error("Facial catalog entry must be an object");
    for(const auto& [key,_]:value.items()) if(!allowed.contains(key))
        throw std::runtime_error("Unknown facial catalog field: "+key);
}
std::string text(const Json& value,const char* key,size_t limit) {
    const auto result=value.at(key).get<std::string>();
    if(result.empty() || result.size()>limit || std::any_of(result.begin(),result.end(),[](unsigned char c){return c<32 || c==127;}))
        throw std::runtime_error("Invalid facial catalog text");
    return result;
}
std::string identity(const Json& value,const char* key) {
    auto result=text(value,key,96);
    if(!valid_id(result) || result=="original" || result=="off") throw std::runtime_error("Invalid facial identity");
    return result;
}
std::string asset(const Json& value,const char* key) {
    const auto result=text(value,key,512);
    if(!valid_asset(result)) throw std::runtime_error("Invalid facial asset");
    return result;
}
std::string folded(std::string name) {
    for(auto& c:name) if(c>='A' && c<='Z') c+=('a'-'A');
    return name;
}
}
FacialRig FacialRig::parse(const Json& value) {
    fields(value,{"id","morphs","expressions","adapters","performances"});
    FacialRig rig; rig.id=identity(value,"id");
    const auto& morphs=value.at("morphs");
    if(!morphs.is_array() || morphs.empty() || morphs.size()>128) throw std::runtime_error("Invalid facial morph count");
    std::set<std::string> names, canonical;
    for(const auto& item:morphs) {
        const auto name=item.get<std::string>();
        if(name.empty() || name.size()>64 || !std::all_of(name.begin(),name.end(),[](unsigned char c){
            return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_';
        }) || !names.insert(folded(name)).second) throw std::runtime_error("Invalid or duplicate facial morph");
        canonical.insert(name); rig.morphs.push_back(name);
    }
    const auto& expressions=value.at("expressions");
    if(!expressions.is_array() || expressions.empty() || expressions.size()>128)
        throw std::runtime_error("Invalid expression count");
    std::set<std::string> ids;
    for(const auto& item:expressions) {
        fields(item,{"id","name","weights"});
        FacialExpression expression; expression.id=identity(item,"id"); expression.name=text(item,"name",96);
        if(!ids.insert(expression.id).second) throw std::runtime_error("Duplicate facial expression");
        const auto& weights=item.at("weights");
        if(!weights.is_object() || weights.size()>rig.morphs.size()) throw std::runtime_error("Invalid expression weights");
        for(const auto& [name,weight]:weights.items()) {
            if(!canonical.contains(name) || !weight.is_number()) throw std::runtime_error("Unknown expression morph or nonnumeric weight");
            const double number=weight.get<double>();
            if(!std::isfinite(number) || std::abs(number)>4.) throw std::runtime_error("Invalid expression weight");
            expression.weights.emplace(name,static_cast<float>(number));
        }
        rig.expressions.push_back(std::move(expression));
    }
    if(value.contains("performances")) {
        const auto& performances=value.at("performances");
        if(!performances.is_array() || performances.size()>64)
            throw std::runtime_error("Invalid facial performance count");
        std::set<std::string> clips;
        size_t keys=0;
        for(const auto& item:performances) {
            auto performance=FacialPerformance::parse(item,rig.morphs);
            if(!clips.insert(performance.clip).second || (keys+=performance.key_count())>262144)
                throw std::runtime_error("Duplicate facial performance or excessive rig key count");
            rig.performances.push_back(std::move(performance));
        }
    }
    const auto& adapters=value.at("adapters");
    if(!adapters.is_array() || adapters.empty() || adapters.size()>32) throw std::runtime_error("Invalid facial adapter count");
    std::set<std::tuple<std::string,std::string,std::string>> sources;
    for(const auto& item:adapters) {
        fields(item,{"source_outfit","source_mesh","target_mesh","skeleton","source_toc_sha256","source_bulk_bytes"});
        FacialMeshAdapter adapter;
        adapter.source_outfit=identity(item,"source_outfit"); adapter.source_mesh=asset(item,"source_mesh");
        adapter.target_mesh=asset(item,"target_mesh"); adapter.skeleton=asset(item,"skeleton");
        adapter.source_toc_sha256=text(item,"source_toc_sha256",64);
        if(!item.at("source_bulk_bytes").is_number_unsigned()) throw std::runtime_error("Invalid facial source bulk size");
        adapter.source_bulk_bytes=item.at("source_bulk_bytes").get<uint64_t>();
        const auto& hash=adapter.source_toc_sha256;
        if(hash.size()!=64 || !std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}) ||
            !adapter.source_bulk_bytes || !adapter.target_mesh.starts_with("/Game/CSS/Locomotion/Faces/") ||
            adapter.target_mesh==adapter.source_mesh || !sources.emplace(adapter.source_outfit,hash,adapter.source_mesh).second)
            throw std::runtime_error("Invalid or duplicate facial mesh adapter");
        rig.adapters.push_back(std::move(adapter));
    }
    return rig;
}
const FacialExpression* FacialRig::find(const std::string& id) const {
    const auto it=std::find_if(expressions.begin(),expressions.end(),[&](const auto& e){return e.id==id;});
    return it==expressions.end()?nullptr:&*it;
}
const FacialPerformance* FacialRig::find_performance(const std::string& id) const {
    const auto it=std::find_if(performances.begin(),performances.end(),[&](const auto& p){return p.clip==id;});
    return it==performances.end()?nullptr:&*it;
}
std::map<std::string,float> FacialRig::weights(const std::string& id) const {
    const auto* expression=find(id);
    if(!expression) throw std::runtime_error("Unknown expression");
    std::map<std::string,float> result;
    for(const auto& name:morphs) result.emplace(name,0.f);
    for(const auto& pair:expression->weights) result.at(pair.first)=pair.second;
    return result;
}
FacialMatch resolve_facial_rig(const std::vector<FacialRig>& rigs,const std::string& outfit,
    const std::string& hash,uint64_t bulk,const std::string& mesh) {
    FacialMatch result;
    if(hash.empty() || !bulk) return result;
    for(const auto& rig:rigs) for(const auto& adapter:rig.adapters) {
        if(adapter.source_outfit!=outfit || adapter.source_toc_sha256!=hash || adapter.source_bulk_bytes!=bulk || adapter.source_mesh!=mesh) continue;
        if(result.rig) throw std::runtime_error("Conflicting facial mesh adapters");
        result={&rig,&adapter};
    }
    return result;
}
FacialPlan plan_expression(FacialMatch match,const std::string& source,const std::string& expression) {
    FacialPlan result{source,source,{},expression,{}, {}};
    if(expression.empty()) return result;
    if(!match.rig || !match.adapter || match.adapter->source_mesh!=source) {
        result.reason="No compatible facial library for this outfit version";
        return result;
    }
    if(!match.rig->find(expression)) {
        result.reason="The saved facial expression is unavailable";
        return result;
    }
    result.mesh=match.adapter->target_mesh;
    result.skeleton=match.adapter->skeleton;
    result.weights=match.rig->weights(expression);
    return result;
}
FacialPlan plan_performance_expression(FacialMatch match,const std::string& source,
    const std::string& expression,const std::string& clip) {
    auto result=plan_expression(match,source,expression);
    if(!expression.empty() || !match.rig || !match.adapter || match.adapter->source_mesh!=source ||
       !match.rig->find_performance(clip)) return result;
    result.mesh=match.adapter->target_mesh;
    result.skeleton=match.adapter->skeleton;
    for(const auto& name:match.rig->morphs) result.weights.emplace(name,0.f);
    return result;
}
}

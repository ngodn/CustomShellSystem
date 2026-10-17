#include "hand_support_catalog.hpp"
#include <fstream>
#include <set>
#include <stdexcept>

namespace css::hand_support {
namespace {
using Json=nlohmann::json;
void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
std::string text(const Json& row,const char* key) {
    const auto& value=row.at(key);
    require(value.is_string(),"Hand catalog field must be a string");
    auto result=value.get<std::string>();
    require(!result.empty() && result.size()<=512,"Hand catalog string exceeds bounds");
    return result;
}
std::string id(const Json& row,const char* key) {
    auto value=text(row,key);
    require(value.size()<=64 && value.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string::npos,
            "Invalid hand catalog identifier");
    return value;
}
std::string asset(const Json& row,const char* key,bool is_class=false) {
    auto value=text(row,key);
    const auto dot=value.find('.');
    require(value.starts_with("/Game/") && value.find_first_not_of(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_/.")==std::string::npos &&
        value.find("..") == std::string::npos && value.find("//")==std::string::npos &&
        dot!=std::string::npos && dot>value.rfind('/') && dot+1<value.size() &&
        value.find('.',dot+1)==std::string::npos && (!is_class || value.ends_with("_C")),
        "Invalid hand catalog asset path");
    return value;
}
const Json& rows(const Json& document,const char* key,size_t limit) {
    const auto& value=document.at(key);
    require(value.is_array() && !value.empty() && value.size()<=limit,"Hand catalog list exceeds bounds");
    return value;
}
template<size_t N> std::array<double,N> numbers(const Json& value,double limit) {
    require(value.is_array() && value.size()==N,"Hand catalog vector has the wrong length");
    std::array<double,N> result;
    for(size_t i=0;i<N;++i) {
        require(value[i].is_number(),"Hand catalog vector must contain numbers");
        result[i]=value[i].get<double>();
        require(std::isfinite(result[i]) && std::abs(result[i])<=limit,"Hand catalog number exceeds bounds");
    }
    return result;
}
Quat rotation(const Json& value) {
    auto q=numbers<4>(value,1.);
    double norm=0.;for(auto v:q) norm+=v*v;
    require(std::abs(norm-1.)<1e-5,"Hand catalog quaternion must be normalized");
    return q;
}
}

Catalog Catalog::load(const std::filesystem::path& path) {
    constexpr size_t max_bytes=1024*1024;
    std::ifstream input(path,std::ios::binary);
    require(bool(input),"Cannot open hand-support.json");
    std::string bytes(max_bytes+1,'\0');
    input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    require(!input.bad() && input.gcount()<=static_cast<std::streamsize>(max_bytes),"Hand catalog exceeds 1 MiB");
    bytes.resize(static_cast<size_t>(input.gcount()));
    std::vector<std::set<std::string>> objects;
    auto reject_duplicates=[&](int depth,Json::parse_event_t event,Json& value) {
        require(depth<=16,"Hand catalog nesting exceeds bounds");
        if(event==Json::parse_event_t::object_start) objects.emplace_back();
        else if(event==Json::parse_event_t::object_end) objects.pop_back();
        else if(event==Json::parse_event_t::key)
            require(objects.back().insert(value.get<std::string>()).second,"Duplicate hand catalog JSON key");
        return true;
    };
    return parse(Json::parse(bytes,reject_duplicates));
}
Catalog Catalog::parse(const Json& document) {
    require(document.is_object() && document.at("schema").is_number_integer() &&
            document.at("schema")==1 && document.at("rig_contract")=="shoulder-support-v1",
            "Unsupported hand catalog schema or rig contract");
    require(document.at("target_bones")==Json(right_bones),"Hand target order differs from the cooked rig contract");
    Catalog result;
    const auto& settings=document.at("carry");
    result.carry_.wrist_height=numbers<2>(settings.at("wrist_height"),100.);
    result.carry_.prop_local_z=numbers<2>(settings.at("prop_local_z"),100.);
    result.carry_.moving_phase=numbers<2>(settings.at("moving_phase"),100.);
    for(const auto& range:{result.carry_.wrist_height,result.carry_.prop_local_z,result.carry_.moving_phase})
        require(range[0]<range[1],"Hand carry range is empty or reversed");
    auto seconds=[&](const char* key) {
        const auto& value=settings.at(key);
        require(value.is_number(),"Hand blend duration must be numeric");
        const auto number=value.get<double>();
        require(std::isfinite(number) && number>0. && number<=1.,"Hand blend duration exceeds bounds");
        return number;
    };
    result.carry_.blend_in_seconds=seconds("blend_in_seconds");
    result.carry_.blend_out_seconds=seconds("blend_out_seconds");
    result.carry_.max_delta_seconds=seconds("max_delta_seconds");
    std::set<std::string> weapons;
    for(const auto& row:rows(document,"weapons",64)) {
        auto name=id(row,"id"),path=asset(row,"class",true);
        require(weapons.insert(name).second && result.weapons_.emplace(path,name).second,"Duplicate hand weapon identity");
    }
    for(const auto& row:rows(document,"targets",1024)) {
        const auto name=id(row,"id"),weapon=id(row,"weapon");
        require(weapons.contains(weapon),"Hand target references an unknown weapon");
        const auto& targets=row.at("standing");
        require(targets.is_array() && targets.size()==right_bones.size(),"Hand target count differs from rig contract");
        Profile profile;
        for(size_t i=0;i<profile.standing.size();++i) profile.standing[i]=rotation(targets[i]);
        require(result.profiles_[name].emplace(weapon,profile).second,"Duplicate hand target profile");
    }
    for(const auto& row:rows(document,"grips",1024)) {
        const auto name=id(row,"id"),weapon=id(row,"weapon");
        require(weapons.contains(weapon),"Hand grip references an unknown weapon");
        Grip grip{rotation(row.at("rotation")),numbers<3>(row.at("translation"),100.)};
        require(result.grips_[name].emplace(weapon,grip).second,"Duplicate hand grip profile");
    }
    std::set<std::pair<std::string,std::string>> pairs;
    for(const auto& row:rows(document,"bindings",256)) {
        Binding b{asset(row,"mesh"),asset(row,"animation",true),asset(row,"original",true),
                  asset(row,"candidate",true),id(row,"finger_profile"),id(row,"grip_profile"),{}};
        require(b.original!=b.candidate && pairs.emplace(b.mesh,b.animation).second,"Ambiguous hand rig binding");
        require(result.profiles_.contains(b.finger_profile) && result.grips_.contains(b.grip_profile),
                "Hand binding references an unknown calibration profile");
        const auto& profiles=result.profiles_.at(b.finger_profile);
        const auto& grips=result.grips_.at(b.grip_profile);
        require(profiles.size()==grips.size(),"Hand binding has mismatched target and grip weapons");
        for(const auto& [weapon,profile]:profiles)
            require(grips.contains(weapon),"Hand binding is missing a matching weapon grip");
        std::set<std::string> bones;
        for(const auto& value:rows(row,"output_bones",64)) {
            require(value.is_string(),"Hand output bone must be a string");
            const auto bone=value.get<std::string>();
            require(!bone.empty() && bone.size()<=64 && bone.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos &&
                bones.insert(bone).second,"Invalid or duplicate hand output bone");
            b.output_bones.push_back(bone);
        }
        for(auto bone:right_bones) require(bones.contains(std::string(bone)),"Hand binding omits a finger output");
        for(auto bone:arm_bones) require(bones.contains(std::string(bone)),"Hand binding omits an arm output");
        result.bindings_.push_back(std::move(b));
    }
    return result;
}
const Binding* Catalog::match(std::string_view mesh,std::string_view animation) const {
    for(const auto& binding:bindings_) if(binding.mesh==mesh && binding.animation==animation) return &binding;
    return nullptr;
}
std::string_view Catalog::weapon_class(std::string_view path) const {
    const auto found=weapons_.find(path);
    return found==weapons_.end()?std::string_view{}:found->second;
}
const Profile* Catalog::profile(std::string_view id,std::string_view weapon) const {
    const auto group=profiles_.find(id);
    if(group==profiles_.end()) return nullptr;
    const auto found=group->second.find(weapon);
    return found==group->second.end()?nullptr:&found->second;
}
const Grip* Catalog::grip(std::string_view id,std::string_view weapon) const {
    const auto group=grips_.find(id);
    if(group==grips_.end()) return nullptr;
    const auto found=group->second.find(weapon);
    return found==group->second.end()?nullptr:&found->second;
}
}

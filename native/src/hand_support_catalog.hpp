#pragma once
#include "hand_support.hpp"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace css::hand_support {
struct Binding {
    std::string mesh,animation,original,candidate,finger_profile,grip_profile;
    std::vector<std::string> output_bones;
};
struct Profile { Targets standing; };
struct Grip { Quat rotation; std::array<double,3> translation; };
class Catalog {
    template<class T> using ByWeapon=std::map<std::string,T,std::less<>>;
    ByWeapon<std::string> weapons_;
    ByWeapon<ByWeapon<Profile>> profiles_;
    ByWeapon<ByWeapon<Grip>> grips_;
    std::vector<Binding> bindings_;
    CarrySettings carry_;
public:
    static Catalog parse(const nlohmann::json&);
    static Catalog load(const std::filesystem::path&);
    const CarrySettings& carry() const {return carry_;}
    const std::vector<Binding>& bindings() const {return bindings_;}
    const Binding* match(std::string_view mesh,std::string_view animation) const;
    std::string_view weapon_class(std::string_view path) const;
    const Profile* profile(std::string_view id,std::string_view weapon) const;
    const Grip* grip(std::string_view id,std::string_view weapon) const;
};
}

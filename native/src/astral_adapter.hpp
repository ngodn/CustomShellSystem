#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace css {
enum class AstralMaterialFamily { authored, uber, eye, smoke, refraction };
struct AstralMaterialDefinition {
    std::string_view root, companion, alternate;
    std::array<uint32_t,4> state_id;
    AstralMaterialFamily family;
    uint8_t blend;
    bool companion_two_sided;
    float clip;
    std::map<std::string,bool> defaults;
};
struct AstralMaterialState {
    uint8_t blend=0;
    bool two_sided=false, replaced_layers=false, other_base_override=false;
    float clip=.3333f;
    std::map<std::string,bool> switches;
};

inline std::string astral_material_rejection(const AstralMaterialDefinition& spec,
                                           const AstralMaterialState& state) {
    if(state.replaced_layers) return "material layers replaced";
    if(state.other_base_override) return "unsupported base property override";
    if(!std::isfinite(state.clip) || state.clip<0.f || state.clip>1.f) return "invalid clip threshold";
    if(spec.family!=AstralMaterialFamily::uber) {
        if(state.blend!=spec.blend) return "blend mode differs from companion";
        if(state.blend==1 && std::abs(state.clip-spec.clip)>1.e-6f) return "authored mask threshold differs";
        if(state.switches!=spec.defaults) return "static switches differ from companion";
        return {};
    }
    if(state.blend>1) return "unsupported Uber blend mode";
    auto matches=[&](const char* name,bool value) {
        const auto found=state.switches.find(name);
        return found!=state.switches.end() && found->second==value;
    };
    if(!matches("USE VIRTUAL TEXTURES",false) || !matches("UseBaseColorMap",true) ||
       !matches("BaseColorAdjust",false)) return "unsupported Uber albedo permutation";
    if(state.blend==1 && (!matches("USE OPACITY",true) ||
       !matches("Opacity from Color Map",true) || !matches("Use Opacity Dither",false)))
        return "unsupported Uber coverage permutation";
    return {};
}

inline std::string astral_material_companion(const AstralMaterialDefinition& spec,
                                            const AstralMaterialState& state) {
    if(!astral_material_rejection(spec,state).empty()) return {};
    std::string path(state.two_sided==spec.companion_two_sided?spec.companion:spec.alternate);
    if(spec.family==AstralMaterialFamily::uber && state.blend==1) {
        constexpr std::string_view opaque="M_Uber_opaque",masked="M_Uber_masked";
        size_t position=0;
        while((position=path.find(opaque,position))!=std::string::npos) {
            path.replace(position,opaque.size(),masked);position+=masked.size();
        }
    }
    return path;
}
}

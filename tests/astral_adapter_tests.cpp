#include "astral_adapter.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace css;
#include "astral_material_catalog.inl"

void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    std::set<std::string_view> roots;
    size_t checked=0;
    for(const auto& spec:astral_material_definitions) {
        check(roots.insert(spec.root).second,"Duplicate material root");
        check(spec.companion.starts_with("/Game/CSS/SharedAssets/") &&
              spec.alternate.starts_with("/Game/CSS/SharedAssets/"),"Companion escaped shared namespace");
        check(spec.companion!=spec.alternate,"Culling alternatives share the same asset");
        check(std::any_of(spec.state_id.begin(),spec.state_id.end(),[](auto value){return value!=0;}),
              "Source graph identity is missing");
        AstralMaterialState state{spec.blend,spec.companion_two_sided,false,false,spec.clip,spec.defaults};
        if(spec.family==AstralMaterialFamily::uber) {
            check(!astral_material_rejection(spec,state).empty(),"Virtual texture root was accepted without a VT adapter");
            state.switches["USE VIRTUAL TEXTURES"]=false;
            state.switches["UseBaseColorMap"]=true;
        }
        check(astral_material_companion(spec,state)==spec.companion,"Supported source did not select its parent");
        state.two_sided=!state.two_sided;
        check(astral_material_companion(spec,state)==spec.alternate,"Instance culling override was lost");
        for(int change=0;change<5;++change) {
            auto invalid=state;
            if(change==0) invalid.replaced_layers=true;
            if(change==1) invalid.other_base_override=true;
            if(change==2) invalid.clip=std::numeric_limits<float>::quiet_NaN();
            if(change==3) invalid.clip=-.1f;
            if(change==4) invalid.clip=1.1f;
            check(astral_material_companion(spec,invalid).empty(),"Unsupported material returned a usable companion");
        }
        if(spec.family==AstralMaterialFamily::uber) {
            state.blend=1;
            check(astral_material_companion(spec,state).empty(),"Masked Uber accepted without coverage configuration");
            state.switches["USE OPACITY"]=true;
            state.switches["Opacity from Color Map"]=true;
            state.switches["Use Opacity Dither"]=false;
            for(bool two_sided:{false,true}) {
                state.two_sided=two_sided;
                const std::string name=two_sided?"MI_M_Uber_masked_TwoSided":"M_Uber_masked";
                check(astral_material_companion(spec,state)==
                    "/Game/CSS/SharedAssets/Astral/Materials/"+name+"."+name,
                    "Masked Uber variant package and object names differ");
                for(float threshold:{0.f,.1f,.3333f,.75f,1.f}) {
                    state.clip=threshold;
                    check(!astral_material_companion(spec,state).empty(),"Runtime Uber clip threshold was rejected");
                }
            }
            for(const auto* key:{"USE VIRTUAL TEXTURES","UseBaseColorMap","BaseColorAdjust",
                                 "USE OPACITY","Opacity from Color Map","Use Opacity Dither"}) {
                auto invalid=state;invalid.switches[key]=!invalid.switches.at(key);
                check(astral_material_companion(spec,invalid).empty(),"Incompatible Uber permutation accepted");
                invalid=state;invalid.switches.erase(key);
                check(astral_material_companion(spec,invalid).empty(),"Unknown Uber switch default accepted");
            }
            state.blend=2;
            check(astral_material_companion(spec,state).empty(),"Translucent Uber accepted without an adapter");
        } else {
            auto invalid=state;invalid.switches["Unrecognized source switch"]=true;
            check(astral_material_companion(spec,invalid).empty(),"New authored shader permutation accepted");
            invalid=state;invalid.blend=uint8_t((state.blend+1)%3);
            check(astral_material_companion(spec,invalid).empty(),"Incompatible authored blend mode accepted");
            if(spec.blend==1) {
                invalid=state;invalid.clip=.7f;
                check(astral_material_companion(spec,invalid).empty(),"Fixed authored mask threshold changed");
            }
        }
        ++checked;
    }
    check(checked==12,"Material catalog coverage changed without updating acceptance");
    std::cout<<"Astral adapter policy passed for "<<checked<<" source graphs\n";
    if(argc==2) {
        std::ifstream input(argv[1]);
        const auto document=nlohmann::json::parse(input);
        size_t accepted=0,rejected=0;
        for(const auto& [path,material]:document.at("materials").items()) {
            std::string root=material.at("chain").back().get<std::string>();
            root+="."+root.substr(root.find_last_of('/')+1);
            const auto spec=std::find_if(astral_material_definitions.begin(),astral_material_definitions.end(),
                [&](const auto& value){return value.root==root;});
            check(spec!=astral_material_definitions.end(),"Readback root has no catalog entry");
            AstralMaterialState state;
            const auto& base=material.at("base");
            const auto blend=base.at("BlendMode").at("value").get<std::string>();
            state.blend=blend=="BLEND_Opaque"?0:blend=="BLEND_Masked"?1:2;
            state.two_sided=base.at("TwoSided").at("value").get<bool>();
            state.clip=base.at("OpacityMaskClipValue").at("value").get<float>();
            state.other_base_override=!material.at("other_base_overrides").empty();
            for(const auto& parameter:material.at("parameters").at("switch")) {
                check(parameter.at("association")=="GlobalParameter" && parameter.at("index")==-1,
                      "Readback has unsupported layer-specific switches");
                state.switches[parameter.at("name").get<std::string>()]=parameter.at("value").get<bool>();
            }
            const auto reason=astral_material_rejection(*spec,state);
            if(reason.empty()) ++accepted;
            else { ++rejected;std::cerr<<path<<": "<<reason<<'\n'; }
        }
        std::cout<<"Cooked source policy: "<<accepted<<" accepted, "<<rejected<<" rejected\n";
        check(accepted>0 && rejected==0,"Released source readbacks rejected by selector");
    }
}

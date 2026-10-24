#include "astral_adapter.hpp"
#include "astral_namespace.hpp"
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
    std::set<std::string_view> alias_roots;
    for(const auto& [legacy,authored]:astral_root_aliases) {
        check(alias_roots.insert(authored).second,"Ambiguous authored material root");
        check(astral_material_root_matches(legacy,authored),"Migrated material root was not recognized");
        check(std::count_if(astral_material_definitions.begin(),astral_material_definitions.end(),
              [&](const auto& spec){return astral_material_root_matches(spec.root,authored);})==1,
              "Migrated root must identify exactly one checked source graph");
        check(!astral_material_root_matches(legacy,std::string(authored)+"_Other"),"Similar root was accepted");
        auto another_author=std::string(authored);
        another_author.replace(another_author.find("eins0fx"),7,"another_author");
        check(!astral_material_root_matches(legacy,another_author),"Unverified author graph was accepted");
    }
    check(!is_astral_pose(std::string(authored_astral_pose)+"_Other"),"Unknown pose class was accepted");
    for(const auto root:{legacy_astral_root,authored_astral_root})
        check(astral_fallback_texture(std::string(root)+"Materials/T_NeutralColor.T_NeutralColor"),
              "Uncopied Astral fallback texture was accepted as a character texture");
    for(const auto path:{"/Game/CSS/eins0fx/EveSB/Hair/T_Color.T_Color",
                         "/Game/CSS/eins0fx/CommanderWhite/Textures/T_BC.T_BC",
                         "/Game/CSS/eins0fx/Shared/Textures/T_Color.T_Color",
                         "/Game/CSS/eins0fx/Shared/AstralOther/T_Color.T_Color",
                         "/Game/CSS/SharedAssetsOther/T_Color.T_Color",
                         "/Game/Sparta/Characters/Textures/T_Color.T_Color"})
        check(!astral_fallback_texture(path),"Character texture was incorrectly rejected as an Astral fallback");
    check(astral_companion_namespace("/Game/CSS/SharedAssets/AstralOther/M.M",authored_astral_pose).empty(),
          "Unrelated shared path crossed the namespace boundary");
    check(astral_companion_namespace("/Game/CSS/SharedAssets/Astral/Materials/M.M","unknown").empty(),
          "Unknown pose selected companion assets");
    constexpr std::string_view peony_companion=
        "/Game/CSS/eins0fx/Shared/Astral/Materials/M_PeonyFabric.M_PeonyFabric";
    check(astral_companion_namespace(peony_companion,authored_astral_pose)==peony_companion,
          "Canonical companion was rejected by the consolidated shared pack");
    check(astral_companion_namespace(peony_companion,legacy_astral_pose).empty(),
          "Canonical-only companion was invented in the legacy pack");
    check(astral_companion_namespace(peony_companion,"unknown").empty(),
          "Canonical companion accepted an unknown pose");
    std::set<std::pair<std::string_view,std::array<uint32_t,4>>> roots;
    size_t checked=0;
    for(const auto& spec:astral_material_definitions) {
        check(roots.insert({spec.root,spec.state_id}).second,"Duplicate material root revision");
        const bool canonical=spec.companion.starts_with(authored_astral_root);
        const auto companion_root=canonical ? authored_astral_root : legacy_astral_root;
        check(spec.companion.starts_with(companion_root) && spec.alternate.starts_with(companion_root),
              "Companion escaped shared namespace");
        check(spec.companion!=spec.alternate,"Culling alternatives share the same asset");
        check(std::any_of(spec.state_id.begin(),spec.state_id.end(),[](auto value){return value!=0;}),
              "Source graph identity is missing");
        std::set<std::string_view> textures;
        for(const auto name:spec.required_textures) {
            check(!name.empty() && !name.starts_with("CSS_"),"Invalid source texture requirement");
            check(textures.insert(name).second,"Duplicate source texture requirement");
        }
        AstralMaterialState state{spec.blend,spec.companion_two_sided,false,false,spec.clip,spec.defaults};
        if(spec.family==AstralMaterialFamily::uber) {
            check(!astral_material_rejection(spec,state).empty(),"Virtual texture root was accepted without a VT adapter");
            state.switches["USE VIRTUAL TEXTURES"]=false;
            state.switches["UseBaseColorMap"]=true;
        }
        check(astral_material_companion(spec,state)==spec.companion,"Supported source did not select its parent");
        for(const auto companion:{spec.companion,spec.alternate}) {
            check(astral_companion_namespace(companion,legacy_astral_pose)==
                  (canonical ? std::string_view{} : companion),
                  "Legacy pack companion changed");
            const auto authored=astral_companion_namespace(companion,authored_astral_pose);
            check(authored==std::string(authored_astral_root)+
                  std::string(companion.substr(companion_root.size())),
                  "Companion did not follow the selected pose pack");
            check(authored.substr(authored.find_last_of('/')+1)==
                  companion.substr(companion.find_last_of('/')+1),"Companion export name changed");
        }
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
                check(astral_companion_namespace(astral_material_companion(spec,state),authored_astral_pose)==
                    std::string(authored_astral_root)+"Materials/"+name+"."+name,
                    "Migrated masked Uber variant selected the wrong companion");
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
    // 23 shipped entries plus Profane Proxima's seven (two additive glow overlays, blend 3).
    check(checked==34,"Material catalog coverage changed without updating acceptance");
    std::cout<<"Astral adapter policy passed for "<<checked<<" source graphs\n";
    if(argc==2) {
        std::ifstream input(argv[1]);
        const auto document=nlohmann::json::parse(input);
        size_t accepted=0,rejected=0;
        for(const auto& [path,material]:document.at("materials").items()) {
            std::string root=material.at("chain").back().get<std::string>();
            root+="."+root.substr(root.find_last_of('/')+1);
            const auto spec=std::find_if(astral_material_definitions.begin(),astral_material_definitions.end(),
                [&](const auto& value){return astral_material_root_matches(value.root,root);});
            check(spec!=astral_material_definitions.end(),"Readback root has no catalog entry");
            for(const auto name:spec->required_textures) {
                const auto& textures=material.at("parameters").at("texture");
                const auto found=std::find_if(textures.begin(),textures.end(),[&](const auto& row) {
                    return row.at("name")==name && row.at("association")=="GlobalParameter" && row.at("index")==-1;
                });
                check(found!=textures.end(),"Required source texture is absent from the cooked material");
                const auto& value=found->at("value");
                const auto path=value.at(value.contains("ObjectPath") ? "ObjectPath" : "AssetPathName")
                                     .template get<std::string>();
                check(path.starts_with("/Game/") && !astral_fallback_texture(path),
                      "Required source texture is missing or uses a companion placeholder");
            }
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

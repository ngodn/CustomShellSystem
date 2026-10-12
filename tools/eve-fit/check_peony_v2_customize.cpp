#include "controls.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace css;

void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}

int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected the V2 customize recipe");
        std::ifstream input(argv[1]);
        const auto recipe=nlohmann::json::parse(input);
        const auto model=ControlSet::parse(recipe);
        require(model.controls.size()==78 && model.palettes.size()==4,"V2 control or palette count changed");
        // Slot identities independently read from the cooked 46-slot character.
        const std::map<std::string,std::set<int>> pieces{
            {"toggle_jacket",{26,33,34}}, {"toggle_corset",{28,32,35,36,37}},
            {"toggle_choker",{27}}, {"toggle_gloves",{20,30}},
            {"toggle_underwear",{24}}, {"toggle_leg_panels",{22}},
            {"toggle_hip_bands",{23}}, {"toggle_hip_straps",{25}},
            {"toggle_boots",{21,29,31}}, {"toggle_stockings",{41,42}},
            {"toggle_stockings_body",{43}}, {"toggle_stockings_arms",{44}},
            {"toggle_stockings_hands",{45}},
        };
        require(hidden_control_sections(model,control_values(model,{}))==std::set<int>{38,39,40,42,43,44,45},
                "Original must hide covered skin and the optional sheer layers");
        for(const auto& [id,expected]:pieces) {
            const auto* control=model.find(id); require(control,"Missing garment control");
            require(std::set<int>(control->sections.begin(),control->sections.end())==expected,"Incorrect garment ownership");
        }
        unsigned combinations=0;
        for(unsigned mask=0;mask<(1u<<pieces.size());++mask) {
            Customization user;
            std::set<int> expected;
            unsigned bit=0;
            for(const auto& [id,sections]:pieces) {
                const bool shown=(mask&(1u<<bit++))!=0;
                user.values[id]={shown?1.f:0.f,0,0,1};
                if(!shown) expected.insert(sections.begin(),sections.end());
                if(id=="toggle_boots" && shown) expected.insert({38,39,40,42});
            }
            require(hidden_control_sections(model,control_values(model,user))==expected,"Incorrect visibility combination");
            require(Customization::parse(user.json())==user,"Visibility did not persist");
            for(const auto& palette:model.palettes) {
                const auto styled=choose_palette(model,user,palette.id);
                require(hidden_control_sections(model,control_values(model,styled))==expected,"Palette changes garment visibility");
            }
            ++combinations;
        }
        Customization user;
        user.values["shape_PBMBreastsSize"]={.63f,0,0,1};
        user.values["shape_PBMGlutesSize"]={.42f,0,0,1};
        user.values["stocking_opacity"]={.44f,0,0,1};
        user.values["trim_glow"]={3.2f,0,0,1};
        user.values["flow_speed"]={.7f,0,0,1};
        user.values["glow_coverage"]={.2f,0,0,1};
        user.values["hardware_glow"]={1.5f,0,0,1};
        user.ground_offset_cm=1.25;
        user.expression="smile";
        for(const auto& palette:model.palettes) {
            const auto styled=choose_palette(model,user,palette.id);
            const auto values=control_values(model,styled);
            for(const auto& [id,value]:user.values)
                require(values.at(id)==value,"Palette resets shape, opacity or flowing glow settings");
            require(styled.ground_offset_cm==user.ground_offset_cm && styled.expression==user.expression,"Palette resets placement or expression");
            require(restore_customization(model,styled.json())==styled,"Profile restore changes the customization");
            for(const auto& control:model.controls) if(control.kind==ControlKind::Choice) {
                for(size_t i=0;i<control.options.size();++i) {
                    auto selected=apply_customize(model,styled,{{"action","control"},{"control",control.id},{"channel",0},{"value",i}});
                    require(control_values(model,selected).at(control.id)[0]==float(i),"Texture selection failed");
                    require(restore_customization(model,selected.json())==selected,"Texture choice did not persist");
                    selected=apply_customize(model,selected,{{"action","reset_control"},{"control",control.id}});
                    require(control_values(model,selected).at(control.id)==palette.values.at(control.id),"Reset does not restore the palette texture");
                }
            }
        }
        require(control_values(model,choose_palette(model,user,"original")).empty(),"Original retains an override");
        std::cout<<"{\"passed\":true,\"controls\":"<<model.controls.size()
                 <<",\"visibility_combinations\":"<<combinations<<",\"palette_visibility_checks\":"<<combinations*4<<"}\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}

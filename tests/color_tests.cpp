#include "data.hpp"
#include <iostream>
#include <stdexcept>
using namespace css;
unsigned checks;
void expect(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
template<class F> void rejects(F action) { bool rejected=false; try { action(); } catch(const std::exception&) { rejected=true; } expect(rejected,"Invalid colors accepted"); }
int main() {
    try {
        auto source=Json::parse(R"({"schema":1,"controls":[
          {"id":"cloth","name":"Clothing","default":[1,1,1,1]},
          {"id":"glow","name":"Eye glow","type":"scalar","default":[1.5,0,0,1],"max":5,"bindings":[{"slot":5,"parameter":"Intensity"},{"slot":10,"parameter":"Intensity"}]}],
          "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0,1],"layers":{"cloth":"dye-cloth.png"}}],
          "palettes":[{"id":"red","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
        auto options=ColorOptions::parse(source);
        Outfit outfit;outfit.colors=options;
        Variant variant;variant.id="different";
        auto variant_source=source;
        variant_source["surfaces"][0]["layers"]["cloth"]="dye-different.png";
        variant.colors=ColorOptions::parse(variant_source);outfit.variants.push_back(variant);
        expect(outfit.colors_for("default").surfaces[0].layers.at("cloth")=="dye-cloth.png","Legacy outfit colors changed");
        expect(outfit.colors_for("different").surfaces[0].layers.at("cloth")=="dye-different.png","Variant dye texture was not selected");
        expect(color_values(options,{}).empty(),"Original must leave authored materials untouched");
        Customization custom;custom.palette="red";
        expect(color_values(options,custom).at("cloth")[0]==.6f,"Palette missing");
        custom.values["cloth"]={.1f,.2f,.3f,1}; custom.values["glow"]={3,0,0,1};
        auto colors=color_values(options,custom);
        expect(colors.at("cloth")[0]==.1f && colors.at("glow")[0]==3,"Independent custom overrides lost");
        custom.values.erase("cloth");
        expect(color_values(options,custom).at("cloth")[0]==.6f,"Reset part did not return to palette");
        expect(Customization::parse(custom.json())==custom,"Custom colors round trip failed");
        State state;state.selections["CharacterId.Player.Shell.Genessa"]={"test","default",custom};
        state.remembered_colors["test"]=custom;state.presets["look.1"]=state.selections;
        expect(State::parse(state.json()).json()==state.json(),"Saved look or remembered colors lost");
        auto old=state.json();old.erase("remembered_colors");for(auto& s:old["selections"])s.erase("colors");
        expect(State::parse(old).selections.begin()->second.colors.values.empty(),"Legacy selection must use original colors");
        custom.values["glow"][0]=6;rejects([&]{color_values(options,custom);});custom.values.erase("glow");
        custom.values["missing"]={1,1,1,1};rejects([&]{color_values(options,custom);});custom.values.clear();
        Customization previous;previous.palette="removed";previous.values["missing"]={1,1,1,1};previous.values["cloth"]={.2f,.3f,.4f,1};
        auto carried=compatible_colors(options,previous);
        expect(carried.palette=="original" && carried.values.size()==1 && carried.values.contains("cloth"),"Variant switch did not preserve compatible custom colors");
        custom.palette="missing";rejects([&]{color_values(options,custom);});
        auto bad=source;bad["palettes"][0]["values"]["cloth"][3]=.5;rejects([&]{ColorOptions::parse(bad);});
        bad=source;bad["controls"][1]["bindings"][0]["slot"]=128;rejects([&]{ColorOptions::parse(bad);});
        bad=source;bad["controls"][1]["bindings"][0]["association"]="layer";rejects([&]{ColorOptions::parse(bad);});
        bad["controls"][1]["bindings"][0]["layer"]=2;
        expect(ColorOptions::parse(bad).controls[1].bindings[0].association==0,"Layer association lost");
        bad=source;bad["surfaces"][0]["layers"]["cloth"]="../dye-cloth.png";rejects([&]{ColorOptions::parse(bad);});
        bad=source;bad["surfaces"].push_back(bad["surfaces"][0]);bad["surfaces"][1]["id"]="other";rejects([&]{ColorOptions::parse(bad);});
        bad=source;bad["surfaces"]=Json::object();rejects([&]{ColorOptions::parse(bad);});
        bad=source;bad["controls"][0]["step"]=0;rejects([&]{ColorOptions::parse(bad);});
        bad=source;bad["controls"][0]["default"][3]=2;rejects([&]{ColorOptions::parse(bad);});
        expect(std::abs(srgb_linear(.5f)-.214041f)<.000001,"sRGB colors must be converted once");
        std::cout<<checks<<" color behavior checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

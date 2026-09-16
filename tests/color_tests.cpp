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
        state.remembered_colors["test"]=custom;state.presets["look.1"]=Preset{state.selections};
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

        {   // 0.4 convention: group, role and hue locking, declared or read off the id.
            auto convention=Json::parse(R"({"schema":1,"controls":[
              {"id":"cloth","name":"Clothing","default":[1,1,1,1]},
              {"id":"metal","name":"Ornaments","default":[1,1,1,1]},
              {"id":"skin","name":"Skin","default":[1,1,1,1]},
              {"id":"eye-glow","name":"Eye glow","kind":"intensity","default":[1,0,0,1],"max":5,
               "bindings":[{"slot":5,"parameter":"Intensity"}]},
              {"id":"wraps","name":"Wraps","group":"body","role":"accent","default":[1,1,1,1]}],
              "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0],
               "layers":{"cloth":"dye-cloth.png","metal":"dye-metal.png","skin":"dye-skin.png","wraps":"dye-wraps.png"}}],
              "palettes":[{"id":"teal","name":"Teal","values":{
               "cloth":[0.6,0.1,0.2,1],"metal":[0.7,0.55,0.2,1],"skin":[0.8,0.7,0.62,1],"wraps":[0.3,0.3,0.3,1]}}]})");
            auto with_roles=ColorOptions::parse(convention);
            // Inferred from the id, because these declare neither group nor role.
            expect(with_roles.find("cloth")->role=="garment" && !with_roles.find("cloth")->hue_locked,
                   "Clothing should infer an unlocked garment");
            expect(with_roles.find("metal")->hue_locked && with_roles.find("skin")->hue_locked,
                   "Metal and skin should infer as hue locked");
            expect(with_roles.find("skin")->group==ColorGroup::Body &&
                   with_roles.find("cloth")->group==ColorGroup::Outfit, "Inferred groups are wrong");
            // "eye-glow" must win over both "eye" and "glow", and kind maps to a scalar.
            expect(with_roles.find("eye-glow")->role=="eye-glow" && with_roles.find("eye-glow")->scalar,
                   "eye-glow should be a body intensity, not a garment glow");
            // Bare-variant parts: pigments of the body, so hue locked with skin, while
            // pubic hair follows the head hair and stays free.
            auto bare=Json::parse(R"({"schema":1,"controls":[
              {"id":"areola","name":"Areolae","default":[1,1,1,1]},
              {"id":"nipples","name":"Nipples","default":[1,1,1,1]},
              {"id":"labia","name":"Labia","default":[1,1,1,1]},
              {"id":"pubic-hair","name":"Pubic hair","default":[1,1,1,1]}],
              "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0],
               "layers":{"areola":"dye-areola.png","nipples":"dye-nipples.png","labia":"dye-labia.png","pubic-hair":"dye-pubic.png"}}]})");
            auto nude=ColorOptions::parse(bare);
            for(const char* id:{"areola","nipples","labia"}) {
                expect(nude.find(id)->group==ColorGroup::Body,"A bare body part belongs to the body group");
                expect(nude.find(id)->hue_locked,"A body pigment must be hue locked with skin");
            }
            expect(nude.find("nipples")->role=="nipple","Nipples should infer the nipple role");
            expect(nude.find("pubic-hair")->role=="body-hair" && !nude.find("pubic-hair")->hue_locked,
                   "Pubic hair follows the hair and is not locked");

            // A declared group beats what the id would have suggested.
            expect(with_roles.find("wraps")->group==ColorGroup::Body && with_roles.find("wraps")->role=="accent",
                   "A declared group and role must win over the id");

            Customization tinted; tinted.palette="teal";
            tinted.tints["outfit"]={120.f,1.f,1.f};
            auto shifted=color_values(with_roles,tinted);
            // The garment rotates a full third of the wheel; the hue-locked ornaments do not.
            const auto& cloth=shifted.at("cloth");
            expect(cloth[1]>cloth[0] && cloth[1]>cloth[2],"A 120 degree tint should turn the red garment green");
            expect(shifted.at("metal")==with_roles.palettes[0].values.at("metal"),
                   "A hue locked part must ignore the group hue");
            expect(shifted.at("skin")==with_roles.palettes[0].values.at("skin"),
                   "An outfit tint must not touch the body");
            // Saturation and brightness reach a hue-locked part.
            Customization dimmed; dimmed.palette="teal"; dimmed.tints["outfit"]={0.f,1.f,.5f};
            expect(std::abs(color_values(with_roles,dimmed).at("metal")[0]-.35f)<.001f,
                   "A hue locked part should still take the group brightness");
            // The tint sits on top of an override, not instead of it.
            Customization both; both.palette="teal"; both.values["cloth"]={0.f,0.f,.5f,1};
            both.tints["outfit"]={0.f,1.f,.5f};
            expect(std::abs(color_values(with_roles,both).at("cloth")[2]-.25f)<.001f,
                   "The group tint must apply on top of a custom color");
            // Original dyes nothing, so a tint has nothing to move.
            Customization untouched; untouched.tints["outfit"]={120.f,1.f,1.f};
            expect(color_values(with_roles,untouched).empty(),"A tint must not dye anything on Original");
            expect(Customization::parse(both.json())==both,"Tints did not round trip");
            auto rejected=both.json(); rejected["tints"]["outfit"]["hue"]=400;
            rejects([&]{Customization::parse(rejected);});
            rejected=both.json(); rejected["tints"]["hat"]={{"hue",0}};
            rejects([&]{Customization::parse(rejected);});

            // Choosing a palette is a look, not a reset: it takes over the parts it
            // sets and the tint of their groups, and leaves the body alone.
            auto outfit_only=convention;                 // a palette for the dress alone
            outfit_only["palettes"][0]["values"]=Json{{"cloth",{.6,.1,.2,1}},{"metal",{.7,.55,.2,1}}};
            auto dressy=ColorOptions::parse(outfit_only);
            Customization before; before.palette="original";
            before.values["cloth"]={1.f,0.f,0.f,1};      // the palette sets this one
            before.values["skin"]={.5f,.5f,.5f,1};       // the palette does not
            before.tints["outfit"]={30.f,1.f,1.f};
            before.tints["body"]={0.f,1.f,.8f};
            auto after=choose_palette(dressy,before,"teal");
            expect(after.palette=="teal","The chosen palette must be recorded");
            expect(!after.values.contains("cloth"),"A palette must take over the parts it sets");
            expect(after.values.at("skin")==before.values.at("skin"),
                   "A palette must leave a custom colour it does not set alone");
            expect(!after.tints.contains("outfit"),"A palette must reset the tint of a group it touches");
            expect(after.tints.at("body")==before.tints.at("body"),
                   "A palette that only sets the outfit must leave the body tint alone");
            // Original means no dye at all, so nothing survives it.
            auto stripped=choose_palette(dressy,after,"original");
            expect(stripped.palette=="original" && stripped.values.empty() && stripped.tints.empty(),
                   "Original must clear every override and tint");
            rejects([&]{choose_palette(dressy,before,"not-installed");});
        }
        std::cout<<checks<<" color behavior checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

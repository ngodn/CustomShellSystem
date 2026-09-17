#include "data.hpp"
#include <cmath>
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
        auto options=ControlSet::parse(source);
        Outfit outfit;outfit.controls=options;
        Variant variant;variant.id="different";
        auto variant_source=source;
        variant_source["surfaces"][0]["layers"]["cloth"]="dye-different.png";
        variant.controls=ControlSet::parse(variant_source);outfit.variants.push_back(variant);
        expect(outfit.controls_for("default").surfaces[0].layers.at("cloth")=="dye-cloth.png","Legacy outfit colors changed");
        expect(outfit.controls_for("different").surfaces[0].layers.at("cloth")=="dye-different.png","Variant dye texture was not selected");
        expect(control_values(options,{}).empty(),"Original must leave authored materials untouched");
        Customization custom;custom.palette="red";
        expect(control_values(options,custom).at("cloth")[0]==.6f,"Palette missing");
        custom.values["cloth"]={.1f,.2f,.3f,1}; custom.values["glow"]={3,0,0,1};
        auto colors=control_values(options,custom);
        expect(colors.at("cloth")[0]==.1f && colors.at("glow")[0]==3,"Independent custom overrides lost");
        custom.values.erase("cloth");
        expect(control_values(options,custom).at("cloth")[0]==.6f,"Reset part did not return to palette");
        expect(Customization::parse(custom.json())==custom,"Custom colors round trip failed");
        State state;state.selections["CharacterId.Player.Shell.Genessa"]={"test","default",custom};
        state.remembered_custom["test"]=custom;state.presets["look.1"]=Preset{state.selections};
        expect(State::parse(state.json()).json()==state.json(),"Saved look or remembered colors lost");
        auto bare=state.json();bare.erase("remembered_custom");for(auto& s:bare["selections"])s.erase("customize");
        expect(State::parse(bare).selections.begin()->second.custom.values.empty(),"A selection with nothing customized must use the author's own");
        // 1.0 renamed the block from "colors" to "customize". A state file written by 0.4
        // still loads, with everything the player had chosen.
        auto legacy=state.json();
        legacy["remembered_colors"]=legacy["remembered_custom"];legacy.erase("remembered_custom");
        for(auto& s:legacy["selections"]) { s["colors"]=s["customize"];s.erase("customize"); }
        auto reopened=State::parse(legacy);
        expect(reopened.selections.begin()->second.custom==custom,"A 0.4 selection lost what the player chose");
        expect(reopened.remembered_custom.at("test")==custom,"A 0.4 remembered outfit lost what the player chose");
        // Naming both is a mistake, not a merge.
        auto muddled=state.json();muddled["selections"].begin().value()["colors"]=Json::object();
        rejects([&]{State::parse(muddled);});
        custom.values["glow"][0]=6;rejects([&]{control_values(options,custom);});custom.values.erase("glow");
        custom.values["missing"]={1,1,1,1};rejects([&]{control_values(options,custom);});custom.values.clear();
        Customization previous;previous.palette="removed";previous.values["missing"]={1,1,1,1};previous.values["cloth"]={.2f,.3f,.4f,1};
        auto carried=compatible_values(options,previous);
        expect(carried.palette=="original" && carried.values.size()==1 && carried.values.contains("cloth"),"Variant switch did not preserve compatible custom colors");
        custom.palette="missing";rejects([&]{control_values(options,custom);});
        auto bad=source;bad["palettes"][0]["values"]["cloth"][3]=.5;rejects([&]{ControlSet::parse(bad);});
        bad=source;bad["controls"][1]["bindings"][0]["slot"]=128;rejects([&]{ControlSet::parse(bad);});
        bad=source;bad["controls"][1]["bindings"][0]["association"]="layer";rejects([&]{ControlSet::parse(bad);});
        bad["controls"][1]["bindings"][0]["layer"]=2;
        expect(ControlSet::parse(bad).controls[1].bindings[0].association==0,"Layer association lost");
        bad=source;bad["surfaces"][0]["layers"]["cloth"]="../dye-cloth.png";rejects([&]{ControlSet::parse(bad);});
        bad=source;bad["surfaces"].push_back(bad["surfaces"][0]);bad["surfaces"][1]["id"]="other";rejects([&]{ControlSet::parse(bad);});
        bad=source;bad["surfaces"]=Json::object();rejects([&]{ControlSet::parse(bad);});
        bad=source;bad["controls"][0]["step"]=0;rejects([&]{ControlSet::parse(bad);});
        bad=source;bad["controls"][0]["default"][3]=2;rejects([&]{ControlSet::parse(bad);});
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
            auto with_roles=ControlSet::parse(convention);
            // Inferred from the id, because these declare neither group nor role.
            expect(with_roles.find("cloth")->role=="garment" && !with_roles.find("cloth")->hue_locked,
                   "Clothing should infer an unlocked garment");
            expect(with_roles.find("metal")->hue_locked && with_roles.find("skin")->hue_locked,
                   "Metal and skin should infer as hue locked");
            expect(with_roles.find("skin")->group==ControlGroup::Body &&
                   with_roles.find("cloth")->group==ControlGroup::Outfit, "Inferred groups are wrong");
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
            auto nude=ControlSet::parse(bare);
            for(const char* id:{"areola","nipples","labia"}) {
                expect(nude.find(id)->group==ControlGroup::Body,"A bare body part belongs to the body group");
                expect(nude.find(id)->hue_locked,"A body pigment must be hue locked with skin");
            }
            expect(nude.find("nipples")->role=="nipple","Nipples should infer the nipple role");
            expect(nude.find("pubic-hair")->role=="body-hair" && !nude.find("pubic-hair")->hue_locked,
                   "Pubic hair follows the hair and is not locked");

            // A declared group beats what the id would have suggested.
            expect(with_roles.find("wraps")->group==ControlGroup::Body && with_roles.find("wraps")->role=="accent",
                   "A declared group and role must win over the id");

            Customization tinted; tinted.palette="teal";
            tinted.tints["outfit"]={120.f,1.f,1.f};
            auto shifted=control_values(with_roles,tinted);
            // The garment rotates a full third of the wheel; the hue-locked ornaments do not.
            const auto& cloth=shifted.at("cloth");
            expect(cloth[1]>cloth[0] && cloth[1]>cloth[2],"A 120 degree tint should turn the red garment green");
            expect(shifted.at("metal")==with_roles.palettes[0].values.at("metal"),
                   "A hue locked part must ignore the group hue");
            expect(shifted.at("skin")==with_roles.palettes[0].values.at("skin"),
                   "An outfit tint must not touch the body");
            // Saturation and brightness reach a hue-locked part.
            Customization dimmed; dimmed.palette="teal"; dimmed.tints["outfit"]={0.f,1.f,.5f};
            expect(std::abs(control_values(with_roles,dimmed).at("metal")[0]-.35f)<.001f,
                   "A hue locked part should still take the group brightness");
            // The tint sits on top of an override, not instead of it.
            Customization both; both.palette="teal"; both.values["cloth"]={0.f,0.f,.5f,1};
            both.tints["outfit"]={0.f,1.f,.5f};
            expect(std::abs(control_values(with_roles,both).at("cloth")[2]-.25f)<.001f,
                   "The group tint must apply on top of a custom color");
            // Original dyes nothing, so a tint has nothing to move.
            Customization untouched; untouched.tints["outfit"]={120.f,1.f,1.f};
            expect(control_values(with_roles,untouched).empty(),"A tint must not dye anything on Original");
            expect(Customization::parse(both.json())==both,"Tints did not round trip");
            auto rejected=both.json(); rejected["tints"]["outfit"]["hue"]=400;
            rejects([&]{Customization::parse(rejected);});
            rejected=both.json(); rejected["tints"]["hat"]={{"hue",0}};
            rejects([&]{Customization::parse(rejected);});

            // Choosing a palette is a look, not a reset: it takes over the parts it
            // sets and the tint of their groups, and leaves the body alone.
            auto outfit_only=convention;                 // a palette for the dress alone
            outfit_only["palettes"][0]["values"]=Json{{"cloth",{.6,.1,.2,1}},{"metal",{.7,.55,.2,1}}};
            auto dressy=ControlSet::parse(outfit_only);
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
        {
            // 1.0: a control declares what it is. Colour is one kind, and everything
            // else is a single number the menu draws as a slider or a switch.
            auto kinds=Json::parse(R"({"schema":1,"controls":[
              {"id":"cloth","name":"Garment","role":"garment","default":[1,1,1,1]},
              {"id":"glow","name":"Eye glow","type":"scalar","default":[1.5,0,0,1],"max":5,
               "bindings":[{"slot":5,"parameter":"Intensity"}]},
              {"id":"gloss","name":"Sheen","kind":"scalar","role":"gloss","default":[0.4,0,0,1],
               "bindings":[{"slot":0,"parameter":"Roughness"}]},
              {"id":"hood","name":"Hood","kind":"toggle","role":"piece","default":[1,0,0,1],"sections":[2,3]}],
              "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0],"layers":{"cloth":"dye-cloth.png"}}],
              "palettes":[{"id":"red","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
            auto model=ControlSet::parse(kinds);
            expect(model.find("cloth")->kind==ControlKind::Color,"A control with no kind is a colour");
            // type=scalar predates the split, and those packages meant a strength.
            expect(model.find("glow")->kind==ControlKind::Intensity,"type=scalar must stay an intensity");
            expect(model.find("gloss")->kind==ControlKind::Scalar,"An ordinary material scalar was not kept");
            expect(model.find("hood")->kind==ControlKind::Toggle,"A toggle was not kept");
            expect(!model.find("cloth")->scalar && model.find("gloss")->scalar,
                   "Every kind but a colour is edited as one number");
            const auto* hood=model.find("hood");
            expect(hood->sections.size()==2 && hood->minimum==0 && hood->maximum==1 && hood->step==1,
                   "A toggle is on or off across the sections it names");
            expect(std::string(control_kind_name(ControlKind::Toggle))=="toggle","Kind name missing");
            // A toggle drives its sections directly and needs them; nothing else may claim them.
            auto drop=kinds; drop["controls"][3].erase("sections");
            rejects([&]{ControlSet::parse(drop);});
            auto stray=kinds; stray["controls"][2]["sections"]=Json::array({1});
            rejects([&]{ControlSet::parse(stray);});
            auto odd=kinds; odd["controls"][2]["kind"]="texture";
            rejects([&]{ControlSet::parse(odd);});

            // A choice picks one of the textures the package ships, by index.
            auto pick=kinds;
            pick["controls"].push_back(Json::parse(R"({"id":"pattern","name":"Pattern","kind":"choice",
              "role":"pattern","default":[1,0,0,1],
              "options":[{"name":"Plain","texture":"/Game/CSS/x/T_Plain.T_Plain"},
                         {"name":"Lace","texture":"/Game/CSS/x/T_Lace.T_Lace"}],
              "bindings":[{"slot":0,"parameter":"BaseColorMap  non VT"}]})"));
            auto chosen=ControlSet::parse(pick);
            const auto* pattern=chosen.find("pattern");
            expect(pattern->kind==ControlKind::Choice && pattern->options.size()==2,"A choice kept its options");
            expect(pattern->options[1].name=="Lace","Choice option names lost");
            expect(pattern->minimum==0 && pattern->maximum==1 && pattern->step==1,
                   "A choice ranges over its options and nothing else");
            auto over=pick; over["controls"][4]["default"]=Json::array({2,0,0,1});
            rejects([&]{ControlSet::parse(over);});
            auto bare=pick; bare["controls"][4].erase("options");
            rejects([&]{ControlSet::parse(bare);});
            auto bad_path=pick; bad_path["controls"][4]["options"][0]["texture"]="/Game/CSS/x/T_Plain";
            rejects([&]{ControlSet::parse(bad_path);});
            auto stray_options=pick; stray_options["controls"][2]["options"]=Json::array();
            rejects([&]{ControlSet::parse(stray_options);});
        }
        {
            // 1.0: a spring tunes the mesh's own secondary motion. It is the only control
            // with two numbers in it, and the only one that writes no material at all.
            auto springs=Json::parse(R"({"schema":1,"controls":[
              {"id":"cloth","name":"Garment","role":"garment","group":"outfit","default":[1,1,1,1]},
              {"id":"bust","name":"Bust","kind":"spring","group":"body","role":"figure",
               "nodes":["brust001","brust002"],
               "frequency":{"min":1.2,"max":2.6,"default":1.6},
               "damping_ratio":{"min":0.4,"max":0.95,"default":0.65}}],
              "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0],"layers":{"cloth":"dye-cloth.png"}}],
              "palettes":[{"id":"red","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
            auto model=ControlSet::parse(springs);
            const auto* bust=model.find("bust");
            expect(bust->kind==ControlKind::Spring && bust->scalar,"A spring was not kept");
            expect(bust->nodes.size()==2 && bust->nodes[0]=="brust001","Spring bones lost");
            expect(bust->minimum==1.2f && bust->maximum==2.6f,"Spring frequency range lost");
            expect(bust->damping_minimum==.4f && bust->damping_maximum==.95f,"Spring damping range lost");
            expect(bust->value[0]==1.6f && bust->value[1]==.65f && bust->value[3]==1,
                   "A spring takes its default from its two ranges");
            expect(std::string(control_kind_name(ControlKind::Spring))=="spring","Kind name missing");
            // The conversion is the whole point of the control: 1.5915 Hz is stiffness 100,
            // and a damping ratio of 0.65 against it is damping 13, which is what the
            // Seductress V2 blueprint actually ships.
            const auto tuning=spring_tuning(1.5915494309189535f,.65f);
            expect(std::abs(tuning.stiffness-100)<.01 && std::abs(tuning.damping-13)<.01,
                   "Frequency and damping ratio did not convert to the engine's numbers");
            // A saved value is checked against the range its own channel belongs to.
            Customization custom; custom.values["bust"]={2.0f,.5f,0,1};
            expect(control_values(model,custom).at("bust")[1]==.5f,"Spring override lost");
            custom.values["bust"]={2.0f,1.4f,0,1};
            rejects([&]{control_values(model,custom);});
            custom.values["bust"]={4.0f,.5f,0,1};
            rejects([&]{control_values(model,custom);});
            // A body tint moves colours, not motion.
            Customization tinted; tinted.palette="red"; tinted.tints["body"]={40,1,1};
            tinted.values["bust"]={2.0f,.5f,0,1};
            expect(control_values(model,tinted).at("bust")[0]==2.f,"A group tint must not touch a spring");
            auto no_bones=springs; no_bones["controls"][1].erase("nodes");
            rejects([&]{ControlSet::parse(no_bones);});
            auto twice=springs; twice["controls"][1]["nodes"]=Json::array({"brust001","brust001"});
            rejects([&]{ControlSet::parse(twice);});
            auto hyphen=springs; hyphen["controls"][1]["nodes"]=Json::array({"brust-001"});
            rejects([&]{ControlSet::parse(hyphen);});
            auto both=springs; both["controls"][1]["default"]=Json::array({1.6,0.65,0,1});
            rejects([&]{ControlSet::parse(both);});
            auto limits=springs; limits["controls"][1]["max"]=4;
            rejects([&]{ControlSet::parse(limits);});
            auto stray_nodes=springs; stray_nodes["controls"][0]["nodes"]=Json::array({"belly"});
            rejects([&]{ControlSet::parse(stray_nodes);});
            auto upside_down=springs; upside_down["controls"][1]["frequency"]["min"]=3;
            rejects([&]{ControlSet::parse(upside_down);});
            auto outside=springs; outside["controls"][1]["frequency"]["default"]=2.9;
            rejects([&]{ControlSet::parse(outside);});
            // Past the engine's damping cutoff the slider would stop meaning what it says.
            auto violent=springs;
            violent["controls"][1]["frequency"]["max"]=8;
            violent["controls"][1]["damping_ratio"]["max"]=2;
            rejects([&]{ControlSet::parse(violent);});

            // 1.0.0 physics engine: a spring with a travel clamp gains a third channel
            // (MaxDisplacement, cm) and may force its axis filters. This is what turns
            // MSII's own spring node into the Better Jiggle behaviour: a lively low damping
            // held on the body by the clamp rather than allowed to fly off.
            auto clamped=springs;
            clamped["controls"][1]["max_displacement"]=Json{{"min",0.2},{"max",4.0},{"default",1.5}};
            clamped["controls"][1]["translate"]=Json::array({true,false,true});
            clamped["controls"][1]["rotate"]=Json::array({true,true,true});
            clamped["controls"][1]["error_reset"]=255.0;
            auto cmodel=ControlSet::parse(clamped);
            const auto* cbust=cmodel.find("bust");
            expect(cbust->spring_clamp,"A travel clamp was not kept");
            expect(cbust->displacement_minimum==.2f && cbust->displacement_maximum==4.f,"Spring travel range lost");
            expect(cbust->value[2]==1.5f,"Spring travel default lost");
            expect(cbust->translate[0]==1 && cbust->translate[1]==0 && cbust->translate[2]==1,"Spring translate flags lost");
            expect(cbust->rotate[0]==1 && cbust->rotate[1]==1 && cbust->rotate[2]==1,"Spring rotate flags lost");
            expect(std::abs(cbust->error_reset-255.0)<1e-9,"Spring reset threshold lost");
            // A plain spring leaves all of that untouched, so recipes from 2.0.3 still load.
            expect(!bust->spring_clamp && bust->translate[0]==-1 && bust->rotate[2]==-1 && bust->error_reset<0,
                   "A plain spring must not gain a clamp or force flags");
            // The third channel is validated against the travel range.
            Customization travel; travel.values["bust"]={1.6f,.65f,3.0f,1};
            expect(control_values(cmodel,travel).at("bust")[2]==3.f,"Spring travel override lost");
            travel.values["bust"]={1.6f,.65f,9.0f,1};
            rejects([&]{control_values(cmodel,travel);});
            auto bad_axis=clamped; bad_axis["controls"][1]["translate"]=Json::array({true,false});
            rejects([&]{ControlSet::parse(bad_axis);});
            auto bad_reset=clamped; bad_reset["controls"][1]["error_reset"]=0;
            rejects([&]{ControlSet::parse(bad_reset);});

            // 1.0: a shape drives a morph target the package cooked into its own mesh. It
            // is a single number like a scalar, and it writes no material.
            auto shapes=Json::parse(R"({"schema":1,"controls":[
              {"id":"cloth","name":"Garment","role":"garment","group":"outfit","default":[1,1,1,1]},
              {"id":"hips","name":"Hips","kind":"shape","group":"body","role":"figure",
               "morph":"Hips","min":0,"max":1,"step":0.05,"default":[0,0,0,1]}],
              "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0],"layers":{"cloth":"dye-cloth.png"}}],
              "palettes":[{"id":"red","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
            auto shaped=ControlSet::parse(shapes);
            const auto* hips=shaped.find("hips");
            expect(hips->kind==ControlKind::Shape && hips->scalar,"A shape was not kept");
            expect(hips->morph=="Hips","Shape morph target lost");
            expect(hips->bindings.empty(),"A shape needs no material binding");
            expect(std::string(control_kind_name(ControlKind::Shape))=="shape","Kind name missing");
            auto nameless=shapes; nameless["controls"][1].erase("morph");
            rejects([&]{ControlSet::parse(nameless);});
            // The importer refuses any name the engine would rename, so the recipe does too.
            for(const char* bad:{"","Hips and thighs","Hips-2"}) {
                auto odd=shapes; odd["controls"][1]["morph"]=bad;
                rejects([&]{ControlSet::parse(odd);});
            }
            auto stray_morph=shapes; stray_morph["controls"][0]["morph"]="Hips";
            rejects([&]{ControlSet::parse(stray_morph);});
            // A saved weight is checked against the author's range like any other slider.
            Customization shape_custom; shape_custom.values["hips"]={0.5f,0,0,1};
            expect(control_values(shaped,shape_custom).at("hips")[0]==.5f,"Shape weight lost");
            shape_custom.values["hips"]={1.5f,0,0,1};
            rejects([&]{control_values(shaped,shape_custom);});
        }
        std::cout<<checks<<" color behavior checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

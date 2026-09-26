#include "data.hpp"
#include "physics_presets.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace css;
unsigned checks;
void expect(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
template<class F> void rejects(F action) { bool rejected=false; try { action(); } catch(const std::exception&) { rejected=true; } expect(rejected,"Invalid colors accepted"); }
int main() {
    try {
        {
            // Physics presets: the built-ins follow what the manifest declares (region bones,
            // the hair solver), not the words in a part's id; a package adds its own after them.
            auto recipe=Json::parse(R"({"schema":1,"controls":[
              {"id":"part-a","name":"Front","kind":"rig","solver":"angular_body","regions":["butt001","butt002"],
               "frequency":{"min":0.5,"max":6,"default":2},"damping_ratio":{"min":0.1,"max":2,"default":0.7},
               "motion_amount":{"min":0,"max":5,"default":1},
               "presets":[{"id":"studio","name":"Studio","description":"Tuned for this mesh","value":[3,0.9,0.5]}]},
              {"id":"strands","name":"Strands","kind":"rig",
               "stiffness":{"min":1,"max":400,"default":150},"damping":{"min":0,"max":60,"default":12},
               "gravity":{"min":-1,"max":1,"default":0}}]})");
            auto model=ControlSet::parse(recipe);
            const auto& body=model.controls[0]; const auto& hair=model.controls[1];
            expect(physics_region(body)==PhysicsRegion::glute,"Declared region bones not used");
            auto presets=physics_presets(body);
            expect(presets.size()==6 && presets.front().id=="firm" && presets.back().id=="studio" && !presets.back().builtin,
                   "Built-ins then package presets expected");
            expect(presets[1].channels[0]==2.10f && presets[1].channels[1]==.50f,"Glute values not chosen from the declared region");
            const ControlValue motion_off{2,.7f,1,0};
            const auto applied=physics_preset_value(body,presets.back(),motion_off);
            expect(applied[0]==3 && applied[1]==.9f && applied[2]==.5f && applied[3]==0,"Preset must keep the motion switch");
            expect(matching_physics_preset(body,applied)=="studio","Applied preset not recognised");
            auto nudged=applied; nudged[0]+=.1f;
            expect(matching_physics_preset(body,nudged).empty(),"A moved slider still matched a preset");
            expect(physics_presets(hair).size()==5 && physics_presets(hair)[1].id=="natural","Hair solver presets missing");
            expect(physics_presets(hair)[0].channels[0]==260 && physics_presets(hair)[4].channels[2]==0,"Hair preset values changed");
            expect(physics_preset_id("OMG_Earthquake")=="earthquake" && physics_preset_id("saggy")=="soft","Legacy preset ids not mapped");
            auto clash=recipe; clash["controls"][0]["presets"][0]["id"]="natural";
            rejects([&]{ControlSet::parse(clash);});
            auto outside=recipe; outside["controls"][0]["presets"][0]["value"]={9,0.9,0.5};
            rejects([&]{ControlSet::parse(outside);});
            auto colour=Json::parse(R"({"schema":1,"controls":[{"id":"tint","name":"Tint","kind":"color","bindings":[{"slot":0,"parameter":"Tint"}],
              "presets":[{"id":"x","name":"X","value":[1,1,1]}]}]})");
            rejects([&]{ControlSet::parse(colour);});
        }
        {
            // Ground height is saved with the look, bounded, carried to a compatible variant
            // and cleared by the author's original.
            Customization look; look.ground_offset_cm=-2.5;
            expect(Customization::parse(look.json()).ground_offset_cm==-2.5,"Ground height not saved");
            expect(!Customization::parse(Customization{}.json()).ground_offset_cm,"Unset ground height saved");
            rejects([&]{Customization::parse(Json::parse(R"({"palette":"original","values":{},"ground_offset_cm":12})"));});
            ControlSet none;
            expect(compatible_values(none,look).ground_offset_cm==-2.5,"Ground height lost between variants");
            expect(!choose_palette(none,look,"original").ground_offset_cm,"Original kept the ground height");
        }
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[{"id":"chest-motion","name":"Chest motion","kind":"rig",
              "solver":"angular_body","regions":["brust001","brust002"],
              "frequency":{"min":0.5,"max":6,"default":2},"damping_ratio":{"min":0.1,"max":2,"default":0.7},
              "motion_amount":{"min":0,"max":1,"default":1}}]})");
            auto model=ControlSet::parse(recipe);
            expect(body_rig_control(model.controls[0]),"Body solver identity missing");
            BodyRigSettings authored; authored.global_frequency=3;authored.global_damping=.8f;authored.global_motion=.6f;
            authored.frequency.fill(5);authored.damping.fill(1.2f);authored.motion.fill(.9f);authored.enabled.fill(false);
            expect(body_rig_settings(model.controls,{},authored)==authored,"Original body settings changed");
            Customization selected;selected.values["chest-motion"]={4,1.1f,.2f,0};
            auto values=control_values(model,selected);
            auto result=body_rig_settings(model.controls,values,authored);
            expect(result.use_regions && result.frequency[0]==4 && result.frequency[1]==4 && !result.enabled[0],"Paired body region mapping failed");
            expect(result.frequency[2]==3 && result.damping[2]==.8f && result.motion[2]==.6f && result.enabled[2],"Global body fallback was not preserved");
            authored.use_regions=true;
            result=body_rig_settings(model.controls,values,authored);
            expect(result.frequency[2]==5 && result.damping[2]==1.2f && !result.enabled[2],"Unselected authored regions changed");
            expect(Customization::parse(selected.json()).values==selected.values,"Body profile values changed");
            rejects([&]{rig_settings(model.controls[0],values.begin()->second);});
            auto duplicate=recipe;duplicate["controls"].push_back(duplicate["controls"][0]);duplicate["controls"][1]["id"]="duplicate";
            rejects([&]{ControlSet::parse(duplicate);});
            duplicate["controls"][1]["regions"]={"belly"};
            auto paired=ControlSet::parse(duplicate);
            expect(paired.controls.size()==2,"Disjoint body owners rejected");
            auto both=body_rig_settings(paired.controls,{{"chest-motion",{4,1.1f,.2f,0}},{"duplicate",{3,.8f,.4f,1}}},authored);
            expect(both.frequency[0]==4 && both.frequency[6]==3 && both.enabled[6],"Independent region composition failed");
            auto dropped=body_rig_settings(paired.controls,{{"duplicate",{3,.8f,.4f,1}}},authored);
            expect(dropped.frequency[0]==authored.frequency[0] && dropped.enabled[0]==authored.enabled[0] && dropped.frequency[6]==3,
                   "Removing one body override did not restore its authored region");
            auto overlap=recipe;
            overlap["controls"].push_back(Json::parse(R"({"id":"old-chest","name":"Old chest","kind":"spring","nodes":["brust001"],
                "frequency":{"min":1,"max":3,"default":2},"damping_ratio":{"min":0.1,"max":1,"default":0.7}})"));
            rejects([&]{ControlSet::parse(overlap);});
            for(const auto& regions:{Json::array(),Json::array({"brust001","brust001"}),Json::array({"head"})}) {
                auto bad=recipe;bad["controls"][0]["regions"]=regions;rejects([&]{ControlSet::parse(bad);});
            }
            for(const char* field:{"stiffness","damping","gravity","nodes"}) {
                auto bad=recipe;bad["controls"][0][field]=1;rejects([&]{ControlSet::parse(bad);});
            }
            auto bad=recipe;bad["controls"][0]["solver"]="unknown";rejects([&]{ControlSet::parse(bad);});
            for(auto value:{ControlValue{0,.7f,1,1},ControlValue{2,3,1,1},ControlValue{2,.7f,2,1},ControlValue{2,.7f,1,.5f}})
                rejects([&]{body_rig_settings(model.controls,{{"chest-motion",value}},authored);});
        }
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[{"id":"hair-motion","name":"Hair motion","kind":"rig",
              "stiffness":{"min":100,"max":250,"default":150},"damping":{"min":12,"max":24,"default":18},
              "gravity":{"min":-0.2,"max":0.2,"default":0},"enabled":true}]})");
            const auto model=ControlSet::parse(recipe);
            const auto& control=model.controls.at(0);
            expect(control.kind==ControlKind::Rig && control.scalar && control_channel_count(control)==4,"Rig channels missing");
            expect(control.value==ControlValue{150,18,0,1},"Rig defaults wrong");
            expect(control_channel(control,3).step==1 && control_channel(control,1).maximum==24,"Rig slider ranges wrong");
            expect(control_values(model,{}).empty(),"Original must keep authored rig values");
            Customization selected; selected.values[control.id]={220,20,-.1f,0};
            const auto applied=control_values(model,selected).at(control.id);
            const auto setting=rig_settings(control,applied);
            expect(setting.stiffness==220 && setting.damping==20 && !setting.enabled &&
                   std::abs(setting.gravity[2]-98)<.00001,"Rig mapping wrong");
            expect(Customization::parse(selected.json()).values==selected.values,"Rig profile round trip failed");
            for(auto bad:{ControlValue{99,18,0,1},ControlValue{150,25,0,1},ControlValue{150,18,1,1},
                         ControlValue{150,18,0,.5f},ControlValue{150,18,0,std::numeric_limits<float>::quiet_NaN()}})
                rejects([&]{rig_settings(control,bad);});
            for(const char* key:{"nodes","bindings","angular_spring","frequency","default"}) {
                auto bad=recipe; bad["controls"][0][key]=Json::array(); rejects([&]{ControlSet::parse(bad);});
            }
            auto duplicate=recipe; duplicate["controls"].push_back(duplicate["controls"][0]);
            duplicate["controls"][1]["id"]="other"; rejects([&]{ControlSet::parse(duplicate);});
            auto palette=recipe; palette["palettes"]=Json::parse(R"([{"id":"off","name":"Off","values":{"hair-motion":[150,18,0,0]}}])");
            auto with_palette=ControlSet::parse(palette);
            Customization choice;choice.palette="off";
            expect(control_values(with_palette,choice).at(control.id)[3]==0,"Rig off palette lost");
        }
        auto source=Json::parse(R"({"schema":1,"controls":[
          {"id":"cloth","name":"Clothing","default":[1,1,1,1]},
          {"id":"glow","name":"Eye glow","type":"scalar","default":[1.5,0,0,1],"max":5,"bindings":[{"slot":5,"parameter":"Intensity"},{"slot":10,"parameter":"Intensity"}]}],
          "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0,1],"layers":{"cloth":"dye-cloth.png"}}],
          "palettes":[{"id":"red","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
        auto options=ControlSet::parse(source);
        {
            auto authored=source;
            authored["controls"][0]["swatches"]=Json::parse(R"([
                {"name":"Default","color":[0.1,0.1,0.1,1],"reset":true},
                {"name":"Wine","color":[0.4,0.1,0.2,1]}])");
            auto checked=ControlSet::parse(authored);
            expect(checked.controls[0].swatches.size()==2 && checked.controls[0].swatches[0].reset,
                   "Authored Default swatch lost");
            expect(checked.controls[0].swatches[1].name=="Wine" && checked.controls[0].swatches[1].color[0]==.4f,
                   "Authored swatch color or order changed");
            expect(control_values(checked,{}).empty(),"Swatch previews must not dye the initial appearance");
            for(const auto* field:{"reset","name","color"}) {
                auto bad=authored;
                if(std::string(field)=="reset") bad["controls"][0]["swatches"][0][field]=false;
                else if(std::string(field)=="name") bad["controls"][0]["swatches"][1][field]="Default";
                else bad["controls"][0]["swatches"][1][field]={.1,.2,.3,.5};
                rejects([&]{ControlSet::parse(bad);});
            }
            auto bad=authored;bad["controls"][1]["swatches"]=authored["controls"][0]["swatches"];
            rejects([&]{ControlSet::parse(bad);});
        }
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
        auto invalid_tint=custom;
        invalid_tint.tints["outfit"].hue=std::numeric_limits<float>::quiet_NaN();
        rejects([&]{control_values(options,invalid_tint);});
        invalid_tint.tints.clear(); invalid_tint.tints["unknown"]={};
        rejects([&]{control_values(options,invalid_tint);});
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
        // Naming both is a mistake, not a merge. The damaged selection is dropped (that shell falls
        // back to its original look) instead of failing the whole state load.
        auto muddled=state.json();muddled["selections"].begin().value()["colors"]=Json::object();
        expect(State::parse(muddled).selections.empty(),"A selection naming both colors and customize was kept");
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
            for(const auto* id:{"hood","pattern"}) {
                Customization fractional;
                fractional.values[id]={.5f,0,0,1};
                rejects([&]{control_values(chosen,fractional);});
            }
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
            auto wardrobe=Json::parse(R"({"schema":1,"controls":[
              {"id":"boots","name":"Boots","kind":"toggle","default":[1,0,0,1],
               "sections":[20],"occludes_sections":[21,22]},
              {"id":"stockings","name":"Stockings","kind":"toggle","default":[1,0,0,1],
               "sections":[19,22]}]})");
            auto model=ControlSet::parse(wardrobe);
            Customization custom;
            auto hidden=[&]{return hidden_control_sections(model,control_values(model,custom));};
            expect(hidden()==std::set<int>({21,22}),"Original must apply authored footwear masks");
            custom.values["boots"]={0,0,0,1};
            expect(hidden()==std::set<int>({20}),"Removing boots must restore body and stocking feet");
            custom.values["stockings"]={0,0,0,1};
            expect(hidden()==std::set<int>({19,20,22}),"Bare feet must remain visible when stockings are off");
            custom.values["boots"]={1,0,0,1};
            expect(hidden()==std::set<int>({19,21,22}),"Boots must cover bare feet with stockings off");
            custom.values.clear();
            expect(hidden()==std::set<int>({21,22}),"Clearing customization must restore authored masks");
            auto shared=wardrobe;
            shared["controls"].push_back(Json::parse(R"({"id":"wrap","name":"Wrap","kind":"toggle",
              "default":[1,0,0,1],"sections":[23],"occludes_sections":[21]})"));
            model=ControlSet::parse(shared);
            custom.values["boots"]={0,0,0,1};
            expect(hidden()==std::set<int>({20,21}),"A second garment must retain its body mask");
            std::reverse(model.controls.begin(),model.controls.end());
            expect(hidden()==std::set<int>({20,21}),"Garment order must not change visibility");
            for(const auto* field:{"sections","occludes_sections"}) {
                for(const auto& invalid:{Json::array(),Json::array({-1}),Json::array({128}),
                        Json::array({1.5}),Json::array({true}),Json::array({4294967296ULL}),Json("21")}) {
                    auto bad=wardrobe; bad["controls"][0][field]=invalid;
                    rejects([&]{ControlSet::parse(bad);});
                }
            }
            for(const auto& invalid:{Json::array({21,21}),Json::array({20})}) {
                auto bad=wardrobe; bad["controls"][0]["occludes_sections"]=invalid;
                rejects([&]{ControlSet::parse(bad);});
            }
            auto stray=wardrobe; stray["controls"][0]["kind"]="scalar";
            stray["controls"][0].erase("sections");
            rejects([&]{ControlSet::parse(stray);});
        }
        {
            // Heeled fabric requires both toggles, while skin lining requires
            // only shoes. Exercise every combination and a return to Original.
            const auto wardrobe=ControlSet::parse(Json::parse(R"({"schema":1,"controls":[
              {"id":"stockings","name":"Stockings","kind":"toggle","default":[1,0,0,1],
               "sections":[19,27,28]},
              {"id":"shoes","name":"Shoes","kind":"toggle","default":[1,0,0,1],
               "sections":[20,25,26,28],"occludes_sections":[23,24,27]}]})"));
            Customization custom;
            auto hidden=[&]{return hidden_control_sections(wardrobe,control_values(wardrobe,custom));};
            expect(hidden()==std::set<int>({23,24,27}),"Original must show lining and heeled fabric");
            custom.values["stockings"]={0,0,0,1};
            expect(hidden()==std::set<int>({19,23,24,27,28}),"Shoes alone must retain skin lining");
            custom.values["shoes"]={0,0,0,1};
            expect(hidden()==std::set<int>({19,20,25,26,27,28}),"Bare feet must restore covered body sections");
            custom.values["stockings"]={1,0,0,1};
            expect(hidden()==std::set<int>({20,25,26,28}),"Stockings alone must show only flat fabric");
            custom.values.clear();
            expect(hidden()==std::set<int>({23,24,27}),"Original must restore all wardrobe dependencies");
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

            // Adjusting frequency/travel must not change an author's axis choices.
            const SpringAxes authored{{false,true,true},{false,true,false}};
            expect(spring_axes(*bust,authored)==authored,"Plain tuning changed authored axes");
            auto clamp_only=springs;
            clamp_only["controls"][1]["max_displacement"]=clamped["controls"][1]["max_displacement"];
            const auto clamp_model=ControlSet::parse(clamp_only);
            expect(spring_axes(*clamp_model.find("bust"),authored)==authored,
                   "A travel clamp must not infer axis changes from a bone name");
            auto plane_only=clamp_only;
            plane_only["controls"][1]["planar_constraint"]="y";
            const auto plane_model=ControlSet::parse(plane_only);
            const SpringAxes expected_plane{{false,false,true},{false,true,false}};
            expect(spring_axes(*plane_model.find("bust"),authored)==expected_plane,
                   "A planar lock must retain other disabled axes and authored rotation");
            plane_only["controls"][1]["translate"]=Json::array({true,true,true});
            const auto explicit_model=ControlSet::parse(plane_only);
            const SpringAxes expected_explicit{{true,false,true},{false,true,false}};
            expect(spring_axes(*explicit_model.find("bust"),authored)==expected_explicit,
                   "Explicit translation must not bypass the planar lock");
            plane_only["controls"][1]["planar_constraint"]="none";
            plane_only["controls"][1].erase("translate");
            const auto neutral_model=ControlSet::parse(plane_only);
            expect(spring_axes(*neutral_model.find("bust"),authored)==authored,
                   "Removing overrides must recover captured authored flags");
            const SpringAxes forced{{true,false,true},{true,true,true}};
            expect(spring_axes(*cbust,authored)==forced,"Explicit rotation overrides were lost");

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
        {
            // 1.0.0-beta Next-Gen: Glow, Opacity, and Shape with Morph Formulas
            auto advanced=Json::parse(R"({"schema":1,"controls":[
              {"id":"cloth","name":"Garment","role":"garment","group":"outfit","default":[1,1,1,1]},
              {"id":"runes","name":"Rune Glow","kind":"glow","group":"body","role":"glow",
               "pulse_hz":1.5,"combat_reactive":true,"default":[5.0,1.5,0,1],
               "bindings":[{"slot":0,"parameter":"GlowIntensity"}]},
              {"id":"sheer","name":"Gown Sheerness","kind":"opacity","group":"outfit","role":"accent",
               "default":[0.4,0,0,1],"bindings":[{"slot":1,"parameter":"OpacityMultiplier"}]},
              {"id":"butt","name":"Glute Shape","kind":"shape","group":"body","role":"figure",
               "morph":"Glutes","min":0,"max":1,"default":[0,0,0,1],
               "formulas":[
                 {"target":"butt001","type":"BoneCenterY","multiplier":0.015},
                 {"target":"pelvis","type":"OrientationX","multiplier":-0.002}
               ]}],
              "surfaces":[{"id":"body","parameter":"BaseColorMap  non VT","slots":[0],"layers":{"cloth":"dye-cloth.png"}}],
              "palettes":[{"id":"red","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
            auto parsed=ControlSet::parse(advanced);
            auto no_glow_binding=advanced;
            no_glow_binding["controls"][1]["bindings"]=Json::array();
            rejects([&]{ControlSet::parse(no_glow_binding);});
            auto invalid_pulse=advanced;
            invalid_pulse["controls"][1]["pulse_hz"]=-1;
            rejects([&]{ControlSet::parse(invalid_pulse);});
            const auto* runes=parsed.find("runes");
            expect(runes->kind==ControlKind::Glow && runes->scalar,"Glow control not parsed");
            expect(std::string(control_kind_name(ControlKind::Glow))=="glow","Glow kind name wrong");
            expect(runes->pulse_hz==1.5f && runes->combat_reactive,"Glow pulse_hz or combat_reactive lost");
            expect(runes->value[0]==5.0f,"Glow intensity value lost");

            const auto* sheer=parsed.find("sheer");
            expect(sheer->kind==ControlKind::Opacity && sheer->scalar,"Opacity control not parsed");
            expect(std::string(control_kind_name(ControlKind::Opacity))=="opacity","Opacity kind name wrong");
            expect(sheer->minimum==0.f && sheer->maximum==1.f,"Opacity bounds wrong");

            const auto* butt=parsed.find("butt");
            expect(butt->kind==ControlKind::Shape && butt->formulas.size()==2,"Morph formulas lost");
            expect(butt->formulas[0].target=="butt001" && butt->formulas[0].type=="BoneCenterY" && std::abs(butt->formulas[0].multiplier-0.015)<1e-6,"Formula 0 mismatch");
            expect(butt->formulas[1].target=="pelvis" && butt->formulas[1].type=="OrientationX" && std::abs(butt->formulas[1].multiplier-(-0.002))<1e-6,"Formula 1 mismatch");

            // Invalid formula tests
            auto bad_formula=advanced;
            bad_formula["controls"][3]["formulas"][0]["type"]="InvalidTargetType";
            rejects([&]{ControlSet::parse(bad_formula);});
            auto bad_target=advanced;
            bad_target["controls"][3]["formulas"][0]["target"]="bad-bone-name";
            rejects([&]{ControlSet::parse(bad_target);});
            auto stray_formula=advanced;
            stray_formula["controls"][0]["formulas"]=Json::array();
            rejects([&]{ControlSet::parse(stray_formula);});
        }
        {
            // Reserved dynamics metadata is retained, not proof of runtime physics.
            auto kawaii = Json::parse(R"({
              "schema": 1,
              "controls": [
                {"id": "bust_jiggle", "name": "Bust Motion", "kind": "spring",
                 "nodes": ["brust001", "brust002"],
                 "frequency": {"min": 0.5, "max": 4.0, "default": 1.8},
                 "damping_ratio": {"min": 0.1, "max": 1.0, "default": 0.45},
                 "max_displacement": {"min": 1.0, "max": 12.0, "default": 6.5},
                 "planar_constraint": "y",
                 "world_damping": 0.25,
                 "limit_angle": 35.0,
                 "collision_radius": 7.5,
                 "gravity_scale": 1.2
                },
                {"id": "ponytail_hair", "name": "Ponytail Sway", "kind": "spring",
                 "nodes": ["hair_root"],
                 "frequency": {"min": 1.0, "max": 5.0, "default": 2.5},
                 "damping_ratio": {"min": 0.2, "max": 0.8, "default": 0.5},
                 "world_damping": 0.4,
                 "limit_angle": 60.0
                },
                {"id": "clitoral_hue", "name": "Clitoral Tone", "default": [1, 1, 1, 1]},
                {"id": "orifice_depth", "name": "Vaginal Depth", "kind": "scalar", "default": [1.0, 0, 0, 1],
                 "bindings": [{"slot": 0, "parameter": "OrificeDepth"}]},
                {"id": "choker_necklace", "name": "Choker", "default": [1, 1, 1, 1]},
                {"id": "cape_fabric", "name": "Velvet Cape", "default": [1, 1, 1, 1]}
              ],
              "surfaces": [
                {"id": "body", "parameter": "BaseColorMap non VT", "slots": [0],
                 "layers": {
                   "clitoral_hue": "dye-clit.png",
                   "choker_necklace": "dye-choker.png",
                   "cape_fabric": "dye-cape.png"
                 }}
              ]
            })");
            auto parsed_k = ControlSet::parse(kawaii);
            const auto* bust = parsed_k.find("bust_jiggle");
            expect(bust != nullptr && bust->kind == ControlKind::Spring, "Bust spring control missing");
            expect(bust->planar_constraint == 2, "Planar constraint Y lost");
            expect(bust->world_damping && std::abs(*bust->world_damping - 0.25f) < 1e-6, "World damping mismatch");
            expect(bust->limit_angle && std::abs(*bust->limit_angle - 35.0f) < 1e-6, "Limit angle mismatch");
            expect(bust->collision_radius && std::abs(*bust->collision_radius - 7.5f) < 1e-6, "Collision radius mismatch");
            expect(bust->gravity_scale && std::abs(*bust->gravity_scale - 1.2f) < 1e-6, "Gravity scale mismatch");
            expect(bust->group == ControlGroup::Body, "Bust role group must be Body");

            const auto* pony = parsed_k.find("ponytail_hair");
            expect(pony != nullptr && pony->group == ControlGroup::Body, "Hair group must be Body");
            expect(pony->world_damping && std::abs(*pony->world_damping - 0.4f) < 1e-6, "Hair world damping mismatch");
            expect(pony->limit_angle && std::abs(*pony->limit_angle - 60.0f) < 1e-6, "Hair limit angle mismatch");
            expect(!pony->collision_radius && !pony->gravity_scale,"Absent dynamics settings became zero overrides");
            auto absent=kawaii;
            for(const auto* key:{"world_damping","limit_angle","collision_radius","gravity_scale"})
                absent["controls"][0].erase(key);
            const auto absent_model=ControlSet::parse(absent);
            const auto* absent_control=absent_model.find("bust_jiggle");
            expect(!absent_control->world_damping && !absent_control->limit_angle &&
                   !absent_control->collision_radius && !absent_control->gravity_scale,
                   "Absence must preserve all authored dynamics values");
            auto zeros=absent;
            for(const auto* key:{"world_damping","limit_angle","collision_radius","gravity_scale"}) {
                zeros["controls"][0][key]=0;
                for(const auto& invalid:{Json(true),Json(nullptr),Json("0"),Json::array(),
                                        Json(std::numeric_limits<double>::quiet_NaN())}) {
                    auto bad=absent; bad["controls"][0][key]=invalid;
                    rejects([&]{ControlSet::parse(bad);});
                }
                auto stray=absent; stray["controls"][2][key]=0;
                rejects([&]{ControlSet::parse(stray);});
            }
            const auto zero_model=ControlSet::parse(zeros);
            const auto* zero_control=zero_model.find("bust_jiggle");
            expect(zero_control->world_damping==0.f && zero_control->limit_angle==0.f &&
                   zero_control->collision_radius==0.f && zero_control->gravity_scale==0.f,
                   "Explicit zero dynamics settings must remain present");

            const auto* clit = parsed_k.find("clitoral_hue");
            expect(clit != nullptr && clit->group == ControlGroup::Body, "Clitoris group must be Body");
            expect(clit->hue_locked, "Clitoris role must be hue locked with skin");

            const auto* orifice = parsed_k.find("orifice_depth");
            expect(orifice != nullptr && orifice->group == ControlGroup::Body, "Orifice group must be Body");

            const auto* choker = parsed_k.find("choker_necklace");
            expect(choker != nullptr && choker->group == ControlGroup::Outfit, "Choker group must be Outfit");
            expect(choker->hue_locked, "Jewelry role must be hue locked");

            const auto* cape = parsed_k.find("cape_fabric");
            expect(cape != nullptr && cape->group == ControlGroup::Outfit && !cape->hue_locked, "Fabric cape must be unlocked Outfit");

            // Rejection tests for invalid secondary physics parameters
            auto bad_pc = kawaii;
            bad_pc["controls"][0]["planar_constraint"] = "w";
            rejects([&]{ ControlSet::parse(bad_pc); });

            auto bad_wd = kawaii;
            bad_wd["controls"][0]["world_damping"] = 1.5;
            rejects([&]{ ControlSet::parse(bad_wd); });

            auto bad_la = kawaii;
            bad_la["controls"][0]["limit_angle"] = 200.0;
            rejects([&]{ ControlSet::parse(bad_la); });

            auto bad_cr = kawaii;
            bad_cr["controls"][0]["collision_radius"] = 150.0;
            rejects([&]{ ControlSet::parse(bad_cr); });

            auto bad_gs = kawaii;
            bad_gs["controls"][0]["gravity_scale"] = 10.0;
            rejects([&]{ ControlSet::parse(bad_gs); });
        }
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[
              {"id":"hair","name":"Hair dynamics","kind":"dynamics","nodes":["CSS_Hair_Ponytail_01"],
               "angular_spring":{"min":0,"max":1000,"default":80},
               "damping":{"min":0.7,"max":1,"default":0.8},
               "gravity":{"min":-5,"max":5,"default":0.1}}],
              "palettes":[{"id":"float","name":"Floating","values":{"hair":[120,0.9,-0.5,1]}}]})");
            const auto model=ControlSet::parse(recipe);
            const auto* hair=model.find("hair");
            expect(hair && hair->kind==ControlKind::Dynamics && hair->scalar,"Missing dynamics kind");
            expect(std::string(control_kind_name(hair->kind))=="dynamics","Dynamics kind serialization lost");
            expect(hair->value==ControlValue({80,.8f,.1f,1}),"Dynamics defaults changed units");
            expect(control_channel_count(*hair)==3,"Dynamics needs three channels");
            expect(control_channel(*hair,0).maximum==1000 && control_channel(*hair,2).minimum==-5,
                   "Solver ranges must permit high stiffness and negative gravity");
            rejects([&]{control_channel(*hair,-1);});
            rejects([&]{control_channel(*hair,3);});
            expect(control_values(model,{}).empty(),"Original must leave authored dynamics untouched");
            const auto selected=choose_palette(model,{},"float");
            const auto value=control_values(model,selected).at("hair");
            expect(value==ControlValue({120,.9f,-.5f,1}),"Dynamics palette changed solver values");
            Customization custom;custom.values["hair"]={1000,1,-5,1};
            const auto roundtrip=Customization::parse(custom.json());
            expect(roundtrip==custom && control_values(model,roundtrip).at("hair")==custom.values.at("hair"),
                   "Snapshot lost high stiffness or negative gravity");
            State saved;
            saved.selections["Solomon"]={"test","default",custom};
            saved.remembered_custom["test"]=custom;
            saved.presets["floating"].selections=saved.selections;
            const auto loaded=State::parse(saved.json());
            expect(loaded.selections.at("Solomon").custom==custom &&
                   loaded.remembered_custom.at("test")==custom &&
                   loaded.presets.at("floating").selections.at("Solomon").custom==custom,
                   "State/profile round-trip lost solver values");
            const auto tuning=dynamics_settings(*hair,value);
            expect(tuning==DynamicsSettings{120,.9f,.9f,-.5f,true,true,true,false},
                   "Dynamics tuning must use direct constants and enable required override flags");
            expect(!dynamics_settings(*hair,{0,.7f,0,1}).spring_enabled,"Zero stiffness must disable angular spring forcing");
            auto changed=tuning; changed.angular_spring=0; changed.spring_enabled=false; changed.gravity=2;
            expect(!dynamics_reset_required(tuning,changed),"Direct spring/gravity updates must not reset motion");
            changed.linear_damping=.75f;
            expect(dynamics_reset_required(tuning,changed),"Cached linear damping requires reset");
            changed=tuning; changed.angular_damping=.75f;
            expect(dynamics_reset_required(tuning,changed),"Cached angular damping requires reset");
            changed=tuning; changed.override_linear=false;
            expect(dynamics_reset_required(tuning,changed),"Restoring authored damping mode requires reset");
            changed=tuning; changed.gravity_override=true;
            expect(dynamics_reset_required(tuning,changed),"Gravity override mode requires reset");
            const auto reset=choose_palette(model,custom,"original");
            expect(control_values(model,reset).empty(),"Original must release solver overrides");
            for(const auto& value:{ControlValue{1001,.8f,0,1},ControlValue{80,.69f,0,1},
                                  ControlValue{80,.8f,-6,1},ControlValue{80,.8f,0,.5f}}) {
                auto invalid=custom;invalid.values["hair"]=value;
                rejects([&]{control_values(model,invalid);});
            }
            for(const auto* key:{"frequency","damping_ratio","default","max_displacement","bindings"}) {
                auto invalid=recipe;invalid["controls"][0][key]=Json::array();
                rejects([&]{ControlSet::parse(invalid);});
            }
            for(const auto* key:{"angular_spring","damping","gravity"}) {
                for(const auto& invalid:{Json(true),Json(nullptr),Json("0"),Json(std::numeric_limits<double>::infinity())}) {
                    auto bad=recipe;bad["controls"][0][key]["default"]=invalid;
                    rejects([&]{ControlSet::parse(bad);});
                }
                auto missing=recipe;missing["controls"][0].erase(key);
                rejects([&]{ControlSet::parse(missing);});
            }
            auto duplicate=recipe;auto second=duplicate["controls"][0];second["id"]="other";
            duplicate["controls"].push_back(second);
            rejects([&]{ControlSet::parse(duplicate);});
            auto color=Json::parse(R"({"schema":1,"controls":[{"id":"hair","name":"Hair","kind":"scalar",
                "default":[1,0,0,1],"bindings":[{"slot":0,"parameter":"Hair"}]}]})");
            const auto color_model=ControlSet::parse(color);
            rejects([&]{control_values(color_model,roundtrip);});
            expect(compatible_values(color_model,roundtrip).values.empty(),"A changed control kind must not retain incompatible solver data");
            for(const auto& invalid:{Json::array({true,.8,0,1}),Json::array({1001,.8,0,1}),Json::array({80,.8,-6,1})}) {
                auto snapshot=custom.json();snapshot["values"]["hair"]=invalid;
                rejects([&]{Customization::parse(snapshot);});
            }
        }
        std::cout<<checks<<" control behavior checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

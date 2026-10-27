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
css::Control hair_rig_fixture() {
    return css::ControlSet::parse(nlohmann::json::parse(R"({"schema":1,"controls":[{"id":"hair-motion","name":"Hair motion","kind":"rig",
      "stiffness":{"min":100,"max":250,"default":150},"damping":{"min":12,"max":24,"default":18},
      "gravity":{"min":-0.2,"max":0.2,"default":0}}]})")).controls.at(0);
}
int main() {
    try {
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[
                {"id":"glow","name":"Glow","kind":"intensity","max":8,"default":[0,0,0,1],"bindings":[{"slot":3,"parameter":"GlowStrength"}]}],
                "palettes":[{"id":"floral","name":"Floral","values":{"glow":[2,0,0,1]},"parameters":[
                    {"slot":3,"parameter":"BaseTexture","kind":"texture","texture":"/Game/CSS/Test/T_Floral.T_Floral"},
                    {"slot":3,"parameter":"PatternTint","kind":"vector","value":[0.04,0.2,4,-2]},
                    {"slot":3,"parameter":"GlowStrength","kind":"scalar","value":2}]}]})");
            const auto model=ControlSet::parse(recipe);
            expect(model.palettes[0].parameters.size()==3,"Authored palette parameters lost");
            expect(model.palettes[0].parameters[1].value==ControlValue{.04f,.2f,4,-2},"Linear material vector was changed");
            Customization current; current.values["glow"]={7,0,0,1}; current.expression="smile";
            const auto chosen=choose_palette(model,current,"floral");
            expect(chosen.values.empty() && control_values(model,chosen).at("glow")[0]==2,"Palette must replace its manual overrides");
            expect(restore_customization(model,chosen.json())==chosen,"Material palette identity must survive profile restore");
            auto manual=chosen; manual.values["glow"]={5,0,0,1};
            expect(control_values(model,manual).at("glow")[0]==5,"Manual control must override palette baseline");
            expect(choose_palette(model,manual,"original").expression=="smile","Original must preserve expression");
            expect(control_values(model,choose_palette(model,manual,"original")).empty(),"Original must drop palette and manual values");
            auto bad=recipe; bad["palettes"][0]["values"]=Json::object();
            rejects([&]{ControlSet::parse(bad);});
            bad=recipe; auto duplicate=bad["palettes"][0]["parameters"][0]; duplicate["parameter"]="basetexture";
            bad["palettes"][0]["parameters"].push_back(duplicate); rejects([&]{ControlSet::parse(bad);});
            for(const auto& patch:{Json{{"slot",.5}},Json{{"slot",128}},Json{{"slot",true}},Json{{"slot",4294967296LL}},
                                  Json{{"texture","/Game/MissingObject"}},Json{{"association","layer"}},Json{{"value",1}}}) {
                bad=recipe; bad["palettes"][0]["parameters"][0].update(patch); rejects([&]{ControlSet::parse(bad);});
            }
            for(const auto& v:{Json(true),Json(65537),Json("1"),Json(nullptr)}) {
                bad=recipe; bad["palettes"][0]["parameters"][2]["value"]=v; rejects([&]{ControlSet::parse(bad);});
            }
            auto only=recipe; only["controls"]=Json::array(); only["palettes"][0]["values"]=Json::object();
            expect(ControlSet::parse(only).palettes[0].parameters.size()==3,"A material-only palette must not need dummy controls");
            bad=only; bad["palettes"][0]["parameters"]=Json::array();
            for(int i=0;i<513;++i) bad["palettes"][0]["parameters"].push_back({{"slot",0},{"parameter","P"+std::to_string(i)},{"kind","scalar"},{"value",0}});
            rejects([&]{ControlSet::parse(bad);});
        }
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[
                {"id":"fabric_visible","name":"Sheer fabric","kind":"toggle","default":[1,0,0,1],
                 "bindings":[{"slot":12,"surface":"overlay","parameter":"FabricVisible"}]},
                {"id":"fabric_opacity","name":"Fabric opacity","kind":"opacity","default":[1,0,0,1],
                 "bindings":[{"slot":12,"surface":"overlay","parameter":"FabricOpacityScale"}]}],
                "palettes":[{"id":"mist","name":"Mist","values":{"fabric_opacity":[0.4,0,0,1]}}]})");
            const auto model=ControlSet::parse(recipe);
            Customization custom; custom.values["fabric_visible"]={0,0,0,1};
            custom.values["fabric_opacity"]={.25f,0,0,1};
            expect(hidden_control_sections(model,control_values(model,custom)).empty(),
                   "Material visibility must not hide the base mesh section");
            expect(Customization::parse(custom.json())==custom,"Material toggle must persist");
            const auto selected=choose_palette(model,custom,"mist");
            const auto values=control_values(model,selected);
            expect(values.at("fabric_visible")[0]==0 && values.at("fabric_opacity")[0]==.4f,
                   "A palette opacity change must preserve the independent fabric switch");
            expect(control_values(model,choose_palette(model,selected,"original")).empty(),
                   "Original must restore authored visibility and opacity");
            auto invalid=recipe; invalid["controls"][0]["default"][0]=.5;
            rejects([&]{ControlSet::parse(invalid);});
            invalid=recipe; invalid["controls"][0]["occludes_sections"]=Json::array({0});
            rejects([&]{ControlSet::parse(invalid);});
            invalid=recipe; invalid["controls"][0]["bindings"]=Json::array();
            rejects([&]{ControlSet::parse(invalid);});
            auto many=Json{{"schema",1},{"controls",Json::array()}};
            Customization saved;
            for(size_t i=0;i<max_customization_controls;++i) {
                auto item=recipe["controls"][0];
                const auto id="part_"+std::to_string(i);
                item["id"]=id; item["bindings"][0]["slot"]=i;
                many["controls"].push_back(item); saved.values[id]={0,0,0,1};
            }
            expect(ControlSet::parse(many).controls.size()==128,"128 controls must be supported");
            expect(Customization::parse(saved.json())==saved,"All 128 control values must persist");
            saved.values["overflow"]={0,0,0,1};
            rejects([&]{Customization::parse(saved.json());});
            auto extra=recipe["controls"][0]; extra["id"]="overflow";
            many["controls"].push_back(extra);
            rejects([&]{ControlSet::parse(many);});
        }
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[{"id":"fabric","name":"Fabric","kind":"color","default":[1,1,1,1],
                "bindings":[{"slot":12,"parameter":"FabricColor"}]}]})");
            expect(ControlSet::parse(recipe).controls[0].bindings[0].surface==MaterialSurface::Base,
                   "Existing material bindings must keep targeting the base");
            auto& binding=recipe["controls"][0]["bindings"][0];
            binding["surface"]="overlay"; binding["association"]="layer"; binding["layer"]=2;
            auto parsed=ControlSet::parse(recipe).controls[0].bindings[0];
            expect(parsed.surface==MaterialSurface::Overlay && parsed.association==0 && parsed.layer==2,
                   "Overlay selection must not replace material parameter layer addressing");
            binding["surface"]="base";
            expect(ControlSet::parse(recipe).controls[0].bindings[0].surface==MaterialSurface::Base,"Explicit base binding rejected");
            for(const auto& bad:{Json("fabric"),Json(""),Json(1),Json(nullptr)}) {
                binding["surface"]=bad;
                rejects([&]{ControlSet::parse(recipe);});
            }
        }
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
            for(double value:{-20.,11.81042492389679,20.}) {
                auto tall=look; tall.ground_offset_cm=value;
                expect(Customization::parse(tall.json()).ground_offset_cm==value,"Supported saved heel height changed");
            }
            rejects([&]{Customization::parse(Json::parse(R"({"palette":"original","values":{},"ground_offset_cm":20.01})"));});
            rejects([&]{Customization::parse(Json::parse(R"({"palette":"original","values":{},"ground_offset_cm":-20.01})"));});
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
            // Body rig travel: an optional fifth channel in degrees, saved beside the
            // four-number value. Recipes and rigs without it keep working unchanged.
            auto legacy=Json::parse(R"({"schema":1,"controls":[{"id":"chest-motion","name":"Chest motion","kind":"rig",
              "solver":"angular_body","regions":["brust001","brust002"],
              "frequency":{"min":0.5,"max":6,"default":2},"damping_ratio":{"min":0.1,"max":2,"default":0.7},
              "motion_amount":{"min":0,"max":5,"default":1}}]})");
            const auto old=ControlSet::parse(legacy);
            const auto& old_chest=old.controls[0];
            expect(!body_travel_control(old_chest) && control_channel_count(old_chest)==4,"A recipe without travel gained a channel");
            rejects([&]{control_channel(old_chest,4);});
            rejects([&]{apply_customize(old,{},{{"action","control"},{"control","chest-motion"},{"channel",4},{"value",6}});});
            for(const auto& preset:physics_presets(old_chest)) expect(!physics_preset_travel(old_chest,preset),"Preset travel on a part without travel");
            const auto old_firm=apply_physics_preset(old,{},"firm","chest-motion");
            expect(old_firm.travel.empty() && old_firm.values.at("chest-motion")[0]==2.6f,"Legacy preset must write only its three channels");

            auto recipe=legacy;
            recipe["controls"][0]["travel"]={{"min",2},{"max",24},{"default",4}};
            recipe["controls"].push_back(Json::parse(R"({"id":"glute-motion","name":"Glute motion","kind":"rig",
              "solver":"angular_body","regions":["butt001","butt002"],
              "frequency":{"min":0.5,"max":6,"default":2},"damping_ratio":{"min":0.1,"max":2,"default":0.7},
              "motion_amount":{"min":0,"max":5,"default":1},"travel":{"min":2,"max":12,"default":8}})"));
            const auto model=ControlSet::parse(recipe);
            const auto& chest=model.controls[0]; const auto& glute=model.controls[1];
            expect(body_travel_control(chest) && control_channel_count(chest)==5,"Travel channel missing");
            // The details window puts Travel before the Motion switch; the map is its own inverse.
            for(int slot=0;slot<5;++slot) {
                expect(physics_slot_channel(chest,slot)==(slot==3?4:slot==4?3:slot),"Travel slot order wrong");
                expect(physics_slot_channel(chest,physics_slot_channel(chest,slot))==slot,"Travel slot map is not its own inverse");
            }
            for(int slot=0;slot<4;++slot) expect(physics_slot_channel(old_chest,slot)==slot,"A rig without travel changed its slot order");
            const auto range=control_channel(chest,4);
            expect(range.minimum==2 && range.maximum==24 && range.value==4 && range.step==.5f,"Travel range wrong");
            expect(chest.value==ControlValue{2,.7f,1,1},"Travel changed the four saved channels");
            for(const auto& bad:{Json{{"min",0},{"max",24},{"default",4}},Json{{"min",2},{"max",46},{"default",4}},
                                 Json{{"min",8},{"max",8},{"default",8}},Json{{"min",2},{"max",24},{"default",30}},
                                 Json{{"min",2},{"max",24}},Json{{"min",2},{"max",24},{"default",4},{"step",1}},
                                 Json{{"min",true},{"max",24},{"default",4}},Json(6),Json::array({2,24,4})}) {
                auto broken=recipe; broken["controls"][0]["travel"]=bad;
                rejects([&]{ControlSet::parse(broken);});
            }
            auto hair=Json::parse(R"({"schema":1,"controls":[{"id":"hair-motion","name":"Hair motion","kind":"rig",
              "stiffness":{"min":100,"max":250,"default":150},"damping":{"min":12,"max":24,"default":18},
              "gravity":{"min":-0.2,"max":0.2,"default":0},"travel":{"min":2,"max":24,"default":4}}]})");
            rejects([&]{ControlSet::parse(hair);});
            auto spring=Json::parse(R"({"schema":1,"controls":[{"id":"bust","name":"Bust","kind":"spring","nodes":["brust001"],
              "frequency":{"min":1,"max":3,"default":2},"damping_ratio":{"min":0.1,"max":1,"default":0.7},
              "travel":{"min":2,"max":24,"default":4}}]})");
            rejects([&]{ControlSet::parse(spring);});

            // Composition: travel lands on the owned regions only, and only on a rig that has the inputs.
            BodyRigSettings authored; authored.use_regions=true; authored.has_travel=true;
            authored.frequency.fill(5); authored.damping.fill(1.2f); authored.motion.fill(.9f); authored.enabled.fill(true);
            const std::map<std::string,float> chest_travel{{"chest-motion",10}};
            auto travelled=body_rig_settings(model.controls,{},authored,chest_travel);
            // The untouched glute part takes its recipe default (8) on a rig with the inputs.
            expect(travelled.travel[0]==10 && travelled.travel[1]==10 && travelled.travel[2]==8 && travelled.travel[6]==0,
                   "Travel must reach exactly the owned regions");
            expect(travelled.frequency==authored.frequency && travelled.enabled==authored.enabled && travelled.use_regions,
                   "A travel-only override must keep the region tuning");
            auto older=authored; older.has_travel=false;
            auto both=body_rig_settings(model.controls,{{"glute-motion",{3,.8f,.5f,1}}},authored,chest_travel);
            expect(both.travel[0]==10 && both.travel[2]==8 && both.frequency[2]==3 && both.frequency[0]==5,"Travel and tuning overrides must compose independently");
            // A look without travel: a part whose sliders match a preset swings with that preset's
            // travel (Soft / Saggy glutes 16, clamped to the part's 12), the others at the recipe
            // default; the Travel readout says the same; an explicit Travel wins over both.
            {
                const auto presets=physics_presets(glute);
                const auto soft=std::find_if(presets.begin(),presets.end(),[](const auto& p){ return p.id=="soft"; });
                expect(soft!=presets.end() && physics_preset_travel(glute,*soft)==12,"Soft glute travel must clamp to the range");
                const ControlValue soft_glute{1.20f,.20f,2.50f,1};
                expect(effective_travel(glute,soft_glute,std::nullopt)==12 && effective_travel(glute,soft_glute,5.f)==5 &&
                       !effective_travel(glute,{3,.8f,.5f,1},std::nullopt) && !effective_travel(old_chest,{2.6f,.65f,.6f,1},std::nullopt),
                       "Effective travel must follow the override, then the matched preset, then nothing");
                auto inferred=body_rig_settings(model.controls,{{"glute-motion",soft_glute}},authored,{});
                expect(inferred.travel[2]==12 && inferred.travel[3]==12 && inferred.travel[0]==4 && inferred.frequency[2]==1.2f,
                       "A matched preset must set the part's travel and the rest keep the recipe default");
                auto overridden=body_rig_settings(model.controls,{{"glute-motion",soft_glute}},authored,{{"glute-motion",5}});
                expect(overridden.travel[2]==5,"An explicit travel must win over the matched preset");
                auto untouched=body_rig_settings(model.controls,{},authored,{});
                expect(untouched.use_regions && untouched.travel[0]==4 && untouched.travel[2]==8 && untouched.frequency==authored.frequency,
                       "Recipe default travel must reach a rig with the inputs even with nothing saved");
                expect(body_rig_settings(model.controls,{{"glute-motion",soft_glute}},older,{}).travel==older.travel,
                       "A rig without travel inputs must ignore inferred travel");
            }
            expect(body_rig_settings(model.controls,{},older,chest_travel)==older,"A rig without travel inputs must be left alone by travel");
            auto older_mixed=body_rig_settings(model.controls,{{"chest-motion",{3,.8f,.5f,1}}},older,chest_travel);
            expect(older_mixed.frequency[0]==3 && older_mixed.travel==older.travel,"Old rig must still take the other channels");
            auto global=authored; global.use_regions=false; global.global_travel=0; global.travel.fill(7);
            auto filled=body_rig_settings(model.controls,{},global,chest_travel);
            expect(filled.use_regions && filled.travel[0]==10 && filled.travel[3]==8 && filled.travel[4]==0 && filled.frequency[3]==global.global_frequency,
                   "Global mode must seed travel from the global input");
            rejects([&]{body_rig_settings(model.controls,{},authored,{{"chest-motion",30}});});
            rejects([&]{body_rig_settings(old.controls,{},authored,{{"chest-motion",6}});});

            // Requests: channel 4 steps from the author's default, the value form sets it, reset drops it.
            Customization look;
            look=apply_customize(model,look,{{"action","control"},{"control","chest-motion"},{"channel",4},{"delta",1}});
            expect(look.travel.at("chest-motion")==4.5f && look.values.empty(),"Travel step must not touch the saved value");
            look=apply_customize(model,look,{{"action","control"},{"control","chest-motion"},{"channel",4},{"value",12}});
            expect(look.travel.at("chest-motion")==12,"Travel value not set");
            rejects([&]{apply_customize(model,look,{{"action","control"},{"control","chest-motion"},{"channel",4},{"value",30}});});
            rejects([&]{apply_customize(model,look,{{"action","control"},{"control","chest-motion"},{"channel",5},{"value",3}});});
            expect(control_travel(model,look).at("chest-motion")==12,"Travel overrides not reported");
            expect(!apply_customize(model,look,{{"action","reset_control"},{"control","chest-motion"}}).travel.contains("chest-motion"),
                   "Reset must drop the travel override");
            look.travel["glute-motion"]=10;

            // Saved looks: round trip, older saves without the key, schema-free bounds.
            const auto saved=look.json();
            expect(saved.contains("travel") && Customization::parse(saved)==look,"Travel did not round trip");
            expect(!Customization{}.json().contains("travel"),"Empty travel must not be written");
            expect(Customization::parse(Json::parse(R"({"palette":"original","values":{"chest-motion":[2,0.7,1,1]}})")).travel.empty(),
                   "An older save must load without travel");
            for(const auto& bad:{Json(0),Json(-1),Json(46),Json("6"),Json(nullptr)}) {
                auto broken=saved; broken["travel"]["chest-motion"]=bad;
                rejects([&]{Customization::parse(broken);});
            }
            auto broken=saved; broken["travel"]=Json::array(); rejects([&]{Customization::parse(broken);});
            expect(restore_customization(model,saved)==look,"Profile restore lost travel");
            auto narrowed=look; narrowed.travel["glute-motion"]=14;
            const auto carried=compatible_values(model,narrowed);
            expect(carried.travel.at("chest-motion")==12 && !carried.travel.contains("glute-motion"),
                   "Travel outside the new range must fall back to the default");
            expect(compatible_values(old,look).travel.empty(),"A variant without travel must drop travel");
            rejects([&]{control_travel(old,look);});

            // Palettes: Original clears travel, a palette clears only the parts it sets.
            auto with_palette=recipe;
            with_palette["palettes"]=Json::parse(R"([{"id":"calm","name":"Calm","values":{"glute-motion":[2,0.9,0.5,1]}}])");
            const auto painted=ControlSet::parse(with_palette);
            expect(choose_palette(painted,look,"original").travel.empty(),"Original kept travel");
            const auto calm=choose_palette(painted,look,"calm");
            expect(calm.travel.contains("chest-motion") && !calm.travel.contains("glute-motion"),"Palette travel scope wrong");

            // Presets: built-ins carry per-region travel, clamped to the part's range.
            const auto presets=physics_presets(chest);
            const float chest_expected[]={4,6,10,14,20}, glute_expected[]={8,10,12,12,12};
            for(size_t i=0;i<5;++i) {
                expect(presets[i].builtin && physics_preset_travel(chest,presets[i])==chest_expected[i],"Chest preset travel wrong");
                expect(physics_preset_travel(glute,physics_presets(glute)[i])==glute_expected[i],"Glute preset travel not clamped to range");
            }
            std::string label;
            const auto firm=apply_physics_preset(model,look,"firm","chest-motion",&label);
            expect(label=="Firm" && firm.travel.at("chest-motion")==4 && firm.travel.at("glute-motion")==10 &&
                   firm.values.at("chest-motion")[0]==2.6f,"Preset must write travel to its part only");
            const auto everywhere=apply_physics_preset(model,{},"OMG_Earthquake","");
            expect(everywhere.travel.at("chest-motion")==20 && everywhere.travel.at("glute-motion")==12,"Preset for all parts lost travel");
            rejects([&]{apply_physics_preset(model,{},"no-such-preset","");});
            const auto held=firm.values.at("chest-motion");
            expect(matching_physics_preset(chest,held)=="firm" && matching_physics_preset(chest,held,4.f)=="firm","Applied preset with travel not recognised");
            expect(matching_physics_preset(chest,held,6.f).empty(),"Moved travel still matched a preset");
            auto own=recipe;
            own["controls"][0]["presets"]=Json::parse(R"([{"id":"studio","name":"Studio","value":[3,0.9,0.5],"travel":16}])");
            const auto studio=ControlSet::parse(own);
            expect(physics_preset_travel(studio.controls[0],studio.controls[0].presets[0])==16,"Package preset travel lost");
            own["controls"][0]["presets"][0]["travel"]=30; rejects([&]{ControlSet::parse(own);});
            own["controls"][0]["presets"][0]["travel"]="16"; rejects([&]{ControlSet::parse(own);});
            auto untravelled=legacy;
            untravelled["controls"][0]["presets"]=Json::parse(R"([{"id":"studio","name":"Studio","value":[3,0.9,0.5],"travel":6}])");
            rejects([&]{ControlSet::parse(untravelled);});

            // What the reflected AnimBP offers: absent is accepted, half-built is refused.
            expect(body_travel_support({})==BodyTravelSupport::Absent,"An older rig must be accepted without travel");
            expect(body_travel_support({true,true,7,true,true})==BodyTravelSupport::Ready,"Complete travel inputs refused");
            for(const auto& bad:{BodyTravelFields{true,true,7,false,false},BodyTravelFields{false,false,0,true,true},
                                 BodyTravelFields{true,true,6,true,true},BodyTravelFields{true,false,0,true,true},
                                 BodyTravelFields{true,true,7,true,false}})
                rejects([&]{body_travel_support(bad);});

            // Soft limit band: each built-in carries its own (Earthquake wider), springs and hair none.
            const float zone_expected[]={.5f,.5f,.5f,.5f,.6f};
            for(size_t i=0;i<5;++i) expect(presets[i].soft_zone==zone_expected[i],"Built-in soft zone wrong");
            for(const auto& preset:physics_presets(hair_rig_fixture())) expect(!preset.soft_zone && preset.builtin && !preset.travel,
                "Hair presets must be built-in without travel or soft zone");
            // The band follows the matched template; hand-tuned parts and package presets get the default.
            const auto quake=apply_physics_preset(model,{},"earthquake","");
            auto zones=physics_soft_zones(model.controls,control_values(model,quake),control_travel(model,quake));
            expect(zones.at("chest-motion")==.6f && zones.at("glute-motion")==.6f,"Earthquake must ask for its wider band");
            zones=physics_soft_zones(model.controls,control_values(model,firm),control_travel(model,firm));
            expect(zones.at("chest-motion")==.5f,"Firm band wrong");
            auto tuned=quake; tuned.values["chest-motion"][0]+=.3f;
            zones=physics_soft_zones(model.controls,control_values(model,tuned),control_travel(model,tuned));
            expect(zones.at("chest-motion")==body_soft_zone_default && zones.at("glute-motion")==.6f,
                   "A part moved off its template must fall back to the default band");
            expect(physics_soft_zones(model.controls,{},{}).empty(),"Untouched parts must leave the rig band alone");
            expect(physics_soft_zones(studio.controls,{{"chest-motion",{3,.9f,.5f,1}}},{{"chest-motion",16}}).at("chest-motion")==
                   body_soft_zone_default,"A package preset must use the default band");
            // Composition: the band reaches the owned regions only, and only on a rig with the inputs.
            auto banded=authored; banded.has_soft_zone=true; banded.soft_zone.fill(.5f);
            auto band=body_rig_settings(model.controls,{{"chest-motion",{2,.7f,1,1}}},banded,{},{{"chest-motion",.6f}});
            expect(band.soft_zone[0]==.6f && band.soft_zone[1]==.6f && band.soft_zone[2]==.5f,"Soft zone must reach exactly the owned regions");
            auto unbanded=authored; unbanded.has_soft_zone=false; unbanded.soft_zone.fill(.25f);
            expect(body_rig_settings(model.controls,{{"chest-motion",{2,.7f,1,1}}},unbanded,{},{{"chest-motion",.6f}}).soft_zone==unbanded.soft_zone,
                   "A rig without soft zone inputs must keep its own band");
            auto seeded=banded; seeded.use_regions=false; seeded.global_soft_zone=.4f; seeded.soft_zone.fill(.9f);
            auto seeded_out=body_rig_settings(model.controls,{{"chest-motion",{2,.7f,1,1}}},seeded,{},{{"chest-motion",.6f}});
            expect(seeded_out.soft_zone[0]==.6f && seeded_out.soft_zone[3]==.4f,"Global mode must seed the band from the global input");
            // On a rig with the travel inputs the recipe default travel activates the parts anyway,
            // so the band-alone rule is checked on a rig without them.
            auto band_only=banded; band_only.has_travel=false;
            expect(body_rig_settings(model.controls,{},band_only,{},{{"chest-motion",.6f}})==band_only,"A band alone must not activate a part");
            for(float bad:{-.1f,.95f,std::numeric_limits<float>::quiet_NaN()})
                rejects([&]{body_rig_settings(model.controls,{{"chest-motion",{2,.7f,1,1}}},banded,{},{{"chest-motion",bad}});});
            expect(body_soft_zone_support({})==BodyTravelSupport::Absent,"A rig without the band inputs must be accepted");
            expect(body_soft_zone_support({true,true,7,true,true})==BodyTravelSupport::Ready,"Complete band inputs refused");
            for(const auto& bad:{BodyTravelFields{true,true,7,false,false},BodyTravelFields{false,false,0,true,true},
                                 BodyTravelFields{true,true,6,true,true},BodyTravelFields{true,false,7,true,true}})
                rejects([&]{body_soft_zone_support(bad);});
        }
        {
            auto recipe=Json::parse(R"({"schema":1,"controls":[{"id":"hair-motion","name":"Hair motion","kind":"rig",
              "stiffness":{"min":100,"max":250,"default":150},"damping":{"min":12,"max":24,"default":18},
              "gravity":{"min":-0.2,"max":0.2,"default":0},"enabled":true}]})");
            const auto model=ControlSet::parse(recipe);
            const auto& control=model.controls.at(0);
            expect(control.kind==ControlKind::Rig && control.scalar && control_channel_count(control)==4,"Rig channels missing");
            for(int slot=0;slot<4;++slot) expect(physics_slot_channel(control,slot)==slot,"A hair rig changed its slot order");
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
            // A garment's rig runs only while one of its toggles shows it.
            auto rigged=Json::parse(R"({"schema":1,"controls":[
              {"id":"belt","name":"Belt","kind":"toggle","default":[1,0,0,1],"sections":[14],"anim_switch":"CSSBeltActive"},
              {"id":"buckle","name":"Buckle","kind":"toggle","default":[0,0,0,1],"sections":[15],"anim_switch":"CSSBeltActive"},
              {"id":"cape","name":"Cape","kind":"toggle","default":[1,0,0,1],"sections":[16]}]})");
            auto model=ControlSet::parse(rigged);
            expect(model.controls[0].anim_switch=="CSSBeltActive" && model.controls[2].anim_switch.empty(),"Toggle must keep its animation switch");
            Customization custom;
            auto switches=[&]{return control_anim_switches(model,control_values(model,custom));};
            expect(switches()==std::map<std::string,bool>{{"CSSBeltActive",true}},"Original must run the shown belt");
            custom.values["belt"]={0,0,0,1};
            expect(switches()==std::map<std::string,bool>{{"CSSBeltActive",false}},"Hiding every belt toggle must stop its rig");
            custom.values["buckle"]={1,0,0,1};
            expect(switches().at("CSSBeltActive"),"Any shown toggle must keep a shared switch on");
            custom.values.clear();
            expect(switches().at("CSSBeltActive"),"Clearing customization must restore the authored switch");
            for(const auto& invalid:{Json(""),Json("1Belt"),Json("Belt Active"),Json("Belt.Active"),Json(std::string(65,'a')),Json(3)}) {
                auto bad=rigged; bad["controls"][0]["anim_switch"]=invalid;
                rejects([&]{ControlSet::parse(bad);});
            }
            auto stray=Json::parse(R"({"schema":1,"controls":[{"id":"hue","name":"Hue","kind":"color","default":[1,1,1,1],"anim_switch":"CSSBeltActive"}]})");
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

            // A shape route (Profane 0.2.4 Chest shape) picks which morph a Shape slider drives,
            // plus the body geometry model that goes with that shape. It binds no texture.
            auto routed=shapes;
            routed["controls"].push_back(Json::parse(R"({"id":"hip_shape","name":"Hip shape","kind":"choice","group":"body",
              "role":"figure","default":[0,0,0,1],"shape_control":"hips","options":[
                {"name":"Perky","morph":"Hips","body_geometry_property":"CSSBodyGeometryJson"},
                {"name":"Juicy","morph":"HipsClassic","body_geometry_property":"CSSBodyGeometryClassicJson"}]})"));
            auto route_model=ControlSet::parse(routed);
            const auto* route=shape_route(route_model,"hips");
            expect(route && route->id=="hip_shape" && route->options.size()==2,"Shape route not found");
            expect(route->options[1].morph=="HipsClassic" && route->options[1].texture.empty(),"Shape route option lost its morph");
            expect(route->options[1].body_geometry_property=="CSSBodyGeometryClassicJson","Shape route lost its geometry model");
            expect(route->maximum==1 && route->step==1,"Shape route range must be its option list");
            expect(!shape_route(route_model,"cloth"),"An unrouted control has no route");
            std::map<std::string,ControlValue> picked;
            expect(choice_index(*route,picked)==0,"An unset route uses its default option");
            picked["hip_shape"]={1,0,0,1};
            expect(choice_index(*route,picked)==1,"A route follows the saved option");
            picked["hip_shape"]={7,0,0,1};
            expect(choice_index(*route,picked)==1,"A route clamps to its last option");
            // Old saved looks without the route load with the default shape and keep the slider.
            Customization old_look; old_look.values["hips"]={0.4f,0,0,1};
            const auto old_values=control_values(route_model,old_look);
            expect(choice_index(*route,old_values)==0 && old_values.at("hips")[0]==.4f,"Old looks must keep the default shape");
            auto bad_route=[&](auto change){ auto r=routed; change(r["controls"][2]); rejects([&]{ControlSet::parse(r);}); };
            bad_route([](Json& c){ c["shape_control"]="nobody"; });
            bad_route([](Json& c){ c["shape_control"]="cloth"; });
            bad_route([](Json& c){ c["options"][0]["morph"]="Waist"; });
            bad_route([](Json& c){ c["options"][1]["morph"]="Hips"; });
            bad_route([](Json& c){ c["options"][1]["morph"]="Hips classic"; });
            bad_route([](Json& c){ c["options"][1]["texture"]="/Game/CSS/x/T_A.T_A"; });
            bad_route([](Json& c){ c["options"][1].erase("body_geometry_property"); });
            bad_route([](Json& c){ c["options"][1]["body_geometry_property"]="Bad Name"; });
            bad_route([](Json& c){ c["kind"]="scalar"; });
            auto two_routes=routed; auto second_route=routed["controls"][2]; second_route["id"]="hip_shape_2";
            two_routes["controls"].push_back(second_route);
            rejects([&]{ControlSet::parse(two_routes);});
            auto texture_morph=routed; texture_morph["controls"][2].erase("shape_control");
            rejects([&]{ControlSet::parse(texture_morph);});
            auto no_geometry=routed;
            for(auto& option:no_geometry["controls"][2]["options"]) option.erase("body_geometry_property");
            expect(!ControlSet::parse(no_geometry).find("hip_shape")->options[0].body_geometry_property.size(),
                   "A route without geometry models is valid");
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
        {
            // apply_customize / restore_customization: the shared path behind Core::request and
            // the customize export used by CSSX extensions (CINE look track).
            auto recipe=Json::parse(R"({"schema":1,"controls":[
                {"id":"coat","name":"Coat","kind":"toggle","default":[1,0,0,1],
                 "bindings":[{"slot":3,"surface":"overlay","parameter":"CoatVisible"}]},
                {"id":"veil","name":"Veil","kind":"opacity","default":[1,0,0,1],
                 "bindings":[{"slot":4,"surface":"overlay","parameter":"VeilOpacity"}]},
                {"id":"cloth","name":"Cloth","kind":"color","default":[1,1,1,1],
                 "bindings":[{"slot":5,"parameter":"ClothColor"}]}],
                "palettes":[{"id":"crimson","name":"Crimson","values":{"cloth":[0.6,0.1,0.2,1]}}]})");
            const auto model=ControlSet::parse(recipe);
            Customization user; user.values["coat"]={0,0,0,1}; user.values["veil"]={.5f,0,0,1};
            // A palette only clears the controls it sets; the user's coat and veil survive.
            auto red=apply_customize(model,user,{{"action","palette"},{"palette","crimson"}});
            expect(red.palette=="crimson" && red.values.at("coat")[0]==0 && red.values.at("veil")[0]==.5f,
                   "A palette must keep the values it does not set");
            // Original clears every value: the reset players see, which callers must avoid mid-shot.
            expect(apply_customize(model,user,{{"action","palette"},{"palette","original"}}).values.empty(),
                   "Original must clear all custom values");
            rejects([&]{apply_customize(model,user,{{"action","palette"},{"palette","missing"}});});
            // One channel per command; a whole array is not a value.
            auto shown=apply_customize(model,user,{{"action","control"},{"control","coat"},{"channel",0},{"value",1.0}});
            expect(shown.values.at("coat")[0]==1 && shown.values.at("veil")[0]==.5f,"A control change must touch only its control");
            rejects([&]{apply_customize(model,user,{{"action","control"},{"control","coat"},{"value",Json::array({1,0,0,1})}});});
            rejects([&]{apply_customize(model,user,{{"action","control"},{"control","coat"},{"channel",0},{"value",0.5}});});   // fractional toggle
            rejects([&]{apply_customize(model,user,{{"action","control"},{"control","nope"},{"channel",0},{"value",1.0}});});
            rejects([&]{apply_customize(model,user,{{"action","control"},{"control","coat"},{"channel",7},{"value",1.0}});});
            // A swatch sets three channels at once and clamps; scalars refuse it.
            auto white=apply_customize(model,user,{{"action","control"},{"control","cloth"},{"rgb",Json::array({1.0,1.0,1.0})}});
            expect(white.values.at("cloth")[0]==1 && white.values.at("cloth")[3]==1,"A swatch must set the colour and keep alpha");
            rejects([&]{apply_customize(model,user,{{"action","control"},{"control","veil"},{"rgb",Json::array({1,1,1})}});});
            rejects([&]{apply_customize(model,user,{{"action","control"},{"control","cloth"},{"rgb",Json::array({1,1})}});});
            // Reset and the 0.4 alias names behave like the request path.
            expect(!apply_customize(model,user,{{"action","reset_control"},{"control","coat"}}).values.contains("coat"),
                   "reset_control must drop the override");
            expect(!apply_customize(model,user,{{"action","reset_color"},{"control","veil"}}).values.contains("veil"),
                   "reset_color must stay an alias of reset_control");
            rejects([&]{apply_customize(model,user,{{"action","tint"},{"group","outfit"}});});
            // Restoring a snapshot gives back the exact look, minus parts this outfit lacks.
            auto snapshot=red.json();
            expect(restore_customization(model,snapshot)==red,"A snapshot must restore the same customization");
            snapshot["values"]["gone"]=Json::array({1,0,0,1});
            expect(!restore_customization(model,snapshot).values.contains("gone"),"Unknown parts must be dropped on restore");
            // The input is never modified (callers keep their snapshot).
            expect(user.values.at("coat")[0]==0 && user.palette=="original","apply_customize must not mutate its input");
        }
        std::cout<<checks<<" control behavior checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}

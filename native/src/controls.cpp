#include "controls.hpp"
#include "data.hpp"
#include <cctype>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace css {
namespace {
// 0.4: packages published before the colour convention carry no group or role, so they are
// read off the control id. Compatibility only; docs/control-convention.md asks a package to
// declare all three. Order matters: the first match wins, so "eye-glow" is tested before
// "eyes" and before the bare "glow".
struct RoleGuess { const char* needle; const char* role; ControlGroup group; bool hue_locked; };
constexpr RoleGuess ROLE_GUESSES[] = {
    {"eye-glow", "eye-glow", ControlGroup::Body, false},
    {"areola", "areola", ControlGroup::Body, true},
    {"nipple", "nipple", ControlGroup::Body, true},
    {"labia", "labia", ControlGroup::Body, true},
    {"vestibule", "vestibule", ControlGroup::Body, true},
    {"clitoris", "clitoris", ControlGroup::Body, true},
    {"clit", "clitoris", ControlGroup::Body, true},
    {"vulva", "labia", ControlGroup::Body, true},
    {"vagina", "orifice", ControlGroup::Body, true},
    {"orifice", "orifice", ControlGroup::Body, true},
    {"anus", "orifice", ControlGroup::Body, true},
    {"breast", "breast", ControlGroup::Body, false},
    {"boob", "breast", ControlGroup::Body, false},
    {"bust", "breast", ControlGroup::Body, false},
    {"glute", "butt", ControlGroup::Body, false},
    {"butt", "butt", ControlGroup::Body, false},
    {"thigh", "thigh", ControlGroup::Body, false},
    {"hip", "thigh", ControlGroup::Body, false},
    {"waist", "waist", ControlGroup::Body, false},
    {"belly", "waist", ControlGroup::Body, false},
    {"pubic", "body-hair", ControlGroup::Body, false},
    {"body-hair", "body-hair", ControlGroup::Body, false},
    {"eye-intensity", "eye-glow", ControlGroup::Body, false},
    {"eye", "eyes", ControlGroup::Body, false},
    {"skin", "skin", ControlGroup::Body, true},
    {"face", "face", ControlGroup::Body, false},
    {"mask", "face", ControlGroup::Body, false},
    {"ponytail", "hair", ControlGroup::Body, false},
    {"bangs", "hair", ControlGroup::Body, false},
    {"braid", "hair", ControlGroup::Body, false},
    {"hair", "hair", ControlGroup::Body, false},
    {"metal", "metal", ControlGroup::Outfit, true},
    {"gem", "gem", ControlGroup::Outfit, true},
    {"jewel", "gem", ControlGroup::Outfit, true},
    {"crystal", "gem", ControlGroup::Outfit, true},
    {"necklace", "jewelry", ControlGroup::Outfit, true},
    {"choker", "jewelry", ControlGroup::Outfit, true},
    {"earring", "jewelry", ControlGroup::Outfit, true},
    {"jewelry", "jewelry", ControlGroup::Outfit, true},
    {"headwear", "headwear", ControlGroup::Outfit, false},
    {"crown", "headwear", ControlGroup::Outfit, true},
    {"hairpin", "headwear", ControlGroup::Outfit, true},
    {"tiara", "headwear", ControlGroup::Outfit, true},
    {"cape", "fabric", ControlGroup::Outfit, false},
    {"cloak", "fabric", ControlGroup::Outfit, false},
    {"scarf", "fabric", ControlGroup::Outfit, false},
    {"skirt", "fabric", ControlGroup::Outfit, false},
    {"sash", "fabric", ControlGroup::Outfit, false},
    {"trim", "accent", ControlGroup::Outfit, false},
    {"ribbon", "accent", ControlGroup::Outfit, false},
    {"accent", "accent", ControlGroup::Outfit, false},
    {"lining", "accent", ControlGroup::Outfit, false},
    {"leather", "leather", ControlGroup::Outfit, false},
    {"strap", "leather", ControlGroup::Outfit, false},
    {"glow", "glow", ControlGroup::Outfit, false},
    {"sheer", "fabric", ControlGroup::Outfit, false},
    {"lace", "fabric", ControlGroup::Outfit, false},
};
RoleGuess guess_role(const std::string& id) {
    for(const auto& guess:ROLE_GUESSES) if(id.find(guess.needle)!=std::string::npos) return guess;
    return {"", "garment", ControlGroup::Outfit, false};
}
// Defaults for a declared role, so a package only spells out hue_locked when it disagrees.
bool role_hue_locked(const std::string& role) {
    // The intimate pigments are a shade of the body rather than a colour of their own, so
    // they sit with skin: a body hue shift must not leave them behind, and a player who
    // wants them pinker or darker sets that one control.
    return role=="metal" || role=="gem" || role=="skin" ||
           role=="nipple" || role=="areola" || role=="labia" || role=="vestibule" ||
           role=="clitoris" || role=="orifice" || role=="jewelry";
}
ControlGroup role_group(const std::string& role) {
    return (role=="skin" || role=="face" || role=="hair" || role=="eyes" || role=="eye-glow" ||
            role=="nipple" || role=="areola" || role=="labia" || role=="vestibule" || role=="clitoris" ||
            role=="orifice" || role=="body-hair" || role=="breast" || role=="butt" || role=="thigh" ||
            role=="waist" || role=="figure")
        ? ControlGroup::Body : ControlGroup::Outfit;
}
ControlValue value(const Json& j) {
    if(!j.is_array() || j.size()!=4) throw std::runtime_error("A value needs four components");
    auto v=j.get<ControlValue>();
    for(float x:v) if(!std::isfinite(x) || x<0 || x>32) throw std::runtime_error("Value component outside supported range");
    if(v[3]>1) throw std::runtime_error("Opacity outside supported range");
    return v;
}
void parameter(const std::string& name) {
    if(name.empty() || name.size()>128 || !std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32 && c<=126;}))
        throw std::runtime_error("Invalid material parameter name");
}
void slot(int n) { if(n<0 || n>=128) throw std::runtime_error("Material slot outside range"); }
// A morph target name as CSSImportMesh cooked it. The importer refuses anything the
// engine would have had to rename, so a name here is a name the runtime can address.
void morph_name(const std::string& name) {
    if(name.empty() || name.size()>64 ||
       !std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c) || c=='_';}))
        throw std::runtime_error("Invalid morph target name");
}
// A skeleton bone name as the cooked mesh spells it: brust001, thigh_twist_02_l.
void bone(const std::string& name) {
    if(name.empty() || name.size()>64 ||
       !std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c) || c=='_';}))
        throw std::runtime_error("Invalid spring bone name");
}
// A slider range an author declares as {"min":..,"max":..,"default":..}. The default
// comes back separately because it lands in the control's value rather than its limits.
struct Range { float minimum=0, maximum=1, value=0; };
Range range(const Json& j,const char* what,float ceiling) {
    if(!j.is_object()) throw std::runtime_error(std::string("A ")+what+" range needs min, max and default");
    Range out{j.at("min").get<float>(),j.at("max").get<float>(),j.at("default").get<float>()};
    if(!std::isfinite(out.minimum) || !std::isfinite(out.maximum) || !std::isfinite(out.value) ||
       out.minimum<=0 || out.maximum>ceiling || out.minimum>=out.maximum ||
       out.value<out.minimum || out.value>out.maximum)
        throw std::runtime_error(std::string("Invalid spring ")+what+" range");
    return out;
}
void valid_value(const Control& c,const ControlValue& v) {
    if(c.kind==ControlKind::Spring) {
        // Two numbers, two ranges: channel 0 is frequency, channel 1 is damping ratio.
        if(!std::isfinite(v[0]) || v[0]<c.minimum || v[0]>c.maximum)
            throw std::runtime_error("Spring frequency outside control limits: "+c.id);
        if(!std::isfinite(v[1]) || v[1]<c.damping_minimum || v[1]>c.damping_maximum)
            throw std::runtime_error("Spring damping outside control limits: "+c.id);
        if(c.spring_clamp && (!std::isfinite(v[2]) || v[2]<c.displacement_minimum || v[2]>c.displacement_maximum))
            throw std::runtime_error("Spring travel outside control limits: "+c.id);
        if(v[3]!=c.value[3]) throw std::runtime_error("Opacity is fixed by its author");
        return;
    }
    for(size_t i=0;i<(c.scalar?1u:3u);++i)
        if(!std::isfinite(v[i]) || v[i]<c.minimum || v[i]>c.maximum) throw std::runtime_error("Value outside control limits: "+c.id);
    if(v[3]!=c.value[3]) throw std::runtime_error("Opacity is fixed by its author");
}
}
bool dye_resource(const std::string& name) {
    return name.starts_with("dye-") && name.ends_with(".png") && valid_id(name) && name.find("..") == name.npos;
}
float srgb_linear(float v) { return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f); }
const Control* ControlSet::find(const std::string& id) const {
    for(const auto& control:controls) if(control.id==id) return &control;
    return nullptr;
}
ControlSet ControlSet::parse(const Json& j) {
    ControlSet out;
    if(j.is_null() || j.empty()) return out;
    if(!j.is_object() || j.at("schema")!=1) throw std::runtime_error("Unsupported customize configuration");
    for(const auto* name:{"controls","surfaces","palettes"})
        if(j.contains(name) && !j.at(name).is_array()) throw std::runtime_error("Control definitions require arrays");
    for(const auto& c:j.at("controls")) {
        Control control;
        control.id=c.at("id"); control.name=c.at("name");
        if(!valid_id(control.id) || out.find(control.id) || control.name.empty() || control.name.size()>96 || out.controls.size()>=32)
            throw std::runtime_error("Invalid control identity");
        // `type` is what packages before 0.4 wrote, `kind` is the convention's name and
        // wins when both are present. A `type` of "scalar" predates the split between a
        // strength and an ordinary material scalar, so it lands on Intensity, which is
        // what those packages meant.
        auto type=c.value("type",std::string("color"));
        if(type=="color") control.kind=ControlKind::Color;
        else if(type=="scalar" || type=="intensity") control.kind=ControlKind::Intensity;
        else throw std::runtime_error("Unsupported control type");
        if(c.contains("kind")) {
            auto kind=c.at("kind").get<std::string>();
            if(kind=="color") control.kind=ControlKind::Color;
            else if(kind=="intensity") control.kind=ControlKind::Intensity;
            else if(kind=="scalar") control.kind=ControlKind::Scalar;
            else if(kind=="toggle") control.kind=ControlKind::Toggle;
            else if(kind=="choice") control.kind=ControlKind::Choice;
            else if(kind=="spring") control.kind=ControlKind::Spring;
            else if(kind=="shape") control.kind=ControlKind::Shape;
            else if(kind=="glow") control.kind=ControlKind::Glow;
            else if(kind=="opacity") control.kind=ControlKind::Opacity;
            else throw std::runtime_error("Unsupported control kind");
        }
        control.scalar=control.kind!=ControlKind::Color;
        if(control.kind==ControlKind::Glow) {
            control.pulse_hz=c.value("pulse_hz",0.f);
            control.combat_reactive=c.value("combat_reactive",false);
        }
        // A spring says what it wants inside its two ranges, so it is the one kind that
        // does not also write `default`: two places to state the same number is one too many.
        if(control.kind==ControlKind::Spring) {
            if(c.contains("default")) throw std::runtime_error("A spring control takes its default from its frequency and damping ratio");
            for(const auto* key:{"min","max","step"})
                if(c.contains(key)) throw std::runtime_error("A spring control takes its limits from its frequency and damping ratio");
        } else control.value=value(c.at("default"));
        // Group, role and hue locking: declared if present, otherwise read off the id.
        const auto fallback=guess_role(control.id);
        control.role=c.value("role",std::string(fallback.role));
        if(control.role.empty() || control.role.size()>32 || !valid_id(control.role))
            throw std::runtime_error("Invalid control role");
        if(c.contains("group")) {
            auto group=c.at("group").get<std::string>();
            if(group!="outfit" && group!="body") throw std::runtime_error("Unsupported control group");
            control.group=group=="body"?ControlGroup::Body:ControlGroup::Outfit;
        } else control.group=c.contains("role")?role_group(control.role):fallback.group;
        control.hue_locked=c.value("hue_locked",
            c.contains("role")?role_hue_locked(control.role):fallback.hue_locked);
        control.minimum=c.value("min",0.f);
        control.maximum=c.value("max",control.kind==ControlKind::Glow?32.f:1.f);
        control.step=c.value("step",.01f);
        // A toggle is on or off. It has no range to declare, so it is given one rather
        // than letting a package invent a half-hidden section.
        if(control.kind==ControlKind::Toggle) { control.minimum=0; control.maximum=1; control.step=1; }
        else if(!std::isfinite(control.minimum) || !std::isfinite(control.maximum) || !std::isfinite(control.step) ||
           control.minimum<0 || control.maximum>32 || control.minimum>=control.maximum || control.step<=0 || control.step>control.maximum-control.minimum)
            throw std::runtime_error("Invalid slider range");
        if(c.contains("sections")) {
            if(control.kind!=ControlKind::Toggle) throw std::runtime_error("Only a toggle control hides material sections");
            control.sections=c.at("sections").get<std::vector<int>>();
            if(control.sections.empty() || control.sections.size()>128) throw std::runtime_error("Invalid toggle sections");
            for(int index:control.sections) slot(index);
        } else if(control.kind==ControlKind::Toggle) throw std::runtime_error("A toggle control needs the sections it hides");
        if(c.contains("options")) {
            if(control.kind!=ControlKind::Choice) throw std::runtime_error("Only a choice control lists texture options");
            if(!c.at("options").is_array()) throw std::runtime_error("Choice options require an array");
            for(const auto& o:c.at("options")) {
                ControlOption option{o.at("name"),o.at("texture")};
                if(option.name.empty() || option.name.size()>96 || !valid_asset(option.texture))
                    throw std::runtime_error("Invalid choice option");
                control.options.push_back(std::move(option));
            }
            if(control.options.size()<2 || control.options.size()>16)
                throw std::runtime_error("A choice control needs between two and sixteen options");
            // The value is which option, so the range is the list and nothing else.
            control.minimum=0; control.maximum=float(control.options.size()-1); control.step=1;
        } else if(control.kind==ControlKind::Choice) throw std::runtime_error("A choice control needs its texture options");
        if(c.contains("nodes") || c.contains("frequency") || c.contains("damping_ratio")) {
            if(control.kind!=ControlKind::Spring) throw std::runtime_error("Only a spring control tunes skeleton nodes");
            control.nodes=c.at("nodes").get<std::vector<std::string>>();
            if(control.nodes.empty() || control.nodes.size()>32) throw std::runtime_error("A spring control needs between one and thirty-two bones");
            std::set<std::string> seen;
            for(const auto& name:control.nodes) {
                bone(name);
                if(!seen.insert(name).second) throw std::runtime_error("A spring control names the same bone twice");
            }
            // 8 Hz is far past anything a body part does, and the slowest useful wobble is
            // well above a tenth of a hertz, so a typo lands outside rather than shipping.
            const auto frequency=range(c.at("frequency"),"frequency",8.f);
            const auto damping=range(c.at("damping_ratio"),"damping ratio",2.f);
            // The engine scales damping down above 1/FixedTimeStep instead of using what it
            // was given, so a range whose corner crosses that line would stop meaning what
            // the slider says. Refuse it here rather than silently disagreeing in game.
            if(spring_tuning(frequency.maximum,damping.maximum).damping>100)
                throw std::runtime_error("Spring range is too stiff and damped for the engine to integrate as written");
            control.minimum=frequency.minimum; control.maximum=frequency.maximum; control.step=.05f;
            control.damping_minimum=damping.minimum; control.damping_maximum=damping.maximum; control.damping_step=.01f;
            // Optional travel clamp on channel 2. With it the spring limits how far the
            // part moves (MaxDisplacement, cm) and turns on bLimitDisplacement in game,
            // which is the whole reason a lively low-damped bounce does not fly off.
            if(c.contains("max_displacement")) {
                const auto travel=range(c.at("max_displacement"),"travel",16.f);
                control.spring_clamp=true;
                control.displacement_minimum=travel.minimum; control.displacement_maximum=travel.maximum; control.displacement_step=.05f;
                control.value={frequency.value,damping.value,travel.value,control.value[3]};
            } else control.value={frequency.value,damping.value,0,control.value[3]};
            // Optional per-axis filters and reset threshold, written on wear and put back
            // on removal. Absent leaves the blueprint's own node alone.
            auto axis=[&](const char* key,std::array<std::int8_t,3>& out){
                if(!c.contains(key)) return;
                const auto& a=c.at(key);
                if(!a.is_array() || a.size()!=3) throw std::runtime_error(std::string("A spring's ")+key+" needs three true or false values");
                for(int i=0;i<3;++i) out[size_t(i)]=a[i].get<bool>()?1:0;
            };
            axis("translate",control.translate); axis("rotate",control.rotate);
            if(c.contains("error_reset")) {
                control.error_reset=c.at("error_reset").get<double>();
                if(!std::isfinite(control.error_reset) || control.error_reset<=0 || control.error_reset>4096)
                    throw std::runtime_error("A spring's error_reset is out of range");
            }
            if(c.contains("planar_constraint")) {
                auto pc=c.at("planar_constraint").get<std::string>();
                if(pc=="none") control.planar_constraint=0;
                else if(pc=="x") control.planar_constraint=1;
                else if(pc=="y") control.planar_constraint=2;
                else if(pc=="z") control.planar_constraint=3;
                else throw std::runtime_error("Invalid planar constraint axis");
            }
            if(c.contains("world_damping")) {
                control.world_damping=c.at("world_damping").get<float>();
                if(!std::isfinite(control.world_damping) || control.world_damping<0 || control.world_damping>1)
                    throw std::runtime_error("Invalid world damping value");
            }
            if(c.contains("limit_angle")) {
                control.limit_angle=c.at("limit_angle").get<float>();
                if(!std::isfinite(control.limit_angle) || control.limit_angle<0 || control.limit_angle>180)
                    throw std::runtime_error("Invalid limit angle");
            }
            if(c.contains("collision_radius")) {
                control.collision_radius=c.at("collision_radius").get<float>();
                if(!std::isfinite(control.collision_radius) || control.collision_radius<0 || control.collision_radius>100)
                    throw std::runtime_error("Invalid collision radius");
            }
            if(c.contains("gravity_scale")) {
                control.gravity_scale=c.at("gravity_scale").get<float>();
                if(!std::isfinite(control.gravity_scale) || control.gravity_scale<-5 || control.gravity_scale>5)
                    throw std::runtime_error("Invalid gravity scale");
            }
        } else if(control.kind==ControlKind::Spring)
            throw std::runtime_error("A spring control needs its bones, frequency and damping ratio");
        if(c.contains("morph")) {
            if(control.kind!=ControlKind::Shape) throw std::runtime_error("Only a shape control drives a morph target");
            control.morph=c.at("morph").get<std::string>();
            morph_name(control.morph);
        } else if(control.kind==ControlKind::Shape)
            throw std::runtime_error("A shape control needs the morph target it drives");
        if(c.contains("formulas")) {
            if(control.kind!=ControlKind::Shape) throw std::runtime_error("Only a shape control carries morph formulas");
            if(!c.at("formulas").is_array()) throw std::runtime_error("Morph formulas require an array");
            for(const auto& f:c.at("formulas")) {
                MorphFormula formula;
                formula.target=f.at("target").get<std::string>();
                formula.type=f.at("type").get<std::string>();
                formula.multiplier=f.at("multiplier").get<double>();
                bone(formula.target);
                if(formula.type!="BoneCenterX" && formula.type!="BoneCenterY" && formula.type!="BoneCenterZ" &&
                   formula.type!="OrientationX" && formula.type!="OrientationY" && formula.type!="OrientationZ")
                    throw std::runtime_error("Invalid morph formula target type");
                control.formulas.push_back(std::move(formula));
            }
        }
        valid_value(control,control.value);
        if(c.contains("bindings") && !c.at("bindings").is_array()) throw std::runtime_error("Bindings require an array");
        for(const auto& b:c.value("bindings",Json::array())) {
            ControlBinding binding; binding.slot=b.at("slot"); binding.parameter=b.at("parameter");
            slot(binding.slot); parameter(binding.parameter);
            auto association=b.value("association",std::string("global"));
            if(association!="global" && association!="layer" && association!="blend") throw std::runtime_error("Invalid parameter association");
            binding.association=association=="layer"?0:association=="blend"?1:2;
            binding.layer=b.value("layer",-1);
            if((binding.association==2 && binding.layer!=-1) || (binding.association!=2 && (binding.layer<0 || binding.layer>63)))
                throw std::runtime_error("Invalid parameter layer");
            if(control.bindings.size()>=128) throw std::runtime_error("Too many linked material bindings");
            control.bindings.push_back(binding);
        }
        out.controls.push_back(std::move(control));
    }
    std::set<std::string> ids, destinations;
    for(const auto& s:j.value("surfaces",Json::array())) {
        DyeSurface surface; surface.id=s.at("id"); surface.parameter=s.at("parameter");
        if(!valid_id(surface.id) || !ids.insert(surface.id).second || out.surfaces.size()>=16) throw std::runtime_error("Invalid dye surface");
        parameter(surface.parameter); surface.slots=s.at("slots").get<std::vector<int>>();
        surface.resolution=s.value("resolution",2048);
        if(surface.slots.empty() || surface.slots.size()>128 || (surface.resolution!=1024 && surface.resolution!=2048 && surface.resolution!=4096))
            throw std::runtime_error("Invalid dye surface dimensions or slots");
        for(int index:surface.slots) {
            slot(index);
            if(!destinations.insert(std::to_string(index)+"/"+surface.parameter).second) throw std::runtime_error("Overlapping dye surfaces");
        }
        surface.layers=s.at("layers").get<std::map<std::string,std::string>>();
        if(surface.layers.empty() || surface.layers.size()>16) throw std::runtime_error("Invalid dye layers");
        for(const auto& [id,file]:surface.layers) {
            auto* c=out.find(id);
            if(!c || c->scalar || !dye_resource(file)) throw std::runtime_error("Invalid dye layer control or file");
        }
        out.surfaces.push_back(std::move(surface));
    }
    for(const auto& c:out.controls) {
        // A toggle drives sections directly, so it needs no parameter to write into.
        // A choice does need one, and its bindings are checked with everything else.
        bool used=!c.bindings.empty() || !c.sections.empty() || !c.nodes.empty() || !c.morph.empty() || c.kind==ControlKind::Glow;
        for(const auto& s:out.surfaces) used|=s.layers.contains(c.id);
        if(!used) throw std::runtime_error("Control has nothing to drive");
    }
    ids.clear();
    for(const auto& p:j.value("palettes",Json::array())) {
        Palette palette; palette.id=p.at("id"); palette.name=p.at("name");
        if(!valid_id(palette.id) || palette.id=="original" || !ids.insert(palette.id).second || palette.name.empty() || palette.name.size()>96 || out.palettes.size()>=64)
            throw std::runtime_error("Invalid palette");
        if(!p.at("values").is_object()) throw std::runtime_error("Palette values require an object");
        for(const auto& [id,v]:p.at("values").items()) {
            auto* c=out.find(id); if(!c) throw std::runtime_error("Palette references an unknown control");
            auto color=value(v); valid_value(*c,color); palette.values[id]=color;
        }
        out.palettes.push_back(std::move(palette));
    }
    return out;
}
Customization Customization::parse(const Json& j) {
    Customization result;
    if(!j.is_object()) throw std::runtime_error("Invalid saved settings");
    result.palette=j.value("palette",std::string("original"));
    if(!valid_id(result.palette)) throw std::runtime_error("Invalid saved palette");
    auto values=j.value("values",Json::object());
    if(!values.is_object() || values.size()>32) throw std::runtime_error("Invalid saved setting count");
    for(const auto& [id,v]:values.items()) {
        if(!valid_id(id)) throw std::runtime_error("Invalid saved control");
        result.values[id]=value(v);
    }
    auto tints=j.value("tints",Json::object());
    if(!tints.is_object() || tints.size()>4) throw std::runtime_error("Invalid saved tint count");
    for(const auto& [group,t]:tints.items()) {
        if(group!="outfit" && group!="body") throw std::runtime_error("Invalid saved tint group");
        if(!t.is_object()) throw std::runtime_error("Invalid saved tint");
        ColorTint tint;
        tint.hue=t.value("hue",0.f);
        tint.saturation=t.value("saturation",1.f);
        tint.brightness=t.value("brightness",1.f);
        if(!std::isfinite(tint.hue) || !std::isfinite(tint.saturation) || !std::isfinite(tint.brightness) ||
           tint.hue<-180 || tint.hue>180 || tint.saturation<0 || tint.saturation>2 ||
           tint.brightness<0 || tint.brightness>2)
            throw std::runtime_error("Saved tint outside supported range");
        if(!tint.neutral()) result.tints[group]=tint;
    }
    return result;
}
Json Customization::json() const {
    Json out={{"palette",palette},{"values",values}};
    if(!tints.empty()) {
        Json saved=Json::object();
        for(const auto& [group,tint]:tints)
            saved[group]={{"hue",tint.hue},{"saturation",tint.saturation},{"brightness",tint.brightness}};
        out["tints"]=std::move(saved);
    }
    return out;
}
Customization compatible_values(const ControlSet& options,const Customization& source) {
    Customization result;
    if(std::any_of(options.palettes.begin(),options.palettes.end(),[&](const auto& p){return p.id==source.palette;})) result.palette=source.palette;
    for(const auto& [id,value]:source.values) if(auto* control=options.find(id)) {
        try { valid_value(*control,value); result.values[id]=value; }
        catch(const std::exception&) { /* A changed part keeps its new authored default. */ }
    }
    // A tint is a plain adjustment, so it carries over to any outfit that still has parts
    // in that group.
    for(const auto& [group,tint]:source.tints)
        if(std::any_of(options.controls.begin(),options.controls.end(),
                       [&](const auto& c){return !c.scalar && control_group_name(c.group)==group;}))
            result.tints[group]=tint;
    return result;
}
const char* control_kind_name(ControlKind kind) {
    switch(kind) {
        case ControlKind::Color: return "color";
        case ControlKind::Intensity: return "intensity";
        case ControlKind::Scalar: return "scalar";
        case ControlKind::Toggle: return "toggle";
        case ControlKind::Choice: return "choice";
        case ControlKind::Spring: return "spring";
        case ControlKind::Shape: return "shape";
        case ControlKind::Glow: return "glow";
        case ControlKind::Opacity: return "opacity";
    }
    return "color";
}
SpringTuning spring_tuning(float frequency,float damping_ratio) {
    const double w=2*3.14159265358979323846*double(frequency);
    return {w*w,2*double(damping_ratio)*w};
}
const char* control_group_name(ControlGroup group) { return group==ControlGroup::Body?"body":"outfit"; }
// A palette is a look, not a reset. It takes over the parts it sets and the tint of the
// groups those parts are in, and leaves everything else alone, so a custom skin survives
// changing the dress. `original` is the exception: it means no dye at all, and nothing
// may survive it. See docs/control-convention.md, rule 1.
Customization choose_palette(const ControlSet& options,const Customization& current,const std::string& palette) {
    Customization result=current;
    result.palette=palette;
    if(palette=="original") { result.values.clear(); result.tints.clear(); return result; }
    auto found=std::find_if(options.palettes.begin(),options.palettes.end(),
                            [&](const auto& p){return p.id==palette;});
    if(found==options.palettes.end()) throw std::runtime_error("That palette is not installed");
    std::set<std::string> groups;
    for(const auto& [id,value]:found->values) if(auto* control=options.find(id)) {
        result.values.erase(id);
        groups.insert(control_group_name(control->group));
    }
    for(const auto& group:groups) result.tints.erase(group);
    return result;
}
// Hue rotation with saturation and brightness scaling, through HSV. A hue-locked control
// keeps its own hue and takes only the other two, which is what stops a group tint turning
// gold green and skin blue. Alpha is author-controlled and never touched.
ControlValue apply_tint(const ColorTint& tint,const ControlValue& colour,bool hue_locked) {
    if(tint.neutral()) return colour;
    // Nothing to do, and worth short-circuiting: a round trip through HSV would otherwise
    // move the value by a rounding step for no reason.
    if(hue_locked && tint.saturation==1 && tint.brightness==1) return colour;
    const float r=colour[0],g=colour[1],b=colour[2];
    const float high=std::max({r,g,b}),low=std::min({r,g,b}),span=high-low;
    float hue=0;
    if(span>1e-6f) {
        if(high==r) hue=std::fmod((g-b)/span+6.f,6.f);
        else if(high==g) hue=(b-r)/span+2.f;
        else hue=(r-g)/span+4.f;
        hue*=60.f;
    }
    float saturation=high>1e-6f?span/high:0.f;
    float brightness=high;
    if(!hue_locked) hue=std::fmod(std::fmod(hue+tint.hue,360.f)+360.f,360.f);
    saturation=std::clamp(saturation*tint.saturation,0.f,1.f);
    brightness=std::clamp(brightness*tint.brightness,0.f,32.f);
    const float chroma=brightness*saturation;
    const float sector=hue/60.f;
    const float second=chroma*(1.f-std::abs(std::fmod(sector,2.f)-1.f));
    float out[3]{};
    switch(static_cast<int>(sector)%6) {
        case 0: out[0]=chroma; out[1]=second; break;
        case 1: out[0]=second; out[1]=chroma; break;
        case 2: out[1]=chroma; out[2]=second; break;
        case 3: out[1]=second; out[2]=chroma; break;
        case 4: out[0]=second; out[2]=chroma; break;
        default: out[0]=chroma; out[2]=second; break;
    }
    const float base=brightness-chroma;
    return {out[0]+base,out[1]+base,out[2]+base,colour[3]};
}
std::map<std::string,ControlValue> control_values(const ControlSet& options,const Customization& custom) {
    std::map<std::string,ControlValue> result;
    if(custom.palette!="original") {
        auto found=std::find_if(options.palettes.begin(),options.palettes.end(),[&](const auto& p){return p.id==custom.palette;});
        if(found==options.palettes.end()) throw std::runtime_error("Saved palette is not installed");
        result=found->values;
    }
    for(const auto& [id,value]:custom.values) result[id]=value;
    for(const auto& [id,value]:result) {
        auto* c=options.find(id); if(!c) throw std::runtime_error("Saved part is not installed");
        valid_value(*c,value);
    }
    // The group tint sits on top of the palette and of any per-part override, so the whole
    // pipeline is one line: tint(override or palette). On Original nothing is dyed at all,
    // so `result` is empty and there is nothing for a tint to move.
    for(auto& [id,value]:result) {
        auto* c=options.find(id);
        if(c->scalar) continue;
        const auto tint=custom.tints.find(control_group_name(c->group));
        if(tint==custom.tints.end()) continue;
        value=apply_tint(tint->second,value,c->hue_locked);
        valid_value(*c,value);
    }
    return result;
}
}

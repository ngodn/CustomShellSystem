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
ControlValue value(const Json& j,bool stored=false) {
    if(!j.is_array() || j.size()!=4) throw std::runtime_error("A value needs four components");
    for(const auto& x:j) if(!x.is_number()) throw std::runtime_error("A value needs numeric components");
    auto v=j.get<ControlValue>();
    // A saved snapshot has no control schema yet. Bound it here, then enforce
    // each installed control's actual domain before applying or retaining it.
    for(float x:v) if(!std::isfinite(x) || x<(stored?-5:0) || x>(stored?1000:32))
        throw std::runtime_error("Value component outside supported range");
    if(v[3]<0 || v[3]>1) throw std::runtime_error("Opacity outside supported range");
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
    if(c.kind==ControlKind::Rig) {
        if(!c.rig) throw std::runtime_error("Missing rig control settings");
        for(size_t channel=0;channel<3;++channel) {
            const auto& range=c.rig->channels[channel];
            if(!std::isfinite(v[channel]) || v[channel]<range.minimum || v[channel]>range.maximum)
                throw std::runtime_error("Rig value outside control limits: "+c.id);
        }
        if(v[3]!=0 && v[3]!=1) throw std::runtime_error("Rig enabled value must be zero or one");
        return;
    }
    if(c.kind==ControlKind::Dynamics) {
        if(!c.dynamics) throw std::runtime_error("Missing dynamics control settings");
        for(size_t channel=0;channel<3;++channel) {
            const auto& range=c.dynamics->channels[channel];
            if(!std::isfinite(v[channel]) || v[channel]<range.minimum || v[channel]>range.maximum)
                throw std::runtime_error("Dynamics value outside control limits: "+c.id);
        }
        if(v[3]!=1) throw std::runtime_error("Dynamics reserved channel must be one");
        return;
    }
    for(float x:v) if(!std::isfinite(x) || x<0 || x>32)
        throw std::runtime_error("Value component outside supported range: "+c.id);
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
    if((c.kind==ControlKind::Toggle || c.kind==ControlKind::Choice) && std::floor(v[0])!=v[0])
        throw std::runtime_error("A toggle or choice needs a whole-number value: "+c.id);
    if(v[3]!=c.value[3]) throw std::runtime_error("Opacity is fixed by its author");
}
void valid_tint(const std::string& group,const ColorTint& tint) {
    if(group!="outfit" && group!="body") throw std::runtime_error("Invalid tint group");
    if(!std::isfinite(tint.hue) || !std::isfinite(tint.saturation) || !std::isfinite(tint.brightness) ||
       tint.hue<-180 || tint.hue>180 || tint.saturation<0 || tint.saturation>2 ||
       tint.brightness<0 || tint.brightness>2)
        throw std::runtime_error("Tint outside supported range");
}
}
bool dye_resource(const std::string& name) {
    return name.starts_with("dye-") && name.ends_with(".png") && valid_id(name) && name.find("..") == name.npos;
}
float srgb_linear(float v) { return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f); }
SpringAxes spring_axes(const Control& control, SpringAxes authored) {
    for(size_t axis=0;axis<3;++axis) {
        if(control.translate[axis]>=0) authored.translate[axis]=control.translate[axis]!=0;
        if(control.rotate[axis]>=0) authored.rotate[axis]=control.rotate[axis]!=0;
        if(control.planar_constraint==int(axis)+1) authored.translate[axis]=false;
    }
    return authored;
}
int control_channel_count(const Control& control) {
    if(control.kind==ControlKind::Rig) return 4;
    if(control.kind==ControlKind::Dynamics) return 3;
    if(control.kind==ControlKind::Spring) return control.spring_clamp?3:2;
    return control.scalar?1:3;
}
DynamicsSettings dynamics_settings(const Control& control,const ControlValue& value) {
    if(control.kind!=ControlKind::Dynamics) throw std::runtime_error("Expected a dynamics control");
    valid_value(control,value);
    return {value[0],value[1],value[1],value[2],value[0]>0,true,true,false};
}
RigSettings rig_settings(const Control& control,const ControlValue& value) {
    if(control.kind!=ControlKind::Rig || body_rig_control(control)) throw std::runtime_error("Expected a hair rig control");
    valid_value(control,value);
    return {value[0],value[1],{0,0,-980.*double(value[2])},value[3]==1};
}
bool body_rig_control(const Control& control) {
    return control.kind==ControlKind::Rig && control.rig && control.rig->body;
}
BodyRigSettings body_rig_settings(const std::vector<Control>& controls,const std::map<std::string,ControlValue>& values,
                                  const BodyRigSettings& authored) {
    auto result=authored;
    bool active=false;
    for(const auto& control:controls) if(body_rig_control(control) && values.contains(control.id)) {
        const auto& value=values.at(control.id);
        valid_value(control,value);
        if(!active && !authored.use_regions) {
            result.frequency.fill(authored.global_frequency);
            result.damping.fill(authored.global_damping);
            result.motion.fill(authored.global_motion);
            result.enabled.fill(true);
        }
        active=true;
        for(auto region:control.rig->regions) {
            if(region>=body_region_names.size()) throw std::runtime_error("Invalid body region index");
            result.frequency[region]=value[0]; result.damping[region]=value[1];
            result.motion[region]=value[2]; result.enabled[region]=value[3]==1;
        }
    }
    if(active) result.use_regions=true;
    return result;
}
bool dynamics_reset_required(const DynamicsSettings& before,const DynamicsSettings& after) {
    // AnimDynamics refreshes spring forcing and gravity scale each update, but
    // copies damping and the gravity-override mode into bodies at initialization.
    return before.linear_damping!=after.linear_damping || before.angular_damping!=after.angular_damping ||
        before.override_linear!=after.override_linear || before.override_angular!=after.override_angular ||
        before.gravity_override!=after.gravity_override;
}
SliderRange control_channel(const Control& control,int channel) {
    if(channel<0 || channel>=control_channel_count(control)) throw std::runtime_error("Invalid control channel");
    if(control.kind==ControlKind::Rig) {
        if(!control.rig) throw std::runtime_error("Missing rig control settings");
        if(channel==3) return {0,1,control.value[3],1};
        return control.rig->channels[size_t(channel)];
    }
    if(control.kind==ControlKind::Dynamics) {
        if(!control.dynamics) throw std::runtime_error("Missing dynamics control settings");
        return control.dynamics->channels[size_t(channel)];
    }
    if(control.kind==ControlKind::Spring && channel==1)
        return {control.damping_minimum,control.damping_maximum,control.value[1],control.damping_step};
    if(control.kind==ControlKind::Spring && channel==2)
        return {control.displacement_minimum,control.displacement_maximum,control.value[2],control.displacement_step};
    return {control.minimum,control.maximum,control.value[size_t(channel)],control.step};
}
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
            else if(kind=="dynamics") control.kind=ControlKind::Dynamics;
            else if(kind=="rig") control.kind=ControlKind::Rig;
            else if(kind=="shape") control.kind=ControlKind::Shape;
            else if(kind=="glow") control.kind=ControlKind::Glow;
            else if(kind=="opacity") control.kind=ControlKind::Opacity;
            else throw std::runtime_error("Unsupported control kind");
        }
        control.scalar=control.kind!=ControlKind::Color;
        if(control.kind==ControlKind::Glow) {
            control.pulse_hz=c.value("pulse_hz",0.f);
            control.combat_reactive=c.value("combat_reactive",false);
            if(!std::isfinite(control.pulse_hz) || control.pulse_hz<0 || control.pulse_hz>10)
                throw std::runtime_error("Glow pulse frequency outside supported range");
        }
        // Motion controls declare their defaults and limits per channel.
        if(control.kind==ControlKind::Spring || control.kind==ControlKind::Dynamics || control.kind==ControlKind::Rig) {
            if(c.contains("default")) throw std::runtime_error("A motion control takes its defaults from its channel ranges");
            for(const auto* key:{"min","max","step"})
                if(c.contains(key)) throw std::runtime_error("A motion control takes its limits from its channel ranges");
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
            const auto& sections=c.at("sections");
            if(!sections.is_array() || sections.empty() || sections.size()>128) throw std::runtime_error("Invalid toggle sections");
            for(const auto& index:sections) {
                if(!index.is_number_integer() || index<0 || index>127) throw std::runtime_error("Invalid toggle section index");
                control.sections.push_back(index.get<int>());
            }
        } else if(control.kind==ControlKind::Toggle) throw std::runtime_error("A toggle control needs the sections it hides");
        if(c.contains("occludes_sections")) {
            if(control.kind!=ControlKind::Toggle) throw std::runtime_error("Only a toggle control occludes material sections");
            const auto& sections=c.at("occludes_sections");
            if(!sections.is_array() || sections.empty() || sections.size()>128) throw std::runtime_error("Invalid occluded sections");
            std::set<int> seen;
            for(const auto& index:sections) {
                if(!index.is_number_integer() || index<0 || index>127) throw std::runtime_error("Invalid occluded section index");
                const int section=index.get<int>();
                if(!seen.insert(section).second || std::find(control.sections.begin(),control.sections.end(),section)!=control.sections.end())
                    throw std::runtime_error("A toggle's occluded sections must be distinct from its own sections");
                control.occludes_sections.push_back(section);
            }
        }
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
        for(const auto* key:{"max_displacement","translate","rotate","error_reset","planar_constraint",
                             "world_damping","limit_angle","collision_radius","gravity_scale"})
            if(c.contains(key) && control.kind!=ControlKind::Spring)
                throw std::runtime_error(std::string("Only a spring control accepts ")+key);
        if(control.kind==ControlKind::Rig) {
            for(const auto* key:{"nodes","bindings","angular_spring"})
                if(c.contains(key)) throw std::runtime_error(std::string("Rig does not accept ")+key);
            RigControl settings;
            const auto solver=c.value("solver",std::string("positional_hair"));
            if(solver!="positional_hair" && solver!="angular_body") throw std::runtime_error("Unknown rig solver");
            settings.body=solver=="angular_body";
            if(settings.body) {
                const auto names=c.at("regions").get<std::vector<std::string>>();
                if(names.empty() || names.size()>7) throw std::runtime_error("Body rig needs one to seven regions");
                for(const auto& name:names) {
                    const auto found=std::find(body_region_names.begin(),body_region_names.end(),name);
                    if(found==body_region_names.end()) throw std::runtime_error("Unknown body region");
                    const auto index=uint8_t(found-body_region_names.begin());
                    if(std::find(settings.regions.begin(),settings.regions.end(),index)!=settings.regions.end())
                        throw std::runtime_error("Duplicate body region");
                    settings.regions.push_back(index);
                }
                for(const auto* key:{"stiffness","damping","gravity"})
                    if(c.contains(key)) throw std::runtime_error("Body rig uses frequency, damping ratio and motion amount");
            } else for(const auto* key:{"frequency","damping_ratio","motion_amount","regions"})
                if(c.contains(key)) throw std::runtime_error("Hair rig does not accept body settings");
            const char* names[]={settings.body?"frequency":"stiffness",settings.body?"damping_ratio":"damping",settings.body?"motion_amount":"gravity"};
            const float low[]={settings.body?.5f:1,settings.body?.1f:0,settings.body?0.f:-5},
                        high[]={settings.body?6.f:1000,settings.body?2.f:120,settings.body?1.f:5},
                        steps[]={settings.body?.1f:1,settings.body?.05f:1,.05f};
            for(size_t i=0;i<3;++i) {
                const auto& r=c.at(names[i]);
                if(!r.is_object() || r.size()!=3 || !r.contains("min") || !r.contains("max") || !r.contains("default"))
                    throw std::runtime_error("A rig range needs min, max and default");
                for(const auto* key:{"min","max","default"})
                    if(!r.at(key).is_number()) throw std::runtime_error("A rig range must be numeric");
                auto& target=settings.channels[i];
                target={r.at("min").get<float>(),r.at("max").get<float>(),r.at("default").get<float>(),steps[i]};
                if(!std::isfinite(target.minimum) || !std::isfinite(target.maximum) || !std::isfinite(target.value) ||
                   target.minimum<low[i] || target.maximum>high[i] || target.minimum>=target.maximum ||
                   target.value<target.minimum || target.value>target.maximum)
                    throw std::runtime_error("Rig range outside solver limits");
                target.step=std::min(target.step,target.maximum-target.minimum);
                control.value[i]=target.value;
            }
            if(c.contains("enabled") && !c.at("enabled").is_boolean()) throw std::runtime_error("Rig enabled default must be boolean");
            control.value[3]=c.value("enabled",true)?1.f:0.f;
            control.rig=settings;
        } else if(c.contains("stiffness") || c.contains("enabled") || c.contains("solver") || c.contains("regions") || c.contains("motion_amount"))
            throw std::runtime_error("Only rig controls accept stiffness and enabled");
        if(control.kind==ControlKind::Dynamics) {
            for(const auto* key:{"frequency","damping_ratio","bindings"})
                if(c.contains(key)) throw std::runtime_error(std::string("Dynamics does not accept ")+key);
            control.nodes=c.at("nodes").get<std::vector<std::string>>();
            if(control.nodes.empty() || control.nodes.size()>32) throw std::runtime_error("Dynamics needs one to thirty-two chain roots");
            std::set<std::string> seen;
            for(const auto& name:control.nodes) {
                bone(name);
                if(!seen.insert(name).second) throw std::runtime_error("Duplicate dynamics chain root");
            }
            DynamicsControl settings;
            const char* names[]={"angular_spring","damping","gravity"};
            const float low[]={0,.7f,-5},high[]={1000,1,5},steps[]={1,.01f,.05f};
            for(size_t i=0;i<3;++i) {
                const auto& r=c.at(names[i]);
                if(!r.is_object() || r.size()!=3 || !r.contains("min") || !r.contains("max") || !r.contains("default"))
                    throw std::runtime_error("A dynamics range needs min, max and default");
                for(const auto* key:{"min","max","default"})
                    if(!r.at(key).is_number()) throw std::runtime_error("A dynamics range must be numeric");
                auto& target=settings.channels[i];
                target={r.at("min").get<float>(),r.at("max").get<float>(),r.at("default").get<float>(),steps[i]};
                if(!std::isfinite(target.minimum) || !std::isfinite(target.maximum) || !std::isfinite(target.value) ||
                   target.minimum<low[i] || target.maximum>high[i] || target.minimum>=target.maximum ||
                   target.value<target.minimum || target.value>target.maximum)
                    throw std::runtime_error("Dynamics range outside solver limits");
                target.step=std::min(target.step,target.maximum-target.minimum);
                control.value[i]=target.value;
            }
            control.value[3]=1;
            control.dynamics=settings;
        } else if(control.kind!=ControlKind::Rig && (c.contains("angular_spring") || c.contains("damping") || c.contains("gravity")))
            throw std::runtime_error("Only dynamics controls accept solver ranges");
        if(control.kind!=ControlKind::Dynamics && !body_rig_control(control) && (c.contains("nodes") || c.contains("frequency") || c.contains("damping_ratio"))) {
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
            auto optional_number=[&](const char* key,float low,float high,std::optional<float>& target) {
                if(!c.contains(key)) return;
                if(!c.at(key).is_number()) throw std::runtime_error(std::string("Invalid numeric setting: ")+key);
                const auto number=c.at(key).get<float>();
                if(!std::isfinite(number) || number<low || number>high)
                    throw std::runtime_error(std::string("Setting outside limits: ")+key);
                target=number;
            };
            optional_number("world_damping",0,1,control.world_damping);
            optional_number("limit_angle",0,180,control.limit_angle);
            optional_number("collision_radius",0,100,control.collision_radius);
            optional_number("gravity_scale",-5,5,control.gravity_scale);
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
    std::set<std::string> dynamics_roots;
    bool rig_owner=false;
    std::set<uint8_t> body_owners;
    for(const auto& c:out.controls) {
        if(body_rig_control(c)) {
            for(auto region:c.rig->regions) if(!body_owners.insert(region).second)
                throw std::runtime_error("Body regions must have one control owner");
        } else if(c.kind==ControlKind::Rig) {
            if(rig_owner) throw std::runtime_error("The post-process rig needs one control owner");
            rig_owner=true;
        }
        if(c.kind==ControlKind::Dynamics) for(const auto& root:c.nodes)
            if(!dynamics_roots.insert(root).second) throw std::runtime_error("Dynamics chain roots must have one control owner");
        // A toggle drives sections directly, so it needs no parameter to write into.
        // A choice does need one, and its bindings are checked with everything else.
        bool used=c.kind==ControlKind::Rig || !c.bindings.empty() || !c.sections.empty() || !c.nodes.empty() || !c.morph.empty();
        for(const auto& s:out.surfaces) used|=s.layers.contains(c.id);
        if(!used) throw std::runtime_error("Control has nothing to drive");
    }
    for(const auto& c:out.controls) if(c.kind==ControlKind::Spring || c.kind==ControlKind::Dynamics)
        for(auto region:body_owners)
            if(std::find(c.nodes.begin(),c.nodes.end(),body_region_names[region])!=c.nodes.end())
                throw std::runtime_error("Body region cannot also be driven by a spring or AnimDynamics control");
    ids.clear();
    for(const auto& p:j.value("palettes",Json::array())) {
        Palette palette; palette.id=p.at("id"); palette.name=p.at("name");
        if(!valid_id(palette.id) || palette.id=="original" || !ids.insert(palette.id).second || palette.name.empty() || palette.name.size()>96 || out.palettes.size()>=64)
            throw std::runtime_error("Invalid palette");
        if(!p.at("values").is_object()) throw std::runtime_error("Palette values require an object");
        for(const auto& [id,v]:p.at("values").items()) {
            auto* c=out.find(id); if(!c) throw std::runtime_error("Palette references an unknown control");
            auto color=value(v,c->kind==ControlKind::Dynamics || c->kind==ControlKind::Rig); valid_value(*c,color); palette.values[id]=color;
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
        result.values[id]=value(v,true);
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
        valid_tint(group,tint);
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
        case ControlKind::Dynamics: return "dynamics";
        case ControlKind::Rig: return "rig";
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
std::set<int> hidden_control_sections(const ControlSet& options,const std::map<std::string,ControlValue>& values) {
    std::set<int> hidden;
    for(const auto& control:options.controls) if(control.kind==ControlKind::Toggle) {
        const auto found=values.find(control.id);
        const auto& value=found==values.end()?control.value:found->second;
        valid_value(control,value);
        const auto& sections=value[0]>=.5f?control.occludes_sections:control.sections;
        hidden.insert(sections.begin(),sections.end());
    }
    return hidden;
}
std::map<std::string,ControlValue> control_values(const ControlSet& options,const Customization& custom) {
    for(const auto& [group,tint]:custom.tints) valid_tint(group,tint);
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

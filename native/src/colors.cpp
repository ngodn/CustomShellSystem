#include "colors.hpp"
#include "data.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace css {
namespace {
// 0.4: packages published before the colour convention carry no group or role, so they are
// read off the control id. Compatibility only; docs/color-convention.md asks a package to
// declare all three. Order matters: the first match wins, so "eye-glow" is tested before
// "eyes" and before the bare "glow".
struct RoleGuess { const char* needle; const char* role; ColorGroup group; bool hue_locked; };
constexpr RoleGuess ROLE_GUESSES[] = {
    {"eye-glow", "eye-glow", ColorGroup::Body, false},
    {"areola", "areola", ColorGroup::Body, true},
    {"nipple", "nipple", ColorGroup::Body, true},
    {"labia", "labia", ColorGroup::Body, true},
    {"vestibule", "vestibule", ColorGroup::Body, true},
    {"pubic", "body-hair", ColorGroup::Body, false},
    {"body-hair", "body-hair", ColorGroup::Body, false},
    {"eye-intensity", "eye-glow", ColorGroup::Body, false},
    {"eye", "eyes", ColorGroup::Body, false},
    {"skin", "skin", ColorGroup::Body, true},
    {"face", "face", ColorGroup::Body, false},
    {"mask", "face", ColorGroup::Body, false},
    {"hair", "hair", ColorGroup::Body, false},
    {"metal", "metal", ColorGroup::Outfit, true},
    {"gem", "gem", ColorGroup::Outfit, true},
    {"jewel", "gem", ColorGroup::Outfit, true},
    {"crystal", "gem", ColorGroup::Outfit, true},
    {"trim", "accent", ColorGroup::Outfit, false},
    {"ribbon", "accent", ColorGroup::Outfit, false},
    {"accent", "accent", ColorGroup::Outfit, false},
    {"lining", "accent", ColorGroup::Outfit, false},
    {"leather", "leather", ColorGroup::Outfit, false},
    {"strap", "leather", ColorGroup::Outfit, false},
    {"glow", "glow", ColorGroup::Outfit, false},
};
RoleGuess guess_role(const std::string& id) {
    for(const auto& guess:ROLE_GUESSES) if(id.find(guess.needle)!=std::string::npos) return guess;
    return {"", "garment", ColorGroup::Outfit, false};
}
// Defaults for a declared role, so a package only spells out hue_locked when it disagrees.
bool role_hue_locked(const std::string& role) {
    // The intimate pigments are a shade of the body rather than a colour of their own, so
    // they sit with skin: a body hue shift must not leave them behind, and a player who
    // wants them pinker or darker sets that one control.
    return role=="metal" || role=="gem" || role=="skin" ||
           role=="nipple" || role=="areola" || role=="labia" || role=="vestibule";
}
ColorGroup role_group(const std::string& role) {
    return (role=="skin" || role=="face" || role=="hair" || role=="eyes" || role=="eye-glow" ||
            role=="nipple" || role=="areola" || role=="labia" || role=="vestibule" || role=="body-hair")
        ? ColorGroup::Body : ColorGroup::Outfit;
}
ColorValue value(const Json& j) {
    if(!j.is_array() || j.size()!=4) throw std::runtime_error("Color needs four components");
    auto v=j.get<ColorValue>();
    for(float x:v) if(!std::isfinite(x) || x<0 || x>32) throw std::runtime_error("Color component outside supported range");
    if(v[3]>1) throw std::runtime_error("Color opacity outside supported range");
    return v;
}
void parameter(const std::string& name) {
    if(name.empty() || name.size()>128 || !std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32 && c<=126;}))
        throw std::runtime_error("Invalid material parameter name");
}
void slot(int n) { if(n<0 || n>=128) throw std::runtime_error("Color material slot outside range"); }
void valid_value(const ColorControl& c,const ColorValue& v) {
    for(size_t i=0;i<(c.scalar?1u:3u);++i)
        if(!std::isfinite(v[i]) || v[i]<c.minimum || v[i]>c.maximum) throw std::runtime_error("Color value outside control limits: "+c.id);
    if(v[3]!=c.value[3]) throw std::runtime_error("Color opacity is fixed by its author");
}
}
bool color_resource(const std::string& name) {
    return name.starts_with("dye-") && name.ends_with(".png") && valid_id(name) && name.find("..") == name.npos;
}
float srgb_linear(float v) { return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f); }
const ColorControl* ColorOptions::find(const std::string& id) const {
    for(const auto& control:controls) if(control.id==id) return &control;
    return nullptr;
}
ColorOptions ColorOptions::parse(const Json& j) {
    ColorOptions out;
    if(j.is_null() || j.empty()) return out;
    if(!j.is_object() || j.at("schema")!=1) throw std::runtime_error("Unsupported color configuration");
    for(const auto* name:{"controls","surfaces","palettes"})
        if(j.contains(name) && !j.at(name).is_array()) throw std::runtime_error("Color definitions require arrays");
    for(const auto& c:j.at("controls")) {
        ColorControl control;
        control.id=c.at("id"); control.name=c.at("name");
        if(!valid_id(control.id) || out.find(control.id) || control.name.empty() || control.name.size()>96 || out.controls.size()>=32)
            throw std::runtime_error("Invalid color control identity");
        auto type=c.value("type",std::string("color"));
        if(type=="color" || type=="scalar") {}
        else if(type=="intensity") type="scalar";      // the convention's name for a scalar
        else throw std::runtime_error("Unsupported color control type");
        if(c.contains("kind")) {
            auto kind=c.at("kind").get<std::string>();
            if(kind!="color" && kind!="intensity") throw std::runtime_error("Unsupported color control kind");
            type=kind=="intensity"?"scalar":"color";
        }
        control.scalar=type=="scalar"; control.value=value(c.at("default"));
        // Group, role and hue locking: declared if present, otherwise read off the id.
        const auto fallback=guess_role(control.id);
        control.role=c.value("role",std::string(fallback.role));
        if(control.role.empty() || control.role.size()>32 || !valid_id(control.role))
            throw std::runtime_error("Invalid color control role");
        if(c.contains("group")) {
            auto group=c.at("group").get<std::string>();
            if(group!="outfit" && group!="body") throw std::runtime_error("Unsupported color control group");
            control.group=group=="body"?ColorGroup::Body:ColorGroup::Outfit;
        } else control.group=c.contains("role")?role_group(control.role):fallback.group;
        control.hue_locked=c.value("hue_locked",
            c.contains("role")?role_hue_locked(control.role):fallback.hue_locked);
        control.minimum=c.value("min",0.f); control.maximum=c.value("max",1.f); control.step=c.value("step",.01f);
        if(!std::isfinite(control.minimum) || !std::isfinite(control.maximum) || !std::isfinite(control.step) ||
           control.minimum<0 || control.maximum>32 || control.minimum>=control.maximum || control.step<=0 || control.step>control.maximum-control.minimum)
            throw std::runtime_error("Invalid color slider range");
        valid_value(control,control.value);
        if(c.contains("bindings") && !c.at("bindings").is_array()) throw std::runtime_error("Color bindings require an array");
        for(const auto& b:c.value("bindings",Json::array())) {
            ColorBinding binding; binding.slot=b.at("slot"); binding.parameter=b.at("parameter");
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
        ColorSurface surface; surface.id=s.at("id"); surface.parameter=s.at("parameter");
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
            if(!c || c->scalar || !color_resource(file)) throw std::runtime_error("Invalid dye layer control or file");
        }
        out.surfaces.push_back(std::move(surface));
    }
    for(const auto& c:out.controls) {
        bool used=!c.bindings.empty();
        for(const auto& s:out.surfaces) used|=s.layers.contains(c.id);
        if(!used) throw std::runtime_error("Color control has no material or texture binding");
    }
    ids.clear();
    for(const auto& p:j.value("palettes",Json::array())) {
        ColorPalette palette; palette.id=p.at("id"); palette.name=p.at("name");
        if(!valid_id(palette.id) || palette.id=="original" || !ids.insert(palette.id).second || palette.name.empty() || palette.name.size()>96 || out.palettes.size()>=64)
            throw std::runtime_error("Invalid color palette");
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
    if(!j.is_object()) throw std::runtime_error("Invalid saved colors");
    result.palette=j.value("palette",std::string("original"));
    if(!valid_id(result.palette)) throw std::runtime_error("Invalid saved palette");
    auto values=j.value("values",Json::object());
    if(!values.is_object() || values.size()>32) throw std::runtime_error("Invalid saved color count");
    for(const auto& [id,v]:values.items()) {
        if(!valid_id(id)) throw std::runtime_error("Invalid saved color control");
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
Customization compatible_colors(const ColorOptions& options,const Customization& source) {
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
                       [&](const auto& c){return !c.scalar && color_group_name(c.group)==group;}))
            result.tints[group]=tint;
    return result;
}
const char* color_group_name(ColorGroup group) { return group==ColorGroup::Body?"body":"outfit"; }
// A palette is a look, not a reset. It takes over the parts it sets and the tint of the
// groups those parts are in, and leaves everything else alone, so a custom skin survives
// changing the dress. `original` is the exception: it means no dye at all, and nothing
// may survive it. See docs/color-convention.md, rule 1.
Customization choose_palette(const ColorOptions& options,const Customization& current,const std::string& palette) {
    Customization result=current;
    result.palette=palette;
    if(palette=="original") { result.values.clear(); result.tints.clear(); return result; }
    auto found=std::find_if(options.palettes.begin(),options.palettes.end(),
                            [&](const auto& p){return p.id==palette;});
    if(found==options.palettes.end()) throw std::runtime_error("That palette is not installed");
    std::set<std::string> groups;
    for(const auto& [id,value]:found->values) if(auto* control=options.find(id)) {
        result.values.erase(id);
        groups.insert(color_group_name(control->group));
    }
    for(const auto& group:groups) result.tints.erase(group);
    return result;
}
// Hue rotation with saturation and brightness scaling, through HSV. A hue-locked control
// keeps its own hue and takes only the other two, which is what stops a group tint turning
// gold green and skin blue. Alpha is author-controlled and never touched.
ColorValue apply_tint(const ColorTint& tint,const ColorValue& colour,bool hue_locked) {
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
std::map<std::string,ColorValue> color_values(const ColorOptions& options,const Customization& custom) {
    std::map<std::string,ColorValue> result;
    if(custom.palette!="original") {
        auto found=std::find_if(options.palettes.begin(),options.palettes.end(),[&](const auto& p){return p.id==custom.palette;});
        if(found==options.palettes.end()) throw std::runtime_error("Saved palette is not installed");
        result=found->values;
    }
    for(const auto& [id,value]:custom.values) result[id]=value;
    for(const auto& [id,value]:result) {
        auto* c=options.find(id); if(!c) throw std::runtime_error("Saved color part is not installed");
        valid_value(*c,value);
    }
    // The group tint sits on top of the palette and of any per-part override, so the whole
    // pipeline is one line: tint(override or palette). On Original nothing is dyed at all,
    // so `result` is empty and there is nothing for a tint to move.
    for(auto& [id,value]:result) {
        auto* c=options.find(id);
        if(c->scalar) continue;
        const auto tint=custom.tints.find(color_group_name(c->group));
        if(tint==custom.tints.end()) continue;
        value=apply_tint(tint->second,value,c->hue_locked);
        valid_value(*c,value);
    }
    return result;
}
}

#include "colors.hpp"
#include "data.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace css {
namespace {
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
        if(type!="color" && type!="scalar") throw std::runtime_error("Unsupported color control type");
        control.scalar=type=="scalar"; control.value=value(c.at("default"));
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
    return result;
}
Json Customization::json() const { return {{"palette",palette},{"values",values}}; }
Customization compatible_colors(const ColorOptions& options,const Customization& source) {
    Customization result;
    if(std::any_of(options.palettes.begin(),options.palettes.end(),[&](const auto& p){return p.id==source.palette;})) result.palette=source.palette;
    for(const auto& [id,value]:source.values) if(auto* control=options.find(id)) {
        try { valid_value(*control,value); result.values[id]=value; }
        catch(const std::exception&) { /* A changed part keeps its new authored default. */ }
    }
    return result;
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
    return result;
}
}

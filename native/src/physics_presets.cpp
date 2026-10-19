#include "physics_presets.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace css {
namespace {
// Body presets per region: {frequency Hz, damping ratio, third channel}. The third channel
// is the rig's motion amount, or a spring's travel clamp in cm (spring rows). `travel` is
// the angular rig's travel in degrees, written only to a part that declares a travel range.
// Firm is the Eve 1.5.0 envelope (chest 3.8, glutes 8.0, thighs 3, belly 5 degrees after
// the cm cap); the looser rows are starting points pending the trace comparison in
// docs/research/2026-10-19-body-motion-travel-channel.md.
struct BodyPreset {
    const char* id; const char* name; const char* description;
    std::array<std::array<float,3>,4> rig, spring;   // chest, glute, thigh, belly
    std::array<float,4> travel;                      // chest, glute, thigh, belly
    // Soft limit band, a fraction of the travel (rig SoftZone). The lightly damped
    // Earthquake row gets a wider band to spread its stop (Fable, 19 Oct 2026).
    float soft_zone=body_soft_zone_default;
};
constexpr BodyPreset body_presets[]={
    {"firm","Firm","High stiffness & damping, perky sculpted look",
     {{{2.60f,.65f,.60f},{2.50f,.65f,.65f},{2.80f,.72f,.40f},{2.70f,.68f,.45f}}},
     {{{2.60f,.65f,1.20f},{2.50f,.65f,1.50f},{2.80f,.72f,.80f},{2.70f,.68f,.90f}}},
     {4,8,3,5}},
    {"natural","Natural","Realistic soft-tissue sway, balanced & restrained",
     {{{2.15f,.48f,1.00f},{2.10f,.50f,1.00f},{2.35f,.58f,.75f},{2.20f,.52f,.85f}}},
     {{{2.15f,.48f,2.20f},{2.10f,.50f,2.20f},{2.35f,.58f,1.60f},{2.20f,.52f,1.80f}}},
     {6,10,4,6}},
    {"bouncy","Bouncy","Playful, energetic motion with plenty of bounce",
     {{{1.70f,.28f,1.80f},{1.65f,.30f,1.80f},{1.85f,.38f,1.35f},{1.75f,.32f,1.50f}}},
     {{{1.70f,.28f,4.50f},{1.65f,.30f,4.50f},{1.85f,.38f,3.20f},{1.75f,.32f,3.80f}}},
     {10,14,6,8}},
    {"soft","Soft / Saggy","Heavier relaxed tissue, slow and swinging",
     {{{1.25f,.18f,2.60f},{1.20f,.20f,2.50f},{1.40f,.25f,2.00f},{1.30f,.22f,2.20f}}},
     {{{1.25f,.18f,7.50f},{1.20f,.20f,7.00f},{1.40f,.25f,5.50f},{1.30f,.22f,6.50f}}},
     {14,16,7,9}},
    {"earthquake","OMG! Earthquake!","Maximum exaggerated comedic jiggle & wobble",
     {{{.85f,.06f,4.20f},{.85f,.08f,4.20f},{.95f,.10f,3.50f},{.90f,.08f,3.80f}}},
     {{{.85f,.06f,14.0f},{.85f,.08f,14.0f},{.95f,.10f,10.0f},{.90f,.08f,13.0f}}},
     {20,18,10,12},.6f},
};
// Positional hair rig: {stiffness, damping, gravity scale}.
struct HairPreset { const char* id; const char* name; const char* description; std::array<float,3> values; };
constexpr HairPreset hair_presets[]={
    {"firm","Firm","Clean, disciplined ponytail with hairspray hold",{260,26,.04f}},
    {"natural","Natural","Athletic flow, quick and steady",{180,16,.08f}},
    {"silky","Silky","Soft, elegant hair with loose fluid sway",{110,10,.14f}},
    {"heavy","Heavy","Dense weighted hair, hugs back and resists lift",{190,22,.35f}},
    {"floaty","Floaty","Light strands with a slow trailing wave",{55,6,0}},
};
std::string lower(std::string text) {
    for(char& c:text) c=char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
PhysicsRegion region_of_bone(const std::string& raw) {
    const auto bone=lower(raw);
    if(bone.starts_with("brust") || bone.starts_with("breast")) return PhysicsRegion::chest;
    if(bone.starts_with("butt") || bone.starts_with("glute")) return PhysicsRegion::glute;
    if(bone.starts_with("thigh")) return PhysicsRegion::thigh;
    if(bone.starts_with("belly")) return PhysicsRegion::belly;
    return PhysicsRegion::none;
}
PhysicsRegion region_of_words(const std::string& raw) {
    const auto text=lower(raw);
    auto has=[&](std::initializer_list<std::string_view> words) {
        return std::any_of(words.begin(),words.end(),[&](std::string_view w){ return text.find(w)!=std::string::npos; });
    };
    if(has({"chest","breast","boob","bust"})) return PhysicsRegion::chest;
    if(has({"glute","butt"})) return PhysicsRegion::glute;
    if(has({"thigh","hip"})) return PhysicsRegion::thigh;
    if(has({"belly","waist","abdomen","stomach"})) return PhysicsRegion::belly;
    return PhysicsRegion::none;
}
bool hair_rig(const Control& c) { return c.kind==ControlKind::Rig && c.rig && !c.rig->body; }
std::optional<float> clamp_travel(const Control& c,std::optional<float> travel) {
    if(!travel || !body_travel_control(c)) return std::nullopt;
    return std::clamp(*travel,c.rig->travel->minimum,c.rig->travel->maximum);
}
// Whether a channel carries a setting: a spring without a travel clamp ignores channel 2.
bool live_channel(const Control& c,int channel) {
    return !(c.kind==ControlKind::Spring && channel==2 && !c.spring_clamp);
}
}

PhysicsRegion physics_region(const Control& c) {
    if(c.kind==ControlKind::Rig && c.rig && c.rig->body)
        for(auto index:c.rig->regions)
            if(index<body_region_names.size())
                if(auto region=region_of_bone(body_region_names[index]);region!=PhysicsRegion::none) return region;
    if(c.kind==ControlKind::Spring)
        for(const auto& node:c.nodes)
            if(auto region=region_of_bone(node);region!=PhysicsRegion::none) return region;
    if(auto region=region_of_words(c.id);region!=PhysicsRegion::none) return region;
    return region_of_words(c.name);
}

std::vector<PhysicsPreset> physics_presets(const Control& c) {
    std::vector<PhysicsPreset> out;
    auto clamp_to=[&](std::array<float,3> values) {
        for(int channel=0;channel<3;++channel) if(live_channel(c,channel)) {
            const auto range=control_channel(c,channel);
            values[size_t(channel)]=std::clamp(values[size_t(channel)],range.minimum,range.maximum);
        }
        return values;
    };
    if(hair_rig(c)) {
        // Designated: a positional `true` here would land in `travel`, not `builtin`.
        for(const auto& p:hair_presets)
            out.push_back({.id=p.id,.name=p.name,.description=p.description,.channels=clamp_to(p.values),.travel=std::nullopt,.soft_zone=std::nullopt,.builtin=true});
    } else if((c.kind==ControlKind::Rig && c.rig && c.rig->body) || c.kind==ControlKind::Spring) {
        const auto region=physics_region(c);
        if(region!=PhysicsRegion::none) {
            const bool spring=c.kind==ControlKind::Spring;
            for(const auto& p:body_presets)
                out.push_back({.id=p.id,.name=p.name,.description=p.description,
                               .channels=clamp_to((spring?p.spring:p.rig)[size_t(region)]),
                               .travel=spring?std::nullopt:clamp_travel(c,p.travel[size_t(region)]),
                               .soft_zone=spring?std::nullopt:std::optional<float>(p.soft_zone),.builtin=true});
        }
    }
    out.insert(out.end(),c.presets.begin(),c.presets.end());
    return out;
}

ControlValue physics_preset_value(const Control& c,const PhysicsPreset& preset,const ControlValue& current) {
    auto value=current;
    for(int channel=0;channel<3;++channel)
        if(live_channel(c,channel)) value[size_t(channel)]=preset.channels[size_t(channel)];
    return value;
}

std::optional<float> physics_preset_travel(const Control& c,const PhysicsPreset& preset) {
    return clamp_travel(c,preset.travel);
}

std::string matching_physics_preset(const Control& c,const ControlValue& value,std::optional<float> travel) {
    for(const auto& preset:physics_presets(c)) {
        bool same=true;
        for(int channel=0;channel<3 && same;++channel) {
            if(!live_channel(c,channel)) continue;
            const auto range=control_channel(c,channel);
            // Half a percent of the range: what applying a preset writes matches exactly,
            // a slider moved by even one step does not.
            const float tolerance=std::max(1e-4f,.005f*(range.maximum-range.minimum));
            same=std::abs(value[size_t(channel)]-preset.channels[size_t(channel)])<=tolerance;
        }
        // Travel counts only when the caller knows it and the preset sets one.
        if(same && travel && preset.travel && body_travel_control(c)) {
            const auto range=control_channel(c,4);
            const float tolerance=std::max(1e-4f,.005f*(range.maximum-range.minimum));
            same=std::abs(*travel-*physics_preset_travel(c,preset))<=tolerance;
        }
        if(same) return preset.id;
    }
    return {};
}

std::map<std::string,float> physics_soft_zones(const std::vector<Control>& controls,
                                               const std::map<std::string,ControlValue>& values,
                                               const std::map<std::string,float>& travel) {
    std::map<std::string,float> out;
    for(const auto& c:controls) {
        if(!body_rig_control(c)) continue;
        const auto value=values.find(c.id);
        const auto degrees=travel.find(c.id);
        if(value==values.end() && degrees==travel.end()) continue;
        float zone=body_soft_zone_default;
        const auto held=value!=values.end()?value->second:c.value;
        const auto id=matching_physics_preset(c,held,degrees!=travel.end()?std::optional<float>(degrees->second):std::nullopt);
        if(!id.empty()) for(const auto& preset:physics_presets(c))
            if(preset.id==id) { if(preset.soft_zone) zone=*preset.soft_zone; break; }
        out[c.id]=std::clamp(zone,0.f,body_soft_zone_ceiling);
    }
    return out;
}

Customization apply_physics_preset(const ControlSet& options,Customization custom,const std::string& requested,
                                   const std::string& control_id,std::string* label) {
    const auto preset_id=physics_preset_id(requested);
    const auto current=control_values(options,custom);
    std::string applied;
    for(const auto& ctrl:options.controls) {
        if(!control_id.empty() && ctrl.id!=control_id) continue;
        const auto presets=physics_presets(ctrl);
        const auto found=std::find_if(presets.begin(),presets.end(),[&](const auto& p){ return p.id==preset_id; });
        if(found==presets.end()) continue;
        const auto held=current.contains(ctrl.id)?current.at(ctrl.id):ctrl.value;
        custom.values[ctrl.id]=physics_preset_value(ctrl,*found,held);
        if(const auto travel=physics_preset_travel(ctrl,*found)) custom.travel[ctrl.id]=*travel;
        applied=found->name;
    }
    if(applied.empty()) throw std::runtime_error("No part has the physics preset "+preset_id);
    if(label) *label=applied;
    return custom;
}

std::string physics_preset_id(std::string id) {
    id=lower(std::move(id));
    if(id=="normal") return "natural";
    if(id=="more_jiggle") return "bouncy";
    if(id=="saggy") return "soft";
    if(id=="omg_earthquake") return "earthquake";
    return id;
}

bool builtin_physics_preset(const std::string& id) {
    for(const auto& p:body_presets) if(id==p.id) return true;
    for(const auto& p:hair_presets) if(id==p.id) return true;
    return false;
}
}

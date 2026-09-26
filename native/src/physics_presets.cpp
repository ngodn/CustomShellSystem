#include "physics_presets.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string_view>

namespace css {
namespace {
// Body presets per region: {frequency Hz, damping ratio, third channel}. The third channel
// is the rig's motion amount, or a spring's travel clamp in cm (spring rows).
struct BodyPreset {
    const char* id; const char* name; const char* description;
    std::array<std::array<float,3>,4> rig, spring;   // chest, glute, thigh, belly
};
constexpr BodyPreset body_presets[]={
    {"firm","Firm","High stiffness & damping, perky sculpted look",
     {{{2.60f,.65f,.60f},{2.50f,.65f,.65f},{2.80f,.72f,.40f},{2.70f,.68f,.45f}}},
     {{{2.60f,.65f,1.20f},{2.50f,.65f,1.50f},{2.80f,.72f,.80f},{2.70f,.68f,.90f}}}},
    {"natural","Natural","Realistic soft-tissue sway, balanced & restrained",
     {{{2.15f,.48f,1.00f},{2.10f,.50f,1.00f},{2.35f,.58f,.75f},{2.20f,.52f,.85f}}},
     {{{2.15f,.48f,2.20f},{2.10f,.50f,2.20f},{2.35f,.58f,1.60f},{2.20f,.52f,1.80f}}}},
    {"bouncy","Bouncy","Playful, energetic motion with plenty of bounce",
     {{{1.70f,.28f,1.80f},{1.65f,.30f,1.80f},{1.85f,.38f,1.35f},{1.75f,.32f,1.50f}}},
     {{{1.70f,.28f,4.50f},{1.65f,.30f,4.50f},{1.85f,.38f,3.20f},{1.75f,.32f,3.80f}}}},
    {"soft","Soft / Saggy","Heavier relaxed tissue, slow and swinging",
     {{{1.25f,.18f,2.60f},{1.20f,.20f,2.50f},{1.40f,.25f,2.00f},{1.30f,.22f,2.20f}}},
     {{{1.25f,.18f,7.50f},{1.20f,.20f,7.00f},{1.40f,.25f,5.50f},{1.30f,.22f,6.50f}}}},
    {"earthquake","OMG! Earthquake!","Maximum exaggerated comedic jiggle & wobble",
     {{{.85f,.06f,4.20f},{.85f,.08f,4.20f},{.95f,.10f,3.50f},{.90f,.08f,3.80f}}},
     {{{.85f,.06f,14.0f},{.85f,.08f,14.0f},{.95f,.10f,10.0f},{.90f,.08f,13.0f}}}},
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
        for(const auto& p:hair_presets) out.push_back({p.id,p.name,p.description,clamp_to(p.values),true});
    } else if((c.kind==ControlKind::Rig && c.rig && c.rig->body) || c.kind==ControlKind::Spring) {
        const auto region=physics_region(c);
        if(region!=PhysicsRegion::none) {
            const bool spring=c.kind==ControlKind::Spring;
            for(const auto& p:body_presets)
                out.push_back({p.id,p.name,p.description,clamp_to((spring?p.spring:p.rig)[size_t(region)]),true});
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

std::string matching_physics_preset(const Control& c,const ControlValue& value) {
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
        if(same) return preset.id;
    }
    return {};
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

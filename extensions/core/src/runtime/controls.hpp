#pragma once
#include "manifest.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace cssx {
inline bool adjustable(const Json& c) {
    const auto type=c.at("type");
    return type=="number" || type=="slider" || type=="choice" || type=="radio";
}
inline bool interactive(const Json& c) {
    const auto type=c.at("type");
    return type!="label" && type!="progress" && type!="loading" && c.value("enabled",true) && !c.value("busy",false);
}
inline std::string display_value(const Json& c) {
    const auto type=c.at("type");
    if(c.value("busy",false)) return "Working...";
    if(type=="toggle") return c.at("value").get<bool>()?"On":"Off";
    if(type=="loading") return c.at("value").get<bool>()?"Loading...":"Ready";
    if(type=="choice" || type=="radio") {
        for(const auto& option:c.at("options")) if(option.at("id")==c.at("value")) return option.at("label").get<std::string>();
    }
    if(type=="text") return c.at("value").get<std::string>();
    if(type=="number" || type=="slider" || type=="progress") {
        const auto value=c.at("value").get<double>()*(type=="progress"?100.:1.);
        std::ostringstream out;out<<std::fixed<<std::setprecision(type=="progress"?0:4)<<value;
        auto text=out.str();if(text.find('.')!=std::string::npos) {while(text.ends_with('0')) text.pop_back();if(text.ends_with('.'))text.pop_back();}
        if(type=="progress")text+="%";
        return text;
    }
    return {};
}
inline double snap_value(const Json& c,double raw) {
    if(!std::isfinite(raw)) throw std::runtime_error("Value must be finite");
    const double low=c.at("min"),high=c.at("max"),step=c.at("step");
    return std::clamp(low+std::round((std::clamp(raw,low,high)-low)/step)*step,low,high);
}
inline double entered_number(const Json& c,std::string_view text) {
    const auto whitespace=[](char ch) { return ch==' ' || ch=='\t' || ch=='\r' || ch=='\n'; };
    while(!text.empty() && whitespace(text.front())) text.remove_prefix(1);
    while(!text.empty() && whitespace(text.back())) text.remove_suffix(1);
    if(!text.empty() && text.front()=='+') text.remove_prefix(1);
    if(text.empty() || text.size()>128) throw std::runtime_error("Enter a number.");
    double value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !std::isfinite(value))
        throw std::runtime_error("Enter a number.");
    const double low=c.at("min"), high=c.at("max"), step=c.at("step");
    auto shown=c;
    shown["value"]=low; const auto minimum=display_value(shown);
    shown["value"]=high; const auto maximum=display_value(shown);
    if(value<low || value>high) throw std::runtime_error("Enter a number from "+minimum+" to "+maximum+".");
    const auto snapped=snap_value(c,value);
    if(std::abs(snapped-value)>std::max(1e-10,step*1e-9)) {
        shown["value"]=step;
        throw std::runtime_error(step==1 ? "Enter a whole number." : "Use steps of "+display_value(shown)+".");
    }
    return snapped;
}
inline Json adjusted_value(const Json& c,int direction) {
    if(!interactive(c) || !adjustable(c)) throw std::runtime_error("Control cannot be adjusted");
    const auto type=c.at("type");
    if(type=="number" || type=="slider") return snap_value(c,c.at("value").get<double>()+(direction<0?-1:1)*c.at("step").get<double>());
    const auto& options=c.at("options");
    for(size_t i=0;i<options.size();++i) if(options[i].at("id")==c.at("value"))
        return options[(i+options.size()+(direction<0?-1:1))%options.size()].at("id");
    throw std::runtime_error("Selected option no longer exists");
}
inline void validate_event(const Json& model,const Json& event) {
    if(!event.is_object()) throw std::runtime_error("Control event must be an object");
    const auto id=event.at("id").get<std::string>();
    for(const auto& section:model.at("sections")) for(const auto& c:section.at("controls")) if(c.at("id")==id) {
        if(!interactive(c)) throw std::runtime_error("Control is disabled or read-only");
        if(c.contains("confirm") && !event.value("confirmed",false)) throw std::runtime_error("Control requires confirmation");
        if(c.at("type")!="button") {
            Json candidate=c;candidate["value"]=event.at("value");
            validate_model({{"sections",Json::array({{{"id","event"},{"title","Event"},{"controls",Json::array({candidate})}}})}});
        }
        return;
    }
    throw std::runtime_error("Unknown control ID");
}
}

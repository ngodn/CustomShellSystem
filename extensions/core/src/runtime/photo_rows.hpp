#pragma once
// Photo Mode rows (CSSX 1.3.2): what one menu control shows as a row of the game's Photo Mode
// panel, and which event a menu key on that row sends. Portable, so the host tests cover it;
// the widgets themselves are core/photo.cpp.
#include "controls.hpp"
#include <string>
#include <vector>

namespace cssx {
struct PhotoRow {
    std::string id, name, value;
    bool slider=false;     // a slider row (bar under the value) instead of an option row
    bool arrows=false;     // left/right change it
    bool enabled=true;
    double fill=0;         // slider position 0..1
};

// The value column of a game row holds about this many characters at the game's size.
constexpr size_t photo_value_chars=22;

// Characters (UTF-8 code points) in a text.
inline size_t photo_chars(const std::string& text) {
    size_t count=0;
    for(unsigned char c:text) count+=(c&0xC0)!=0x80;
    return count;
}

// The rows of one menu section, in order. Left out, so every row reads like one of the game's:
// text input (the panel has no text field), and a label whose text is too long for the value
// column (prose belongs in the Player Menu, where it has room).
inline std::vector<PhotoRow> photo_rows(const Json& model,const std::string& section) {
    std::vector<PhotoRow> rows;
    if(!model.is_object() || !model.contains("sections")) return rows;
    for(const auto& s:model.at("sections")) {
        if(s.value("id",std::string())!=section) continue;
        for(const auto& c:s.at("controls")) {
            const auto type=c.at("type").get<std::string>();
            if(type=="text") continue;
            PhotoRow row;
            row.id=c.at("id").get<std::string>();
            row.name=c.value("label",row.id);
            row.enabled=interactive(c) || type=="label" || type=="progress" || type=="loading";
            row.slider=type=="slider";
            row.arrows=interactive(c) && (adjustable(c) || type=="toggle");
            if(type=="button") {
                row.value=c.value("enabled",true) ? std::string() : c.value("disabled_label",std::string());
            } else if(type=="label") {
                row.value=c.contains("value") && c.at("value").is_string() ? c.at("value").get<std::string>() : std::string();
                if(photo_chars(row.value)>photo_value_chars) continue;
            } else {
                row.value=display_value(c);
                if(!c.value("enabled",true) && c.contains("disabled_label")) row.value=c.at("disabled_label").get<std::string>();
                else if((type=="number" || type=="slider") && c.contains("unit")) row.value+=" "+c.at("unit").get<std::string>();
            }
            if(row.slider) {
                const double low=c.at("min"),high=c.at("max"),value=c.at("value");
                row.fill=high>low ? std::clamp((value-low)/(high-low),0.,1.) : 0.;
            }
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

// The control with this id in a section, or null.
inline const Json* photo_control(const Json& model,const std::string& section,const std::string& id) {
    if(!model.is_object() || !model.contains("sections")) return nullptr;
    for(const auto& s:model.at("sections")) if(s.value("id",std::string())==section)
        for(const auto& c:s.at("controls")) if(c.at("id")==id) return &c;
    return nullptr;
}

// The event a key on a control sends ("left", "right", "accept"), or null when the key does
// nothing there. A control that asks for confirmation sends nothing until `confirmed`.
inline Json photo_event(const Json& c,const std::string& key,bool confirmed) {
    if(!interactive(c)) return nullptr;
    const auto type=c.at("type").get<std::string>();
    const auto id=c.at("id");
    if(type=="button") {
        if(key!="accept") return nullptr;
        if(c.contains("confirm") && !confirmed) return nullptr;
        Json e={{"id",id}}; if(c.contains("confirm")) e["confirmed"]=true; return e;
    }
    if(type=="toggle") {
        if(key!="left" && key!="right" && key!="accept") return nullptr;
        Json e={{"id",id},{"value",!c.at("value").get<bool>()}};
        if(c.contains("confirm")) { if(!confirmed) return nullptr; e["confirmed"]=true; }
        return e;
    }
    if(!adjustable(c) || (key!="left" && key!="right")) return nullptr;
    const auto value=adjusted_value(c,key=="left"?-1:1);
    if(value==c.at("value")) return nullptr;   // at the end of the range
    Json e={{"id",id},{"value",value}};
    if(c.contains("confirm")) { if(!confirmed) return nullptr; e["confirmed"]=true; }
    return e;
}
}

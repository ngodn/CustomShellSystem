#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <cmath>

namespace cssx::engine {
enum class WidgetParamType { object, byte_enum, margin, other };
enum class WidgetParamRole { input, result, other };
struct WidgetParam {
    std::wstring name;
    WidgetParamType type;
    WidgetParamRole role;
};

// Shipping reflection may rename parameters, and a slot result has the same
// property type as the child input. Resolve both before changing the widget.
template<class Frame>
std::wstring widget_parameter(const Frame& call,WidgetParamType type,WidgetParamRole role) {
    std::wstring name;
    for(const auto& field:call.parameters()) {
        const auto param=call.describe_widget_parameter(field);
        if(param.type!=type || param.role!=role) continue;
        if(!name.empty()) throw std::runtime_error("Ambiguous widget parameter");
        name=param.name;
    }
    if(name.empty()) throw std::runtime_error("Missing widget parameter of the expected type and direction");
    return name;
}

template<class Frame,class Widget>
Widget* set_widget_child(Widget* parent,const wchar_t* function,Widget* child) {
    Frame call(parent,function,2);
    const auto input=widget_parameter(call,WidgetParamType::object,WidgetParamRole::input);
    const auto result=widget_parameter(call,WidgetParamType::object,WidgetParamRole::result);
    call.set(input.c_str(),child);
    call.run();
    return call.template get<Widget*>(result.c_str());
}

template<class Frame,class Widget,class Value>
void set_widget_option(Widget* target,const wchar_t* function,WidgetParamType type,const Value& value) {
    Frame call(target,function,1);
    const auto input=widget_parameter(call,type,WidgetParamRole::input);
    call.set(input.c_str(),value);
    call.run();
}

template<class Frame,class Widget>
void fit_information_value(Widget* column,Widget* value) {
    auto* slot=set_widget_child<Frame>(column,L"SetContent",value);
    set_widget_option<Frame>(slot,L"SetHorizontalAlignment",WidgetParamType::byte_enum,uint8_t{2});
    set_widget_option<Frame>(slot,L"SetVerticalAlignment",WidgetParamType::byte_enum,uint8_t{2});
    Frame wrap(value,L"SetAutoWrapText",1);
    wrap.set_single_bool_input(false);
    wrap.run();
    set_widget_option<Frame>(value,L"SetTextOverflowPolicy",WidgetParamType::byte_enum,uint8_t{1});
    set_widget_option<Frame>(value,L"SetClipping",WidgetParamType::byte_enum,uint8_t{1});
}

// Measure each candidate as shaped text on the same block that will paint it.
// A literal marker also works when an ancestor clips before Slate paints its overflow.
template<class Measure>
std::string fit_information_text(const std::string& source,double width,Measure measure) {
    if(source.empty() || !std::isfinite(width) || width<=0) return source;
    const double available=std::floor(width)-1;  // leave space for pixel snapping
    const auto newline=source.find_first_of("\r\n");
    const auto length=std::min(newline,source.size());
    size_t end=std::min<size_t>(128,length);
    // Avoid asking Slate to shape a whole menu-sized paragraph to fill one column.
    while(end<length) {
        while(end<length && (static_cast<unsigned char>(source[end])&0xc0)==0x80) ++end;
        if(end==length || measure(source.substr(0,end))>available) break;
        end=std::min(end*2,length);
    }
    if(end==length && newline==std::string::npos && measure(source)<=available) return source;
    const auto line=source.substr(0,end);
    std::vector<size_t> ends{0};
    for(size_t i=1;i<line.size();++i)
        if((static_cast<unsigned char>(line[i])&0xc0)!=0x80) ends.push_back(i);
    if(!line.empty()) ends.push_back(line.size());
    size_t lo=0,hi=ends.size();
    while(lo+1<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(measure(line.substr(0,ends[mid])+"...")<=available) lo=mid;
        else hi=mid;
    }
    return line.substr(0,ends[lo])+"...";
}

template<class Frame,class Size,class Widget>
double desired_text_width(Widget* block) {
    Frame size(block,L"GetDesiredSize",1);
    size.run();
    return size.template get<Size>().x;
}

template<class Frame,class Size,class Widget,class Write>
std::pair<std::string,double> display_information(Widget* block,const std::string& source,double width,Write write) {
    auto measure=[&](const std::string& text) {
        write(block,text);
        Frame prepass(block,L"ForceLayoutPrepass",0);
        prepass.run();
        return desired_text_width<Frame,Size>(block);
    };
    auto fitted=fit_information_text(source,width,measure);
    // A binary search can finish on a rejected probe. Commit the chosen text.
    const auto measured=measure(fitted);
    return {std::move(fitted),measured};
}

}

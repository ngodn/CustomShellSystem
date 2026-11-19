#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

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
    set_widget_option<Frame>(value,L"SetTextOverflowPolicy",WidgetParamType::byte_enum,uint8_t{1});
    set_widget_option<Frame>(value,L"SetClipping",WidgetParamType::byte_enum,uint8_t{1});
}
}

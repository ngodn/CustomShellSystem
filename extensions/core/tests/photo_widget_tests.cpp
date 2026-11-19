#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/core/widget_calls.hpp"

namespace {
enum class Type { object, byte, margin };
enum class Direction { input, result, output, reference };
struct Field { std::wstring name; Type type; Direction direction; };
struct Widget {
    std::map<std::wstring,std::vector<Field>> functions;
    Widget* child=nullptr;
    Widget* slot=nullptr;
    std::map<std::wstring,uint8_t> settings;
    std::array<float,4> padding{};
};
struct Frame {
    Widget* target;
    std::wstring fn;
    const std::vector<Field>& fields;
    Widget* child=nullptr;
    uint8_t setting=0;
    bool ran=false;
    std::array<float,4> padding{};
    Frame(Widget* target,const wchar_t* fn,size_t count)
        :target(target),fn(fn),fields(target->functions.at(fn)) {
        if(fields.size()!=count) throw std::runtime_error("wrong arity");
    }
    const auto& parameters() const { return fields; }
    static cssx::engine::WidgetParam describe_widget_parameter(const Field& f) {
        using namespace cssx::engine;
        const auto role=f.direction==Direction::result?WidgetParamRole::result:
                        f.direction==Direction::input?WidgetParamRole::input:WidgetParamRole::other;
        const auto type=f.type==Type::object?WidgetParamType::object:
                        f.type==Type::byte?WidgetParamType::byte_enum:WidgetParamType::margin;
        return {f.name,type,role};
    }
    const Field& named(const wchar_t* name) const {
        for(const auto& field:fields) if(field.name==name) return field;
        std::wstring key(name);
        throw std::runtime_error("Missing parameter: "+std::string(key.begin(),key.end()));
    }
    void set(const wchar_t* name,Widget* value) {
        const auto& f=named(name);
        if(f.type!=Type::object || f.direction!=Direction::input) throw std::runtime_error("wrong child input");
        child=value;
    }
    void set(const wchar_t* name,uint8_t value) {
        const auto& f=named(name);
        if(f.type!=Type::byte || f.direction!=Direction::input) throw std::runtime_error("wrong enum input");
        setting=value;
    }
    void set(const wchar_t* name,const std::array<float,4>& value) {
        const auto& f=named(name);
        if(f.type!=Type::margin || f.direction!=Direction::input) throw std::runtime_error("wrong margin input");
        padding=value;
    }
    template<class T> T get(const wchar_t* name=L"ReturnValue") const {
        const auto& f=named(name);
        if(!ran || f.type!=Type::object || f.direction!=Direction::result) throw std::runtime_error("wrong slot result");
        return target->slot;
    }
    void run() {
        if(fn==L"SetContent" || fn==L"AddChild" || fn==L"AddChildToHorizontalBox") target->child=child;
        else if(fn==L"SetPadding") target->padding=padding;
        else target->settings[fn]=setting;
        ran=true;
    }
};
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void check_layout(std::wstring child_name,std::wstring result_name,std::wstring option_name) {
    Widget column{}, value{}, slot{};
    column.slot=&slot;
    column.functions[L"SetContent"]={{result_name,Type::object,Direction::result},{child_name,Type::object,Direction::input}};
    for(auto fn:{L"SetHorizontalAlignment",L"SetVerticalAlignment"}) slot.functions[fn]={{option_name,Type::byte,Direction::input}};
    for(auto fn:{L"SetTextOverflowPolicy",L"SetClipping"}) value.functions[fn]={{option_name,Type::byte,Direction::input}};
    cssx::engine::fit_information_value<Frame>(&column,&value);
    check(column.child==&value,"SetContent did not receive the value widget");
    check(slot.settings.at(L"SetHorizontalAlignment")==2 && slot.settings.at(L"SetVerticalAlignment")==2,"wrong slot alignment");
    check(value.settings.at(L"SetTextOverflowPolicy")==1 && value.settings.at(L"SetClipping")==1,"wrong overflow or clipping");
}
void check_invalid_child(std::vector<Field> fields) {
    Widget parent{},child{};
    parent.functions[L"SetContent"]=fields;
    bool rejected=false;
    try { cssx::engine::set_widget_child<Frame>(&parent,L"SetContent",&child); }
    catch(const std::runtime_error&) { rejected=true; }
    check(rejected && parent.child==nullptr,"invalid signature should fail before the widget changes");
}
void check_tab_calls() {
    using namespace cssx::engine;
    Widget parent{},child{},slot{};
    parent.slot=&slot;
    for(auto fn:{L"AddChild",L"AddChildToHorizontalBox"}) {
        parent.child=nullptr;
        parent.functions[fn]={{L"result",Type::object,Direction::result},{L"widget",Type::object,Direction::input}};
        check(set_widget_child<Frame>(&parent,fn,&child)==&slot && parent.child==&child,"wrong attached widget or slot");
    }
    slot.functions[L"SetPadding"]={{L"margin",Type::margin,Direction::input}};
    const std::array<float,4> padding{1,2,3,4};
    set_widget_option<Frame>(&slot,L"SetPadding",WidgetParamType::margin,padding);
    check(slot.padding==padding,"tab margin did not reach the slot");
}
}
int main() {
    try {
        check_layout(L"content",L"ReturnValue",L"setting");
        check_layout(L"ChildWidget",L"ResultSlot",L"renamed");
        const Field child{L"content",Type::object,Direction::input},slot{L"ReturnValue",Type::object,Direction::result};
        check_invalid_child({{L"content",Type::byte,Direction::input},slot});
        check_invalid_child({{L"content",Type::object,Direction::reference},slot});
        check_invalid_child({child,{L"ReturnValue",Type::object,Direction::output}});
        check_invalid_child({child,{L"other",Type::object,Direction::input}});
        check_tab_calls();
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

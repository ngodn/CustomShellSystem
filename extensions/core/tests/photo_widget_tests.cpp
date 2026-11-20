#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/core/widget_calls.hpp"

namespace {
enum class Type { object, byte, integer, margin };
enum class Direction { input, result, output, reference };
struct Field { std::wstring name; Type type; Direction direction; };
struct Size { double x,y; };
struct Widget {
    std::string text;
    double cached_width=-1;
    std::map<std::wstring,std::vector<Field>> functions;
    Widget* child=nullptr;
    Widget* slot=nullptr;
    std::vector<Widget*> pages;
    std::map<std::wstring,uint8_t> settings;
    std::array<float,4> padding{};
};
struct Frame {
    Widget* target;
    std::wstring fn;
    const std::vector<Field>& fields;
    Widget* child=nullptr;
    uint8_t setting=0;
    int32_t index=0;
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
                        f.type==Type::byte?WidgetParamType::byte_enum:
                        f.type==Type::integer?WidgetParamType::integer:WidgetParamType::margin;
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
    void set(const wchar_t* name,int32_t value) {
        const auto& f=named(name);
        if(f.type!=Type::integer || f.direction!=Direction::input) throw std::runtime_error("wrong page index");
        index=value;
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
    void set_single_bool_input(bool value) {
        if(fields.size()!=1 || fields.front().direction!=Direction::input) throw std::runtime_error("wrong wrap input");
        setting=value;
    }
    template<class T> T get(const wchar_t* name=L"ReturnValue") const {
        if constexpr(std::is_same_v<T,Size>) {
            if(!ran || fn!=L"GetDesiredSize" || target->cached_width<0) throw std::runtime_error("width read before prepass");
            return {target->cached_width,35};
        } else {
            const auto& f=named(name);
            if(!ran || f.type!=Type::object || f.direction!=Direction::result) throw std::runtime_error("wrong slot result");
            return target->slot;
        }
    }
    void run() {
        if(fn==L"SetContent" || fn==L"AddChild" || fn==L"AddChildToHorizontalBox") target->child=child;
        else if(fn==L"ForceLayoutPrepass") {
            target->cached_width=0;
            for(unsigned char c:target->text) if((c&0xc0)!=0x80) target->cached_width+=c=='.'?2:c=='i'?1:6;
        }
        else if(fn==L"HandleTabSwitch") target->child->slot=target->pages.at(index);
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
    for(auto fn:{L"SetTextOverflowPolicy",L"SetClipping",L"SetAutoWrapText"}) value.functions[fn]={{option_name,Type::byte,Direction::input}};
    cssx::engine::fit_information_value<Frame>(&column,&value);
    check(value.settings.at(L"SetAutoWrapText")==0,"word wrapping must be disabled");
    value.functions[L"ForceLayoutPrepass"]={};
    value.functions[L"GetDesiredSize"]={{L"ReturnValue",Type::object,Direction::result}};
    auto write=[](Widget* block,const std::string& text){ block->text=text; block->cached_width=-1; };
    auto label=cssx::engine::display_information<Frame,Size>(column.child,"My five-second camera arc.",70,write);
    check(label.first==value.text && value.text.ends_with("...") && label.second==value.cached_width && label.second<70,
          "the attached native text block must keep the final fitted run after every width probe");
    check(column.child==&value,"SetContent did not receive the value widget");
    check(slot.settings.at(L"SetHorizontalAlignment")==2 && slot.settings.at(L"SetVerticalAlignment")==2,"wrong slot alignment");
    check(value.settings.at(L"SetTextOverflowPolicy")==1 && value.settings.at(L"SetClipping")==1,"wrong overflow or clipping");
}
void check_measured_information() {
    // Proportional mock font. The measure callback receives the combined run including all dots.
    auto measure=[](const std::string& text) {
        double width=0;
        for(unsigned char c:text) if((c&0xc0)!=0x80) width+=c=='.'?2:c=='i'?1:6;
        return width;
    };
    using cssx::engine::fit_information_text;
    check(fit_information_text("iiiiiiii",12,measure)=="iiiiiiii","narrow glyphs fit");
    check(fit_information_text("MMMMMMMM",20,measure)=="MM...","dots must fit inside the column");
    check(fit_information_text("界界界界",14,measure)=="界...","UTF-8 prefix must end on a character boundary");
    check(fit_information_text("ii\nMMMM",20,measure)=="ii...","second line needs a visible marker");
    size_t largest=0;
    auto bounded=[&](const std::string& text){ largest=std::max(largest,text.size()); return measure(text); };
    check(fit_information_text(std::string(100000,'M'),20,bounded)=="MM..." && largest<=131,
          "long labels should only shape the beginning of the line");
    for(auto text:{"My five-second camera arc.","Cycles every palette, then your own look"}) {
        auto displayed=fit_information_text(text,70,measure);
        check(displayed.ends_with("...") && measure(displayed)<70,"live regression must display all three dots");
        check(fit_information_text(text,500,measure)==text,"restore complete label when the column grows");
    }
}
void check_invalid_child(std::vector<Field> fields) {
    Widget parent{},child{};
    parent.functions[L"SetContent"]=fields;
    bool rejected=false;
    try { cssx::engine::set_widget_child<Frame>(&parent,L"SetContent",&child); }
    catch(const std::runtime_error&) { rejected=true; }
    check(rejected && parent.child==nullptr,"invalid signature should fail before the widget changes");
}
void check_owner_selection() {
    Widget panel{}, switcher{}, owner{}, guest{};
    panel.child=&switcher;
    panel.pages={&guest,&owner};
    panel.functions[L"HandleTabSwitch"]={{L"ChosenIndex",Type::integer,Direction::input}};
    switcher.functions[L"GetActiveWidget"]={{L"ReturnValue",Type::object,Direction::result}};
    switcher.slot=&guest;
    check(cssx::engine::select_photo_page<Frame>(&panel,&switcher,&owner,1) && switcher.slot==&owner,
          "removed guest page still active in the switcher");
    switcher.slot=&guest;
    check(!cssx::engine::select_photo_page<Frame>(&panel,&switcher,&owner,0),
          "focus granted before the owner page became active");
    switcher.slot=&owner;
    panel.functions.erase(L"HandleTabSwitch");
    check(cssx::engine::select_photo_page<Frame>(&panel,&switcher,&owner,1),
          "already selected page was reopened");
    Widget viewport{};
    bool ui_focus=false;
    panel.functions[L"TogglePhotomodeFocus"]={{L"bIsInFocus",Type::byte,Direction::input}};
    viewport.functions[L"SetFocusToGameViewport"]={};
    cssx::engine::restore_photo_focus<Frame>(&panel,&viewport,[&]{ ui_focus=true; });
    check(ui_focus && panel.settings.at(L"TogglePhotomodeFocus")==1 && viewport.settings.contains(L"SetFocusToGameViewport"),
          "owner recovery must restore the input gate, menu bindings and viewport focus");
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
        check_owner_selection();
        check_tab_calls();
        check_measured_information();
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

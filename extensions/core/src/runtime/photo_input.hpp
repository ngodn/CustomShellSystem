#pragma once
#include <algorithm>
#include <map>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace cssx {
enum class PhotoInputSource { navigation, arrow_left, arrow_right, button, button_down, button_up, unrelated };

inline PhotoInputSource photo_input_source(std::wstring_view name) {
    if(name==L"HandleNavigableInput") return PhotoInputSource::navigation;
    if(!name.starts_with(L"BndEvt__")) return PhotoInputSource::unrelated;
    if(name.starts_with(L"BndEvt__WBP_NavigationButton_ButtonWidget_")) {
        if(name.ends_with(L"_OnButtonClickedEvent__DelegateSignature")) return PhotoInputSource::button;
        if(name.ends_with(L"_OnButtonPressedEvent__DelegateSignature")) return PhotoInputSource::button_down;
        if(name.ends_with(L"_OnButtonReleasedEvent__DelegateSignature")) return PhotoInputSource::button_up;
    }
    if(name.ends_with(L"_OnClicked__DelegateSignature") || name.ends_with(L"_OnContinuousTrigger__DelegateSignature")) {
        if(name.find(L"_WBP_ArrowButton_L_")!=name.npos) return PhotoInputSource::arrow_left;
        if(name.find(L"_WBP_ArrowButton_R_")!=name.npos) return PhotoInputSource::arrow_right;
    }
    return PhotoInputSource::unrelated;
}

// Optimized bound events sharing one Blueprint graph differ only by its entry parameter.
struct PhotoScriptInput {
    PhotoInputSource direct=PhotoInputSource::unrelated;
    std::map<int32_t,PhotoInputSource> entries;

    PhotoInputSource source(int32_t entry) const {
        if(entries.empty()) return direct;
        const auto found=entries.find(entry);
        return found==entries.end()?PhotoInputSource::unrelated:found->second;
    }
};

struct PhotoInputHook {
    PhotoScriptInput route;
    int offset=-1;
    uint64_t token=0;
};

template<class Function,class Subscribe>
void bind_photo_script(std::map<Function*,PhotoInputHook>& hooks,Function* fn,PhotoInputSource source,
                       int input_offset,Subscribe subscribe) {
    Function* target=fn;
    int32_t entry=-1;
    if(source!=PhotoInputSource::navigation) {
        if(fn->GetNumParms()!=0) throw std::runtime_error("Photo Mode click signature changed");
        if(auto* graph=fn->GetEventGraphFunction()) {
            if(graph->GetParmsSize()!=sizeof(int32_t) || graph->GetNumParms()!=1 || fn->GetEventGraphCallOffset()<0)
                throw std::runtime_error("Photo Mode event graph signature changed");
            target=graph;
            entry=fn->GetEventGraphCallOffset();
            input_offset=0; // ProcessEvent passes the sole int32 entry offset here.
        }
    } else if(input_offset<0) throw std::runtime_error("Photo Mode row input signature changed");
    auto& hook=hooks[target];
    hook.offset=input_offset;
    if(entry>=0) {
        const auto [it,inserted]=hook.route.entries.emplace(entry,source);
        if(!inserted && it->second!=source) throw std::runtime_error("Ambiguous Photo Mode click handler");
    } else hook.route.direct=source;
    if(!hook.token) {
        hook.token=subscribe(target);
        if(!hook.token) throw std::runtime_error("Could not watch Photo Mode row input");
    }
}

inline std::string_view photo_command(PhotoInputSource source,uint8_t input,bool button,bool arrows) {
    switch(source) {
    case PhotoInputSource::button: return button?"accept":"";
    case PhotoInputSource::arrow_left: return arrows?"left":"";
    case PhotoInputSource::arrow_right: return arrows?"right":"";
    case PhotoInputSource::navigation:
        if(input==5 || input==6) return "left";
        if(input==7 || input==8) return "right";
        if(input==13) return "accept";
        break;
    default: break;
    }
    return {};
}

struct PhotoBounds {
    double left,top,right,bottom;
    std::optional<PhotoBounds> clipped(const PhotoBounds& clip) const {
        PhotoBounds r{std::max(left,clip.left),std::max(top,clip.top),std::min(right,clip.right),std::min(bottom,clip.bottom)};
        if(!(r.left<r.right && r.top<r.bottom)) return {};
        return r;
    }
};

struct PhotoGesture {
    uintptr_t widget;
    std::string id;
    std::string command;
    PhotoInputSource source;
    bool matches(uintptr_t current,std::string_view control) const {
        return widget==current && id==control;
    }
};

// CommonAnalogCursor marks its synthetic down only. The click can arrive on a later frame.
class PhotoInputQueue {
    struct VirtualPress {
        uintptr_t widget;
        std::string id;
        bool navigation=false,click=false,released=false;
    };
    std::optional<VirtualPress> press_;
    std::vector<PhotoGesture> queue_;
public:
    void down(uintptr_t widget,const std::string& id,bool simulated) {
        press_.reset();
        if(simulated) {
            bool navigation=false;
            for(const auto& g:queue_) if(g.matches(widget,id) && g.source==PhotoInputSource::navigation && g.command=="accept") navigation=true;
            press_=VirtualPress{widget,id,navigation};
        }
    }
    void up(uintptr_t widget,const std::string& id) {
        if(press_ && press_->widget==widget && press_->id==id) press_->released=true;
    }
    void push(PhotoGesture next) {
        if(press_ && next.command=="accept" && next.matches(press_->widget,press_->id)) {
            if(next.source==PhotoInputSource::button) {
                press_->click=true;
                if(press_->navigation) return;
            } else if(next.source==PhotoInputSource::navigation) {
                press_->navigation=true;
                if(press_->click) return;
            }
        }
        if(queue_.size()<64) queue_.push_back(std::move(next));
    }
    std::vector<PhotoGesture> take() {
        if(press_ && press_->released) press_.reset();
        return std::exchange(queue_,{});
    }
    void clear() { queue_.clear(); press_.reset(); }
};
}

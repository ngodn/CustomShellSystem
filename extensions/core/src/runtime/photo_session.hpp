#pragma once
#include "extensions.hpp"
#include <utility>

namespace cssx {
// Participants survive a trip out to gameplay and a widget rebuild. A runtime restart ends
// their session before unloading their manifests and callback tables.
class PhotoSession {
public:
    struct Step { enum Kind { none, out, in } kind=none; std::string provider, section; };
    void request(const Json& request) {
        const auto caller=request.value("extension",std::string{});
        if(!includes(caller)) throw std::runtime_error("Caller is outside this Photo Mode session");
        const auto op=request.at("op").get<std::string>();
        if(pending_.kind!=Step::none && pending_.provider!=caller && caller!=owner_->id)
            throw std::runtime_error("Another participant already requested a Photo Mode step");
        if(op=="photo.step_out" && state_==State::open) pending_={Step::out,caller,{}};
        else if(op=="photo.step_in" && state_==State::parked) pending_={Step::in,caller,request.value("section",std::string{})};
        else if(op=="photo.step_in" && pending_.kind==Step::out) pending_={};
        else throw std::runtime_error("Cannot run "+op+" in the current Photo Mode state");
    }
    bool has_step() const { return pending_.kind!=Step::none; }
    Step take_step() {
        auto step=std::exchange(pending_,{});
        if(!includes(step.provider)) return {};
        if(step.kind==Step::in) state_=State::returning;
        return step;
    }
    bool includes(const std::string& id) const {
        return owner_ && (owner_->id==id || std::ranges::any_of(guests_,[&](auto* m){ return m->id==id; }));
    }
    void open(Runtime& runtime,const PhotoModeDefinition& mode) {
        const bool resumed=owner_!=nullptr;
        state_=State::open;
        owner_=mode.owner;
        guests_=mode.contributors;
        runtime.photo_notice(*owner_,"open");
        for(auto* guest:guests_) {
            if(resumed) runtime.photo_notice(*guest,"step_in");
            runtime.photo_notice(*guest,"open");
        }
    }
    void park(Runtime& runtime) {
        if(!owner_) return;
        state_=State::parked;
        runtime.photo_notice(*owner_,"parked");
        for(auto* guest:guests_) {
            runtime.photo_notice(*guest,"step_out");
            runtime.photo_notice(*guest,"parked");
        }
    }
    void end(Runtime& runtime,bool detached) {
        if(!owner_) return;
        pending_={};
        state_=State::closed;
        auto* owner=std::exchange(owner_,nullptr);
        auto leaving=std::move(guests_);
        runtime.photo_notice(*owner,"closed");
        for(auto* guest:leaving) runtime.photo_notice(*guest,detached?"detached":"closed");
    }
    void prune(Runtime& runtime,const PhotoModeDefinition& mode) {
        std::erase_if(guests_,[&](auto* guest){
            if(std::ranges::find(mode.contributors,guest)!=mode.contributors.end()) return false;
            runtime.photo_notice(*guest,"detached");
            return true;
        });
    }
private:
    enum class State { closed, open, parked, returning } state_=State::closed;
    Step pending_;
    const Manifest* owner_=nullptr;
    std::vector<const Manifest*> guests_;
};

inline int photo_first_visible(int first,int index,int total,int fit) {
    if(index<first) first=index;
    else if(index>=first+fit) first=index-fit+1;
    return std::clamp(first,0,std::max(0,total-fit));
}
}

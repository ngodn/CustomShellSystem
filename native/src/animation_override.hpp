#pragma once
#include <cmath>
#include <functional>
#include <optional>

namespace css {
enum class AnimationGait { None, Idle, Walk, Jog, Sprint };
inline AnimationGait animation_gait(double speed,std::optional<bool> walking,std::optional<bool> sprinting,bool require_flags) {
    if(!std::isfinite(speed) || speed<0 || (require_flags && (!walking || !sprinting)) ||
       (walking.value_or(false) && sprinting.value_or(false))) return AnimationGait::None;
    if(speed<=20.) return AnimationGait::Idle;
    if(walking.value_or(speed<300.)) return AnimationGait::Walk;
    if(sprinting.value_or(speed>=700.)) return AnimationGait::Sprint;
    return AnimationGait::Jog;
}
// Keep the original pair across CSS-to-CSS changes. A different writer's pair
// becomes the new baseline if CSS takes ownership again.
template<class Object,class Equal=std::equal_to<Object>>
class AnimationOverrideLease {
public:
    struct Value { Object object; bool enabled=false; };
private:
    std::optional<Value> original_,written_;
    static bool same(const Value& a,const Value& b) {
        return a.enabled==b.enabled && Equal{}(a.object,b.object);
    }
public:
    bool owns(const Value& current) const { return written_ && same(*written_,current); }
    const std::optional<Value>& original() const { return original_; }
    bool engaged() const { return written_.has_value(); }
    void claim(const Value& current,const Value& next) {
        if(!owns(current)) original_=current;
        written_=next;
    }
    std::optional<Value> restoration(const Value& current) const {
        return owns(current)?original_:std::nullopt;
    }
    void reset() { original_.reset();written_.reset(); }
};

inline float feminine_walk_speed(float requested,bool enabled,bool player_component,bool game_thread) {
    return enabled && player_component && game_thread && std::isfinite(requested) && std::abs(requested-184.f)<=15.f
        ?85.f:requested;
}
}

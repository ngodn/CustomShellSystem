#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
class SunlightShadows {
    using Json = nlohmann::json;
    struct Light {
        Json target;
        std::optional<float> original;
        bool relinquished=false;
        Json pending_preset;
        std::optional<float> pending_angle;
    };
    std::vector<Light> lights_;
    Json pawn_;
    uint64_t next_check_=0;
    static constexpr float reduced_angle=.5357f;
    static bool same(float a,float b) { return std::abs(a-b)<.00001f; }
public:
    using Query = std::function<Json(const Json&)>;
private:
    static Json get(const Query& query,const Json& target,const char* property) {
        return query({{"op","get"},{"target",target},{"property",property}});
    }
    static float angle(const Query& query,const Json& target) {
        const float value=get(query,target,"LightSourceAngle").get<float>();
        if(!std::isfinite(value) || value<0 || value>180) throw std::runtime_error("Sunlight source angle is invalid");
        return value;
    }
    static void set_angle(const Query& query,const Json& target,float value) {
        query({{"op","call"},{"target",target},{"function","SetLightSourceAngle"},{"args",{{"NewValue",value}}}});
        if(!same(angle(query,target),value)) throw std::runtime_error("Sunlight source angle did not apply");
    }
    static bool valid(const Query& query,const Json& target) {
        return query({{"op","valid"},{"target",target}}).get<bool>();
    }
    static void release(const Query& query,Light& light) {
        if(!light.original) return;
        if(valid(query,light.target) && same(angle(query,light.target),reduced_angle))
            set_angle(query,light.target,*light.original);
        // A later writer owns any different value. Do not overwrite it.
        light.original.reset();
    }
    static void recover_preset(const Query& query,const Json& manager,Light& light) {
        if(!light.relinquished) return;
        const auto timeline=get(query,manager,"SpartaLightingTimeline");
        const auto preset=get(query,manager,"ActiveLightPreset");
        if(timeline.is_null() || preset.is_null() ||
           query({{"op","call"},{"target",timeline},{"function","IsPlaying"},{"args",Json::object()}}).at("ReturnValue").get<bool>()) {
            light.pending_angle.reset(); return;
        }
        const float current=angle(query,light.target);
        const float expected=get(query,preset,"SourceAngle").get<float>();
        if(!std::isfinite(expected) || !same(current,expected) || current<=reduced_angle) {
            light.pending_angle.reset(); return;
        }
        // A completed game preset can replace our value during load/weather changes.
        // Require two unchanged observations; unrelated custom angles stay untouched.
        if(light.pending_preset!=preset || !light.pending_angle || !same(*light.pending_angle,current)) {
            light.pending_preset=preset; light.pending_angle=current; return;
        }
        light.original=current;
        set_angle(query,light.target,reduced_angle);
        light.relinquished=false; light.pending_angle.reset();
    }
public:
    void refresh() { next_check_=0; }
    void restore(const Query& query) {
        // Retain a failed restoration for the next tick or unload retry.
        for(auto& light:lights_) release(query,light);
        lights_.clear(); pawn_=nullptr; next_check_=0;
    }
    void tick(bool enabled,uint64_t now,const Query& query) {
        if(!enabled) { restore(query); return; }
        if(now<next_check_) return;
        next_check_=now+2000;
        const auto pawn=query({{"op","player"}}).at("pawn");
        if(pawn!=pawn_) { restore(query); pawn_=pawn; next_check_=now+2000; }
        if(pawn.is_null()) return;
        for(auto it=lights_.begin();it!=lights_.end();) {
            if(!valid(query,it->target)) { it=lights_.erase(it); continue; }
            if(it->original && !same(angle(query,it->target),reduced_angle)) {
                it->original.reset(); it->relinquished=true;
            }
            if(!get(query,it->target,"bVisible").get<bool>() || get(query,it->target,"bHiddenInGame").get<bool>() ||
               !get(query,it->target,"CastShadows").get<bool>()) {
                release(query,*it); it=lights_.erase(it); continue;
            }
            ++it;
        }
        const auto managers=query({{"op","find"},{"path","/Game/Sparta/Lighting/Blueprints/BP_SpartaLighting.BP_SpartaLighting_C"}});
        if(managers.is_null()) return;
        const auto gameplay=query({{"op","find"},{"path","/Script/Engine.Default__GameplayStatics"}});
        const auto found=query({{"op","call"},{"target",gameplay},{"function","GetAllActorsOfClass"},
            {"args",{{"WorldContextObject",pawn},{"ActorClass",managers}}}}).at("OutActors");
        if(!found.is_array() || found.size()>32) throw std::runtime_error("Sunlight manager list exceeds the supported bound");
        for(const auto& manager:found) {
            if(get(query,manager,"IsMenu").get<bool>()) continue;
            const auto light=get(query,manager,"DirectionalLight");
            if(light.is_null() || light.value("class","")!="Class /Script/Engine.DirectionalLightComponent") continue;
            auto existing=std::find_if(lights_.begin(),lights_.end(),[&](const Light& old){return old.target==light;});
            if(existing!=lights_.end()) { recover_preset(query,manager,*existing); continue; }
            if(!get(query,light,"bVisible").get<bool>() || get(query,light,"bHiddenInGame").get<bool>() ||
               !get(query,light,"CastShadows").get<bool>()) continue;
            const auto signature=query({{"op","describe"},{"target",light},{"function","SetLightSourceAngle"}});
            if(signature!=Json{{"NewValue",{{"out",false},{"return",false},{"size",4}}}})
                throw std::runtime_error("Sunlight setter signature is unsupported");
            const float original=angle(query,light);
            if(original<=reduced_angle) continue;
            if(lights_.size()>=32) throw std::runtime_error("Sunlight component list exceeds the supported bound");
            // Store the original before calling the setter, including a failed readback.
            lights_.push_back({light,original,false,Json(),std::nullopt});
            set_angle(query,light,reduced_angle);
        }
    }
    Json diagnostics() const {
        size_t owned=0,relinquished=0;
        for(const auto& light:lights_) { owned+=light.original.has_value(); relinquished+=light.relinquished; }
        return {{"modified_lights",owned},{"relinquished_lights",relinquished}};
    }
};
}

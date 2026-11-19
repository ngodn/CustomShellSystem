#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace css {
inline constexpr std::array<std::wstring_view,2> arm_ik_nodes{L"AnimGraphNode_TwoBoneIK_1",L"AnimGraphNode_TwoBoneIK"};
struct ArmStretchValues {
    bool allow;
    double maximum;
    bool operator==(const ArmStretchValues&) const = default;
};
using ArmStretchPair=std::array<ArmStretchValues,2>;
inline constexpr ArmStretchValues locked_arm_stretch{false,1.0};
inline constexpr ArmStretchPair locked_arm_pair{locked_arm_stretch,locked_arm_stretch};

class ArmStretchLease {
    std::optional<ArmStretchPair> original_;
public:
    bool bound() const { return original_.has_value(); }
    void lock(const ArmStretchPair& current) {
        if(!original_) original_=current;
        else {
            // A reinit or another writer may replace either setting while Locked.
            for(size_t i=0;i<current.size();++i) {
                if(current[i].allow) (*original_)[i].allow=true;
                if(current[i].maximum!=1.0) (*original_)[i].maximum=current[i].maximum;
            }
        }
    }
    ArmStretchPair restoration(ArmStretchPair current) const {
        if(original_) for(size_t i=0;i<current.size();++i) {
            if(!current[i].allow) current[i].allow=(*original_)[i].allow;
            if(current[i].maximum==1.0) current[i].maximum=(*original_)[i].maximum;
        }
        return current;
    }
    void reset() { original_.reset(); }
};
}

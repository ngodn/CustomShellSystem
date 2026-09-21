#pragma once
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>

namespace css {
inline constexpr const char* feminine_animation_id = "css.feminine";
enum class AnimationSlot { Idle, Walk, Jog, Sprint, Beacon };
const char* animation_slot_name(AnimationSlot);
std::optional<AnimationSlot> animation_slot_from_name(std::string_view);

struct AnimationOption {
    std::string id, name;
    // Idle sequences, or a movement BlendSpace. Beacon keeps two sequences so
    // the runtime can preserve the game's montage events and cancellation.
    std::string clip, blend_space, depart, arrive;
    std::map<std::string,std::string> by_weapon;
    bool hide_weapons = false;
};
struct AnimationSet {
    // A present empty slot explicitly disables inherited outfit options.
    std::map<AnimationSlot,std::vector<AnimationOption>> slots;
    static AnimationSet parse(const nlohmann::json&);
};
struct ResolvedAnimation {
    const AnimationOption* option = nullptr;
    std::string asset;
    bool hide_weapons = false;
};
ResolvedAnimation resolve_animation(const std::vector<AnimationOption>&, AnimationSlot,
    const std::string& choice, const std::string& weapon_tag = {});

struct AnimationMenuItem {
    std::string id, name;
    bool available = true;
};
struct AnimationMenu {
    std::vector<AnimationMenuItem> items;
    size_t selected = 0;
    std::string step(int direction) const;
};
// Missing saved options remain visible, but cycling only visits usable choices.
AnimationMenu animation_menu(const std::vector<AnimationOption>&, AnimationSlot,
    const std::string* saved_choice, bool legacy_feminine);

// Choices belong to an outfit and variant, not whichever gameplay shell is
// currently wearing them. Keep missing-mod IDs for later reinstall/updates.
struct AnimationChoices {
    using Slots = std::map<AnimationSlot,std::string>;
    using Variants = std::map<std::string,Slots>;
    std::map<std::string,Variants> outfits;
    static AnimationChoices parse(const nlohmann::json&);
    nlohmann::json json() const;
    const std::string* find(const std::string& outfit, const std::string& variant, AnimationSlot) const;
    std::string get(const std::string& outfit, const std::string& variant, AnimationSlot) const;
    bool operator==(const AnimationChoices&) const = default;
};
}

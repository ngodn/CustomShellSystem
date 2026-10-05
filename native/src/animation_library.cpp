#include "animation_library.hpp"
#include "data.hpp"
#include "skeleton_compatibility.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>

namespace css {
namespace {
void fields(const Json& value, const std::set<std::string>& allowed) {
    if(!value.is_object()) throw std::runtime_error("Animation library entry must be an object");
    for(const auto& [key,_]:value.items()) if(!allowed.contains(key))
        throw std::runtime_error("Unknown animation library field: "+key);
}
std::string text(const Json& value,const char* key,size_t limit) {
    auto result=value.at(key).get<std::string>();
    if(result.empty() || result.size()>limit ||
       std::any_of(result.begin(),result.end(),[](unsigned char c){return c<32 || c==127;}))
        throw std::runtime_error(std::string("Invalid animation library text: ")+key);
    return result;
}
std::string identity(const Json& value) {
    auto id=text(value,"id",96);
    if(!valid_id(id) || id=="original" || id==feminine_animation_id)
        throw std::runtime_error("Invalid animation library identity");
    return id;
}
std::string asset(const Json& value,const char* key) {
    auto result=value.at(key).get<std::string>();
    if(!valid_asset(result)) throw std::runtime_error("Invalid animation library asset");
    return result;
}
AnimationSlot slot(const Json& value) {
    const auto result=animation_slot_from_name(value.get<std::string>());
    if(!result || *result==AnimationSlot::Beacon)
        throw std::runtime_error("Unsupported shared animation slot");
    return *result;
}
}
AnimationLibrary AnimationLibrary::parse(const Json& value) {
    fields(value,{"id","name","author","version","entries"});
    AnimationLibrary library;
    library.id=identity(value);
    library.name=text(value,"name",96);
    library.author=text(value,"author",96);
    library.version=text(value,"version",32);
    const auto& entries=value.at("entries");
    if(!entries.is_array() || entries.empty() || entries.size()>256)
        throw std::runtime_error("Animation library needs 1 to 256 entries");
    std::set<std::string> ids;
    std::set<std::tuple<AnimationSlot,std::string,std::string>> aliases;
    library.entries.reserve(entries.size());
    for(const auto& entry:entries) {
        fields(entry,{"id","name","category","playback","slot","hide_weapons","assets","aliases"});
        LibraryAnimation animation;
        animation.id=identity(entry);
        if(!ids.insert(animation.id).second) throw std::runtime_error("Duplicate shared animation identity");
        animation.name=text(entry,"name",96);
        animation.category=text(entry,"category",48);
        if(!valid_id(animation.category)) throw std::runtime_error("Invalid animation category");
        const auto playback=text(entry,"playback",16);
        if(playback=="loop") animation.playback=AnimationPlayback::Loop;
        else if(playback=="once") animation.playback=AnimationPlayback::Once;
        else if(playback=="pose") animation.playback=AnimationPlayback::Pose;
        else throw std::runtime_error("Invalid shared animation playback");
        if(entry.contains("slot")) animation.slot=slot(entry.at("slot"));
        animation.hide_weapons=entry.value("hide_weapons",false);
        if(animation.slot && *animation.slot!=AnimationSlot::Idle &&
           (animation.playback!=AnimationPlayback::Loop || animation.hide_weapons))
            throw std::runtime_error("Movement requires looping playback with weapons preserved");
        if(animation.slot==AnimationSlot::Idle && animation.playback==AnimationPlayback::Once)
            throw std::runtime_error("One-shot performance cannot occupy an idle slot");
        const auto& assets=entry.at("assets");
        if(!assets.is_array() || assets.empty() || assets.size()>16)
            throw std::runtime_error("Invalid shared animation asset alternatives");
        std::set<std::string> skeletons;
        for(const auto& item:assets) {
            fields(item,{"asset","skeleton"});
            LibraryAnimationAsset alternative{asset(item,"asset"),asset(item,"skeleton")};
            if(!alternative.asset.starts_with("/Game/CSS/Locomotion/") ||
               !skeletons.insert(alternative.skeleton).second)
                throw std::runtime_error("Shared assets need unique skeleton alternatives under /Game/CSS/Locomotion/");
            animation.assets.push_back(std::move(alternative));
        }
        if(entry.contains("aliases")) {
            const auto& list=entry.at("aliases");
            if(!list.is_array() || list.size()>64) throw std::runtime_error("Invalid legacy animation aliases");
            for(const auto& item:list) {
                fields(item,{"slot","id","asset"});
                LegacyAnimationAlias alias{slot(item.at("slot")),identity(item),asset(item,"asset")};
                if(!animation.slot || alias.slot!=*animation.slot ||
                   !aliases.emplace(alias.slot,alias.id,alias.asset).second)
                    throw std::runtime_error("Conflicting legacy animation alias");
                animation.aliases.push_back(std::move(alias));
            }
        }
        library.entries.push_back(std::move(animation));
    }
    return library;
}
std::vector<AnimationOption> merge_library_animation_options(const std::vector<AnimationLibrary>& libraries,
    AnimationSlot slot,const std::vector<AnimationOption>& bundled) {
    auto result=bundled;
    for(const auto& library:libraries) for(const auto& entry:library.entries) {
        if(entry.slot!=slot) continue;
        std::vector<size_t> matches;
        for(size_t i=0;i<result.size();++i) {
            const auto old=resolve_animation(bundled,slot,result[i].id);
            if(old.option && old.hide_weapons==entry.hide_weapons &&
               std::any_of(entry.aliases.begin(),entry.aliases.end(),[&](const auto& alias) {
                   return alias.slot==slot && alias.id==old.option->id && alias.asset==old.asset;
               })) matches.push_back(i);
        }
        if(matches.empty()) {
            // A specialized outfit option owns a colliding ID until explicitly aliased.
            if(std::any_of(result.begin(),result.end(),[&](const auto& option){return animation_choice_matches(option,entry.id);})) continue;
            AnimationOption option;
            option.id=entry.id;option.name=entry.name;option.library_identity=entry.id;
            option.hide_weapons=entry.hide_weapons;
            // The engine resolver chooses and validates an asset before playback.
            if(slot==AnimationSlot::Idle) option.clip=entry.assets.front().asset;
            else option.blend_space=entry.assets.front().asset;
            result.push_back(std::move(option));
        } else {
            auto& primary=result[matches.front()];
            primary.library_identity=entry.id;
            if(primary.id!=entry.id) primary.saved_aliases.push_back(entry.id);
            for(size_t i=1;i<matches.size();++i) primary.saved_aliases.push_back(result[matches[i]].id);
            for(size_t i=matches.size();i>1;--i) result.erase(result.begin()+static_cast<std::ptrdiff_t>(matches[i-1]));
        }
    }
    return result;
}
AnimationLibraryMatch resolve_library_animation(const std::vector<AnimationLibrary>& libraries,
    AnimationSlot requested_slot,const std::string& choice,const std::vector<AnimationOption>& bundled,
    const std::string& target_skeleton,const std::string& weapon) {
    if(choice.empty() || choice=="original" || choice==feminine_animation_id) return {};
    const auto legacy=resolve_animation(bundled,requested_slot,choice,weapon);
    const auto aliases=[&](const LibraryAnimation& entry,const LegacyAnimationAlias& alias,
                          const ResolvedAnimation& option) {
        return option.option && alias.slot==requested_slot && alias.id==option.option->id &&
               alias.asset==option.asset && option.hide_weapons==entry.hide_weapons;
    };
    const LibraryAnimation* found=nullptr;
    for(const auto& library:libraries) for(const auto& entry:library.entries) {
        if(entry.slot!=requested_slot) continue;
        const bool matched_alias=std::any_of(entry.aliases.begin(),entry.aliases.end(),
            [&](const auto& alias){return aliases(entry,alias,legacy);});
        // An existing unaliased option owns its saved ID, even if another pack
        // declares that same string as a canonical library ID.
        if(!matched_alias && (entry.id!=choice || legacy.option)) continue;
        if(found) return {};
        found=&entry;
    }
    if(!found) return {};
    AnimationLibraryMatch result{found,{}};
    std::set<std::string> paths;
    auto append=[&](const std::string& path,const std::string& skeleton,bool bundled_asset) {
        if(paths.insert(path).second) result.candidates.push_back({path,skeleton,bundled_asset});
    };
    // Prefer an exact target rig over an audited compatible base alternative.
    for(bool exact:{true,false}) for(const auto& alternative:found->assets) {
        if((alternative.skeleton==target_skeleton)==exact &&
           compatible_animation_skeletons(alternative.skeleton,target_skeleton))
            append(alternative.asset,alternative.skeleton,false);
    }
    for(const auto& alias:found->aliases) {
        const auto option=resolve_animation(bundled,requested_slot,alias.id,weapon);
        if(aliases(*found,alias,option)) append(option.asset,{},true);
    }
    return result;
}
}

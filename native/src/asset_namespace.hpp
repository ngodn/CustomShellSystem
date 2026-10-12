#pragma once
#include <algorithm>
#include <string_view>

namespace css {
inline bool asset_namespace_component(std::string_view component) {
    return !component.empty() && std::all_of(component.begin(),component.end(),[](char c) {
        return (c>='a' && c<='z') || (c>='A' && c<='Z') ||
               (c>='0' && c<='9') || c=='_';
    });
}

// The caller validates the complete Unreal object path before checking ownership.
inline std::string_view authored_css_project_path(std::string_view asset) {
    constexpr std::string_view root="/Game/CSS/";
    if(!asset.starts_with(root)) return {};
    asset.remove_prefix(root.size());
    const auto author_end=asset.find('/');
    if(author_end==asset.npos || !asset_namespace_component(asset.substr(0,author_end))) return {};
    asset.remove_prefix(author_end+1);
    const auto project_end=asset.find('/');
    if(project_end==asset.npos || !asset_namespace_component(asset.substr(0,project_end))) return {};
    return asset;
}

inline bool shared_locomotion_asset(std::string_view asset,std::string_view folder={}) {
    constexpr std::string_view legacy="/Game/CSS/Locomotion/";
    constexpr std::string_view authored="Shared/Locomotion/";
    if(asset.starts_with(legacy)) asset.remove_prefix(legacy.size());
    else {
        asset=authored_css_project_path(asset);
        if(!asset.starts_with(authored)) return false;
        asset.remove_prefix(authored.size());
    }
    return asset.starts_with(folder) && asset.size()>folder.size();
}

inline bool facial_adapter_asset(std::string_view asset) {
    constexpr std::string_view legacy="/Game/CSS/Locomotion/Faces/";
    if(asset.starts_with(legacy)) return asset.size()>legacy.size();
    asset=authored_css_project_path(asset);
    if(asset.empty()) return false;
    // Facial replacements may be grouped with their outfit's other meshes.
    // The catalog pins the source revision; runtime checks the mesh and rig.
    asset.remove_prefix(asset.find('/')+1);
    return !asset.empty();
}

inline bool playback_adapter_asset(std::string_view asset) {
    if(shared_locomotion_asset(asset,"Adapters/")) return true;
    asset=authored_css_project_path(asset);
    if(asset.empty()) return false;
    asset.remove_prefix(asset.find('/')+1);
    constexpr std::string_view folder="Animations/Adapters/";
    return asset.starts_with(folder) && asset.size()>folder.size();
}
}

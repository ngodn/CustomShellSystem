#pragma once
#include <filesystem>
#include <string>

namespace css {
inline std::filesystem::path wardrobe_packages(const std::filesystem::path& executable,
                                                const std::filesystem::path& engine_content = {}) {
    std::error_code error;
    if(engine_content.is_absolute() && std::filesystem::is_directory(engine_content/"Paks",error))
        return (engine_content/"Paks").lexically_normal();
    // Unreal searches Content/Paks recursively, including ~mods and its siblings.
    // Resolve from the game, independently of UE4SS's ModsFolderPath setting.
    for(auto parent=executable.parent_path(); !parent.empty();) {
        auto paks=parent/"Content/Paks";
        if(std::filesystem::is_directory(paks,error)) return paks;
        auto next=parent.parent_path();
        if(next==parent) break;
        parent=next;
    }
    return executable.parent_path().parent_path().parent_path()/"Content/Paks";
}
inline std::string wardrobe_startup_message(size_t outfits) {
    return outfits?"Choose an appearance to wear it.":"No CSS outfit packages found. Install an outfit package, then restart the game.";
}
}

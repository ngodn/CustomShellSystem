#pragma once
// Outfit materials that borrow the game's shaders (instances of a game material carrying their own
// shared shader map) draw grey on a game build that lacks those exact shaders. catalog/shader-donors.json
// pairs each such material with the game material that compiles to the same permutation. On a game
// build CSS has not verified, the engine side rebuilds those materials as dynamic instances of their
// donor (whose shaders always ship with the player's own game) and copies our parameter values on top.
// This header holds the engine-free part: the catalog, game build recognition and the mode.
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace css::shader_fallback {

enum class Mode { automatic, always, off };

inline std::optional<Mode> parse_mode(std::string_view text) {
    if(text=="auto") return Mode::automatic;
    if(text=="always") return Mode::always;
    if(text=="off") return Mode::off;
    return std::nullopt;
}
inline const char* mode_name(Mode mode) {
    return mode==Mode::always?"always":mode==Mode::off?"off":"auto";
}

// Sizes of the game's own IoStore tables. Every game patch so far changed at least one of them,
// and reading sizes costs a directory listing, never a file read.
using PakTables=std::map<std::string,std::uintmax_t,std::less<>>;

inline bool is_game_table(std::string_view name) {
    if(name=="global.utoc") return true;
    constexpr std::string_view head="pakchunk",tail="-Windows.utoc";
    if(name.size()<=head.size()+tail.size() || !name.starts_with(head) || !name.ends_with(tail)) return false;
    const auto digits=name.substr(head.size(),name.size()-head.size()-tail.size());
    if(digits.empty() || digits.size()>4) return false;
    for(char c:digits) if(c<'0' || c>'9') return false;
    return true;
}

inline PakTables read_pak_tables(const std::filesystem::path& paks) {
    PakTables result;
    std::error_code error;
    std::filesystem::directory_iterator it(paks,error),end;
    if(error) return result;
    for(;it!=end;it.increment(error)) {
        if(error) return {};
        const auto name=it->path().filename().string();
        if(!is_game_table(name)) continue;
        const auto size=it->file_size(error);
        if(error) return {};
        result.emplace(name,size);
    }
    return result;
}

struct VerifiedBuild { std::string steam_build; PakTables tables; };

inline bool valid_object_path(std::string_view path) {
    if(!path.starts_with("/Game/") || path.size()>512) return false;
    const auto dot=path.rfind('.');
    const auto slash=path.rfind('/');
    if(dot==std::string_view::npos || dot<slash || dot+1==path.size()) return false;
    for(unsigned char c:path) if(c<0x20 || c==0x7f || c==':' || c=='\\') return false;
    return path.substr(slash+1,dot-slash-1)==path.substr(dot+1);
}

class Catalog {
    std::unordered_map<std::string,std::string> donors_;
    std::vector<VerifiedBuild> builds_;
    // Released outfit versions whose packaged shaders are missing on the verified builds above
    // (they failed the shader audit there). Players who never updated them still need the fallback.
    std::map<std::string,std::set<std::string>,std::less<>> stale_;
public:
    static Catalog parse(const nlohmann::json& json) {
        if(!json.is_object() || json.value("schema",0)!=1) throw std::runtime_error("Unsupported shader donor schema");
        Catalog result;
        const auto& builds=json.at("verified_builds");
        if(!builds.is_array() || builds.size()>64) throw std::runtime_error("Invalid verified build list");
        for(const auto& build:builds) {
            VerifiedBuild entry{build.at("steam_build").get<std::string>(),{}};
            const auto& tables=build.at("pak_tables");
            if(!tables.is_object() || tables.empty() || tables.size()>64) throw std::runtime_error("Invalid verified pak tables");
            for(const auto& [name,size]:tables.items()) {
                if(!is_game_table(name) || !size.is_number_integer() || size.get<std::int64_t>()<0)
                    throw std::runtime_error("Invalid verified pak table: "+name);
                entry.tables.emplace(name,static_cast<std::uintmax_t>(size.get<std::int64_t>()));
            }
            result.builds_.push_back(std::move(entry));
        }
        if(const auto stale=json.find("stale_outfits"); stale!=json.end()) {
            if(!stale->is_object() || stale->size()>256) throw std::runtime_error("Invalid stale outfit list");
            for(const auto& [id,versions]:stale->items()) {
                if(id.empty() || id.size()>96 || !versions.is_array() || versions.empty() || versions.size()>64)
                    throw std::runtime_error("Invalid stale outfit entry: "+id);
                auto& set=result.stale_[id];
                for(const auto& version:versions) {
                    if(!version.is_string() || version.get_ref<const std::string&>().empty() || version.get_ref<const std::string&>().size()>32)
                        throw std::runtime_error("Invalid stale outfit version for "+id);
                    set.insert(version.get<std::string>());
                }
            }
        }
        const auto& donors=json.at("donors");
        if(!donors.is_object() || donors.size()>20000) throw std::runtime_error("Invalid shader donor table");
        for(const auto& [material,donor]:donors.items()) {
            if(!donor.is_string()) throw std::runtime_error("Invalid shader donor for "+material);
            const auto& value=donor.get_ref<const std::string&>();
            if(!valid_object_path(material) || !valid_object_path(value) || material==value)
                throw std::runtime_error("Invalid shader donor pair: "+material);
            result.donors_.emplace(material,value);
        }
        return result;
    }
    // Empty tables (an unreadable folder) are never a verified build.
    std::optional<std::string> verified_build(const PakTables& tables) const {
        if(tables.empty()) return std::nullopt;
        for(const auto& build:builds_) if(build.tables==tables) return build.steam_build;
        return std::nullopt;
    }
    const std::string* donor(std::string_view material) const {
        const auto found=donors_.find(std::string(material));
        return found==donors_.end()?nullptr:&found->second;
    }
    size_t size() const { return donors_.size(); }
    bool stale(std::string_view outfit,std::string_view version) const {
        const auto found=stale_.find(outfit);
        return found!=stale_.end() && found->second.contains(std::string(version));
    }
};

// Auto rebuilds only on a game CSS has not verified, so a verified game draws exactly what it drew
// before this feature existed.
inline bool active(Mode mode,bool verified,bool has_donors) {
    if(!has_donors) return false;
    return mode==Mode::always || (mode==Mode::automatic && !verified);
}
// Auto also rebuilds a worn outfit whose installed version is known to lack its shaders on this
// verified game, so a player who has not updated that outfit still sees it textured.
inline bool active_for_outfit(Mode mode,bool verified,bool has_donors,bool stale_outfit) {
    return active(mode,verified,has_donors) || (has_donors && mode==Mode::automatic && stale_outfit);
}
}

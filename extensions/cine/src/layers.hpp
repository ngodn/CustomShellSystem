#pragma once
// The breakdown's vocabulary, with no engine or host dependency: layer names as written in
// presets, bone classification by name, and the one bit of rotation maths a bone stick needs.
// Pure, so presets and tests use it without the rest of the breakdown.
#include "shot.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

enum class Layer : uint8_t {
    Clay, Wireframe, Skeleton, VirtualBones, IkBones, Springs, Sockets, Collision, Capsule,
    BaseColor, Normals, Roughness, Metallic, Depth, Occlusion, Lighting,
    Count
};
using LayerSet = uint32_t;
constexpr LayerSet bit(Layer l) { return LayerSet{1} << static_cast<unsigned>(l); }
constexpr LayerSet frame_layers = bit(Layer::BaseColor) | bit(Layer::Normals) | bit(Layer::Roughness) | bit(Layer::Metallic)
                                | bit(Layer::Depth) | bit(Layer::Occlusion) | bit(Layer::Lighting);
constexpr LayerSet bone_layers = bit(Layer::Skeleton) | bit(Layer::VirtualBones) | bit(Layer::IkBones) | bit(Layer::Springs) | bit(Layer::Sockets);
const char* layer_id(Layer);                       // as written in presets: "wireframe", "base_color"...
const char* layer_label(Layer);                    // for the caption: "Wireframe", "Base colour"...
std::optional<Layer> layer_from(const std::string& id);
// Preset form of a set: ["clay", "wireframe"]. Throws std::invalid_argument naming a bad id.
LayerSet layers_from(const Json& array);

// How a bone is drawn, decided from its name alone (pure, tested).
enum class BoneKind { Regular, Virtual, Ik, Spring };
BoneKind classify_bone(const std::string& name, const std::vector<std::string>& spring_bones);
// Default spring bones by name, for outfits that follow the usual naming (chest, glutes, belly).
bool spring_by_name(const std::string& bone);
// "chest", "glutes", "belly" or "" for a spring bone name: used to group the callouts.
std::string spring_group(const std::string& bone);
// Unit quaternion (x, y, z, w) turning +Z onto `direction` by the shortest arc.
struct Quat { double x = 0, y = 0, z = 0, w = 1; };
Quat z_to(Vec3 direction);

} // namespace cine

#include "breakdown.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cine {
namespace {
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
uint64_t id_of(const Json& j) { return is_handle(j) ? j["$object"].get<uint64_t>() : 0; }
std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); }); return s; }

constexpr const char* asset_root = "/Game/CSSX/CINE/";
constexpr const char* path_collection = "/Game/CSSX/CINE/MPC_Cine.MPC_Cine";
constexpr const char* path_cone = "/Engine/BasicShapes/Cone.Cone";
constexpr const char* path_sphere = "/Engine/BasicShapes/Sphere.Sphere";
constexpr size_t max_bones = 1200;      // the game's widest skeleton is 1199 bones
constexpr size_t max_sockets = 128;
constexpr size_t max_bodies = 160;
constexpr size_t max_skins = 40;
constexpr size_t max_worn_actors = 12;
// A stick longer than any limb is an offset to a helper bone (camera, prop, interaction
// points), not anatomy: drawn it is a spike across the picture.
constexpr double max_stick_cm = 70;
constexpr double clay_ambient = 0.22;   // clay stays readable in a dark scene
constexpr int keep_relative = 0, snap_to_target = 2, no_collision = 0;

struct LayerInfo { const char* id; const char* label; Vec3 colour; };
constexpr LayerInfo info[static_cast<size_t>(Layer::Count)] = {
    {"clay", "Clay", {0.42, 0.42, 0.42}},
    {"wireframe", "Wireframe", {0.05, 0.85, 1.0}},
    {"skeleton", "Skeleton", {1.0, 0.86, 0.25}},
    {"virtual_bones", "Virtual bones", {1.0, 0.25, 0.9}},
    {"ik_bones", "IK bones", {0.25, 1.0, 0.4}},
    {"springs", "Spring bones", {1.0, 0.45, 0.1}},
    {"sockets", "Sockets", {0.3, 0.75, 1.0}},
    {"collision", "Physics bodies", {0.0, 1.0, 1.0}},
    {"capsule", "Movement capsule", {1.0, 0.55, 0.65}},
    {"base_color", "Base colour", {}},
    {"normals", "Normals", {}},
    {"roughness", "Roughness", {}},
    {"metallic", "Metallic", {}},
    {"depth", "Depth", {}},
    {"occlusion", "Ambient occlusion", {}},
    {"lighting", "Lighting", {}},
};
// The frame-pass material's weight parameter for each frame layer.
const char* pass_weight(Layer l) {
    switch (l) {
        case Layer::BaseColor: return "W_BaseColor";
        case Layer::Normals: return "W_Normal";
        case Layer::Roughness: return "W_Roughness";
        case Layer::Metallic: return "W_Metallic";
        case Layer::Depth: return "W_Depth";
        case Layer::Occlusion: return "W_AO";
        default: return nullptr;   // Lighting has no weight in the after-tonemapping material
    }
}
std::optional<Layer> first_frame_layer(LayerSet set) {
    for (auto l : {Layer::BaseColor, Layer::Normals, Layer::Roughness, Layer::Metallic, Layer::Depth, Layer::Occlusion})
        if (set & bit(l)) return l;
    return std::nullopt;
}
Json vec(Vec3 v) { return {{"X", v.x}, {"Y", v.y}, {"Z", v.z}}; }
Json transform(Vec3 at, Quat q, Vec3 scale) {
    return {{"Rotation", {{"X", q.x}, {"Y", q.y}, {"Z", q.z}, {"W", q.w}}}, {"Translation", vec(at)}, {"Scale3D", vec(scale)}};
}
Json colour8(Vec3 c) { return {{"R", int(c.x * 255)}, {"G", int(c.y * 255)}, {"B", int(c.z * 255)}, {"A", 255}}; }
}

// ---------------------------------------------------------------- pure helpers

const char* layer_id(Layer l) { return info[static_cast<size_t>(l)].id; }
const char* layer_label(Layer l) { return info[static_cast<size_t>(l)].label; }
std::optional<Layer> layer_from(const std::string& id) {
    for (size_t i = 0; i < static_cast<size_t>(Layer::Count); ++i) if (id == info[i].id) return static_cast<Layer>(i);
    return std::nullopt;
}
LayerSet layers_from(const Json& array) {
    if (!array.is_array() || array.size() > static_cast<size_t>(Layer::Count)) throw std::invalid_argument("layers must be a short list of layer names");
    LayerSet set = 0;
    for (const auto& item : array) {
        if (!item.is_string()) throw std::invalid_argument("a layer must be a name");
        const auto layer = layer_from(item.get<std::string>());
        if (!layer) throw std::invalid_argument("unknown layer \"" + item.get<std::string>() + "\"");
        set |= bit(*layer);
    }
    return set;
}
bool spring_by_name(const std::string& bone) { return !spring_group(bone).empty(); }
std::string spring_group(const std::string& bone) {
    const auto n = lower(bone);
    if (n.starts_with("brust") || n.starts_with("breast")) return "chest";
    if (n.starts_with("butt") || n.starts_with("glute")) return "glutes";
    if (n.starts_with("belly")) return "belly";
    return {};
}
BoneKind classify_bone(const std::string& name, const std::vector<std::string>& spring_bones) {
    if (name.starts_with("VB ")) return BoneKind::Virtual;
    const auto n = lower(name);
    if (n.starts_with("ik_")) return BoneKind::Ik;
    if (spring_bones.empty() ? spring_by_name(name)
                             : std::any_of(spring_bones.begin(), spring_bones.end(), [&](const std::string& s) { return lower(s) == n; }))
        return BoneKind::Spring;
    return BoneKind::Regular;
}
Quat z_to(Vec3 d) {
    const double length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (!(length > 1e-9)) return {};
    d = d * (1.0 / length);
    // Shortest arc from (0,0,1): axis = z x d, w = 1 + z.d. Exactly straight down has no unique
    // axis; any horizontal one is a half turn.
    if (1 + d.z < 1e-12) return {1, 0, 0, 0};
    Quat q{-d.y, d.x, 0, 1 + d.z};
    const double n = std::sqrt(q.x * q.x + q.y * q.y + q.w * q.w);
    return {q.x / n, q.y / n, 0, q.w / n};
}

// ---------------------------------------------------------------- bridge helpers

Json Breakdown::call(const Json& target, const std::string& function, const Json& args) const { return host_.call(target, function, args); }
bool Breakdown::valid(const Json& object) const {
    try { return is_handle(object) && host_.request({{"op", "valid"}, {"target", object}}) == true; } catch (...) { return false; }
}
Json Breakdown::load(const std::string& path) const {
    try { auto o = host_.request({{"op", "load"}, {"path", path}}); return is_handle(o) ? o : Json(); } catch (...) { return Json(); }
}
double Breakdown::game_time() const {
    const auto t = call(statics_, "GetTimeSeconds", {{"WorldContextObject", pawn_}});
    return t.is_number() ? t.get<double>() : 0.0;
}
void Breakdown::add_note(const std::string& text) {
    if (note_.find(text) != std::string::npos) return;
    note_ += note_.empty() ? text : "  " + text;
    try { host_.log("Breakdown: " + text, "warning"); } catch (...) {}
}
void Breakdown::set_param(const Json& mid, const char* name, double value) {
    if (is_handle(mid)) call(mid, "SetScalarParameterValue", {{"ParameterName", name}, {"Value", value}});
}
// The wipe every CINE material follows: position goes from `from` to `to` over `seconds` of
// game time starting now. Written once; the GPU animates it.
void Breakdown::wipe_numbers(double from, double to, double seconds) {
    if (!pak_) return;
    const auto collection = load(path_collection);   // reloaded each time: a bare asset handle can be collected
    if (!is_handle(collection)) return;
    const double now = game_time();
    for (const auto& [name, value] : {std::pair<const char*, double>{"WipeFrom", from}, {"WipeTo", to}, {"WipeDur", std::max(0.01, seconds)}, {"WipeT0", now}})
        call(kismet_materials_, "SetScalarParameterValue",
             {{"WorldContextObject", pawn_}, {"Collection", collection}, {"ParameterName", name}, {"ParameterValue", value}});
}
Json Breakdown::instance(const char* key, Vec3 colour, double opacity) {
    const auto parent = load(std::string(asset_root) + "Materials/" + key + "." + key);
    if (!is_handle(parent)) return Json();
    auto mid = call(kismet_materials_, "CreateDynamicMaterialInstance",
                    {{"WorldContextObject", pawn_}, {"Parent", parent}, {"OptionalName", "None"}, {"CreationFlags", 0}});
    if (!is_handle(mid)) return Json();
    for (const char* name : {"Color", "BaseColor"}) {
        try { call(mid, "SetVectorParameterValue", {{"ParameterName", name}, {"Value", {{"R", colour.x}, {"G", colour.y}, {"B", colour.z}, {"A", 1.0}}}}); } catch (...) {}
    }
    if (opacity >= 0) set_param(mid, "Opacity", opacity);
    return mid;
}
// A bone layer's shared instance. Hidden until its layer is shown: the caller assigns it to a
// component in the same tick, which is what keeps it alive.
Json Breakdown::material(Layer layer) {
    auto& mid = mids_[static_cast<size_t>(layer)];
    if (is_handle(mid)) return mid;
    mid = instance("M_Cine_XRay", info[static_cast<size_t>(layer)].colour, 0.9);
    if (is_handle(mid) && !(shown_ & bit(layer))) set_param(mid, "Show", 0.0);
    return mid;
}
Json Breakdown::add_component(const Json& cls, const Json& xf) {
    auto component = call(pawn_, "AddComponentByClass", {{"Class", cls}, {"bManualAttachment", true}, {"RelativeTransform", xf}, {"bDeferredFinish", false}});
    if (!is_handle(component)) throw std::runtime_error("could not create a component");
    call(component, "SetCollisionEnabled", {{"NewType", no_collision}});   // before it has any geometry
    return component;
}

// ---------------------------------------------------------------- begin and build

void Breakdown::begin(const Json& pawn, const Json& camera, LayerSet needed, std::vector<std::string> spring_bones) {
    if (active_) end();
    forget();
    repaint_ = false;
    if (!is_handle(pawn) || needed == 0) return;
    try {
        pawn_ = pawn; camera_ = camera; needed_ = needed; spring_names_ = std::move(spring_bones);
        mesh_ = host_.get(pawn_, "Mesh");
        capsule_ = host_.get(pawn_, "CapsuleComponent");
        statics_ = host_.find("/Script/Engine.Default__GameplayStatics");
        kismet_materials_ = host_.find("/Script/Engine.Default__KismetMaterialLibrary");
        class_mesh_ = load("/Script/Engine.StaticMeshComponent"); class_capsule_ = load("/Script/Engine.CapsuleComponent");
        class_sphere_ = load("/Script/Engine.SphereComponent"); class_box_ = load("/Script/Engine.BoxComponent");
        if (!is_handle(mesh_) || !is_handle(statics_) || !is_handle(kismet_materials_)) { add_note("This character has no mesh to break down."); return; }
        active_ = true;
        pak_ = is_handle(load(path_collection));
        if (!pak_) {
            if (needed_ & ~(bit(Layer::Collision) | bit(Layer::Capsule))) add_note("CINE's asset pak is not installed: only collision and capsule layers can show.");
            needed_ &= bit(Layer::Collision) | bit(Layer::Capsule);
        }
        if (needed_ & bit(Layer::Lighting)) { needed_ &= ~bit(Layer::Lighting); add_note("The lighting pass is not in this version."); }
        if (needed_ & frame_layers) {
            if (!is_handle(camera_)) { needed_ &= ~frame_layers; add_note("Frame passes need a CINE camera."); }
            else {
                try {
                    for (auto& pass : pass_) {
                        pass = instance("PP_Cine_Buffer_Post", {}, -1);
                        if (!is_handle(pass)) throw std::runtime_error("material missing");
                        set_param(pass, "Show", 0.0);
                        call(camera_, "AddOrUpdateBlendable", {{"InBlendableObject", pass}, {"InWeight", 1.0}});
                    }
                    frame_passes_ = true;
                } catch (const std::exception& e) {
                    needed_ &= ~frame_layers; pass_[0] = pass_[1] = nullptr;
                    add_note(std::string(e.what()).find("reflected property type") != std::string::npos
                                 ? "Frame passes need CSSX 1.3.1 or newer." : std::string("Frame passes are unavailable: ") + e.what());
                }
            }
        }
        // Everything she is made of: the body, then hair, garments and accessories (their own
        // skinned or static mesh components), then what is attached as its own actor (weapons,
        // seals). Skinned and static meshes only: a widget component is a mesh component too.
        skins_.push_back({mesh_, Json(), {}});
        if (needed_ & (bit(Layer::Clay) | bit(Layer::Wireframe))) {
            try {
                std::vector<Json> owners{pawn_};
                const auto attached = host_.request({{"op", "call"}, {"target", pawn_}, {"function", "GetAttachedActors"},
                                                     {"args", {{"bResetArray", true}, {"bRecursivelyIncludeAttachedActors", true}}}});
                const auto list = attached.is_object() ? attached.value("OutActors", Json::array()) : Json::array();
                if (list.is_array()) for (const auto& actor : list) if (is_handle(actor) && owners.size() <= max_worn_actors) owners.push_back(actor);
                for (const char* path : {"/Script/Engine.SkinnedMeshComponent", "/Script/Engine.StaticMeshComponent"}) {
                    const auto cls = load(path);
                    if (!is_handle(cls)) continue;
                    for (const auto& owner : owners) {
                        Json parts;
                        try { parts = call(owner, "K2_GetComponentsByClass", {{"ComponentClass", cls}}); } catch (...) { continue; }
                        if (!parts.is_array()) continue;
                        for (const auto& part : parts) {
                            if (!is_handle(part) || skins_.size() >= max_skins) continue;
                            if (std::none_of(skins_.begin(), skins_.end(), [&](const Skin& s) { return id_of(s.component) == id_of(part); })) skins_.push_back({part, Json(), {}});
                        }
                    }
                }
            } catch (...) {}   // the body alone still works
        }
        if (needed_ & (bit(Layer::Clay) | bit(Layer::Wireframe))) {
            for (auto& skin : skins_) {
                try { skin.overlay_before = host_.get(skin.component, "OverlayMaterial"); } catch (...) { skin.overlay_before = nullptr; }
                if (ours(skin.overlay_before)) { skin.overlay_before = nullptr; repaint_ = true; }   // left by a take that did not end cleanly
            }
        }
        if (needed_ & (bone_layers | bit(Layer::Collision))) {
            const auto count = call(mesh_, "GetNumBones");
            bones_expected_ = count.is_number() ? std::min<size_t>(count.get<size_t>(), max_bones) : 0;
            reading_ = bones_expected_ > 0;
        }
        if (needed_ & bit(Layer::Sockets)) {
            const auto names = call(mesh_, "GetAllSocketNames");
            if (names.is_array()) for (const auto& n : names) if (n.is_string() && sockets_.size() < max_sockets) sockets_.push_back(n.get<std::string>());
        }
        if (needed_ & bit(Layer::Collision)) {
            Json asset;
            try { asset = host_.get(mesh_, "PhysicsAssetOverride"); } catch (...) {}
            if (!is_handle(asset)) { const auto skinned = call(mesh_, "GetSkinnedAsset"); if (is_handle(skinned)) asset = host_.get(skinned, "PhysicsAsset"); }
            const auto setups = is_handle(asset) ? host_.get(asset, "SkeletalBodySetups") : Json();
            if (setups.is_array()) for (const auto& s : setups) if (is_handle(s) && bodies_.size() < max_bodies) bodies_.push_back(s);
            if (bodies_.empty()) add_note("This mesh has no physics bodies to show.");
        }
        wipe_numbers(1, 1, 0.01);
        if (!reading_) plan();
    } catch (const std::exception& e) {
        add_note(std::string("could not start: ") + e.what());
    }
}

void Breakdown::read_skeleton() {
    while (bones_.size() < bones_expected_ && spend(3)) {
        const int index = int(bones_.size());
        Bone bone;
        const auto name = call(mesh_, "GetBoneName", {{"BoneIndex", index}});
        bone.name = name.is_string() ? name.get<std::string>() : std::string();
        if (index > 0) { const auto parent = call(mesh_, "GetParentBone", {{"BoneName", bone.name}}); if (parent.is_string()) bone.parent = parent.get<std::string>(); }
        // Reference pose, relative to the parent: where this bone sits on its parent's stick.
        const auto pose = call(mesh_, "GetRefPoseTransform", {{"BoneIndex", index}});
        if (pose.is_object() && pose.contains("Translation")) { const auto& t = pose["Translation"]; bone.local = {t.value("X", 0.0), t.value("Y", 0.0), t.value("Z", 0.0)}; }
        bone.kind = classify_bone(bone.name, spring_names_);
        bones_.push_back(std::move(bone));
    }
    if (bones_.size() >= bones_expected_) { reading_ = false; plan(); }
}

void Breakdown::plan() {
    auto kind_of = [&](const std::string& name) {
        for (const auto& b : bones_) if (b.name == name) return b.kind;
        return BoneKind::Regular;
    };
    auto body_bone = [](BoneKind k) { return k == BoneKind::Regular || k == BoneKind::Spring; };
    for (size_t i = 0; i < bones_.size(); ++i) {
        const auto& b = bones_[i];
        const double length = std::sqrt(b.local.x * b.local.x + b.local.y * b.local.y + b.local.z * b.local.z);
        // A stick from the parent to this bone. Virtual and IK bones hang off the root, far from
        // what they describe, so they get a marker instead of a stick.
        if ((needed_ & bit(Layer::Skeleton)) && !b.parent.empty() && length > 0.5 && length <= max_stick_cm && body_bone(b.kind) && body_bone(kind_of(b.parent)))
            queue_.push_back({Work::Stick, i});
        if (((needed_ & bit(Layer::VirtualBones)) && b.kind == BoneKind::Virtual) || ((needed_ & bit(Layer::IkBones)) && b.kind == BoneKind::Ik)
            || ((needed_ & bit(Layer::Springs)) && b.kind == BoneKind::Spring)) queue_.push_back({Work::Joint, i});
    }
    if (needed_ & bit(Layer::Sockets)) for (size_t i = 0; i < sockets_.size(); ++i) queue_.push_back({Work::Socket, i});
    if (needed_ & bit(Layer::Collision)) for (size_t i = 0; i < bodies_.size(); ++i) queue_.push_back({Work::Body, i});
    refresh_callouts();
}

void Breakdown::build(const Work& work) {
    auto mesh_piece = [&](Layer layer, const char* shape_path, const std::string& socket, Vec3 at, Quat rotation, Vec3 scale) {
        const auto shape = load(shape_path);
        const auto mid = material(layer);
        if (!is_handle(shape) || !is_handle(mid)) return;
        const auto xf = transform(at, rotation, scale);
        const auto component = add_component(class_mesh_, xf);
        pieces_.push_back({component, bit(layer)});
        call(component, "SetStaticMesh", {{"NewMesh", shape}});
        call(component, "SetMaterial", {{"ElementIndex", 0}, {"Material", mid}});
        call(component, "SetCastShadow", {{"NewCastShadow", false}});
        call(component, "K2_AttachToComponent", {{"Parent", mesh_}, {"SocketName", socket}, {"LocationRule", keep_relative},
                                                 {"RotationRule", keep_relative}, {"ScaleRule", keep_relative}, {"bWeldSimulatedBodies", false}});
        host_.request({{"op", "call"}, {"target", component}, {"function", "K2_SetRelativeTransform"},
                       {"args", {{"NewTransform", xf}, {"bSweep", false}, {"bTeleport", true}}}});
    };
    if (work.kind == Work::Skin) { read_skin(skins_[work.index]); return; }
    if (work.kind == Work::Stick) {
        const auto& b = bones_[work.index];
        const double length = std::sqrt(b.local.x * b.local.x + b.local.y * b.local.y + b.local.z * b.local.z);
        const Vec3 direction = b.local * (1.0 / length);
        // The engine cone is 100 tall along Z with a base radius of 50, centred on its origin.
        const double radius = std::clamp(length * 0.035, 0.25, 0.7);
        mesh_piece(Layer::Skeleton, path_cone, b.parent, direction * (length / 2), z_to(direction), {radius / 50, radius / 50, length / 100});
    } else if (work.kind == Work::Joint) {
        const auto& b = bones_[work.index];
        const Layer layer = b.kind == BoneKind::Virtual ? Layer::VirtualBones : b.kind == BoneKind::Ik ? Layer::IkBones : Layer::Springs;
        const double radius = b.kind == BoneKind::Spring ? 2.6 : 2.0;   // the engine sphere has radius 50
        mesh_piece(layer, path_sphere, b.name, {}, {}, {radius / 50, radius / 50, radius / 50});
    } else if (work.kind == Work::Socket) {
        mesh_piece(Layer::Sockets, path_sphere, sockets_[work.index], {}, {}, {0.024, 0.024, 0.024});
    } else {
        // One physics body: every primitive becomes the engine's own shape component on that bone.
        const auto& setup = bodies_[work.index];
        const auto bone = host_.get(setup, "BoneName");
        const auto geometry = host_.get(setup, "AggGeom");
        if (!bone.is_string() || !geometry.is_object()) return;
        const auto colour = colour8(info[static_cast<size_t>(Layer::Collision)].colour);
        auto shape = [&](const Json& cls, const Json& element, auto&& size) {
            if (!is_handle(cls)) return;
            const auto component = add_component(cls, transform({}, {}, {1, 1, 1}));
            pieces_.push_back({component, bit(Layer::Collision)});
            call(component, "K2_AttachToComponent", {{"Parent", mesh_}, {"SocketName", bone}, {"LocationRule", snap_to_target},
                                                     {"RotationRule", snap_to_target}, {"ScaleRule", keep_relative}, {"bWeldSimulatedBodies", false}});
            host_.request({{"op", "call"}, {"target", component}, {"function", "K2_SetRelativeLocationAndRotation"},
                           {"args", {{"NewLocation", element.value("Center", Json{{"X", 0}, {"Y", 0}, {"Z", 0}})},
                                     {"NewRotation", element.value("Rotation", Json{{"Pitch", 0}, {"Yaw", 0}, {"Roll", 0}})}, {"bSweep", false}, {"bTeleport", true}}}});
            size(component);
            host_.set(component, "ShapeColor", colour);
            call(component, "SetLineThickness", {{"Thickness", 1.2}});
        };
        auto each = [&](const char* list, auto&& make) {
            const auto elements = geometry.value(list, Json::array());
            if (elements.is_array()) for (const auto& e : elements) if (e.is_object()) make(e);
        };
        each("SphylElems", [&](const Json& e) {
            const double r = e.value("Radius", 1.0), l = e.value("Length", 0.0);
            shape(class_capsule_, e, [&](const Json& c) { call(c, "SetCapsuleSize", {{"InRadius", r}, {"InHalfHeight", l / 2 + r}, {"bUpdateOverlaps", false}}); });
        });
        each("TaperedCapsuleElems", [&](const Json& e) {
            const double r = std::max(e.value("Radius0", 1.0), e.value("Radius1", 1.0)), l = e.value("Length", 0.0);
            shape(class_capsule_, e, [&](const Json& c) { call(c, "SetCapsuleSize", {{"InRadius", r}, {"InHalfHeight", l / 2 + r}, {"bUpdateOverlaps", false}}); });
        });
        each("SphereElems", [&](const Json& e) {
            shape(class_sphere_, e, [&](const Json& c) { call(c, "SetSphereRadius", {{"InSphereRadius", e.value("Radius", 1.0)}, {"bUpdateOverlaps", false}}); });
        });
        each("BoxElems", [&](const Json& e) {
            shape(class_box_, e, [&](const Json& c) {
                call(c, "SetBoxExtent", {{"InBoxExtent", {{"X", e.value("X", 1.0) / 2}, {"Y", e.value("Y", 1.0) / 2}, {"Z", e.value("Z", 1.0) / 2}}}, {"bUpdateOverlaps", false}});
            });
        });
    }
}

// ---------------------------------------------------------------- showing layers

void Breakdown::set_overlay(const Json* material) {
    // Taking the overlay slot: what each part has there now is what goes back later.
    if (material && !overlay_ours_) {
        for (auto& skin : skins_) {
            try {
                auto now = host_.get(skin.component, "OverlayMaterial");
                if (!ours(now)) { skin.overlay_before = is_handle(now) ? now : Json(); keep(skin.overlay_before); }
            } catch (...) {}
        }
    }
    for (auto& skin : skins_) {
        try { call(skin.component, "SetOverlayMaterial", {{"NewOverlayMaterial", material ? *material : skin.overlay_before}}); } catch (...) {}
    }
    overlay_ours_ = material != nullptr;
}

// One of CINE's own materials (an instance of M_Cine_* or PP_Cine_*), by its name.
bool Breakdown::ours(const Json& material) {
    return is_handle(material) && material.value("name", std::string()).find("_Cine_") != std::string::npos;
}

void Breakdown::keep(const Json& material) {
    if (!is_handle(material) || locked_ >= 2048) return;
    try {
        if (!is_handle(locker_)) {
            locker_ = add_component(class_mesh_, transform({}, {}, {1, 1, 1}));
            call(locker_, "SetHiddenInGame", {{"NewHidden", true}, {"bPropagateToChildren", false}});
        }
        call(locker_, "SetMaterial", {{"ElementIndex", locked_}, {"Material", material}});
        ++locked_;
    } catch (...) {}   // unlocked is how it was before; the end-of-take check still covers her
}

// A part's own slot materials, read once while the take prepares and locked.
void Breakdown::read_skin(Skin& skin) {
    skin.slots_before.clear();
    const auto count = call(skin.component, "GetNumMaterials");
    const int slots = count.is_number() ? std::clamp(count.get<int>(), 0, 64) : 0;
    for (int i = 0; i < slots; ++i) {
        Json material = call(skin.component, "GetMaterial", {{"ElementIndex", i}});
        // CINE's own clay in a slot is what a take that did not end cleanly left behind: there
        // is no original to go back to. The slot is cleared at the end and her look repainted.
        if (ours(material)) { material = nullptr; repaint_ = true; }
        keep(material);
        skin.slots_before.push_back(std::move(material));
    }
    skin.read = true;
}

// Clay on every slot of every part, or her own materials back. Only slots that still hold
// the clay are restored: if the outfit repainted a slot meanwhile, that newer material stays.
// A slot whose own material cannot be set again is cleared (the mesh's default shows, never
// an invisible clay) and flagged for a repaint.
void Breakdown::apply_clay(bool on) {
    if (on == clay_slots_) return;
    if (on) {
        // Read now, not while the take prepared: the wardrobe mod makes new material
        // instances each time a look is worn, and putting back older ones leaves it with
        // materials it does not know (it then refuses every further change, 5 Oct).
        for (auto& skin : skins_) {
            try { read_skin(skin); } catch (...) { skin.slots_before.clear(); continue; }
            for (int i = 0; i < int(skin.slots_before.size()); ++i) {
                try { call(skin.component, "SetMaterial", {{"ElementIndex", i}, {"Material", clay_}}); } catch (...) {}
            }
        }
        clay_slots_ = true;
    } else {
        for (auto& skin : skins_) {
            for (int i = 0; i < int(skin.slots_before.size()); ++i) {
                try {
                    if (id_of(call(skin.component, "GetMaterial", {{"ElementIndex", i}})) != id_of(clay_)) continue;
                } catch (...) { continue; }   // the part is gone
                const auto& original = skin.slots_before[size_t(i)];
                bool back = false;
                if (is_handle(original)) { try { call(skin.component, "SetMaterial", {{"ElementIndex", i}, {"Material", original}}); back = true; } catch (...) {} }
                if (!back) {
                    repaint_ = true;
                    try { call(skin.component, "SetMaterial", {{"ElementIndex", i}, {"Material", nullptr}}); } catch (...) {}
                }
            }
        }
        clay_slots_ = false;
    }
}

// The last word of a take: nothing of CINE's may be left on her, whatever happened before.
void Breakdown::verify_restored() {
    for (auto& skin : skins_) {
        if (!valid(skin.component)) continue;
        try {
            if (ours(host_.get(skin.component, "OverlayMaterial"))) {
                call(skin.component, "SetOverlayMaterial", {{"NewOverlayMaterial", skin.overlay_before}});
            }
        } catch (...) {}
        for (int i = 0; i < int(skin.slots_before.size()); ++i) {
            try {
                if (!ours(call(skin.component, "GetMaterial", {{"ElementIndex", i}}))) continue;
                repaint_ = true;
                call(skin.component, "SetMaterial", {{"ElementIndex", i}, {"Material", nullptr}});
            } catch (...) {}
        }
    }
}

void Breakdown::depth_range(double cm) {
    cm = std::clamp(cm, 150.0, 20000.0);
    if (!frame_passes_ || (depth_range_ > 0 && std::abs(cm - depth_range_) < depth_range_ * 0.12)) return;
    depth_range_ = cm;
    for (auto& pass : pass_) { try { set_param(pass, "DepthRange", cm); } catch (...) {} }
}

void Breakdown::show(LayerSet set, double wipe_seconds) {
    if (!active_) return;
    try {
        if (transition_end_ >= 0) settle();
        set &= needed_;
        const LayerSet incoming = set & ~shown_, outgoing = shown_ & ~set, staying = shown_ & set;
        if (!incoming && !outgoing) return;
        const bool wiping = pak_ && wipe_seconds > 0.01;
        wipe_numbers(wiping ? 0 : 1, 1, wiping ? wipe_seconds : 0.01);
        const double now = wiping || ((incoming | outgoing) & (bit(Layer::Collision) | bit(Layer::Capsule))) ? game_time() : 0;
        // Where a layer stands in this transition: coming in behind the wipe, going out ahead
        // of it, or staying whatever the wipe does.
        auto place = [&](const Json& mid, bool in, bool out) {
            if (!is_handle(mid)) return;
            if (in) { set_param(mid, "Show", 1); set_param(mid, "Invert", 0); set_param(mid, "Hold", wiping ? 0 : 1); }
            else if (out) { if (wiping) { set_param(mid, "Invert", 1); set_param(mid, "Hold", 0); } else set_param(mid, "Show", 0); }
            else set_param(mid, "Hold", 1);
        };
        for (auto l : {Layer::Skeleton, Layer::VirtualBones, Layer::IkBones, Layer::Springs, Layer::Sockets})
            if ((incoming | outgoing | staying) & bit(l)) place(mids_[static_cast<size_t>(l)], incoming & bit(l), outgoing & bit(l));

        // Her mesh has one overlay slot. Wireframe always uses it; clay borrows it for its own
        // wipe when wireframe is not involved, and otherwise changes her slot materials at once.
        const LayerSet wire = bit(Layer::Wireframe), clay = bit(Layer::Clay);
        const bool overlay_taken = (incoming | outgoing | staying) & wire;
        if ((incoming | staying) & wire) {
            if (!is_handle(wire_)) wire_ = instance("M_Cine_Wire", info[static_cast<size_t>(Layer::Wireframe)].colour, -1);
            place(wire_, incoming & wire, false);
            if (incoming & wire) set_overlay(&wire_);
        } else if (outgoing & wire) {
            place(wire_, false, true);
            if (!wiping) set_overlay(nullptr);
        }
        if (incoming & clay) {
            if (!is_handle(clay_)) { clay_ = instance("M_Cine_Clay", info[static_cast<size_t>(Layer::Clay)].colour, -1); set_param(clay_, "Ambient", clay_ambient); }
            if (wiping && !overlay_taken) { place(clay_, true, false); set_overlay(&clay_); }
            else { set_param(clay_, "Show", 1); set_param(clay_, "Invert", 0); set_param(clay_, "Hold", 1); apply_clay(true); }
        } else if (outgoing & clay) {
            apply_clay(false);
            if (wiping && !overlay_taken) { place(clay_, false, true); set_overlay(&clay_); }
        } else if (staying & clay) set_param(clay_, "Hold", 1);

        // Frame passes: one at a time, on two instances so the outgoing pass and the incoming
        // one can share a wipe.
        if (frame_passes_) {
            const auto want = first_frame_layer(set), have = first_frame_layer(shown_);
            if (want != have) {
                const int next = 1 - pass_now_;
                if (want) {
                    for (auto l : {Layer::BaseColor, Layer::Normals, Layer::Roughness, Layer::Metallic, Layer::Depth, Layer::Occlusion})
                        set_param(pass_[next], pass_weight(l), l == *want ? 1.0 : 0.0);
                    place(pass_[next], true, false);
                }
                if (have) place(pass_[pass_now_], false, true);
                pass_now_ = next;
            } else if (want) set_param(pass_[pass_now_], "Hold", 1);
        }
        // Shape components cannot wipe: they change in the middle of it.
        shapes_pending_ = (incoming | outgoing) & (bit(Layer::Collision) | bit(Layer::Capsule));
        shapes_flip_at_ = shapes_pending_ ? now + (wiping ? wipe_seconds / 2 : 0) : -1;
        leaving_ = outgoing;
        shown_ = set;
        transition_end_ = wiping ? now + wipe_seconds : -1;
        if (!wiping) settle();
        refresh_callouts();
    } catch (const std::exception& e) {
        add_note(std::string("a layer could not be shown: ") + e.what());
    }
}

// The wipe has passed: outgoing layers are off, and every switch is back to plain "shown".
void Breakdown::settle() {
    transition_end_ = -1;
    const LayerSet wire = bit(Layer::Wireframe), clay = bit(Layer::Clay);
    for (auto l : {Layer::Skeleton, Layer::VirtualBones, Layer::IkBones, Layer::Springs, Layer::Sockets}) {
        const auto& mid = mids_[static_cast<size_t>(l)];
        if (!is_handle(mid)) continue;
        if (shown_ & bit(l)) { set_param(mid, "Invert", 0); set_param(mid, "Hold", 0); set_param(mid, "Show", 1); }
        else if (leaving_ & bit(l)) { set_param(mid, "Show", 0); set_param(mid, "Invert", 0); }
    }
    if ((shown_ & clay) && !clay_slots_) { set_param(clay_, "Hold", 1); set_param(clay_, "Invert", 0); apply_clay(true); }
    if (shown_ & wire) { set_overlay(&wire_); set_param(wire_, "Invert", 0); set_param(wire_, "Hold", 0); }
    else if (overlay_ours_ || (leaving_ & (wire | clay))) set_overlay(nullptr);
    if (frame_passes_) {
        set_param(pass_[1 - pass_now_], "Show", 0);
        if (first_frame_layer(shown_)) { set_param(pass_[pass_now_], "Invert", 0); set_param(pass_[pass_now_], "Hold", 0); }
        else set_param(pass_[pass_now_], "Show", 0);
    }
    leaving_ = 0;
    wipe_numbers(1, 1, 0.01);
}

void Breakdown::tick() {
    budget_ = calls_per_tick;
    try {
        while (!teardown_.empty() && spend(2)) {
            const Json component = std::move(teardown_.front()); teardown_.pop_front();
            try { if (valid(component)) call(component, "K2_DestroyComponent", {{"Object", component}}); } catch (...) {}
        }
        if (!active_) return;
        if (reading_) read_skeleton();
        while (!reading_ && !queue_.empty() && spend(queue_.front().kind == Work::Skin ? 56 : queue_.front().kind == Work::Body ? 24 : 8)) {
            const Work work = queue_.front(); queue_.pop_front();
            try { build(work); } catch (const std::exception& e) { add_note(std::string("a piece could not be built: ") + e.what()); }
        }
        if (transition_end_ < 0 && shapes_flip_at_ < 0) return;
        const double now = game_time();
        if (shapes_flip_at_ >= 0 && now >= shapes_flip_at_) {
            shapes_flip_at_ = -1;
            if (shapes_pending_ & bit(Layer::Collision)) {
                const bool hide = !(shown_ & bit(Layer::Collision));
                for (const auto& piece : pieces_) if (piece.layer & bit(Layer::Collision)) { try { call(piece.component, "SetHiddenInGame", {{"NewHidden", hide}, {"bPropagateToChildren", false}}); } catch (...) {} }
            }
            if ((shapes_pending_ & bit(Layer::Capsule)) && is_handle(capsule_)) {
                if (!capsule_touched_) {
                    capsule_touched_ = true;
                    const auto hidden = host_.get(capsule_, "bHiddenInGame");
                    capsule_hidden_before_ = !hidden.is_boolean() || hidden.get<bool>();
                    try { capsule_colour_before_ = host_.get(capsule_, "ShapeColor"); } catch (...) {}
                    host_.set(capsule_, "ShapeColor", colour8(info[static_cast<size_t>(Layer::Capsule)].colour));
                }
                call(capsule_, "SetHiddenInGame", {{"NewHidden", !(shown_ & bit(Layer::Capsule))}, {"bPropagateToChildren", false}});
            }
            shapes_pending_ = 0;
        }
        if (transition_end_ >= 0 && now >= transition_end_) settle();
    } catch (const std::exception& e) {
        add_note(std::string("a step failed: ") + e.what());
    }
}

// ---------------------------------------------------------------- callouts and caption

void Breakdown::refresh_callouts() {
    callouts_.clear();
    auto bone_like = [&](std::initializer_list<const char*> wanted) -> std::string {
        for (const char* want : wanted) for (const auto& b : bones_) if (lower(b.name) == want) return b.name;
        return bones_.empty() ? std::string(*wanted.begin()) : std::string();
    };
    auto count = [&](BoneKind kind) { return std::count_if(bones_.begin(), bones_.end(), [&](const Bone& b) { return b.kind == kind; }); };
    auto first = [&](BoneKind kind) -> std::string { for (const auto& b : bones_) if (b.kind == kind) return b.name; return {}; };
    auto add = [&](std::string text, const Json& component, std::string socket) { if (is_handle(component)) callouts_.push_back({std::move(text), component, std::move(socket)}); };
    if (shown_ & bit(Layer::Wireframe)) add("Mesh wireframe", mesh_, bone_like({"head"}));
    else if (shown_ & bit(Layer::Clay)) add("Clay shading", mesh_, bone_like({"head"}));
    if (shown_ & bit(Layer::Skeleton)) add("Skeleton, " + std::to_string(bones_.size()) + " bones", mesh_, bone_like({"spine_03", "spine_02", "pelvis"}));
    if ((shown_ & bit(Layer::VirtualBones)) && count(BoneKind::Virtual)) add("Virtual bones, " + std::to_string(count(BoneKind::Virtual)), mesh_, first(BoneKind::Virtual));
    if ((shown_ & bit(Layer::IkBones)) && count(BoneKind::Ik)) add("IK bones, " + std::to_string(count(BoneKind::Ik)), mesh_, bone_like({"ik_hand_r", "ik_foot_r"}).empty() ? first(BoneKind::Ik) : bone_like({"ik_hand_r", "ik_foot_r"}));
    if (shown_ & bit(Layer::Springs)) {
        std::vector<std::string> seen;
        for (const auto& b : bones_) {
            if (b.kind != BoneKind::Spring) continue;
            auto group = spring_group(b.name);
            if (group.empty()) group = "outfit";
            if (std::find(seen.begin(), seen.end(), group) != seen.end()) continue;
            seen.push_back(group);
            add("Spring bones: " + group, mesh_, b.name);
        }
    }
    if ((shown_ & bit(Layer::Sockets)) && !sockets_.empty()) add("Sockets, " + std::to_string(sockets_.size()), mesh_, sockets_.front());
    if ((shown_ & bit(Layer::Collision)) && !bodies_.empty()) add("Physics bodies, " + std::to_string(bodies_.size()), mesh_, bone_like({"thigh_r", "thigh_l", "pelvis"}));
    if (shown_ & bit(Layer::Capsule)) add("Movement capsule", capsule_, "");
}

std::string Breakdown::caption() const {
    std::string text;
    for (size_t i = 0; i < static_cast<size_t>(Layer::Count); ++i)
        if (shown_ & (LayerSet{1} << i)) text += (text.empty() ? "" : " + ") + std::string(info[i].label);
    return text;
}

// ---------------------------------------------------------------- restore

void Breakdown::finish() {
    if (!active_) return;
    const auto failures = end();   // materials, overlay, capsule: now, so the frame is clean at once
    (void)failures;
}

std::vector<std::string> Breakdown::end() {
    std::vector<std::string> failures;
    if (!active_) return failures;
    auto step = [&](const char* what, auto&& undo) { try { undo(); } catch (const std::exception& e) { failures.push_back(std::string(what) + ": " + e.what()); } };
    const bool mesh_alive = valid(mesh_);
    if (mesh_alive) {
        step("clay", [&] { apply_clay(false); });
        step("overlay", [&] { if (overlay_ours_ || is_handle(wire_) || is_handle(clay_)) set_overlay(nullptr); });
    }
    verify_restored();
    if (is_handle(locker_)) teardown_.push_back(locker_);   // her materials are back in their slots, which hold them now
    if (frame_passes_ && valid(camera_))
        for (auto& pass : pass_) if (is_handle(pass)) step("frame pass", [&] { call(camera_, "RemoveBlendable", {{"InBlendableObject", pass}}); });
    if (capsule_touched_ && valid(capsule_)) {
        step("capsule", [&] {
            call(capsule_, "SetHiddenInGame", {{"NewHidden", capsule_hidden_before_}, {"bPropagateToChildren", false}});
            if (capsule_colour_before_.is_object()) host_.set(capsule_, "ShapeColor", capsule_colour_before_);
        });
    }
    if (pak_) step("wipe", [&] { wipe_numbers(1, 1, 0.01); });
    // The attached pieces go over the next ticks; hidden first so nothing lingers on screen.
    for (auto l : {Layer::Skeleton, Layer::VirtualBones, Layer::IkBones, Layer::Springs, Layer::Sockets})
        step("layer", [&] { set_param(mids_[static_cast<size_t>(l)], "Show", 0); });
    for (auto& piece : pieces_) {
        if (piece.layer & bit(Layer::Collision)) { try { call(piece.component, "SetHiddenInGame", {{"NewHidden", true}, {"bPropagateToChildren", false}}); } catch (...) {} }
        teardown_.push_back(std::move(piece.component));
    }
    auto pending = std::move(teardown_);
    const bool repaint = repaint_;
    forget();
    teardown_ = std::move(pending);
    repaint_ = repaint;   // read by the caller after end()
    return failures;
}

void Breakdown::forget() {
    active_ = reading_ = pak_ = frame_passes_ = false;
    needed_ = shown_ = leaving_ = shapes_pending_ = 0;
    pawn_ = mesh_ = capsule_ = camera_ = statics_ = kismet_materials_ = nullptr;
    class_mesh_ = class_capsule_ = class_sphere_ = class_box_ = nullptr;
    for (auto& mid : mids_) mid = nullptr;
    clay_ = wire_ = pass_[0] = pass_[1] = capsule_colour_before_ = nullptr;
    skins_.clear();
    locker_ = nullptr; locked_ = 0; repaint_ = false; depth_range_ = 0;
    pass_now_ = 0;
    spring_names_.clear(); bones_.clear(); sockets_.clear(); bodies_.clear();
    queue_.clear(); pieces_.clear(); teardown_.clear(); callouts_.clear();
    bones_expected_ = 0;
    clay_slots_ = overlay_ours_ = capsule_touched_ = false; capsule_hidden_before_ = true;
    transition_end_ = shapes_flip_at_ = -1;
    note_.clear();
}

} // namespace cine

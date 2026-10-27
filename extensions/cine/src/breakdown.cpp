#include "breakdown.hpp"
#include "content.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace cine {
namespace {
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
uint64_t id_of(const Json& j) { return is_handle(j) ? j["$object"].get<uint64_t>() : 0; }
std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); }); return s; }

constexpr const char* path_cone = "/Engine/BasicShapes/Cone.Cone";
constexpr const char* path_sphere = "/Engine/BasicShapes/Sphere.Sphere";
constexpr size_t max_bones = 1200;      // the game's widest skeleton is 1199 bones
constexpr size_t max_sockets = 128;
constexpr size_t max_bodies = 160;
constexpr size_t max_skins = 40;
constexpr size_t max_worn_actors = 12;
constexpr size_t max_looks = 30;
constexpr int keep_relative = 0, snap_to_target = 2, no_collision = 0;

constexpr const char* layer_ids[] = {"clay", "wireframe", "skeleton", "virtual_bones", "ik_bones", "springs", "sockets", "collision", "capsule", "no_body", "base_color", "normals", "roughness", "metallic", "depth", "occlusion", "lighting"};
const CoreBreakdownLayersItem& layer_style(Layer layer) {
    const auto& layers = content().breakdown.layers;
    return *std::find_if(layers.begin(), layers.end(), [&](const auto& s) { return s.id == layer_ids[static_cast<size_t>(layer)]; });
}
Vec3 layer_colour(Layer layer) { const auto& c = layer_style(layer).color; return {c[0], c[1], c[2]}; }
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

const char* layer_id(Layer l) { return layer_ids[static_cast<size_t>(l)]; }
const char* layer_label(Layer l) { return layer_style(l).label.c_str(); }
std::optional<Layer> layer_from(const std::string& id) {
    for (size_t i = 0; i < static_cast<size_t>(Layer::Count); ++i) if (id == layer_ids[i]) return static_cast<Layer>(i);
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
    for (const auto& group : content().bones.spring_groups)
        for (const auto& prefix : group.prefixes) if (n.starts_with(prefix)) return group.group;
    return {};
}
bool helper_bone(const std::string& name) {
    const auto n = lower(name);
    for (const auto& part : content().bones.helper_contains)
        if (n.find(part) != std::string::npos) return true;
    for (const auto& start : content().bones.helper_prefixes)
        if (n.starts_with(start)) return true;
    return false;
}
BoneKind classify_bone(const std::string& name, const std::vector<std::string>& spring_bones) {
    const auto n = lower(name);
    if (n.starts_with(content().bones.virtual_prefix)) return BoneKind::Virtual;
    if (n.starts_with(content().bones.ik_prefix)) return BoneKind::Ik;
    // The usual spring names always count; an outfit adds its own chains to them.
    if (spring_by_name(name) || std::any_of(spring_bones.begin(), spring_bones.end(), [&](const std::string& s) { return lower(s) == n; }))
        return BoneKind::Spring;
    return helper_bone(name) ? BoneKind::Helper : BoneKind::Regular;
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
    const auto collection = load(content().breakdown.collection);   // reloaded each time: a bare asset handle can be collected
    if (!is_handle(collection)) return;
    const double now = game_time();
    for (const auto& [name, value] : {std::pair<const char*, double>{"WipeFrom", from}, {"WipeTo", to}, {"WipeDur", std::max(0.01, seconds)}, {"WipeT0", now}, {"WipeSoft", content().materials.wipe_soft}, {"WipeLine", content().materials.wipe_line}})
        call(kismet_materials_, "SetScalarParameterValue",
             {{"WorldContextObject", pawn_}, {"Collection", collection}, {"ParameterName", name}, {"ParameterValue", value}});
}
Json Breakdown::instance(const std::string& path, Vec3 colour, double opacity) {
    const auto parent = load(path);
    if (!is_handle(parent)) return Json();
    auto mid = call(kismet_materials_, "CreateDynamicMaterialInstance",
                    {{"WorldContextObject", pawn_}, {"Parent", parent}, {"OptionalName", "None"}, {"CreationFlags", 0}});
    if (!is_handle(mid)) return Json();
    for (const char* name : {"Color", "BaseColor"}) {
        try { call(mid, "SetVectorParameterValue", {{"ParameterName", name}, {"Value", {{"R", colour.x}, {"G", colour.y}, {"B", colour.z}, {"A", 1.0}}}}); } catch (...) {}
    }
    const auto& m = content().materials;
    set_param(mid, "EndpointFade", m.endpoint_fade);
    if (path == content().breakdown.wire) set_param(mid, "Push", m.wire_push);
    if (path == content().breakdown.clay) {
        set_param(mid, "Roughness", m.clay_roughness); set_param(mid, "Specular", m.clay_specular); set_param(mid, "Push", m.clay_push);
    }
    if (path == content().breakdown.xray) {
        set_param(mid, "Fresnel", m.fresnel); set_param(mid, "Rim", m.rim);
    }
    if (path == content().breakdown.buffer_post) {
        set_param(mid, "DepthRange", m.depth_range); set_param(mid, "LightingScale", m.lighting_scale);
        set_param(mid, "LightingFloor", m.lighting_floor); set_param(mid, "Gain", m.gain);
        set_param(mid, "Gamma", m.gamma); set_param(mid, "LineGlow", m.line_glow);
        call(mid, "SetVectorParameterValue", {{"ParameterName", "LineColor"}, {"Value", {{"R", m.line_color[0]}, {"G", m.line_color[1]}, {"B", m.line_color[2]}, {"A", m.line_color[3]}}}});
    }
    if (opacity >= 0) set_param(mid, "Opacity", opacity);
    return mid;
}
// Our wireframe and clay instances. Nothing references one while it is off her (between two
// steps of a long take), and a garbage collection would take it: the locker holds them too.
const Json& Breakdown::wire_mid() {
    if (is_handle(wire_) && valid(wire_)) return wire_;
    wire_ = instance(content().breakdown.wire, layer_colour(Layer::Wireframe), -1);
    set_param(wire_, "Glow", content().breakdown.wire_glow);
    keep(wire_);
    return wire_;
}
const Json& Breakdown::clay_mid() {
    if (is_handle(clay_) && valid(clay_)) return clay_;
    clay_ = instance(content().breakdown.clay, layer_colour(Layer::Clay), -1);
    set_param(clay_, "Ambient", content().breakdown.clay_ambient);
    // As an overlay clay is a second skin over her own: lifted a little further, her own
    // surface cannot shadow it in speckles.
    if (options_.overlay_only) set_param(clay_, "Push", content().breakdown.clay_overlay_push);
    keep(clay_);
    return clay_;
}

// The shared instance of one group of bone shapes: a layer, and the set of looks that have
// those bones. Hidden until its layer is shown on a look that has them: the caller assigns it
// to a component in the same tick, which is what keeps it alive.
Json Breakdown::material(Layer layer, uint32_t looks) {
    for (const auto& g : groups_) if (g.layer == layer && g.looks == looks) return g.mid;
    auto mid = instance(content().breakdown.xray, layer_colour(layer), content().breakdown.bone_opacity);
    if (!is_handle(mid)) return mid;
    set_param(mid, "Glow", content().breakdown.bone_glow);
    if (!((shown_ & bit(layer)) && worn(looks))) set_param(mid, "Show", 0.0);
    groups_.push_back({layer, looks, mid});
    return mid;
}
size_t Breakdown::bones_worn() const {
    return size_t(std::count_if(bones_.begin(), bones_.end(), [&](const Bone& b) { return worn(b.looks); }));
}
Json Breakdown::add_component(const Json& cls, const Json& xf) {
    auto component = call(pawn_, "AddComponentByClass", {{"Class", cls}, {"bManualAttachment", true}, {"RelativeTransform", xf}, {"bDeferredFinish", false}});
    if (!is_handle(component)) throw std::runtime_error("could not create a component");
    try { host_.set(component, "ComponentTags", Json::array({piece_tag})); } catch (...) {}
    call(component, "SetCollisionEnabled", {{"NewType", no_collision}});   // before it has any geometry
    return component;
}

// Her parts out of the picture (or back), leaving the bones, sockets and shapes attached to
// them: hiding a component does not touch its children unless asked to.
void Breakdown::hide_body(bool hide) {
    for (const auto& skin : skins_) {
        try { call(skin.component, "SetHiddenInGame", {{"NewHidden", hide}, {"bPropagateToChildren", false}}); } catch (...) {}
    }
    body_hidden_ = hide;
}

Json Breakdown::rig_parent() {
    if (is_handle(proxy_) && valid(proxy_)) return proxy_;
    const auto cls = load("/Script/Engine.SkeletalMeshComponent");
    if (!is_handle(cls)) throw std::runtime_error("no skeletal mesh component class");
    proxy_ = add_component(cls, transform({}, {}, {1, 1, 1}));
    call(proxy_, "K2_AttachToComponent", {{"Parent", mesh_}, {"SocketName", ""}, {"LocationRule", snap_to_target},
                                          {"RotationRule", snap_to_target}, {"ScaleRule", snap_to_target}, {"bWeldSimulatedBodies", false}});
    try { call(proxy_, "SetVisibility", {{"bNewVisibility", false}, {"bPropagateToChildren", false}}); } catch (...) {}
    try { call(proxy_, "SetCastShadow", {{"NewCastShadow", false}}); } catch (...) {}
    refresh_rig_parent();
    return proxy_;
}

void Breakdown::refresh_rig_parent() {
    if (!is_handle(proxy_) || !valid(proxy_)) return;
    // Her current mesh asset, so the follower has her bones and sockets, following her pose
    // (spring, virtual and IK bones included: it copies the final pose) without a graph of its own.
    // Through the getter: the property is SkinnedAsset on this engine's mesh class.
    Json asset;
    try { asset = call(mesh_, "GetSkeletalMeshAsset"); } catch (...) {}
    if (!is_handle(asset)) throw std::runtime_error("her mesh has no skeletal mesh asset to follow");
    call(proxy_, "SetSkeletalMeshAsset", {{"NewMesh", asset}});
    call(proxy_, "SetLeaderPoseComponent", {{"NewLeaderBoneComponent", mesh_}, {"bForceUpdate", true}, {"bInFollowerShouldTickPose", false}});
}

// ---------------------------------------------------------------- begin and build

void Breakdown::sweep_orphans(const Json& pawn) {
    try {
        const auto cls = load("/Script/Engine.ActorComponent");
        if (!is_handle(cls)) return;
        const auto found = call(pawn, "GetComponentsByTag", {{"ComponentClass", cls}, {"Tag", piece_tag}});
        if (!found.is_array() || found.empty()) return;
        for (const auto& c : found) { try { if (valid(c)) call(c, "K2_DestroyComponent", {{"Object", c}}); } catch (...) {} }
        try { host_.log("Breakdown: removed " + std::to_string(found.size()) + " pieces left from an earlier session"); } catch (...) {}
    } catch (...) {}
}

void Breakdown::flush() {
    while (!teardown_.empty()) {
        const Json component = std::move(teardown_.front()); teardown_.pop_front();
        try { if (valid(component)) call(component, "K2_DestroyComponent", {{"Object", component}}); } catch (...) {}
    }
}

void Breakdown::begin(const Json& pawn, const Json& camera, LayerSet needed, std::vector<std::string> spring_bones, Options options) {
    if (active_) end();
    flush();
    forget();
    repaint_ = false;
    if (!is_handle(pawn) || needed == 0) return;
    sweep_orphans(pawn);
    try {
        pawn_ = pawn; camera_ = camera; needed_ = needed; options_ = options;
        mesh_ = host_.get(pawn_, "Mesh");
        capsule_ = host_.get(pawn_, "CapsuleComponent");
        statics_ = host_.find("/Script/Engine.Default__GameplayStatics");
        kismet_materials_ = host_.find("/Script/Engine.Default__KismetMaterialLibrary");
        class_mesh_ = load("/Script/Engine.StaticMeshComponent"); class_capsule_ = load("/Script/Engine.CapsuleComponent");
        class_sphere_ = load("/Script/Engine.SphereComponent"); class_box_ = load("/Script/Engine.BoxComponent");
        if (!is_handle(mesh_) || !is_handle(statics_) || !is_handle(kismet_materials_)) { add_note("This character has no mesh to break down."); return; }
        active_ = true;
        pak_ = is_handle(load(content().breakdown.collection));
        if (!pak_) {
            if (needed_ & ~(bit(Layer::Collision) | bit(Layer::Capsule) | bit(Layer::NoBody))) add_note("CINE's asset pak is not installed: only collision and capsule layers can show.");
            needed_ &= bit(Layer::Collision) | bit(Layer::Capsule) | bit(Layer::NoBody);
        }
        if (needed_ & bit(Layer::Lighting)) { needed_ &= ~bit(Layer::Lighting); add_note("The lighting pass is not in this version."); }
        if (needed_ & frame_layers) {
            if (!is_handle(camera_)) { needed_ &= ~frame_layers; add_note("Frame passes need a CINE camera."); }
            else {
                try {
                    for (auto& pass : pass_) {
                        pass = instance(content().breakdown.buffer_post, {}, -1);
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
        find_skins();
        if (needed_ & (bit(Layer::Clay) | bit(Layer::Wireframe))) {
            for (auto& skin : skins_) {
                try { skin.overlay_before = host_.get(skin.component, "OverlayMaterial"); } catch (...) { skin.overlay_before = nullptr; }
                if (ours(skin.overlay_before)) { skin.overlay_before = nullptr; repaint_ = true; }   // left by a take that did not end cleanly
            }
        }
        if (needed_ & bit(Layer::Sockets)) {
            const auto names = call(mesh_, "GetAllSocketNames");
            if (names.is_array()) for (const auto& n : names) if (n.is_string() && sockets_.size() < max_sockets) sockets_.push_back(n.get<std::string>());
        }
        wipe_numbers(1, 1, 0.01);
        // One look: read it and build. Several: the director teaches each, then assembles.
        if (!options_.looks) { learn("", spring_bones); if (!reading_) plan(); }
    } catch (const std::exception& e) {
        add_note(std::string("could not start: ") + e.what());
    }
}

// Every skinned and static mesh part she is made of now. A part already known keeps what was
// read from it (its own overlay, its slot materials).
void Breakdown::find_skins() {
    std::vector<Skin> before = std::move(skins_);
    skins_.clear();
    auto add = [&](const Json& part) {
        if (!is_handle(part) || skins_.size() >= max_skins) return;
        if (std::any_of(skins_.begin(), skins_.end(), [&](const Skin& s) { return id_of(s.component) == id_of(part); })) return;
        for (auto& old : before) if (id_of(old.component) == id_of(part)) { skins_.push_back(std::move(old)); return; }
        Skin skin{part, Json(), {}};
        if (overlay_ours_) {
            // A new part while our overlay is on the others: its own goes back at the end.
            try { auto now = host_.get(part, "OverlayMaterial"); if (is_handle(now) && !ours(now)) { skin.overlay_before = now; keep(now); } } catch (...) {}
        }
        skins_.push_back(std::move(skin));
    };
    add(mesh_);
    if (!(needed_ & (bit(Layer::Clay) | bit(Layer::Wireframe)))) return;
    // Our own pieces (bone shapes, the locker) are static meshes on her too.
    std::unordered_set<uint64_t> mine{id_of(locker_)};
    for (const auto& piece : pieces_) mine.insert(id_of(piece.component));
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
                for (const auto& part : parts) if (!mine.contains(id_of(part))) add(part);
            }
        }
    } catch (...) {}   // the body alone still works
}

// The bones of the look she wears now. A bone seen on an earlier look only gets this look's
// bit; a new one is read in full (parent and reference pose).
void Breakdown::learn(const std::string& look, const std::vector<std::string>& spring_bones) {
    if (!active_ || !(needed_ & (bone_layers | bit(Layer::Collision)))) return;
    try {
        auto it = std::find(looks_.begin(), looks_.end(), look);
        if (it == looks_.end()) {
            if (looks_.size() >= max_looks) { add_note("Too many looks for one breakdown; the later ones show the body's bones only."); return; }
            looks_.push_back(look);
            it = looks_.end() - 1;
        }
        look_now_ = size_t(it - looks_.begin());
        all_looks_ = (uint32_t{1} << looks_.size()) - 1;
        for (auto& b : bones_) b.looks &= ~(uint32_t{1} << look_now_);
        // A bone any look calls a spring is one.
        for (const auto& name : spring_bones)
            if (spring_names_.size() < 512 && std::find(spring_names_.begin(), spring_names_.end(), name) == spring_names_.end()) spring_names_.push_back(name);
        for (auto& b : bones_) b.kind = classify_bone(b.name, spring_names_);
        const auto count = call(mesh_, "GetNumBones");
        bones_expected_ = count.is_number() ? std::min<size_t>(count.get<size_t>(), max_bones) : 0;
        read_index_ = 0;
        reading_ = bones_expected_ > 0;
    } catch (const std::exception& e) {
        reading_ = false;
        add_note(std::string("could not read a look's bones: ") + e.what());
    }
}

void Breakdown::read_skeleton() {
    const uint32_t look = uint32_t{1} << look_now_;
    while (read_index_ < bones_expected_ && budget_ >= 3) {
        const int index = int(read_index_++);
        const auto name = call(mesh_, "GetBoneName", {{"BoneIndex", index}});
        --budget_;
        const std::string text = name.is_string() ? name.get<std::string>() : std::string();
        const auto known = std::find_if(bones_.begin(), bones_.end(), [&](const Bone& b) { return b.name == text; });
        if (known != bones_.end()) { known->looks |= look; continue; }
        if (bones_.size() >= max_bones * 2) continue;
        Bone bone;
        bone.name = text; bone.looks = look;
        if (index > 0) { const auto parent = call(mesh_, "GetParentBone", {{"BoneName", bone.name}}); if (parent.is_string()) bone.parent = parent.get<std::string>(); }
        // Reference pose, relative to the parent: where this bone sits on its parent's stick.
        const auto pose = call(mesh_, "GetRefPoseTransform", {{"BoneIndex", index}});
        budget_ -= 2;
        if (pose.is_object() && pose.contains("Translation")) { const auto& t = pose["Translation"]; bone.local = {t.value("X", 0.0), t.value("Y", 0.0), t.value("Z", 0.0)}; }
        bone.kind = classify_bone(bone.name, spring_names_);
        bones_.push_back(std::move(bone));
    }
    if (read_index_ >= bones_expected_) { reading_ = false; if (!options_.looks) plan(); }
}

void Breakdown::assemble() {
    if (!active_ || assembled_) return;
    reading_ = false;
    plan();
}

void Breakdown::plan() {
    assembled_ = true;
    // The physics bodies of the mesh worn now: read here, not at begin(), because a take that
    // tries on several looks first would hold setups of an asset that has been unloaded since.
    if (needed_ & bit(Layer::Collision)) {
        try {
            Json asset;
            try { asset = host_.get(mesh_, "PhysicsAssetOverride"); } catch (...) {}
            if (!is_handle(asset)) { const auto skinned = call(mesh_, "GetSkinnedAsset"); if (is_handle(skinned)) asset = host_.get(skinned, "PhysicsAsset"); }
            const auto setups = is_handle(asset) ? host_.get(asset, "SkeletalBodySetups") : Json();
            if (setups.is_array()) for (const auto& setup : setups) if (is_handle(setup) && bodies_.size() < max_bodies) bodies_.push_back(setup);
        } catch (...) {}
        if (bodies_.empty()) add_note("This mesh has no physics bodies to show.");
    }
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
        if ((needed_ & bit(Layer::Skeleton)) && !b.parent.empty() && length > content().geometry.stick_min && length <= content().geometry.stick_max && body_bone(b.kind) && body_bone(kind_of(b.parent)))
            queue_.push_back({Work::Stick, i});
        if (((needed_ & bit(Layer::VirtualBones)) && b.kind == BoneKind::Virtual) || ((needed_ & bit(Layer::IkBones)) && b.kind == BoneKind::Ik)
            || ((needed_ & bit(Layer::Springs)) && b.kind == BoneKind::Spring)) queue_.push_back({Work::Joint, i});
    }
    if (needed_ & bit(Layer::Sockets)) for (size_t i = 0; i < sockets_.size(); ++i) queue_.push_back({Work::Socket, i});
    if (needed_ & bit(Layer::Collision)) for (size_t i = 0; i < bodies_.size(); ++i) queue_.push_back({Work::Body, i});
    refresh_callouts();
}

void Breakdown::build(const Work& work) {
    auto looks_of = [&](const std::string& bone) {
        for (const auto& b : bones_) if (b.name == bone) return b.looks;
        return all_looks_;   // not a bone we read (a socket): taken as always there
    };
    auto mesh_piece = [&](Layer layer, const char* shape_path, const std::string& socket, Vec3 at, Quat rotation, Vec3 scale, uint32_t looks) {
        const auto shape = load(shape_path);
        const auto mid = material(layer, looks);
        if (!is_handle(shape) || !is_handle(mid)) return;
        const auto xf = transform(at, rotation, scale);
        const auto component = add_component(class_mesh_, xf);
        pieces_.push_back({component, bit(layer), looks});
        call(component, "SetStaticMesh", {{"NewMesh", shape}});
        call(component, "SetMaterial", {{"ElementIndex", 0}, {"Material", mid}});
        call(component, "SetCastShadow", {{"NewCastShadow", false}});
        call(component, "K2_AttachToComponent", {{"Parent", rig_parent()}, {"SocketName", socket}, {"LocationRule", keep_relative},
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
        const double radius = std::clamp(length * content().geometry.radius_per_length, content().geometry.radius_min, content().geometry.radius_max);
        mesh_piece(Layer::Skeleton, path_cone, b.parent, direction * (length / 2), z_to(direction), {radius / 50, radius / 50, length / 100}, b.looks & looks_of(b.parent));
    } else if (work.kind == Work::Joint) {
        const auto& b = bones_[work.index];
        const Layer layer = b.kind == BoneKind::Virtual ? Layer::VirtualBones : b.kind == BoneKind::Ik ? Layer::IkBones : Layer::Springs;
        const double radius = b.kind == BoneKind::Spring ? content().geometry.spring_radius : content().geometry.joint_radius;   // the engine sphere has radius 50
        mesh_piece(layer, path_sphere, b.name, {}, {}, {radius / 50, radius / 50, radius / 50}, b.looks);
    } else if (work.kind == Work::Socket) {
        mesh_piece(Layer::Sockets, path_sphere, sockets_[work.index], {}, {}, {content().geometry.socket_radius / 50, content().geometry.socket_radius / 50, content().geometry.socket_radius / 50}, all_looks_);
    } else {
        // One physics body: every primitive becomes the engine's own shape component on that bone.
        const auto& setup = bodies_[work.index];
        const auto bone = host_.get(setup, "BoneName");
        const auto geometry = host_.get(setup, "AggGeom");
        if (!bone.is_string() || !geometry.is_object()) return;
        const auto colour = colour8(layer_colour(Layer::Collision));
        auto shape = [&](const Json& cls, const Json& element, auto&& size) {
            if (!is_handle(cls)) return;
            const auto component = add_component(cls, transform({}, {}, {1, 1, 1}));
            pieces_.push_back({component, bit(Layer::Collision), looks_of(bone.get<std::string>())});
            call(component, "K2_AttachToComponent", {{"Parent", rig_parent()}, {"SocketName", bone}, {"LocationRule", snap_to_target},
                                                     {"RotationRule", snap_to_target}, {"ScaleRule", keep_relative}, {"bWeldSimulatedBodies", false}});
            host_.request({{"op", "call"}, {"target", component}, {"function", "K2_SetRelativeLocationAndRotation"},
                           {"args", {{"NewLocation", element.value("Center", Json{{"X", 0}, {"Y", 0}, {"Z", 0}})},
                                     {"NewRotation", element.value("Rotation", Json{{"Pitch", 0}, {"Yaw", 0}, {"Roll", 0}})}, {"bSweep", false}, {"bTeleport", true}}}});
            size(component);
            host_.set(component, "ShapeColor", colour);
            call(component, "SetLineThickness", {{"Thickness", content().geometry.physics_line}});
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
    const Json wanted = material ? *material : Json();   // a copy: `material` may be overlay_now_ itself
    for (auto& skin : skins_) {
        try { call(skin.component, "SetOverlayMaterial", {{"NewOverlayMaterial", material ? wanted : skin.overlay_before}}); } catch (...) {}
    }
    overlay_ours_ = material != nullptr;
    overlay_now_ = wanted;
}

// The wardrobe mod is about to change her outfit: nothing of ours may be on her while it does.
void Breakdown::suspend() {
    if (!active_) return;
    try {
        if (transition_end_ >= 0) settle();
        if (clay_slots_) { apply_clay(false); suspended_clay_ = true; }
        if (overlay_ours_) { const Json mine = overlay_now_; set_overlay(nullptr); suspended_ = mine; }
    } catch (const std::exception& e) {
        add_note(std::string("could not step aside for an outfit change: ") + e.what());
    }
}

void Breakdown::follow(double place) {
    if (!active_ || !pak_ || shown_ == 0 || transition_end_ >= 0 || edge_ >= 1.0) return;
    place = std::clamp(place, 0.05, 0.95);
    if (std::abs(place - edge_) < 0.001) return;   // under three pixels on a 2560 wide frame
    try {
        const auto collection = load(content().breakdown.collection);
        if (!is_handle(collection)) return;
        // From and To alike: the line stands there whatever the running clock says.
        for (const char* name : {"WipeFrom", "WipeTo"})
            call(kismet_materials_, "SetScalarParameterValue", {{"WorldContextObject", pawn_}, {"Collection", collection}, {"ParameterName", name}, {"ParameterValue", place}});
        edge_ = place;
    } catch (...) {}
}

void Breakdown::reskin() {
    if (!active_ || !overlay_ours_) return;
    try {
        const size_t before = skins_.size();
        const uint64_t last = skins_.empty() ? 0 : id_of(skins_.back().component);
        find_skins();
        if (skins_.size() == before && (skins_.empty() || id_of(skins_.back().component) == last)) return;   // the same parts
        const Json mine = overlay_now_;
        set_overlay(&mine);
    } catch (...) {}
}

// She wears `look` now: her parts are new, and so is the set of bones she has.
void Breakdown::adopt(const std::string& look) {
    if (!active_) return;
    try {
        if (transition_end_ >= 0) settle();
        const auto it = std::find(looks_.begin(), looks_.end(), look);
        look_now_ = it == looks_.end() ? max_looks + 1 : size_t(it - looks_.begin());
        find_skins();
        refresh_rig_parent();   // her new outfit's mesh: the pieces' bones come from it
        if (body_hidden_) hide_body(true);
        if (is_handle(suspended_)) { const Json mine = suspended_; suspended_ = nullptr; set_overlay(&mine); }
        else if (overlay_ours_) { const Json mine = overlay_now_; set_overlay(&mine); }
        if (suspended_clay_) { suspended_clay_ = false; apply_clay(true); }
        for (const auto& g : groups_) set_param(g.mid, "Show", (shown_ & bit(g.layer)) && worn(g.looks) ? 1.0 : 0.0);
        if (shown_ & bit(Layer::Collision))
            for (const auto& piece : pieces_)
                if ((piece.layer & bit(Layer::Collision)) && piece.looks != all_looks_) { try { call(piece.component, "SetHiddenInGame", {{"NewHidden", !worn(piece.looks)}, {"bPropagateToChildren", false}}); } catch (...) {} }
        refresh_callouts();
    } catch (const std::exception& e) {
        add_note(std::string("could not follow an outfit change: ") + e.what());
    }
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

void Breakdown::show(LayerSet set, double wipe_seconds, double stop) {
    if (!active_) return;
    try {
        if (transition_end_ >= 0) settle();
        set &= needed_;
        // One overlay slot: where clay may not go into her own slots, wireframe has it alone.
        if (options_.overlay_only && (set & bit(Layer::Wireframe))) set &= ~bit(Layer::Clay);
        stop = pak_ ? std::clamp(stop, 0.05, 1.0) : 1.0;
        // A line that rests, or has to move from, part of the way: only the line moves and
        // what is left of it changes at once. A whole frame to a whole frame is the classic
        // wipe below, with the outgoing layers ahead of the line.
        if (edge_ < 1.0 || (stop < 1.0 && set != 0)) { edge_show(set, wipe_seconds, stop); return; }
        const LayerSet incoming = set & ~shown_, outgoing = shown_ & ~set, staying = shown_ & set;
        if (!incoming && !outgoing) return;
        const bool wiping = pak_ && wipe_seconds > 0.01;
        wipe_numbers(wiping ? 0 : 1, 1, wiping ? wipe_seconds : 0.01);
        const double now = wiping || ((incoming | outgoing) & (bit(Layer::Collision) | bit(Layer::Capsule) | bit(Layer::NoBody))) ? game_time() : 0;
        // Where a layer stands in this transition: coming in behind the wipe, going out ahead
        // of it, or staying whatever the wipe does.
        auto place = [&](const Json& mid, bool in, bool out) {
            if (!is_handle(mid)) return;
            if (in) { set_param(mid, "Show", 1); set_param(mid, "Invert", 0); set_param(mid, "Hold", wiping ? 0 : 1); }
            else if (out) { if (wiping) { set_param(mid, "Invert", 1); set_param(mid, "Hold", 0); } else set_param(mid, "Show", 0); }
            else set_param(mid, "Hold", 1);
        };
        for (const auto& g : groups_)
            if (((incoming | outgoing | staying) & bit(g.layer)) && worn(g.looks)) place(g.mid, incoming & bit(g.layer), outgoing & bit(g.layer));

        // Her mesh has one overlay slot. Wireframe always uses it; clay borrows it for its own
        // wipe when wireframe is not involved, and otherwise changes her slot materials at once.
        const LayerSet wire = bit(Layer::Wireframe), clay = bit(Layer::Clay);
        const bool overlay_taken = (incoming | outgoing | staying) & wire;
        if ((incoming | staying) & wire) {
            wire_mid();
            place(wire_, incoming & wire, false);
            if (incoming & wire) set_overlay(&wire_);
        } else if (outgoing & wire) {
            place(wire_, false, true);
            if (!wiping) set_overlay(nullptr);
        }
        if (incoming & clay) {
            clay_mid();
            if (!overlay_taken && (wiping || options_.overlay_only)) { place(clay_, true, false); set_overlay(&clay_); }
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
                    set_param(pass_[next], "Gamma", *want == Layer::Depth ? content().breakdown.depth_gamma : content().materials.gamma);
                    place(pass_[next], true, false);
                }
                if (have) place(pass_[pass_now_], false, true);
                pass_now_ = next;
            } else if (want) set_param(pass_[pass_now_], "Hold", 1);
        }
        // Shape components cannot wipe: they change in the middle of it.
        shapes_pending_ = (incoming | outgoing) & (bit(Layer::Collision) | bit(Layer::Capsule) | bit(Layer::NoBody));
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

// Only the line moves: from where it rests to `stop` (or back to the left edge when nothing
// is to be shown). What is left of it becomes `set` at once; right of it is the final picture.
void Breakdown::edge_show(LayerSet set, double wipe_seconds, double stop) {
    const LayerSet before = shown_, wire = bit(Layer::Wireframe), clay = bit(Layer::Clay), shapes = bit(Layer::Collision) | bit(Layer::Capsule);
    const bool leaving = set == 0;
    if (leaving && before == 0) return;
    const double from = before == 0 ? 0.0 : edge_, to = leaving ? 0.0 : stop;
    if (set == before && std::abs(to - from) < 1e-4) return;
    const double seconds = std::max(wipe_seconds, 0.01), now = game_time();
    auto left = [&](const Json& mid) { set_param(mid, "Show", 1); set_param(mid, "Invert", 0); set_param(mid, "Hold", 0); };
    if (!leaving && set != before) {
        for (const auto& g : groups_) {
            if ((set & bit(g.layer)) && worn(g.looks)) left(g.mid);
            else if (before & bit(g.layer)) set_param(g.mid, "Show", 0);
        }
        if (set & wire) {
            wire_mid();
            left(wire_);
            if (!overlay_ours_ || id_of(overlay_now_) != id_of(wire_)) set_overlay(&wire_);
            // Clay under a wireframe sits in her slots, so it covers the whole frame.
            if (set & clay) {
                clay_mid();
                set_param(clay_, "Show", 1); set_param(clay_, "Invert", 0); set_param(clay_, "Hold", 1);
                apply_clay(true);
            } else if (clay_slots_) apply_clay(false);
        } else if (set & clay) {
            clay_mid();
            if (clay_slots_) apply_clay(false);
            left(clay_);
            if (!overlay_ours_ || id_of(overlay_now_) != id_of(clay_)) set_overlay(&clay_);
        } else {
            if (clay_slots_) apply_clay(false);
            if (overlay_ours_) set_overlay(nullptr);
        }
        if (frame_passes_) {
            const auto want = first_frame_layer(set);
            set_param(pass_[1 - pass_now_], "Show", 0);
            if (want) {
                for (auto l : {Layer::BaseColor, Layer::Normals, Layer::Roughness, Layer::Metallic, Layer::Depth, Layer::Occlusion})
                    set_param(pass_[pass_now_], pass_weight(l), l == *want ? 1.0 : 0.0);
                set_param(pass_[pass_now_], "Gamma", *want == Layer::Depth ? content().breakdown.depth_gamma : content().materials.gamma);
                left(pass_[pass_now_]);
            } else set_param(pass_[pass_now_], "Show", 0);
        }
    } else if (!leaving) {
        // The same layers, a new place for the line. Clay in her slots cannot be split.
        if (clay_slots_ && to < 1.0 && !(set & wire)) { apply_clay(false); left(clay_); set_overlay(&clay_); }
    }
    wipe_numbers(from, to, seconds);
    shapes_pending_ = (before ^ set) & shapes;
    shapes_flip_at_ = shapes_pending_ ? now + (leaving ? seconds / 2 : 0) : -1;
    edge_mode_ = true;
    edge_to_ = to;
    leaving_ = leaving ? before : 0;
    shown_ = set;
    transition_end_ = now + seconds;
    refresh_callouts();
}

// The wipe has passed: outgoing layers are off, and every switch is back to plain "shown".
void Breakdown::settle() {
    transition_end_ = -1;
    const LayerSet wire = bit(Layer::Wireframe), clay = bit(Layer::Clay);
    if (edge_mode_) {
        edge_mode_ = false;
        if (edge_to_ <= 0.0) {
            // The line went back to the left edge: nothing is shown.
            for (const auto& g : groups_) if (leaving_ & bit(g.layer)) set_param(g.mid, "Show", 0);
            if (clay_slots_) apply_clay(false);
            if (overlay_ours_) set_overlay(nullptr);
            if (frame_passes_) for (auto& pass : pass_) set_param(pass, "Show", 0);
            leaving_ = 0;
            edge_ = 1.0;
            wipe_numbers(1, 1, 0.01);
        } else if (edge_to_ >= 1.0) {
            // The whole frame, as after any wipe: clay moves into her slots where it may.
            edge_ = 1.0;
            if ((shown_ & clay) && !(shown_ & wire) && !clay_slots_ && !options_.overlay_only) {
                set_param(clay_, "Hold", 1);
                apply_clay(true);
                set_overlay(nullptr);
            }
            wipe_numbers(1, 1, 0.01);
        } else edge_ = edge_to_;   // the line stays where it stopped
        return;
    }
    for (const auto& g : groups_) {
        if (shown_ & bit(g.layer)) { if (worn(g.looks)) { set_param(g.mid, "Invert", 0); set_param(g.mid, "Hold", 0); set_param(g.mid, "Show", 1); } }
        else if (leaving_ & bit(g.layer)) { set_param(g.mid, "Show", 0); set_param(g.mid, "Invert", 0); }
    }
    const bool clay_overlay = options_.overlay_only && (shown_ & clay);
    if (clay_overlay) { set_param(clay_, "Invert", 0); set_param(clay_, "Hold", 0); }
    else if ((shown_ & clay) && !clay_slots_) { set_param(clay_, "Hold", 1); set_param(clay_, "Invert", 0); apply_clay(true); }
    if (shown_ & wire) { set_overlay(&wire_); set_param(wire_, "Invert", 0); set_param(wire_, "Hold", 0); }
    else if (clay_overlay) { if (!overlay_ours_) set_overlay(&clay_); }
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
        // The wardrobe mod repaints her under a running breakdown (a palette, a piece): if that
        // took our overlay off the body, it goes back on, over whatever is there now.
        if (overlay_ours_ && ++watch_ >= 5) {
            watch_ = 0;
            try { if (!ours(host_.get(mesh_, "OverlayMaterial"))) { const Json mine = overlay_now_; overlay_ours_ = false; find_skins(); set_overlay(&mine); } } catch (...) {}
        }
        if (transition_end_ < 0 && shapes_flip_at_ < 0) return;
        const double now = game_time();
        if (shapes_flip_at_ >= 0 && now >= shapes_flip_at_) {
            shapes_flip_at_ = -1;
            if (shapes_pending_ & bit(Layer::Collision)) {
                const bool hide = !(shown_ & bit(Layer::Collision));
                for (const auto& piece : pieces_) if (piece.layer & bit(Layer::Collision)) { try { call(piece.component, "SetHiddenInGame", {{"NewHidden", hide}, {"bPropagateToChildren", false}}); } catch (...) {} }
            }
            if (shapes_pending_ & bit(Layer::NoBody)) hide_body(shown_ & bit(Layer::NoBody));
            if ((shapes_pending_ & bit(Layer::Capsule)) && is_handle(capsule_)) {
                if (!capsule_touched_) {
                    capsule_touched_ = true;
                    const auto hidden = host_.get(capsule_, "bHiddenInGame");
                    capsule_hidden_before_ = !hidden.is_boolean() || hidden.get<bool>();
                    try { capsule_colour_before_ = host_.get(capsule_, "ShapeColor"); } catch (...) {}
                    host_.set(capsule_, "ShapeColor", colour8(layer_colour(Layer::Capsule)));
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
    auto bone_like = [&](const std::vector<std::string>& wanted) -> std::string {
        for (const auto& want : wanted) for (const auto& b : bones_) if (worn(b.looks) && lower(b.name) == want) return b.name;
        return bones_.empty() ? std::string(*wanted.begin()) : std::string();
    };
    auto count = [&](BoneKind kind) { return std::count_if(bones_.begin(), bones_.end(), [&](const Bone& b) { return b.kind == kind && worn(b.looks); }); };
    auto first = [&](BoneKind kind) -> std::string { for (const auto& b : bones_) if (b.kind == kind && worn(b.looks)) return b.name; return {}; };
    auto add = [&](std::string text, const Json& component, std::string socket) { if (is_handle(component)) callouts_.push_back({std::move(text), component, std::move(socket)}); };
    if (shown_ & bit(Layer::Wireframe)) add(content().annotations.wire, mesh_, bone_like(content().annotations.head));
    else if (shown_ & bit(Layer::Clay)) add(content().annotations.clay, mesh_, bone_like(content().annotations.head));
    if (shown_ & bit(Layer::Skeleton)) add(annotation(content().annotations.skeleton, bones_worn()), mesh_, bone_like(content().annotations.torso));
    // Every bone of a kind this look has, spread over her: the first, then the ones furthest
    // along the list (bones are listed head to toe, left before right).
    auto spread = [&](auto&& keep) {
        std::vector<std::string> all;
        for (const auto& b : bones_) if (worn(b.looks) && keep(b)) all.push_back(b.name);
        std::vector<std::string> out;
        const size_t n = std::min(all.size(), size_t(content().annotations.spread_targets));
        for (size_t k = 0; k < n; ++k) out.push_back(all[n == 1 ? 0 : k * (all.size() - 1) / (n - 1)]);
        return out;
    };
    auto add_many = [&](std::string text, std::vector<std::string> places) {
        if (places.empty() || !is_handle(mesh_)) return;
        Callout c{std::move(text), mesh_, places.front(), {}};
        c.more.assign(places.begin() + 1, places.end());
        callouts_.push_back(std::move(c));
    };
    if ((shown_ & bit(Layer::VirtualBones)) && count(BoneKind::Virtual))
        add_many(annotation(content().annotations.virtual_bones, size_t(count(BoneKind::Virtual))), spread([](const Bone& b) { return b.kind == BoneKind::Virtual; }));
    if ((shown_ & bit(Layer::IkBones)) && count(BoneKind::Ik))
        add_many(annotation(content().annotations.ik, size_t(count(BoneKind::Ik))), spread([](const Bone& b) { return b.kind == BoneKind::Ik; }));
    if (shown_ & bit(Layer::Springs)) {
        // One label per group (chest, glutes, belly...), an arrow to each chain of it: the
        // chain's first bone, the one whose parent is not in the group.
        std::vector<std::string> groups;
        for (const auto& b : bones_) {
            if (b.kind != BoneKind::Spring || !worn(b.looks)) continue;
            auto group = spring_group(b.name); if (group.empty()) group = content().annotations.spring_fallback;
            if (std::find(groups.begin(), groups.end(), group) == groups.end()) groups.push_back(group);
        }
        for (const auto& group : groups) {
            auto in_group = [&](const std::string& name) { for (const auto& b : bones_) if (b.name == name) { auto g = spring_group(b.name); return b.kind == BoneKind::Spring && (g.empty() ? content().annotations.spring_fallback : g) == group; } return false; };
            std::vector<std::string> roots, rest;
            for (const auto& b : bones_) {
                if (b.kind != BoneKind::Spring || !worn(b.looks)) continue;
                auto g = spring_group(b.name); if ((g.empty() ? content().annotations.spring_fallback : g) != group) continue;
                (in_group(b.parent) ? rest : roots).push_back(b.name);
            }
            std::vector<std::string> places(roots.begin(), roots.begin() + std::min(roots.size(), max_arrows));
            for (size_t k = 0; places.size() < std::min<size_t>(size_t(content().annotations.preferred_targets), roots.size() + rest.size()) && k < rest.size(); ++k) places.push_back(rest[k]);
            add_many(annotation(content().annotations.springs, 0, group), places);
        }
    }
    if ((shown_ & bit(Layer::Sockets)) && !sockets_.empty()) {
        // Arrows to the sockets where things hang: her hands, her head, her back, else spread.
        std::vector<std::string> places;
        for (const auto& group : content().annotations.socket_groups) {
            const auto& want = group.contains;
            for (const auto& socket : sockets_) {
                const auto l = lower(socket);
                if (std::any_of(want.begin(), want.end(), [&](const std::string& w) { return l.find(w) != std::string::npos; }) && std::find(places.begin(), places.end(), socket) == places.end()) { places.push_back(socket); break; }
            }
        }
        for (size_t k = 0; places.size() < std::min(max_arrows, sockets_.size()) && k < sockets_.size(); k += std::max<size_t>(1, sockets_.size() / max_arrows))
            if (std::find(places.begin(), places.end(), sockets_[k]) == places.end()) places.push_back(sockets_[k]);
        add_many(annotation(content().annotations.sockets, sockets_.size()), places);
    }
    if ((shown_ & bit(Layer::Collision)) && !bodies_.empty()) add(annotation(content().annotations.physics, bodies_.size()), mesh_, bone_like(content().annotations.physics_bones));
    if (shown_ & bit(Layer::Capsule)) add(content().annotations.capsule, capsule_, "");
}

std::string Breakdown::caption() const {
    std::string text;
    for (const auto& style : content().breakdown.layers)
        if (shown_ & bit(*layer_from(style.id))) text += (text.empty() ? "" : content().breakdown.separator) + style.label;
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
    for (const auto& g : groups_) step("layer", [&] { set_param(g.mid, "Show", 0); });
    if (body_hidden_) step("body", [&] { hide_body(false); });
    for (auto& piece : pieces_) {
        if (piece.layer & bit(Layer::Collision)) { try { call(piece.component, "SetHiddenInGame", {{"NewHidden", true}, {"bPropagateToChildren", false}}); } catch (...) {} }
        teardown_.push_back(std::move(piece.component));
    }
    if (is_handle(proxy_)) teardown_.push_back(std::move(proxy_));   // after its pieces
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
    pawn_ = mesh_ = capsule_ = camera_ = statics_ = kismet_materials_ = proxy_ = nullptr;
    body_hidden_ = false;
    class_mesh_ = class_capsule_ = class_sphere_ = class_box_ = nullptr;
    groups_.clear(); looks_.clear();
    look_now_ = 0; all_looks_ = 1; options_ = {};
    edge_mode_ = false; edge_ = edge_to_ = 1.0;
    suspended_ = overlay_now_ = nullptr; suspended_clay_ = assembled_ = false; read_index_ = 0; watch_ = 0;
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

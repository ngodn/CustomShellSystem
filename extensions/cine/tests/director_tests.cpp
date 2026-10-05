// CINE director tests against a mock CSSX host that answers the calls CINE makes and moves
// her toward the path-following goal like the game would. Run with the extension directory
// as argv[1].
#include "cine.hpp"
#include "manifest.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <chrono>
#include <set>

using cine::Json;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void expect(bool ok, const std::string& what) { ++checks; if (!ok) throw std::runtime_error(what); }
Json object(uint64_t id) { return {{"$object", id}}; }
uint64_t id_of(const Json& j) { return j.is_object() && j.contains("$object") ? j["$object"].get<uint64_t>() : 0; }

struct Host {
    fs::path dir;
    Json state = Json::object();
    uint64_t next = 100;
    std::set<uint64_t> cameras, destroyed;
    uint64_t view = 1;                  // current view target ($object id); 1 = pawn
    int hud_visibility = 4, force_walk = 0, move_component = 0, speed_adjust_removed = 0, place_player = 0;
    bool speed_adjusted = false, moving = false, fail_move = false, hit_walls = false;
    double speed_limit = 0;
    cine::Vec3 pos{0, 0, 100}, goal{0, 0, 100};
    double yaw = 0;
    Json keys = Json::object();
    std::vector<Json> css_applies;
    bool css_present = true;
    int css_refuse_restores = 0;   // the next N restore applies fail like CSS between looks
    size_t requests = 0;

    // ---- A small modelled world, off for the older tests. Objects carry real properties, so
    // a test can compare the whole world before CINE touched it and after.
    struct Thing { std::string kind; Json props = Json::object(); };
    bool model = false, pak = true, old_bridge = false;
    std::map<uint64_t, Thing> things;
    std::map<std::string, uint64_t> paths;
    std::set<uint64_t> components;        // attached breakdown pieces alive now
    size_t components_made = 0, projections = 0, view_points = 0, pauses = 0, lock_releases = 0;
    Json mpc = Json::object();            // MPC_Cine scalars as last written
    double game_time = 0, slowest = 1.0;
    int effect_handles = 0;
    uint64_t hair = 0;                    // a second skinned part on the pawn, with an overlay of its own
    uint64_t crown = 0;                   // a static mesh accessory on the pawn
    const std::vector<std::pair<std::string, std::string>> bones{
        {"root", "None"}, {"pelvis", "root"}, {"spine_01", "pelvis"}, {"spine_02", "spine_01"}, {"spine_03", "spine_02"}, {"head", "spine_03"},
        {"thigh_r", "pelvis"}, {"VB hand_r", "root"}, {"ik_hand_r", "root"}, {"ik_foot_r", "root"}, {"Brust_L", "spine_03"}, {"Butt_R", "pelvis"}, {"skirt_01", "pelvis"}};
    enum : uint64_t { Pawn = 1, Controller = 2, World = 3, WorldSettings = 4, Lock = 5, PlayerAsc = 6, PlayerMesh = 7, PlayerCapsule = 8, CameraManager = 9, PlayerHealth = 10 };

    uint64_t make(std::string kind, Json props = Json::object()) { const auto id = next++; things[id] = {std::move(kind), std::move(props)}; return id; }
    Json& prop(uint64_t id, const char* name) { return things.at(id).props[name]; }
    uint64_t part(uint64_t id, const char* name) { return id_of(things.at(id).props.at(name)); }
    Json mesh_props() {
        const auto body = make("body", {{"BoneName", "pelvis"}, {"AggGeom", {{"SphylElems", Json::array({{{"Radius", 10.0}, {"Length", 20.0}}})},
                                        {"SphereElems", Json::array({{{"Radius", 5.0}}})}, {"BoxElems", Json::array({{{"X", 4.0}, {"Y", 4.0}, {"Z", 4.0}}})}}}});
        const auto physics = make("physics", {{"SkeletalBodySetups", Json::array({object(body)})}});
        const auto asset = make("skinned", {{"PhysicsAsset", object(physics)}});
        return {{"bPauseAnims", false}, {"_tick", true}, {"_cloth", false}, {"OverlayMaterial", nullptr}, {"PhysicsAssetOverride", nullptr}, {"_asset", object(asset)},
                {"_materials", Json::array({object(make("material")), object(make("material")), object(make("material"))})}};
    }
    Json capsule_props() { return {{"bHiddenInGame", true}, {"ShapeColor", {{"R", 223}, {"G", 149}, {"B", 157}, {"A", 255}}}}; }
    void populate() {
        model = true;
        things[Pawn] = {"pawn", {{"LockOnComponent", object(Lock)}, {"AbilitySystemComponent", object(PlayerAsc)}, {"Mesh", object(PlayerMesh)},
                                 {"CapsuleComponent", object(PlayerCapsule)}, {"HealthComponent", object(PlayerHealth)}, {"CharacterMovement", object(make("movement"))}}};
        things[Controller] = {"player_controller", {{"PlayerCameraManager", object(CameraManager)}}};
        things[World] = {"world", {}};
        things[WorldSettings] = {"world_settings", {{"CinematicTimeDilation", 1.0}}};
        things[Lock] = {"lock", {{"LockedTarget", nullptr}}};
        things[PlayerAsc] = {"asc", {{"_effects", Json::array()}, {"_tags", Json::object()}}};
        things[PlayerMesh] = {"mesh", mesh_props()};
        things[PlayerCapsule] = {"capsule", capsule_props()};
        things[CameraManager] = {"camera_manager", {{"_shake", 1.0}}};
        things[PlayerHealth] = {"health", {{"_health", 1.0}, {"_dead", false}}};
        hair = make("mesh", {{"OverlayMaterial", object(make("material"))}, {"_materials", Json::array({object(make("material")), object(make("material"))})}});
        crown = make("mesh", {{"OverlayMaterial", nullptr}, {"_materials", Json::array({object(make("material"))})}});
    }
    uint64_t add_character(const char* team, cine::Vec3 at = {300, 0, 100}) {
        Json props{{"bHidden", false}, {"Team", {{"TagName", team}}}, {"_collision", true}, {"_boss", nullptr}, {"_pos", vec(at)}};
        props["Mesh"] = object(make("mesh", mesh_props()));
        props["CapsuleComponent"] = object(make("capsule", capsule_props()));
        props["CharacterMovement"] = object(make("movement", {{"_tick", true}}));
        props["Controller"] = object(make("controller", {{"BrainComponent", object(make("brain", {{"_running", true}}))}}));
        props["HealthComponent"] = object(make("health", {{"_health", 1.0}, {"_dead", false}}));
        props["HitTraceComponent"] = object(make("hit", {{"_on", true}}));
        props["VoxComponent"] = object(make("vox", {{"m_bVoxEnabled", true}}));
        props["FoleyComponent"] = object(make("foley", {{"_loop", true}}));
        props["AbilitySystemComponent"] = object(make("asc", {{"_effects", Json::array()}, {"_tags", Json::object()}}));
        props["Enemy Health Widget"] = object(make("bar", {{"_visible", true}}));
        props["BPC_LockOnTarget"] = object(make("marker", {{"WBP_LockOnIndicator", object(make("indicator", {{"_visibility", 0}}))}}));
        props["_weapon"] = object(make("weapon", {{"bHidden", false}}));
        return make("ai", std::move(props));
    }
    // Everything a restore must put back: the world's own objects, not what CINE created.
    Json snapshot(const std::set<uint64_t>& skip = {}) const {
        Json out = Json::object();
        for (const auto& [id, t] : things) {
            if (skip.contains(id) || destroyed.contains(id) || t.kind.starts_with("asset:")) continue;
            if (t.kind == "mid" || t.kind == "component" || t.kind == "camera" || t.kind == "camera_component" || t.kind == "root") continue;
            out[std::to_string(id) + " " + t.kind] = t.props;
        }
        return out;
    }
    std::string actor_kind(uint64_t cls) const {
        const auto it = things.find(cls);
        if (it == things.end()) return {};
        const auto& k = it->second.kind;
        if (k.ends_with("BP_AICharacter_C")) return "ai";
        if (k.ends_with("BP_SpawnTrigger_C")) return "trigger";
        if (k.ends_with("BP_Spawner_Proximity_C")) return "proximity";
        if (k.ends_with("BP_Spawner_C")) return "spawner";
        if (k.ends_with("SpartaProjectile")) return "projectile";
        return {};
    }
    Json by_path(const std::string& path) {
        if (!pak && path.starts_with("/Game/CSSX/CINE/")) return nullptr;
        if (const auto it = paths.find(path); it != paths.end()) return object(it->second);
        const auto id = make("asset:" + path);
        paths[path] = id;
        return object(id);
    }
    bool model_call(uint64_t target, const std::string& fn, const Json& a, Json& out) {
        auto ret = [&](Json v) { out = Json{{"ReturnValue", std::move(v)}}; return true; };
        auto done = [&] { out = Json::object(); return true; };
        if (fn == "GetTimeSeconds") return ret(game_time);
        if (fn == "K2_GetWorldSettings") return ret(object(WorldSettings));
        if (fn == "GetAllActorsOfClass") {
            const auto kind = actor_kind(id_of(a.at("ActorClass")));
            Json list = Json::array();
            for (const auto& [id, t] : things) if (!kind.empty() && t.kind == kind && !destroyed.contains(id)) list.push_back(object(id));
            out = Json{{"OutActors", list}};
            return true;
        }
        if (fn == "GetPlayerViewPoint") { ++view_points; out = Json{{"Location", vec(pos + cine::Vec3{-400, 0, 20})}, {"Rotation", {{"Pitch", 0.0}, {"Yaw", 0.0}, {"Roll", 0.0}}}}; return true; }
        if (fn == "ProjectWorldLocationToWidgetPosition") {   // a 90 degree lens looking along +X from the view point, on a 2560x1440 HUD
            ++projections;
            const cine::Vec3 d = cine::Vec3{a["WorldLocation"]["X"], a["WorldLocation"]["Y"], a["WorldLocation"]["Z"]} - (pos + cine::Vec3{-400, 0, 20});
            out = Json{{"ReturnValue", d.x > 1}, {"ScreenPosition", {{"X", 1280.0 + 1280.0 * d.y / d.x}, {"Y", 720.0 - 1280.0 * d.z / d.x}}}};
            return true;
        }
        if (fn == "CreateDynamicMaterialInstance") return ret(object(make("mid", {{"_parent", a.at("Parent")}, {"_params", Json::object()}})));
        if (fn == "SetScalarParameterValue" && a.contains("Collection")) { mpc[a.at("ParameterName").get<std::string>()] = a.at("ParameterValue"); return done(); }
        const auto it = things.find(target);
        if (it == things.end()) return false;
        Thing& t = it->second;
        auto flag = [&](const char* name) -> Json& { return t.props[name]; };
        auto drop = [](Json& list, auto&& match) { for (size_t i = list.size(); i-- > 0;) if (match(list[i])) list.erase(i); };
        if (fn == "K2_GetComponentsByClass") {
            const bool skinned = things.at(id_of(a.at("ComponentClass"))).kind.ends_with("SkinnedMeshComponent");
            if (t.kind == "pawn") return ret(skinned ? Json::array({object(PlayerMesh), object(hair)}) : Json::array({object(crown)}));
            return ret(skinned && t.props.contains("Mesh") ? Json::array({t.props.at("Mesh")}) : Json::array());
        }
        if (fn == "GetAttachedActors") { out = Json{{"OutActors", t.props.contains("_weapon") ? Json::array({t.props["_weapon"]}) : Json::array()}}; return true; }
        if (fn == "K2_GetActorLocation" && t.props.contains("_pos")) return ret(t.props["_pos"]);
        if (fn == "IsDeadOrDying") return ret(flag("_dead"));
        if (fn == "GetHealthNormalized") return ret(flag("_health"));
        if (fn == "IsComponentTickEnabled") return ret(flag("_tick"));
        if (fn == "SetComponentTickEnabled") { flag("_tick") = a.at("bEnabled"); return done(); }
        if (fn == "IsRunning") return ret(flag("_running"));
        if (fn == "StartLogic") { flag("_running") = true; return done(); }
        if (fn == "Pause") {   // what the game's Pause() does, per its blueprint
            ++pauses;
            prop(part(target, "Mesh"), "bPauseAnims") = true; prop(part(target, "Mesh"), "_tick") = false;
            prop(part(target, "CharacterMovement"), "_tick") = false; prop(part(part(target, "Controller"), "BrainComponent"), "_running") = false;
            return done();
        }
        if (fn == "GetActorEnableCollision") return ret(flag("_collision"));
        if (fn == "SetActorEnableCollision") { flag("_collision") = a.at("bNewActorEnableCollision"); return done(); }
        if (fn == "SetActorHiddenInGame") { flag("bHidden") = a.at("bNewHidden"); return done(); }
        if (fn == "IsHitProcessingEnabled") return ret(flag("_on"));
        if (fn == "DisableHitProcessing" || fn == "EnableHitProcessing") { flag("_on") = fn[0] == 'E'; return done(); }
        if (fn == "BP_ApplyGameplayEffectToTarget" || fn == "BP_ApplyGameplayEffectToSelf") {
            const auto to = fn.ends_with("Self") ? target : id_of(a.at("Target"));
            const int h = ++effect_handles;
            prop(to, "_effects").push_back({{"Handle", h}, {"cls", id_of(a.at("GameplayEffectClass"))}});
            return ret({{"Handle", h}});
        }
        if (fn == "RemoveActiveGameplayEffect") {
            const int h = a.at("Handle").at("Handle");
            const auto before = flag("_effects").size();
            drop(flag("_effects"), [&](const Json& e) { return e["Handle"] == h; });
            return ret(flag("_effects").size() < before);
        }
        if (fn == "RemoveActiveGameplayEffectBySourceEffect") { const auto cls = id_of(a.at("GameplayEffect")); drop(flag("_effects"), [&](const Json& e) { return e["cls"] == cls; }); return done(); }
        if (fn == "GetGameplayTagCount") return ret(flag("_tags").value(a.at("GameplayTag").at("TagName").get<std::string>(), 0));
        if (fn == "SetVocalizationsEnabled") { flag("m_bVoxEnabled") = a.at("in_bEnabled"); return done(); }
        if (fn == "IsPlayingFoleyLoop") return ret(flag("_loop"));
        if (fn == "StopFoleyLoop" || fn == "PlayFoleyLoop") { flag("_loop") = fn[0] == 'P'; return done(); }
        if (fn == "InterruptPlayingIdsOnComponents") return done();
        if (fn == "IsClothingSimulationSuspended") return ret(flag("_cloth"));
        if (fn == "SuspendClothingSimulation" || fn == "ResumeClothingSimulation") { flag("_cloth") = fn[0] == 'S'; return done(); }
        if (fn == "GetComponentByClass") return ret(t.props.value("_boss", Json()));
        if (fn == "TargetSpecificCharacter") { ++lock_releases; flag("LockedTarget") = a.at("Target"); return done(); }
        if (fn == "ToggleTargetLock") { flag("LockedTarget") = nullptr; return ret(true); }
        if (fn == "GetGlobalCameraShakeScale") return ret(flag("_shake"));
        if (fn == "SetGlobalCameraShakeScale") { flag("_shake") = a.at("NewGlobalCameraShakeScale"); return done(); }
        if (fn == "IsVisible") return ret(flag("_visible"));
        if (fn == "SetVisibility" && a.contains("bNewVisibility")) { flag("_visible") = a.at("bNewVisibility"); return done(); }
        if (fn == "GetVisibility" && t.props.contains("_visibility")) return ret(flag("_visibility"));
        if (fn == "SetVisibility" && t.props.contains("_visibility")) { flag("_visibility") = a.at("InVisibility"); return done(); }
        // The breakdown's side.
        if (fn == "GetNumBones") return ret(bones.size());
        if (fn == "GetBoneName") return ret(bones.at(a.at("BoneIndex").get<size_t>()).first);
        if (fn == "GetParentBone") { for (const auto& [name, parent] : bones) if (name == a.at("BoneName").get<std::string>()) return ret(parent); return ret("None"); }
        if (fn == "GetRefPoseTransform") return ret({{"Translation", {{"X", 0.0}, {"Y", 3.0}, {"Z", 12.0}}}});
        if (fn == "GetAllSocketNames") return ret(Json::array({"Socket_Target", "weapon_r"}));
        if (fn == "GetSkinnedAsset") return ret(flag("_asset"));
        if (fn == "GetNumMaterials") return ret(flag("_materials").size());
        if (fn == "GetMaterial") return ret(flag("_materials").at(a.at("ElementIndex").get<size_t>()));
        if (fn == "SetMaterial") { if (t.kind == "mesh") flag("_materials").at(a.at("ElementIndex").get<size_t>()) = a.at("Material"); return done(); }
        if (fn == "SetOverlayMaterial") { flag("OverlayMaterial") = a.at("NewOverlayMaterial"); return done(); }
        if (fn == "AddComponentByClass") {
            const auto id = make("component", {{"_class", a.at("Class")}, {"_hidden", true}, {"_socket", ""}});
            components.insert(id); ++components_made;
            return ret(object(id));
        }
        if (fn == "K2_AttachToComponent") { flag("_socket") = a.at("SocketName"); return ret(true); }
        if (fn == "SetHiddenInGame") { flag(t.kind == "capsule" ? "bHiddenInGame" : "_hidden") = a.at("NewHidden"); return done(); }
        if (fn == "K2_DestroyComponent") { destroyed.insert(target); components.erase(target); return done(); }
        if (fn == "AddOrUpdateBlendable") {
            if (old_bridge) throw std::runtime_error("Cannot write this reflected property type");
            flag("_blendables").push_back(a.at("InBlendableObject"));
            return done();
        }
        if (fn == "RemoveBlendable") { drop(flag("_blendables"), [&](const Json& e) { return e == a.at("InBlendableObject"); }); return done(); }
        if (fn == "SetScalarParameterValue") { flag("_params")[a.at("ParameterName").get<std::string>()] = a.at("Value"); return done(); }
        for (const char* quiet : {"SetVectorParameterValue", "SetCollisionEnabled", "SetStaticMesh", "SetCastShadow", "K2_SetRelativeTransform", "SetCapsuleSize",
                                  "SetSphereRadius", "SetBoxExtent", "SetLineThickness"})
            if (fn == quiet) return done();
        return false;
    }
    size_t param_count(const char* kind_parent, const char* name, double value) {   // instances of a material with this switch at this value
        size_t n = 0;
        for (const auto& [id, t] : things) {
            if (t.kind != "mid") continue;
            const auto parent = things.find(id_of(t.props.at("_parent")));
            if (parent != things.end() && parent->second.kind.ends_with(kind_parent) && t.props.at("_params").value(name, -1.0) == value) ++n;
        }
        return n;
    }
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};
    static int request(void* context, const char* value, CssxSink sink, void* output) {
        try { auto r = static_cast<Host*>(context)->handle(Json::parse(value)).dump(); sink(output, r.data(), r.size()); return 1; }
        catch (const std::exception& e) { auto r = Json{{"error", e.what()}}.dump(); sink(output, r.data(), r.size()); return 0; }
    }
    Json vec(cine::Vec3 v) const { return {{"X", v.x}, {"Y", v.y}, {"Z", v.z}}; }
    Json handle(const Json& j) {
        ++requests;
        const auto op = j.at("op").get<std::string>();
        if (op == "state.load") return state;
        if (op == "state.save") { state = j.at("value"); return true; }
        if (op == "log" || op == "invalidate" || op == "menu.close") return nullptr;
        if (op == "extension.info") return {{"directory", dir.string()}, {"id", "eins0fx.cine"}};
        if (op == "player") return model ? Json{{"pawn", object(Pawn)}, {"controller", object(Controller)}, {"world", object(World)}} : Json{{"pawn", object(1)}, {"controller", object(2)}};
        if ((op == "find" || op == "load") && model) return by_path(j.at("path").get<std::string>());
        if (op == "find" || op == "load") return object(next++);
        // Like the real bridge: class_default resolves a class name, never an asset path.
        if (op == "class_default") { const auto c = j.at("class").get<std::string>(); return c.find('/') == std::string::npos && c.size() <= 96 ? object(next++) : Json(nullptr); }
        if (op == "valid") return !destroyed.contains(id_of(j.at("target")));
        if (op == "input.keys") { Json r = Json::object(); for (const auto& k : j.at("keys")) r[k.get<std::string>()] = keys.value(k.get<std::string>(), false); return r; }
        if (op == "get" && model) {
            const auto id = id_of(j.at("target"));
            if (destroyed.contains(id)) throw std::runtime_error("Object is no longer valid");
            const auto name = j.at("property").get<std::string>();
            if (name == "ActiveCameraActor") return cameras.contains(view) ? object(view) : Json(nullptr);
            if (const auto it = things.find(id); it != things.end()) {
                if (!it->second.props.contains(name)) throw std::runtime_error("No property " + name + " on " + it->second.kind);
                return it->second.props[name];
            }
        }
        if (op == "get") return object(next++);
        if (op == "set" && model) {
            if (const auto it = things.find(id_of(j.at("target"))); it != things.end()) {
                const auto name = j.at("property").get<std::string>();
                if (name == "PostProcessSettings") it->second.props[name].update(j.at("value")); else it->second.props[name] = j.at("value");
                if (name == "CinematicTimeDilation") slowest = std::min(slowest, j.at("value").get<double>());
            }
            return true;
        }
        if (op == "set") return true;
        if (op == "service.call") {
            if (j.at("service") != "css.customize" || j.value("version", 0) != 1) throw std::runtime_error("unexpected service call");
            if (!css_present) throw std::runtime_error("No service named css.customize is loaded");
            const auto& r = j.at("request");
            if (r.at("action") == "describe")
                return {{"palette", "original"}, {"customize", {{"palette", "original"}, {"values", {{"coat", {1, 0, 0, 1}}}}}},
                        {"palettes", Json::array({{{"id", "original"}}, {{"id", "crimson"}}, {{"id", "midnight"}}})},
                        {"controls", Json::array({{{"id", "coat"}, {"kind", "toggle"}}, {{"id", "cloth"}, {"kind", "color"}}})}};
            if (css_refuse_restores > 0 && r.value("commands", Json::array()).size() && r["commands"][0].value("action", std::string()) == "restore") {
                --css_refuse_restores; throw std::runtime_error("No CSS appearance is worn");
            }
            css_applies.push_back(r);
            return {{"palette", "x"}};
        }
        if (op != "call") throw std::runtime_error("unexpected op " + op);
        const auto fn = j.at("function").get<std::string>();
        const auto& a = j.value("args", Json::object());
        const uint64_t target = id_of(j.at("target"));
        if (destroyed.contains(target)) throw std::runtime_error("Object is no longer valid");
        if (model) { Json out; if (model_call(target, fn, a, out)) return out; }
        if (fn == "K2_GetActorLocation") return {{"ReturnValue", vec(pos)}};
        if (fn == "GetSocketLocation") return {{"ReturnValue", vec(pos + cine::Vec3{0, 0, 65})}};
        if (fn == "BeginDeferredActorSpawnFromClass") {
            const auto id = next++; cameras.insert(id);
            if (model) {
                const auto component = make("camera_component", {{"FieldOfView", 90.0}, {"PostProcessSettings", Json::object()}, {"_blendables", Json::array()}});
                const auto root = make("root");
                things[id] = {"camera", {{"CameraComponent", object(component)}, {"RootComponent", object(root)}}};
            }
            return {{"ReturnValue", object(id)}};
        }
        if (fn == "FinishSpawningActor" || fn == "K2_AttachToActor" || fn == "K2_SetRelativeLocationAndRotation") return {{"ReturnValue", true}};
        if (fn == "K2_SetActorLocationAndRotation") { if (target == 1) { pos = cine::Vec3{a["NewLocation"]["X"], a["NewLocation"]["Y"], a["NewLocation"]["Z"]}; ++place_player; } return {{"ReturnValue", true}}; }
        if (fn == "MoveComponentTo") { if (fail_move) throw std::runtime_error("ProcessEvent failed"); ++move_component; return Json::object(); }
        if (fn == "SetViewTargetWithBlend") { view = id_of(a.at("NewViewTarget")); return Json::object(); }
        if (fn == "GetAllWidgetsOfClass") return {{"FoundWidgets", Json::array({object(50)})}};
        if (fn == "GetVisibility") return {{"ReturnValue", hud_visibility}};
        if (fn == "SetVisibility") { hud_visibility = a.at("InVisibility"); return Json::object(); }
        if (fn == "SetForceWalk") { ++force_walk; return Json::object(); }
        if (fn == "RemoveForceWalk") { force_walk = std::max(0, force_walk - 1); return Json::object(); }
        if (fn == "HasForcedWalk") return {{"ReturnValue", force_walk > 0}};
        if (fn == "SetMaxSpeedAdjustment") { speed_adjusted = true; speed_limit = a.at("Speed"); return Json::object(); }
        if (fn == "RemoveMaxSpeedAdjustment") { speed_adjusted = false; ++speed_adjust_removed; return Json::object(); }
        if (fn == "SimpleMoveToLocation") { goal = cine::Vec3{a["Goal"]["X"], a["Goal"]["Y"], a["Goal"]["Z"]}; moving = true; return Json::object(); }
        if (fn == "StopMovement") { moving = false; return Json::object(); }
        if (fn == "SetControlRotation" || fn == "SetFieldOfView") return Json::object();
        if (fn == "K2_DestroyActor") { destroyed.insert(target); cameras.erase(target); return {{"ReturnValue", true}}; }
        if (fn == "LineTraceSingle") return {{"ReturnValue", hit_walls}, {"OutHit", {{"Distance", 150.0}}}};
        if (fn == "GetCameraLocation") return {{"ReturnValue", vec(pos + cine::Vec3{0, 300, 0})}};
        if (fn == "GetCameraRotation") return {{"ReturnValue", {{"Pitch", 10.0}, {"Yaw", -90.0}, {"Roll", 0.0}}}};
        if (fn == "GetFOVAngle") return {{"ReturnValue", 70.0}};
        throw std::runtime_error("unexpected call " + fn);
    }
    // The game's side of one 0.1 s tick: path following moves her toward the goal.
    void advance(double dt) {
        if (!moving) return;
        const double speed = speed_adjusted ? std::min(184.0, speed_limit) : 540.0;
        const double d = cine::flat_distance(pos, goal);
        if (d < 1) return;
        const double step = std::min(d, speed * dt);
        pos.x += (goal.x - pos.x) / d * step; pos.y += (goal.y - pos.y) / d * step;
        yaw = std::atan2(goal.y - pos.y, goal.x - pos.x) * 180 / 3.14159265358979;
    }
};

struct Rig {
    Host host;
    std::unique_ptr<cine::Extension> ext;
    CssxFrame frame{};
    uint32_t world = 1;
    bool menu = false;
    explicit Rig(const fs::path& dir) { host.dir = dir; ext = std::make_unique<cine::Extension>(&host.api); }
    void step(double dt = 0.1) {
        const auto before = host.pos;
        host.advance(dt);
        host.game_time += dt;
        frame.abi = CSSX_ABI; frame.size = sizeof(CssxFrame); frame.seconds = dt; frame.world_generation = world; frame.world_ready = 1;
        frame.in_menu = menu ? 1 : 0; frame.player_x = host.pos.x; frame.player_y = host.pos.y; frame.player_z = host.pos.z; frame.player_yaw = host.yaw;
        frame.velocity_x = (host.pos.x - before.x) / dt; frame.velocity_y = (host.pos.y - before.y) / dt; frame.velocity_z = 0;
        frame.viewport_w = 2560; frame.viewport_h = 1440; frame.pawn = 1; frame.controller = 2;
        ext->render(&frame);
        ext->tick(dt);
    }
    void steps(int n) { for (int i = 0; i < n; ++i) step(); }
    void press(const std::string& key) { host.keys[key] = false; step(); host.keys[key] = true; step(); host.keys[key] = false; step(); }
    std::string status() const { return ext->model()["values"]["status"].get<std::string>(); }
    std::string error() const { return ext->model()["error"].get<std::string>(); }
};

// Every value the world had before is there again; names the first one that is not.
std::string unrestored(const Json& before, const Json& after) {
    for (const auto& [key, props] : before.items()) {
        if (!after.contains(key)) return key + " is gone";
        for (const auto& [name, value] : props.items())
            if (after[key].value(name, Json("<missing>")) != value) return key + "." + name + " was " + value.dump() + ", now " + after[key].value(name, Json("<missing>")).dump();
    }
    return {};
}
bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

fs::path temp_extension(const fs::path& shipped) {
    const auto dir = fs::temp_directory_path() / ("cine-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(dir); fs::create_directories(dir / "presets");
    for (const auto& e : fs::directory_iterator(shipped / "presets")) fs::copy_file(e.path(), dir / "presets" / e.path().filename());
    std::ofstream(dir / "presets" / "broken.cine.json") << "{\"schema\":1,\"id\":\"broken\"";
    return dir;
}
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: cine_director_tests <extension dir>\n"; return 2; }
    const fs::path shipped = argv[1];
    const auto definition = cssx::read_json(cssx::utf8_path((shipped / "menu.json").string()));
    const auto dir = temp_extension(shipped);
    try {
        {   // Idle costs nothing; menu binds; broken preset files are skipped, not fatal.
            Rig r(dir);
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            expect(r.ext->model()["values"]["look"] == "off", "the look track is off until asked for");
            expect(r.ext->model()["values"]["status"].get<std::string>().find("skipped") != std::string::npos, "a broken preset is reported");
            r.step();
            const auto before = r.host.requests;
            r.steps(50);
            expect(r.host.requests == before, "idle CINE must not call the host");
        }
        {   // Enter, glide take, everything restored, still in Cine World; then leave.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-3"}});
            r.ext->event({{"id", "countdown"}, {"value", 1}});
            r.ext->event({{"id", "enter"}});
            expect(r.host.hud_visibility == 2, "entering hides the game HUD");
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            r.press("F8");
            r.steps(10);   // prepare (walls in slices) + countdown
            expect(!r.host.cameras.empty() && r.host.view != 1, "the take views through CINE's camera");
            expect(r.host.force_walk == 0, "a pose take does not force walk");
            r.steps(40);
            expect(r.host.move_component > 10, "the camera glides with engine-interpolated moves");
            expect(r.host.cameras.empty() && r.host.view == 1, "after the take: cameras destroyed, view back to her");
            expect(r.host.hud_visibility == 2, "still in Cine World, HUD still hidden");
            r.press("F9");
            expect(r.host.hud_visibility == 4, "leaving restores the HUD");
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
        }
        {   // A walk take needs a route; the error names the fix.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "fashion-walk-360"}});
            r.ext->event({{"id", "enter"}});
            r.press("F8");
            r.steps(3);
            expect(r.error().find("Record a route") != std::string::npos, "walk without a route explains itself");
            expect(r.host.cameras.empty() && r.host.view == 1, "a refused take leaves nothing behind");
        }
        {   // Record a route, capture a final shot, roll a walk take with the palette look track.
            Rig r(dir);
            r.step();
            r.menu = true; r.ext->event({{"id", "record"}}); r.step();
            r.menu = false; r.step(); r.step();
            r.host.moving = true; r.host.goal = {2000, 0, 100}; r.host.speed_adjusted = true; r.host.speed_limit = 184;
            r.steps(115);                       // ~21 m of walking
            r.host.moving = false; r.steps(35);  // stand still 3.5 s
            expect(r.host.state.contains("route") && r.host.state["route"].size() >= 10, "the route is saved");
            expect(r.host.hud_visibility == 4 && r.ext->model()["enabled"]["enter"].get<bool>(), "recording from outside goes back outside; Enter still works");
            r.ext->event({{"id", "enter"}});
            r.host.pos = {2300, 150, 100}; r.host.yaw = 30; r.step();
            r.press("F7");
            expect(r.host.state.contains("final"), "the final shot is saved: " + r.status() + " / " + r.error() + " / " + r.host.state.dump().substr(0, 300));
            r.ext->event({{"id", "preset"}, {"value", "fashion-walk-360"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.press("F8");
            r.steps(40);
            expect(r.host.place_player == 1, "she is put on the route start");
            expect(r.host.force_walk > 0, "force walk is on for a walk take");
            r.steps(500);   // the whole take (max 49 s)
            expect(r.host.force_walk == 0 && !r.host.speed_adjusted && !r.host.moving, "walk settings are restored after the take: " + r.status() + " / " + r.error());
            expect(r.host.cameras.empty() && r.host.view == 1, "cameras gone, view back to her");
            expect(cine::flat_distance(r.host.pos, {2300, 150, 100}) < 120, "she walks into the final shot's spot");
            std::set<std::string> palettes;
            for (const auto& a : r.host.css_applies) for (const auto& c : a["commands"]) if (c.value("action", std::string()) == "palette") palettes.insert(c["palette"]);
            expect(palettes == std::set<std::string>{"crimson", "midnight"}, "every palette except Original is shown");
            bool transient = false, own = false, saved = false;
            for (const auto& a : r.host.css_applies) {
                if (a.value("persist", true) == false && a["commands"][0]["action"] == "palette") transient = true;
                if (a.value("persist", true) == false && a["commands"][0]["action"] == "restore") own = true;
                if (a.value("persist", false) == true && a["commands"][0]["action"] == "restore") saved = true;
            }
            expect(transient && own && saved, "palettes cycle unsaved, own look returns, the restore is saved");
            for (const auto& a : r.host.css_applies) for (const auto& c : a["commands"]) expect(c.value("palette", std::string()) != "original", "Original is never used mid-shot");
        }
        {   // F9 cancels a recording started in Cine World and stays there; a loading screen while armed drops to idle.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "enter"}});
            r.ext->event({{"id", "record"}});
            r.step(); r.step();
            expect(r.status().find("Recording") != std::string::npos, "recording started");
            r.press("F9");
            expect(r.status().find("cancelled") != std::string::npos && r.host.hud_visibility == 2, "F9 cancels the recording and stays in Cine World");
            expect(!r.ext->model()["enabled"]["enter"].get<bool>() && r.ext->model()["enabled"]["exit"].get<bool>(), "the menu shows Cine World");
            r.world = 2; r.step();
            const auto before = r.host.requests;
            r.steps(10);
            expect(r.host.requests == before && r.ext->model()["enabled"]["enter"].get<bool>(), "a loading screen while armed returns to idle quietly");
        }
        {   // A menu opening mid-take cancels it and restores.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "turntable"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(10);
            expect(!r.host.cameras.empty(), "take running");
            r.menu = true; r.step(); r.menu = false; r.step();
            expect(r.host.cameras.empty() && r.host.view == 1, "a menu cancels the take and restores");
            expect(r.status().find("menu") != std::string::npos || r.error().find("menu") != std::string::npos, "the reason is shown");
        }
        {   // A bridge failure mid-take stops it cleanly; tick never throws.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-5"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            r.host.fail_move = true; r.steps(3);
            expect(r.host.cameras.empty() && r.host.view == 1, "a failed call ends the take and restores");
            expect(r.error().find("ProcessEvent failed") != std::string::npos, "the failure is shown");
        }
        {   // The world changing mid-take (loading screen) drops everything without calling dead handles.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-5"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            r.world = 2; r.step();
            const auto before = r.host.requests;
            r.steps(20);
            expect(r.host.requests == before, "after a world change CINE is idle");
            expect(r.status().find("world changed") != std::string::npos, "the user is told why");
        }
        {   // World change mid-take with the palette track: the look still goes back, retried until CSS takes it.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-10"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(40);
            bool changed = false;
            for (const auto& a : r.host.css_applies) if (a["commands"][0]["action"] == "palette") changed = true;
            expect(changed, "a palette was shown before the world changed");
            r.host.css_refuse_restores = 3;
            r.world = 2; r.steps(60);
            bool restored = false;
            for (const auto& a : r.host.css_applies) if (a.value("persist", false) && a["commands"][0]["action"] == "restore") restored = true;
            expect(restored && r.host.css_refuse_restores == 0, "the own look is put back once CSS accepts it");
            expect(r.status().find("look is back") != std::string::npos, "the user is told: " + r.status());
            const auto before = r.host.requests;
            r.steps(20);
            expect(r.host.requests == before, "idle again after the restore");
        }
        {   // "From the preset" on a preset without look steps says so instead of doing nothing.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-3"}});
            r.ext->event({{"id", "look"}, {"value", "preset"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            expect(r.status().find("no look steps") != std::string::npos, "no-steps preset is explained: " + r.status());
            expect(r.host.css_applies.empty(), "and the look is not touched");
        }
        {   // A restore that fails at the end of a take is retried, and blocks a new look snapshot until done.
            Rig r(dir);
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-3"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.host.css_refuse_restores = 2;
            r.press("F8"); r.steps(45);
            bool restored = false;
            for (const auto& a : r.host.css_applies) if (a.value("persist", false) && a["commands"][0]["action"] == "restore") restored = true;
            expect(restored, "the end-of-take restore went through on a retry");
        }
        {   // stop() mid-take restores everything including the HUD; CSS missing keeps the camera working.
            Rig r(dir);
            r.host.css_present = false;
            r.step();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-3"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8"); r.steps(6);
            expect(!r.host.cameras.empty(), "the camera works without CSS");
            expect(r.host.css_applies.empty(), "no look changes without CSS");
            expect(r.status().find("CSS 1.0.0-beta.9") != std::string::npos || r.error().find("CSS 1.0.0-beta.9") != std::string::npos, "the missing look service is explained: " + r.status());
            expect(r.ext->stop(), "stop succeeds");
            expect(r.host.cameras.empty() && r.host.view == 1 && r.host.hud_visibility == 4, "stop restores everything");
        }
        {   // Clean scene: enemies out without stopping the world, and every value back on leaving.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            const auto a = h.add_character("Team.Hostile.Undead"), b = h.add_character("Team.Hostile.Undead"), c = h.add_character("Team.Hostile.Beast");
            const auto npc = h.add_character("Team.Neutral");
            const auto boss = h.add_character("Team.Hostile.Boss");
            h.prop(boss, "_boss") = object(h.make("boss", {{"bIsAggroed", false}}));
            const auto corpse = h.add_character("Team.Hostile.Undead");
            h.prop(h.part(corpse, "HealthComponent"), "_dead") = true;
            // One the game itself had already stopped and hidden: it must come back exactly so.
            h.prop(b, "bHidden") = true;
            h.prop(h.part(h.part(b, "Controller"), "BrainComponent"), "_running") = false;
            h.prop(h.part(b, "VoxComponent"), "m_bVoxEnabled") = false;
            const auto trigger = h.make("trigger", {{"_collision", true}});
            h.make("trigger", {{"_collision", false}});
            h.make("proximity", {{"_collision", true}});
            const auto addon = h.make("addon", {{"bHidden", false}});
            const auto spawner = h.make("spawner", {{"bHidden", false}, {"UnitProxyAddons", Json::array({object(addon)})}});
            const auto arrow = h.make("projectile", {{"bHidden", false}, {"_collision", true}});
            h.prop(Host::Lock, "LockedTarget") = object(a);
            r.step();
            const auto before = h.snapshot({Host::Lock});
            r.ext->event({{"id", "scene"}, {"value", "enemies"}});
            r.steps(5);
            expect(unrestored(before, h.snapshot()).empty(), "choosing clean scene outside Cine World changes nothing");
            r.ext->event({{"id", "enter"}});
            r.steps(12);
            auto hidden = [&](uint64_t id) { return h.prop(id, "bHidden") == true; };
            auto out = [&](uint64_t id) {
                return hidden(id) && h.prop(id, "_collision") == false && h.prop(h.part(id, "HitTraceComponent"), "_on") == false
                    && h.prop(h.part(id, "AbilitySystemComponent"), "_effects").size() == 2 && h.prop(h.part(id, "Mesh"), "bPauseAnims") == true
                    && h.prop(h.part(id, "Mesh"), "_cloth") == true && h.prop(h.part(id, "VoxComponent"), "m_bVoxEnabled") == false
                    && h.prop(id_of(h.prop(id, "_weapon")), "bHidden") == true;
            };
            expect(out(a) && out(b) && out(c), "enemies are frozen, hidden, silent, untouchable and unkillable");
            expect(h.pauses == 3, "each enemy is paused once");
            expect(hidden(corpse) && h.prop(corpse, "_collision") == true, "a corpse is only hidden");
            expect(!hidden(npc) && !hidden(boss) && h.prop(h.part(boss, "Mesh"), "bPauseAnims") == false, "NPCs and bosses are left alone");
            expect(h.prop(trigger, "_collision") == false && hidden(spawner) && hidden(addon), "spawn triggers are closed and spawner stand-ins hidden");
            expect(hidden(arrow) && h.prop(arrow, "_collision") == false, "a projectile in the air is taken out");
            expect(h.prop(Host::Lock, "LockedTarget").is_null(), "the lock-on is dropped before its target is hidden");
            expect(h.prop(Host::PlayerAsc, "_effects").size() == 3, "with nobody kept she is protected");
            const auto info = r.ext->model()["values"]["scene_info"].get<std::string>();
            expect(has(info, "4 enemies hidden"), "the menu says what is hidden: " + info);
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            // A newcomer (streamed in, spawned by a script) goes the same way.
            const auto late = h.add_character("Team.Hostile.Undead");
            r.steps(8);
            expect(out(late), "an enemy that appears later is taken out too");
            // A spawner un-hides its enemy when the spawn ends: it is hidden again.
            h.prop(c, "bHidden") = false;
            r.steps(8);
            expect(hidden(c), "an enemy the game un-hides is hidden again");
            h.prop(c, "bHidden") = true;   // (the test's own write; the game had it visible before)
            r.ext->event({{"id", "scene"}, {"value", "everyone"}});
            r.steps(8);
            expect(out(npc) && !hidden(boss), "Enemies and NPCs takes the NPC out as well");
            r.ext->event({{"id", "scene"}, {"value", "enemies"}});
            r.steps(8);
            expect(!hidden(npc) && h.prop(npc, "_collision") == true && h.prop(h.part(npc, "AbilitySystemComponent"), "_effects").empty(), "back to Enemies returns the NPC");
            const auto calls = h.requests;
            r.steps(50);
            expect(h.requests - calls < 450, "five seconds of clean scene stay cheap: " + std::to_string(h.requests - calls) + " calls");
            r.press("F9");
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty(), "leaving puts every value back: " + left);
            expect(h.prop(late, "bHidden") == false && h.prop(late, "_collision") == true, "the newcomer is back too");
            const auto idle = h.requests;
            r.steps(30);
            expect(h.requests == idle, "and CINE is idle again");
        }
        {   // Keep my lock-on target: that one stays in, and the fight is real.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            const auto a = h.add_character("Team.Hostile.Undead"), b = h.add_character("Team.Hostile.Undead");
            h.prop(Host::Lock, "LockedTarget") = object(a);
            r.step();
            const auto before = h.snapshot({Host::Lock});
            r.ext->event({{"id", "scene"}, {"value", "enemies"}});
            r.ext->event({{"id", "keep_target"}, {"value", true}});
            r.ext->event({{"id", "enter"}});
            r.steps(12);
            expect(h.prop(a, "bHidden") == false && h.prop(a, "_collision") == true && h.prop(b, "bHidden") == true, "the locked enemy stays, the rest go");
            expect(h.prop(Host::Lock, "LockedTarget") == object(a) && h.lock_releases == 0, "the lock is kept");
            expect(h.prop(Host::PlayerAsc, "_effects").empty(), "with a kept target she can be hurt: the fight is real");
            expect(has(r.ext->model()["values"]["scene_info"].get<std::string>(), "target kept"), "the menu says the target is kept");
            r.ext->event({{"id", "keep_target"}, {"value", false}});
            r.steps(12);
            expect(h.prop(a, "bHidden") == true && h.prop(Host::PlayerAsc, "_effects").size() == 3, "turning it off takes the target out and protects her");
            expect(r.ext->stop(), "stop succeeds");
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty(), "stop puts the scene back: " + left);
        }
        {   // Clean scene stays off in a boss fight, waits out a cutscene, and survives a loading screen.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            const auto a = h.add_character("Team.Hostile.Undead");
            const auto boss = h.add_character("Team.Hostile.Boss");
            const auto brain = h.make("boss", {{"bIsAggroed", true}});
            h.prop(boss, "_boss") = object(brain);
            r.step();
            const auto before = h.snapshot();
            r.ext->event({{"id", "scene"}, {"value", "everyone"}});
            r.ext->event({{"id", "enter"}});
            r.steps(25);
            const auto touched = unrestored(before, h.snapshot());
            expect(touched.empty(), "nothing is touched during a boss fight: " + touched);
            expect(has(r.ext->model()["values"]["scene_info"].get<std::string>(), "boss fight"), "and the menu says why");
            h.prop(brain, "bIsAggroed") = false;
            h.prop(Host::PlayerAsc, "_tags")["State.InCutscene"] = 1;
            r.steps(15);
            expect(h.prop(a, "bHidden") == false && has(r.ext->model()["values"]["scene_info"].get<std::string>(), "cutscene"), "it waits for a cutscene to end");
            h.prop(Host::PlayerAsc, "_tags")["State.InCutscene"] = 0;
            r.steps(15);
            expect(h.prop(a, "bHidden") == true && h.prop(Host::PlayerAsc, "_effects").size() == 3, "then it applies by itself");
            ++r.world;   // loading screen: every actor handle is dead, the pawn travelled
            r.steps(5);
            expect(h.prop(Host::PlayerAsc, "_effects").empty(), "after a world change her protection is removed by effect class");
            const auto idle = h.requests;
            r.steps(30);
            expect(h.requests == idle, "and CINE is idle in the new world");
        }
        {   // Breakdown: layers build over ticks, wipe in on the preset's track, and it is all put back.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            r.step();
            const auto before = h.snapshot();
            const auto originals = h.prop(Host::PlayerMesh, "_materials");
            r.ext->event({{"id", "preset"}, {"value", "breakdown-turntable"}});
            r.ext->event({{"id", "breakdown"}, {"value", "preset"}});
            r.ext->event({{"id", "look"}, {"value", "palettes"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            r.press("F8");
            size_t worst = 0;
            int waited = 0;
            for (; waited < 60 && !has(r.status(), "Rolling"); ++waited) { const auto c = h.requests; r.step(); worst = std::max(worst, h.requests - c); }
            expect(has(r.status(), "Rolling"), "the take rolls once the layers are built: " + r.status());
            expect(h.components_made == 19, "9 bone sticks, 5 markers, 2 sockets and 3 physics shapes are built, got " + std::to_string(h.components_made));
            expect(worst <= 200, "building never spends more than a bounded slice per tick: " + std::to_string(worst));
            expect(has(r.status(), "Look track off"), "the look track steps aside for a breakdown");
            expect(h.css_applies.empty(), "and no look is applied");
            auto camera_component = [&]() -> Host::Thing& { for (auto& [id, t] : h.things) if (t.kind == "camera_component") return t; throw std::runtime_error("no camera component"); };
            auto parent_of = [&](const Json& mid) { const auto it = h.things.find(id_of(mid)); return it == h.things.end() || it->second.kind != "mid" ? std::string() : h.things.at(id_of(it->second.props.at("_parent"))).kind; };
            auto until = [&](double seconds) { while (h.game_time < seconds) r.step(); };
            const double start = h.game_time;
            expect(camera_component().props["_blendables"].size() == 2, "the two frame passes sit on CINE's camera");
            until(start + 5);     // clay came in at 2.4 s with a 1.4 s wipe
            expect(has(parent_of(h.prop(Host::PlayerMesh, "_materials")[0]), "M_Cine_Clay"), "clay is on her mesh");
            expect(has(parent_of(h.prop(h.hair, "_materials")[1]), "M_Cine_Clay") && has(parent_of(h.prop(h.crown, "_materials")[0]), "M_Cine_Clay"),
                   "and on her other parts (hair, garments, static accessories)");
            expect(h.mpc.value("WipeTo", 0.0) == 1.0 && h.mpc.contains("WipeT0"), "the wipe is driven by numbers written once");
            until(start + 9);     // clay + wireframe at 6.4 s
            expect(has(parent_of(h.prop(Host::PlayerMesh, "OverlayMaterial")), "M_Cine_Wire") && has(parent_of(h.prop(h.hair, "OverlayMaterial")), "M_Cine_Wire"), "wireframe is the overlay on every part");
            expect(h.projections > 0 && h.view_points > 0, "labels follow their components");
            {   // The labels cost one host call a frame, whatever their number.
                const auto calls = h.requests;
                for (int i = 0; i < 20; ++i) r.ext->render(&r.frame);
                expect(h.requests - calls == 20, "a frame of labels is one host call: " + std::to_string(h.requests - calls) + " for 20 frames");
            }
            until(start + 13.5);  // clay + skeleton at 10.8 s
            expect(has(parent_of(h.prop(Host::PlayerMesh, "_materials")[0]), "M_Cine_Clay") && h.prop(Host::PlayerMesh, "OverlayMaterial").is_null(),
                   "the skeleton shows over clay, wireframe gone");
            expect(h.param_count("M_Cine_XRay", "Show", 1.0) == 1, "the skeleton layer is the one bone layer showing");
            until(start + 25);    // collision + capsule at 22.8 s
            size_t shapes = 0;
            for (const auto id : h.components) if (h.prop(id, "_hidden") == false) ++shapes;
            expect(shapes == 3 && h.prop(Host::PlayerCapsule, "bHiddenInGame") == false, "physics bodies and the capsule show");
            expect(h.prop(Host::PlayerMesh, "_materials") == originals, "clay is off: her own materials are back");
            expect(h.prop(Host::PlayerMesh, "OverlayMaterial").is_null() && parent_of(h.prop(h.hair, "OverlayMaterial")).empty() && !h.prop(h.hair, "OverlayMaterial").is_null(),
                   "wireframe is off again, and a part that had its own overlay has it back");
            until(start + 29.5);  // base colour at 26.8 s
            expect(h.param_count("PP_Cine_Buffer_Post", "W_BaseColor", 1.0) == 1 && h.param_count("PP_Cine_Buffer_Post", "Show", 1.0) == 1, "one frame pass shows base colour");
            expect(h.prop(Host::PlayerCapsule, "bHiddenInGame") == true, "the capsule is hidden again");
            const auto steady = h.requests;
            r.steps(10);
            expect(h.requests - steady <= 10 * 10, "between steps a breakdown costs only its labels: " + std::to_string(h.requests - steady));
            until(start + 41);
            r.steps(5);
            expect(h.cameras.empty() && h.view == 1 && h.components.empty(), "after the take: camera gone, every piece destroyed");
            expect(camera_component().props["_blendables"].empty(), "the frame passes were taken off the camera first");
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty(), "her mesh, overlay and capsule are as they were: " + left);
        }
        {   // Cancel in the middle of a breakdown (clay on, wireframe wiping in): all back at once.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            r.step();
            const auto before = h.snapshot();
            r.ext->event({{"id", "preset"}, {"value", "breakdown-turntable"}});
            r.ext->event({{"id", "breakdown"}, {"value", "preset"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8");
            for (int i = 0; i < 60 && !has(r.status(), "Rolling"); ++i) r.step();
            r.steps(70);
            expect(h.prop(Host::PlayerMesh, "_materials") != before["7 mesh"]["_materials"], "clay is on when the take is cancelled");
            r.press("F9");
            r.steps(5);
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty() && h.components.empty() && h.cameras.empty(), "cancel restores everything: " + left);
            // And a loading screen in the middle of one never throws.
            r.press("F8");
            for (int i = 0; i < 60 && !has(r.status(), "Rolling"); ++i) r.step();
            r.steps(40);
            ++r.world;
            r.steps(10);
            expect(has(r.status(), "world changed"), "a world change during a breakdown stops CINE cleanly");
        }
        {   // Without CINE's asset pak only the engine-drawn layers show; with an older CSSX there are no frame passes.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            h.pak = false;
            r.step();
            const auto before = h.snapshot();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-10"}});
            r.ext->event({{"id", "breakdown"}, {"value", "reel"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8");
            for (int i = 0; i < 60 && !has(r.status(), "Rolling"); ++i) r.step();
            expect(has(r.status(), "asset pak is not installed"), "the missing pak is named: " + r.status());
            expect(h.components_made == 3, "only the physics shapes are built");
            const double start = h.game_time;
            while (h.game_time < start + 6.6) r.step();   // collision + capsule at 60% of 10 s
            expect(h.prop(Host::PlayerCapsule, "bHiddenInGame") == false, "the capsule still shows without the pak");
            r.steps(50);
            auto left = unrestored(before, h.snapshot());
            expect(left.empty() && h.components.empty(), "and it is all put back: " + left);
            h.pak = true; h.old_bridge = true;
            r.press("F8");
            for (int i = 0; i < 60 && !has(r.status(), "Rolling"); ++i) r.step();
            expect(has(r.status(), "CSSX 1.3.1"), "an older CSSX is named for the frame passes: " + r.status());
            r.steps(110);
            left = unrestored(before, h.snapshot());
            expect(left.empty() && h.components.empty(), "the rest of the reel ran and was put back: " + left);
        }
        {   // The lock-on target can be broken down with her.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            const auto foe = h.add_character("Team.Hostile.Undead");
            h.prop(Host::Lock, "LockedTarget") = object(foe);
            r.step();
            const auto before = h.snapshot();
            r.ext->event({{"id", "preset"}, {"value", "pose-glide-10"}});
            r.ext->event({{"id", "breakdown"}, {"value", "reel"}});
            r.ext->event({{"id", "breakdown_target"}, {"value", true}});
            r.ext->event({{"id", "labels"}, {"value", false}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8");
            for (int i = 0; i < 80 && !has(r.status(), "Rolling"); ++i) r.step();
            expect(h.components_made == 2 * 17, "both characters get their pieces: " + std::to_string(h.components_made));
            r.steps(25);
            expect(h.things.at(id_of(h.prop(h.part(foe, "Mesh"), "_materials")[0])).kind == "mid", "the target is in clay too");
            expect(h.projections == 0, "no labels when they are off");
            r.steps(100);
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty() && h.components.empty(), "both are put back: " + left);
        }
        {   // A fight take: the camera frames both, reacts to the fight, and ends when the target is down.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            const auto foe = h.add_character("Team.Hostile.Undead", {400, 0, 100});
            const auto health = h.part(foe, "HealthComponent");
            h.prop(Host::Lock, "LockedTarget") = object(foe);
            r.step();
            const auto before = h.snapshot();
            r.ext->event({{"id", "preset"}, {"value", "fight-duel"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            cssx::validate_model(cssx::bind_menu(definition, r.ext->model()));
            r.press("F8");
            r.steps(8);
            expect(!h.cameras.empty() && h.view != 1, "the fight is seen through CINE's camera");
            expect(h.prop(h.part(foe, "Enemy Health Widget"), "_visible") == false, "the target's health bar is out of the picture");
            expect(h.prop(Host::CameraManager, "_shake") == 0.5, "camera shake is scaled for the take");
            auto fov = [&] { for (auto& [id, t] : h.things) if (t.kind == "camera_component") return t.props["FieldOfView"].get<double>(); return 0.0; };
            expect(std::abs(fov() - 38) < 0.01, "the preset's lens");
            const auto moves = h.move_component;
            const auto calls = h.requests;
            r.steps(20);
            expect(h.move_component - moves >= 19, "the camera follows on every tick");
            expect(h.requests - calls <= 20 * 7, "a fight take is a handful of calls per tick: " + std::to_string(h.requests - calls));
            h.prop(health, "_health") = 0.6;
            r.steps(6);
            expect(fov() < 37, "a landed hit pushes the camera in");
            h.prop(Host::PlayerAsc, "_tags")["State.CustomCamera"] = 1;   // the game's own finisher camera
            h.view = 77;
            const auto held = h.move_component;
            r.steps(10);
            expect(h.view == 77 && h.move_component == held, "the game's own camera is left to play");
            h.prop(Host::PlayerAsc, "_tags").erase("State.CustomCamera");
            r.steps(3);
            expect(h.cameras.contains(h.view), "then CINE takes the view back");
            h.prop(health, "_dead") = true;
            r.steps(6);
            expect(h.slowest < 0.6, "the kill plays in slow motion");
            r.steps(50);
            expect(h.cameras.empty() && h.view == 1 && has(r.status(), "target is down"), "the take ends by itself after the kill: " + r.status());
            h.prop(health, "_health") = 1.0; h.prop(health, "_dead") = false;
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty(), "time, shake and the health bar are as they were: " + left);
        }
        {   // A fight take without a target follows her; it ends when she falls.
            Rig r(dir); r.host.populate(); auto& h = r.host;
            r.step();
            const auto before = h.snapshot();
            r.ext->event({{"id", "preset"}, {"value", "fight-hero"}});
            r.ext->event({{"id", "countdown"}, {"value", 0}});
            r.ext->event({{"id", "enter"}});
            r.press("F8");
            r.steps(30);
            expect(!h.cameras.empty() && h.move_component > 20, "it runs with nobody locked");
            h.prop(Host::PlayerHealth, "_health") = 0.0;
            r.steps(4);
            expect(h.cameras.empty() && has(r.status(), "she fell"), "her death ends the take: " + r.status());
            h.prop(Host::PlayerHealth, "_health") = 1.0;
            const auto left = unrestored(before, h.snapshot());
            expect(left.empty(), "and nothing is left behind: " + left);
        }
        {   // Bad menu input is refused with the error kept for the menu.
            Rig r(dir);
            r.step();
            bool threw = false;
            try { r.ext->event({{"id", "preset"}, {"value", "nope"}}); } catch (...) { threw = true; }
            expect(threw && !r.error().empty(), "an unknown preset is refused");
            r.ext->event({{"id", "zoom"}, {"value", 99}});
            expect(r.ext->model()["values"]["zoom"].get<double>() <= 2.0, "values are clamped");
        }
        fs::remove_all(dir);
        std::cout << checks << " CINE director checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        fs::remove_all(dir);
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}

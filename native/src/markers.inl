// Native world-marker provider for the compass & minimap (ported from the compass
// main.lua target resolution). The extension can't FindAllOf/FindFirstOf by class,
// so css_core collects the marker world positions and hands them over via the
// hud.markers request op. Included in engine.cpp after minimap_widget.inl (uses
// find/Call/read/narrow/WeakObject/FVectorD).
namespace css {
namespace {
using namespace RC::Unreal;

namespace mk {
constexpr const wchar_t* SPOILS = L"BP_DeathSpoils_C";
// The player's map ping. The persistent tracked marker (the blue world beam) is a
// BP_MapTracker (the compass main.lua's primary source, rank 40) / WBP_WMI_Tracker; its
// world position is a property, not its actor transform. BP_MapPingData / BP_PP_MapPing
// are fallbacks. WBP_MapPingActor is only the screen widget (no world location, and
// calling actor functions on it crashes), so it is never used.
const wchar_t* PING_CLASSES[] = { L"BP_MapTracker_C", L"WBP_WMI_Tracker_C", L"BP_MapPingData_C", L"BP_PP_MapPing_C" };
// World-position property names and nested-object names the compass main.lua probes.
const wchar_t* PING_VEC_PROPS[] = { L"WorldLocation", L"TrackerLocation", L"TargetLocation",
    L"Location", L"WorldPosition", L"Position", L"MapLocation", L"MarkerLocation",
    L"TrackedLocation", L"Destination", L"PingLocation" };
const wchar_t* PING_OBJ_PROPS[] = { L"MapTracker", L"Tracker", L"TrackedActor", L"TargetActor",
    L"Target", L"MapActor", L"OwnerActor", L"Actor", L"Data", L"TrackerData" };
const wchar_t* DUNGEON_CLASSES[] = { L"BP_STH_Entrance_Dungeon_C", L"BP_STH_Entrance_Gate_C", L"BPO_STH_Beacon_C" };

bool is_real(UObject* o) {
    if(!o || !WeakObject(o).Get()) return false;
    return narrow(o->GetFullName()).find("Default__") == std::string::npos;
}
bool actor_xyz(UObject* actor, double& x, double& y, double& z) {
    if(!is_real(actor)) return false;
    // Only real Actors have K2_GetActorLocation. Calling it on something that isn't an
    // Actor (e.g. WBP_MapPingActor_C, which is a widget) is a hard access violation that
    // C++ try/catch cannot trap, so gate on the function existing first.
    if(!actor->GetFunctionByNameInChain(L"K2_GetActorLocation")) return false;
    try { Call c(actor, L"K2_GetActorLocation", 1); c.run(); auto v = c.get<FVectorD>(); x = v.x; y = v.y; z = v.z; return true; }
    catch(...) { return false; }
}
// A world position is sane when it is finite and inside the map's coordinate range;
// reading a non-vector struct by name would otherwise yield garbage that projects the
// marker to a nonsense bearing.
bool sane_xyz(double x, double y, double z) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
        && std::abs(x) < 1.0e7 && std::abs(y) < 1.0e7 && std::abs(z) < 1.0e7
        && (x != 0.0 || y != 0.0 || z != 0.0);
}
// Try the named FVector properties (a Vector struct is 24 bytes: three doubles).
bool named_vector(UObject* o, double& x, double& y, double& z) {
    for(auto* name : PING_VEC_PROPS) {
        try {
            auto* prop = o->GetPropertyByNameInChain(name);
            if(prop && prop->IsA<FStructProperty>() && prop->GetElementSize() >= 24) {
                auto v = read<FVectorD>(o, name);
                if(sane_xyz(v.x, v.y, v.z)) { x = v.x; y = v.y; z = v.z; return true; }
            }
        } catch(...) {}
    }
    return false;
}
// Resolve a map-ping/tracker world position. Faithful to main.lua
// player_marker_location_from_object: prefer explicit world/target properties (map/UI
// actors can carry presentation-space transforms), then the actor location, then a
// nested tracker/target object.
bool ping_xyz(UObject* o, double& x, double& y, double& z) {
    if(!is_real(o)) return false;
    if(named_vector(o, x, y, z)) return true;
    if(actor_xyz(o, x, y, z) && sane_xyz(x, y, z)) return true;
    for(auto* name : PING_OBJ_PROPS) {
        try {
            auto* prop = o->GetPropertyByNameInChain(name);
            if(!prop || !prop->IsA<FObjectProperty>()) continue;
            auto* nested = read<UObject*>(o, name);
            if(!is_real(nested) || nested == o) continue;
            if(named_vector(nested, x, y, z)) return true;
            if(actor_xyz(nested, x, y, z) && sane_xyz(x, y, z)) return true;
        } catch(...) {}
    }
    return false;
}
} // namespace mk

Json collect_markers(void* engine, double radius_m) {
    using namespace mk;
    Json out = {{"gloom", nullptr}, {"dungeons", Json::array()}, {"pings", Json::array()}};
    double px = 0, py = 0, pz = 0;
    UObject* pawn = nullptr;
    if(auto* vp = read<UObject*>(static_cast<UObject*>(engine), L"GameViewport"))
        if(auto* world = read<UObject*>(vp, L"World")) {
            Call gp(find(L"/Script/Engine.Default__GameplayStatics"), L"GetPlayerCharacter", 3);
            gp.set_arg(0, world); gp.set_arg(1, int32_t{0}); gp.run(); pawn = gp.get<UObject*>();
        }
    actor_xyz(pawn, px, py, pz);
    out["player"] = {{"x", px}, {"y", py}, {"z", pz}};
    const double radius_cm = radius_m * 100.0;

    // Static world dungeons cache: dungeon entrance locations are immutable in world space.
    // Scanning 100k+ UObjects for them every 1.5s is completely redundant.
    static void* s_dungeon_world = nullptr;
    static std::vector<std::array<double, 3>> s_cached_dungeons;
    static WeakObject s_cached_gloom;

    UObject* world_ptr = nullptr;
    if(auto* vp = read<UObject*>(static_cast<UObject*>(engine), L"GameViewport"))
        world_ptr = read<UObject*>(vp, L"World");

    if(world_ptr != s_dungeon_world) {
        s_dungeon_world = world_ptr;
        s_cached_dungeons.clear();
        s_cached_gloom.Reset();
    }

    const bool need_dungeon_scan = s_cached_dungeons.empty();
    UObject* live_gloom = s_cached_gloom.Get();
    bool gloom_recovered = true;
    if(live_gloom && is_real(live_gloom)) {
        try { gloom_recovered = read<bool>(live_gloom, L"Recovered"); } catch(...) {}
        if(!gloom_recovered) {
            double gx, gy, gz;
            if(actor_xyz(live_gloom, gx, gy, gz)) {
                out["gloom"] = {{"x", gx}, {"y", gy}, {"z", gz}};
            } else {
                live_gloom = nullptr;
            }
        } else {
            live_gloom = nullptr;
        }
    }
    const bool need_gloom_scan = (live_gloom == nullptr);

    static const FName spoils_fn(SPOILS, FNAME_Add);
    static const FName dungeon_fn[3] = { FName(DUNGEON_CLASSES[0], FNAME_Add), FName(DUNGEON_CLASSES[1], FNAME_Add), FName(DUNGEON_CLASSES[2], FNAME_Add) };
    static const FName ping_fn[4] = { FName(PING_CLASSES[0], FNAME_Add), FName(PING_CLASSES[1], FNAME_Add), FName(PING_CLASSES[2], FNAME_Add), FName(PING_CLASSES[3], FNAME_Add) };

    std::vector<UObject*> spoils_objs, dungeon_objs, ping_objs;
    spoils_objs.reserve(4);
    if(need_dungeon_scan) dungeon_objs.reserve(16);
    ping_objs.reserve(8);

    try {
        UObjectGlobals::ForEachUObject([&](UObject* o, int32_t, int32_t) -> RC::LoopAction {
            if(o) if(auto* cls = o->GetClassPrivate()) {
                const FName cn = cls->GetNamePrivate();
                if(need_gloom_scan && cn == spoils_fn) { spoils_objs.push_back(o); return RC::LoopAction::Continue; }
                if(need_dungeon_scan) {
                    for(auto& f : dungeon_fn) if(cn == f) { dungeon_objs.push_back(o); return RC::LoopAction::Continue; }
                }
                for(auto& f : ping_fn) if(cn == f) { ping_objs.push_back(o); return RC::LoopAction::Continue; }
            }
            return RC::LoopAction::Continue;
        });
    } catch(...) {}

    // Lost Gloom: your dropped currency, unless already recovered (first live one).
    if(need_gloom_scan) {
        for(auto* spoils : spoils_objs) {
            if(!is_real(spoils)) continue;
            bool recovered = false;
            try { recovered = read<bool>(spoils, L"Recovered"); } catch(...) {}
            double x, y, z;
            if(!recovered && actor_xyz(spoils, x, y, z)) {
                out["gloom"] = {{"x", x}, {"y", y}, {"z", z}};
                s_cached_gloom = spoils;
                break;
            }
        }
    }

    // Populate static dungeon cache on world change
    if(need_dungeon_scan && !dungeon_objs.empty()) {
        std::vector<std::pair<int64_t, int64_t>> seen_cache;
        seen_cache.reserve(16);
        for(auto* o : dungeon_objs) {
            double x, y, z;
            if(!actor_xyz(o, x, y, z)) continue;
            auto key = std::make_pair(static_cast<int64_t>(x / 100.0), static_cast<int64_t>(y / 100.0));
            if(std::find(seen_cache.begin(), seen_cache.end(), key) != seen_cache.end()) continue;
            seen_cache.push_back(key);
            s_cached_dungeons.push_back({x, y, z});
        }
    }

    // Nearby dungeon entrances (closest 4 within radius, from cached world locations)
    std::vector<std::array<double, 4>> dungeons;
    dungeons.reserve(s_cached_dungeons.size());
    for(const auto& d : s_cached_dungeons) {
        const double x = d[0], y = d[1], z = d[2];
        const double d2 = (x - px) * (x - px) + (y - py) * (y - py);
        if(radius_cm > 0 && d2 > radius_cm * radius_cm) continue;
        dungeons.push_back({x, y, z, d2});
    }
    std::sort(dungeons.begin(), dungeons.end(), [](const auto& a, const auto& b) { return a[3] < b[3]; });
    for(size_t i = 0; i < dungeons.size() && i < 4; ++i)
        out["dungeons"].push_back({{"x", dungeons[i][0]}, {"y", dungeons[i][1]}, {"z", dungeons[i][2]}});

    // Map pings the player placed (up to 5), deduped by a 1 m grid using flat vector
    std::vector<std::pair<int64_t, int64_t>> ping_seen;
    ping_seen.reserve(8);
    int ping_count = 0;
    for(auto* o : ping_objs) {
        if(ping_count >= 5) break;
        double x, y, z;
        if(!ping_xyz(o, x, y, z)) continue;
        auto key = std::make_pair(static_cast<int64_t>(x / 100.0), static_cast<int64_t>(y / 100.0));
        if(std::find(ping_seen.begin(), ping_seen.end(), key) != ping_seen.end()) continue;
        ping_seen.push_back(key);
        out["pings"].push_back({{"x", x}, {"y", y}, {"z", z}});
        ++ping_count;
    }

    return out;
}
} // namespace
Json markers_collect(void* engine, double radius_m) { try { return collect_markers(engine, radius_m); } catch(...) { return Json::object(); } }
} // namespace css

#pragma once
// Everything CINE does to the game goes through Rig, and Rig records what it changed so every
// exit path can put it back. Call shapes are the ones proven live on 2026-10-02/03.
// Game thread only (CSSX calls tick/render/event there).
#include "shot.hpp"
#include <cssx/client.hpp>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

class Rig {
public:
    explicit Rig(const cssx::Client& host) : host_(host) {}

    // Player and library handles; resolved once per world, dropped by forget().
    void resolve();
    bool resolved() const { return !pawn_.is_null(); }
    void forget();                        // the world changed: handles are dead, owned list too
    const Json& pawn() const { return pawn_; }
    const Json& controller() const { return controller_; }

    // HUD: hides the game's player HUD (WBP_Player_HUD) and remembers each widget's visibility.
    void hide_hud();
    void restore_hud();

    // Cameras CINE owns. Free cameras move in world space; attached ones ride on her.
    Json spawn_camera(Vec3 position, Rotator rotation, double fov);
    Json spawn_attached(Vec3 relative, Rotator relative_rotation, double fov);
    void place_camera(const Json& camera, Vec3 position, Rotator rotation);
    // Engine-interpolated move, retargeted in place (fixed latent id), so it never stops.
    void move_camera(const Json& camera, Vec3 position, Rotator rotation, double over_time);
    void set_fov(const Json& camera, double fov);
    void view(const Json& target, double blend);
    void view_player();

    // Walking: the game's force-walk gait, the speed limiter, path following, steering.
    void force_walk();
    void speed_limit(double cm_s);
    void move_to(Vec3 goal);
    void stop_moving();
    void control_yaw(double yaw);
    void place_player(Vec3 position, double yaw);

    // Queries.
    bool trace(Vec3 from, Vec3 to, double* distance);   // camera channel, ignores her
    Vec3 head_position();
    // The game camera relative to her, for "use current view as final shot".
    FinalShot capture_final(Vec3 her_position, double her_yaw);
    Json css(const Json& request);                       // service css.customize v1 (throws why not)
    bool key_down(const std::vector<std::string>& keys, std::vector<bool>& out);

    // Restore: take-level changes (cameras, view, walk) or everything including the HUD.
    // Every step runs even when an earlier one fails; returns the failures.
    std::vector<std::string> restore_take();
    std::vector<std::string> restore_all();
    bool owns_take_changes() const;
    bool hud_hidden() const { return hud_hidden_; }

private:
    Json call(const Json& target, const std::string& function, const Json& args) const;
    Json call_raw(const Json& target, const std::string& function, const Json& args) const;
    Json cdo(const std::string& path) const;

    const cssx::Client& host_;
    Json pawn_, controller_, movement_, mesh_, camera_manager_;
    Json statics_, kismet_, ai_, widgets_, player_lib_, camera_class_;
    std::vector<Json> cameras_;
    std::vector<std::pair<Json, int>> hud_;
    bool hud_hidden_ = false, view_changed_ = false, speed_set_ = false, moving_ = false;
    int force_walk_sets_ = 0;
};

} // namespace cine

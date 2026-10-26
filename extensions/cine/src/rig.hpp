#pragma once
// Everything CINE does to the game goes through Rig, and Rig records what it changed so every
// exit path can put it back. Call shapes are the ones proven live on 2026-10-02/03.
// Game thread only (CSSX calls tick/render/event there).
#include "shot.hpp"
#include <cssx/client.hpp>
#include <map>
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
    // One of the game's own animation montages on her (an asset object path), through
    // ACharacter::PlayAnimMontage; stopped by stop_montage and at the end of the take.
    void play_montage(const std::string& path, double rate);
    void stop_montage();

    // Action takes: who she fights, how the fight is going, and the take's own time and lens.
    Json locked_target();                                // the lock-on target actor, or null
    Vec3 actor_location(const Json& actor);
    double health_fraction(const Json& actor);           // 0..1, or -1 when unreadable
    bool dead(const Json& actor);
    int tag_count(const char* tag);                      // a gameplay tag on the player
    // Slow motion through the world's cinematic time scale: it multiplies with the game's own
    // time dilation (hit-stop, the Cheat Menu's game speed) instead of replacing it.
    void time_scale(double scale);
    void shake_scale(double scale);                      // the game's camera shake amount, restored after
    void depth_of_field(const Json& camera, double fstop, double focus_cm);
    bool viewing(const Json& camera);                    // is this camera the one on screen
    void hide_target_ui(const Json& enemy);              // its health bar and lock-on marker
    Json camera_component(const Json& camera);

    // Callouts: a component's socket in the world, and a world point on screen (HUD pixels).
    Vec3 socket_location(const Json& component, const std::string& socket);
    bool project(Vec3 world, double& x, double& y);     // the engine's own projection: for measuring the lens
    bool view_point(Vec3& location, Rotator& rotation);
    bool asset_present(const std::string& path) const;  // one load; false when the package is not mounted // where the picture is taken from this frame: one call
    bool key_down(const std::vector<std::string>& keys, std::vector<bool>& out);

    // Restore: take-level changes (cameras, view, walk) or everything including the HUD.
    // Every step runs even when an earlier one fails; returns the failures.
    std::vector<std::string> restore_take();
    // The picture for the length of a take. The game renders shadows for play: coarse on a
    // moving character and from a small page pool, which shows as stair-stepped shadow edges
    // and dotted skin in a close-up. Level 1 smooths the shadows, level 2 also renders at
    // full resolution. Only values that are lower than the target are touched, each is
    // read first, and restore_picture() puts every one back (it works across a world change:
    // console variables belong to the process).
    void picture(int level);
    std::vector<std::string> restore_picture();
    bool picture_raised() const { return !picture_saved_.empty(); }
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
    std::vector<std::pair<std::string, double>> picture_saved_;   // console variables we raised, with the game's own values
    Json world_, world_settings_, lock_, asc_, layout_;
    std::vector<std::pair<Json, bool>> hidden_components_;   // enemy health bars: prior visibility
    // Looked up once per take instead of on every tick: an actor's sub-object by property name.
    Json part(const Json& actor, const char* property);
    std::map<std::pair<uint64_t, std::string>, Json> parts_;
    std::vector<std::pair<Json, int>> hidden_widgets_;       // lock-on markers: prior visibility
    bool time_scaled_ = false, shake_set_ = false;
    double shake_before_ = 1.0;
    std::vector<Json> cameras_;
    bool montage_playing_ = false;
    std::vector<std::pair<Json, int>> hud_;
    bool hud_hidden_ = false, view_changed_ = false, speed_set_ = false, moving_ = false;
    int force_walk_sets_ = 0;
};

} // namespace cine

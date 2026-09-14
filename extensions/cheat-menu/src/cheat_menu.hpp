#pragma once
#include <cssx/client.hpp>
#include "prologue_recovery.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>

namespace cheat {
using cssx::Json;
struct PendingShell {
    std::string target;
    Json controller;
    bool issued=false;
    unsigned close_checks=0,checks=0;
    double elapsed=0;
};
class Menu {
    inline static constexpr const char* toggle_ids[]{"god","auto_heal","infinite_resolve","move_fast","max_shell_points","no_cooldown","perfect_parry","perfect_block","perfect_harden"};
    inline static constexpr const char* combat_ids[]{"no_cooldown","perfect_parry","perfect_block","perfect_harden"};
    inline static constexpr const char* cooldown_fields[]{"CooldownDuration","GlobalCooldownDuration","Cooldown","GlobalCooldown","StoneFormCooldown","PerfectStoneFormCooldown"};
    struct OwnedHook {uint64_t id;std::string feature;Json target;};
    std::map<std::string,OwnedHook> combat_hooks_;
    std::set<uint64_t> cooldown_seen_;
    double combat_time_=0;
    Json owned_abilities(const Json&);
    void combat_sync();
    void combat_clear(const std::string& feature={});
    void combat_hook(const std::string&,const Json&,const Json&);
    cssx::Client host_;
    PrologueRecovery recovery_;
    Json settings_=Json::object();
    Json values_=Json::object(), applied_=Json::object();
    bool cleanup_required_=false;
    void apply_settings();
    void disable_all();
    bool has_changes() const;
    Json shells_=Json::array({{{"id","none"},{"label","Open this page in a loaded world"}}});
    Json current_;
    Json pickup_table_,pickups_=Json::array({{{"id","none"},{"label","Refresh the pickup list"}}});
    void refresh_pickups();
    Json pickup_class(const Json& pawn);
    Json tarstones_=Json::array({{{"id","none"},{"label","Refresh the Tarstone list"}}});
    void refresh_tarstones();
    void tarstone_action(const std::string& id,const Json& player);
    void set_tarstone_levels(const Json& player);
    std::string status_,last_error_,action_error_;
    double refresh_=0,heal_time_=0,resolve_time_=0,catalog_time_=0,points_time_=0;
    bool catalog_ready_=false;
    uint64_t owner_controller_=0;
    std::optional<PendingShell> pending_;
    struct Saved {Json object,before,expected;std::string property;};
    std::map<std::string,Saved> saved_;
    void report(const std::string& value);
    Json require_player();
    void persist(const Json&);
    void shell_tick(double);
    void refresh_shells();
    void unlock_shells(const Json& player);
    void override_value(const Json&,const std::string&,const Json&);
    void restore(const std::string&);
    void god(bool);
    void movement(bool);
    struct MapSaved {Json owner,key,before,expected;};
    std::map<std::string,MapSaved> points_saved_;
    void shell_points(bool);
    bool stopped_=false;
    void apply_event(const Json&);
public:
    explicit Menu(const CssxHost*);
    Json model();
    void event(const Json&);
    void tick(double);
    bool stop();
};
}

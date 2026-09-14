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
    std::string status_,last_error_,action_error_;
    double refresh_=0,heal_time_=0,resolve_time_=0,catalog_time_=0;
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
    void override_value(const Json&,const std::string&,const Json&);
    void restore(const std::string&);
    void god(bool);
    void movement(bool);
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

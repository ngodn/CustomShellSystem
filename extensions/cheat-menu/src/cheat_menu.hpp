#pragma once
#include <cssx/client.hpp>
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
    Json settings_=Json::object();
    Json values_=Json::object();
    Json shells_=Json::array({{{"id","none"},{"label","Open this page in a loaded world"}}});
    Json current_;
    std::string status_,last_error_;
    double refresh_=0,heal_time_=0,resolve_time_=0;
    std::optional<PendingShell> pending_;
    struct Saved {Json object,before,expected;std::string property;};
    std::map<std::string,Saved> saved_;
    void report(const std::string& value);
    Json require_player();
    void persist();
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

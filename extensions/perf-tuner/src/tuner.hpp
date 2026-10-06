#pragma once
// Perf Tuner: more frames per second with the same picture. It applies the catalog's
// tweaks through the game's console, measures each one fairly (off/on/off/on in the same
// scene), finds where the frame time goes (GPU or CPU), and points out settings in the
// player's own config that cost frames for nothing on this machine. Everything it sets it
// reads first and puts back on stop.
#include "audit.hpp"
#include "catalog.hpp"
#include "inifile.hpp"
#include "trial.hpp"
#include <cssx/client.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tuner {

// One console variable the tuner changed and what it read before.
struct Applied { std::string cvar; double before = 0, after = 0; };

// A line of the settings check: what was found and what it means, in plain words.
struct Finding { std::string id, title, text, severity; };   // severity: info, warning

// One line of the player's Engine.ini, judged against the live game.
struct IniVerdict {
    IniLine line;
    // "managed": a variable the tuner sets itself; "game": written by the game's own Video
    // settings; "picture": changes the look; "dead": no such variable in this engine;
    // "setting": an engine setting, not a variable; "kept": a variable the engine knows, left alone.
    std::string kind, note;
    std::string live;          // what the variable reads now, or empty when it does not exist
};

class Tuner {
public:
    explicit Tuner(const CssxHost* host);
    Json model();
    void event(const Json& event);
    void tick(double seconds);
    Json status() const;
    bool stop();

    // For tests: the pieces the menu is built from.
    const Catalog& catalog() const { return catalog_; }
    const std::string& catalog_problem() const { return catalog_problem_; }
    bool enabled() const { return enable_; }
    bool tweak_on(const std::string& id) const;
    const std::map<std::string, std::vector<Applied>>& applied() const { return applied_; }
    std::vector<Finding> findings() const { return findings_; }
    const Machine& machine() const { return machine_; }
    const std::vector<IniVerdict>& ini_verdicts() const { return ini_verdicts_; }
    // Judge the lines of an Engine.ini text against the catalog and the live game (for tests
    // and for the real file alike).
    std::vector<IniVerdict> judge_ini(const IniScan& scan);

private:
    cssx::Client host_;
    Catalog catalog_;
    std::string catalog_problem_;
    Machine machine_;
    // The player's Engine.ini, read once per session when a world is up (the reads need the console).
    std::string ini_path_, ini_pack_;
    bool ini_done_ = false, ini_written_by_game_ = false;
    std::vector<IniVerdict> ini_verdicts_;
    bool enable_ = false;
    std::map<std::string, bool> choices_;                   // per tweak, when the player changed the catalog default
    std::map<std::string, Json> results_;                   // trial results by tweak id (or "all"), with "when"
    Json probe_result_ = Json::object();
    std::map<std::string, std::vector<Applied>> applied_;   // live changes by tweak id; "probe"/"trial:<id>" for measurements
    std::vector<std::string> unavailable_;                  // tweaks this build refused (a value did not take)
    std::vector<Finding> findings_;
    std::string status_, error_;
    Json player_ = Json::object();
    bool world_ready_ = false;
    double clock_ = 0, last_player_check_ = -1, last_findings_ = -1e9, last_brief_ = -1e9, last_invalidate_ = 0;
    Json brief_ = Json::object();
    bool stopped_ = false;

    // The world audit and its one fix.
    Audit audit_;
    AuditSummary audit_summary_;
    bool anim_fix_ = false;            // the player's choice
    int anim_fixed_ = 0;               // meshes changed in this world
    bool audit_wanted_ = false;        // run one when a world is up (the fix is on)
    // Tune: one button that runs the audit, the probe, every switch that fits this machine,
    // and the whole set at the end, keeping what wins. A queue of trial targets.
    bool tuning_ = false;
    std::vector<std::string> tune_queue_;
    size_t tune_total_ = 0;
    Json tune_log_ = Json::array();     // {target, title, verdict, gain_pct, kept}
    Json tune_result_ = Json::object(); // the last finished tune: when, kept, dropped, whole-set gain
    void start_tune();
    void drive_tune();
    void finish_tune(const std::string& why);
    void tune_decide(const std::string& target, const TrialResult& r);
    // The running measurement: a trial of one tweak, of the whole set, the fix, or the probe.
    Trial trial_;
    std::string trial_target_;        // tweak id, "all" or "probe"
    std::string measure_target_ = "all";

    void load_state();
    void save_state();
    void report(const std::string& text);
    Json kismet();
    bool refresh_player();
    std::optional<double> read_cvar(const std::string& name);
    void exec(const std::string& command);
    // Set the tweak's variables and remember what they were. Returns false (and records
    // the tweak as unavailable) when a value did not take.
    bool apply(const Tweak& t, const std::string& key);
    void restore(const std::string& key);
    void apply_enabled();
    void restore_all();
    void start_trial(const std::string& target);
    void drive_trial();
    void switch_trial(bool on);
    Sample sample(double seconds);
    void refresh_findings();
    void check_ini();
    std::string ini_report() const;
    std::string fit_reason(const Tweak& t) const;
    void refresh_brief();
    void drive_audit();
    void start_audit();
    void apply_anim_fix();
    void restore_anim_fix();
    Json sections();
    std::string result_line(const std::string& id) const;
    std::vector<std::string> active_ids() const;
};
}

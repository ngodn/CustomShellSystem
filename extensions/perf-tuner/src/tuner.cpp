#include "tuner.hpp"
#include "machine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace tuner {
namespace fs = std::filesystem;
namespace {
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
std::string today() {
    char text[32] = "";
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::strftime(text, sizeof text, "%d %b %H:%M", &tm);
    std::string s = text;
    if (s.starts_with('0')) s.erase(0, 1);
    return s;
}
std::string percent(double v) {
    char text[32];
    std::snprintf(text, sizeof text, "%+.1f%%", v);
    return text;
}
std::string fixed1(double v) { char text[32]; std::snprintf(text, sizeof text, "%.1f", v); return text; }
std::string env_value(const char* name) {
#ifdef _WIN32
    char buffer[512];
    const DWORD n = GetEnvironmentVariableA(name, buffer, sizeof buffer);
    return n > 0 && n < sizeof buffer ? std::string(buffer, n) : std::string();
#else
    const char* v = std::getenv(name);
    return v ? v : "";
#endif
}
}

Tuner::Tuner(const CssxHost* host) : host_(host), audit_(host_) {
    machine_ = detect_machine();
    try {
        const auto info = host_.request({{"op", "extension.info"}});
        const fs::path file = fs::path(info.at("directory").get<std::string>()) / "tweaks.json";
        std::ifstream in(file);
        if (!in) throw std::runtime_error("tweaks.json is missing from the Perf Tuner folder");
        catalog_ = Catalog::parse(Json::parse(in));
    } catch (const std::exception& e) { catalog_problem_ = e.what(); catalog_ = {}; }
    load_state();
}

void Tuner::load_state() {
    Json s;
    try { s = host_.request({{"op", "state.load"}}); } catch (...) { s = Json::object(); }
    if (!s.is_object()) s = Json::object();
    if (s.value("enable", Json()).is_boolean()) enable_ = s["enable"].get<bool>();
    if (s.value("choices", Json()).is_object()) for (auto& [id, v] : s["choices"].items()) if (v.is_boolean() && catalog_.find(id)) choices_[id] = v.get<bool>();
    if (s.value("results", Json()).is_object()) for (auto& [id, v] : s["results"].items()) if (v.is_object()) results_[id] = v;
    if (s.value("probe", Json()).is_object() && s["probe"].value("verified", false)) {
        probe_result_ = s["probe"];
        const auto v = probe_result_.value("verdict", std::string());   // names from the first build
        if (v == "gpu") probe_result_["verdict"] = "pixels"; else if (v == "cpu") probe_result_["verdict"] = "fixed";
    }
    if (s.value("measure_target", Json()).is_string()) measure_target_ = s["measure_target"].get<std::string>();
    if (measure_target_ != "all" && measure_target_ != "fix.anim" && !catalog_.find(measure_target_)) measure_target_ = "all";
    if (s.value("anim_fix", Json()).is_boolean()) anim_fix_ = s["anim_fix"].get<bool>();
    if (s.value("tune", Json()).is_object()) tune_result_ = s["tune"];
    if (s.value("audit", Json()).is_object()) audit_summary_ = AuditSummary::parse(s["audit"]);
    audit_wanted_ = anim_fix_;
}

void Tuner::save_state() {
    Json choices = Json::object(), results = Json::object();
    for (const auto& [id, v] : choices_) choices[id] = v;
    for (const auto& [id, v] : results_) results[id] = v;
    const Json value = {{"enable", enable_}, {"choices", choices}, {"results", results}, {"probe", probe_result_}, {"measure_target", measure_target_},
                        {"anim_fix", anim_fix_}, {"audit", audit_summary_.json()}, {"tune", tune_result_}};
    try { host_.request({{"op", "state.save"}, {"value", value}}); } catch (...) {}
}

void Tuner::report(const std::string& text) {
    if (status_ == text) return;
    status_ = text;
    try { host_.log(text); } catch (...) {}
    try { host_.invalidate(); } catch (...) {}
}

bool Tuner::tweak_on(const std::string& id) const {
    const auto* t = catalog_.find(id);
    if (!t) return false;
    const auto it = choices_.find(id);
    return it != choices_.end() ? it->second : t->on;
}

std::string Tuner::fit_reason(const Tweak& t) const {
    if (std::find(unavailable_.begin(), unavailable_.end(), t.id) != unavailable_.end()) return "This build keeps one of its variables; the tweak is skipped";
    return unfit_reason(t, machine_);
}

std::vector<std::string> Tuner::active_ids() const {
    std::vector<std::string> ids;
    for (const auto& t : catalog_.tweaks) if (tweak_on(t.id) && fit_reason(t).empty()) ids.push_back(t.id);
    return ids;
}

// ---- the console ----

Json Tuner::kismet() {
    if (!is_handle(kismet_)) kismet_ = host_.find("/Script/Engine.Default__KismetSystemLibrary");
    if (!is_handle(kismet_)) throw std::runtime_error("KismetSystemLibrary is missing");
    return kismet_;
}

bool Tuner::refresh_player() {
    Json p;
    try { p = host_.player(); } catch (...) { p = Json(); }
    const bool ready = p.is_object() && is_handle(p.value("pawn", Json()));
    player_ = ready ? p.at("pawn") : Json(nullptr);
    world_ready_ = ready;
    return ready;
}

std::optional<double> Tuner::read_cvar(const std::string& name) {
    try {
        const auto v = host_.call(kismet(), "GetConsoleVariableFloatValue", {{"VariableName", name}});
        if (v.is_number() && std::isfinite(v.get<double>())) return v.get<double>();
    } catch (...) {}
    return std::nullopt;
}

void Tuner::exec(const std::string& command) {
    // Kismet routes cvars through IConsoleManager even with a null world context.
    host_.call(kismet(), "ExecuteConsoleCommand", {{"WorldContextObject", player_}, {"Command", command}, {"SpecificPlayer", nullptr}});
}

bool Tuner::apply(const Tweak& t, const std::string& key) {
    if (applied_.contains(key)) return true;
    std::vector<Applied> done;
    bool ok = true;
    for (const auto& s : t.set) {
        for (const auto& [owner, values] : applied_) {
            if (owner == key) continue;
            if (std::any_of(values.begin(), values.end(), [&](const Applied& a) { return a.cvar == s.cvar; })) return false;
        }
    }
    for (const auto& s : t.set) {
        const double value = s.resolve(machine_.vram_mb);
        const auto before = read_cvar(s.cvar);
        if (!before) { ok = false; host_.log("Perf Tuner: " + s.cvar + " could not be read; skipped", "warning"); continue; }
        if (std::fabs(*before - value) < 1e-9) continue;   // already there: nothing to change, nothing to restore
        if (s.vram_fraction > 0 && *before > value) continue;   // the player or the game already gives it more: keep theirs
        done.push_back({s.cvar, *before, value});
        try { exec(s.cvar + " " + console_number(value)); } catch (const std::exception& e) { ok = false; host_.log("Perf Tuner: " + s.cvar + ": " + e.what(), "warning"); continue; }
        const auto after = read_cvar(s.cvar);
        if (!after || std::fabs(*after - value) > 1e-6) {
            // The build refused it (a read-only or cheat-protected variable): put back what was there.
            ok = false;
            host_.log("Perf Tuner: " + s.cvar + " did not take (" + (after ? console_number(*after) : "unreadable") + " instead of " + console_number(value) + "); this build keeps it", "warning");
            continue;
        }
    }
    applied_[key] = std::move(done);
    if (!ok) {
        restore(key);
        if (std::find(unavailable_.begin(), unavailable_.end(), t.id) == unavailable_.end()) unavailable_.push_back(t.id);
        return false;
    }
    return true;
}

void Tuner::restore(const std::string& key) {
    const auto it = applied_.find(key);
    if (it == applied_.end()) return;
    std::vector<std::string> failures;
    std::vector<Applied> pending;
    for (auto a = it->second.rbegin(); a != it->second.rend(); ++a) {
        try {
            const auto current = read_cvar(a->cvar);
            if (!current || std::fabs(*current - a->before) > 1e-6) exec(a->cvar + " " + console_number(a->before));
            const auto after = read_cvar(a->cvar);
            if (!after || std::fabs(*after - a->before) > 1e-6) throw std::runtime_error("original value did not take");
        }
        catch (const std::exception& e) { pending.push_back(*a); failures.push_back(a->cvar + ": " + e.what()); }
    }
    if (pending.empty()) applied_.erase(it);
    else { std::reverse(pending.begin(), pending.end()); it->second = std::move(pending); }
    for (const auto& f : failures) host_.log("Perf Tuner: could not restore " + f, "warning");
}

void Tuner::apply_enabled() {
    if (!world_ready_ || !enable_) return;
    for (const auto& t : catalog_.tweaks) {
        if (!tweak_on(t.id) || applied_.contains(t.id) || !fit_reason(t).empty()) continue;
        apply(t, t.id);
    }
}

void Tuner::restore_all() {
    std::vector<std::string> keys;
    for (const auto& [k, _] : applied_) keys.push_back(k);
    for (const auto& k : keys) restore(k);
}

// ---- measuring ----

Sample Tuner::sample(double seconds) {
    Sample s;
    try {
        const auto b = host_.request({{"op", "frame.brief"}, {"seconds", seconds}});
        s.median_ms = b.value("median_ms", 0.0);
        s.mean_ms = b.value("mean_ms", 0.0);
        s.p99_ms = b.value("p99_ms", s.median_ms);
        s.frames = b.value("frames", 0u);
    } catch (...) {}
    return s;
}

void Tuner::refresh_brief() {
    if (clock_ - last_brief_ < 1.0) return;
    last_brief_ = clock_;
    try { brief_ = host_.request({{"op", "frame.brief"}, {"seconds", 5}}); } catch (...) { brief_ = Json::object(); }
}

void Tuner::start_trial(const std::string& target) {
    if (trial_.running() || audit_.running()) throw std::runtime_error("Wait for the current test to finish.");
    if (!world_ready_) throw std::runtime_error("Load into the world first.");
    if (target == "fix.anim" && (audit_.running() || audit_summary_.actors == 0)) throw std::runtime_error("Audit this world first.");
    if (target != "all" && target != "probe" && target != "fix.anim" && !catalog_.find(target)) throw std::runtime_error("Unknown tweak: " + target);
    if (const auto* t = catalog_.find(target); t && !fit_reason(*t).empty()) throw std::runtime_error(fit_reason(*t));
    if (target == "all" && active_ids().empty() && !anim_fix_) throw std::runtime_error("Enable a tweak first.");
    trial_target_ = target;
    TrialPlan plan;
    if (target == "probe") { plan.rounds = 1; plan.settle = 3; plan.measure = 5; }
    trial_.start(clock_, plan);
    report(target == "probe" ? "Resolution test: 16 seconds." : "Measuring " + (target == "all" ? std::string("enabled tweaks") : target == "fix.anim" ? std::string("offscreen animation") : catalog_.find(target)->title) + ".");
}

void Tuner::switch_trial(bool on) {
    if (trial_target_ == "probe") {
        const std::string key = "probe";
        if (on) {
            const auto now = read_cvar("r.ScreenPercentage");
            if (!now || !std::isfinite(*now) || *now <= 0) throw std::runtime_error("Render resolution could not be read.");
            const double current = *now;
            const double target = current < 95 ? 100 : 50;   // already native: go the other way
            probe_result_["from"] = current; probe_result_["to"] = target;
            applied_[key] = {{"r.ScreenPercentage", current, target}};
            exec("r.ScreenPercentage " + console_number(target));
            const auto after = read_cvar("r.ScreenPercentage");
            if (!after || std::fabs(*after - target) > 1e-6) throw std::runtime_error("The game kept its render resolution; the diagnostic cannot compare it.");
        } else {
            restore(key);
            if (applied_.contains(key)) throw std::runtime_error("Render resolution could not be restored.");
        }
        return;
    }
    if (trial_target_ == "fix.anim") { if (on) apply_anim_fix(); else restore_anim_fix(); return; }
    if (trial_target_ == "all" && anim_fix_) { if (on) apply_anim_fix(); else restore_anim_fix(); }
    const std::vector<std::string> ids = trial_target_ == "all" ? active_ids() : std::vector<std::string>{trial_target_};
    for (const auto& id : ids) {
        const auto* t = catalog_.find(id);
        if (!t) continue;
        // While a trial runs, the live copy of the tweak is lifted and the trial owns it.
        restore(id);
        if (on) {
            if (applied_.contains(id) || !apply(*t, "trial." + id)) throw std::runtime_error("Could not apply the measured tweak.");
        } else {
            restore("trial." + id);
            if (applied_.contains(id) || applied_.contains("trial." + id)) throw std::runtime_error("Could not restore the measured tweak.");
        }
    }
}

void Tuner::drive_trial() {
    if (!trial_.running()) return;
    if (!world_ready_) {   // the world went away mid-measurement: give up cleanly
        trial_.cancel();
        try { switch_trial(false); } catch (...) {}
        report("Measurement stopped: the world went away.");
        return;
    }
    try {
        switch (trial_.advance(clock_)) {
            case Trial::Step::Switch: switch_trial(trial_.change_on()); break;
            case Trial::Step::Sample: trial_.record(sample(trial_.plan().measure)); break;
            default: break;
        }
    } catch (const std::exception& e) {
        trial_.cancel();
        try { switch_trial(false); } catch (...) {}
        if (tuning_) finish_tune(e.what());
        else report(std::string("Measurement stopped: ") + e.what());
        return;
    }
    if (trial_.running()) return;
    // Finished: put the live state back and keep the numbers.
    const auto r = trial_.result();
    if (trial_target_ == "probe") {
        restore("probe");
        if (applied_.contains("probe")) { report("Resolution restore pending. Cleanup will retry."); return; }
        Json p = probe_result_;
        p["when"] = today(); p["verified"] = true; p["before_ms"] = r.off_median_ms; p["after_ms"] = r.on_median_ms;
        const double from = p.value("from", 67.0), to = p.value("to", 100.0);
        const double pixels = (to * to) / (from * from);                     // how much more (or less) the GPU drew
        const double expected = r.off_median_ms * (pixels - 1);              // the frame time change if the GPU were the whole frame
        const double seen = r.on_median_ms - r.off_median_ms;
        const double share = expected != 0 ? std::clamp(seen / expected, 0.0, 1.5) : 0;
        p["gpu_share"] = share;
        p["verdict"] = r.off_median_ms <= 0 || r.on_median_ms <= 0 ? "unclear" : share >= 0.6 ? "pixels" : share <= 0.2 ? "fixed" : "mixed";
        probe_result_ = p;
        save_state();
        if (tuning_) tune_decide("probe", r);
        const std::string v = p["verdict"].get<std::string>();
        // What the probe can and cannot tell: more pixels cost more only in the passes that
        // run per pixel. Shadows, Lumen's scene lighting, ray tracing and the CPU side do not
        // scale with the resolution; a frame that ignores the pixel count lives there.
        report(v == "pixels" ? "Frame time changes with resolution. Original value restored."
             : v == "fixed" ? "Little change with resolution. Original value restored."
             : v == "mixed" ? "Frame time partly changes with resolution. Original value restored." : "Resolution test inconclusive. Try again standing still.");
        try { host_.invalidate(); } catch (...) {}
        return;
    }
    switch_trial(false);
    Json j = r.json(); j["when"] = today();
    results_[trial_target_] = j;
    save_state();
    apply_enabled();   // the live copies come back
    if ((trial_target_ == "fix.anim" || trial_target_ == "all") && enable_ && anim_fix_) apply_anim_fix();
    if (tuning_) { tune_decide(trial_target_, r); return; }
    const std::string what = trial_target_ == "all" ? "The tweaks together" : trial_target_ == "fix.anim" ? std::string("'Animate only what is seen'") : "'" + catalog_.find(trial_target_)->title + "'";
    report(what + ": " + result_line(trial_target_));
    try { host_.invalidate(); } catch (...) {}
}

// ---- tune: the one button ----

void Tuner::start_tune() {
    if (tuning_) throw std::runtime_error("Tuning already runs.");
    if (trial_.running() || audit_.running()) throw std::runtime_error("Wait for the running measurement to finish.");
    if (!catalog_problem_.empty()) throw std::runtime_error("Fix tweaks.json first: " + catalog_problem_);
    refresh_player();
    if (!world_ready_) throw std::runtime_error("Load into the world first.");
    if (!enable_) { enable_ = true; apply_enabled(); }
    tune_queue_.clear(); tune_log_ = Json::array();
    tune_queue_.push_back("audit");
    // Switches that start off are the ones to decide by measuring; the always-on group is
    // kept (hitch spacing and resident textures do not show in a 15 s window).
    for (const auto& t : catalog_.tweaks) if (!t.on && fit_reason(t).empty()) tune_queue_.push_back(t.id);
    tune_queue_.push_back("all");
    tune_total_ = tune_queue_.size();
    tuning_ = true;
    report("Tuning: checking the scene.");
}

void Tuner::drive_tune() {
    if (!tuning_ || trial_.running() || audit_.running()) return;
    if (!world_ready_) { finish_tune("the world went away"); return; }
    if (tune_queue_.empty()) { finish_tune(""); return; }
    const auto next = tune_queue_.front();
    tune_queue_.erase(tune_queue_.begin());
    try {
        if (next == "audit") { audit_.start(player_); return; }
        if (next == "fix.anim") {
            // Only worth a minute when the audit found meshes to change (the player's own do not count).
            if (audit_.fixable() < 2) { tune_log_.push_back({{"target", next}, {"title", "Animate only what is seen"}, {"verdict", "skipped"}, {"gain_pct", 0}, {"kept", false}}); return; }
        }
        if (next == "all" && active_ids().empty() && !anim_fix_) { tune_log_.push_back({{"target", next}, {"title", "The kept set"}, {"verdict", "skipped"}, {"gain_pct", 0}, {"kept", false}}); return; }
        start_trial(next);
    } catch (const std::exception& e) { finish_tune(e.what()); }
}

void Tuner::tune_decide(const std::string& target, const TrialResult& r) {
    std::string title = target == "probe" ? "Where the time goes" : target == "all" ? "The kept set together" : target == "fix.anim" ? "Animate only what is seen" : catalog_.find(target) ? catalog_.find(target)->title : target;
    bool kept = false;
    if (target == "probe") { tune_log_.push_back({{"target", target}, {"title", title}, {"verdict", probe_result_.value("verdict", std::string())}, {"gain_pct", 0}, {"kept", false}}); return; }
    if (target == "all") {
        const bool rollback = r.verdict == "loss" || r.hitch_gain_pct < -trial_threshold_pct;
        if (rollback) {
            enable_ = false;
            restore_all(); restore_anim_fix();
            for (auto& entry : tune_log_) entry["kept"] = false;
            save_state();
        }
        tune_log_.push_back({{"target", target}, {"title", title}, {"verdict", r.verdict}, {"gain_pct", r.gain_pct}, {"kept", !rollback}, {"rolled_back", rollback}});
        return;
    }
    kept = r.verdict == "gain" && r.hitch_gain_pct >= -trial_threshold_pct;
    if (target == "fix.anim") {
        anim_fix_ = kept;
        if (kept) apply_anim_fix(); else restore_anim_fix();
    } else {
        choices_[target] = kept;
        if (kept) { if (const auto* t = catalog_.find(target)) kept = apply(*t, target); choices_[target] = kept; }
        else restore(target);
    }
    save_state();
    tune_log_.push_back({{"target", target}, {"title", title}, {"verdict", r.verdict}, {"gain_pct", r.gain_pct}, {"kept", kept}, {"tail_regression", r.hitch_gain_pct < -trial_threshold_pct}});
}

void Tuner::finish_tune(const std::string& why) {
    tuning_ = false; tune_queue_.clear();
    if (!why.empty()) { report("Tuning stopped: " + why + ". What was decided so far stays."); return; }
    std::string kept, dropped; double whole = 0; std::string whole_verdict; bool rolled_back = false;
    for (const auto& e : tune_log_) {
        const auto t = e.value("target", std::string()), v = e.value("verdict", std::string());
        if (t == "probe") continue;
        if (t == "all") { whole = e.value("gain_pct", 0.0); whole_verdict = v; rolled_back = e.value("rolled_back", false); continue; }
        if (v == "skipped") continue;
        if (e.value("kept", false)) kept += (kept.empty() ? "" : ", ") + e.value("title", std::string()) + " (" + percent(e.value("gain_pct", 0.0)) + ")";
        else dropped += (dropped.empty() ? "" : ", ") + e.value("title", std::string());
    }
    tune_result_ = {{"when", today()}, {"kept", kept}, {"dropped", dropped}, {"whole_gain_pct", whole}, {"whole_verdict", whole_verdict}, {"log", tune_log_}, {"rolled_back", rolled_back}};
    save_state();
    if (rolled_back) { report(applied_.empty() && audit_.anim_fix_count() == 0 ? "Slower set rejected. Original settings restored." : "Slower set rejected. Restore pending."); return; }
    report(whole_verdict == "gain" ? "Tune complete: " + fixed1(whole) + "% lower frame time."
         : whole_verdict == "loss" ? "Tune complete: combined set measured slower."
         : whole_verdict == "same" ? "Tune complete: no combined gain measured."
         : "Tune complete: combined result inconclusive.");
}

// ---- the world audit ----

void Tuner::start_audit() {
    if (audit_.running()) throw std::runtime_error("The audit is already running.");
    if (trial_.running()) throw std::runtime_error("Wait for the measurement to finish.");
    if (!world_ready_) throw std::runtime_error("Load into the world first.");
    restore_anim_fix();
    audit_.start(player_);
    report("Auditing the world: a few seconds.");
}

void Tuner::drive_audit() {
    if (!audit_.running()) return;
    if (!world_ready_) { audit_.cancel(); report("Audit stopped: the world went away."); return; }
    if (!audit_.step(300)) return;   // about 300 reflected calls a tick: no hitch
    audit_summary_ = audit_.summary();
    try { host_.request({{"op", "output.write"}, {"file", "world-audit-report.txt"}, {"text", audit_.report()}}); } catch (...) {}
    save_state();
    if (enable_ && anim_fix_) apply_anim_fix();
    const auto& a = audit_summary_;
    report("Scan done: " + std::to_string(a.actors) + " actors, " + std::to_string(a.lights_shadow) + " shadow lights.");
}

void Tuner::apply_anim_fix() {
    if (!world_ready_ || audit_.running()) return;
    anim_fixed_ = audit_.apply_anim_fix(player_);
}

void Tuner::restore_anim_fix() {
    audit_.restore_anim_fix();
    anim_fixed_ = audit_.anim_fix_count();
}

// ---- the player's Engine.ini ----

std::vector<IniVerdict> Tuner::judge_ini(const IniScan& scan) {
    std::vector<IniVerdict> out;
    auto lower = [](std::string x) { for (auto& c : x) c = char(std::tolower(static_cast<unsigned char>(c))); return x; };
    std::map<std::string, std::pair<std::string, double>> managed;   // cvar -> tweak title, value
    for (const auto& t : catalog_.tweaks) for (const auto& st : t.set) managed[lower(st.cvar)] = {t.title, st.value};
    for (const auto& l : scan.lines) {
        IniVerdict v; v.line = l;
        if (l.setting) { v.kind = "setting"; v.note = "An engine setting read at start, not a console variable; left as it is."; out.push_back(v); continue; }
        try {
            const auto r = host_.call(kismet(), "GetConsoleVariableStringValue", {{"VariableName", l.name}});
            if (r.is_string()) v.live = r.get<std::string>();
        } catch (...) {}
        const auto key = lower(l.name);
        if (v.live.empty()) { v.kind = "dead"; v.note = "No such variable in this engine (5.6); the line does nothing."; }
        else if (managed.contains(key)) {
            const auto& [title, value] = managed.at(key);
            v.kind = "managed";
            v.note = "The tuner's '" + title + "' sets this to " + console_number(value) + " while it is on and puts your value back when off.";
            (void)0;
        }
        else if (game_owned_cvar(l.name)) { v.kind = "game"; v.note = "Written by the game's own Video settings; change it there."; }
        else if (picture_cvar(l.name)) {
            v.kind = "picture";
            const auto why = forbidden_reason(l.name);
            v.note = (why.empty() ? std::string("Changes the look of the game") : why) + "; left as you set it, but it is not a free frame-rate change.";
        }
        else { v.kind = "kept"; v.note = (v.live == l.value || (v.live == "true" && l.value == "1") || (v.live == "false" && l.value == "0")) ? "Known to the engine; left alone." : "Known to the engine; reads " + v.live + " now (something set it after the file was read)."; }
        out.push_back(v);
    }
    return out;
}

void Tuner::check_ini() {
    if (ini_done_ || !world_ready_) return;
    ini_done_ = true;
    try {
        const auto saved = host_.call(kismet(), "GetProjectSavedDirectory");
        if (!saved.is_string()) return;
        ini_path_ = saved.get<std::string>();
        if (!ini_path_.empty() && ini_path_.back() != '/' && ini_path_.back() != '\\') ini_path_ += '/';
        ini_path_ += "Config/Windows/Engine.ini";
        std::ifstream in(ini_path_);
        if (!in) { ini_path_.clear(); return; }
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto scan = scan_engine_ini(text);
        ini_pack_ = scan.pack; ini_written_by_game_ = scan.written_by_game;
        ini_verdicts_ = judge_ini(scan);
        try { host_.request({{"op", "output.write"}, {"file", "engine-ini-report.txt"}, {"text", ini_report()}}); } catch (...) {}
        last_findings_ = -1e9;
    } catch (const std::exception& e) { host_.log(std::string("Perf Tuner: could not read Engine.ini: ") + e.what(), "warning"); }
}

std::string Tuner::ini_report() const {
    std::string r = "Perf Tuner: your Engine.ini, line by line\n" + ini_path_ + "\n";
    if (!ini_pack_.empty()) r += "Recognised pack: " + ini_pack_ + "\n";
    if (ini_written_by_game_) r += "The game rewrites this file with its own video settings; lines it does not know are kept.\n";
    r += "\nThe tuner never edits this file. 'dead' lines do nothing in this engine; 'picture' lines change the look and are not free frame rate;\n'managed' lines are overridden by the tuner while it is on; 'game' lines come from the Video settings menu.\n\n";
    for (const auto& v : ini_verdicts_) {
        r += std::to_string(v.line.number) + "  [" + v.kind + "]  " + v.line.name + "=" + v.line.value;
        const bool same = v.live == v.line.value || (v.live == "true" && v.line.value == "1") || (v.live == "false" && v.line.value == "0");
        if (!v.live.empty() && !same) r += "  (reads " + v.live + ")";
        r += "\n    " + v.note + "\n";
    }
    return r;
}

// ---- the settings check ----

void Tuner::refresh_findings() {
    if (clock_ - last_findings_ < 3.0) return;
    last_findings_ = clock_;
    std::vector<Finding> f;
    auto add = [&](const char* id, const std::string& title, const std::string& text, const char* severity) { f.push_back({id, title, text, severity}); };
    {
        std::string card = machine_.gpu_name.empty() ? "an unknown adapter" : machine_.gpu_name;
        if (machine_.vram_mb > 0) card += " with " + console_number(std::round(machine_.vram_mb / 1024)) + " GB";
        add("machine", "This machine", (machine_.wine ? "Proton (Wine " + machine_.wine_version + "), " : std::string("Windows, ")) + card + (machine_.steam_deck ? ", Steam Deck" : "") + ". Unsupported tweaks are disabled.", "info");
    }
    if (machine_.wine) {
        add("proton", "Proton", "DirectX 12 runs through VKD3D-Proton. Tune measures this setup; Windows results may differ.", "info");
        const auto cfg = env_value("VKD3D_CONFIG");
        if (!cfg.empty()) add("vkd3d", "VKD3D_CONFIG", "Set to '" + cfg + "' in the launch options.", "info");
        const auto rate = env_value("DXVK_FRAME_RATE");
        if (!rate.empty() && rate != "0") add("dxvk_rate", "DXVK frame cap", "DXVK_FRAME_RATE=" + rate + " caps the game outside its own settings.", "warning");
        const auto vk_rate = env_value("VKD3D_FRAME_RATE");
        if (!vk_rate.empty() && vk_rate != "0") add("vkd3d_rate", "VKD3D frame cap", "VKD3D_FRAME_RATE=" + vk_rate + " caps the game outside its own settings.", "warning");
    }
    if (!ini_verdicts_.empty()) {
        size_t dead = 0, picture = 0, managed = 0;
        for (const auto& v : ini_verdicts_) { if (v.kind == "dead") ++dead; else if (v.kind == "picture") ++picture; else if (v.kind == "managed") ++managed; }
        std::string text = "Your Engine.ini has " + std::to_string(ini_verdicts_.size()) + " setting lines" + (ini_pack_.empty() ? "" : " (" + ini_pack_ + ")") + ": "
            + std::to_string(dead) + " do nothing, " + std::to_string(picture) + " change the look, " + std::to_string(managed) + " managed tweaks. The file is unchanged.";
        add("ini", "Engine.ini", text, picture > 0 ? "warning" : "info");
    } else if (!ini_path_.empty()) add("ini", "Engine.ini", "Your Engine.ini has no console variable lines; nothing from the web is set there.", "info");
    if (world_ready_) {
        auto num = [&](const char* name) { return read_cvar(name); };
        if (const auto v = num("r.GPUCrashDebugging"); v && *v > 0)
            add("gpucrash", "GPU crash debugging", "On. Adds work for crash reports. Leave it on if you are diagnosing a crash.", "warning");
        if (const auto v = num("r.Streamline.DLSSG.Enable"); v && *v > 0) {
            add("dlssg", "Frame generation", "Enabled in the config. Check support in Video settings; the config alone does not prove it is running.", "info");
        }
        if (const auto v = num("t.Streamline.Reflex.Enable"); v && *v > 0)
            add("reflex", "Reflex", "On. Reduces input latency. Tune tests whether turning it off helps frame time on this machine.", "info");
        if (const auto v = num("r.RayTracing.Enable"); v && *v > 0)
            add("hwrt", "Hardware ray tracing", "On. Tune leaves this picture setting unchanged. Compare it yourself in Video settings if needed.", "info");
        if (const auto v = num("r.Streaming.PoolSize"); v && *v > 0 && *v < 1500 && !applied_.contains("texture_pool"))
            add("pool", "Texture pool", console_number(*v) + " MB. The texture-pool tweak can raise this on supported cards.", "info");
        if (const auto pages = num("r.Shadow.Virtual.MaxPhysicalPages"); pages && *pages > 0 && *pages < 2048 && audit_summary_.lights_shadow >= 12)
            add("vsm", "Shadow page pool", console_number(*pages) + " pages; " + std::to_string(audit_summary_.lights_shadow) + " shadow lights in the last scan. This count alone does not prove the pool is full. Tune leaves it unchanged.", "info");
        if (const auto v = num("r.VSync"); v && *v > 0) add("vsync", "V-sync", "On. Limits displayed frames to the refresh rate and can hide measured gains.", "info");
        if (const auto sp = num("r.ScreenPercentage"); sp && *sp > 0 && *sp < 100)
            add("sp", "Render resolution", fixed1(*sp) + "%. Tune leaves this unchanged. Only the separate resolution test changes it temporarily.", "info");
    } else add("world", "No world", "Load into the world to read the game's settings.", "info");
    findings_ = std::move(f);
}

// ---- the menu ----

std::string Tuner::result_line(const std::string& id) const {
    const auto it = results_.find(id);
    if (it == results_.end()) return "Not measured yet";
    const auto& r = it->second;
    const auto verdict = r.value("verdict", std::string("unclear"));
    const auto when = r.value("when", std::string());
    const double gain = r.value("gain_pct", 0.0), off = r.value("off_median_ms", 0.0), on = r.value("on_median_ms", 0.0);
    std::string line = verdict == "gain" ? fixed1(gain) + "% lower frame time (" + fixed1(off) + " to " + fixed1(on) + " ms)"
                     : verdict == "loss" ? fixed1(-gain) + "% higher frame time (" + fixed1(off) + " to " + fixed1(on) + " ms)"
                     : verdict == "same" ? "No measured gain (" + fixed1(off) + " vs " + fixed1(on) + " ms)"
                     : "Inconclusive. Test again in the same spot.";
    return when.empty() ? line : line + ", " + when;
}

Json Tuner::sections() {
    Json out = Json::array();
    const bool busy = trial_.running() || tuning_ || audit_.running();
    // Tuner
    {
        Json controls = Json::array();
        std::string tuned = "Not tuned yet";
        if (tune_result_.is_object() && tune_result_.contains("when")) {
            const auto kept = tune_result_.value("kept", std::string());
            tuned = (tune_result_.value("rolled_back", false) ? std::string("Slower set rejected") : kept.empty() ? std::string("No extra gains. Current tweaks kept") : "Kept " + kept) + ", " + tune_result_.value("when", std::string());
        }
        if (!world_ready_) tuned = "Load your save first";
        else if (tuning_) tuned = audit_.running() ? "Checking scene" : "Testing " + std::to_string(tune_total_ - tune_queue_.size()) + "/" + std::to_string(tune_total_);
        controls.push_back({{"id", "tune"}, {"type", "button"}, {"label", "Tune"}, {"value", tuned}, {"busy", tuning_}, {"enabled", !busy && world_ready_ && catalog_problem_.empty()},
            {"effect", "reversible"}, {"description", "Tests tweaks for this machine and keeps measured gains. Your resolution and graphics settings stay as they are."},
            {"hint", "Takes several minutes. Close the menu and stay in one area. Stop cancels and restores your settings."}});
        controls.push_back({{"id", "enable"}, {"type", "toggle"}, {"label", "Enabled"}, {"value", enable_}, {"enabled", enable_ || (!busy && catalog_problem_.empty())},
            {"description", "Use the selected tweaks. Off restores the values from before the tuner ran."}});
        if (busy) controls.push_back({{"id", "stop"}, {"type", "button"}, {"label", tuning_ ? "Stop tuning" : trial_.running() ? "Stop test" : "Stop scan"}, {"enabled", true},
            {"description", "Cancel the current work and turn the tuner off. Restores every value it changed."}});
        std::string fps = "Load into the world to read it";
        if (brief_.is_object() && brief_.value("frames", 0) > 0) {
            fps = fixed1(brief_.value("hz", 0.0)) + " fps, " + fixed1(brief_.value("median_ms", 0.0)) + " ms a frame, slowest 1%: " + fixed1(brief_.value("p99_ms", 0.0)) + " ms";
        }
        controls.push_back({{"id", "fps"}, {"type", "label"}, {"label", "Frame rate"}, {"value", fps}, {"description", "The last five seconds, from the loader's own frame clock."}});
        out.push_back({{"id", "tuner"}, {"title", "Tune"}, {"controls", controls}});
    }
    {
        Json controls = Json::array();
        std::string probe = "Not measured yet";
        if (probe_result_.is_object() && probe_result_.contains("verdict")) {
            const auto v = probe_result_.value("verdict", std::string());
            probe = (v == "pixels" ? "Scales with pixels" : v == "fixed" ? "Does not scale with pixels" : v == "mixed" ? "Partly" : "Unclear") + std::string(": ") + fixed1(probe_result_.value("before_ms", 0.0)) + " ms became " + fixed1(probe_result_.value("after_ms", 0.0)) + " ms at " + console_number(probe_result_.value("to", 100.0)) + "%, " + probe_result_.value("when", std::string());
        }
        controls.push_back({{"id", "probe"}, {"type", "button"}, {"label", "Resolution test"}, {"value", probe}, {"busy", trial_.running() && trial_target_ == "probe"}, {"enabled", !busy && world_ready_},
            {"effect", "reversible"}, {"description", "Checks whether frame time changes with render resolution. Cannot identify a CPU or GPU bottleneck. Tune never runs this test."},
            {"hint", "16 seconds. Stand still."}, {"confirm", "Temporarily set render resolution to 100%, or 50% if already native? Your current value is restored afterward."}});
        Json options = Json::array({{{"id", "all"}, {"label", "All tweaks that are on"}}});
        for (const auto& t : catalog_.tweaks) options.push_back({{"id", t.id}, {"label", t.title}});
        options.push_back({{"id", "fix.anim"}, {"label", "Animate only what is seen (world fix)"}});
        controls.push_back({{"id", "measure_target"}, {"type", "choice"}, {"label", "Test"}, {"value", measure_target_}, {"options", options}, {"enabled", !busy},
            {"description", "Choose a tweak or test all enabled tweaks together."}});
        controls.push_back({{"id", "measure"}, {"type", "button"}, {"label", "Run test"}, {"value", result_line(measure_target_)},
            {"busy", trial_.running() && trial_target_ != "probe"}, {"enabled", !busy && world_ready_ && catalog_problem_.empty()}, {"effect", "reversible"},
            {"description", "Compares off/on/off/on over one minute. Reports a gain only when both on rounds are faster and the average improves by at least 1.5%."}, {"hint", "Stay in one area. Moving scenes can make the result inconclusive."}});
        out.push_back({{"id", "tests"}, {"title", "Tests"}, {"controls", controls}});
    }
    // Tweaks, by group
    if (!catalog_problem_.empty()) {
        out.push_back({{"id", "catalog"}, {"title", "Tweaks"}, {"controls", Json::array({{{"id", "catalog_problem"}, {"type", "label"}, {"label", "tweaks.json"}, {"value", catalog_problem_}, {"severity", "danger"}}})}});
    } else {
        std::vector<std::string> groups;
        for (const auto& t : catalog_.tweaks) if (std::find(groups.begin(), groups.end(), t.group) == groups.end()) groups.push_back(t.group);
        for (const auto& g : groups) {
            Json controls = Json::array();
            for (const auto& t : catalog_.tweaks) {
                if (t.group != g) continue;
                const auto unfit = fit_reason(t);
                const bool off_limits = !unfit.empty();
                std::string hint = result_line(t.id);
                if (off_limits) hint = unfit;
                else if (applied_.contains(t.id)) hint += " (live)";
                std::string what = t.what + " Sets ";
                for (size_t i = 0; i < t.set.size(); ++i) what += (i ? ", " : "") + t.set[i].cvar + " " + console_number(t.set[i].resolve(machine_.vram_mb));
                what += ".";
                if (t.picture == "check") what += " Worth a look of your own: take the same screenshot with it off and on.";
                controls.push_back({{"id", "tweak." + t.id}, {"type", "toggle"}, {"label", t.title}, {"value", tweak_on(t.id)}, {"enabled", !busy && !off_limits},
                    {"hint", hint}, {"unit", t.side == "gpu" ? "GPU" : t.side == "cpu" ? "CPU" : "GPU+CPU"}, {"description", what}});
            }
            std::string slug; for (unsigned char c : g) slug += std::isalnum(c) ? char(std::tolower(c)) : '_';
            out.push_back({{"id", "group." + slug}, {"title", g}, {"controls", controls}});
        }
    }
    // The world
    {
        Json controls = Json::array();
        const auto& a = audit_summary_;
        std::string summary = a.actors ? std::to_string(a.actors) + " actors, " + std::to_string(a.lights_shadow) + " shadow lights, " + std::to_string(a.skeletal_always_unseen) + " unseen meshes animating" : "Not scanned yet";
        controls.push_back({{"id", "audit"}, {"type", "button"}, {"label", "Scan scene"}, {"value", summary}, {"busy", audit_.running()}, {"enabled", !busy && world_ready_},
            {"description", "Counts characters, animated props, lights and effects in batches. A scan alone changes nothing."}, {"hint", "Report: Mods/CSSX/output/eins0fx.perf-tuner/world-audit-report.txt"}});
        std::string hint = result_line("fix.anim");
        if (anim_fixed_) hint += " (" + std::to_string(anim_fixed_) + " meshes live)";
        controls.push_back({{"id", "fix.anim"}, {"type", "toggle"}, {"label", "Offscreen animation"}, {"value", anim_fix_}, {"enabled", !busy}, {"hint", hint}, {"unit", "CPU"},
            {"description", "Experimental: reduces animation work on other characters while unseen. Montages continue; ordinary pose updates stop. Your character is excluded. Tune does not enable this."},
            {"confirm", "Change offscreen animation? This can affect gameplay and moving shadows. Turn it off if you see a problem."}});
        out.push_back({{"id", "world"}, {"title", "Scene"}, {"controls", controls}});
    }
    // Your Engine.ini: the lines that matter, the rest in the report file
    if (!ini_verdicts_.empty()) {
        Json controls = Json::array();
        size_t shown = 0;
        for (const auto& v : ini_verdicts_) {
            if (v.kind != "picture" && v.kind != "managed") continue;
            if (++shown > 12) break;
            controls.push_back({{"id", "ini." + std::to_string(v.line.number)}, {"type", "label"}, {"label", v.line.name + " = " + v.line.value}, {"value", v.note}, {"severity", v.kind == "picture" ? "warning" : "info"}});
        }
        if (controls.empty()) controls.push_back({{"id", "ini.none"}, {"type", "label"}, {"label", "Lines"}, {"value", "Nothing in it changes the look or collides with the tuner."}});
        out.push_back({{"id", "ini"}, {"title", "Your Engine.ini"}, {"description", "Read-only. Shows picture changes and managed tweaks. Full report: Mods/CSSX/output/eins0fx.perf-tuner/engine-ini-report.txt"}, {"controls", controls}});
    }
    // The check
    {
        Json controls = Json::array();
        for (const auto& f : findings_) controls.push_back({{"id", "find." + f.id}, {"type", "label"}, {"label", f.title}, {"value", f.text}, {"severity", f.severity}});
        if (controls.empty()) controls.push_back({{"id", "find.none"}, {"type", "label"}, {"label", "Settings"}, {"value", "Nothing to point out."}});
        out.push_back({{"id", "check"}, {"title", "Settings"}, {"description", "Your machine and current settings. Read-only."}, {"controls", controls}});
    }
    return out;
}

Json Tuner::model() {
    refresh_brief();
    refresh_findings();
    Json status = status_;
    if (audit_.running()) status = "Scanning scene, " + std::to_string(int(std::round(audit_.progress() * 100))) + "%";
    if (trial_.running()) {
        const int pct = int(std::round(trial_.progress(clock_) * 100));
        const std::string what = trial_target_ == "probe" ? "the probe" : trial_target_ == "all" ? "the kept set" : trial_target_ == "fix.anim" ? "'Animate only what is seen'" : catalog_.find(trial_target_) ? "'" + catalog_.find(trial_target_)->title + "'" : trial_target_;
        status = (trial_target_ == "probe" ? "Resolution test" : "Measuring " + what) + std::string(", ") + std::to_string(pct) + "%: " + (trial_target_ == "probe" ? (trial_.change_on() ? "test resolution" : "your resolution") : (trial_.change_on() ? "on" : "off"));
    }
    if (tuning_) status = "Tuning, step " + std::to_string(tune_total_ - tune_queue_.size()) + " of " + std::to_string(tune_total_) + ". " + status.get<std::string>();
    auto pages = sections();
    // CSSX renders label descriptions and button hints, not their value fields.
    for (auto& page : pages) for (auto& control : page["controls"]) {
        const auto type = control.value("type", std::string());
        const auto value = control.value("value", Json());
        if (!value.is_string() || value.get_ref<const std::string&>().empty()) continue;
        if (type == "label" || type == "button") {
            const char* field = type == "label" ? "description" : "hint";
            const auto explanation = control.value(field, std::string());
            control[field] = value.get<std::string>() + (explanation.empty() ? std::string() : (type == "label" ? " " : "\n") + explanation);
        }
    }
    return {{"schema", 2}, {"sections", std::move(pages)}, {"status", status}, {"error", error_}};
}

void Tuner::event(const Json& event) {
    error_.clear();
    try {
        const auto id = event.at("id").get<std::string>();
        if (id == "enable" || id == "stop") {
            const bool on = id == "enable" && event.at("value").get<bool>();
            if (!on) { tuning_ = false; tune_queue_.clear(); trial_.cancel(); audit_.cancel(); }
            else if (trial_.running() || tuning_) throw std::runtime_error("Wait for the current test to finish.");
            if (on && !catalog_problem_.empty()) throw std::runtime_error("Fix tweaks.json first: " + catalog_problem_);
            enable_ = on;
            if (enable_) { refresh_player(); apply_enabled(); if (anim_fix_) { if (audit_summary_.actors > 0 && !audit_.running()) apply_anim_fix(); else if (world_ready_ && !audit_.running()) { try { audit_.start(player_); } catch (...) {} } } }
            else { refresh_player(); restore_all(); restore_anim_fix(); audit_.cancel(); }
            save_state();
            if (!enable_ && (!applied_.empty() || audit_.anim_fix_count() > 0)) throw std::runtime_error("Some original values could not be restored; cleanup will retry.");
            const auto n = applied_.size();
            report(enable_ ? (n ? "Perf Tuner on: " + std::to_string(n) + (n == 1 ? " tweak" : " tweaks") + " live." : world_ready_ ? "Perf Tuner on; no tweak took effect." : "Perf Tuner on; the tweaks apply once you are in the world.") : "Perf Tuner off; everything put back.");
            return;
        }
        if (id.starts_with("tweak.")) {
            if (trial_.running()) throw std::runtime_error("Wait for the measurement to finish.");
            const auto tid = id.substr(6);
            const auto* t = catalog_.find(tid);
            if (!t) throw std::runtime_error("Unknown tweak: " + tid);
            const bool on = event.at("value").get<bool>();
            if (on && !fit_reason(*t).empty()) throw std::runtime_error(fit_reason(*t) + ".");
            choices_[tid] = on;
            if (enable_) { refresh_player(); if (on) { if (world_ready_) apply(*t, tid); } else restore(tid); }
            save_state();
            report(t->title + (on ? (applied_.contains(tid) ? ": on, live." : enable_ ? ": on; it applies in the world." : ": on; it applies once the tuner is on.") : ": off."));
            return;
        }
        if (id == "measure_target") {
            measure_target_ = event.at("value").get<std::string>();
            if (measure_target_ != "all" && measure_target_ != "fix.anim" && !catalog_.find(measure_target_)) throw std::runtime_error("Unknown tweak.");
            save_state();
            return;
        }
        if (id == "measure") { refresh_player(); start_trial(measure_target_); return; }
        if (id == "tune") { start_tune(); return; }
        if (id == "audit") { refresh_player(); start_audit(); return; }
        if (id == "fix.anim") {
            if (trial_.running()) throw std::runtime_error("Wait for the measurement to finish.");
            anim_fix_ = event.at("value").get<bool>();
            refresh_player();
            if (anim_fix_) {
                if (enable_ && world_ready_) { if (audit_summary_.actors > 0 && !audit_.running()) apply_anim_fix(); else if (!audit_.running()) { try { audit_.start(player_); } catch (...) {} } }
            } else restore_anim_fix();
            save_state();
            report(anim_fix_ ? (anim_fixed_ ? "Animate only what is seen: " + std::to_string(anim_fixed_) + " meshes changed." : enable_ ? "Animate only what is seen: on; it applies after the audit." : "Animate only what is seen: on; it applies once the tuner is on.") : "Animate only what is seen: off, meshes put back.");
            return;
        }
        if (id == "probe") { refresh_player(); start_trial("probe"); return; }
        throw std::runtime_error("Unknown Perf Tuner control: " + id);
    } catch (const std::exception& e) {
        error_ = e.what();
        try { host_.invalidate(); } catch (...) {}
        throw;
    }
}

void Tuner::tick(double seconds) {
    if (stopped_) return;
    clock_ += seconds;
    if (clock_ - last_player_check_ >= 1.0) {
        last_player_check_ = clock_;
        const bool was = world_ready_;
        refresh_player();
        if (!trial_.running()) {
            const bool cleanup_pending = !enable_ && (!applied_.empty() || audit_.anim_fix_count() > 0);
            std::vector<std::string> pending;
            for (const auto& [key, _] : applied_) {
                const auto* t = catalog_.find(key);
                if (!enable_ || key == "probe" || key.starts_with("trial.") || (t && (!tweak_on(key) || !fit_reason(*t).empty()))) pending.push_back(key);
            }
            for (const auto& key : pending) restore(key);
            if (!enable_ || !anim_fix_) {
                if (audit_.anim_fix_count() > 0) restore_anim_fix();
            }
            if (cleanup_pending && applied_.empty() && audit_.anim_fix_count() == 0) {
                error_.clear();
                report("Off. Original settings restored.");
            }
        }
        // Console variables belong to the process, not the world: what was set stays set
        // through a load, and what was read before stays the thing to restore. A new world
        // only needs whatever was waiting for one.
        if (was != world_ready_) { last_findings_ = -1e9; try { host_.invalidate(); } catch (...) {} }   // the menu is cached until told otherwise
        if (!was && world_ready_) {
            check_ini(); apply_enabled();
            // The old world's meshes are gone with it; a new world needs its own audit for the fix.
            anim_fixed_ = 0;
            if (enable_ && anim_fix_ && !audit_.running()) { try { audit_.start(player_); } catch (...) {} }
        }
        if (was && !world_ready_) { audit_.cancel(); restore_anim_fix(); }
    }
    drive_audit();
    drive_trial();
    drive_tune();
    // The menu is a cached model: the frame-rate line and the progress of an audit or a
    // measurement only move when the tuner says it has something new.
    const double period = (audit_.running() || trial_.running()) ? 1.0 : 2.0;
    if (clock_ - last_invalidate_ >= period) { last_invalidate_ = clock_; try { host_.invalidate(); } catch (...) {} }
}

Json Tuner::status() const {
    const auto n = applied_.size();
    if (tuning_) return {{"summary", "Tuning"}, {"active", true}};
    if (trial_.running()) return {{"summary", trial_target_ == "probe" ? "Probing the frame" : "Measuring"}, {"active", true}};
    return {{"summary", enable_ ? (n ? std::to_string(n) + (n == 1 ? " tweak live" : " tweaks live") : "On, nothing applied yet") : "Off"}, {"active", enable_}};
}

bool Tuner::stop() {
    stopped_ = true;
    tuning_ = false; tune_queue_.clear();
    refresh_player();
    trial_.cancel();
    audit_.cancel();
    restore_all();
    restore_anim_fix();
    return applied_.empty() && audit_.anim_fix_count() == 0;
}
}

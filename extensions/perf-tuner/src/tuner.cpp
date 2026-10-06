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
    if (s.value("probe", Json()).is_object()) {
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
    auto h = host_.find("/Script/Engine.Default__KismetSystemLibrary");
    if (!is_handle(h)) throw std::runtime_error("KismetSystemLibrary is missing");
    return h;
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
        if (v.is_number()) return v.get<double>();
    } catch (...) {}
    return std::nullopt;
}

void Tuner::exec(const std::string& command) {
    if (!world_ready_) throw std::runtime_error("No world to run a console command in yet");
    host_.call(kismet(), "ExecuteConsoleCommand", {{"WorldContextObject", player_}, {"Command", command}, {"SpecificPlayer", nullptr}});
}

bool Tuner::apply(const Tweak& t, const std::string& key) {
    if (applied_.contains(key)) return true;
    std::vector<Applied> done;
    bool ok = true;
    for (const auto& s : t.set) {
        const double value = s.resolve(machine_.vram_mb);
        const auto before = read_cvar(s.cvar);
        if (!before) { ok = false; host_.log("Perf Tuner: " + s.cvar + " could not be read; skipped", "warning"); continue; }
        if (std::fabs(*before - value) < 1e-9) continue;   // already there: nothing to change, nothing to restore
        if (s.vram_fraction > 0 && *before > value) continue;   // the player or the game already gives it more: keep theirs
        try { exec(s.cvar + " " + console_number(value)); } catch (const std::exception& e) { ok = false; host_.log("Perf Tuner: " + s.cvar + ": " + e.what(), "warning"); continue; }
        const auto after = read_cvar(s.cvar);
        if (!after || std::fabs(*after - value) > 1e-6) {
            // The build refused it (a read-only or cheat-protected variable): put back what was there.
            ok = false;
            try { exec(s.cvar + " " + console_number(*before)); } catch (...) {}
            host_.log("Perf Tuner: " + s.cvar + " did not take (" + (after ? console_number(*after) : "unreadable") + " instead of " + console_number(value) + "); this build keeps it", "warning");
            continue;
        }
        done.push_back({s.cvar, *before, *after});
    }
    if (!ok) {
        for (auto it = done.rbegin(); it != done.rend(); ++it) { try { exec(it->cvar + " " + console_number(it->before)); } catch (...) {} }
        if (std::find(unavailable_.begin(), unavailable_.end(), t.id) == unavailable_.end()) unavailable_.push_back(t.id);
        return false;
    }
    applied_[key] = std::move(done);
    return true;
}

void Tuner::restore(const std::string& key) {
    const auto it = applied_.find(key);
    if (it == applied_.end()) return;
    std::vector<std::string> failures;
    for (auto a = it->second.rbegin(); a != it->second.rend(); ++a) {
        try { exec(a->cvar + " " + console_number(a->before)); }
        catch (const std::exception& e) { failures.push_back(a->cvar + ": " + e.what()); }
    }
    applied_.erase(it);
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
    if (trial_.running()) throw std::runtime_error("A measurement is already running.");
    if (!world_ready_) throw std::runtime_error("Load into the world first.");
    if (target == "fix.anim" && (audit_.running() || audit_summary_.actors == 0)) throw std::runtime_error("Audit this world first.");
    if (target != "all" && target != "probe" && target != "fix.anim" && !catalog_.find(target)) throw std::runtime_error("Unknown tweak: " + target);
    if (target == "all" && active_ids().empty()) throw std::runtime_error("No tweak is on; turn one on first.");
    trial_target_ = target;
    TrialPlan plan;
    if (target == "probe") { plan.rounds = 1; plan.settle = 3; plan.measure = 5; }
    trial_.start(clock_, plan);
    report(target == "probe" ? "Measuring where the frame time goes: 16 seconds, stand still." :
           "Measuring " + (target == "all" ? std::string("the tweaks") : target == "fix.anim" ? std::string("'Animate only what is seen'") : "'" + catalog_.find(target)->title + "'") + ": about a minute, off and on twice. Keep playing or stand still, either is fine.");
}

void Tuner::switch_trial(bool on) {
    if (trial_target_ == "probe") {
        // The probe: render at 100 percent for one window. If the frame takes much longer the
        // GPU is the limit; if it hardly changes, the CPU is. Picture-only, put back after.
        const std::string key = "probe";
        if (on) {
            const auto now = read_cvar("r.ScreenPercentage");
            const double current = now && *now > 0 ? *now : 100;
            const double target = current < 95 ? 100 : 50;   // already native: go the other way
            probe_result_["from"] = current; probe_result_["to"] = target;
            try { exec("r.ScreenPercentage " + console_number(target)); applied_[key] = {{"r.ScreenPercentage", current, target}}; } catch (...) {}
        } else restore(key);
        return;
    }
    if (trial_target_ == "fix.anim") { if (on) apply_anim_fix(); else restore_anim_fix(); return; }
    const std::vector<std::string> ids = trial_target_ == "all" ? active_ids() : std::vector<std::string>{trial_target_};
    for (const auto& id : ids) {
        const auto* t = catalog_.find(id);
        if (!t) continue;
        // While a trial runs, the live copy of the tweak is lifted and the trial owns it.
        restore(id);
        if (on) apply(*t, "trial." + id); else restore("trial." + id);
    }
}

void Tuner::drive_trial() {
    if (!trial_.running()) return;
    if (!world_ready_) {   // the world went away mid-measurement: give up cleanly
        trial_.cancel();
        for (const auto& k : std::vector<std::string>{"probe"}) applied_.erase(k);   // the world's variables are gone with it
        std::vector<std::string> keys; for (const auto& [k, _] : applied_) if (k.starts_with("trial.")) keys.push_back(k);
        for (const auto& k : keys) applied_.erase(k);
        report("Measurement stopped: the world went away.");
        return;
    }
    switch (trial_.advance(clock_)) {
        case Trial::Step::Switch: switch_trial(trial_.change_on()); break;
        case Trial::Step::Sample: trial_.record(sample(trial_.plan().measure)); break;
        default: break;
    }
    if (trial_.running()) return;
    // Finished: put the live state back and keep the numbers.
    const auto r = trial_.result();
    if (trial_target_ == "probe") {
        restore("probe");
        Json p = probe_result_;
        p["when"] = today(); p["before_ms"] = r.off_median_ms; p["after_ms"] = r.on_median_ms;
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
        report(v == "pixels" ? "The frame scales with the pixels: " + fixed1(r.off_median_ms) + " ms became " + fixed1(r.on_median_ms) + " ms at " + console_number(to) + "% resolution. The resolution and the upscaler are the levers here; the GPU-side tweaks can help a little."
             : v == "fixed" ? "The frame hardly moves with the resolution (" + fixed1(r.off_median_ms) + " to " + fixed1(r.on_median_ms) + " ms): its time is in work that is the same at any resolution, shadows (every shadow-casting light), Lumen's scene lighting, ray tracing, and the CPU and driver side. Resolution tweaks will not show; the Video settings for shadows and lighting, and CPU-side tweaks, are what moves it."
             : v == "mixed" ? "Both: " + fixed1(r.off_median_ms) + " ms became " + fixed1(r.on_median_ms) + " ms at " + console_number(to) + "% resolution, part of the frame scales with the pixels and part does not." : "The probe did not get a clean reading; try again standing still.");
        try { host_.invalidate(); } catch (...) {}
        return;
    }
    switch_trial(false);
    Json j = r.json(); j["when"] = today();
    results_[trial_target_] = j;
    save_state();
    apply_enabled();   // the live copies come back
    if (trial_target_ == "fix.anim" && enable_ && anim_fix_) apply_anim_fix();
    if (tuning_) { tune_decide(trial_target_, r); return; }
    const std::string what = trial_target_ == "all" ? "The tweaks together" : trial_target_ == "fix.anim" ? std::string("'Animate only what is seen'") : "'" + catalog_.find(trial_target_)->title + "'";
    report(r.verdict == "gain" ? what + ": " + percent(r.gain_pct) + " frame time (" + fixed1(r.off_median_ms) + " to " + fixed1(r.on_median_ms) + " ms), consistent over both rounds."
         : r.verdict == "loss" ? what + ": slower by " + percent(-r.gain_pct) + " (" + fixed1(r.off_median_ms) + " to " + fixed1(r.on_median_ms) + " ms). Turn it off."
         : r.verdict == "same" ? what + ": no measurable change (" + fixed1(r.off_median_ms) + " vs " + fixed1(r.on_median_ms) + " ms)."
         : what + ": the two rounds disagreed (" + percent(r.gain_pct) + "); the scene moved more than the tweak did. Try again standing still.");
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
    tune_queue_.push_back("probe");
    // Switches that start off are the ones to decide by measuring; the always-on group is
    // kept (hitch spacing and resident textures do not show in a 15 s window).
    for (const auto& t : catalog_.tweaks) if (!t.on && fit_reason(t).empty()) tune_queue_.push_back(t.id);
    tune_queue_.push_back("fix.anim");   // skipped after the audit when there is nothing to fix
    tune_queue_.push_back("all");
    tune_total_ = tune_queue_.size();
    tuning_ = true;
    report("Tuning: audit, probe, then every switch that fits this machine, off and on twice each. About " + std::to_string((tune_total_ - 2) + 1) + " minutes; keep playing or stand still.");
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
        if (next == "all" && active_ids().empty()) { tune_log_.push_back({{"target", next}, {"title", "The kept set"}, {"verdict", "skipped"}, {"gain_pct", 0}, {"kept", false}}); return; }
        start_trial(next);
    } catch (const std::exception& e) { finish_tune(e.what()); }
}

void Tuner::tune_decide(const std::string& target, const TrialResult& r) {
    std::string title = target == "probe" ? "Where the time goes" : target == "all" ? "The kept set together" : target == "fix.anim" ? "Animate only what is seen" : catalog_.find(target) ? catalog_.find(target)->title : target;
    bool kept = false;
    if (target == "probe") { tune_log_.push_back({{"target", target}, {"title", title}, {"verdict", probe_result_.value("verdict", std::string())}, {"gain_pct", 0}, {"kept", false}}); return; }
    if (target == "all") { tune_log_.push_back({{"target", target}, {"title", title}, {"verdict", r.verdict}, {"gain_pct", r.gain_pct}, {"kept", true}}); return; }
    kept = r.verdict == "gain";
    if (target == "fix.anim") {
        anim_fix_ = kept;
        if (kept) apply_anim_fix(); else restore_anim_fix();
    } else {
        choices_[target] = kept;
        if (kept) { if (const auto* t = catalog_.find(target)) apply(*t, target); } else restore(target);
    }
    save_state();
    tune_log_.push_back({{"target", target}, {"title", title}, {"verdict", r.verdict}, {"gain_pct", r.gain_pct}, {"kept", kept}});
}

void Tuner::finish_tune(const std::string& why) {
    tuning_ = false; tune_queue_.clear();
    if (!why.empty()) { report("Tuning stopped: " + why + ". What was decided so far stays."); return; }
    std::string kept, dropped; double whole = 0; std::string whole_verdict;
    for (const auto& e : tune_log_) {
        const auto t = e.value("target", std::string()), v = e.value("verdict", std::string());
        if (t == "probe") continue;
        if (t == "all") { whole = e.value("gain_pct", 0.0); whole_verdict = v; continue; }
        if (v == "skipped") continue;
        if (e.value("kept", false)) kept += (kept.empty() ? "" : ", ") + e.value("title", std::string()) + " (" + percent(e.value("gain_pct", 0.0)) + ")";
        else dropped += (dropped.empty() ? "" : ", ") + e.value("title", std::string());
    }
    tune_result_ = {{"when", today()}, {"kept", kept}, {"dropped", dropped}, {"whole_gain_pct", whole}, {"whole_verdict", whole_verdict}, {"log", tune_log_}, {"probe", probe_result_.value("verdict", std::string())}};
    save_state();
    std::string text = "Tuned. ";
    text += kept.empty() ? "No switch measured a gain on this machine; the always-on set stays. " : "Kept: " + kept + ". ";
    if (!dropped.empty()) text += "Off (no gain here): " + dropped + ". ";
    if (!whole_verdict.empty() && whole_verdict != "unclear") text += "The whole set together: " + (whole_verdict == "same" ? std::string("no measurable change") : percent(whole) + " frame time") + ".";
    report(text);
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
    report("World audited: " + std::to_string(a.actors) + " actors, " + std::to_string(a.ticking) + " ticking, " + std::to_string(a.skeletal_always) + " of " + std::to_string(a.skeletal) + " skeletal meshes animate off screen, "
        + std::to_string(a.lights_shadow) + " shadow lights, " + std::to_string(a.niagara_active) + " live particle systems. Report: world-audit-report.txt.");
}

void Tuner::apply_anim_fix() {
    if (!world_ready_ || audit_.running()) return;
    anim_fixed_ = audit_.apply_anim_fix(player_);
}

void Tuner::restore_anim_fix() {
    audit_.restore_anim_fix();
    anim_fixed_ = 0;
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
        add("machine", "This machine", (machine_.wine ? "Proton (Wine " + machine_.wine_version + "), " : std::string("Windows, ")) + card + (machine_.steam_deck ? ", Steam Deck" : "") + ". Tweaks that do not fit are shown greyed out with the reason.", "info");
    }
    if (machine_.wine) {
        add("proton", "Proton", "DirectX 12 goes through VKD3D-Proton, so ray tracing, mesh shaders and async compute are translated, not native. Measure rather than assume: a thing that is faster on Windows can be slower here.", "info");
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
            + std::to_string(dead) + " do nothing in this engine, " + std::to_string(picture) + " change the look, " + std::to_string(managed) + " are taken over by the tuner while it is on. The full list is in engine-ini-report.txt under Mods/CSSX/output/eins0fx.perf-tuner. The file itself is left alone.";
        add("ini", "Engine.ini", text, picture > 0 ? "warning" : "info");
    } else if (!ini_path_.empty()) add("ini", "Engine.ini", "Your Engine.ini has no console variable lines; nothing from the web is set there.", "info");
    if (world_ready_) {
        auto num = [&](const char* name) { return read_cvar(name); };
        if (const auto v = num("r.GPUCrashDebugging"); v && *v > 0)
            add("gpucrash", "GPU crash debugging", "On. The driver keeps extra breadcrumbs every frame for crash reports; it costs frames. Turn it off in Video settings unless you are chasing a crash.", "warning");
        if (const auto v = num("r.Streamline.DLSSG.Enable"); v && *v > 0) {
            const auto gpu = env_value("");   // no reliable GPU name through the console; the player knows the card
            (void)gpu;
            add("dlssg", "Frame generation", "DLSS frame generation is on in your config. It needs an RTX 40 or newer; on an older card the game loads the feature for nothing. If the Video settings show it as unsupported, set Frame Generation to Off there.", "warning");
        }
        if (const auto v = num("t.Streamline.Reflex.Enable"); v && *v > 0)
            add("reflex", "Reflex", "On. Reflex trims input lag by not letting the GPU queue up frames; that can cost a few frames per second. Measure it from here before deciding.", "info");
        if (const auto v = num("r.RayTracing.Enable"); v && *v > 0)
            add("hwrt", "Hardware ray tracing", "On: Lumen traces against the real geometry instead of its distance fields. Not automatically the slower choice: this game turned mesh distance fields off for software tracing, and on an RTX 3060 under Proton hardware tracing measured faster (28.6 vs 31 ms a frame, 6 Oct 2026). It is a picture choice, so the tuner leaves it to you; the Video settings have the switch.", "info");
        if (const auto v = num("r.Streaming.PoolSize"); v && *v > 0 && *v < 1500 && !applied_.contains("texture_pool"))
            add("pool", "Texture pool", "The texture streaming pool is " + console_number(*v) + " MB. On a card with more memory a bigger pool keeps textures resident instead of streaming them back and forth; the 'Texture pool' tweak raises it.", "info");
        if (const auto pages = num("r.Shadow.Virtual.MaxPhysicalPages"); pages && *pages > 0 && *pages < 2048 && audit_summary_.lights_shadow >= 12)
            add("vsm", "Shadow page pool", "The shadow page pool is " + console_number(*pages) + " pages and the audited area has " + std::to_string(audit_summary_.lights_shadow) + " shadow-casting lights. When the pool runs out, shadows go missing or flicker and the engine redraws pages every frame. Shadows at Ultra in the Video settings raises the pool to 2048 (about 256 MB of video memory); the tuner does not resize it itself, a live resize once crashed this GPU.", "info");
        if (const auto v = num("r.VSync"); v && *v > 0) add("vsync", "V-sync", "On. The frame rate is tied to the display; fine for tearing, but it hides any gain above the refresh rate.", "info");
        if (const auto sp = num("r.ScreenPercentage"); sp && *sp > 0 && *sp < 100)
            add("sp", "Render resolution", "The game renders at " + fixed1(*sp) + "% and the upscaler fills the rest. The tuner never changes this.", "info");
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
    std::string line = verdict == "gain" ? "Measured " + percent(gain) + " frame time (" + fixed1(off) + " to " + fixed1(on) + " ms)"
                     : verdict == "loss" ? "Measured slower by " + percent(-gain) + " (" + fixed1(off) + " to " + fixed1(on) + " ms)"
                     : verdict == "same" ? "Measured: no change (" + fixed1(off) + " vs " + fixed1(on) + " ms)"
                     : "Measured: unclear (" + percent(gain) + ", rounds disagreed)";
    return when.empty() ? line : line + ", " + when;
}

Json Tuner::sections() {
    Json out = Json::array();
    const bool busy = trial_.running();
    // Tuner
    {
        Json controls = Json::array();
        controls.push_back({{"id", "enable"}, {"type", "toggle"}, {"label", "Perf Tuner"}, {"value", enable_}, {"enabled", !busy && catalog_problem_.empty()},
            {"description", "Apply the tweaks below. Each one is set through the game's console, read first and put back when the tuner is off or unloaded. The picture stays as your Video settings have it: the tuner never touches resolution, quality levels, the upscaler or the frame cap."}});
        std::string tuned = "Not tuned yet";
        if (tune_result_.is_object() && tune_result_.contains("when")) {
            const auto kept = tune_result_.value("kept", std::string());
            tuned = (kept.empty() ? std::string("Nothing measured a gain; the always-on set stays") : "Kept " + kept) + ", " + tune_result_.value("when", std::string());
        }
        controls.push_back({{"id", "tune"}, {"type", "button"}, {"label", tuning_ ? "Tuning" : "Tune"}, {"value", tuned}, {"busy", tuning_}, {"enabled", !busy && !tuning_ && !audit_.running() && world_ready_ && catalog_problem_.empty()},
            {"effect", "reversible"}, {"description", "The one button. Turns the tuner on, audits the area, finds where the frame time goes, then measures every switch that fits this machine off and on twice each and keeps the ones that win; the always-on set stays on. Ends by measuring the kept set as a whole. About eight minutes on a full catalog; keep playing or stand still. Everything it keeps is still a switch below, and Perf Tuner off puts it all back."}});
        std::string fps = "Load into the world to read it";
        if (brief_.is_object() && brief_.value("frames", 0) > 0) {
            fps = fixed1(brief_.value("hz", 0.0)) + " fps, " + fixed1(brief_.value("median_ms", 0.0)) + " ms a frame, slowest 1%: " + fixed1(brief_.value("p99_ms", 0.0)) + " ms";
        }
        controls.push_back({{"id", "fps"}, {"type", "label"}, {"label", "Frame rate"}, {"value", fps}, {"description", "The last five seconds, from the loader's own frame clock."}});
        std::string probe = "Not measured yet";
        if (probe_result_.is_object() && probe_result_.contains("verdict")) {
            const auto v = probe_result_.value("verdict", std::string());
            probe = (v == "pixels" ? "Scales with pixels" : v == "fixed" ? "Does not scale with pixels" : v == "mixed" ? "Partly" : "Unclear") + std::string(": ") + fixed1(probe_result_.value("before_ms", 0.0)) + " ms became " + fixed1(probe_result_.value("after_ms", 0.0)) + " ms at " + console_number(probe_result_.value("to", 100.0)) + "%, " + probe_result_.value("when", std::string());
        }
        controls.push_back({{"id", "probe"}, {"type", "button"}, {"label", "Where does the time go?"}, {"value", probe}, {"busy", busy && trial_target_ == "probe"}, {"enabled", !busy && world_ready_},
            {"effect", "reversible"}, {"description", "Sixteen seconds: five at your resolution, then five at native 100% (or 50% if you already run native), then back. If the frame gets much slower with more pixels, resolution and the upscaler are your levers. If it hardly moves, the time is in work that is the same at any resolution: shadows from every shadow-casting light, Lumen's scene lighting, ray tracing, and the CPU and driver side; then the shadow and lighting settings in the Video menu are what moves it, not resolution."}});
        Json options = Json::array({{{"id", "all"}, {"label", "All tweaks that are on"}}});
        for (const auto& t : catalog_.tweaks) options.push_back({{"id", t.id}, {"label", t.title}});
        options.push_back({{"id", "fix.anim"}, {"label", "Animate only what is seen (world fix)"}});
        controls.push_back({{"id", "measure_target"}, {"type", "choice"}, {"label", "Measure"}, {"value", measure_target_}, {"options", options}, {"enabled", !busy},
            {"description", "What the next measurement switches off and on."}});
        controls.push_back({{"id", "measure"}, {"type", "button"}, {"label", busy && trial_target_ != "probe" ? "Measuring" : "Measure it"}, {"value", result_line(measure_target_)},
            {"busy", busy && trial_target_ != "probe"}, {"enabled", !busy && world_ready_ && catalog_problem_.empty()}, {"effect", "reversible"},
            {"description", "About a minute: off, on, off, on, fifteen seconds each (the first five of every window are not counted while the lighting and shadow caches settle). Same scene, back to back, so the scene's own drift cancels out; the second round shows whether the first was luck. A gain counts when both on windows beat both off windows by more than 1.5%."}});
        out.push_back({{"id", "tuner"}, {"title", "Tuner"}, {"controls", controls}});
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
        std::string summary = a.actors ? std::to_string(a.actors) + " actors, " + std::to_string(a.ticking) + " ticking; " + std::to_string(a.skeletal_always) + " of " + std::to_string(a.skeletal) + " skeletal meshes animate off screen ("
            + std::to_string(a.skeletal_always_unseen) + " were unseen); " + std::to_string(a.lights_shadow) + " shadow lights (" + std::to_string(a.lights_shadow_movable) + " movable); " + std::to_string(a.niagara_active) + " of " + std::to_string(a.niagara) + " particle systems live. " + a.when
            : "Not audited yet";
        controls.push_back({{"id", "audit"}, {"type", "button"}, {"label", audit_.running() ? "Auditing, " + std::to_string(int(std::round(audit_.progress() * 100))) + "%" : "Audit this world"}, {"value", summary}, {"busy", audit_.running()}, {"enabled", !busy && !audit_.running() && world_ready_},
            {"effect", "reversible"}, {"description", "Walks every actor in the loaded level in small batches (a few seconds, no hitch) and counts what costs time without showing: actors ticking every frame, skeletal meshes animating while nobody sees them, shadow-casting lights, live particle systems. Nothing is changed. The full breakdown by class is written to world-audit-report.txt under Mods/CSSX/output/eins0fx.perf-tuner."}});
        std::string hint = result_line("fix.anim");
        if (anim_fixed_) hint += " (" + std::to_string(anim_fixed_) + " meshes live)";
        controls.push_back({{"id", "fix.anim"}, {"type", "toggle"}, {"label", "Animate only what is seen"}, {"value", anim_fix_}, {"enabled", !busy}, {"hint", hint}, {"unit", "CPU"},
            {"description", "Skeletal meshes the game set to animate fully even when off screen are set to the engine's own default: animate when seen, and keep montages (attacks, reactions, hit stops) running when not. Your own character is never touched. Reversible; re-applied after a fresh audit of each new area. Measure it before trusting it: on a GPU-bound scene it will read 'no change'."}});
        out.push_back({{"id", "world"}, {"title", "World"}, {"controls", controls}});
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
        out.push_back({{"id", "ini"}, {"title", "Your Engine.ini"}, {"description", (ini_pack_.empty() ? std::string("Lines found in ") : ini_pack_ + " found in ") + ini_path_ + ". Shown here: the lines that change the look and the ones the tuner takes over. Everything, including the lines that do nothing in this engine, is in engine-ini-report.txt under Mods/CSSX/output/eins0fx.perf-tuner."}, {"controls", controls}});
    }
    // The check
    {
        Json controls = Json::array();
        for (const auto& f : findings_) controls.push_back({{"id", "find." + f.id}, {"type", "label"}, {"label", f.title}, {"value", f.text}, {"severity", f.severity}});
        if (controls.empty()) controls.push_back({{"id", "find.none"}, {"type", "label"}, {"label", "Settings"}, {"value", "Nothing to point out."}});
        out.push_back({{"id", "check"}, {"title", "Check"}, {"description", "Settings in your own config and launch that cost frames for nothing on this machine, or that the tuner wants you to know about. Nothing here is changed by the tuner."}, {"controls", controls}});
    }
    return out;
}

Json Tuner::model() {
    refresh_brief();
    refresh_findings();
    Json status = status_;
    if (audit_.running()) status = "Auditing the world, " + std::to_string(int(std::round(audit_.progress() * 100))) + "%";
    if (trial_.running()) {
        const int pct = int(std::round(trial_.progress(clock_) * 100));
        const std::string what = trial_target_ == "probe" ? "the probe" : trial_target_ == "all" ? "the kept set" : trial_target_ == "fix.anim" ? "'Animate only what is seen'" : catalog_.find(trial_target_) ? "'" + catalog_.find(trial_target_)->title + "'" : trial_target_;
        status = (trial_target_ == "probe" ? "Probing" : "Measuring " + what) + std::string(", ") + std::to_string(pct) + "%: " + (trial_target_ == "probe" ? (trial_.change_on() ? "more pixels" : "your resolution") : (trial_.change_on() ? "on" : "off"));
    }
    if (tuning_) status = "Tuning, step " + std::to_string(tune_total_ - tune_queue_.size()) + " of " + std::to_string(tune_total_) + ". " + status.get<std::string>();
    return {{"schema", 2}, {"sections", sections()}, {"status", status}, {"error", error_}};
}

void Tuner::event(const Json& event) {
    error_.clear();
    try {
        const auto id = event.at("id").get<std::string>();
        if (id == "enable") {
            if (tuning_ && event.at("value").get<bool>() == false) { tuning_ = false; tune_queue_.clear(); trial_.cancel(); switch_trial(false); audit_.cancel(); }
            else if (trial_.running()) throw std::runtime_error("Wait for the measurement to finish.");
            if (!catalog_problem_.empty()) throw std::runtime_error("Fix tweaks.json first: " + catalog_problem_);
            enable_ = event.at("value").get<bool>();
            if (enable_) { refresh_player(); apply_enabled(); if (anim_fix_) { if (audit_summary_.actors > 0 && !audit_.running()) apply_anim_fix(); else if (world_ready_ && !audit_.running()) { try { audit_.start(player_); } catch (...) {} } } }
            else { restore_all(); restore_anim_fix(); audit_.cancel(); }
            save_state();
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
        if (was && !world_ready_) { audit_.cancel(); anim_fixed_ = 0; }
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
    if (trial_.running()) { trial_.cancel(); try { switch_trial(false); } catch (...) {} }
    audit_.cancel();
    try { refresh_player(); restore_all(); restore_anim_fix(); } catch (...) {}
    return true;
}
}

#define CCS_BUILD_CORE
#include "core.hpp"
#include "common.hpp"
#include "ccs_version.hpp"
#include <windows.h>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace ccs {
static std::unique_ptr<Core> g_core;
namespace {
const char* slot_titles[] = {"L1  Light attack 1", "L2  Light attack 2", "L3  Light attack 3", "LF  Light finisher", "LC  Light charged (hold)",
                             "H1  Heavy attack 1", "H2  Heavy attack 2", "H3  Heavy attack 3", "HF  Heavy finisher", "HC  Heavy charged (hold)"};
std::string pretty(std::string text) {
    for (const char* prefix : {"Attack_", "Player_"}) if (text.starts_with(prefix)) text.erase(0, std::strlen(prefix));
    std::replace(text.begin(), text.end(), '_', ' ');
    return text;
}
std::string weapon_icon(const std::string& source) {
    static const std::pair<const char*, const char*> icons[] = {
        {"HadernSword", "HadernSword"}, {"HadernsSword", "HadernSword"}, {"AxeDagger", "AxeDagger"}, {"BattleAxe", "BattleAxe"}, {"MartyrsBlade", "MartyrsBlade"},
        {"HeavyHammer", "HeavyHammer"}, {"Axatana", "Axatana"}, {"BlackNeedle", "BlackNeedle"}, {"Scythe", "ClockworkScythe"}, {"ClockworkScythe", "ClockworkScythe"}};
    for (const auto& [key, name] : icons) if (source == key) return std::string("/Game/Sparta/UI/Icons/Weapons/T_UI_Icon_") + name + ".T_UI_Icon_" + name;
    return {};
}
std::string weapon_name(const std::string& source) {
    static const std::pair<const char*, const char*> names[] = {
        {"HadernSword", "The Iconoclast"}, {"HadernsSword", "The Iconoclast"}, {"AxeDagger", "Axe & Dagger"}, {"BattleAxe", "Veteran's Battle Axe"},
        {"MartyrsBlade", "Great Martyr's Blade"}, {"HeavyHammer", "Obsidian Hammer"}, {"Axatana", "Axatana"}, {"BlackNeedle", "Black Needle"},
        {"Scythe", "Clockwork Scythe"}, {"ClockworkScythe", "Clockwork Scythe"}, {"Combos", "Smert's fists"}};
    for (const auto& [key, name] : names) if (source == key) return name;
    return source.empty() ? "Other" : source;
}
}

Core::Core(const CcsLoaderContext* loader) {
    if (!loader || loader->abi_version != CCS_ABI_VERSION || loader->size < sizeof(CcsLoaderContext) || !loader->mod_root)
        throw std::runtime_error("CCS loader context is incompatible");
    root_dir_ = loader->mod_root;
    if (loader->hooks && loader->hooks->version == CCS_HOOK_ABI_VERSION && loader->hooks->size >= sizeof(CcsHookHost) &&
        loader->hooks->add_native_pre && loader->hooks->remove && loader->hooks->statistics && loader->hooks->on_game_thread) hooks_ = loader->hooks;
    writer_ = std::make_unique<runtime::Writer>(root_dir_ / "logs/ccs.jsonl");
    auto log = [this](const std::string& message) { if (writer_) writer_->write(nlohmann::json{{"level", "info"}, {"msg", message}}.dump()); };
#ifdef CCS_FRAME_PROFILE
    timing_ = std::make_unique<runtime::FrameProfile>(root_dir_ / "runtime/timing", "core");
#endif
#if defined(CCS_DISCOVERY_PROBE)
    probe_ = std::make_unique<DiscoveryProbe>(root_dir_ / "logs/discovery.jsonl");
#elif defined(CCS_ATTACK_PROBE)
    attack_probe_ = std::make_unique<AttackProbe>(hooks_, root_dir_ / "logs/attack-calls.jsonl");
#elif defined(CCS_SWAP_PROBE)
    swap_probe_ = std::make_unique<SwapProbe>(hooks_, root_dir_);
#else
    settings_ = std::make_unique<runtime::Settings>(root_dir_ / "settings.json");
    if (!settings_->load()) log("Settings rejected; using defaults");
    storage_ = std::make_unique<runtime::Storage>(root_dir_ / "presets");
    if (!catalog_.load(root_dir_ / "catalog.json")) { catalog_error_ = catalog_.error(); log("Catalog unavailable: " + catalog_error_); }
    // Move options once: id, label and weapon group, sorted by weapon then name.
    std::vector<const MoveDefinition*> moves;
    for (const auto& move : catalog_.moves()) moves.push_back(&move);
    std::sort(moves.begin(), moves.end(), [](const MoveDefinition* a, const MoveDefinition* b) {
        return std::tie(a->source_name, a->display_name) < std::tie(b->source_name, b->display_name); });
    options_ = nlohmann::json::array();
    options_.push_back({{"id", ""}, {"label", "Weapon's own attack"}, {"group", "Vanilla"}});
    for (const auto* move : moves) options_.push_back({{"id", move->id}, {"label", pretty(move->display_name)}, {"group", weapon_name(move->source_name)}, {"icon", weapon_icon(move->source_name)}});
    if (hooks_) {
        combat_ = std::make_unique<Combat>(Combat::Deps{hooks_, &catalog_, log});
        combat_->set_rate(settings_->attack_speed_scale());
        combat_->set_enabled(settings_->enabled());
        apply_slots_from_settings();
    } else log("Loader offers no native hook host; combat disabled");
    Menu::Deps deps;
    deps.log = log;
    deps.model = [this] { return model(); };
    deps.event = [this](const nlohmann::json& event) { handle_event(event); };
    deps.ui_scale = [this] { return settings_->ui_scale(); };
    deps.version = CCS_VERSION;
    deps.root = root_dir_;
    auto present = [&](const char* name) {
        const auto mod = root_dir_.parent_path() / name;
        std::error_code ec;
        return std::filesystem::exists(mod / "enabled.txt", ec) && std::filesystem::exists(mod / "dlls/main.dll", ec);
    };
    deps.css_present = present("CustomShellSystem");
    deps.cssx_present = present("CSSX");
    menu_ = std::make_unique<Menu>(std::move(deps));
    save_name_ = "my-preset";
#endif
    initialized_ = true;
    writer_->write(R"({"level":"info","msg":"CCS core initialized"})");
}
Core::~Core() { shutdown(); }

#ifdef CCS_PRODUCT
void Core::apply_slots_from_settings() {
    if (!combat_) return;
    const auto& slots = settings_->slots();
    for (size_t i = 0; i < slots.size(); ++i) combat_->set_slot(SlotId(i), slots[i]);
}
void Core::save_settings_or_log() {
    if (combat_) { std::array<std::string, 10> slots; for (size_t i = 0; i < slots.size(); ++i) slots[i] = combat_->slot_move(SlotId(i)); settings_->set_slots(std::move(slots)); }
    if (!settings_->save()) last_message_ = "Settings could not be saved";
}
void Core::list_presets(uint64_t now, bool force) {
    if (!force && now < presets_listed_) return;
    presets_listed_ = now + 2000;
    std::vector<std::string> names;
    if (storage_->list_presets(names)) preset_names_ = std::move(names);
    if (selected_preset_.empty() && !preset_names_.empty()) selected_preset_ = preset_names_.front();
    if (!selected_preset_.empty() && std::find(preset_names_.begin(), preset_names_.end(), selected_preset_) == preset_names_.end()) selected_preset_ = preset_names_.empty() ? "" : preset_names_.front();
}
std::string Core::move_label(const std::string& id) const {
    if (id.empty()) return "Weapon's own attack";
    const auto* move = catalog_.find_move(id);
    return move ? pretty(move->display_name) : "Unknown move (" + id + ")";
}
std::string Core::move_icon(const std::string& id) const {
    const auto* move = id.empty() ? nullptr : catalog_.find_move(id);
    return move ? weapon_icon(move->source_name) : std::string{};
}
std::string Core::move_group(const std::string& id) const {
    const auto* move = id.empty() ? nullptr : catalog_.find_move(id);
    return move ? weapon_name(move->source_name) : std::string{};
}
std::string Core::move_description(const std::string& id) const {
    const auto* move = id.empty() ? nullptr : catalog_.find_move(id);
    if (!move) return "The slot plays the equipped weapon's own attack.";
    std::string text = weapon_name(move->source_name) + ", " + pretty(move->display_name) + ".";
    std::string slots; for (unsigned i = 0; i < 10; ++i) if (move->compatible_slots & (1u << i)) slots += (slots.empty() ? "" : ", ") + std::string(slot_to_string(SlotId(i)));
    if (!slots.empty()) text += " Originally the weapon's " + slots + " attack.";
    const auto pos = move->montage_path.rfind('.');
    text += " Montage " + (pos == std::string::npos ? move->montage_path : move->montage_path.substr(pos + 1)) + ".";
    return text;
}
nlohmann::json Core::model() const {
    using Json = nlohmann::json;
    Json sections = Json::array();
    // ---- Customize
    Json customize = Json::array();
    for (unsigned i = 0; i < 10; ++i) {
        const auto slot = SlotId(i);
        const auto id = combat_ ? combat_->slot_move(slot) : std::string{};
        Json control = {{"type", "choice"}, {"id", std::string("slot.") + slot_to_string(slot)}, {"label", slot_titles[i]},
            {"group", i < 5 ? "Light chain" : "Heavy chain"}, {"value", id}, {"options", options_}, {"value_label", "Move"},
            {"title", move_label(id)}, {"subtitle", move_group(id)}, {"description", move_description(id)}, {"icon", move_icon(id)}, {"enabled", combat_ != nullptr && catalog_error_.empty()},
            {"disabled_label", catalog_error_.empty() ? "Combat engine unavailable" : "Move catalog missing"}};
        if (combat_) {
            const auto& err = combat_->slot_error(slot);
            if (!err.empty()) control["hint"] = "Not applied: " + err;
            else if (!id.empty() && !combat_->slot_ready(slot)) control["hint"] = "Loading the animation...";
            else if (!id.empty()) control["hint"] = "Applied. Played " + std::to_string(combat_->slot_hits(slot)) + " time(s) this session.";
            control["badge"] = !id.empty() && combat_->slot_ready(slot);
        }
        customize.push_back(std::move(control));
    }
    sections.push_back({{"id", "customize"}, {"title", "Customize"}, {"controls", std::move(customize)},
        {"description", "Pick which animation each attack of the chain plays. Any weapon's move fits any slot; the weapon in hand keeps its damage and hit timing rules."}});
    // ---- Presets
    Json presets = Json::array();
    Json preset_options = Json::array();
    for (const auto& name : preset_names_) preset_options.push_back({{"id", name}, {"label", name}, {"group", "Mods/CCS/presets"}});
    const bool any = !preset_names_.empty();
    presets.push_back({{"type", "choice"}, {"id", "preset.selected"}, {"label", "Preset"}, {"value", any ? selected_preset_ : ""},
        {"options", any ? preset_options : Json::array({{{"id", ""}, {"label", "No presets saved"}}})}, {"enabled", any}, {"disabled_label", "No presets yet"},
        {"description", "Presets are JSON files in Mods/CCS/presets. Share them by copying the file."}, {"value_label", "Preset"}});
    presets.push_back({{"type", "button"}, {"id", "preset.load"}, {"label", "Load preset"}, {"action_label", "Load"}, {"enabled", any},
        {"description", "Replace every slot with the selected preset's moves."}, {"disabled_label", "No presets yet"}});
    presets.push_back({{"type", "text"}, {"id", "preset.name"}, {"label", "Preset name"}, {"value", save_name_}, {"action_label", "Save name"},
        {"description", "Name for the next save. Letters, digits, dash and underscore."}});
    presets.push_back({{"type", "button"}, {"id", "preset.save"}, {"label", "Save current slots"}, {"action_label", "Save"},
        {"description", "Write the ten slots to Mods/CCS/presets/<name>.json. An existing file with that name is replaced."},
        {"confirm", "Save the current slots as \"" + save_name_ + "\"?"}, {"effect", "persistent"}});
    presets.push_back({{"type", "button"}, {"id", "preset.delete"}, {"label", "Delete preset"}, {"action_label", "Delete"}, {"enabled", any},
        {"description", "Delete the selected preset file."}, {"confirm", "Delete \"" + selected_preset_ + "\"?"}, {"severity", "danger"}, {"effect", "irreversible"}, {"disabled_label", "No presets yet"}});
    sections.push_back({{"id", "presets"}, {"title", "Presets"}, {"controls", std::move(presets)}});
    // ---- Settings
    Json settings = Json::array();
    settings.push_back({{"type", "toggle"}, {"id", "enabled"}, {"label", "Custom Combat System"}, {"value", settings_->enabled()},
        {"description", "Master switch. Off restores every attack to the weapon's own animation at once; nothing of the game is changed on disk."}, {"enabled", combat_ != nullptr}});
    settings.push_back({{"type", "slider"}, {"id", "rate"}, {"label", "Attack speed"}, {"value", settings_->attack_speed_scale()}, {"min", 0.5}, {"max", 2.0}, {"step", 0.05}, {"unit", "x"},
        {"description", "Play-rate multiplier applied to every attack of the chain, swapped or not. 1x is the game's speed."}, {"enabled", combat_ != nullptr}});
    settings.push_back({{"type", "slider"}, {"id", "ui_scale"}, {"label", "Menu scale"}, {"value", settings_->ui_scale()}, {"min", 0.75}, {"max", 1.5}, {"step", 0.05}, {"unit", "x"},
        {"description", "Size of this page relative to the game's menus."}});
    settings.push_back({{"type", "button"}, {"id", "reset"}, {"label", "Reset all slots"}, {"action_label", "Reset"}, {"enabled", combat_ != nullptr && combat_->assigned() > 0},
        {"description", "Every slot back to the weapon's own attack. Presets on disk are kept."}, {"confirm", "Clear all ten slots?"}, {"disabled_label", "Nothing assigned"}});
    Json lines = Json::array();
    if (combat_) {
        const auto status = combat_->status();
        lines.push_back(std::string("Hook: ") + (status.value("hooked", false) ? "installed" : "not installed") + ", " + std::to_string(status.value("swapped", 0ull)) + " swaps, " + std::to_string(status.value("seen", 0ull)) + " attacks seen.");
        lines.push_back("Worst callback " + std::to_string(status.value("maximum_callback_us", 0ull)) + " us. Failures " + std::to_string(status.value("failures", 0ull)) + ".");
        if (!combat_->error().empty()) lines.push_back("Error: " + combat_->error());
    }
    if (!catalog_error_.empty()) lines.push_back("Catalog: " + catalog_error_);
    settings.push_back({{"type", "label"}, {"id", "about"}, {"label", "About"}, {"state", std::string("CCS ") + CCS_VERSION},
        {"description", "Custom Combat System by eins0fx. The engine rewrites the animation an attack plays at the moment the game starts it; damage, hit windows and Resolve stay the weapon's own."},
        {"lines", std::move(lines)}});
    sections.push_back({{"id", "settings"}, {"title", "Settings"}, {"controls", std::move(settings)}});
    std::string status;
    if (!last_message_.empty()) status = last_message_;
    else if (!combat_) status = "Combat engine unavailable";
    else if (!settings_->enabled()) status = "Disabled";
    else status = std::to_string(combat_->assigned()) + " slot(s) assigned" + (combat_->hooked() ? ", engine active" : "");
    return {{"sections", std::move(sections)}, {"status", status}};
}
void Core::handle_event(const nlohmann::json& event) {
    const auto id = event.at("id").get<std::string>();
    last_message_.clear();
    if (id.starts_with("slot.")) {
        const auto slot = string_to_slot(id.substr(5));
        if (!slot || !combat_) throw std::runtime_error("Unknown slot");
        combat_->set_slot(*slot, event.at("value").get<std::string>());
        save_settings_or_log(); return;
    }
    if (id == "enabled") { settings_->set_enabled(event.at("value").get<bool>()); if (combat_) combat_->set_enabled(settings_->enabled()); save_settings_or_log(); return; }
    if (id == "rate") { settings_->set_attack_speed_scale(event.at("value").get<double>()); if (combat_) combat_->set_rate(settings_->attack_speed_scale()); save_settings_or_log(); return; }
    if (id == "ui_scale") { settings_->set_ui_scale(event.at("value").get<double>()); save_settings_or_log(); return; }
    if (id == "reset") { if (combat_) for (unsigned i = 0; i < 10; ++i) combat_->set_slot(SlotId(i), ""); save_settings_or_log(); return; }
    if (id == "preset.selected") { selected_preset_ = event.at("value").get<std::string>(); return; }
    if (id == "preset.name") {
        const auto name = event.at("value").get<std::string>();
        if (!runtime::valid_preset_name(name)) throw std::runtime_error("Use letters, digits, dash or underscore, up to 64 characters");
        save_name_ = name; return;
    }
    if (id == "preset.load") {
        auto preset = storage_->load_preset(selected_preset_);
        if (!preset) throw std::runtime_error("Preset could not be read: " + selected_preset_);
        if (combat_) for (const auto& binding : preset->slots) combat_->set_slot(binding.slot, binding.move_id);
        save_name_ = preset->name; save_settings_or_log(); last_message_ = "Loaded " + selected_preset_; return;
    }
    if (id == "preset.save") {
        if (!runtime::valid_preset_name(save_name_)) throw std::runtime_error("Invalid preset name");
        PresetData preset; preset.name = save_name_; preset.author = "eins0fx"; preset.description = "Custom Combat System preset";
        for (unsigned i = 0; i < 10; ++i) {
            auto& binding = preset.slots[i]; binding.slot = SlotId(i);
            binding.move_id = combat_ ? combat_->slot_move(SlotId(i)) : std::string{};
            if (const auto* move = binding.move_id.empty() ? nullptr : catalog_.find_move(binding.move_id)) { binding.montage_path = move->montage_path; binding.ability_path = move->ability_path; binding.source = move->source_name; binding.origin = move->origin; }
        }
        if (storage_->save_preset(preset, true) != runtime::FileWriteResult::Success) throw std::runtime_error("Preset could not be written");
        list_presets(GetTickCount64(), true); selected_preset_ = save_name_; last_message_ = "Saved " + save_name_; return;
    }
    if (id == "preset.delete") {
        if (!storage_->delete_preset(selected_preset_)) throw std::runtime_error("Preset could not be deleted");
        last_message_ = "Deleted " + selected_preset_; selected_preset_.clear(); list_presets(GetTickCount64(), true); return;
    }
    throw std::runtime_error("Unknown control: " + id);
}
#endif

void Core::tick(const CcsPlayerContext* player, double delta) {
    if (!initialized_) return;
    if (error_reported_) {
        const auto now = runtime::now_ms();
        if (fault_cleanup_complete_ || now < fault_cleanup_after_) return;
        fault_cleanup_after_ = now + 250;
        try { fault_cleanup_complete_ = stop(); } catch (...) {}
        return;
    }
#ifdef CCS_FRAME_PROFILE
    runtime::FrameProfile::Scope frame(*timing_, delta);
#endif
    (void)delta;
#if defined(CCS_DISCOVERY_PROBE)
    probe_->tick(player ? player->engine : nullptr);
#elif defined(CCS_ATTACK_PROBE)
    attack_probe_->tick(player ? player->engine : nullptr);
#elif defined(CCS_SWAP_PROBE)
    swap_probe_->tick(player ? player->engine : nullptr);
#else
    const auto context = engine::player_context(player ? player->engine : nullptr);
    const auto now = GetTickCount64();
    if (combat_) combat_->tick(context, now);
    if (menu_ && menu_->is_open()) list_presets(now, false);
    if (menu_) menu_->tick(context, delta);
#endif
}
void Core::on_hotkey(uint32_t key) {
    if (error_reported_) return;
#ifdef CCS_FRAME_PROFILE
    if (key == VK_F7) timing_->arm(runtime::FrameProfile::now_ns());
#endif
#if defined(CCS_DISCOVERY_PROBE)
    if (key == VK_F7) probe_->toggle();
#elif defined(CCS_ATTACK_PROBE)
    if (key == VK_F7) attack_probe_->toggle();
#elif defined(CCS_SWAP_PROBE)
    if (key == VK_F7) swap_probe_->toggle();
#else
    (void)key;
#endif
}
bool Core::stop() {
    if (hooks_ && !hooks_->on_game_thread(hooks_->context)) return false;
#if defined(CCS_DISCOVERY_PROBE)
    probe_->cancel();
#elif defined(CCS_ATTACK_PROBE)
    if (!attack_probe_->stop()) return false;
#elif defined(CCS_SWAP_PROBE)
    if (!swap_probe_->stop()) return false;
#else
    if (menu_) menu_->detach();
    if (combat_ && !combat_->stop()) return false;
#endif
    if (hooks_) {
        CcsHookStats stats{}; stats.size = sizeof(stats);
        if (!hooks_->statistics(hooks_->context, &stats) || stats.slots || stats.running) return false;
    }
    return true;
}
void Core::report_error(const char* message) noexcept {
    if (error_reported_) return;
    error_reported_ = true;
    try { if (writer_) writer_->write(nlohmann::json{{"level", "error"}, {"msg", message}}.dump()); } catch (...) {}
}
void Core::shutdown() {
    // Loader destruction can run off the game thread. Release CPU resources only.
    initialized_ = false;
#if defined(CCS_DISCOVERY_PROBE)
    probe_.reset();
#elif defined(CCS_ATTACK_PROBE)
    attack_probe_.reset();
#elif defined(CCS_SWAP_PROBE)
    swap_probe_.reset();
#else
    menu_.reset(); combat_.reset();
#endif
    writer_.reset();
#ifdef CCS_FRAME_PROFILE
    timing_.reset();
#endif
}
const char* Core::get_status_json() {
    nlohmann::json status = {{"status", error_reported_ ? "faulted" : "running"}, {"version", CCS_VERSION}};
    if (hooks_) {
        CcsHookStats stats{}; stats.size = sizeof(stats);
        if (hooks_->statistics(hooks_->context, &stats))
            status["hooks"] = {{"slots", stats.slots}, {"running", stats.running}, {"stopped", stats.stopped != 0}, {"calls", stats.calls}, {"wrong_thread", stats.wrong_thread}, {"failures", stats.failures}};
    }
#if defined(CCS_DISCOVERY_PROBE)
    status["discovery_state"] = probe_ ? probe_->state() : "uninitialized";
#elif defined(CCS_ATTACK_PROBE)
    status["attack_probe"] = attack_probe_->status();
#elif defined(CCS_SWAP_PROBE)
    status["swap_probe"] = swap_probe_->status();
#else
    if (combat_) status["combat"] = combat_->status();
    if (menu_) status["menu"] = menu_->diagnostics();
#endif
    status_json_ = status.dump();
    return status_json_.c_str();
}
}

extern "C" {
static int ccs_core_init(const CcsLoaderContext* loader) noexcept {
    try { if (ccs::g_core) return -1; ccs::g_core = std::make_unique<ccs::Core>(loader); return 0; }
    catch (...) { ccs::g_core.reset(); return -1; }
}
static void ccs_core_tick(const CcsPlayerContext* player, double delta) noexcept {
    try { if (ccs::g_core) ccs::g_core->tick(player, delta); }
    catch (const std::exception& e) { if (ccs::g_core) ccs::g_core->report_error(e.what()); }
    catch (...) { if (ccs::g_core) ccs::g_core->report_error("Unknown CCS tick failure"); }
}
static void ccs_core_on_hotkey(uint32_t key) noexcept {
    try { if (ccs::g_core) ccs::g_core->on_hotkey(key); }
    catch (const std::exception& e) { if (ccs::g_core) ccs::g_core->report_error(e.what()); }
    catch (...) { if (ccs::g_core) ccs::g_core->report_error("Unknown CCS hotkey failure"); }
}
static int ccs_core_stop() noexcept { try { return ccs::g_core ? (ccs::g_core->stop() ? 1 : 0) : 1; } catch (...) { return 0; } }
static void ccs_core_shutdown() noexcept { try { ccs::g_core.reset(); } catch (...) {} }
static const char* ccs_core_status() noexcept {
    try { return ccs::g_core ? ccs::g_core->get_status_json() : R"({"status":"uninitialized"})"; }
    catch (...) { return R"({"status":"error"})"; }
}
static const CcsCoreApi g_api = {CCS_ABI_VERSION, sizeof(CcsCoreApi), ccs_core_init, ccs_core_tick, ccs_core_on_hotkey, ccs_core_stop, ccs_core_shutdown, ccs_core_status};
CCS_CORE_EXPORT const CcsCoreApi* ccs_get_core_api(void) { return &g_api; }
}

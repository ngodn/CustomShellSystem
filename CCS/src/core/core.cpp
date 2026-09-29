#define CCS_BUILD_CORE
#include "core.hpp"
#include "common.hpp"
#include "ccs_version.hpp"
#include <windows.h>
#include <cstring>
#include <cctype>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ccs {
static std::unique_ptr<Core> g_core;
namespace {
const char* slot_titles[] = {"L1  Light attack 1", "L2  Light attack 2", "L3  Light attack 3", "LF  Light finisher", "LC  Light charged (hold)",
                             "H1  Heavy attack 1", "H2  Heavy attack 2", "H3  Heavy attack 3", "HF  Heavy finisher", "HC  Heavy charged (hold)",
                             "R   Ranged (sidearm fire)", "SL  Sprint light attack", "SH  Sprint heavy attack"};
const char* tile_labels[] = {"L1", "L2", "L3", "LF", "LC", "H1", "H2", "H3", "HF", "HC", "R", "S+L", "S+H"};
constexpr unsigned slot_count = 13;
enum class Role { Chain, Finisher, Hold, Ranged, Sprint };
Role slot_role(unsigned i) { return i == 10 ? Role::Ranged : i >= 11 ? Role::Sprint : (i == 3 || i == 8) ? Role::Finisher : (i == 4 || i == 9) ? Role::Hold : Role::Chain; }
// What a move may do in a slot. Chain moves fit any chain position of either chain; finisher
// montages only the finisher slots; hold montages (wind-up plus charged release) only the hold
// slots; sidearm fire only the ranged slot. Enemy melee fits the melee slots that are not holds,
// enemy ranged only the ranged slot, unverified.
bool eligible(const MoveDefinition& move, unsigned i) {
    (void)move; (void)i;
    return true;   // every move in every slot, by the user's call; the rig filter is the only gate
    const auto role = slot_role(i);
    if (move.origin == MoveOrigin::EnemyHumanoid) {
        const bool ranged = move.display_name.find("Shoot") != std::string::npos || move.display_name.find("Crossbow") != std::string::npos ||
                            move.display_name.find("Throw") != std::string::npos || move.display_name.find("Bow ") != std::string::npos;
        return ranged ? role == Role::Ranged : (role == Role::Chain || role == Role::Finisher || role == Role::Sprint);
    }
    if (move.compatible_slots & (1u << unsigned(SlotId::R))) return role == Role::Ranged;
    if (move.category == "Running") return role == Role::Sprint;   // sprint attacks carry their run-in
    if (move.is_hold) return role == Role::Hold;
    if (move.is_finisher) return role == Role::Finisher;
    return role == Role::Chain || role == Role::Sprint;
}
std::string pretty(std::string text) {
    for (const char* prefix : {"Attack_", "Player_"}) if (text.starts_with(prefix)) text.erase(0, std::strlen(prefix));
    std::replace(text.begin(), text.end(), '_', ' ');
    return text;
}
std::string weapon_icon(const std::string& source) {
    static const std::pair<const char*, const char*> icons[] = {
        {"HadernSword", "HadernSword"}, {"HadernsSword", "HadernSword"}, {"AxeDagger", "AxeDagger"}, {"BattleAxe", "BattleAxe"}, {"MartyrsBlade", "MartyrsBlade"},
        {"HeavyHammer", "HeavyHammer"}, {"Axatana", "Axatana"}, {"Katanas", "Axatana"}, {"BlackNeedle", "BlackNeedle"}, {"Scythe", "ClockworkScythe"}, {"ClockworkScythe", "ClockworkScythe"}};
    for (const auto& [key, name] : icons) if (source == key || source.starts_with(std::string(key) + "_")) return std::string("/Game/Sparta/UI/Icons/Weapons/T_UI_Icon_") + name + ".T_UI_Icon_" + name;
    return {};
}
std::string enemy_name(const std::string& source) {
    if (source == "MS1") return "Mortal Shell 1";
    std::string out;
    for (size_t i = 0; i < source.size(); ++i) {
        const char ch = source[i];
        if (i && std::isupper(static_cast<unsigned char>(ch)) && !std::isupper(static_cast<unsigned char>(source[i - 1]))) out += ' ';
        out += ch;
    }
    return out;
}
std::string weapon_name(const std::string& source) {
    static const std::pair<const char*, const char*> names[] = {
        {"HadernSword", "The Iconoclast"}, {"HadernsSword", "The Iconoclast"}, {"AxeDagger", "Axe & Dagger"}, {"BattleAxe", "Veteran's Battle Axe"},
        {"MartyrsBlade", "Great Martyr's Blade"}, {"HeavyHammer", "Obsidian Hammer"}, {"Axatana", "Axatana"}, {"Katanas", "Axatana (katanas)"}, {"BlackNeedle", "Black Needle"},
        {"Scythe", "Clockwork Scythe"}, {"ClockworkScythe", "Clockwork Scythe"}, {"Combos", "Smert's fists"}};
    for (const auto& [key, name] : names) if (source == key || source.starts_with(std::string(key) + "_")) return name;
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
    else {
        if (!catalog_.load_enemy(root_dir_ / "enemy-catalog.json")) log("Enemy catalog unavailable: " + catalog_.error());
        if (!catalog_.load_ranged(root_dir_ / "ranged-catalog.json")) log("Ranged catalog unavailable: " + catalog_.error());
        if (!catalog_.load_running(root_dir_ / "running-catalog.json")) log("Running catalog unavailable: " + catalog_.error());
    }
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
        combat_->set_enabled(settings_->enabled());
        combat_->set_hold_cheat(settings_->charged_without_stone());
        combat_->set_chain(settings_->next_attack());
        combat_->set_enemy_difficulty(settings_->enemy_difficulty());
        apply_slots_from_settings();
    } else log("Loader offers no native hook host; combat disabled");
    Menu::Deps deps;
    deps.log = log;
    deps.model = [this] { return model(); };
    deps.revision = [this] { return model_revision(); };
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
    status_ = std::make_unique<runtime::StatusWriter>(root_dir_ / "runtime/status.json");
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(root_dir_ / "assets/enemy-icons", ec))
            if (entry.is_regular_file(ec) && entry.path().extension() == ".png") enemy_icons_.insert(entry.path().stem().string());
    }
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
    const auto& tuning = settings_->tuning();
    for (size_t i = 0; i < tuning.size(); ++i) combat_->set_tuning(SlotId(i), tuning[i]);
}
void Core::save_settings_or_log() {
    if (combat_) {
        std::array<std::string, 13> slots; for (size_t i = 0; i < slots.size(); ++i) { slots[i] = combat_->slot_move(SlotId(i)); settings_->set_tuning(i, combat_->tuning(SlotId(i))); }
        settings_->set_slots(std::move(slots));
    }
    // The write waits for a quiet spell: a held direction key changes a row eleven times a second,
    // and each save is a synchronous temp-file write and rename on the game thread.
    settings_dirty_ = true; settings_flush_at_ = runtime::now_ms() + 400;
}
void Core::flush_settings() {
    if (!settings_dirty_) return;
    settings_dirty_ = false;
    if (!settings_->save()) { last_message_ = "Settings could not be saved"; ++model_revision_; }
}
// A preset names moves by catalog id; older or hand-written files may only carry the montage
// path, which resolves through the catalogs. Every slot is set (the preset is the whole state),
// unknown moves and moves that do not fit their slot are skipped and reported.
void Core::apply_preset(const std::string& name) {
    auto preset = storage_->load_preset(name);
    if (!preset) throw std::runtime_error("Preset could not be read: " + name);
    if (!combat_) throw std::runtime_error("Combat engine is not running");
    unsigned applied = 0, unknown = 0, misplaced = 0;
    for (unsigned i = 0; i < slot_count && i < preset->slots.size(); ++i) {
        const auto& binding = preset->slots[i];
        std::string id;
        if (slot_role(i) == Role::Ranged && !sidearm_slot_enabled) continue;   // the sidearm slot is parked
        if (!binding.move_id.empty() || !binding.montage_path.empty()) {
            if (catalog_.find_move(binding.move_id) || binding.move_id.starts_with("found:")) id = binding.move_id;
            else if (!binding.montage_path.empty()) for (const auto& move : catalog_.moves()) if (move.montage_path == binding.montage_path) { id = move.id; break; }
            if (id.empty()) ++unknown;
            else if (const auto* move = catalog_.find_move(id); move && !eligible(*move, i)) { ++misplaced; id.clear(); }
            else ++applied;
        }
        combat_->set_slot(SlotId(i), id);
        if (valid_tuning(binding.tuning)) { combat_->set_tuning(SlotId(i), binding.tuning); settings_->set_tuning(i, binding.tuning); }
    }
    save_name_ = preset->name; save_settings_or_log(); ++model_revision_;
    last_message_ = "Loaded " + name + ": " + std::to_string(applied) + " slot(s) set"
        + (unknown ? ", " + std::to_string(unknown) + " unknown move(s) skipped" : "")
        + (misplaced ? ", " + std::to_string(misplaced) + " not allowed in that slot" : "");
}
void Core::list_presets(uint64_t now, bool force) {
    if (!force && now < presets_listed_) return;
    presets_listed_ = now + 10000;   // a directory listing on the game thread; saves and deletes force it
    std::vector<std::string> names;
    if (storage_->list_presets(names) && names != preset_names_) { preset_names_ = std::move(names); ++model_revision_; }
    if (selected_preset_.empty() && !preset_names_.empty()) selected_preset_ = preset_names_.front();
    if (!selected_preset_.empty() && std::find(preset_names_.begin(), preset_names_.end(), selected_preset_) == preset_names_.end()) selected_preset_ = preset_names_.empty() ? "" : preset_names_.front();
}
std::string Core::move_label(const std::string& id) const {
    if (id.empty()) return "Weapon's own attack";
    if (id.starts_with("found:")) { const auto at = id.rfind('.'); return pretty(at == std::string::npos ? id.substr(6) : id.substr(at + 1)); }
    const auto* move = catalog_.find_move(id);
    return move ? pretty(move->display_name) : "Unknown move (" + id + ")";
}
// A move without an icon of its own (an enemy family with no render, a montage the asset registry
// found after a patch) shows the mod's emblem, so an assigned slot never looks empty.
static const char* fallback_icon = "file:assets/icons/move-fallback.png";
std::string Core::move_icon(const std::string& id) const {
    if (id.empty()) return {};
    const auto* move = catalog_.find_move(id);
    if (!move) return fallback_icon;
    const auto icon = move->origin == MoveOrigin::EnemyHumanoid ? enemy_icon(move->source_name) : weapon_icon(move->source_name);
    return icon.empty() ? fallback_icon : icon;
}
// An enemy family's icon is the mod's own PNG under assets/enemy-icons, when the file exists.
std::string Core::enemy_icon(const std::string& source) const {
    return enemy_icons_.contains(source) ? "file:assets/enemy-icons/" + source + ".png" : std::string{};
}
std::string Core::move_group(const std::string& id) const {
    const auto* move = id.empty() ? nullptr : catalog_.find_move(id);
    return move ? weapon_name(move->source_name) : std::string{};
}
std::string Core::move_description(const std::string& id) const {
    const auto* move = id.empty() ? nullptr : catalog_.find_move(id);
    if (!move) return "The slot plays the equipped weapon's own attack.";
    std::string text = weapon_name(move->source_name) + ", " + pretty(move->display_name) + ".";
    std::string slots; for (unsigned i = 0; i < slot_count; ++i) if (move->compatible_slots & (1u << i)) slots += (slots.empty() ? "" : ", ") + std::string(slot_to_string(SlotId(i)));
    if (!slots.empty()) text += " Originally the weapon's " + slots + " attack.";
    const auto pos = move->montage_path.rfind('.');
    text += " Montage " + (pos == std::string::npos ? move->montage_path : move->montage_path.substr(pos + 1)) + ".";
    return text;
}
// The candidate rows are the same for every slot (any move may go in any slot): the player
// weapons' moves sorted by weapon, the enemy moves by family, then montages the running game
// lists that no catalog knows. Built once, and again only when the registry scan changes them.
const nlohmann::json& Core::candidate_options() const {
    using Json = nlohmann::json;
    const bool scanned = discovery_.done();
    const uint64_t key = (scanned ? 1u : 0u) + discovery_.found().size() * 2u + catalog_.moves().size() * 65536u;
    if (candidates_.is_array() && key == candidates_key_) return candidates_;
    std::vector<const MoveDefinition*> moves, enemy;
    for (const auto& move : catalog_.moves()) {
        if (move.origin == MoveOrigin::EnemyHumanoid) enemy.push_back(&move);
        else moves.push_back(&move);
    }
    std::sort(moves.begin(), moves.end(), [](const MoveDefinition* a, const MoveDefinition* b) { return weapon_name(a->source_name) < weapon_name(b->source_name); });
    std::sort(enemy.begin(), enemy.end(), [](const MoveDefinition* a, const MoveDefinition* b) { return std::tie(a->source_name, a->display_name) < std::tie(b->source_name, b->display_name); });
    auto missing = [&](const MoveDefinition& move) { return scanned && !discovery_.known(move.montage_path); };
    Json options = Json::array();
    for (const auto* move : moves) {
        const bool sidearm = move->category == "Sidearm";
        if (sidearm && !sidearm_slot_enabled) continue;   // the sidearm slot is parked
        Json option = {{"id", move->id}, {"label", sidearm ? move->display_name : weapon_name(move->source_name)}, {"group", sidearm ? "Player's Sidearm" : "Player's Weapon"}, {"icon", sidearm ? std::string{} : weapon_icon(move->source_name)},
            {"title", sidearm ? move->display_name : weapon_name(move->source_name)}, {"subtitle", sidearm ? std::string("Sidearm fire") : pretty(move->display_name)}, {"description", sidearm ? move->description : move_description(move->id)}, {"value", sidearm ? std::string{} : pretty(move->display_name)}};
        if (missing(*move)) { option["enabled"] = false; option["disabled_label"] = "Not in this game version"; }
        options.push_back(std::move(option));
    }
    for (const auto* move : enemy) {
        options.push_back({{"id", move->id}, {"label", move->display_name}, {"group", "Enemy: " + enemy_name(move->source_name)}, {"title", move->display_name}, {"icon", move_icon(move->id)},
            {"subtitle", enemy_name(move->source_name) + " attack"}, {"enabled", !missing(*move)}, {"disabled_label", "Not in this game version"}, {"description", move->description}, {"value", move->payload_known ? std::string{} : std::string("No hit window in this animation")}});
    }
    if (scanned) {   // montages the running game lists that neither catalog knows: new content after a patch
        std::unordered_set<std::string> known;
        for (const auto& move : catalog_.moves()) known.insert(move.montage_path);
        for (const auto& f : discovery_.found()) {
            if (known.contains(f.path)) continue;
            options.push_back({{"id", "found:" + f.path}, {"label", pretty(f.name)}, {"group", std::string(f.player ? "New player move: " : "New enemy move: ") + enemy_name(f.source)},
                {"title", pretty(f.name)}, {"subtitle", "Found in this game version, not yet verified"}, {"icon", fallback_icon},
                {"description", "Listed by the game's asset registry but absent from the shipped catalog. Assigning it loads and checks the animation the same way; hit windows are unknown until then."}});
        }
    }
    candidates_ = std::move(options); candidates_key_ = key;
    return candidates_;
}
// Anything the page shows comes from here; the menu asks for the model only when this moves.
uint64_t Core::model_revision() {
    uint64_t signature = 0;
    if (combat_) for (unsigned i = 0; i < slot_count; ++i) { const auto slot = SlotId(i); signature = signature * 31 + combat_->slot_hits(slot) * 4 + (combat_->slot_ready(slot) ? 2 : 0) + (combat_->slot_error(slot).empty() ? 0 : 1); }
    signature = signature * 31 + (combat_ && combat_->hooked() ? 1 : 0) + preset_names_.size() * 2;
    if (combat_) signature = signature * 31 + (combat_->hold_unlocked(false) ? 1 : 0) + (combat_->hold_unlocked(true) ? 2 : 0);
    if (signature != model_signature_) { model_signature_ = signature; ++model_revision_; }
    return model_revision_;
}
nlohmann::json Core::model() const {
    using Json = nlohmann::json;
    Json sections = Json::array();
    // ---- Customize: a slots section. Each control is a slot; its options are the candidates
    // for that slot, grouped the way the user sketched them: the weapons that have a move for
    // this position, then the finisher and hold Tarstones, then enemy moves (later).
    // Every slot offers the same candidates, so the list lives once on the section ("candidates")
    // and each slot carries only its own state: the assigned move, its status hint and its rows.
    // (The parked sidearm slot would need its own list headed "Sidearm's own fire"; it is hidden.)
    Json candidates = Json::array();
    candidates.push_back({{"id", ""}, {"label", "Weapon's own attack"}, {"group", "Player's Weapon"}, {"title", "Weapon's own attack"},
        {"description", "Your weapon's own attack for this slot. Nothing is swapped."}});
    for (const auto& row : candidate_options()) candidates.push_back(row);
    Json customize = Json::array();
    for (unsigned i = 0; i < slot_count; ++i) {
        const auto slot = SlotId(i);
        const auto id = combat_ ? combat_->slot_move(slot) : std::string{};
        const bool ranged_slot = slot_role(i) == Role::Ranged;
        std::string hint;
        if (combat_ && !id.empty()) {
            const auto& err = combat_->slot_error(slot);
            const bool hold_slot = slot == SlotId::LC || slot == SlotId::HC;
            const bool locked = hold_slot && !combat_->hold_unlocked(slot == SlotId::HC);
            hint = !err.empty() ? "Not applied: " + err : !combat_->slot_ready(slot) ? std::string("Loading the animation...")
                : locked ? std::string("Waiting for the ") + (slot == SlotId::HC ? "Unwieldy Stone" : "Acolyte's Stone") + ". Without it a long press does the normal attack. Settings: Charged attacks, Always (cheat) removes the need."
                : "Applied. Played " + std::to_string(combat_->slot_hits(slot)) + " time(s) this session.";
        }
        // Per-slot settings: rows at the top of the same list, so keys and pad reach them like any
        // candidate. Each mirrors a choice control under "settings" that the event path validates.
        const SlotTuning tune = combat_ ? combat_->tuning(slot) : SlotTuning{};
        const std::string sid = std::string("slot.") + slot_to_string(slot);
        static const char* speeds[] = {"0.5", "0.75", "0.9", "1", "1.1", "1.25", "1.5", "1.75", "2", "2.5", "3", "4"};
        std::string speed_id = "1"; double best = 1e9;
        for (const char* v : speeds) if (const double d = std::abs(std::stod(v) - tune.speed); d < best) { best = d; speed_id = v; }
        Json speed_options = Json::array();
        for (const char* v : speeds) speed_options.push_back({{"id", v}, {"label", std::string(v) + "x"}});
        const bool mesh_ready = combat_ && combat_->slot_weapon_available(slot);
        // Short values (the row shows about twenty characters beside the arrows); the reasons go
        // in the description the window shows while the row is focused. An empty slot only has speed.
        Json settings_rows = Json::array();
        settings_rows.push_back({{"type", "choice"}, {"id", sid + ".speed"}, {"label", "Speed"}, {"value", speed_id}, {"options", speed_options},
            {"description", "How fast this slot plays. 1x is the game's speed."}});
        if (!id.empty()) {
            settings_rows.push_back({{"type", "choice"}, {"id", sid + ".feel"}, {"label", "Feel"}, {"value", tune.feel},
                {"options", Json::array({{{"id", "game"}, {"label", "Game's"}}, {{"id", "move"}, {"label", "Move's own"}}})},
                {"description", "Game's: your weapon's attack for this slot, with this animation fitted into it. Movement lock, combo timing, sounds and hit numbers stay the game's. "
                    "Move's own: the animation plays as its owner plays it, with its own hit windows and timing. Charged attacks still work either way."}});
        }
        if (!id.empty() && tune.feel == "move") {
            settings_rows.push_back({{"type", "choice"}, {"id", sid + ".hit_damage"}, {"label", "Damage"}, {"value", tune.hit_damage},
                {"options", Json::array({{{"id", "move"}, {"label", "Move's own"}}, {{"id", "weapon"}, {"label", "Weapon's own"}}})},
                {"description", "Move's own: the hit numbers that come with this animation (damage multiplier, poise damage, stagger). "
                    "Weapon's own: your weapon's numbers for this slot, on this animation's timing. Base damage is always your weapon's."}});
        }
        if (!id.empty()) {
            settings_rows.push_back({{"type", "choice"}, {"id", sid + ".weapon"}, {"label", "Visual"}, {"value", tune.weapon},
                {"options", Json::array({{{"id", "inventory"}, {"label", "My weapon"}}, {{"id", "move"}, {"label", "Move's weapon"}}})},
                {"description", std::string("My weapon: you keep holding your own weapon. Move's weapon: the weapon this move belongs to appears in your hand for the swing, then yours comes back. Hits always use your weapon.")
                    + (mesh_ready ? "" : combat_ && combat_->slot_unarmed(slot) ? " This move is a punch or a kick, so Move's weapon hides yours for the swing." : " No model is known for this move's weapon, so yours stays either way.")}});
            settings_rows.push_back({{"type", "choice"}, {"id", sid + ".armor"}, {"label", "Armor"}, {"value", tune.armor},
                {"options", Json::array({{{"id", "full"}, {"label", "Hyper armor (cheat)"}}, {{"id", "move"}, {"label", "Move's own"}}})},
                {"description", "Hyper armor: the game's hyper armor covers the whole move. Hits still hurt but do not stagger you out of it. Attacks that break hyper armor still do. "
                    "Move's own: a hit interrupts this move wherever it would interrupt its owner. Most of your own attacks have no hyper armor, so any hit cancels them."}});
        }
        if (!id.empty() && tune.feel == "move") {
            settings_rows.push_back({{"type", "choice"}, {"id", sid + ".steer"}, {"label", "Steer"}, {"value", tune.steer},
                {"options", Json::array({{{"id", "full"}, {"label", "Whole move"}}, {{"id", "move"}, {"label", "Move's own"}}})},
                {"description", "Whole move: the stick or your lock-on turns you through the whole move at your weapon's turn rate, and after the last hit pushing the stick cancels the recovery, like your own attacks. "
                    "Move's own: only the turn windows the move brings, and its recovery plays out."}});
        }
        customize.push_back({{"type", "choice"}, {"id", sid}, {"label", slot_to_string(slot)}, {"tile", tile_labels[i]}, {"name", slot_titles[i] + 4}, {"hidden", ranged_slot && !sidearm_slot_enabled},
            {"value", id}, {"icon", id.empty() ? weapon_icon(current_weapon_) : move_icon(id)}, {"hint", hint}, {"settings", settings_rows},
            {"settings_key", speed_id + "/" + tune.hit_damage + "/" + tune.weapon + "/" + tune.armor + "/" + tune.steer + (mesh_ready ? "/m" : combat_ && combat_->slot_unarmed(slot) ? "/u" : "")}, {"enabled", combat_ != nullptr && catalog_error_.empty()}});
    }
    sections.push_back({{"id", "customize"}, {"title", "Customize"}, {"kind", "slots"}, {"candidates", std::move(candidates)}, {"controls", std::move(customize)}});
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
        {"description", "Master switch. Off puts every attack back to the weapon's own animation at once. Nothing on disk changes."}, {"enabled", combat_ != nullptr}});
    settings.push_back({{"type", "choice"}, {"id", "charged_attacks"}, {"label", "Charged attacks"}, {"value", settings_->charged_without_stone() ? "always" : "stone"},
        {"options", Json::array({{{"id", "stone"}, {"label", "Need a Tarstone"}}, {{"id", "always"}, {"label", "Always (cheat)"}}})},
        {"description", "The game allows a charged light attack only with the Acolyte's Stone equipped, and a charged heavy attack only with the Unwieldy Stone. "
            "Always (cheat) gives you both unlocks without the stones, so the LC and HC slots work on any build. The charge still costs Resolve. Switching back takes the unlocks away again."},
        {"enabled", combat_ != nullptr}});
    settings.push_back({{"type", "choice"}, {"id", "next_attack"}, {"label", "Next attack"}, {"value", settings_->next_attack()},
        {"options", Json::array({{{"id", "hit"}, {"label", "After the first hit"}}, {{"id", "move"}, {"label", "After the whole move"}}})},
        {"description", "When you press the next attack while a swapped move is still playing. After the first hit: the press is kept and the next attack starts right after the move's first hit, the way your own combos chain. "
            "After the whole move: the press is kept until the move's last hit has landed, so long enemy combos play out before the chain continues. Applies to moves with Feel set to Move's own; Game's already chains like your weapon."},
        {"enabled", combat_ != nullptr}});
    settings.push_back({{"type", "choice"}, {"id", "enemy_difficulty"}, {"label", "Enemy difficulty"}, {"value", settings_->enemy_difficulty()},
        {"options", Json::array({{{"id", "game"}, {"label", "Follow Thestus"}}, {{"id", "night"}, {"label", "Always night"}}, {{"id", "day"}, {"label", "Always day (cheat)"}}})},
        {"description", "Thestus's night doubles the health and damage of every enemy that spawns. Follow Thestus: the game decides. "
            "Always night: enemies are that strong in daylight too, and day keeps its light and its world. Always day (cheat): enemies keep their day strength at night, and night keeps its light and its world. "
            "It applies to enemies as they spawn; the ones already standing change after you rest at a beacon or reload."},
        {"enabled", combat_ != nullptr}});
    {
        static const Combat::WorldTime unavailable{"Combat is unavailable.", false};
        const auto& time = combat_ ? combat_->world_time(runtime::now_ms()) : unavailable;
        const std::string& blocker = time.blocker; const bool night = time.night;
        settings.push_back({{"type", "button"}, {"id", "world_time"}, {"label", night ? "World: night" : "World: day"}, {"action_label", night ? "Seek Daylight" : "Call forth the Night"},
            {"enabled", blocker.empty()}, {"disabled_label", blocker.empty() ? std::string{} : blocker},
            {"description", std::string("Does what Thestus does, from here: the world turns to night or back to day, with its lighting and its world layers, and the game saves it. "
                "Night doubles the health and damage of enemies as they spawn, unless Enemy difficulty holds one strength. ") + (blocker.empty() ? "" : blocker)},
            {"confirm", night ? "Seek daylight? The world returns to day." : "Call forth the night? The world turns to night."}});
    }
    settings.push_back({{"type", "slider"}, {"id", "ui_scale"}, {"label", "Menu scale"}, {"value", settings_->ui_scale()}, {"min", 0.75}, {"max", 1.5}, {"step", 0.05}, {"unit", "x"},
        {"description", "Size of this page relative to the game's menus."}});
    settings.push_back({{"type", "button"}, {"id", "reset"}, {"label", "Reset all slots"}, {"action_label", "Reset"}, {"enabled", combat_ != nullptr && combat_->assigned() > 0},
        {"description", "Every slot back to the weapon's own attack. Presets on disk are kept."}, {"confirm", "Clear every slot?"}, {"disabled_label", "Nothing assigned"}});
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
    if (combat_ && !settings_->enabled()) status = "Custom Combat System is OFF: nothing is swapped. Turn it on in the Settings tab.";
    else if (!combat_) status = "Combat engine unavailable";
    else if (!settings_->enabled()) status = "Disabled";
    else status = std::to_string(combat_->assigned()) + " slot(s) assigned" + (combat_->hooked() ? ", engine active" : "");
    if (combat_ && settings_->enabled() && last_message_.empty()) {
        const bool light = !combat_->slot_move(SlotId::LC).empty() && !combat_->hold_unlocked(false);
        const bool heavy = !combat_->slot_move(SlotId::HC).empty() && !combat_->hold_unlocked(true);
        if (light || heavy) status = std::string("Charged attacks need a Tarstone: ") + (light && heavy ? "the Acolyte's Stone for LC and the Unwieldy Stone for HC" : light ? "the Acolyte's Stone for LC" : "the Unwieldy Stone for HC")
            + ". Or set Charged attacks to Always (cheat) in Settings.";
    }
    return {{"sections", std::move(sections)}, {"status", status}};
}
void Core::handle_event(const nlohmann::json& event) {
    const auto id = event.at("id").get<std::string>();
    last_message_.clear(); ++model_revision_;
    if (id.starts_with("slot.") && id.find('.', 5) != std::string::npos) {   // slot.<S>.<speed|feel|hit_damage|weapon|armor|steer>
        const auto second = id.find('.', 5);
        const auto slot = string_to_slot(id.substr(5, second - 5)); const auto key = id.substr(second + 1);
        if (!slot || !combat_) throw std::runtime_error("Unknown slot");
        auto tune = combat_->tuning(*slot);
        const auto value = event.at("value").get<std::string>();
        try {
            if (key == "speed") tune.speed = std::stod(value);
            else if (key == "feel") tune.feel = value;
            else if (key == "hit_damage") tune.hit_damage = value;
            else if (key == "weapon") tune.weapon = value;
            else if (key == "armor") tune.armor = value;
            else if (key == "steer") tune.steer = value;
            else throw std::runtime_error("Unknown slot setting");
        } catch (const std::logic_error&) { throw std::runtime_error("Invalid slot setting value"); }
        if (!valid_tuning(tune)) throw std::runtime_error("Invalid slot setting value");
        combat_->set_tuning(*slot, tune); settings_->set_tuning(size_t(*slot), tune);
        save_settings_or_log(); ++model_revision_; return;
    }
    if (id.starts_with("slot.")) {
        const auto slot = string_to_slot(id.substr(5));
        if (!slot || !combat_) throw std::runtime_error("Unknown slot");
        combat_->set_slot(*slot, event.at("value").get<std::string>());
        save_settings_or_log(); ++model_revision_; return;
    }
    if (id == "enabled") { settings_->set_enabled(event.at("value").get<bool>()); if (combat_) combat_->set_enabled(settings_->enabled()); save_settings_or_log(); return; }
    if (id == "charged_attacks") { settings_->set_charged_without_stone(event.at("value").get<std::string>() == "always"); if (combat_) combat_->set_hold_cheat(settings_->charged_without_stone()); save_settings_or_log(); return; }
    if (id == "world_time") {
        if (!combat_) throw std::runtime_error("Combat is unavailable");
        last_message_ = combat_->set_world_time(!combat_->world_night()); ++model_revision_; return;   // world_night() reads live; the cache is for the row
    }
    if (id == "enemy_difficulty") { settings_->set_enemy_difficulty(event.at("value").get<std::string>()); if (combat_) combat_->set_enemy_difficulty(settings_->enemy_difficulty()); save_settings_or_log(); return; }
    if (id == "next_attack") { settings_->set_next_attack(event.at("value").get<std::string>()); if (combat_) combat_->set_chain(settings_->next_attack()); save_settings_or_log(); return; }
    if (id == "ui_scale") { settings_->set_ui_scale(event.at("value").get<double>()); save_settings_or_log(); return; }
    if (id == "reset") { if (combat_) for (unsigned i = 0; i < slot_count; ++i) combat_->set_slot(SlotId(i), ""); save_settings_or_log(); return; }
    if (id == "preset.selected") { selected_preset_ = event.at("value").get<std::string>(); apply_preset(selected_preset_); return; }   // choosing a preset applies it
    if (id == "preset.name") {
        const auto name = event.at("value").get<std::string>();
        if (!runtime::valid_preset_name(name)) throw std::runtime_error("Use letters, digits, dash or underscore, up to 64 characters");
        save_name_ = name; return;
    }
    if (id == "preset.load") { apply_preset(selected_preset_); return; }
    if (id == "preset.save") {
        if (!runtime::valid_preset_name(save_name_)) throw std::runtime_error("Invalid preset name");
        PresetData preset; preset.name = save_name_; preset.author = "eins0fx"; preset.description = "Custom Combat System preset";
        for (unsigned i = 0; i < slot_count; ++i) {
            auto& binding = preset.slots[i]; binding.slot = SlotId(i);
            binding.move_id = combat_ ? combat_->slot_move(SlotId(i)) : std::string{};
            if (combat_) binding.tuning = combat_->tuning(SlotId(i));
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
    const auto now = GetTickCount64();
    engine::PlayerContext context;
    if (!phase(0, "player lookup", now, [&] { context = engine::player_context(player ? player->engine : nullptr); })) return;
    phase(1, "combat", now, [&] {
    // The equipped weapon, for the slots that play its own attack: one cached read, compared by pointer.
    if (now >= weapon_check_) {
        weapon_check_ = now + 500;
        auto* item = context.pc ? engine::cached_object_of(context.pc, L"ActiveWeaponItemDefinition") : nullptr;
        if (!item && context.pc) { try { item = engine::object_of(context.pc, L"ActiveWeaponItemDefinition"); } catch (...) {} }
        if (item != current_weapon_object_) {
            current_weapon_object_ = item;
            auto name = item ? engine::narrow(item->GetNamePrivate().ToString()) : std::string{};
            if (name.starts_with("ID_")) name.erase(0, 3);
            if (name.ends_with("_C")) name.resize(name.size() - 2);
            current_weapon_ = name; ++model_revision_;
            if (combat_) combat_->weapon_changed();
        }
    }
    if (combat_) combat_->tick(context, now);
    });
    // The registry scan runs once, spread over ticks, and the page reflects it when it finishes.
    phase(2, "asset scan", now, [&] {
    if (!discovery_.done()) {
        const auto before = discovery_.state();
        discovery_.tick(context);
        if (discovery_.done() && !discovery_reported_) {
            discovery_reported_ = true;
            nlohmann::json missing = nlohmann::json::array();
            std::unordered_set<std::string> known;
            for (const auto& move : catalog_.moves()) { known.insert(move.montage_path); if (!discovery_.known(move.montage_path) && missing.size() < 32) missing.push_back(move.montage_path); }
            size_t fresh = 0; for (const auto& f : discovery_.found()) if (!known.contains(f.path)) ++fresh;
            writer_->write(nlohmann::json{{"level", "info"}, {"msg", "Asset registry scan done"}, {"assets", discovery_.assets_seen()}, {"attack_montages", discovery_.present().size()},
                {"catalog_missing", missing}, {"new_candidates", fresh}, {"other_rig", discovery_.rig_skipped()}, {"untagged", discovery_.untagged()},
                {"roots", discovery_.path_rows()}, {"player_rig", discovery_.player_rig()}}.dump());
            ++model_revision_;
        } else if (discovery_.state() == Discovery::State::Failed && before != Discovery::State::Failed)
            writer_->write(nlohmann::json{{"level", "warn"}, {"msg", "Asset registry scan failed"}, {"error", discovery_.error()}}.dump());
    }
    });
    phase(3, "menu", now, [&] {
        if (menu_ && menu_->is_open()) list_presets(now, false);
        if (menu_) menu_->tick(context, delta);
    });
    phase(4, "status", now, [&] {
        if (settings_dirty_ && now >= settings_flush_at_) flush_settings();
        if (status_ && now >= status_after_) { status_after_ = now + 5000; status_->publish(get_status_json()); }
    });
#endif
}
template<class F> bool Core::phase(unsigned index, const char* name, uint64_t now, F&& body) {
    auto& p = phases_[index];
    if (p.disabled || now < p.retry_at) return false;
    const auto started = std::chrono::steady_clock::now();
    bool ok = true;
    try { body(); p.failures = 0; }
    catch (const std::exception& e) { ok = false; phase_failed(p, name, e.what(), now); }
    catch (...) { ok = false; phase_failed(p, name, "unknown failure", now); }
    const auto us = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
    p.total_us += us; ++p.samples; if (us > p.max_us) p.max_us = us;
    return ok;
}
void Core::phase_failed(Phase& p, const char* name, const char* what, uint64_t now) {
    ++p.failures; p.retry_at = now + 1000;
    const std::string message = std::string("CCS ") + name + " failed: " + what;
    if (message != p.last_error) { p.last_error = message; if (writer_) writer_->write(nlohmann::json{{"level", "warn"}, {"msg", message}}.dump()); }
    if (p.failures >= 60 && !p.disabled) {
        p.disabled = true;
        if (writer_) writer_->write(nlohmann::json{{"level", "error"}, {"msg", std::string("CCS ") + name + " switched off after sixty failures in a row; restart the game to try again"}}.dump());
    }
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
    try { flush_settings(); } catch (...) {}
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
    status["current_weapon"] = current_weapon_;
    status["discovery"] = {{"state", int(discovery_.state())}, {"assets", discovery_.assets_seen()}, {"montages", discovery_.present().size()}, {"error", discovery_.error()}};
    // Cost per phase since the last status write: microseconds on the game thread, average and worst tick.
    static const char* phase_names[] = {"player_lookup", "combat", "asset_scan", "menu", "status"};
    nlohmann::json timing = nlohmann::json::object();
    for (size_t i = 0; i < phases_.size(); ++i) {
        auto& p = phases_[i];
        timing[phase_names[i]] = {{"average_us", p.samples ? p.total_us / p.samples : 0}, {"max_us", p.max_us}, {"ticks", p.samples}, {"disabled", p.disabled}};
        p.total_us = 0; p.max_us = 0; p.samples = 0;
    }
    status["timing"] = std::move(timing);
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

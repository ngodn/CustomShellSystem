#include "combat.hpp"
#include <windows.h>
#include "rig.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <Unreal/FFrame.hpp>
#include <Unreal/UStruct.hpp>
#include <Unreal/FMemory.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace ccs {
using namespace engine;
namespace {
uint64_t now_us() { return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
constexpr size_t max_cached_classes = 512;
// A class name's identity: the FName's comparison index and number, as stored (8 bytes in a
// shipping build). Cached per class so the hook never converts a name to text twice.
static_assert(sizeof(FName) == 8, "FName layout is the 8 byte shipping layout");
uint64_t name_key(const FName& name) { uint64_t key{}; std::memcpy(&key, &name, sizeof(key)); return key; }
bool ends_with(const std::string& s, const char* suffix) { const std::string_view v(suffix); return s.size() >= v.size() && s.compare(s.size() - v.size(), v.size(), v) == 0; }
}
Combat::Combat(Deps deps) : deps_(std::move(deps)) {
    if (!deps_.hooks || deps_.hooks->version != CCS_HOOK_ABI_VERSION || deps_.hooks->size < sizeof(CcsHookHost) ||
        !deps_.hooks->add_native_pre || !deps_.hooks->remove || !deps_.hooks->statistics || !deps_.hooks->on_game_thread)
        throw std::runtime_error("Combat engine needs the loader's native hook host");
}
int Combat::hold_step(const std::string& full) {
    std::string name = full;
    if (ends_with(name, "_C")) name.resize(name.size() - 2);
    for (const char* twin : {"_Double", "_double", "_dble"}) if (ends_with(name, twin)) { name.resize(name.size() - std::string_view(twin).size()); break; }
    if (!ends_with(name, "_Hold")) return 0;
    const auto stem = name.substr(0, name.size() - 5);
    if (stem.size() < 3 || stem[stem.size() - 3] != '_' || (stem[stem.size() - 2] != 'A' && stem[stem.size() - 2] != 'B')) return 0;
    const char n = stem.back();
    return n >= '1' && n <= '3' ? n - '0' : 0;
}
int Combat::classify(const std::string& full) {
    std::string name = full;
    if (ends_with(name, "_C")) name.resize(name.size() - 2);
    // Sidearm primary fire: GA_<Sidearm>Attack_Primary and the shared ranged attack bases.
    if (sidearm_slot_enabled && name.starts_with("GA_") && !name.starts_with("GA_Player") &&
        (ends_with(name, "Attack_Primary") || ends_with(name, "Attack_Primary_InfiniteAmmo") || name == "GA_SidearmRangedAttackBase" ||
         name == "GA_SidearmRangedBurstAttackBase" || name == "GA_SidearmRangedChargedAttackBase")) return int(SlotId::R);
    // Sprint attacks: GA_Running_Attack_<Weapon>[_B], GA_Running_Attack_B_<Weapon>, GA_Player_<Weapon>_RunningAttack[_B],
    // GA_Player_Attack_Katanas_RunningAttack[_Axe]. The B variants are the sprint heavy; on the
    // Axatana the sprint heavy is the axe one (RunningAttack_Axe plays Running_Axe_02).
    if (name.find("Running") != std::string::npos && (name.starts_with("GA_Running") || name.starts_with("GA_Player"))) {
        const bool heavy = ends_with(name, "_B") || name.find("_Attack_B_") != std::string::npos || name.find("RunningAttack_B") != std::string::npos || ends_with(name, "RunningAttack_Axe");
        return int(heavy ? SlotId::SH : SlotId::SL);
    }
    if (!full.starts_with("GA_Player")) return -1;
    // Duality Stone variants (two hit windows) are the same step: GA_Player_Attack_Katanas_A1_double.
    for (const char* twin : {"_Double", "_double", "_dble"}) if (ends_with(name, twin)) { name.resize(name.size() - std::string_view(twin).size()); break; }
    if (ends_with(name, "_A_Finisher") || ends_with(name, "_A3_Finisher")) return int(SlotId::LF);
    if (ends_with(name, "_B_Finisher") || ends_with(name, "_B3_Finisher")) return int(SlotId::HF);
    if (ends_with(name, "_Hold")) {
        const auto stem = name.substr(0, name.size() - 5);
        if (ends_with(stem, "_A1") || ends_with(stem, "_A2") || ends_with(stem, "_A3")) return int(SlotId::LC);
        if (ends_with(stem, "_B1") || ends_with(stem, "_B2") || ends_with(stem, "_B3")) return int(SlotId::HC);
        return -1;
    }
    if (ends_with(name, "_A1")) return int(SlotId::L1);
    if (ends_with(name, "_A2")) return int(SlotId::L2);
    if (ends_with(name, "_A3")) return int(SlotId::L3);
    if (ends_with(name, "_B1")) return int(SlotId::H1);
    if (ends_with(name, "_B2")) return int(SlotId::H2);
    if (ends_with(name, "_B3")) return int(SlotId::H3);
    return -1;
}
void Combat::set_enabled(bool on) { enabled_ = on; if (!on) active_ = false; }
void Combat::set_slot(SlotId slot, std::string move_id) {
    auto& s = slots_[size_t(slot)];
    if (s.move_id == move_id) return;
    release(s);
    s.move_id = std::move(move_id); s.error.clear(); s.hits = 0; s.path.clear(); s.show_mesh_path.clear();
    if (s.move_id.empty()) { s.pending = false; return; }
    if (s.move_id.starts_with("found:")) { s.path = s.move_id.substr(6); s.pending = s.path.starts_with("/Game/") && s.path.size() < 1024; if (!s.pending) s.error = "Invalid montage path"; return; }
    const auto* move = deps_.catalog ? deps_.catalog->find_move(s.move_id) : nullptr;
    if (!move) { s.error = "Unknown move"; s.pending = false; return; }
    s.path = move->montage_path; s.show_mesh_path = weapon_mesh_path(move->source_name); s.pending = true;
}
void Combat::set_tuning(SlotId slot, const SlotTuning& tuning) {
    auto& s = slots_[size_t(slot)];
    if (!valid_tuning(tuning) || s.tuning == tuning) return;
    if (tuning.hit_damage != "weapon") restore_payload(s);   // back to the replacement's own payload at once
    if (tuning.weapon != "move" && shown_montage_.get() && shown_montage_.get() == s.montage.get()) restore_weapon();
    if (tuning.armor != s.tuning.armor || tuning.steer != s.tuning.steer || tuning.feel != s.tuning.feel || tuning.hit_damage != s.tuning.hit_damage) release_overlays(s);   // the overlay's rows depend on all four
    if (std::abs(tuning.speed - s.tuning.speed) > 1e-9) { release_transplants(s); release_carries(s); release_overlays(s); }   // every copy's charge window is sized by the speed
    s.tuning = tuning;
}
// The visible mesh of each move source's weapon: player primaries are WP_<X> blueprints whose
// SM_Weapon static mesh component holds these assets (grip pivot baked in, identity transform);
// enemy weapons are the static meshes the icon renders came from. Skeletal enemy weapons are left out.
std::string Combat::weapon_mesh_path(const std::string& source) {
    static const std::pair<const char*, const char*> meshes[] = {
        {"HeavyHammer", "/Game/Sparta/Weapons/Player/HeavyHammer/Art/Mesh/SM_ObsidianHammer_01"},
        {"Axatana", "/Game/Sparta/Weapons/Player/Axatana/Katanas/Katanas/SM_Axatana_Axe_Offset"},
        {"Katanas", "/Game/Sparta/Weapons/Player/Axatana/Katanas/Katanas/SM_Axatana_Katana_R_Model"},
        {"Scythe", "/Game/Sparta/Weapons/Player/Clockwork_Scythe/Art/Mesh/SM_Clockwork_Scythe"},
        {"ClockworkScythe", "/Game/Sparta/Weapons/Player/Clockwork_Scythe/Art/Mesh/SM_Clockwork_Scythe"},
        {"BattleAxe", "/Game/Sparta/Weapons/Player/BattleAxe/SM_Gragu_BattleAxe_01"},
        {"MartyrsBlade", "/Game/Sparta/Weapons/Player/GiantSword_AltSkin/SM_GiantSword_AltSkin"},
        {"HadernSword", "/Game/Sparta/Weapons/Player/HadernsSword/SM_HadernsSword_01"},
        {"HadernsSword", "/Game/Sparta/Weapons/Player/HadernsSword/SM_HadernsSword_01"},
        {"BlackNeedle", "/Game/Sparta/Weapons/Player/Spear_BlackNeedle/Mesh/SM_BlackNeedle"},
        {"AxeDagger", "/Game/Sparta/Weapons/Player/Axe_Dagger/SM_Weap_Player_Axe"},
        {"Aristocrat", "/Game/Sparta/Weapons/Enemies/Aristocrat/Art/Mesh/SM_Aristocrat_Model"},
        {"Batushka", "/Game/Sparta/Weapons/Enemies/Batushka_Mace/Art/Mesh/SM_Batushka_Mace"},
        {"Brigands", "/Game/Sparta/Characters/Enemies/Brigands/Brigand_Robe/Art/Mesh/SM_CultBrigand_Mace"},
        {"CannibalKnight", "/Game/Sparta/Weapons/Enemies/CannibalKnightSword/Textures/SM_CannibalSword_01"},
        {"CultistBase", "/Game/Sparta/Weapons/Enemies/BaseCultist_Staff/SM_BaseCultist_Staff"},
        {"CultistSpearLady", "/Game/Sparta/Weapons/Enemies/SpearCultistLady_Spear/SM_CrossbowSpear_01_Model"},
        {"Draugr", "/Game/Sparta/Weapons/Enemies/DraugrMorningStar/Art/Mesh/SM_Draurg_MorningStar"},
        {"FrogMama", "/Game/Sparta/Weapons/Enemies/FrogMama_Mace/SM_FrogMama_Mace_01"},
        {"HutchbackCarrier", "/Game/Sparta/Weapons/Enemies/HutchbackCarrior_Staff/Art/Mesh/SM_Hutchback_Staff_01"},
        {"MS1", "/Game/Sparta/Weapons/Enemies/MS1_HeavyCultist/Art/Mesh/SM_HeavyCultist_Hammer_01"},
        {"Miner", "/Game/Sparta/Weapons/Enemies/Miner_Pick/Art/Mesh/SM_Miner_Pick"},
        {"Sicario", "/Game/Sparta/Weapons/Enemies/SicarioDaggers/Art/Mesh/SM_Sicario_Dagger"},
        {"TarredCorpse", "/Game/Sparta/Weapons/Enemies/TarredCorpse_Sabre/SM_TarredCorpse_Sabre"},
        {"Wraith", "/Game/Sparta/Weapons/Enemies/WraithSword/SM_Weap_WraithSword"}};
    for (const auto& [key, path] : meshes) if (source == key || source.starts_with(std::string(key) + "_")) {
        const std::string p = path; return p + "." + p.substr(p.rfind('/') + 1);
    }
    return {};
}
int Combat::assigned() const { int n = 0; for (const auto& s : slots_) if (!s.move_id.empty()) ++n; return n; }
void Combat::release(Slot& slot) {
    restore_payload(slot);
    release_transplants(slot);
    release_carries(slot);
    release_overlays(slot);
    if (shown_montage_.get() && shown_montage_.get() == slot.montage.get()) restore_weapon();
    if (slot.rooted) { if (auto* montage = slot.montage.get()) drop_referenced(world_.get(), montage); slot.rooted = false; }
    if (auto* copy = slot.play.get(); copy && copy != slot.montage.get()) drop_referenced(world_.get(), copy);
    slot.play = {}; slot.play_kinds = 0; slot.unarmed = false;
    if (slot.show_rooted) { if (auto* mesh = slot.show_mesh.get()) drop_referenced(world_.get(), mesh); slot.show_rooted = false; }
    if (slot.ability_rooted) { if (auto* cls = slot.ability_class.get()) drop_referenced(world_.get(), cls); slot.ability_rooted = false; }
    slot.ability_class = {}; slot.move_payload = {}; slot.payload_gap = false;
    slot.montage = {}; slot.show_mesh = {}; slot.was_ready = false;   // a deliberate release is not a loss
}
// ---- hit payload: every hit-check notify inside a montage owns its payload object (multiplier,
// poise, break, reaction, effects). "Weapon's own" copies the slot's original payload fields onto
// the replacement's payloads and keeps a backup for restoring.
namespace {
const wchar_t* payload_fields[] = {L"HealthDamage", L"PoiseDamageOption", L"BreakDamageOption", L"DamageEffect", L"AdditionalEffects", L"DamageDataTag", L"ReactionTag",
    L"StrikeDirection", L"PoiseDamage", L"PoiseDamageEffect", L"BreakDamage", L"BreakDamageEffect", L"PoiseBrokenEffect", L"PoiseDamageDataTag", L"BreakDamageDataTag",
    L"PayloadTags", L"CustomElementalStacks"};
}
const Combat::NotifyLayout& Combat::notify_layout() const {
    auto* montage_class = static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage"));
    if (layout_.notifies && layout_.montage_class == montage_class) return layout_;
    NotifyLayout l; l.montage_class = montage_class;
    auto* p = montage_class ? montage_class->GetPropertyByNameInChain(L"Notifies") : nullptr;
    if (!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("AnimMontage.Notifies is missing");
    auto* inner = static_cast<FArrayProperty*>(p)->GetInner();
    if (!inner || !inner->IsA<FStructProperty>()) throw std::runtime_error("AnimMontage.Notifies is not a struct array");
    auto* row = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    l.notifies = p;
    l.notify = row ? row->GetPropertyByNameInChain(L"Notify") : nullptr;
    l.state = row ? row->GetPropertyByNameInChain(L"NotifyStateClass") : nullptr;
    l.link = row ? row->GetPropertyByNameInChain(L"LinkValue") : nullptr;
    l.duration = row ? row->GetPropertyByNameInChain(L"duration") : nullptr;   // reflected in lowercase; a state's window length
    l.rate_scale = montage_class->GetPropertyByNameInChain(L"RateScale");    // the montage's own play-rate multiplier
    if (!l.notify || !l.state || !l.link || !l.notify->IsA<FObjectProperty>() || !l.state->IsA<FObjectProperty>()) throw std::runtime_error("FAnimNotifyEvent layout changed");
    l.end_link = row->GetPropertyByNameInChain(L"EndLink");
    if (l.end_link && l.end_link->IsA<FStructProperty>()) l.end_value = static_cast<FStructProperty*>(l.end_link)->GetStruct().Get()->GetPropertyByNameInChain(L"LinkValue");
    // The hit classes first, then their fields (which weapon slot swings, which payload applies).
    l.hit_state = find_cached(L"/Script/Sparta.SpartaAnimNotifyState_HitCheck");
    l.hit_notify = find_cached(L"/Script/Sparta.SpartaAnimNotify_HitCheck");
    if (auto* c = static_cast<UClass*>(l.hit_state)) { l.slot_state = c->GetPropertyByNameInChain(L"WeaponSlot"); l.payload_state = c->GetPropertyByNameInChain(L"DamagePayload"); }
    if (auto* c = static_cast<UClass*>(l.hit_notify)) { l.slot_notify = c->GetPropertyByNameInChain(L"WeaponSlot"); l.payload_notify = c->GetPropertyByNameInChain(L"DamagePayload"); }
    layout_ = l;
    return layout_;
}
// ---- copies own what they play. A fresh instance of `source`'s class inside `outer`, with the
// source's reflected values; a blueprint's ubergraph frame belongs to its instance and is never
// copied (the new object gets its own at construction).
UObject* Combat::copy_object(UObject* source, UObject* outer) {
    auto* cls = source->GetClassPrivate();
    if (!cls) throw std::runtime_error("Object has no class");
    auto* copy = construct_class(cls, outer);
    auto* src = reinterpret_cast<std::byte*>(source); auto* dst = reinterpret_cast<std::byte*>(copy);
    static const FName frame(L"UberGraphFrame", FNAME_Add);
    for (UStruct* type = cls; type; type = type->GetSuperStruct()) {
        if (narrow(type->GetNamePrivate().ToString()) == "Object") break;
        for (auto* p : type->ForEachProperty()) {
            if (p->GetArrayDim() != 1 || p->GetOffset_Internal() < 0 || p->GetFName() == frame) continue;
            p->CopyCompleteValue(dst + p->GetOffset_Internal(), src + p->GetOffset_Internal());
        }
    }
    return copy;
}
namespace {
bool is_cancel_window(UObject* object) {
    return object && object->GetClassPrivate() && narrow(object->GetClassPrivate()->GetNamePrivate().ToString()).find("InterruptWithMovement") != std::string::npos;
}
}
int Combat::adopt_cancel_rows(UObject* copy) {
    if (!copy) return 0;
    const auto& l = notify_layout();
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(copy) + l.notifies->GetOffset_Internal());
    int adopted = 0;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + l.state->GetOffset_Internal(), sizeof(object));
        if (!is_cancel_window(object) || object->GetOuterPrivate() == copy) continue;
        auto* own = copy_object(object, copy);
        std::memcpy(rows.GetRawPtr(i) + l.state->GetOffset_Internal(), &own, sizeof(own));
        ++adopted;
    }
    return adopted;
}
// Steer's movement cancel reuses a window the copy already has when one reaches into the recovery:
// the controller clears its check when any window ends, so two overlapping windows would switch
// the check off early. The window grows to [from, to], never back over an earlier window.
bool Combat::extend_cancel(UObject* copy, float from, float to) {
    const auto& l = notify_layout();
    if (!l.end_link || !l.end_value) return false;
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(copy) + l.notifies->GetOffset_Internal());
    const int end_value = l.end_link->GetOffset_Internal() + l.end_value->GetOffset_Internal();
    int best = -1; float best_end = -1.f, earlier_end = 0.f;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + l.state->GetOffset_Internal(), sizeof(object));
        if (!is_cancel_window(object)) continue;
        float start{}, end{}; std::memcpy(&start, rows.GetRawPtr(i) + l.link->GetOffset_Internal(), 4); std::memcpy(&end, rows.GetRawPtr(i) + end_value, 4);
        if (!std::isfinite(start) || !std::isfinite(end)) continue;
        if (end >= from && end > best_end) { best = i; best_end = end; }
        else if (end < from) earlier_end = std::max(earlier_end, end);
    }
    if (best < 0) return false;
    auto* b = rows.GetRawPtr(best);
    float start{}; std::memcpy(&start, b + l.link->GetOffset_Internal(), 4);
    start = std::max(std::min(start, from), earlier_end + 0.01f);
    const float end = std::max(to, start + 0.05f), length = end - start;
    std::memcpy(b + l.link->GetOffset_Internal(), &start, 4); std::memcpy(b + end_value, &end, 4);
    if (l.duration) std::memcpy(b + l.duration->GetOffset_Internal(), &length, 4);
    return true;
}
bool Combat::payload_gaps(UObject* montage) const {
    if (!montage) return false;
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object) continue;
            FProperty* payload = hit_state && object->IsA(hit_state) ? l.payload_state : hit_notify && object->IsA(hit_notify) ? l.payload_notify : nullptr;
            if (!payload) continue;
            UObject* value{}; std::memcpy(&value, reinterpret_cast<std::byte*>(object) + payload->GetOffset_Internal(), sizeof(value));
            if (!value) return true;
        }
    }
    return false;
}
int Combat::fill_payloads(UObject* copy, UObject* payload) {
    if (!copy || !payload) return 0;
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(copy) + l.notifies->GetOffset_Internal());
    int filled = 0;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object) continue;
            FProperty* slot = hit_state && object->IsA(hit_state) ? l.payload_state : hit_notify && object->IsA(hit_notify) ? l.payload_notify : nullptr;
            if (!slot) continue;
            UObject* value{}; std::memcpy(&value, reinterpret_cast<std::byte*>(object) + slot->GetOffset_Internal(), sizeof(value));
            if (value) continue;   // a window with its own payload keeps it
            // The hit object is shared with the montage it came from; the copy gets its own.
            auto* own = object->GetOuterPrivate() == copy ? object : copy_object(object, copy);
            std::memcpy(reinterpret_cast<std::byte*>(own) + slot->GetOffset_Internal(), &payload, sizeof(payload));
            std::memcpy(rows.GetRawPtr(i) + field->GetOffset_Internal(), &own, sizeof(own));
            ++filled;
        }
    }
    return filled;
}
std::vector<UObject*> Combat::hit_payloads(UObject* montage) const {
    std::vector<UObject*> out;
    if (!montage) return out;
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    const int count = std::min(rows.Num(), 256);
    for (int i = 0; i < count; ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object || !((hit_state && object->IsA(hit_state)) || (hit_notify && object->IsA(hit_notify)))) continue;
            if (auto* payload = object_of(object, L"DamagePayload")) out.push_back(payload);
        }
    }
    return out;
}
void Combat::apply_weapon_payload(Slot& s, UObject* original, UObject* replacement, UObject* ability) {
    if (s.payload_source.get() == original) return;   // already carrying this weapon's payload
    restore_payload(s);
    // The weapon's payload: on its montage's hit windows, or (most player montages) on the attack
    // ability that plays it.
    const auto sources = hit_payloads(original);
    auto* from = !sources.empty() ? sources.front() : ability ? object_of(ability, L"AbilityHitPayload") : nullptr;
    if (!from) return;
    for (auto* to : hit_payloads(replacement)) {
        if (to == from) continue;
        PayloadBackup backup; backup.payload.capture(to);
        for (const auto* name : payload_fields) {
            auto* pf = from->GetPropertyByNameInChain(name); auto* pt = to->GetPropertyByNameInChain(name);
            if (!pf || !pt || !pf->SameType(pt) || pf->GetElementSize() != pt->GetElementSize() || pt->GetArrayDim() != 1) continue;
            std::vector<std::byte> saved(size_t(pt->GetElementSize()));
            pt->InitializeValue(saved.data());
            pt->CopyCompleteValue(saved.data(), reinterpret_cast<std::byte*>(to) + pt->GetOffset_Internal());
            pt->CopyCompleteValue(reinterpret_cast<std::byte*>(to) + pt->GetOffset_Internal(), reinterpret_cast<std::byte*>(from) + pf->GetOffset_Internal());
            backup.values.emplace_back(pt, std::move(saved));
        }
        s.backups.push_back(std::move(backup));
    }
    s.payload_source.capture(original);
    log("CCS payload copied onto " + narrow(replacement->GetNamePrivate().ToString()) + " from " + narrow(original->GetNamePrivate().ToString()) + " (" + std::to_string(s.backups.size()) + " hit window(s))");
}
void Combat::restore_payload(Slot& s) {
    for (auto& backup : s.backups) {
        auto* to = backup.payload.get();
        for (auto& [property, bytes] : backup.values) {
            if (to) property->CopyCompleteValue(reinterpret_cast<std::byte*>(to) + property->GetOffset_Internal(), bytes.data());
            property->DestroyValue(bytes.data());
        }
    }
    s.backups.clear(); s.payload_source = {};
}
// ---- game feel: clone the slot's own montage and fit the move's animation into its track.
// Everything the game authored for that weapon and slot stays (input block and combo windows,
// hyper armor, camera, whoosh, hit windows with the weapon's payload, sections, blends, root
// motion settings); the animation is time-warped so its first hit lands where the original's did.
namespace {
struct Segment { UObject* anim{}; float start_pos{}, anim_start{}, anim_end{}, rate{1.f}; };
// A TArray's raw {Data, Num, Max} resized through the engine's allocator, zero-filled: the SDK's
// script-array resize helpers do not link, and the engine frees the buffer with the same allocator.
void resize_raw_array(std::byte* array, int element_size, int count) {
    struct Raw { void* data; int32_t num; int32_t max; };
    auto* raw = reinterpret_cast<Raw*>(array);
    if (!GMalloc || !*GMalloc) throw std::runtime_error("Engine allocator unavailable");
    void* fresh = count > 0 ? (*GMalloc)->Malloc(size_t(count) * size_t(element_size), 16) : nullptr;
    if (count > 0 && !fresh) throw std::runtime_error("Out of memory");
    if (fresh) std::memset(fresh, 0, size_t(count) * size_t(element_size));
    if (raw->data) (*GMalloc)->Free(raw->data);
    raw->data = fresh; raw->num = count; raw->max = count;
}
}
float Combat::first_hit_time(UObject* montage) const { return first_hit_span(montage).first; }
std::pair<float, float> Combat::first_hit_span(UObject* montage) const {
    if (!montage) return {-1.f, -1.f};
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    float best = -1.f, best_end = -1.f;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object || !((hit_state && object->IsA(hit_state)) || (hit_notify && object->IsA(hit_notify)))) continue;
            float at{}, length{}; std::memcpy(&at, rows.GetRawPtr(i) + l.link->GetOffset_Internal(), sizeof(at));
            if (l.duration && field == l.state) std::memcpy(&length, rows.GetRawPtr(i) + l.duration->GetOffset_Internal(), sizeof(length));
            if (!std::isfinite(at) || at < 0.f || (best >= 0.f && at >= best)) continue;
            best = at; best_end = std::isfinite(length) && length > 0.f ? at + length : at;
        }
    }
    return {best, best_end};
}
UObject* Combat::transplant(Slot& s, UObject* original, UObject* replacement) {
    for (auto& t : s.feel) if (t.original.get() == original) { if (auto* clone = t.clone.get()) return clone; }
    if (s.feel.size() >= 4) { auto& old = s.feel.front(); if (auto* c = old.clone.get()) drop_referenced(world_.get(), c); s.feel.erase(s.feel.begin()); }
    auto* clone = build_transplant(original, replacement);
    try { stretch_hold_rows(clone, s.tuning.speed); } catch (const std::exception& e) { log(std::string("CCS hold window stretch failed: ") + e.what()); }
    adopt_cancel_rows(clone);
    Transplant t; t.original.capture(original); t.clone.capture(clone);
    keep_referenced(world_.get(), clone);
    s.feel.push_back(std::move(t));
    log("CCS game feel: " + narrow(original->GetNamePrivate().ToString()) + " now plays " + narrow(replacement->GetNamePrivate().ToString()));
    return clone;
}
void Combat::release_transplants(Slot& s) {
    for (auto& t : s.feel) if (auto* c = t.clone.get()) drop_referenced(world_.get(), c);
    s.feel.clear(); s.feel_warned = false;
}
// A runtime copy of a montage: every reflected property of the chain (notifies, sections, blends,
// curves, skeleton, root motion flags), with the notify links pointed at the copy. Outer: the
// source, so the copy shares its package and is not a "dynamic montage".
UObject* Combat::clone_montage(UObject* source) {
    auto* montage_class = static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage"));
    if (!source || !source->IsA(montage_class)) throw std::runtime_error("Not a montage");
    auto* clone = construct_class(montage_class, source);
    auto* src = reinterpret_cast<std::byte*>(source); auto* dst = reinterpret_cast<std::byte*>(clone);
    for (UStruct* type = montage_class; type; type = type->GetSuperStruct()) {
        if (narrow(type->GetNamePrivate().ToString()) == "Object") break;
        for (auto* p : type->ForEachProperty()) {
            if (p->GetArrayDim() != 1 || p->GetOffset_Internal() < 0) continue;
            p->CopyCompleteValue(dst + p->GetOffset_Internal(), src + p->GetOffset_Internal());
        }
    }
    auto* notifies_prop = montage_class->GetPropertyByNameInChain(L"Notifies");
    if (notifies_prop && notifies_prop->IsA<FArrayProperty>()) {
        auto* event_struct = static_cast<FStructProperty*>(static_cast<FArrayProperty*>(notifies_prop)->GetInner())->GetStruct().Get();
        auto* linked = event_struct->GetPropertyByNameInChain(L"LinkedMontage");
        auto* end_link = event_struct->GetPropertyByNameInChain(L"EndLink");
        auto* end_linked = end_link && end_link->IsA<FStructProperty>() ? static_cast<FStructProperty*>(end_link)->GetStruct().Get()->GetPropertyByNameInChain(L"LinkedMontage") : nullptr;
        FScriptArrayHelper events(static_cast<FArrayProperty*>(notifies_prop), dst + notifies_prop->GetOffset_Internal());
        for (int i = 0; i < std::min(events.Num(), 256); ++i) {
            auto* b = events.GetRawPtr(i);
            if (linked) std::memcpy(b + linked->GetOffset_Internal(), &clone, sizeof(clone));
            if (end_link && end_linked) std::memcpy(b + end_link->GetOffset_Internal() + end_linked->GetOffset_Internal(), &clone, sizeof(clone));
        }
    }
    return clone;
}
// Enemy montages carry notifies written for an AI: warps onto the attacker or the AI's target,
// warp re-initialisation, weapon equip state, AI events. On the player they read state that does
// not exist and can produce impossible transforms. A cleaned copy drops them. Two stay because the
// game authors them on the player's own swings too and they read only player state there: the
// turn window (ANS_RotateToFaceTarget takes a player branch that follows the lock-on, the soft
// target, the stick, or the camera) and the game's own warps toward WT_DesiredEndLocation and
// WT_DesiredRotation_Target, which every attack ability sets through GA_SpartaBase.
std::vector<std::string> Combat::strip_ai_notifies(UObject* clone) {
    static const char* dropped_classes[] = {"MotionWarpToFaceTarget", "MW_Attacker", "ReinitializeWarpTargets", "AlignHeightToWarpReference", "AI_EarlyOut",
        "UnparryableAttackWarning", "MeshOffset", "AbyssCheckForRM", "TriggerElementalMechanic", "HandleWeaponsEquipState", "SetWeaponEquipState",
        "HideWeapon", "PlayWeaponAnimation", "AddGameplayTags", "AddGameplayEffect", "SendGameplayTagEvent", "UnconstrainedMovement"};
    std::vector<std::string> dropped;
    auto* montage_class = static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage"));
    auto* notifies_prop = montage_class->GetPropertyByNameInChain(L"Notifies");
    if (!notifies_prop || !notifies_prop->IsA<FArrayProperty>()) return dropped;
    auto* inner = static_cast<FArrayProperty*>(notifies_prop)->GetInner();
    auto* event_struct = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    auto* notify = event_struct->GetPropertyByNameInChain(L"Notify"); auto* state = event_struct->GetPropertyByNameInChain(L"NotifyStateClass");
    if (!notify || !state) return dropped;
    auto* array = reinterpret_cast<std::byte*>(clone) + notifies_prop->GetOffset_Internal();
    FScriptArrayHelper events(static_cast<FArrayProperty*>(notifies_prop), array);
    const int count = std::min(events.Num(), 256), element = inner->GetElementSize();
    std::vector<int> keep;
    for (int i = 0; i < count; ++i) {
        std::string cls;
        for (auto* field : {notify, state}) { UObject* o{}; std::memcpy(&o, events.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(o)); if (o && o->GetClassPrivate()) cls = narrow(o->GetClassPrivate()->GetNamePrivate().ToString()); }
        bool drop = false; for (const char* word : dropped_classes) if (cls.find(word) != std::string::npos) drop = true;
        if (drop) dropped.push_back(cls); else keep.push_back(i);
    }
    if (dropped.empty()) return dropped;
    if (!GMalloc || !*GMalloc) throw std::runtime_error("Engine allocator unavailable");
    struct Raw { std::byte* data; int32_t num; int32_t max; };
    auto* raw = reinterpret_cast<Raw*>(array);
    auto* fresh = static_cast<std::byte*>(keep.empty() ? nullptr : (*GMalloc)->Malloc(keep.size() * size_t(element), 16));
    if (!keep.empty() && !fresh) throw std::runtime_error("Out of memory");
    for (size_t k = 0; k < keep.size(); ++k) { inner->InitializeValue(fresh + k * element); inner->CopyCompleteValue(fresh + k * element, raw->data + size_t(keep[k]) * element); }
    for (int i = 0; i < raw->num; ++i) inner->DestroyValue(raw->data + size_t(i) * element);
    if (raw->data) (*GMalloc)->Free(raw->data);
    raw->data = fresh; raw->num = int32_t(keep.size()); raw->max = int32_t(keep.size());
    // Branching-point tables index the notify list; without their notifies they must go too.
    for (auto name : {L"BranchingPointMarkers", L"BranchingPointStateNotifyIndices"})
        if (auto* p = montage_class->GetPropertyByNameInChain(name); p && p->IsA<FArrayProperty>())
            resize_raw_array(reinterpret_cast<std::byte*>(clone) + p->GetOffset_Internal(), static_cast<FArrayProperty*>(p)->GetInner()->GetElementSize(), 0);
    std::sort(dropped.begin(), dropped.end()); dropped.erase(std::unique(dropped.begin(), dropped.end()), dropped.end());
    return dropped;
}
UObject* Combat::build_transplant(UObject* original, UObject* replacement) {
    auto* montage_class = static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage"));
    if (!original->IsA(montage_class) || !replacement->IsA(montage_class)) throw std::runtime_error("Not montages");
    auto* clone = clone_montage(original);
    auto* dst = reinterpret_cast<std::byte*>(clone);
    // The move's animation: the replacement's first slot track, first segment.
    auto* tracks_prop = montage_class->GetPropertyByNameInChain(L"SlotAnimTracks");
    if (!tracks_prop || !tracks_prop->IsA<FArrayProperty>()) throw std::runtime_error("SlotAnimTracks missing");
    auto* track_struct = static_cast<FStructProperty*>(static_cast<FArrayProperty*>(tracks_prop)->GetInner())->GetStruct().Get();
    auto* anim_track = track_struct ? track_struct->GetPropertyByNameInChain(L"AnimTrack") : nullptr;
    auto* segments_prop = anim_track && anim_track->IsA<FStructProperty>() ? static_cast<FStructProperty*>(anim_track)->GetStruct().Get()->GetPropertyByNameInChain(L"AnimSegments") : nullptr;
    if (!segments_prop || !segments_prop->IsA<FArrayProperty>()) throw std::runtime_error("AnimSegments missing");
    auto* segment_struct = static_cast<FStructProperty*>(static_cast<FArrayProperty*>(segments_prop)->GetInner())->GetStruct().Get();
    auto field = [&](const wchar_t* name) { auto* f = segment_struct->GetPropertyByNameInChain(name); if (!f) throw std::runtime_error("AnimSegment field missing"); return f->GetOffset_Internal(); };
    const int off_anim = field(L"AnimReference"), off_pos = field(L"StartPos"), off_start = field(L"AnimStartTime"), off_end = field(L"AnimEndTime"), off_rate = field(L"AnimPlayRate"), off_loop = field(L"LoopingCount");
    // FAnimSegment::bValid is not reflected: it follows LoopingCount (AnimCompositeBase.h) and the
    // track evaluates a segment only when it is set. Loaded montages get it from PostLoad; ours must set it.
    const int segment_size = static_cast<FArrayProperty*>(segments_prop)->GetInner()->GetElementSize();
    const int off_valid = off_loop + 4;
    if (off_valid >= segment_size) throw std::runtime_error("AnimSegment layout has no room for the validity flag");
    auto segments_of = [&](UObject* montage, int track) {
        FScriptArrayHelper tracks(static_cast<FArrayProperty*>(tracks_prop), reinterpret_cast<std::byte*>(montage) + tracks_prop->GetOffset_Internal());
        if (track >= tracks.Num()) throw std::runtime_error("Montage has no slot track");
        return FScriptArrayHelper(static_cast<FArrayProperty*>(segments_prop), tracks.GetRawPtr(track) + anim_track->GetOffset_Internal() + segments_prop->GetOffset_Internal());
    };
    Segment rep;
    {
        auto segs = segments_of(replacement, 0);
        if (segs.Num() < 1) throw std::runtime_error("Move montage has no animation");
        auto* b = segs.GetRawPtr(0);
        std::memcpy(&rep.anim, b + off_anim, sizeof(rep.anim)); std::memcpy(&rep.anim_start, b + off_start, 4); std::memcpy(&rep.anim_end, b + off_end, 4); std::memcpy(&rep.rate, b + off_rate, 4);
        if (!rep.anim) throw std::runtime_error("Move montage has no animation reference");
        if (!(rep.rate > 0.f)) rep.rate = 1.f;
    }
    const float orig_len = read<float>(original, L"SequenceLength");
    const float rep_len = (rep.anim_end - rep.anim_start) / rep.rate;
    if (!(orig_len > 0.05f) || !(rep_len > 0.05f)) throw std::runtime_error("Montage length unusable");
    // Time warp: the move's first hit lands when the original's did; each side is stretched linearly.
    const float hit_old = first_hit_time(original), hit_new = first_hit_time(replacement);
    std::vector<Segment> plan;
    const bool warp = hit_old > 0.05f && hit_new > 0.05f && hit_old < orig_len - 0.05f && hit_new < rep_len - 0.05f;
    if (warp) {
        plan.push_back({rep.anim, 0.f, rep.anim_start, rep.anim_start + hit_new * rep.rate, hit_new / hit_old * rep.rate});
        plan.push_back({rep.anim, hit_old, rep.anim_start + hit_new * rep.rate, rep.anim_end, (rep_len - hit_new) / (orig_len - hit_old) * rep.rate});
    } else plan.push_back({rep.anim, 0.f, rep.anim_start, rep.anim_end, rep_len / orig_len * rep.rate});
    plan.back().anim_end += 0.002f * plan.back().rate;   // the track must reach SequenceLength despite float rounding
    for (const auto& g : plan)   // a bad segment would feed the skinning garbage poses; refuse instead
        if (!std::isfinite(g.rate) || g.rate < 0.05f || g.rate > 20.f || !std::isfinite(g.anim_start) || !std::isfinite(g.anim_end) || g.anim_end <= g.anim_start || g.start_pos < 0.f)
            throw std::runtime_error("Time warp produced an unusable segment");
    FScriptArrayHelper tracks(static_cast<FArrayProperty*>(tracks_prop), dst + tracks_prop->GetOffset_Internal());
    if (tracks.Num() < 1) throw std::runtime_error("Original montage has no slot track");
    for (int t = 0; t < tracks.Num(); ++t) {
        auto* array = reinterpret_cast<std::byte*>(tracks.GetRawPtr(t)) + anim_track->GetOffset_Internal() + segments_prop->GetOffset_Internal();
        const int element = static_cast<FArrayProperty*>(segments_prop)->GetInner()->GetElementSize();
        resize_raw_array(array, element, int(plan.size()));
        FScriptArrayHelper segs(static_cast<FArrayProperty*>(segments_prop), array);
        for (size_t i = 0; i < plan.size(); ++i) {
            auto* b = segs.GetRawPtr(int(i)); const auto& g = plan[i]; const int32_t loops = 1;
            std::memcpy(b + off_anim, &g.anim, sizeof(g.anim)); std::memcpy(b + off_pos, &g.start_pos, 4); std::memcpy(b + off_start, &g.anim_start, 4);
            std::memcpy(b + off_end, &g.anim_end, 4); std::memcpy(b + off_rate, &g.rate, 4); std::memcpy(b + off_loop, &loops, 4);
            b[off_valid] = 1;
        }
    }
    return clone;
}
// ---- the charge window. Hadern's-style weapons check the button inside a window on the normal
// swing (ANS_HoldAttackHandler: Input.Attack.<x>.Hold held past the threshold fires the _Hold
// selector event); hold-first weapons run the same check on the _Hold montage (ANS_HAH_*: released
// early fires the fail event that starts the normal cut). Both live in a notify state on the
// original montage. A replacement without one can never charge and never fall back, so the
// original's handler rides along on a copy of the replacement, its window scaled to the
// replacement's own wind-up (first hit to first hit, else length to length).
// The rows a weapon's own montage authors for the player that a replacement clip may lack.
Combat::RowKind Combat::row_kind(const std::string& cls) {
    if (cls.find("HoldAttackHandler") != std::string::npos || cls.starts_with("ANS_HAH_")) return RowKind::Hold;
    if (cls.find("RotateToFaceTarget") != std::string::npos) return RowKind::Turn;
    // Weapon state: the Axatana's transform notifies swap the katanas item for the axe item and
    // back; equip-state notifies stow or draw a slot (fists on Gragu, seal parries).
    if (cls.find("Transform_To") != std::string::npos || cls.find("WeaponEquipState") != std::string::npos || cls.find("WeaponsEquipState") != std::string::npos) return RowKind::State;
    // Elemental Tarstones: the weapon's charged montages send Event.TriggerElementalMechanic a
    // few frames after each hit, with the mechanic (blast, cloud, projectile, shockwave) and the
    // slot in the payload. GA_ElementalMechanicHandler reads only that payload, never the montage.
    if (cls.find("TriggerElementalMechanic") != std::string::npos) return RowKind::Mechanic;
    // The chain window: while it is open an attack press is buffered, and when it closes the
    // buffered press fires the next combo step (the game closes it shortly after the hit).
    // Without it a press during the swing is dropped and only the 0.75 s after the ability ends
    // chains, which is the "needs perfect timing" feel.
    if (cls.find("InputQueue") != std::string::npos) return RowKind::Queue;
    return RowKind::None;
}
void Combat::stretch_hold_rows(UObject* clone, double speed) {
    if (!clone || speed <= 1.0 + 1e-6) return;
    const auto& l = notify_layout();
    auto* inner = static_cast<FArrayProperty*>(l.notifies)->GetInner();
    auto* event_struct = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    auto field = [&](UStruct* type, const wchar_t* name) { auto* f = type->GetPropertyByNameInChain(name); if (!f) throw std::runtime_error("Notify event field missing"); return f; };
    auto* end_link = field(event_struct, L"EndLink");
    const int end_value = end_link->GetOffset_Internal() + field(static_cast<FStructProperty*>(end_link)->GetStruct().Get(), L"LinkValue")->GetOffset_Internal();
    const float len = read<float>(clone, L"SequenceLength"), hit = first_hit_time(clone);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(clone) + l.notifies->GetOffset_Internal());
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + l.state->GetOffset_Internal(), sizeof(object));
        if (!object || !object->GetClassPrivate() || row_kind(narrow(object->GetClassPrivate()->GetNamePrivate().ToString())) != RowKind::Hold) continue;
        auto* b = rows.GetRawPtr(i);
        float start{}, end{}; std::memcpy(&start, b + l.link->GetOffset_Internal(), 4); std::memcpy(&end, b + end_value, 4);
        const float window = std::max(0.02f, end - start) * float(speed);
        float limit = len - 0.01f;
        if (hit > 0.05f && hit > start) limit = std::min(limit, hit - 0.02f);
        end = std::min(start + window, limit); start = std::clamp(end - window, 0.f, std::max(0.f, end - 0.02f));
        const float length = end - start;
        std::memcpy(b + l.link->GetOffset_Internal(), &start, 4); std::memcpy(b + end_value, &end, 4);
        if (l.duration) std::memcpy(b + l.duration->GetOffset_Internal(), &length, 4);
    }
}
void Combat::set_chain(const std::string& mode) {
    const std::string wanted = mode == "move" ? "move" : "hit";
    if (wanted == chain_) return;
    chain_ = wanted;
    for (auto& s : slots_) release_carries(s);   // the carried chain windows are timed by the mode
}
unsigned Combat::row_kinds(UObject* montage) const {
    if (!montage) return 0;
    const auto& l = notify_layout();
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    unsigned kinds = 0;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (object && object->GetClassPrivate()) kinds |= bit(row_kind(narrow(object->GetClassPrivate()->GetNamePrivate().ToString())));
        }
    }
    return kinds;
}
std::vector<float> Combat::hit_begins(UObject* montage) const {
    std::vector<float> out;
    if (!montage) return out;
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object || !((hit_state && object->IsA(hit_state)) || (hit_notify && object->IsA(hit_notify)))) continue;
            float at{}; std::memcpy(&at, rows.GetRawPtr(i) + l.link->GetOffset_Internal(), sizeof(at));
            if (std::isfinite(at) && at >= 0.f) out.push_back(at);
        }
    }
    std::sort(out.begin(), out.end());
    // Paired weapons author two hit rows per strike (one per hand); those count as one hit.
    std::vector<float> merged;
    for (float t : out) if (merged.empty() || t - merged.back() > 0.05f) merged.push_back(t);
    return merged;
}
const Combat::MontageFacts& Combat::montage_facts(UObject* montage) {
    const auto key = name_key(montage->GetNamePrivate());
    if (auto it = montage_facts_.find(key); it != montage_facts_.end() && it->second.montage == montage) return it->second;
    if (montage_facts_.size() >= 256) montage_facts_.clear();
    MontageFacts facts; facts.montage = montage;
    // Some abilities play companion clips around the attack (the Axatana heavy plays the axe
    // transform before its hold and cut). Those stay the game's; only the attack clip is swapped.
    const auto played = narrow(montage->GetNamePrivate().ToString());
    for (const char* companion : {"Transform", "Equip", "Unequip", "Draw", "Stow", "Sheath"}) if (played.find(companion) != std::string::npos) facts.companion = true;
    try { facts.kinds = row_kinds(montage); } catch (...) { facts.kinds = 0; }
    return montage_facts_[key] = facts;
}
UObject* Combat::carry_windows(Slot& s, UObject* original, UObject* replacement, unsigned kinds) {
    for (auto& t : s.carries) if (t.original.get() == original) { if (auto* clone = t.clone.get()) return clone; }
    if (s.carries.size() >= 4) { auto& old = s.carries.front(); if (auto* c = old.clone.get()) drop_referenced(world_.get(), c); s.carries.erase(s.carries.begin()); }
    auto* clone = build_carry(original, replacement, kinds, s.tuning.speed);
    adopt_cancel_rows(clone);
    Transplant t; t.original.capture(original); t.clone.capture(clone);
    keep_referenced(world_.get(), clone);
    s.carries.push_back(std::move(t));
    return clone;
}
void Combat::release_carries(Slot& s) {
    for (auto& t : s.carries) if (auto* c = t.clone.get()) drop_referenced(world_.get(), c);
    s.carries.clear(); s.carry_warned = false;
}
// Rows copied from one montage onto a clone of another: the clone's notify list grows by the
// picked rows, each re-timed by `time` (absolute seconds on the clone) and re-linked to the clone.
// The copied row keeps its notify-state instance, owned by the source montage, so the source must
// stay referenced for as long as the clone plays (the game does the same for the hold carry).
std::string Combat::append_rows(UObject* clone, UObject* source, const std::vector<int>& rows, const std::function<void(int, float&, float&)>& time) {
    auto* montage_class = static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage"));
    auto* notifies_prop = montage_class->GetPropertyByNameInChain(L"Notifies");
    if (!notifies_prop || !notifies_prop->IsA<FArrayProperty>()) throw std::runtime_error("Notifies missing");
    auto* inner = static_cast<FArrayProperty*>(notifies_prop)->GetInner();
    auto* event_struct = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    auto field = [&](UStruct* type, const wchar_t* name) { auto* f = type->GetPropertyByNameInChain(name); if (!f) throw std::runtime_error("Notify event field missing"); return f; };
    auto* link_value = field(event_struct, L"LinkValue"); auto* link_method = field(event_struct, L"LinkMethod"); auto* linked = field(event_struct, L"LinkedMontage");
    auto* trigger_offset = field(event_struct, L"TriggerTimeOffset"); auto* end_offset = field(event_struct, L"EndTriggerTimeOffset"); auto* duration = field(event_struct, L"duration");
    auto* end_link = field(event_struct, L"EndLink");
    if (!end_link->IsA<FStructProperty>()) throw std::runtime_error("EndLink is not a struct");
    auto* link_struct = static_cast<FStructProperty*>(end_link)->GetStruct().Get();
    const int end_value = end_link->GetOffset_Internal() + field(link_struct, L"LinkValue")->GetOffset_Internal();
    const int end_method = end_link->GetOffset_Internal() + field(link_struct, L"LinkMethod")->GetOffset_Internal();
    const int end_linked = end_link->GetOffset_Internal() + field(link_struct, L"LinkedMontage")->GetOffset_Internal();
    FScriptArrayHelper from(static_cast<FArrayProperty*>(notifies_prop), reinterpret_cast<std::byte*>(source) + notifies_prop->GetOffset_Internal());
    for (int row : rows) if (row < 0 || row >= from.Num()) throw std::runtime_error("Notify row out of range");
    auto* array = reinterpret_cast<std::byte*>(clone) + notifies_prop->GetOffset_Internal();
    FScriptArrayHelper events(static_cast<FArrayProperty*>(notifies_prop), array);
    const int count = events.Num(), element = inner->GetElementSize(), total = count + int(rows.size());
    if (!GMalloc || !*GMalloc) throw std::runtime_error("Engine allocator unavailable");
    struct Raw { std::byte* data; int32_t num; int32_t max; };
    auto* raw = reinterpret_cast<Raw*>(array);
    auto* fresh = static_cast<std::byte*>((*GMalloc)->Malloc(size_t(total) * size_t(element), 16));
    if (!fresh) throw std::runtime_error("Out of memory");
    for (int i = 0; i < count; ++i) { inner->InitializeValue(fresh + size_t(i) * element); inner->CopyCompleteValue(fresh + size_t(i) * element, raw->data + size_t(i) * element); }
    std::string report;
    for (size_t k = 0; k < rows.size(); ++k) {
        auto* b = fresh + size_t(count + int(k)) * element;
        inner->InitializeValue(b); inner->CopyCompleteValue(b, from.GetRawPtr(rows[k]));
        float start{}, end{}; std::memcpy(&start, b + link_value->GetOffset_Internal(), 4); std::memcpy(&end, b + end_value, 4);
        time(int(k), start, end);
        const float length = end - start, zero = 0.f; const uint8_t absolute = 0;
        std::memcpy(b + link_value->GetOffset_Internal(), &start, 4); std::memcpy(b + end_value, &end, 4); std::memcpy(b + duration->GetOffset_Internal(), &length, 4);
        std::memcpy(b + trigger_offset->GetOffset_Internal(), &zero, 4); std::memcpy(b + end_offset->GetOffset_Internal(), &zero, 4);
        std::memcpy(b + link_method->GetOffset_Internal(), &absolute, 1); std::memcpy(b + end_method, &absolute, 1);
        std::memcpy(b + linked->GetOffset_Internal(), &clone, sizeof(clone)); std::memcpy(b + end_linked, &clone, sizeof(clone));
        report += (report.empty() ? "" : ", ") + std::to_string(start).substr(0, 4) + " to " + std::to_string(end).substr(0, 4) + " s";
    }
    for (int i = 0; i < raw->num; ++i) inner->DestroyValue(raw->data + size_t(i) * element);
    if (raw->data) (*GMalloc)->Free(raw->data);
    raw->data = fresh; raw->num = total; raw->max = total;
    return report;
}
UObject* Combat::build_carry(UObject* original, UObject* replacement, unsigned wanted, double speed) {
    auto* montage_class = static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage"));
    if (!original->IsA(montage_class) || !replacement->IsA(montage_class)) throw std::runtime_error("Not montages");
    // The rows to carry from the original: hold handlers, turn windows, weapon-state notifies and
    // elemental triggers, each timed its own way.
    const auto& l = notify_layout();
    std::vector<int> rows; std::vector<RowKind> kinds;
    const auto hits_old = hit_begins(original), hits_new = hit_begins(replacement);
    // An elemental trigger follows a hit; it goes with the replacement's hit of the same rank. A
    // replacement with fewer hits keeps only the triggers of the original's last hit, on its own
    // last hit, so a one-hit clip fires the finishing blast once and not once per lost hit.
    auto hit_rank = [&](float t) { int r = -1; for (size_t i = 0; i < hits_old.size(); ++i) if (hits_old[i] <= t + 0.001f) r = int(i); return r; };
    FScriptArrayHelper source(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(original) + l.notifies->GetOffset_Internal());
    for (int i = 0; i < std::min(source.Num(), 256); ++i) {
        RowKind kind = RowKind::None;
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, source.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (object && object->GetClassPrivate()) { const auto k = row_kind(narrow(object->GetClassPrivate()->GetNamePrivate().ToString())); if (k != RowKind::None) kind = k; }
        }
        if (kind == RowKind::None || !(wanted & bit(kind))) continue;
        if (kind == RowKind::Queue && (hits_new.empty() || hits_old.empty())) continue;   // nothing to anchor the window on
        if (kind == RowKind::Mechanic) {
            float at{}; std::memcpy(&at, source.GetRawPtr(i) + l.link->GetOffset_Internal(), sizeof(at));
            const int rank = hit_rank(at);
            if (hits_new.empty() || rank < 0) continue;                                        // nothing to hang it on
            if (rank >= int(hits_new.size()) && rank != int(hits_old.size()) - 1) continue;   // a lost middle hit
        }
        rows.push_back(i); kinds.push_back(kind);
    }
    if (rows.empty()) throw std::runtime_error("Original has none of the rows to carry");
    const float rep_len = read<float>(replacement, L"SequenceLength");
    const float orig_len = read<float>(original, L"SequenceLength");
    if (!(rep_len > 0.05f) || !(orig_len > 0.05f)) throw std::runtime_error("Montage length unusable");
    const auto [hit_new, hit_new_end] = first_hit_span(replacement);
    const auto [hit_old, hit_old_end] = first_hit_span(original);
    const float last_new_end = last_hit_end(replacement);
    auto* clone = clone_montage(replacement);
    std::string report;
    append_rows(clone, original, rows, [&](int k, float& start, float& end) {
        const RowKind kind = kinds[size_t(k)];
        if (kind == RowKind::Queue) {
            // The weapon opens its chain window a little before its hit and closes it a little
            // after; the replacement keeps that lead and tail around the hit the window ends on.
            const float lead = hit_old >= 0.f ? start - hit_old : 0.f, tail = hit_old_end >= 0.f ? end - hit_old_end : 0.15f;
            const float anchor = chain_ == "move" && last_new_end > 0.f ? last_new_end : hit_new_end;
            start = std::clamp(hit_new + lead, 0.f, std::max(0.f, rep_len - 0.06f));
            end = std::clamp(anchor + tail, start + 0.05f, std::max(start + 0.05f, rep_len - 0.01f));
        } else if (kind == RowKind::Mechanic) {
            const int rank = hit_rank(start);
            const size_t j = size_t(std::min(rank, int(hits_new.size()) - 1));
            start = std::clamp(hits_new[j] + (start - hits_old[size_t(rank)]), 0.f, std::max(0.f, rep_len - 0.01f));
            end = start;
        } else if (kind == RowKind::State) {
            // Weapon state fires where the weapon's montage fires it (the Axatana joins or splits
            // in its first frames), kept inside the replacement. An instant notify stays instant.
            const bool instant = !(end > start + 0.001f);
            start = std::clamp(start, 0.f, std::max(0.f, rep_len - 0.03f));
            end = instant ? start : std::clamp(end, start + 0.02f, std::max(start + 0.02f, rep_len - 0.01f));
        } else if (kind == RowKind::Turn) {
            // The turn window is animation timing: the game closes it where the weapon's first
            // hit lands, so the stick steers the wind-up and never the strike. On the replacement
            // it closes where that clip's first hit ends, or at the same fraction of the clip when
            // the clip has no hit window, and opens where the original's did, scaled to the clip.
            const float scale = rep_len / orig_len;
            float open = start * scale, close = hit_new_end > 0.05f ? hit_new_end : end * scale;
            close = std::clamp(close, 0.05f, std::max(0.05f, rep_len - 0.01f));
            open = std::clamp(open, 0.f, std::max(0.f, close - 0.05f));
            start = open; end = close;
        } else {
            // The hold window is input timing, not animation: the combo counter starts its 0.5 s
            // and 1.05 s timers when the window begins, so it keeps the original's absolute times
            // and a held button charges after the same delay on every weapon. It moves earlier only
            // when the replacement's first hit would land inside it, and never leaves the montage.
            // The slot's speed runs the montage faster, so the window grows by it to last as long.
            const float window = std::max(0.02f, end - start) * float(std::max(1.0, speed));
            end = start + window;
            float limit = rep_len - 0.01f;
            if (hit_new > 0.05f) limit = std::min(limit, hit_new - 0.02f);
            if (end > limit) { end = limit; start = end - window; }
            start = std::clamp(start, 0.f, std::max(0.f, rep_len - 0.03f));
            end = std::clamp(end, start + 0.02f, std::max(start + 0.02f, rep_len - 0.01f));
        }
        report += std::string(report.empty() ? "" : ", ") + (kind == RowKind::Turn ? "turn " : kind == RowKind::State ? "weapon state " : kind == RowKind::Mechanic ? "elemental " : kind == RowKind::Queue ? "chain " : "hold ") + std::to_string(start).substr(0, 4) + (end <= start ? " s" : " to " + std::to_string(end).substr(0, 4) + " s");
    });
    log("CCS windows carried: " + narrow(original->GetNamePrivate().ToString()) + " -> " + narrow(replacement->GetNamePrivate().ToString()) + ": " + report);
    return clone;
}
// ---- the player-feel overlay: rows the game authors on the player's own swings, appended over
// whatever the slot plays. Hyper armor (ANS_HyperArmor applies GE_HyperArmor, State.HyperArmor,
// on begin and removes it on end) covers the clone from its first frame to its last. Steering is
// the weapon's turn window (ANS_RotateToFaceTarget with the weapon's interp speed, continuous
// target) from the first frame to the last hit, then the movement cancel (ANS_InterruptWithMovement)
// from the last hit to the end, the shape of every player attack. The rows come from a player
// montage that authors all three with no required tag (the Martyr's Blade heavy), kept as the donor.
bool Combat::ensure_donor() {
    if (donor_.get() && donor_armor_ >= 0 && donor_turn_ >= 0 && donor_cancel_ >= 0) return true;
    if (donor_failed_) return false;
    try {
        static const char* donor_path = "/Game/Sparta/Characters/Humans/Player/Animations/MartyrsBlade/AM_Shells_MartyrsBlade_B1.AM_Shells_MartyrsBlade_B1";
        auto* donor = load(donor_path);
        const auto& l = notify_layout();
        FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(donor) + l.notifies->GetOffset_Internal());
        int armor = -1, turn = -1, cancel = -1;
        for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + l.state->GetOffset_Internal(), sizeof(object));
            if (!object || !object->GetClassPrivate()) continue;
            const auto cls = narrow(object->GetClassPrivate()->GetNamePrivate().ToString());
            if (armor < 0 && cls == "ANS_HyperArmor_C") armor = i;
            else if (turn < 0 && cls == "ANS_RotateToFaceTarget_C") turn = i;
            else if (cancel < 0 && cls == "ANS_InterruptWithMovement_C") cancel = i;
        }
        if (armor < 0 || turn < 0 || cancel < 0) throw std::runtime_error("donor montage lacks a hyper armor, turn or movement cancel row");
        if (!keep_referenced(world_.get(), donor)) throw std::runtime_error("no world to hold the donor");
        donor_.capture(donor); donor_armor_ = armor; donor_turn_ = turn; donor_cancel_ = cancel;
        return true;
    } catch (const std::exception& e) {
        donor_failed_ = true;
        log(std::string("CCS player-feel rows unavailable: ") + e.what() + "; Armor and Steer stay the move's own");
        return false;
    }
}
float Combat::last_hit_end(UObject* montage) const {
    if (!montage) return -1.f;
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    float best = -1.f;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object || !((hit_state && object->IsA(hit_state)) || (hit_notify && object->IsA(hit_notify)))) continue;
            float at{}, length{}; std::memcpy(&at, rows.GetRawPtr(i) + l.link->GetOffset_Internal(), sizeof(at));
            if (l.duration && field == l.state) std::memcpy(&length, rows.GetRawPtr(i) + l.duration->GetOffset_Internal(), sizeof(length));
            if (!std::isfinite(at) || at < 0.f) continue;
            const float end = std::isfinite(length) && length > 0.f ? at + length : at;
            if (end > best) best = end;
        }
    }
    return best;
}
UObject* Combat::build_overlay(UObject* source, bool armor, bool steer, UObject* payload) {
    if ((armor || steer) && !donor_ready()) throw std::runtime_error("player-feel rows not loaded yet");   // never loads from the hook
    const float len = read<float>(source, L"SequenceLength");
    if (!(len > 0.05f)) throw std::runtime_error("Montage length unusable");
    const float last = std::max(0.02f, len - 0.01f);
    // Steering ends where the last hit does; the movement cancel starts there. A move without a
    // hit window steers to the end and keeps its recovery (nothing marks where it starts).
    const float hits_end = steer ? last_hit_end(source) : -1.f;
    bool cancel = steer && hits_end > 0.05f && hits_end < last - 0.1f;
    auto* clone = clone_montage(source);
    std::string report;
    auto note = [&](const char* what, float start, float end) { report += std::string(report.empty() ? "" : ", ") + what + " " + std::to_string(start).substr(0, 4) + " to " + std::to_string(end).substr(0, 4) + " s"; };
    if (cancel && extend_cancel(clone, hits_end + 0.02f, last)) { note("cancel (own window, extended)", hits_end + 0.02f, last); cancel = false; }
    std::vector<int> rows; std::vector<std::pair<float, float>> spans; std::vector<const char*> names;
    if (armor) { rows.push_back(donor_armor_); spans.push_back({0.f, last}); names.push_back("armor"); }
    if (steer) { rows.push_back(donor_turn_); spans.push_back({0.f, hits_end > 0.05f && hits_end < last - 0.1f ? hits_end : last}); names.push_back("turn"); }
    if (cancel) { rows.push_back(donor_cancel_); spans.push_back({hits_end + 0.02f, last}); names.push_back("cancel"); }
    if (!rows.empty()) append_rows(clone, donor_.get(), rows, [&](int k, float& start, float& end) {
        start = spans[size_t(k)].first; end = spans[size_t(k)].second; note(names[size_t(k)], start, end);
    });
    if (payload) { const int filled = fill_payloads(clone, payload); if (filled) report += std::string(report.empty() ? "" : ", ") + "move payload on " + std::to_string(filled) + " hit window(s)"; }
    adopt_cancel_rows(clone);   // the donor's window and any copied one now stop this copy
    log("CCS player feel added: " + narrow(source->GetNamePrivate().ToString()) + ": " + (report.empty() ? std::string("nothing to add") : report));
    return clone;
}
UObject* Combat::overlaid(Slot& s, UObject* source, bool armor, bool steer, UObject* payload) {
    const unsigned flags = (armor ? 1u : 0u) | (steer ? 2u : 0u) | (payload ? 4u : 0u);
    for (auto& t : s.overlays) if (t.original.get() == source && t.flags == flags) { if (auto* clone = t.clone.get()) return clone; }
    if (s.overlays.size() >= 8) { auto& old = s.overlays.front(); if (auto* c = old.clone.get()) drop_referenced(world_.get(), c); s.overlays.erase(s.overlays.begin()); }
    auto* clone = build_overlay(source, armor, steer, payload);
    // A source that is the montage itself (not a carry or transplant of ours) brings its own hold rows unstretched.
    if (source == s.montage.get() || source == s.play.get()) { try { stretch_hold_rows(clone, s.tuning.speed); } catch (const std::exception& e) { log(std::string("CCS hold window stretch failed: ") + e.what()); } }
    Transplant t; t.original.capture(source); t.clone.capture(clone); t.flags = flags;
    keep_referenced(world_.get(), clone);
    s.overlays.push_back(std::move(t));
    return clone;
}
void Combat::release_overlays(Slot& s) {
    for (auto& t : s.overlays) if (auto* c = t.clone.get()) drop_referenced(world_.get(), c);
    s.overlays.clear(); s.overlay_warned = false;
}
// ---- weapon in hand: the held weapon actor (WP_WeaponBase_Static) draws its SM_Weapon static
// mesh component; swapping that mesh shows the move's weapon without touching inventory, slots,
// abilities or collision. Restored when the montage stops playing (polled while shown).
void Combat::show_weapon(UObject* mesh, UObject* montage, uint64_t now) {
    auto* pawn = pawn_.get(); if (!pawn || !mesh) return;
    auto* weapons = object_of(pawn, L"WeaponsComponent"); if (!weapons) return;
    Call in_hand(weapons, L"GetWeaponInHand", 1); in_hand.run();
    auto* weapon = in_hand.get<UObject*>(); if (!weapon) return;
    auto* component = object_of(weapon, L"SM_Weapon"); if (!component) return;
    if (shown_component_.get() == component) { shown_montage_ = {}; shown_montage_.capture(montage); shown_since_ = now; shown_seen_playing_ = false; }
    else {
        restore_weapon();
        auto* current = object_of(component, L"StaticMesh");
        if (current == mesh) return;
        shown_original_ = {}; if (current) shown_original_.capture(current);
        Call materials(component, L"GetMaterials", 1); materials.run();
        auto* out = materials.param(L"ReturnValue");
        if (out->IsA<FArrayProperty>()) {
            FScriptArrayHelper list(static_cast<FArrayProperty*>(out), materials.data(out));
            for (int i = 0; i < std::min(list.Num(), 16); ++i) { UObject* m{}; std::memcpy(&m, list.GetRawPtr(i), sizeof(m)); ObjectHandle h; if (m) h.capture(m); shown_materials_.push_back(h); }
        }
        shown_component_.capture(component); shown_actor_ = {}; shown_actor_.capture(weapon);
        shown_montage_ = {}; shown_montage_.capture(montage); shown_since_ = now; shown_seen_playing_ = false;
    }
    Call set(component, L"SetStaticMesh", 2); set.set(L"NewMesh", mesh); set.run();
    log("CCS weapon shown: " + narrow(mesh->GetNamePrivate().ToString()) + " on " + narrow(weapon->GetNamePrivate().ToString()));
}
void Combat::weapon_changed() { restore_weapon(); }
// Unarmed: the montage has hit windows and none of them swings a Primary or Secondary slot
// weapon; the hits come from Weapon.Slot.Body.* (fists, legs) instead.
bool Combat::unarmed_montage(UObject* montage) const {
    if (!montage) return false;
    const auto& l = notify_layout();
    auto* hit_state = static_cast<UClass*>(l.hit_state); auto* hit_notify = static_cast<UClass*>(l.hit_notify);
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(l.notifies), reinterpret_cast<std::byte*>(montage) + l.notifies->GetOffset_Internal());
    int hits = 0;
    for (int i = 0; i < std::min(rows.Num(), 256); ++i) {
        for (auto* field : {l.notify, l.state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object) continue;
            FProperty* slot = hit_state && object->IsA(hit_state) ? l.slot_state : hit_notify && object->IsA(hit_notify) ? l.slot_notify : nullptr;
            if (!slot) continue;
            ++hits;
            FName tag{}; std::memcpy(&tag, reinterpret_cast<std::byte*>(object) + slot->GetOffset_Internal(), sizeof(tag));   // FGameplayTag starts with its FName
            const auto name = narrow(tag.ToString());
            if (name.starts_with("Weapon.Slot.Primary") || name.starts_with("Weapon.Slot.Secondary")) return false;
        }
    }
    return hits > 0;
}
void Combat::hide_weapons(UObject* montage, uint64_t now, uint64_t provisional_ms) {
    auto* pawn = pawn_.get(); if (!pawn || !montage) return;
    if (!hidden_actors_.empty()) {   // already hidden: follow the montage that plays now; a firm hide ends a provisional one
        hidden_montage_ = {}; hidden_montage_.capture(montage); hidden_since_ = now; hidden_seen_playing_ = false;
        hidden_until_ = provisional_ms ? (hidden_until_ ? now + provisional_ms : 0) : 0;
        return;
    }
    auto* weapons = object_of(pawn, L"WeaponsComponent"); if (!weapons) return;
    std::vector<UObject*> actors;
    { Call in_hand(weapons, L"GetWeaponInHand", 1); in_hand.run(); if (auto* w = in_hand.get<UObject*>()) actors.push_back(w); }
    for (const wchar_t* slot : {L"Weapon.Slot.Primary.Left", L"Weapon.Slot.Primary.Right"}) {   // a paired weapon holds one actor per hand
        Call in_slot(weapons, L"GetWeaponInSlot", 2); in_slot.set(L"WeaponSlot", FName(slot, FNAME_Add)); in_slot.run();
        if (auto* w = in_slot.get<UObject*>(); w && std::find(actors.begin(), actors.end(), w) == actors.end()) actors.push_back(w);
    }
    for (auto* actor : actors) {
        bool hidden = false;
        if (auto* p = actor->GetPropertyByNameInChain(L"bHidden"); p && p->IsA<FBoolProperty>()) hidden = static_cast<FBoolProperty*>(p)->GetPropertyValue(reinterpret_cast<std::byte*>(actor) + p->GetOffset_Internal());
        if (hidden) continue;   // already hidden by the game or another mod: not ours to show back
        Call hide(actor, L"SetActorHiddenInGame", 1); hide.set(L"bNewHidden", true); hide.run();
        ObjectHandle h; h.capture(actor); hidden_actors_.push_back(h);
    }
    if (hidden_actors_.empty()) return;
    hidden_montage_ = {}; hidden_montage_.capture(montage); hidden_since_ = now; hidden_seen_playing_ = false;
    hidden_until_ = provisional_ms ? now + provisional_ms : 0;
    log("CCS weapon hidden for an unarmed move (" + std::to_string(hidden_actors_.size()) + " actor(s)" + (provisional_ms ? ", until the charge stage decides" : "") + ")");
}
void Combat::restore_hidden() {
    if (hidden_actors_.empty()) return;
    for (auto& h : hidden_actors_) if (auto* actor = h.get()) {
        try { Call show(actor, L"SetActorHiddenInGame", 1); show.set(L"bNewHidden", false); show.run(); }
        catch (const std::exception& e) { log(std::string("CCS weapon show back failed: ") + e.what()); }
    }
    hidden_actors_.clear(); hidden_montage_ = {}; hidden_seen_playing_ = false; hidden_until_ = 0;
    log("CCS weapon shown again");
}
void Combat::poll_hidden(const PlayerContext& player, uint64_t now) {
    if (hidden_actors_.empty()) return;
    auto* montage = hidden_montage_.get();
    if (!player.pawn || player.pawn != pawn_.get() || !montage) { restore_hidden(); return; }
    if (hidden_until_ && now >= hidden_until_) { restore_hidden(); return; }   // the press became a charge: the weapon swings after all
    try {
        auto* mesh = object_of(player.pawn, L"Mesh"); if (!mesh) { restore_hidden(); return; }
        Call instance(mesh, L"GetAnimInstance", 1); instance.run();
        auto* anim = instance.get<UObject*>(); if (!anim) { restore_hidden(); return; }
        Call playing(anim, L"Montage_IsPlaying", 2); playing.set(L"Montage", montage); playing.run();
        if (playing.get<bool>()) {
            hidden_seen_playing_ = true;
            // CSS's MISC "in use" rule shows a held weapon back when an action starts; keep ours hidden until the swing ends.
            for (auto& h : hidden_actors_) if (auto* actor = h.get())
                if (auto* p = actor->GetPropertyByNameInChain(L"bHidden"); p && p->IsA<FBoolProperty>() && !static_cast<FBoolProperty*>(p)->GetPropertyValue(reinterpret_cast<std::byte*>(actor) + p->GetOffset_Internal())) {
                    Call hide(actor, L"SetActorHiddenInGame", 1); hide.set(L"bNewHidden", true); hide.run();
                }
        } else if (hidden_seen_playing_ || now - hidden_since_ > 500) restore_hidden();   // the task starts the montage a frame later
    } catch (const std::exception& e) { log(std::string("CCS weapon hide poll failed: ") + e.what()); restore_hidden(); }
}
void Combat::restore_weapon() {
    restore_hidden();
    auto* component = shown_component_.get();
    if (component) {
        log("CCS weapon restored");
        try {
            Call set(component, L"SetStaticMesh", 2); set.set(L"NewMesh", shown_original_.get()); set.run();
            for (size_t i = 0; i < shown_materials_.size(); ++i) if (auto* m = shown_materials_[i].get()) { Call mat(component, L"SetMaterial", 2); mat.set(L"ElementIndex", int32_t(i)); mat.set(L"Material", m); mat.run(); }
        } catch (const std::exception& e) { log(std::string("CCS weapon restore failed: ") + e.what()); }
    }
    shown_component_ = {}; shown_original_ = {}; shown_montage_ = {}; shown_actor_ = {}; shown_materials_.clear(); shown_seen_playing_ = false;
}
void Combat::poll_weapon(const PlayerContext& player, uint64_t now) {
    if (!shown_component_.ptr) return;
    auto* montage = shown_montage_.get();
    if (!shown_component_.alive() || !player.pawn || player.pawn != pawn_.get() || !montage) { restore_weapon(); return; }
    try {
        // The weapon left the hand (stowed, switched, dropped): put it back whatever the montage does.
        if (auto* weapons = object_of(player.pawn, L"WeaponsComponent")) {
            Call in_hand(weapons, L"GetWeaponInHand", 1); in_hand.run();
            if (in_hand.get<UObject*>() != shown_actor_.get()) { restore_weapon(); return; }
        }
        auto* mesh = object_of(player.pawn, L"Mesh"); if (!mesh) { restore_weapon(); return; }
        Call instance(mesh, L"GetAnimInstance", 1); instance.run();
        auto* anim = instance.get<UObject*>(); if (!anim) { restore_weapon(); return; }
        Call playing(anim, L"Montage_IsPlaying", 2); playing.set(L"Montage", montage); playing.run();
        if (playing.get<bool>()) shown_seen_playing_ = true;
        else if (shown_seen_playing_ || now - shown_since_ > 500) restore_weapon();   // the task starts the montage a frame later
    } catch (const std::exception& e) { log(std::string("CCS weapon poll failed: ") + e.what()); restore_weapon(); }
}
bool Combat::player_outer(UObject* object) const {
    if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))) return false;
    const auto* pawn = pawn_.get(); const auto* asc = asc_.get();
    if (!pawn && !asc) return false;
    for (unsigned depth = 0; object && depth < 8; ++depth, object = object->GetOuterPrivate())
        if (object == pawn || object == asc) return true;
    return false;
}
// One montage per tick, on the game thread, only while a player exists: the load is blocking
// (a few milliseconds from the pak the first time) and the skeleton check needs the pawn's mesh.
void Combat::load_pending(const PlayerContext& player) {
    if (!player.pawn) return;
    // The player-feel donor loads here, off the swing hook, as soon as any slot with a move wants
    // armor or steering; the hook only ever uses it once it is in memory.
    if (!donor_ready() && !donor_failed_) {
        bool wanted = false;
        for (const auto& s : slots_) if (!s.move_id.empty() && (s.tuning.armor == "full" || s.tuning.steer == "full")) wanted = true;
        if (wanted) ensure_donor();
    }
    for (auto& s : slots_) {
        if (!s.pending) continue;
        s.pending = false;
        try {
            auto* montage = load(s.path);
            if (!montage->IsA(static_cast<UClass*>(find_cached(L"/Script/Engine.AnimMontage")))) throw std::runtime_error("Not an animation montage");
            // The montage must be authored on a rig the player's body can play: the game's human
            // skeleton, a CSS package's copy of it, or the exact rig of the body worn now (rig.hpp).
            auto* skeleton = object_of(montage, L"Skeleton");
            const auto rig_path = skeleton ? narrow(skeleton->GetPathName()) : std::string{};
            if (!rig::compatible(rig_path, rig::player_skeleton(player)))
                throw std::runtime_error("Montage is not on the player's rig (" + (skeleton ? rig::short_name(rig_path) : std::string("no skeleton")) + ")");
            if (!keep_referenced(player.world, montage)) throw std::runtime_error("No world to hold the animation");
            s.rooted = true;
            s.montage.capture(montage);
            // Enemy and unverified montages play through a cleaned copy without their AI notifies.
            const auto* move = deps_.catalog ? deps_.catalog->find_move(s.move_id) : nullptr;
            const bool foreign = (move && move->origin == MoveOrigin::EnemyHumanoid) || (!move && s.path.find("/Shells/") == std::string::npos);
            s.play = {};
            if (foreign) {
                auto* cleaned = clone_montage(montage);
                const auto dropped = strip_ai_notifies(cleaned);
                adopt_cancel_rows(cleaned);   // the enemy's own movement-cancel windows now cut this copy
                keep_referenced(player.world, cleaned);
                s.play.capture(cleaned);
                std::string list; for (const auto& d : dropped) list += (list.empty() ? "" : ", ") + d;
                log("CCS cleaned copy of " + narrow(montage->GetNamePrivate().ToString()) + (dropped.empty() ? ": nothing to drop" : ": dropped " + list));
            }
            try { s.play_kinds = row_kinds(s.play.get() ? s.play.get() : montage); } catch (...) { s.play_kinds = 0; }
            try { s.unarmed = s.show_mesh_path.empty() && unarmed_montage(montage); } catch (...) { s.unarmed = false; }
            // A player move's own attack ability carries its hit payload (poise, break, reaction);
            // the hook puts it on the hit windows that have none. Loaded here, never in the hook.
            if (move && !move->ability_class.empty()) {
                try {
                    auto* cls = load(move->ability_class);
                    const auto dot = move->ability_class.rfind('.');
                    const auto cdo_path = move->ability_class.substr(0, dot) + ".Default__" + move->ability_class.substr(dot + 1);
                    auto* cdo = dot == std::string::npos ? nullptr : find_optional(wide(cdo_path).c_str());
                    auto* payload = cdo ? object_of(cdo, L"AbilityHitPayload") : nullptr;
                    if (!payload) throw std::runtime_error("no AbilityHitPayload on " + cdo_path);
                    if (!keep_referenced(player.world, cls)) throw std::runtime_error("no world to hold the ability class");
                    s.ability_rooted = true; s.ability_class.capture(cls); s.move_payload.capture(payload);
                    s.payload_gap = payload_gaps(montage);
                } catch (const std::exception& e) { log("CCS move payload unavailable for " + s.move_id + ": " + e.what() + "; its hits use the slot weapon's payload"); }
            }
            if (!s.show_mesh_path.empty()) {
                try {
                    auto* mesh = load(s.show_mesh_path);
                    if (!mesh->IsA(static_cast<UClass*>(find_cached(L"/Script/Engine.StaticMesh")))) throw std::runtime_error("Not a static mesh");
                    if (keep_referenced(player.world, mesh)) s.show_rooted = true;
                    s.show_mesh.capture(mesh);
                } catch (const std::exception& e) { log("CCS weapon mesh unavailable for " + s.move_id + ": " + e.what()); }
            }
            s.error.clear();
            log("CCS slot ready: " + s.move_id + " -> " + narrow(montage->GetNamePrivate().ToString()));
        } catch (const std::exception& e) { s.error = e.what(); release(s); log("CCS slot load failed: " + s.move_id + ": " + e.what()); }
        return;
    }
}
void Combat::ensure_hook() {
    if (token_) return;
    auto* cdo = find(L"/Script/CSAbilityTasks.Default__AbilityTask_PlayMontageAndWaitWithNotifies");
    Call signature(cdo, L"PlayMontageAndWaitWithNotifies", 8);
    auto* function = signature.function();
    if (!function->HasAnyFunctionFlags(FUNC_Native) || function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate)) throw std::runtime_error("Montage task is not a native function");
    auto* ability = signature.param(L"OwningAbility"); auto* montage = signature.param(L"MontageToPlay"); auto* rate = signature.param(L"Rate");
    for (auto* p : {ability, montage, rate})
        if (p->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm) || p->GetOffset_Internal() < 0 || p->GetOffset_Internal() + p->GetElementSize() > function->GetParmsSize())
            throw std::runtime_error("Montage task parameter layout changed");
    for (auto* p : {ability, montage}) if (!p->IsA<FObjectProperty>() || p->GetElementSize() != sizeof(UObject*)) throw std::runtime_error("Montage task object parameter changed");
    if (!rate->IsA<FNumericProperty>() || rate->GetElementSize() != sizeof(float) || !static_cast<FNumericProperty*>(rate)->IsFloatingPoint()) throw std::runtime_error("Montage task rate parameter changed");
    auto* montage_class = static_cast<FObjectProperty*>(montage)->GetPropertyClass().Get();
    if (!montage_class || narrow(montage_class->GetPathName()) != "/Script/Engine.AnimMontage") throw std::runtime_error("Montage task montage class changed");
    WeakObject serial(function);
    if (serial.Get() != function) throw std::runtime_error("Montage task weak identity is unavailable");
    inputs_ = {ability, montage, rate};
    function_.capture(function);
    token_ = deps_.hooks->add_native_pre(deps_.hooks->context, function, callback, this);
    if (!token_) { inputs_ = {}; function_ = {}; throw std::runtime_error("Native montage pre-hook registration rejected"); }
    log("CCS combat hook installed");
}
void Combat::ensure_effect_hook() {
    if (effect_token_) return;
    static const char* paths[] = {"/Game/Sparta/Core/GameplayEffects/GE_DamageMultiplier.GE_DamageMultiplier_C",
                                  "/Game/Sparta/Core/GameplayEffects/GE_EffectMaxHealth_Multiplier.GE_EffectMaxHealth_Multiplier_C",
                                  "/Game/Sparta/Core/GameplayEffects/GE_NGP_DamageMultiplier.GE_NGP_DamageMultiplier_C"};
    UObject* classes[3]{};
    for (int i = 0; i < 3; ++i) { classes[i] = load(paths[i]); if (!classes[i]) throw std::runtime_error(std::string("missing ") + paths[i]); keep_referenced(world_.get(), classes[i]); }
    auto* cdo = find(L"/Script/GameplayAbilities.Default__AbilitySystemComponent");
    Call signature(cdo, L"BP_ApplyGameplayEffectToSelf", 4);
    auto* function = signature.function();
    if (!function->HasAnyFunctionFlags(FUNC_Native)) throw std::runtime_error("BP_ApplyGameplayEffectToSelf is not native");
    auto* param = signature.param(L"GameplayEffectClass");
    if (!param || !param->IsA<FObjectProperty>() || param->GetElementSize() != sizeof(UObject*) || param->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)
        || param->GetOffset_Internal() < 0 || param->GetOffset_Internal() + param->GetElementSize() > function->GetParmsSize())
        throw std::runtime_error("BP_ApplyGameplayEffectToSelf parameter layout changed");
    night_damage_ = {}; night_damage_.capture(classes[0]); night_health_ = {}; night_health_.capture(classes[1]); ngp_damage_ = {}; ngp_damage_.capture(classes[2]);
    effect_class_param_ = param;
    effect_function_ = {}; effect_function_.capture(function);
    effect_token_ = deps_.hooks->add_native_pre(deps_.hooks->context, function, effect_callback, this);
    if (!effect_token_) { effect_class_param_ = nullptr; effect_function_ = {}; throw std::runtime_error("Effect pre-hook registration rejected"); }
    log("CCS enemy difficulty hook installed (" + difficulty_ + ")");
}
void Combat::remove_effect_hook() {
    if (!effect_token_) return;
    if (!deps_.hooks->remove(deps_.hooks->context, effect_token_)) return;   // deferred: retried next tick
    effect_token_ = 0; effect_class_param_ = nullptr; effect_function_ = {};
    log("CCS enemy difficulty hook removed");
}
void Combat::effect_callback(void* user, void* object, void* frame, void*) noexcept {
    auto* self = static_cast<Combat*>(user);
    try { self->observe_effect(static_cast<UObject*>(object), frame); }
    catch (...) { ++self->failures_; }
}
void Combat::observe_effect(UObject* asc, void* frame_ptr) {
    if (injecting_ || difficulty_ == "game" || !effect_class_param_ || !asc || asc == asc_.get()) return;   // the player's own effects are never touched
    auto* frame = static_cast<FFrame*>(frame_ptr);
    auto* locals = frame ? frame->Locals() : nullptr;
    if (!locals || frame->Node() != effect_function_.get()) return;
    auto* slot = static_cast<std::byte*>(static_cast<void*>(locals)) + effect_class_param_->GetOffset_Internal();
    UObject* cls{}; std::memcpy(&cls, slot, sizeof(cls));
    if (!cls) return;
    if (difficulty_ == "day") {
        if (cls != night_damage_.get() && cls != night_health_.get()) return;
        UObject* none = nullptr; std::memcpy(slot, &none, sizeof(none));   // the engine applies nothing for an empty class
        ++difficulty_removed_;
        return;
    }
    // "night": the spawner applies New Game+ right after the night pair. An enemy reaching it
    // without the night damage effect spawned in daylight; it gets the pair now, in the game's order.
    if (cls != ngp_damage_.get()) return;
    auto* damage = night_damage_.get(); auto* health = night_health_.get();
    if (!damage || !health) return;
    Call count(asc, L"GetGameplayEffectCount", 4);
    count.set(L"SourceGameplayEffect", damage); count.set(L"OptionalInstigatorFilterComponent", static_cast<UObject*>(nullptr)); count.set(L"bEnforceOnGoingCheck", true);
    count.run();
    if (count.get<int32_t>() > 0) return;
    injecting_ = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{injecting_};
    for (auto* effect : {damage, health}) {
        Call context(asc, L"MakeEffectContext", 1); context.run();
        Call apply(asc, L"BP_ApplyGameplayEffectToSelf", 4);
        apply.set(L"GameplayEffectClass", effect); apply.set(L"Level", 0.0f);
        apply.copy(L"EffectContext", context, L"ReturnValue");
        apply.run();
    }
    ++difficulty_added_;
}
namespace {
UObject* library(const char* class_path) {
    // A blueprint function library's CDO, loading its class first: <package>.<Name>_C -> <package>.Default__<Name>_C.
    const std::string path = class_path; const auto dot = path.rfind('.');
    load(path);
    auto* cdo = find_optional(wide(path.substr(0, dot) + ".Default__" + path.substr(dot + 1)).c_str());
    if (!cdo) throw std::runtime_error("library missing: " + path);
    return cdo;
}
}
std::string Combat::world_time_blocker() {
    auto* pawn = pawn_.get(); auto* world = world_.get();
    if (!pawn || !world) return "Enter the world first.";
    if (narrow(world->GetNamePrivate().ToString()) != "L_Core_World_01") return "Only in the open world, not in a dungeon or a boss arena.";
    try {
        Call unlocked(library("/Game/Sparta/Core/Utility/BPFL_TimeOfDay.BPFL_TimeOfDay_C"), L"IsThestusUnlocked", 2);
        unlocked.set(L"__WorldContext", pawn); unlocked.run();
        if (!unlocked.get<bool>()) return "Thestus has not offered the night yet in this playthrough.";
        Call instance(find(L"/Script/Engine.Default__GameplayStatics"), L"GetGameInstance", 2);
        instance.set(L"WorldContextObject", pawn); instance.run();
        if (auto* gi = instance.get<UObject*>()) {
            Call boss(gi, L"GetInBossFight", 2); boss.run();
            if (boss.get<bool>(L"InBossFight")) return "Not during a boss fight.";
        }
    } catch (const std::exception& e) { return std::string("Unavailable: ") + e.what(); }
    return {};
}
bool Combat::world_night() {
    auto* pawn = pawn_.get(); if (!pawn) return false;
    try {
        Call night(library("/Game/Sparta/Core/Utility/BPFL_TimeOfDay.BPFL_TimeOfDay_C"), L"IsInNightMode", 2);
        night.set(L"__WorldContext", pawn); night.run();
        return night.get<bool>();
    } catch (...) { return false; }
}
std::string Combat::set_world_time(bool night) {
    if (const auto why = world_time_blocker(); !why.empty()) return why;
    auto* pawn = pawn_.get();
    const FName tag(L"World.TimeOfDay.Night", FNAME_Add);
    auto* player = library("/Game/Sparta/Core/Player/BPFL_Player.BPFL_Player_C");
    if (night) {
        Call add(player, L"AddNarrativeTag", 4);
        add.set(L"TagToAdd", tag); add.set(L"SaveToFile", true); add.set(L"LoadTagsAfterSave", true); add.set(L"__WorldContext", pawn); add.run();
    } else {
        Call remove(player, L"RemoveNarrativeTag", 3);
        remove.set(L"TagToRemove", tag); remove.set(L"SaveToFile", true); remove.set(L"__WorldContext", pawn); remove.run();
    }
    // The weather zones relight on TriggerNight or TriggerDay; the rest of the game hears TimeOfDayChanged.
    auto* events = find(L"/Script/GlobalEventSubsystem.Default__GESFunctions");
    for (const char* event : {night ? "/Game/Sparta/Core/GES/GlobalEvent_TriggerNight.GlobalEvent_TriggerNight_C" : "/Game/Sparta/Core/GES/GlobalEvent_TriggerDay.GlobalEvent_TriggerDay_C",
                              "/Game/Sparta/Core/GES/GlobalEvent_TimeOfDayChanged.GlobalEvent_TimeOfDayChanged_C"}) {
        Call broadcast(events, L"BroadcastEventNoData", 3);
        broadcast.set(L"EventClass", load(event)); broadcast.set(L"Filter", FName()); broadcast.run();
    }
    // The world layers, with the states Thestus sets: night activates DL_Night and unloads DL_Day;
    // day loads DL_Day and unloads DL_Night.
    Call manager(find(L"/Script/Engine.Default__WorldPartitionBlueprintLibrary"), L"GetDataLayerManager", 2);
    manager.set(L"WorldContextObject", pawn); manager.run();
    if (auto* layers = manager.get<UObject*>()) {
        const std::pair<const char*, uint8_t> states[] = {{"/Game/Sparta/Environments/Data_Layers/L_Core_World_01/DayNight/DL_Day.DL_Day", uint8_t(night ? 0 : 1)},
                                                          {"/Game/Sparta/Environments/Data_Layers/L_Core_World_01/DayNight/DL_Night.DL_Night", uint8_t(night ? 2 : 0)}};
        for (const auto& [asset, state] : states) {
            Call set(layers, L"SetDataLayerRuntimeState", 4);
            set.set(L"InDataLayerAsset", load(asset)); set.set(L"InState", state); set.set(L"bInIsRecursive", false); set.run();
        }
    }
    log(std::string("CCS world time set to ") + (night ? "night" : "day") + " from the Settings tab");
    return night ? "Night falls. Enemies that spawn from now on are night-strong unless Enemy difficulty says otherwise." : "Day returns.";
}
void Combat::remove_hook() {
    active_ = false;
    if (!token_) return;
    if (!deps_.hooks->remove(deps_.hooks->context, token_)) { retry_after_ = 0; return; }   // deferred: retried next tick
    token_ = 0; inputs_ = {}; function_ = {};
    log("CCS combat hook removed");
}
bool Combat::stop() {
    enabled_ = false; active_ = false;
    remove_hook();
    remove_effect_hook();
    if (token_ || effect_token_) return false;
    restore_weapon();
    for (auto& s : slots_) release(s);
    hold_cheat_ = false; sync_hold_cheat(PlayerContext{});   // a core going away takes its granted unlocks with it
    skeleton_ = {}; pawn_ = {}; asc_ = {};
    return true;
}
void Combat::tick(const PlayerContext& player, uint64_t now) {
    // Player identity for the callback's owner filter, refreshed twice a second.
    if (now >= player_check_) {
        player_check_ = now + 500;
        if (player.pawn != pawn_.get()) { pawn_ = {}; if (player.pawn) pawn_.capture(player.pawn); skeleton_ = {}; }
        if (player.asc != asc_.get()) { asc_ = {}; if (player.asc) asc_.capture(player.asc); }
        sync_hold_cheat(player);
        // Whether this character has the hold attack unlocks: the game's own charge check needs the tags.
        if (player.pawn) {
            try {
                for (int heavy = 0; heavy < 2; ++heavy) {
                    Call has(player.pawn, L"HasMatchingGameplayTag", 2);
                    has.set(L"TagToCheck", FName(heavy ? L"Character.Unlocked.HoldAttack.Heavy" : L"Character.Unlocked.HoldAttack.Light", FNAME_Add));
                    has.run();
                    const bool on = has.get<bool>();
                    if (on != hold_unlocked_[heavy]) { hold_unlocked_[heavy] = on; log(std::string("CCS ") + (heavy ? "heavy" : "light") + " hold attacks are " + (on ? "unlocked" : "locked") + " on this character"); }
                }
                if (!hold_check_logged_) {   // once: the same query on a tag every armed character carries, so a silent miss cannot pass as "locked"
                    hold_check_logged_ = true;
                    Call control(player.pawn, L"HasMatchingGameplayTag", 2);
                    control.set(L"TagToCheck", FName(L"Character.State.PrimaryWeapon", FNAME_Add)); control.run();
                    log(std::string("CCS hold unlock check: light ") + (hold_unlocked_[0] ? "unlocked" : "locked") + ", heavy " + (hold_unlocked_[1] ? "unlocked" : "locked")
                        + "; control tag Character.State.PrimaryWeapon " + (control.get<bool>() ? "present" : "absent"));
                }
            } catch (const std::exception& e) { if (!hold_check_warned_) { hold_check_warned_ = true; log(std::string("CCS hold unlock check unavailable: ") + e.what()); } }
        } else hold_unlocked_[0] = hold_unlocked_[1] = false;
        // The body worn now: a shell change, a CSS body, the Harbinger form. Swaps only happen on a human-family rig.
        try { const auto rig_now = rig::player_skeleton(player); if (rig_now != pawn_rig_) { pawn_rig_ = rig_now; pawn_humanoid_ = rig::humanoid(pawn_rig_); if (!pawn_humanoid_ && !pawn_rig_.empty()) log("CCS pawn rig is not human: " + rig::short_name(pawn_rig_) + "; swaps paused on it"); } } catch (...) {}
    }
    poll_weapon(player, now);
    poll_hidden(player, now);
    if (player.world && player.world != world_.get()) {   // a new world holds none of our references: everything reloads
        const bool had_world = world_.ptr != nullptr;
        world_ = {}; world_.capture(player.world);
        donor_ = {}; donor_armor_ = donor_turn_ = donor_cancel_ = -1; donor_failed_ = false;   // the donor rows live in the old world's references
        if (effect_token_) { for (auto* c : {night_damage_.get(), night_health_.get(), ngp_damage_.get()}) if (c) keep_referenced(player.world, c); }
        if (had_world) {
            log("CCS world changed; reloading the slots");
            montage_facts_.clear();
            for (auto& s : slots_) { s.rooted = s.show_rooted = s.ability_rooted = false; s.ability_class = {}; s.move_payload = {}; s.payload_gap = false; s.feel.clear(); s.carries.clear(); s.overlays.clear(); s.was_ready = false; if (!s.move_id.empty() && !s.path.empty()) { s.montage = {}; s.play = {}; s.play_kinds = 0; s.show_mesh = {}; s.pending = true; } }
        }
    }
    for (auto& s : slots_) {   // a loaded montage that stops answering: say why once, and load it again
        const bool ready = s.montage.alive();
        if (s.was_ready && !ready) { log("CCS slot lost its montage: " + s.move_id + ": " + s.montage.why_dead()); s.rooted = false; release_transplants(s); release_carries(s); release_overlays(s); s.pending = !s.path.empty(); }   // every cached copy was built from the lost montage
        s.was_ready = ready;
    }
    // The difficulty hook exists only while the setting overrides the game and a player is in a world.
    if (enabled_ && difficulty_ != "game" && player.pawn && player.world) {
        if (!effect_token_ && now >= effect_retry_after_) {
            try { ensure_effect_hook(); difficulty_error_.clear(); }
            catch (const std::exception& e) { if (difficulty_error_ != e.what()) log(std::string("CCS enemy difficulty hook failed: ") + e.what()); difficulty_error_ = e.what(); effect_retry_after_ = now + 5000; }
        }
    } else if (effect_token_) remove_effect_hook();
    bool tuned = false; for (const auto& s : slots_) if (std::abs(s.tuning.speed - 1.0) > 1e-6) tuned = true;
    const bool wanted = enabled_ && (assigned() > 0 || tuned);
    if (wanted) {
        load_pending(player);
        if (!token_ && now >= retry_after_) {
            try { ensure_hook(); error_.clear(); }
            catch (const std::exception& e) { if (error_ != e.what()) log(std::string("CCS combat hook failed: ") + e.what()); error_ = e.what(); retry_after_ = now + 2000; }
        }
        active_ = token_ != 0 && player.pawn != nullptr;
    } else {
        active_ = false;
        if (token_) remove_hook();
    }
}
// Why a montage-task call was left alone, once per ability class: the class, then its outer
// chain (instance, then owners) so a rejected owner shows what the game used instead of the pawn.
void Combat::note_skip(const char* why, UObject* ability) {
    if (!ability || !ability->GetClassPrivate() || noted_.size() >= 64) return;
    const auto key = name_key(ability->GetClassPrivate()->GetNamePrivate());
    if (!noted_.insert(key).second) return;
    std::string chain;
    auto* object = ability;
    for (unsigned depth = 0; object && depth < 6; ++depth, object = object->GetOuterPrivate())
        chain += (depth ? " < " : "") + narrow(object->GetNamePrivate().ToString()) + " (" + (object->GetClassPrivate() ? narrow(object->GetClassPrivate()->GetNamePrivate().ToString()) : std::string("?")) + ")";
    log(std::string("CCS attack skipped: ") + why + ": " + narrow(ability->GetClassPrivate()->GetNamePrivate().ToString()) + "; outers: " + chain
        + "; pawn " + (pawn_.get() ? narrow(pawn_.get()->GetNamePrivate().ToString()) : std::string("none")) + ", asc " + (asc_.get() ? narrow(asc_.get()->GetNamePrivate().ToString()) : std::string("none")));
}
// The cheat grants the same two effects the Acolyte's and Unwieldy Stones apply, on the player's
// ability component, and takes them off again when switched off. A new component (death, a new
// world) gets them again; the charge still costs Resolve like the game's own.
void Combat::sync_hold_cheat(const PlayerContext& player) {
    static const char* paths[] = {"/Game/Sparta/Core/Player/Upgrades/Effects/GE_Unlock_Attack_Hold_Light.GE_Unlock_Attack_Hold_Light_C",
                                  "/Game/Sparta/Core/Player/Upgrades/Effects/GE_Unlock_Attack_Hold_Heavy.GE_Unlock_Attack_Hold_Heavy_C"};
    auto* asc = player.asc;
    auto* holder = hold_grant_asc_.get();
    if (!hold_cheat_ || !asc) {
        if (!holder && !hold_grant_asc_.ptr) return;
        if (holder) for (auto& grant : hold_grant_) {
            if (grant.handle < 0) continue;
            try { Call remove(holder, L"RemoveActiveGameplayEffect", 3); remove.set(L"Handle", grant); remove.set(L"StacksToRemove", int32_t{-1}); remove.run(); }
            catch (const std::exception& e) { log(std::string("CCS charged attacks: unlock removal failed: ") + e.what()); }
        }
        hold_grant_asc_ = {}; hold_grant_[0] = {}; hold_grant_[1] = {};
        if (holder) log("CCS charged attacks: the granted unlocks were taken off again");
        return;
    }
    if (holder == asc) return;
    hold_grant_asc_ = {}; hold_grant_[0] = {}; hold_grant_[1] = {};
    try {
        for (int i = 0; i < 2; ++i) {
            auto* effect = load(paths[i]);
            if (player.world && hold_effect_world_.get() != player.world) { keep_referenced(player.world, effect); if (i == 1) { hold_effect_world_ = {}; hold_effect_world_.capture(player.world); } }
            Call context(asc, L"MakeEffectContext", 1); context.run();
            Call apply(asc, L"BP_ApplyGameplayEffectToSelf", 4);
            apply.set(L"GameplayEffectClass", static_cast<UObject*>(effect)); apply.set(L"Level", 1.0f);
            apply.copy(L"EffectContext", context, L"ReturnValue");
            apply.run();
            hold_grant_[i] = apply.get<EffectHandle>();
        }
        hold_grant_asc_.capture(asc);
        log("CCS charged attacks: light and heavy unlocks granted without Tarstones (cheat)");
    } catch (const std::exception& e) {
        hold_grant_[0] = {}; hold_grant_[1] = {};
        if (!hold_cheat_warned_) { hold_cheat_warned_ = true; log(std::string("CCS charged attacks cheat unavailable: ") + e.what()); }
    }
}
void Combat::note_recent(UObject* cls, int slot, const char* what) {
    static const uint64_t started = now_us();
    std::string line = std::to_string((now_us() - started) / 1000) + " ms  " + (cls ? narrow(cls->GetNamePrivate().ToString()) : std::string("?"));
    if (slot >= 0) line += " -> " + std::string(slot_to_string(SlotId(slot)));
    line += ": "; line += what;
    recent_.push_back(std::move(line));
    while (recent_.size() > 24) recent_.pop_front();
}
void Combat::callback(void* user, void*, void* frame, void*) noexcept {
    auto* self = static_cast<Combat*>(user);
    const auto started = now_us();
    try { self->observe(frame); }
    catch (...) { ++self->failures_; }
    const auto took = now_us() - started;
    if (took > self->maximum_us_) self->maximum_us_ = took;
}
void Combat::observe(void* frame_ptr) {
    ++seen_;
    if (!active_ || !function_.alive()) { ++skipped_; return; }
    auto* frame = static_cast<FFrame*>(frame_ptr);
    auto* locals = frame ? frame->Locals() : nullptr;
    if (!locals || frame->Node() != function_.get()) { ++wrong_frame_; return; }
    auto* bytes = static_cast<std::byte*>(static_cast<void*>(locals));
    auto* ability = static_cast<FObjectProperty*>(inputs_[0])->GetObjectPropertyValue(bytes + inputs_[0]->GetOffset_Internal());
    if (!player_outer(ability)) { ++skipped_; note_skip("owner is not the player", ability); return; }
    auto* cls = ability->GetClassPrivate();
    if (!cls) { ++skipped_; return; }
    const auto key = name_key(cls->GetNamePrivate());
    int slot = -1;
    if (auto it = class_slots_.find(key); it != class_slots_.end()) slot = it->second;
    else {
        slot = classify(narrow(cls->GetNamePrivate().ToString()));
        if (class_slots_.size() < max_cached_classes) class_slots_.emplace(key, int8_t(slot));
    }
    if (slot < 0) { ++skipped_; note_skip("class name has no slot", ability); note_recent(cls, slot, "no slot"); return; }
    if (!pawn_humanoid_) { ++skipped_; note_recent(cls, slot, "rig not humanoid"); return; }   // the Harbinger form or a creature shell: its rig cannot play these montages
    // Hold-first weapons play the charge stage on every press; a tap cuts out of it into step
    // L<n>/H<n>. When that step is an unarmed move with "Move's weapon", the weapon hides from
    // the first frame of the stage, provisionally: a charge that goes through shows it back.
    bool tap_hide = false;
    if (slot == int(SlotId::LC) || slot == int(SlotId::HC)) {
        if (const int step = hold_step(narrow(cls->GetNamePrivate().ToString()))) {
            const auto& stage = slots_[size_t(slot)];
            const auto& target = slots_[size_t(int(slot == int(SlotId::LC) ? SlotId::L1 : SlotId::H1) + step - 1)];
            const bool stage_hides = stage.montage.get() && stage.tuning.weapon == "move" && stage.unarmed;
            tap_hide = !stage_hides && target.montage.get() && target.tuning.weapon == "move" && target.unarmed;
        }
    }
    auto frame_montage = [&]() -> UObject* { return static_cast<FObjectProperty*>(inputs_[1])->GetObjectPropertyValue(bytes + inputs_[1]->GetOffset_Internal()); };
    auto provisional_hide = [&]() { if (!tap_hide) return; try { hide_weapons(frame_montage(), GetTickCount64(), 450); } catch (const std::exception& e) { ++failures_; log(std::string("CCS weapon hide failed: ") + e.what()); } };
    // A locked hold: the game's charge check fails as soon as its window opens and the normal
    // attack follows, so the game's own hold clip stays (it blends into that attack seamlessly).
    if ((slot == int(SlotId::LC) && !hold_unlocked_[0]) || (slot == int(SlotId::HC) && !hold_unlocked_[1])) { ++skipped_; note_recent(cls, slot, "hold attacks locked on this character, left alone"); provisional_hide(); return; }
    auto& s = slots_[size_t(slot)];
    bool changed = false, scaled = false;
    if (!s.montage.get()) {
        note_recent(cls, slot, s.move_id.empty() ? "nothing assigned" : "move not ready");
        if (s.move_id.empty() && noted_.size() < 64 && noted_.insert(key ^ 0x2545f4914f6cdd1dull).second)
            log("CCS attack seen: " + narrow(cls->GetNamePrivate().ToString()) + " is slot " + slot_to_string(SlotId(slot)) + ", nothing assigned, the game's own attack plays");
    }
    if (auto* source = s.montage.get()) {
        auto* replacement = s.play.get() ? s.play.get() : source;   // move feel plays the cleaned copy when there is one
        auto* original = static_cast<FObjectProperty*>(inputs_[1])->GetObjectPropertyValue(bytes + inputs_[1]->GetOffset_Internal());
        // Some abilities play companion clips around the attack (the Axatana heavy plays the axe
        // transform before its hold and cut). Those stay the game's; the attack clip is swapped.
        // The exact attack montage varies with the shell the body wears, so it is not matched by name.
        const MontageFacts* facts = original ? &montage_facts(original) : nullptr;
        if (facts && facts->companion) {
            ++skipped_; note_recent(cls, slot, "companion clip left alone"); provisional_hide();
            if (noted_.size() < 64 && noted_.insert(key ^ 0x9e3779b97f4a7c15ull).second) log("CCS attack left alone: " + narrow(cls->GetNamePrivate().ToString()) + " played its companion clip " + narrow(original->GetNamePrivate().ToString()));
            return;
        }
        UObject* pointer = replacement;
        if (s.tuning.feel == "game" && original && original != source) {
            // The game's feel: the slot's own montage keeps its input windows, locks, sounds and hit
            // payload; only the animation inside it is the move's, fitted to the original timing.
            try { if (auto* clone = transplant(s, original, source)) pointer = clone; }
            catch (const std::exception& e) { ++failures_; if (!s.feel_warned) { s.feel_warned = true; log("CCS game feel unavailable for " + s.move_id + ": " + e.what() + "; playing the move's own montage"); } }
        } else if (original && original != replacement) {
            // The move's own feel keeps the windows the weapon authors for the player: without the
            // original's hold handler a long press could never charge (and on hold-first weapons
            // never fall back to the cut); without its turn window the stick could not steer the
            // wind-up onto a moving enemy.
            // Weapon-state notifies come along too (without the Axatana's transform notify a light
            // attack after a heavy would play with the joined axe still in hand), and the elemental
            // Tarstone triggers that follow the weapon's hits.
            const unsigned want = facts ? (facts->kinds & ~s.play_kinds) : 0u;
            const bool want_hold = want & bit(RowKind::Hold);
            if (want) {
                try { if (auto* carried = carry_windows(s, original, replacement, want)) pointer = carried; }
                catch (const std::exception& e) {
                    ++failures_;
                    if (!s.carry_warned) { s.carry_warned = true; log("CCS windows unavailable for " + s.move_id + ": " + e.what() + (want_hold ? "; long presses will not charge this move" : "; the stick will not steer this move")); }
                }
            }
            if (s.tuning.hit_damage == "weapon") {
                try { apply_weapon_payload(s, original, replacement, ability); }
                catch (const std::exception& e) { ++failures_; if (s.error.empty()) { s.error = std::string("Hit payload copy failed: ") + e.what(); log("CCS " + s.error); } }
            }
        }
        const char* how = pointer == replacement ? "swapped" : (s.tuning.feel == "game" ? "swapped, game feel" : "swapped, windows carried");
        // The player-feel overlay: hyper armor over the whole swing, and with "Move's own" feel
        // the weapon's steering and movement cancel ("Game's" feel already carries the weapon's).
        // and, for a player move with "Damage: Move's own", its own ability payload on hit windows
        // that carry none (the playing ability's payload would otherwise hit with the slot weapon's).
        const bool donor = donor_ready();   // the donor loads from the tick; a swing before that plays without armor or steering
        const bool armor = donor && s.tuning.armor == "full", steer = donor && s.tuning.steer == "full" && s.tuning.feel != "game";
        auto* move_payload = s.tuning.feel != "game" && s.tuning.hit_damage == "move" && s.payload_gap ? s.move_payload.get() : nullptr;
        if (armor || steer || move_payload) {
            try { pointer = overlaid(s, pointer, armor, steer, move_payload); }
            catch (const std::exception& e) { ++failures_; if (!s.overlay_warned) { s.overlay_warned = true; log("CCS player feel unavailable for " + s.move_id + ": " + e.what()); } }
        }
        // Speed on a copy of ours goes into the montage's own RateScale, which multiplies whatever
        // play rate the game sets later: the charge handler resets the rate on release, so a rate
        // passed to the task alone was lost on the LC and HC slots. The game's own asset is never
        // touched; there the rate parameter carries the speed instead.
        if (pointer != source && pointer != original) {
            try {
                const auto& l = notify_layout();
                if (l.rate_scale) {
                    auto* base_owner = (s.tuning.feel == "game" && original) ? original : source;
                    float base{}; std::memcpy(&base, reinterpret_cast<std::byte*>(base_owner) + l.rate_scale->GetOffset_Internal(), sizeof(base));
                    const float value = (std::isfinite(base) && base > 0.f ? base : 1.f) * float(s.tuning.speed);
                    std::memcpy(reinterpret_cast<std::byte*>(pointer) + l.rate_scale->GetOffset_Internal(), &value, sizeof(value));
                    scaled = true;
                }
            } catch (...) {}
        }
        std::memcpy(bytes + inputs_[1]->GetOffset_Internal(), &pointer, sizeof(pointer));
        ++s.hits; changed = true;
        note_recent(cls, slot, how);
        if (noted_.size() < 64 && noted_.insert(key ^ 0x51ed270b9d1c3a7full).second)
            log("CCS swap: " + narrow(cls->GetNamePrivate().ToString()) + " is slot " + slot_to_string(SlotId(slot)) + " -> " + s.move_id + " (" + how + (armor ? ", hyper armor" : "") + (steer ? ", steering" : "") + ")"
                + "; feel " + s.tuning.feel + ", damage " + s.tuning.hit_damage + ", visual " + s.tuning.weapon + ", armor " + s.tuning.armor + ", steer " + s.tuning.steer + ", speed " + std::to_string(s.tuning.speed) + ", rig " + rig::short_name(pawn_rig_));
        if (s.tuning.weapon == "move") {
            if (auto* mesh = s.show_mesh.get()) { try { show_weapon(mesh, pointer, GetTickCount64()); } catch (const std::exception& e) { ++failures_; log(std::string("CCS weapon show failed: ") + e.what()); } }
            else if (s.unarmed) { try { hide_weapons(pointer, GetTickCount64()); } catch (const std::exception& e) { ++failures_; log(std::string("CCS weapon hide failed: ") + e.what()); } }
        }
    }
    if (!scaled && std::abs(s.tuning.speed - 1.0) > 1e-6) {
        float rate{}; std::memcpy(&rate, bytes + inputs_[2]->GetOffset_Internal(), sizeof(rate));
        if (std::isfinite(rate) && rate > 0.f) { rate *= float(s.tuning.speed); std::memcpy(bytes + inputs_[2]->GetOffset_Internal(), &rate, sizeof(rate)); changed = true; }
    }
    provisional_hide();
    if (changed) ++swapped_; else ++skipped_;
}
nlohmann::json Combat::status() const {
    nlohmann::json slots = nlohmann::json::array();
    for (size_t i = 0; i < slots_.size(); ++i) {
        const auto& s = slots_[i];
        slots.push_back({{"slot", slot_to_string(SlotId(i))}, {"move", s.move_id}, {"ready", s.montage.alive()}, {"pending", s.pending}, {"error", s.error}, {"hits", s.hits},
            {"speed", s.tuning.speed}, {"feel", s.tuning.feel}, {"hit_damage", s.tuning.hit_damage}, {"weapon", s.tuning.weapon}, {"weapon_mesh", s.show_mesh.alive()}, {"payload_copied", !s.backups.empty()}, {"clones", s.feel.size()}});
    }
    CcsHookStats stats{}; stats.size = sizeof(stats);
    nlohmann::json host = {{"available", false}};
    if (deps_.hooks->statistics(deps_.hooks->context, &stats))
        host = {{"available", true}, {"slots", stats.slots}, {"calls", stats.calls}, {"wrong_thread", stats.wrong_thread}, {"failures", stats.failures}};
    return {{"enabled", enabled_}, {"active", active_}, {"hooked", token_ != 0}, {"seen", seen_}, {"swapped", swapped_},
        {"skipped", skipped_}, {"failures", failures_}, {"wrong_frame", wrong_frame_}, {"maximum_callback_us", maximum_us_}, {"recent", recent_},
        {"hold_unlocked", {{"light", hold_unlocked_[0]}, {"heavy", hold_unlocked_[1]}, {"cheat", hold_cheat_}, {"granted", hold_grant_asc_.alive()}}},
        {"enemy_difficulty", {{"mode", difficulty_}, {"hooked", effect_token_ != 0}, {"night_pairs_added", difficulty_added_}, {"night_effects_removed", difficulty_removed_}, {"error", difficulty_error_}}},
        {"cached_classes", class_slots_.size()}, {"error", error_}, {"slots", std::move(slots)}, {"host", std::move(host)}};
}
}

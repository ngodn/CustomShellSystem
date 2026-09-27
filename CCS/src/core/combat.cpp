#include "combat.hpp"
#include <windows.h>
#include "rig.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <Unreal/FFrame.hpp>
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
int Combat::classify(const std::string& full) {
    std::string name = full;
    if (ends_with(name, "_C")) name.resize(name.size() - 2);
    // Sidearm primary fire: GA_<Sidearm>Attack_Primary and the shared ranged attack bases.
    if (name.starts_with("GA_") && !name.starts_with("GA_Player") &&
        (ends_with(name, "Attack_Primary") || ends_with(name, "Attack_Primary_InfiniteAmmo") || name == "GA_SidearmRangedAttackBase" ||
         name == "GA_SidearmRangedBurstAttackBase" || name == "GA_SidearmRangedChargedAttackBase")) return int(SlotId::R);
    if (!full.starts_with("GA_Player")) return -1;
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
    s.tuning = tuning;
}
// The visible mesh of each move source's weapon: player primaries are WP_<X> blueprints whose
// SM_Weapon static mesh component holds these assets (grip pivot baked in, identity transform);
// enemy weapons are the static meshes the icon renders came from. Skeletal enemy weapons are left out.
std::string Combat::weapon_mesh_path(const std::string& source) {
    static const std::pair<const char*, const char*> meshes[] = {
        {"HeavyHammer", "/Game/Sparta/Weapons/Player/HeavyHammer/Art/Mesh/SM_ObsidianHammer_01"},
        {"Axatana", "/Game/Sparta/Weapons/Player/Axatana/Katanas/Katanas/SM_Axatana_Axe_Offset"},
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
    if (shown_montage_.get() && shown_montage_.get() == slot.montage.get()) restore_weapon();
    if (slot.rooted) { if (auto* montage = slot.montage.get()) montage->ClearRootSet(); slot.rooted = false; }
    if (slot.show_rooted) { if (auto* mesh = slot.show_mesh.get()) mesh->ClearRootSet(); slot.show_rooted = false; }
    slot.montage = {}; slot.show_mesh = {};
}
// ---- hit payload: every hit-check notify inside a montage owns its payload object (multiplier,
// poise, break, reaction, effects). "Weapon's own" copies the slot's original payload fields onto
// the replacement's payloads and keeps a backup for restoring.
namespace {
const wchar_t* payload_fields[] = {L"HealthDamage", L"PoiseDamageOption", L"BreakDamageOption", L"DamageEffect", L"AdditionalEffects", L"DamageDataTag", L"ReactionTag",
    L"StrikeDirection", L"PoiseDamage", L"PoiseDamageEffect", L"BreakDamage", L"BreakDamageEffect", L"PoiseBrokenEffect", L"PoiseDamageDataTag", L"BreakDamageDataTag",
    L"PayloadTags", L"CustomElementalStacks"};
}
std::vector<UObject*> Combat::hit_payloads(UObject* montage) const {
    std::vector<UObject*> out;
    if (!montage) return out;
    auto* p = montage->GetPropertyByNameInChain(L"Notifies");
    if (!p || !p->IsA<FArrayProperty>()) return out;
    auto* array = static_cast<FArrayProperty*>(p); auto* inner = array->GetInner();
    if (!inner->IsA<FStructProperty>()) return out;
    auto* row = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    auto* notify = row ? row->GetPropertyByNameInChain(L"Notify") : nullptr;
    auto* state = row ? row->GetPropertyByNameInChain(L"NotifyStateClass") : nullptr;
    if (!notify || !state || !notify->IsA<FObjectProperty>() || !state->IsA<FObjectProperty>()) return out;
    auto* hit_state = static_cast<UClass*>(find_cached(L"/Script/Sparta.SpartaAnimNotifyState_HitCheck"));
    auto* hit_notify = static_cast<UClass*>(find_cached(L"/Script/Sparta.SpartaAnimNotify_HitCheck"));
    FScriptArrayHelper rows(array, reinterpret_cast<std::byte*>(montage) + p->GetOffset_Internal());
    const int count = std::min(rows.Num(), 256);
    for (int i = 0; i < count; ++i) {
        for (auto* field : {notify, state}) {
            UObject* object{}; std::memcpy(&object, rows.GetRawPtr(i) + field->GetOffset_Internal(), sizeof(object));
            if (!object || !((hit_state && object->IsA(hit_state)) || (hit_notify && object->IsA(hit_notify)))) continue;
            if (auto* payload = object_of(object, L"DamagePayload")) out.push_back(payload);
        }
    }
    return out;
}
void Combat::apply_weapon_payload(Slot& s, UObject* original, UObject* replacement) {
    if (s.payload_source.get() == original) return;   // already carrying this weapon's payload
    restore_payload(s);
    const auto sources = hit_payloads(original);
    if (sources.empty()) return;
    auto* from = sources.front();
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
        shown_component_.capture(component); shown_montage_ = {}; shown_montage_.capture(montage); shown_since_ = now; shown_seen_playing_ = false;
    }
    Call set(component, L"SetStaticMesh", 2); set.set(L"NewMesh", mesh); set.run();
}
void Combat::restore_weapon() {
    auto* component = shown_component_.get();
    if (component) {
        try {
            Call set(component, L"SetStaticMesh", 2); set.set(L"NewMesh", shown_original_.get()); set.run();
            for (size_t i = 0; i < shown_materials_.size(); ++i) if (auto* m = shown_materials_[i].get()) { Call mat(component, L"SetMaterial", 2); mat.set(L"ElementIndex", int32_t(i)); mat.set(L"Material", m); mat.run(); }
        } catch (const std::exception& e) { log(std::string("CCS weapon restore failed: ") + e.what()); }
    }
    shown_component_ = {}; shown_original_ = {}; shown_montage_ = {}; shown_materials_.clear(); shown_seen_playing_ = false;
}
void Combat::poll_weapon(const PlayerContext& player, uint64_t now) {
    if (!shown_component_.ptr) return;
    auto* montage = shown_montage_.get();
    if (!shown_component_.alive() || !player.pawn || player.pawn != pawn_.get() || !montage) { restore_weapon(); return; }
    try {
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
            if (!montage->IsRootSet()) { montage->SetRootSet(); s.rooted = true; }
            s.montage.capture(montage);
            if (!s.show_mesh_path.empty()) {
                try {
                    auto* mesh = load(s.show_mesh_path);
                    if (!mesh->IsA(static_cast<UClass*>(find_cached(L"/Script/Engine.StaticMesh")))) throw std::runtime_error("Not a static mesh");
                    if (!mesh->IsRootSet()) { mesh->SetRootSet(); s.show_rooted = true; }
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
    // Diagnostic counters on the two audio paths a swing takes (weapon whoosh, character vocal),
    // so a silent swapped swing can be told from an unprimed sound bank.
    auto count = [&](const wchar_t* cdo, const wchar_t* name, CcsNativePreHook fn, uint64_t& token) {
        try {
            auto* object = find(cdo); if (!object) return;
            auto* target = object->GetFunctionByNameInChain(name);
            if (!target || !target->HasAnyFunctionFlags(FUNC_Native)) return;
            token = deps_.hooks->add_native_pre(deps_.hooks->context, target, fn, this);
        } catch (...) {}
    };
    count(L"/Script/Sparta.Default__SpartaWeaponComponent_Melee", L"StartWhooshFX", count_whoosh, whoosh_token_);
    count(L"/Script/Sparta.Default__SpartaCharacterVoxComponent", L"OnCharacterAttack", count_vox, vox_token_);
}
void Combat::remove_hook() {
    active_ = false;
    if (!token_) return;
    if (!deps_.hooks->remove(deps_.hooks->context, token_)) { retry_after_ = 0; return; }   // deferred: retried next tick
    token_ = 0; inputs_ = {}; function_ = {};
    for (auto* token : {&whoosh_token_, &vox_token_}) if (*token && deps_.hooks->remove(deps_.hooks->context, *token)) *token = 0;
    log("CCS combat hook removed");
}
bool Combat::stop() {
    enabled_ = false; active_ = false;
    remove_hook();
    if (token_ || whoosh_token_ || vox_token_) return false;
    restore_weapon();
    for (auto& s : slots_) release(s);
    skeleton_ = {}; pawn_ = {}; asc_ = {};
    return true;
}
void Combat::tick(const PlayerContext& player, uint64_t now) {
    // Player identity for the callback's owner filter, refreshed twice a second.
    if (now >= player_check_) {
        player_check_ = now + 500;
        if (player.pawn != pawn_.get()) { pawn_ = {}; if (player.pawn) pawn_.capture(player.pawn); skeleton_ = {}; }
        if (player.asc != asc_.get()) { asc_ = {}; if (player.asc) asc_.capture(player.asc); }
    }
    poll_weapon(player, now);
    bool tuned = false; for (const auto& s : slots_) if (std::abs(s.tuning.speed - 1.0) > 1e-6) tuned = true;
    const bool wanted = enabled_ && (assigned() > 0 || tuned);
    if (wanted) {
        load_pending(player);
        if (!token_ && now >= retry_after_) {
            try { ensure_hook(); error_.clear(); }
            catch (const std::exception& e) { error_ = e.what(); retry_after_ = now + 2000; log(std::string("CCS combat hook failed: ") + e.what()); }
        }
        active_ = token_ != 0 && player.pawn != nullptr;
    } else {
        active_ = false;
        if (token_) remove_hook();
    }
}
void Combat::count_whoosh(void* user, void*, void*, void*) noexcept { ++static_cast<Combat*>(user)->whoosh_; }
void Combat::count_vox(void* user, void*, void*, void*) noexcept { ++static_cast<Combat*>(user)->vox_; }
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
    if (!player_outer(ability)) { ++skipped_; return; }
    auto* cls = ability->GetClassPrivate();
    if (!cls) { ++skipped_; return; }
    const auto key = name_key(cls->GetNamePrivate());
    int slot = -1;
    if (auto it = class_slots_.find(key); it != class_slots_.end()) slot = it->second;
    else {
        slot = classify(narrow(cls->GetNamePrivate().ToString()));
        if (class_slots_.size() < max_cached_classes) class_slots_.emplace(key, int8_t(slot));
    }
    if (slot < 0) { ++skipped_; return; }
    auto& s = slots_[size_t(slot)];
    bool changed = false;
    if (auto* replacement = s.montage.get()) {
        auto* original = static_cast<FObjectProperty*>(inputs_[1])->GetObjectPropertyValue(bytes + inputs_[1]->GetOffset_Internal());
        UObject* pointer = replacement;
        std::memcpy(bytes + inputs_[1]->GetOffset_Internal(), &pointer, sizeof(pointer));
        ++s.hits; changed = true;
        if (s.tuning.hit_damage == "weapon" && original && original != replacement) {
            try { apply_weapon_payload(s, original, replacement); }
            catch (const std::exception& e) { ++failures_; if (s.error.empty()) { s.error = std::string("Hit payload copy failed: ") + e.what(); log("CCS " + s.error); } }
        }
        if (s.tuning.weapon == "move") {
            if (auto* mesh = s.show_mesh.get()) { try { show_weapon(mesh, replacement, GetTickCount64()); } catch (const std::exception& e) { ++failures_; log(std::string("CCS weapon show failed: ") + e.what()); } }
        }
    }
    if (std::abs(s.tuning.speed - 1.0) > 1e-6) {
        float rate{}; std::memcpy(&rate, bytes + inputs_[2]->GetOffset_Internal(), sizeof(rate));
        if (std::isfinite(rate) && rate > 0.f) { rate *= float(s.tuning.speed); std::memcpy(bytes + inputs_[2]->GetOffset_Internal(), &rate, sizeof(rate)); changed = true; }
    }
    if (changed) ++swapped_; else ++skipped_;
}
nlohmann::json Combat::status() const {
    nlohmann::json slots = nlohmann::json::array();
    for (size_t i = 0; i < slots_.size(); ++i) {
        const auto& s = slots_[i];
        slots.push_back({{"slot", slot_to_string(SlotId(i))}, {"move", s.move_id}, {"ready", s.montage.alive()}, {"pending", s.pending}, {"error", s.error}, {"hits", s.hits},
            {"speed", s.tuning.speed}, {"hit_damage", s.tuning.hit_damage}, {"weapon", s.tuning.weapon}, {"weapon_mesh", s.show_mesh.alive()}, {"payload_copied", !s.backups.empty()}});
    }
    CcsHookStats stats{}; stats.size = sizeof(stats);
    nlohmann::json host = {{"available", false}};
    if (deps_.hooks->statistics(deps_.hooks->context, &stats))
        host = {{"available", true}, {"slots", stats.slots}, {"calls", stats.calls}, {"wrong_thread", stats.wrong_thread}, {"failures", stats.failures}};
    return {{"enabled", enabled_}, {"active", active_}, {"hooked", token_ != 0}, {"whoosh_calls", whoosh_}, {"vox_calls", vox_}, {"seen", seen_}, {"swapped", swapped_},
        {"skipped", skipped_}, {"failures", failures_}, {"wrong_frame", wrong_frame_}, {"maximum_callback_us", maximum_us_},
        {"cached_classes", class_slots_.size()}, {"error", error_}, {"slots", std::move(slots)}, {"host", std::move(host)}};
}
}

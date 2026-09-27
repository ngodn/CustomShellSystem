#include "discovery.hpp"
#include "rig.hpp"
#include <chrono>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>

namespace ccs {
using namespace engine;
namespace {
constexpr int32_t rows_per_tick = 400;
constexpr size_t max_found = 2048;
// Content roots scanned in order: the game's characters, then whatever CSS packages cooked.
constexpr const wchar_t* roots[] = {L"/Game/Sparta/Characters", L"/Game/CSS"};
constexpr size_t root_count = sizeof(roots) / sizeof(roots[0]);
uint64_t now_ms() { return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
bool attack_like(const std::string& text) {
    static const char* words[] = {"Attack", "Swipe", "Slash", "Stab", "Thrust", "Slam", "Smash", "Swing", "Combo", "Lunge", "Spin", "Kick", "Punch", "Cleave", "Overhead", "Frenzy", "Finisher", "Hold"};
    for (const char* w : words) if (text.find(w) != std::string::npos) return true;
    return false;
}
bool reaction_like(const std::string& text) {
    static const char* words[] = {"HitReact", "Death", "Stagger", "Dodge", "Taunt", "Idle", "Spawn", "Flinch", "Knock", "Stun", "Parr", "Block", "Guard", "Evade", "Roll", "Riposte", "Cine", "Execut"};
    for (const char* w : words) if (text.find(w) != std::string::npos) return true;
    return false;
}
}
void Discovery::begin(const PlayerContext& player) {
    if (!player.pc || path_index_ >= root_count) return;
    // UAssetRegistryHelpers::GetAssetRegistry returns a TScriptInterface: the object pointer comes first.
    auto* helpers = find_cached(L"/Script/AssetRegistry.Default__AssetRegistryHelpers");
    Call get(helpers, L"GetAssetRegistry", 1); get.run();
    auto* ret = get.param(L"ReturnValue");
    if (ret->GetElementSize() < int(sizeof(UObject*))) throw std::runtime_error("Asset registry interface layout changed");
    UObject* registry{}; std::memcpy(&registry, get.data(ret), sizeof(registry));
    if (!registry) throw std::runtime_error("Asset registry is unavailable");
    query_ = std::make_unique<Call>(registry, L"GetAssetsByPath", 5);
    out_ = query_->param(L"OutAssetData");
    if (!out_->IsA<FArrayProperty>()) throw std::runtime_error("Asset registry output is not an array");
    auto* inner = static_cast<FArrayProperty*>(out_)->GetInner();
    if (!inner->IsA<FStructProperty>()) throw std::runtime_error("Asset registry rows are not structs");
    auto* row = static_cast<FStructProperty*>(inner)->GetStruct().Get();
    if (!row || narrow(row->GetPathName()) != "/Script/CoreUObject.AssetData") throw std::runtime_error("Asset registry row type changed");
    element_ = inner->GetElementSize();
    auto need = [&](const wchar_t* name, size_t size) {
        auto* p = row->GetPropertyByNameInChain(name);
        if (!p || p->GetOffset_Internal() < 0 || p->GetOffset_Internal() + int(size) > element_) throw std::runtime_error("Asset data layout changed: " + narrow(name));
        return p;
    };
    package_name_ = need(L"PackageName", sizeof(FName)); asset_name_ = need(L"AssetName", sizeof(FName)); class_path_ = need(L"AssetClassPath", 2 * sizeof(FName));
    if (!package_name_->IsA<FNameProperty>() || !asset_name_->IsA<FNameProperty>()) throw std::runtime_error("Asset data name fields changed");
    // Every animation asset carries its Skeleton as a registry tag (UAnimationAsset::Skeleton is
    // AssetRegistrySearchable; USkeleton::CollectAnimationNotifies reads the same "Skeleton" tag).
    // Its value is the skeleton's export text name, so the rig is known without loading anything.
    tag_ = std::make_unique<Call>(helpers, L"GetTagValue", 4);
    tag_asset_ = tag_->param(L"InAssetData"); tag_out_ = tag_->param(L"OutTagValue");
    if (!tag_asset_->IsA<FStructProperty>() || static_cast<FStructProperty*>(tag_asset_)->GetStruct().Get() != row || tag_asset_->GetElementSize() != element_)
        throw std::runtime_error("Asset registry tag lookup takes a different row type");
    if (!tag_out_->IsA<FStrProperty>() || tag_out_->GetElementSize() != int(sizeof(FString))) throw std::runtime_error("Asset registry tag value layout changed");
    tag_->set(L"InTagName", FName(L"Skeleton", FNAME_Add));
    query_->set(L"PackagePath", FName(roots[path_index_], FNAME_Add));
    query_->set(L"bRecursive", true);
    query_->set(L"bIncludeOnlyOnDiskAssets", true);
    query_->run();
    // The game's own root must answer; an absent CSS root is simply empty.
    if (!query_->get<bool>() && path_index_ == 0) throw std::runtime_error("Asset registry query failed");
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(out_), query_->data(out_));
    count_ = rows.Num(); next_ = 0;
    if (count_ < 0 || count_ > 200000) throw std::runtime_error("Asset registry result count out of bounds");
    state_ = State::Scanning;
}
// The rig a montage was authored on, from its registry tag: "/Script/Engine.Skeleton'/Game/.../
// SKEL_Human_Skeleton.SKEL_Human_Skeleton'" gives the object path inside the quotes. Empty when untagged.
std::string Discovery::rig_of(const uint8_t* row) {
    tag_asset_->CopyCompleteValue(tag_->data(tag_asset_), row);
    tag_->run();
    if (!tag_->get<bool>()) return {};
    const auto& chars = static_cast<const FString*>(tag_->data(tag_out_))->GetCharArray();
    if (chars.Num() < 2 || chars.Num() > 1024 || !chars.GetData() || chars.GetData()[chars.Num() - 1] != L'\0') return {};
    auto text = narrow(std::wstring(chars.GetData(), static_cast<size_t>(chars.Num() - 1)));
    while (!text.empty() && text.back() == '\'') text.pop_back();
    const auto quote = text.find('\'');
    return quote == std::string::npos ? text : text.substr(quote + 1);
}
void Discovery::scan() {
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(out_), query_->data(out_));
    if (rows.Num() != count_) throw std::runtime_error("Asset registry result changed under the scan");
    const int32_t end = std::min(count_, next_ + rows_per_tick);
    for (; next_ < end; ++next_) {
        const auto* bytes = rows.GetRawPtr(next_);
        ++seen_;
        FName class_asset{}; std::memcpy(&class_asset, bytes + class_path_->GetOffset_Internal() + sizeof(FName), sizeof(FName));
        if (narrow(class_asset.ToString()) != "AnimMontage") continue;
        FName package{}, asset{};
        std::memcpy(&package, bytes + package_name_->GetOffset_Internal(), sizeof(FName));
        std::memcpy(&asset, bytes + asset_name_->GetOffset_Internal(), sizeof(FName));
        const auto name = narrow(asset.ToString()), pkg = narrow(package.ToString());
        const auto path = pkg + "." + name;
        present_.insert(path);   // every montage the game lists, so a catalog entry is only "missing" when it truly is
        if (!attack_like(pkg + name) || reaction_like(name)) continue;
        if (const auto rig = rig_of(bytes); rig.empty()) ++untagged_;
        else if (!rig::compatible(rig, player_rig_)) { ++rig_skipped_; continue; }   // would never play on the player
        if (found_.size() >= max_found) continue;
        Found f; f.name = name; f.path = path; f.player = pkg.find("/Characters/Shells/") != std::string::npos;
        const auto marker = f.player ? std::string("/Shells/") : std::string("/Enemies/");
        if (const auto at = pkg.find(marker); at != std::string::npos) { const auto rest = pkg.substr(at + marker.size()); f.source = rest.substr(0, rest.find('/')); }
        if (f.source.empty()) { const auto at = pkg.find("/Bosses/"); if (at != std::string::npos) { const auto rest = pkg.substr(at + 8); f.source = "Boss " + rest.substr(0, rest.find('/')); } }
        found_.push_back(std::move(f));
    }
    if (next_ >= count_) {
        path_rows_.emplace_back(narrow(roots[path_index_]), count_);
        query_.reset(); tag_.reset(); out_ = tag_asset_ = tag_out_ = nullptr;
        state_ = ++path_index_ < root_count ? State::Querying : State::Done;
    }
}
void Discovery::tick(const PlayerContext& player) {
    const auto now = now_ms();
    try {
        if (state_ == State::Idle || (state_ == State::Failed && now >= retry_after_)) {
            if (!player.pc || !player.pawn) return;
            player_rig_ = rig::player_skeleton(player);
            path_index_ = 0; path_rows_.clear(); rig_skipped_ = untagged_ = 0;
            state_ = State::Querying;
        }
        if (state_ == State::Querying) begin(player);
        if (state_ == State::Scanning) scan();
    } catch (const std::exception& e) {
        error_ = e.what(); state_ = State::Failed; query_.reset(); tag_.reset(); out_ = tag_asset_ = tag_out_ = nullptr; retry_after_ = now + 30000;
    }
}
}

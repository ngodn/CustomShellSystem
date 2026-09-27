#include "discovery.hpp"
#include <chrono>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>

namespace ccs {
using namespace engine;
namespace {
constexpr int32_t rows_per_tick = 400;
constexpr size_t max_found = 2048;
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
    if (!player.pc) return;
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
    query_->set(L"PackagePath", FName(L"/Game/Sparta/Characters", FNAME_Add));
    query_->set(L"bRecursive", true);
    query_->set(L"bIncludeOnlyOnDiskAssets", true);
    query_->run();
    if (!query_->get<bool>()) throw std::runtime_error("Asset registry query failed");
    FScriptArrayHelper rows(static_cast<FArrayProperty*>(out_), query_->data(out_));
    count_ = rows.Num(); next_ = 0;
    if (count_ < 0 || count_ > 200000) throw std::runtime_error("Asset registry result count out of bounds");
    state_ = State::Scanning;
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
        if (found_.size() >= max_found) continue;
        Found f; f.name = name; f.path = path; f.player = pkg.find("/Characters/Shells/") != std::string::npos;
        const auto marker = f.player ? std::string("/Shells/") : std::string("/Enemies/");
        if (const auto at = pkg.find(marker); at != std::string::npos) { const auto rest = pkg.substr(at + marker.size()); f.source = rest.substr(0, rest.find('/')); }
        if (f.source.empty()) { const auto at = pkg.find("/Bosses/"); if (at != std::string::npos) { const auto rest = pkg.substr(at + 8); f.source = "Boss " + rest.substr(0, rest.find('/')); } }
        found_.push_back(std::move(f));
    }
    if (next_ >= count_) { query_.reset(); out_ = nullptr; state_ = State::Done; }
}
void Discovery::tick(const PlayerContext& player) {
    const auto now = now_ms();
    try {
        if (state_ == State::Idle || (state_ == State::Failed && now >= retry_after_)) {
            if (!player.pc || !player.pawn) return;
            state_ = State::Querying; begin(player);
        }
        if (state_ == State::Scanning) scan();
    } catch (const std::exception& e) {
        error_ = e.what(); state_ = State::Failed; query_.reset(); out_ = nullptr; retry_after_ = now + 30000;
    }
}
}

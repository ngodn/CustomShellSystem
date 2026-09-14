#include "prologue_recovery.hpp"
using cssx::Json;
namespace cheat {
uint64_t PrologueRecovery::id(const Json& v) {return v.is_object()?v.value("$object",uint64_t{}):0;}
namespace {
bool class_is(const Json& object,const char* name) {
    return object.is_object() && object.value("class",std::string{}).ends_with(std::string(".")+name);
}
}
void PrologueRecovery::start() {
    const auto player=host_.player();
    const auto pawn=id(player.value("pawn",Json())),controller=id(player.value("controller",Json()));
    if(!pawn || !controller) throw std::runtime_error("Enter the world before checking the intro lock.");
    if(pawn_!=pawn) attempted_.clear();
    pawn_=pawn;controller_=controller;ability_=0;observations_=0;elapsed_=0;running_=true;
    message_="Checking for the known completed-intro lock.";
}
void PrologueRecovery::cancel() {running_=false;ability_=0;observations_=0;elapsed_=0;}
Json PrologueRecovery::inspect(const Json& pawn) {
    auto asc=host_.get(pawn,"AbilitySystemComponent");
    for(size_t i=0;i<tags.size();++i) {
        const auto count=host_.call(asc,"GetGameplayTagCount",Json::array({{{"TagName",tags[i]}}})).get<int>();
        if((i<2 && count!=1) || (i>=2 && count!=0 && count!=1)) return nullptr;
    }
    const auto container=host_.get(asc,"ActivatableAbilities");const auto& specs=container.at("Items");
    if(!specs.is_array() || specs.size()>256) return nullptr;
    Json match;unsigned matches=0;
    for(const auto& spec:specs) {
        if(spec.at("ActiveCount").get<int>()<=0 || !class_is(spec.at("Ability"),"GA_Player_Prologue_EggStrandingCustom_C")) continue;
        for(const auto* field:{"NonReplicatedInstances","ReplicatedInstances"}) {
            const auto& instances=spec.at(field);if(!instances.is_array() || instances.size()>8) return nullptr;
            for(const auto& instance:instances) if(id(instance)) {match=instance;if(++matches>1) return nullptr;}
        }
    }
    if(matches!=1 || !class_is(match,"GA_Player_Prologue_EggStrandingCustom_C")) return nullptr;
    if(host_.call(match,"HasPlayedGetUp")!=true || host_.call(match,"IsMapUnlocked")!=true) return nullptr;
    const auto lib=host_.find("/Script/GameplayAbilities.Default__AbilitySystemBlueprintLibrary");
    const auto effect=host_.call(lib,"GetGameplayEffectFromActiveEffectHandle",Json::array({host_.get(match,"WeaponPutInHandBlock")}));
    if(!class_is(effect,"GE_State_Block_Weapon_PutInHand_Primary_C")) return nullptr;
    const auto mesh=host_.get(pawn,"Mesh"),anim=host_.call(mesh,"GetAnimInstance");
    if(!id(anim) || id(host_.call(anim,"GetCurrentActiveMontage"))) return nullptr;
    return {{"ability",match},{"asc",asc}};
}
void PrologueRecovery::tick(double seconds) {
    if(!running_) return;elapsed_+=seconds;if(elapsed_<1) return;elapsed_=0;
    try {
        const auto player=host_.player();
        if(id(player.value("pawn",Json()))!=pawn_ || id(player.value("controller",Json()))!=controller_)
            throw std::runtime_error("Player changed. The intro-lock check was cancelled.");
        const auto found=inspect(player.at("pawn"));
        if(found.is_null()) {cancel();message_="Known intro lock not present. No game state changed.";return;}
        const auto ability=id(found.at("ability"));
        if(attempted_.contains({pawn_,ability})) {cancel();message_="Cleanup was already attempted on this ability. No repeat request sent.";return;}
        if(ability_!=ability) {ability_=ability;observations_=0;}
        if(++observations_<9) {message_="Intro lock still present. Check "+std::to_string(observations_)+" of 9.";return;}
        // Mark before calling: an exception or partial game-side cleanup must
        // never cause the same ability to be reset repeatedly.
        attempted_.insert({pawn_,ability});running_=false;
        host_.call(found.at("ability"),"ResetPlayerState");
        Json counts=Json::object();bool clear=true;
        for(const auto* tag:tags) {
            const int count=host_.call(found.at("asc"),"GetGameplayTagCount",Json::array({{{"TagName",tag}}}));
            counts[tag]=count;clear=clear && count==0;
        }
        message_=clear?"Completed-intro lock cleared.":"Cleanup returned, but some restrictions remain. No repeat request sent.";
        host_.log(message_,clear?"info":"warning",{{"tags",counts}});
    } catch(const std::exception& e) {cancel();message_=e.what();host_.log("Intro recovery stopped: "+message_,"warning");}
}
}

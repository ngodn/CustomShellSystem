// Included inside css after attachment_follower.inl. Optional feminine walk and run (0.4).
//
// Player locomotion is motion matching keyed by weapon stance, but ABP_Player keeps a
// blendspace override (UseActiveBlendspace / ActiveBlendSpace). The walk mods fed it a
// pak copy of the game's own Cultist Spear Lady blendspace; CSS points it straight at the
// game's assets instead, so nothing extra is installed and any walk mod is ignored:
//   walk / idle  -> BS_CultistSpearLady (eight walk directions at 85 cm/s, idle at 0)
//   jog / sprint -> BS_Aristocrat (Tishina's Confessor: idle, eight walks at 77, run at 564)
// The Cultist cycle is authored for 85 cm/s, so while the feminine walk is on the walking
// speed the game requests (184) is answered with 85 in SetMaxSpeedAdjustment; jog and
// sprint pass through untouched.
//
// Matching the run to the player's speed is the engine's own job, through
// UBlendSpace::ComputeAxisScaleFactor: when the speed fed to a blendspace exceeds that
// axis's Max, and AxisToScaleAnimation names that axis, the engine time-scales the
// animation by input/Max. The Aristocrat's run is authored at 564 cm/s, which is what the
// player jogs at (540, a 4% difference nobody sees); sprinting at 800 exceeds the axis and
// is played 800/564 = 1.42x faster, so the feet keep up. Only AxisToScaleAnimation is set,
// and it is restored on release. The axis Max is deliberately left alone: raising it both
// disables that extrapolation and drags the jog pose back toward a walk, because the
// triangulation baked into the asset is normalised against the authored range.
namespace {
constexpr const wchar_t* WALK_BLENDSPACE=L"/Game/Sparta/Characters/Enemies/CultistSpearLady/Art/Animation/Locomotion/BS_CultistSpearLady.BS_CultistSpearLady";
constexpr const wchar_t* RUN_BLENDSPACE=L"/Game/Sparta/Characters/Enemies/Aristocrat/Animation/Locomotion/BS_Aristocrat.BS_Aristocrat";
constexpr const wchar_t* WALK_SPEED_FUNCTION=L"/Script/Sparta.SpartaCharacterMovementComponent:SetMaxSpeedAdjustment";
constexpr float WALK_DEFAULT_SPEED=184.f, WALK_BORROWED_SPEED=85.f, WALK_SPEED_TOLERANCE=15.f;
constexpr uint8_t AXIS_SCALE_Y=2;
// The game jogs at 540 and sprints at 800 (its own locomotion blendspace samples).
constexpr double WALK_IDLE_MAX=25., WALK_IDLE_OFF=70., WALK_MIN=20., WALK_FALLBACK_MAX=300., WALK_SLIDE_MAX=130., JOG_MIN=300., SPRINT_MIN=700.;
constexpr int WALK_IDLE_SETTLE=3, WALK_OFF_DWELL=3;
constexpr uint64_t WALK_SLIDE_COOLDOWN=1500, WALK_ABILITY_RETRY=3000, WALK_MODS_RECHECK=5000;
template<typename T> void write_field(UObject* object,const wchar_t* name,const T& value) {
    auto* p=field(object,name,sizeof(T));
    std::memcpy(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal(),&value,sizeof(T));
}
bool has_field(UObject* object,const wchar_t* name,size_t size) {
    if(!object) return false;
    auto* p=object->GetPropertyByNameInChain(name);
    return p && p->GetElementSize()==static_cast<int32_t>(size) && p->GetArrayDim()==1;
}
}
bool WalkOverride::walk_mod_active(const fs::path& mods) {
    const auto now=GetTickCount64();
    if(!mods.empty() && mods!=mods_) { mods_=mods; mods_checked_=false; }
    if(mods_checked_ && now<mods_check_) return mod_active_;
    mods_checked_=true; mods_check_=now+WALK_MODS_RECHECK;
    mod_active_=false; mod_name_.clear(); genessa_active_=proxima_active_=false;
    std::error_code ec;
    if(mods_.empty()) return false;

    std::set<std::string> disabled_mods, enabled_mods;
    if(fs::is_regular_file(mods_/L"mods.txt",ec)) {
        if(std::ifstream in(mods_/L"mods.txt"); in.is_open()) {
            std::string line;
            while(std::getline(in,line)) {
                auto hash=line.find(';'); if(hash!=std::string::npos) line=line.substr(0,hash);
                auto colon=line.find(':');
                if(colon!=std::string::npos) {
                    auto name=line.substr(0,colon);
                    while(!name.empty() && (name.back()==' ' || name.back()=='\t' || name.back()=='\r')) name.pop_back();
                    while(!name.empty() && (name.front()==' ' || name.front()=='\t')) name.erase(name.begin());
                    for(char& c:name) c=char(std::tolower(static_cast<unsigned char>(c)));
                    auto val=line.substr(colon+1);
                    while(!val.empty() && (val.front()==' ' || val.front()=='\t')) val.erase(val.begin());
                    if(!val.empty() && val[0]=='0') disabled_mods.insert(name);
                    else if(!val.empty() && val[0]=='1') enabled_mods.insert(name);
                }
            }
        }
    }

    auto detect=[&](const wchar_t* wname,const char* sname) {
        std::string lower(sname);
        for(char& c:lower) c=char(std::tolower(static_cast<unsigned char>(c)));
        if(disabled_mods.contains(lower)) return false;
        if(enabled_mods.contains(lower)) return true;
        return fs::is_regular_file(mods_/wname/L"enabled.txt",ec);
    };

    genessa_active_ = detect(L"GenessaWalk","GenessaWalk");
    proxima_active_ = detect(L"ProximaWalk","ProximaWalk");
    if(genessa_active_ && proxima_active_) mod_name_ = "GenessaWalk & ProximaWalk";
    else if(genessa_active_) mod_name_ = "GenessaWalk";
    else if(proxima_active_) mod_name_ = "ProximaWalk";
    mod_active_ = !mod_name_.empty();
    return mod_active_;
}
UObject* WalkOverride::blendspace(WeakObject& slot,const wchar_t* path) {
    if(auto* found=slot.Get()) return found;
    auto* asset=UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path);
    if(!asset) asset=load(narrow(path));
    if(!asset || !asset->IsA(static_cast<UClass*>(find(L"/Script/Engine.BlendSpace")))) throw std::runtime_error("Locomotion blendspace is unavailable");
    slot=asset; return asset;
}
void WalkOverride::hook_speed() {
    if(hook_) return;
    auto* function=static_cast<UFunction*>(find(WALK_SPEED_FUNCTION));
    if(!function || !function->IsA<UFunction>()) throw std::runtime_error("Walk speed function is missing");
    FProperty* speed=nullptr;
    for(auto* p:function->ForEachProperty()) if(p->GetName()==L"Speed" && p->HasAnyPropertyFlags(CPF_Parm)) speed=p;
    if(!speed || speed->GetElementSize()!=sizeof(float) || speed->GetOffset_Internal()<0 ||
       speed->GetOffset_Internal()+int32_t(sizeof(float))>function->GetParmsSize())
        throw std::runtime_error("Walk speed parameter layout mismatch");
    const auto offset=speed->GetOffset_Internal();
    hook_=function->RegisterPreHook([this,offset](UnrealScriptFunctionCallableContext& context,void*) {
        if(!scale_walk_.load(std::memory_order_relaxed)) return;
        auto* locals=context.TheStack.Locals(); if(!locals) return;
        auto* value=reinterpret_cast<float*>(locals+offset);
        if(std::isfinite(*value) && std::abs(*value-WALK_DEFAULT_SPEED)<=WALK_SPEED_TOLERANCE) *value=WALK_BORROWED_SPEED;
    });
    if(!*hook_) { hook_.reset(); throw std::runtime_error("Walk speed hook was refused"); }
}
void WalkOverride::unhook_speed() {
    scale_walk_=false;
    if(!hook_) return;
    if(auto* function=static_cast<UFunction*>(find(WALK_SPEED_FUNCTION))) function->UnregisterHook(*hook_);
    hook_.reset();
}
void WalkOverride::prepare_run(UObject* run) {
    if(run_tweaked_) return;
    original_run_axis_=read<uint8_t>(run,L"AxisToScaleAnimation");
    write_field<uint8_t>(run,L"AxisToScaleAnimation",AXIS_SCALE_Y);
    run_tweaked_=true;
}
void WalkOverride::restore_run() {
    if(!run_tweaked_) return;
    if(auto* run=run_bs_.Get()) { try { write_field<uint8_t>(run,L"AxisToScaleAnimation",original_run_axis_); } catch(...) {} }
    run_tweaked_=false;
}
void WalkOverride::push_on(UObject* target) {
    auto* anim=anim_.Get();
    if(!anim || !target) throw std::runtime_error("Walk animation targets vanished");
    if(target==run_bs_.Get()) prepare_run(target);
    write_field<UObject*>(anim,L"ActiveBlendSpace",target);
    write_field<bool>(anim,L"UseActiveBlendspace",true);
    if(!read<bool>(anim,L"UseActiveBlendspace")) throw std::runtime_error("Walk animation flag did not take");
    active_=target; engaged_=true;
}
void WalkOverride::push_off() {
    // Clear the override only while it still points at the blendspace CSS set. A walk mod
    // (argisht's GenessaWalk or ProximaWalk) drives the same two fields, so once it owns
    // them CSS leaves its state alone instead of switching that mod off.
    if(auto* anim=anim_.Get()) {
        try {
            auto* mine=active_.Get();
            if(mine && read<UObject*>(anim,L"ActiveBlendSpace")==mine) write_field<bool>(anim,L"UseActiveBlendspace",false);
        } catch(...) {}
    }
    engaged_=false; active_.Reset(); reason_.clear();
}
void WalkOverride::release() {
    push_off();
    restore_run();
    try { unhook_speed(); } catch(...) { hook_.reset(); }
    pawn_.Reset(); anim_.Reset(); movement_.Reset(); walk_ability_.Reset(); walk_bs_.Reset(); run_bs_.Reset();
    idle_ticks_=off_ticks_=slide_ticks_=0; slide_until_=0; last_heal_=0;
}
void WalkOverride::update(UObject* pawn,bool idle_feminine,bool walk_feminine,bool jog_feminine,bool sprint_feminine) {
    const auto now=GetTickCount64();
    const bool run_feminine=jog_feminine||sprint_feminine;
    if(!idle_feminine && !walk_feminine && !run_feminine) { if(engaged_ || hook_ || run_tweaked_) release(); return; }
    if(!pawn) { if(engaged_) push_off(); return; }
    if(pawn_.Get()!=pawn) {
        if(engaged_) push_off();
        pawn_=pawn; anim_.Reset(); movement_.Reset(); walk_ability_.Reset();
    }
    auto* anim=anim_.Get();
    if(!anim) {
        auto* mesh=read<UObject*>(pawn,L"Mesh");
        Call instance(mesh,L"GetAnimInstance",1); instance.run(); anim=instance.get<UObject*>();
        if(!anim || !has_field(anim,L"UseActiveBlendspace",sizeof(bool)) || !has_field(anim,L"ActiveBlendSpace",sizeof(UObject*)))
            throw std::runtime_error("Player animation instance has no blendspace override");
        anim_=anim;
    }
    if(!movement_.Get()) movement_=read<UObject*>(pawn,L"CharacterMovement");
    UObject* walk_bs=(walk_feminine || idle_feminine)?blendspace(walk_bs_,WALK_BLENDSPACE):nullptr;
    UObject* run_bs=run_feminine?blendspace(run_bs_,RUN_BLENDSPACE):nullptr;
    if(walk_feminine) { hook_speed(); scale_walk_=true; } else { scale_walk_=false; if(hook_) unhook_speed(); }
    if(!run_feminine) restore_run();
    auto* movement=movement_.Get();
    double speed=0; bool speed_known=false;
    if(movement) { auto v=read<std::array<double,3>>(movement,L"Velocity"); speed=std::hypot(v[0],v[1]); speed_known=std::isfinite(speed); }
    std::optional<bool> walking;
    if(!walk_ability_.Get() && now>=next_ability_search_) {
        next_ability_search_=now+WALK_ABILITY_RETRY;
        if(auto* ability=UObjectGlobals::FindFirstOf(L"GA_Walk_C"); ability && has_field(ability,L"IsWalking",sizeof(bool))) walk_ability_=ability;
    }
    if(auto* ability=walk_ability_.Get()) walking=read<bool>(ability,L"IsWalking");
    const bool walk_now=walking.value_or(speed_known && speed>WALK_MIN && speed<WALK_FALLBACK_MAX);
    const bool sprinting=!walk_now && speed_known && speed>=SPRINT_MIN;
    const bool jogging=!walk_now && speed_known && speed>=JOG_MIN && speed<SPRINT_MIN;
    const double idle_cut=(engaged_ && reason_=="idle")?WALK_IDLE_OFF:WALK_IDLE_MAX;
    const bool standing=!walk_now && !jogging && !sprinting && speed_known && speed<idle_cut;
    idle_ticks_=standing?idle_ticks_+1:0;
    const bool settled=standing && (engaged_ || idle_ticks_>=WALK_IDLE_SETTLE);
    UObject* want=nullptr; bool hard_off=false; std::string reason;
    if(sprinting && sprint_feminine && run_bs) { want=run_bs; reason="sprint"; }
    else if(jogging && jog_feminine && run_bs) { want=run_bs; reason="jog"; }
    else if(walk_now && walk_feminine && walk_bs) { want=walk_bs; reason="walk"; }
    else if(settled && idle_feminine && walk_bs) { want=walk_bs; reason="idle"; }
    else if((standing && !idle_feminine) || (walk_now && !walk_feminine)) hard_off=true;
    // Slide guard: engaged for walking but still travelling at the stock speed means the
    // scaling has not landed; fall back until the next gait change re-issues the speed.
    if(want && reason=="walk" && speed_known && speed>WALK_SLIDE_MAX) {
        if(++slide_ticks_>=2) { want=nullptr; hard_off=true; slide_until_=now+WALK_SLIDE_COOLDOWN; }
    } else slide_ticks_=0;
    if(reason=="walk" && now<slide_until_) { want=nullptr; hard_off=true; }
    if(want) off_ticks_=0;
    else if(hard_off) off_ticks_=WALK_OFF_DWELL;
    else if(++off_ticks_<WALK_OFF_DWELL && engaged_ && active_.Get() && (!speed_known || speed<=WALK_SLIDE_MAX || active_.Get()==run_bs)) { want=active_.Get(); reason=reason_.empty()?"hold":reason_; }
    if(want) {
        // Re-assert on every mismatch, not on a timer: a walk mod ticking against the same
        // two fields must not be able to take the animation back while Feminine is chosen.
        if(!engaged_ || active_.Get()!=want || !read<bool>(anim,L"UseActiveBlendspace") || read<UObject*>(anim,L"ActiveBlendSpace")!=want) { push_on(want); last_heal_=now; }
        reason_=reason;
    } else if(engaged_) push_off();
}

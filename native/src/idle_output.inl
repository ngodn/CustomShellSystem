// FPoseLinkBase::SetLinkNode changes only LinkedNode. Leave the serialized LinkID and the
// class defaults intact. Each address is resolved on this instance after joining its worker.
namespace {
struct PlayerRootLink {
    std::byte* address;
    std::byte* original;
    std::byte* selected;
    int32_t source_id() const {
        int32_t id{};
        std::memcpy(&id,address+sizeof(void*),sizeof(id));
        return id;
    }
    std::byte* current() const {
        std::byte* value{};std::memcpy(&value,address,sizeof(value));return value;
    }
    void set(std::byte* value) const { std::memcpy(address,&value,sizeof(value)); }
};
void join_player_animation(UObject* animation) {
    Call barrier(animation,L"GetCurveValue",2);
    barrier.set(L"CurveName",FName(L"None"));
    barrier.run();
}
PlayerRootLink player_idle_link(UObject* animation,IdleOutput output) {
    if(!animation || animation->GetClassPrivate()->GetPathName()!=
       L"/Game/Sparta/Characters/Humans/Player/Animations/ABP_Player.ABP_Player_C")
        throw std::runtime_error("Idle requires the player animation graph");
    join_player_animation(animation);
    auto node=[&](std::wstring_view name,const wchar_t* type) {
        auto* p=idle_graph_property<FProperty>(animation->GetClassPrivate()->ForEachProperty(),name,
            [](FProperty* property){return property->GetName();});
        if(!p || !p->IsA<FStructProperty>() || p->GetArrayDim()!=1 ||
           !static_cast<FStructProperty*>(p)->GetStruct().Get() ||
           static_cast<FStructProperty*>(p)->GetStruct()->GetFName()!=FName(type) ||
           p->GetOffset_Internal()<0 || p->GetOffset_Internal()>animation->GetClassPrivate()->GetPropertiesSize()-p->GetElementSize())
            throw std::runtime_error("Player idle graph node layout changed");
        return reinterpret_cast<std::byte*>(animation)+p->GetOffset_Internal();
    };
    auto* root=node(player_idle_root,L"AnimNode_Root");
    auto* root_type=static_cast<UStruct*>(find(L"/Script/Engine.AnimNode_Root"));
    auto* result=optional_field(root_type,L"Result");
    auto* pose=find(L"/Script/Engine.PoseLink");
    auto* id=field(pose,L"LinkID",sizeof(int32_t));
    if(!result || !result->IsA<FStructProperty>() || static_cast<FStructProperty*>(result)->GetStruct().Get()!=pose ||
       result->GetElementSize()!=16 || !id->IsA<FIntProperty>() || id->GetOffset_Internal()!=sizeof(void*) ||
       result->GetOffset_Internal()<0 || result->GetOffset_Internal()>root_type->GetPropertiesSize()-result->GetElementSize())
        throw std::runtime_error("Player idle root link layout changed");
    auto* link=root+result->GetOffset_Internal();
    auto* original=node(player_idle_source,L"AnimNode_LinkedAnimGraph");
    auto* selected=output==IdleOutput::carrier ? node(player_idle_carrier,L"AnimNode_BlendSpacePlayer") :
                                                node(player_idle_slot,L"AnimNode_Slot");
    return {link,original,selected};
}
}
bool PlayerIdleOutput::linked() const {
    auto* owner=owner_.Get();
    if(!owner || !address_) return false;
    join_player_animation(owner);
    return PlayerRootLink{address_,original_,selected_}.current()==selected_;
}
void PlayerIdleOutput::bind(UObject* animation,IdleOutput output) {
    if(output==IdleOutput::game) { release();return; }
    const bool same=owner_.Get()==animation && output_==output;
    if(same && address_ && linked()) return;
    if(!same || address_) {
        release();
        owner_=animation;
        output_=output;
    }
    // The 1.0.11 BlendSpace or the positioned menu montage is already playing.
    // A rejected direct link must leave that playback running.
    fallback_.attempt(GetTickCount64(),[&] {
        auto link=player_idle_link(animation,output);
        const int32_t source_id=link.source_id();
        acquire_idle_link(link.current(),link.original,link.selected,[&](auto* value){link.set(value);});
        source_id_=source_id;
        address_=link.address;
        original_=link.original;
        selected_=link.selected;
    });
}
void PlayerIdleOutput::release() {
    if(auto* owner=owner_.Get(); owner && address_) {
        join_player_animation(owner);
        PlayerRootLink link{address_,original_,selected_};
        // If another owner changed the serialized source, let Unreal relink that new source.
        restore_idle_link(link.current(),selected_,link.source_id()==source_id_ ? original_ : nullptr,[&](auto* value){link.set(value);});
    }
    fallback_.reset();
    owner_.Reset();address_=original_=selected_=nullptr;output_=IdleOutput::game;
}

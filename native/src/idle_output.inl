// FPoseLinkBase::SetLinkNode changes only LinkedNode. Leave the serialized LinkID and the
// class defaults intact. Each address is resolved on this instance after joining its worker.
namespace {
struct PlayerRootLink {
    std::byte* address;
    std::byte* original;
    std::byte* selected;
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
    auto node=[&](const wchar_t* name,const wchar_t* type) {
        auto* p=optional_field(animation,name);
        if(!p || !p->IsA<FStructProperty>() || p->GetArrayDim()!=1 ||
           !static_cast<FStructProperty*>(p)->GetStruct().Get() ||
           static_cast<FStructProperty*>(p)->GetStruct()->GetFName()!=FName(type) ||
           p->GetOffset_Internal()<0 || p->GetOffset_Internal()>animation->GetClassPrivate()->GetPropertiesSize()-p->GetElementSize())
            throw std::runtime_error("Player idle graph node layout changed");
        return reinterpret_cast<std::byte*>(animation)+p->GetOffset_Internal();
    };
    auto* root=node(L"AnimGraphNode_Root",L"AnimNode_Root");
    auto* root_type=static_cast<UStruct*>(find(L"/Script/Engine.AnimNode_Root"));
    auto* result=optional_field(root_type,L"Result");
    auto* pose=find(L"/Script/Engine.PoseLink");
    auto* id=field(pose,L"LinkID",sizeof(int32_t));
    if(!result || !result->IsA<FStructProperty>() || static_cast<FStructProperty*>(result)->GetStruct().Get()!=pose ||
       result->GetElementSize()!=16 || !id->IsA<FIntProperty>() || id->GetOffset_Internal()!=sizeof(void*) ||
       result->GetOffset_Internal()<0 || result->GetOffset_Internal()>root_type->GetPropertiesSize()-result->GetElementSize())
        throw std::runtime_error("Player idle root link layout changed");
    auto* link=root+result->GetOffset_Internal();
    int32_t link_id{};std::memcpy(&link_id,link+id->GetOffset_Internal(),sizeof(link_id));
    if(link_id!=30) throw std::runtime_error("Player idle root has a different source");
    auto* original=node(L"AnimGraphNode_LinkedAnimGraph_1",L"AnimNode_LinkedAnimGraph");
    auto* selected=output==IdleOutput::carrier ? node(L"AnimGraphNode_BlendSpacePlayer",L"AnimNode_BlendSpacePlayer") :
                                                node(L"AnimGraphNode_Slot_5",L"AnimNode_Slot");
    return {link,original,selected};
}
}
bool PlayerIdleOutput::linked() const {
    auto* owner=owner_.Get();
    if(!owner) return false;
    join_player_animation(owner);
    return PlayerRootLink{address_,original_,selected_}.current()==selected_;
}
void PlayerIdleOutput::bind(UObject* animation,IdleOutput output) {
    if(output==IdleOutput::game) { release();return; }
    if(owner_.Get()==animation && output_==output && linked()) return;
    release();
    auto link=player_idle_link(animation,output);
    WeakObject guard=animation;
    acquire_idle_link(link.current(),link.original,link.selected,[&](auto* value){link.set(value);});
    owner_=guard;address_=link.address;original_=link.original;selected_=link.selected;output_=output;
}
void PlayerIdleOutput::release() {
    if(auto* owner=owner_.Get()) {
        join_player_animation(owner);
        PlayerRootLink link{address_,original_,selected_};
        int32_t id{};std::memcpy(&id,address_+sizeof(void*),sizeof(id));
        // If another owner changed the serialized source, let Unreal relink that new source.
        restore_idle_link(link.current(),selected_,id==30 ? original_ : nullptr,[&](auto* value){link.set(value);});
    }
    owner_.Reset();address_=original_=selected_=nullptr;output_=IdleOutput::game;
}

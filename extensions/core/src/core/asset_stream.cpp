#include "bridge.hpp"
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <chrono>

namespace cssx {
using namespace engine;
namespace {
UObject* kismet() { return find_cached(L"/Script/Engine.Default__KismetSystemLibrary"); }

std::wstring argument(Call& call,const wchar_t* spelling) {
    const FName name(spelling);
    for(auto* p:call.parameters()) if(p->GetFName()==name) return p->GetName();
    throw std::runtime_error("Missing async-load parameter: "+narrow(spelling));
}

// Only converts a pathname into a soft reference. Neither conversion loads the asset.
void soft_argument(Call& call,const wchar_t* parameter,const std::string& path) {
    Call make(kismet(),L"MakeSoftObjectPath",2);
    make.set(L"PathString",FString(wide(path).c_str())); make.run();
    Call convert(kismet(),L"Conv_SoftObjPathToSoftObjRef",2);
    convert.copy(L"SoftObjectPath",make,L"ReturnValue"); convert.run();
    call.copy(argument(call,parameter).c_str(),convert,L"ReturnValue");
}

class KismetStream final : public AssetStream {
    Bridge& bridge_;
    WeakObject instance_,pc_,receiver_,result_,items_;
    std::chrono::steady_clock::time_point started_;
    bool pending_=false;
    WeakObject pin(UObject* object) {
        WeakObject weak(object);
        keep_alive(instance_.Get(),object);
        return weak;
    }
    void unpin(WeakObject& weak) noexcept {
        if(auto* keeper=instance_.Get()) if(auto* object=weak.Get()) let_go(keeper,object);
        weak.Reset();
    }
public:
    KismetStream(Bridge& bridge,const PlayerContext& p):bridge_(bridge),instance_(object_of(p.world,L"OwningGameInstance")),pc_(p.pc) {
        if(!instance_.Get() || !pc_.Get()) throw std::runtime_error("Async loading needs a live world and player controller");
    }
    ~KismetStream() override {
        cancel_path();
        if(auto* list=items_.Get()) try { invoke(list,L"ClearListItems"); } catch(...) {}
        unpin(items_);
    }
    void start(const std::string& path) override {
        auto* instance=instance_.Get();
        if(!instance || !pc_.Get()) throw std::runtime_error("Async load context expired");
        if(!items_.Get()) items_=pin(construct(L"/Script/UMG.ListView",instance));
        if(!result_.Get()) result_=pin(construct(L"/Script/UMG.ListView",instance));
        else invoke(result_.Get(),L"ClearListItems");
        if(!receiver_.Get()) {
            // This concrete native UserWidget has an empty tree (SSpacer). Its Slate tick
            // processes its own latent actions even while the world's latent pass is paused.
            receiver_=pin(create_widget(pc_.Get(),static_cast<UClass*>(find_cached(L"/Script/LevelSequence.LevelSequenceBurnIn"))));
            set_widget_option<Call>(receiver_.Get(),L"SetVisibility",WidgetParamType::byte_enum,uint8_t{3}); // HitTestInvisible
        }
        auto* receiver=receiver_.Get();
        auto* list=result_.Get();
        Call add_item(list,L"AddItem",1);
        (void)widget_parameter(add_item,WidgetParamType::object,WidgetParamRole::input);
        Call resume(receiver,L"RemoveFromParent",0);
        Call load(kismet(),L"LoadAsset",4);
        soft_argument(load,L"Asset",path);
        load.set(argument(load,L"WorldContextObject").c_str(),pc_.Get());
        const auto on_loaded=argument(load,L"OnLoaded");
        auto* delegate=load.param(on_loaded.c_str());
        auto* latent=load.param(argument(load,L"LatentInfo").c_str());
        auto* info=find_cached(L"/Script/Engine.LatentActionInfo");
        if(!delegate->IsA<FDelegateProperty>() || delegate->GetElementSize()!=sizeof(FScriptDelegate) ||
           !latent->IsA<FStructProperty>() || static_cast<FStructProperty*>(latent)->GetStruct().Get()!=info)
            throw std::runtime_error("Unexpected LoadAsset delegate or latent signature");
        auto* signature=static_cast<FDelegateProperty*>(delegate)->GetSignatureFunction().Get();
        if(!signature || signature->GetNumParms()!=1) throw std::runtime_error("Unexpected async completion arity");
        bool compatible=false;
        for(auto* p:signature->ForEachProperty()) if(p->HasAnyPropertyFlags(CPF_Parm))
            compatible=!p->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm) && p->SameType(add_item.parameters().front());
        if(!compatible) throw std::runtime_error("Async completion must deliver one object");
        FScriptDelegate capture;
        capture.Object=static_cast<const FWeakObjectPtr&>(result_);
        capture.FunctionName=add_item.function()->GetFName();
        load.set(on_loaded.c_str(),capture);
        auto* data=load.data(latent);
        const auto bytes=latent->GetElementSize();
        member(data,bytes,info,L"CallbackTarget",receiver);
        // The engine calls the continuation after OnLoaded, even when the asset is null.
        // RemoveFromParent has a zero-byte parameter frame and ignores the linkage value.
        member(data,bytes,info,L"ExecutionFunction",resume.function()->GetFName());
        member(data,bytes,info,L"Linkage",int32_t{0});
        member(data,bytes,info,L"UUID",int32_t{1}); // one outstanding action per private receiver
        pending_=true;
        Call add(receiver,L"AddToViewport",1);
        add.set(argument(add,L"ZOrder").c_str(),int32_t{-10000}); add.run();
        if(!in_viewport()) throw std::runtime_error("Async completion receiver could not enter the viewport");
        started_=std::chrono::steady_clock::now();
        load.run();
    }
    bool in_viewport() const {
        Call visible(receiver_.Get(),L"IsInViewport",1); visible.run(); return visible.get<bool>();
    }
    std::optional<uint64_t> poll() override {
        if(!pending_ || !items_.Get() || !receiver_.Get()) throw std::runtime_error("Async completion context expired");
        if(in_viewport()) {
            if(std::chrono::steady_clock::now()-started_>std::chrono::seconds(120)) throw std::runtime_error("Async load timed out");
            return {};
        }
        // The delegate put its argument in a GC-scanned ListItems array before the engine
        // released its streamable handle. Transfer it to the batch's permanent list.
        Call get(result_.Get(),L"GetItemAt",2); get.set(argument(get,L"Index").c_str(),int32_t{0}); get.run();
        auto* object=get.get<UObject*>();
        if(!object) { cancel_path(); return uint64_t{0}; }
        Call index(items_.Get(),L"GetIndexForItem",2);
        const auto arg=widget_parameter(index,WidgetParamType::object,WidgetParamRole::input);
        index.set(arg.c_str(),object); index.run();
        if(index.get<int32_t>()<0) set_widget_option<Call>(items_.Get(),L"AddItem",WidgetParamType::object,object);
        const auto id=bridge_.track(object);
        pending_=false;
        return id;
    }
    void cancel_path() noexcept override {
        if(pending_) if(auto* receiver=receiver_.Get()) {
            try { invoke(receiver,L"CancelLatentActions"); } catch(...) {}
            try { invoke(receiver,L"RemoveFromParent"); } catch(...) {}
        }
        pending_=false;
        // Latent removal is deferred. A late engine delegate can only reach its old,
        // unanchored result list, never another path's mailbox or extension code.
        unpin(receiver_);
        unpin(result_);
    }
};
}

std::unique_ptr<AssetStream> Bridge::asset_stream(const PlayerContext& player) {
    return std::make_unique<KismetStream>(*this,player);
}
}

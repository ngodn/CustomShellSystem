#include "menu_input.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <unordered_map>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace cssx {
using namespace engine;
// Each mapped key's FName is made once; the menu asks for every key every frame.
FName key_name(const std::string& key) {
    static std::unordered_map<std::string,FName> names;
    if(auto it=names.find(key);it!=names.end()) return it->second;
    return names.emplace(key,FName(wide(key).c_str())).first->second;
}
bool key_down(UObject* pc,const std::string& key) {
    Call call(pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
    member(call.data(p),p->GetElementSize(),find_cached(L"/Script/InputCore.Key"),L"KeyName",key_name(key));
    call.run(); return call.get<bool>();
}

std::vector<MenuKeyBinding> read_menu_bindings(UObject* pc,UObject* handler) {
    std::vector<MenuKeyBinding> bindings_;
    auto* mapping=object_of(handler,L"InputMapping");
    auto* p=mapping?mapping->GetPropertyByNameInChain(L"Mappings"):nullptr;
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Menu input mapping is unavailable");
    Call subsystem(find_cached(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetLocalPlayerSubSystemFromPlayerController",3);
    subsystem.set(L"PlayerController",pc); subsystem.set(L"Class",static_cast<UClass*>(find_cached(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem"))); subsystem.run();
    auto* input=subsystem.get<UObject*>();
    if(!input) throw std::runtime_error("Player input subsystem is unavailable");
    static const std::map<std::wstring,std::string> actions={
        {L"IA_Menu_Up","up"},{L"IA_Menu_Down","down"},{L"IA_Menu_Left_Primary","left"},{L"IA_Menu_Right_Primary","right"},
        {L"IA_Menu_Left_Tertiary","previous_section"},{L"IA_Menu_Right_Tertiary","next_section"},
        {L"IA_Menu_Confirm_Primary_Press","accept"},{L"IA_Menu_Confirm_Secondary_Press","secondary"},{L"IA_Menu_Back","close"}};
    auto* a=static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(mapping)+p->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>256) throw std::runtime_error("Input map exceeds bound");
    auto* mapping_struct=find_cached(L"/Script/EnhancedInput.EnhancedActionKeyMapping");
    auto* ap=field(mapping_struct,L"Action",8);
    auto* key_field=mapping_struct->GetPropertyByNameInChain(L"Key");
    auto* kn=field(find_cached(L"/Script/InputCore.Key"),L"KeyName",sizeof(FName));
    if(!key_field || key_field->GetOffset_Internal()<0 || kn->GetOffset_Internal()+int(sizeof(FName))>key_field->GetElementSize()) throw std::runtime_error("Input mapping key layout mismatch");
    const int element=a->GetInner()->GetElementSize();
    if(ap->GetOffset_Internal()+8>element || key_field->GetOffset_Internal()+key_field->GetElementSize()>element) throw std::runtime_error("Input mapping element layout mismatch");
    auto usable=[](const std::string& name){ return !(name=="Gamepad_LeftX" || name=="Gamepad_LeftY" || name=="Gamepad_RightX" || name=="Gamepad_RightY"); };
    std::map<UObject*,size_t> index;
    // Pass 1: the keys the context itself declares (covers every action even
    // before Enhanced Input has rebuilt the player's applied mappings).
    for(int i=0;i<values.Num();++i) {
        UObject* action{}; std::memcpy(&action,values.GetRawPtr(i)+ap->GetOffset_Internal(),8);
        if(!action) continue;
        const auto found=actions.find(action->GetName());
        if(found==actions.end()) continue;
        auto it=index.find(action);
        if(it==index.end()) { MenuKeyBinding binding; binding.input_action=action; binding.action=found->second; bindings_.push_back(std::move(binding)); it=index.emplace(action,bindings_.size()-1).first; }
        FName key{}; std::memcpy(&key,values.GetRawPtr(i)+key_field->GetOffset_Internal()+kn->GetOffset_Internal(),sizeof(key));
        auto name=narrow(key.ToString());
        auto& keys=bindings_[it->second].keys;
        if(usable(name) && !name.empty() && name!="None" && std::find(keys.begin(),keys.end(),name)==keys.end()) keys.push_back(name);
    }
    // The page has no character preview, so the left stick can navigate too.
    static const std::map<std::string,std::vector<std::string>> stick={{"up",{"Gamepad_LeftStick_Up"}},{"down",{"Gamepad_LeftStick_Down"}},{"left",{"Gamepad_LeftStick_Left"}},{"right",{"Gamepad_LeftStick_Right"}}};
    for(auto& binding:bindings_) if(auto extra=stick.find(binding.action); extra!=stick.end()) for(const auto& k:extra->second) if(std::find(binding.keys.begin(),binding.keys.end(),k)==binding.keys.end()) binding.keys.push_back(k);
    // Pass 2: the player's applied mappings (remaps). When the query answers,
    // it replaces the declared keys for that action.
    for(auto& binding:bindings_) {
        try {
            Call query(input,L"QueryKeysMappedToAction",2); query.set(L"Action",binding.input_action); query.run();
            auto* out=query.param(L"ReturnValue");
            if(!out->IsA<FArrayProperty>()) continue;
            auto* array=static_cast<FArrayProperty*>(out); FScriptArrayHelper keys(array,query.data(out));
            if(keys.Num()<=0 || keys.Num()>32 || kn->GetOffset_Internal()+8>array->GetInner()->GetElementSize()) continue;
            std::vector<std::string> applied;
            for(int n=0;n<keys.Num();++n) { FName key{}; std::memcpy(&key,keys.GetRawPtr(n)+kn->GetOffset_Internal(),sizeof(key)); auto name=narrow(key.ToString()); if(usable(name)) applied.push_back(name); }
            if(!applied.empty()) { for(const auto& k:binding.keys) if(k.starts_with("Gamepad_LeftStick_") && std::find(applied.begin(),applied.end(),k)==applied.end()) applied.push_back(k); binding.keys=std::move(applied); }
        } catch(...) {}
    }
    if(bindings_.empty()) throw std::runtime_error("No menu navigation actions were found in the input mapping");
    return bindings_;
}
}

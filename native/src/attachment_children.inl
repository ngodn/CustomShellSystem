static std::vector<WeakObject> attached_children(UObject* component) {
    Call call(component,L"GetChildrenComponents",2);call.set(L"bIncludeAllDescendants",false);call.run();
    auto* property=call.param(L"Children");
    if(!property->IsA<FArrayProperty>()) throw std::runtime_error("Attachment children layout mismatch");
    auto* array=static_cast<FArrayProperty*>(property);
    if(!array->GetInner() || !array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("Attachment child is not an object");
    FScriptArrayHelper values(array,call.data(property));
    const auto count=values.Num();
    // Direct children are distinct registered components. Use the engine's
    // object population to reject corrupt counts, not an outfit-size limit.
    if(count<0 || count>FUObjectArray::GetNumElements())
        throw std::runtime_error("Attachment count exceeds the live object array");
    std::vector<WeakObject> result;
    result.reserve(count);
    auto* scene_class=static_cast<UClass*>(find(L"/Script/Engine.SceneComponent"));
    for(int i=0;i<count;++i) {
        auto* entry=values.GetRawPtr(i);
        if(!entry) throw std::runtime_error("Attachment array has no storage");
        UObject* child{};std::memcpy(&child,entry,sizeof(child));
        WeakObject weak(child);child=weak.Get();
        if(!child) continue;
        if(!child->IsA(scene_class)) throw std::runtime_error("Attachment is not a scene component");
        if(read<UObject*>(child,L"AttachParent")!=component) continue;
        result.push_back(weak);
    }
    return result;
}

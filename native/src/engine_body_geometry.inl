// Geometry coefficients are cooked into the AnimBP. Resolve them once per
// instance and evaluate only when CSS changes the shape or replaces the instance.
FProperty* body_geometry_property(UObject* instance,const wchar_t* name) {
    if(!instance || !instance->IsA(static_cast<UClass*>(find(L"/Script/Engine.AnimInstance"))) ||
       instance->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Body geometry requires an animation instance");
    auto* owner=instance->GetClassPrivate();
    if(!owner) throw std::runtime_error("Body geometry instance has no class");
    FProperty* result=nullptr;const FName wanted(name);
    unsigned depth=0,count=0;
    for(UStruct* type=owner;type;type=type->GetSuperStruct()) {
        if(++depth>64) throw std::runtime_error("Body geometry hierarchy exceeds bound");
        for(auto* property:type->ForEachProperty()) {
            if(++count>4096) throw std::runtime_error("Body geometry property count exceeds bound");
            if(property->GetFName()!=wanted) continue;
            if(result && result!=property) throw std::runtime_error("Ambiguous body geometry property");
            result=property;
        }
    }
    if(result && (!narrow(owner->GetPathName()).starts_with("/Game/CSS/") || result->GetArrayDim()!=1 ||
       result->GetElementSize()<=0 || result->GetOffset_Internal()<0 ||
       result->GetOffset_Internal()>owner->GetPropertiesSize()-result->GetElementSize()))
        throw std::runtime_error("Body geometry property layout mismatch");
    return result;
}

// The model is a string property cooked into the AnimBP; a shape route can select another one.
std::optional<BodyGeometryModel> body_geometry_model(UObject* instance,const std::string& name="CSSBodyGeometryJson") {
    auto* property=body_geometry_property(instance,wide(name).c_str());
    if(!property) return std::nullopt;
    if(!property->IsA<FStrProperty>() || property->GetElementSize()!=sizeof(FString))
        throw std::runtime_error("Body geometry requires a string model");
    const auto& value=*reinterpret_cast<const FString*>(reinterpret_cast<const std::byte*>(instance)+property->GetOffset_Internal());
    const auto& chars=value.GetCharArray();
    if(chars.Num()<2 || chars.Num()>262145 || !chars.GetData() || chars[chars.Num()-1]!=0)
        throw std::runtime_error("Body geometry model string exceeds bounds");
    const auto json=narrow(std::wstring(chars.GetData(),size_t(chars.Num()-1)));
    if(json.size()>262144) throw std::runtime_error("Body geometry model exceeds byte bound");
    return BodyGeometryModel::parse(Json::parse(json));
}

struct BodyGeometryInputs {
    UObject* instance;
    std::array<FArrayProperty*,6> arrays;
    FProperty* epoch;
    // CSSBodyShape0..n-1, one per model axis, or none on AnimBPs without shape inputs.
    std::array<FFloatProperty*,body_geometry_max_axes> shape{};
    size_t shapes=0;
    BodyGeometry capture() const {
        BodyGeometry value;
        value.shape.resize(shapes);
        for(size_t i=0;i<shapes;++i) {
            std::memcpy(&value.shape[i],reinterpret_cast<const std::byte*>(instance)+shape[i]->GetOffset_Internal(),sizeof(float));
            if(!std::isfinite(value.shape[i]) || value.shape[i]<0 || value.shape[i]>1)
                throw std::runtime_error("Body shape input outside [0,1]");
        }
        for(size_t field=0;field<arrays.size();++field) {
            auto* property=arrays[field];
            FScriptArrayHelper values(property,reinterpret_cast<std::byte*>(instance)+property->GetOffset_Internal());
            if(values.Num()!=7 || !values.GetRawPtr(0)) throw std::runtime_error("Body geometry requires seven stored entries");
            for(int32_t i=0;i<7;++i) {
                if(field==1) {
                    std::memcpy(&value.moments[size_t(i)],values.GetRawPtr(i),sizeof(float));
                    if(!std::isfinite(value.moments[size_t(i)]) || value.moments[size_t(i)]<=0)
                        throw std::runtime_error("Invalid authored body inertia");
                } else {
                    auto& target=field==0?value.offsets[size_t(i)]:field==2?value.contact_centers[size_t(i)]:
                        value.contact_axes[size_t(i)][field-3];
                    std::memcpy(target.data(),values.GetRawPtr(i),sizeof(target));
                    for(double v:target) if(!std::isfinite(v)) throw std::runtime_error("Non-finite authored body geometry");
                }
            }
        }
        return value;
    }
    void apply(BodyGeometry value) const {
        // Older AnimBPs have geometry arrays but no shape inputs to retain or restore.
        if(!shapes) value.shape.clear();
        else if(value.shape.size()!=shapes) throw std::runtime_error("Body shape inputs differ from the model axes");
        if(capture()==value) return;
        for(float coefficient:value.shape)
            if(!std::isfinite(coefficient) || coefficient<0 || coefficient>1)
                throw std::runtime_error("Body shape input outside [0,1]");
        for(size_t i=0;i<7;++i) {
            if(!std::isfinite(value.moments[i]) || value.moments[i]<=0) throw std::runtime_error("Invalid body inertia");
            auto finite=[](const BodyVector& v){return std::all_of(v.begin(),v.end(),[](double x){return std::isfinite(x);});};
            if(!finite(value.offsets[i]) || !finite(value.contact_centers[i]) ||
               !std::all_of(value.contact_axes[i].begin(),value.contact_axes[i].end(),finite))
                throw std::runtime_error("Non-finite body geometry");
        }
        for(size_t i=0;i<shapes;++i)
            shape[i]->CopyCompleteValue(reinterpret_cast<std::byte*>(instance)+shape[i]->GetOffset_Internal(),&value.shape[i]);
        for(size_t field=0;field<arrays.size();++field) {
            auto* property=arrays[field];
            FScriptArrayHelper values(property,reinterpret_cast<std::byte*>(instance)+property->GetOffset_Internal());
            for(int32_t i=0;i<7;++i) {
                if(field==1) property->GetInner()->CopyCompleteValue(values.GetRawPtr(i),&value.moments[size_t(i)]);
                else {
                    const auto& source=field==0?value.offsets[size_t(i)]:field==2?value.contact_centers[size_t(i)]:
                        value.contact_axes[size_t(i)][field-3];
                    property->GetInner()->CopyCompleteValue(values.GetRawPtr(i),source.data());
                }
            }
        }
        if(capture()!=value) throw std::runtime_error("Body geometry read-back failed");
        advance_rig_epoch(instance,epoch);
    }
};

BodyGeometryInputs body_geometry_inputs(UObject* instance) {
    const wchar_t* names[]={L"CSSBodyOffsets",L"CSSBodyMoments",L"CSSBodyContactCenters",
        L"CSSBodyContactAxesX",L"CSSBodyContactAxesY",L"CSSBodyContactAxesZ"};
    std::array<FArrayProperty*,6> arrays{};
    for(size_t i=0;i<arrays.size();++i) {
        auto* property=body_geometry_property(instance,names[i]);
        if(!property || !property->IsA<FArrayProperty>() || property->GetElementSize()!=sizeof(TArray<float>))
            throw std::runtime_error("Body geometry requires array fields");
        arrays[i]=static_cast<FArrayProperty*>(property);
        auto* inner=arrays[i]->GetInner();
        if(!inner || inner->GetArrayDim()!=1 || inner->GetOffset_Internal()!=0)
            throw std::runtime_error("Body geometry array element layout mismatch");
        if(i==1) {
            if(!inner->IsA<FFloatProperty>() || inner->GetElementSize()!=sizeof(float))
                throw std::runtime_error("Body geometry inertia requires floats");
        } else if(!inner->IsA<FStructProperty>() || inner->GetElementSize()!=sizeof(BodyVector) ||
                  static_cast<FStructProperty*>(inner)->GetStruct().Get()!=find(L"/Script/CoreUObject.Vector"))
            throw std::runtime_error("Body geometry requires FVector entries");
    }
    auto* epoch=control_rig_reset_epoch(instance,L"CSSBodyResetEpoch");
    if(!epoch) throw std::runtime_error("Body geometry requires independent reset");
    std::array<FFloatProperty*,body_geometry_max_axes> shape{};
    for(size_t i=0;i<shape.size();++i) {
        const auto name=L"CSSBodyShape"+std::to_wstring(i);
        auto* property=body_geometry_property(instance,name.c_str());
        if(!property) continue;
        if(!property->IsA<FFloatProperty>() || property->GetElementSize()!=sizeof(float))
            throw std::runtime_error("Body shape inputs require floats");
        shape[i]=static_cast<FFloatProperty*>(property);
    }
    // Contiguous from CSSBodyShape0: none, or one per axis of a 6 to 8 axis model.
    const auto count=size_t(std::count_if(shape.begin(),shape.end(),[](auto* p){return p!=nullptr;}));
    for(size_t i=0;i<count;++i) if(!shape[i]) throw std::runtime_error("Body shape inputs must be numbered without gaps");
    if(count!=0 && (count<body_geometry_min_axes || count>body_geometry_max_axes))
        throw std::runtime_error("Body shape inputs require one coefficient per model axis");
    return {instance,arrays,epoch,shape,count};
}

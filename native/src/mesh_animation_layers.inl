// Included by engine.cpp. A cross-skeleton mesh swap can recreate the main
// instance and silently discard layers installed when a weapon was equipped.
namespace mesh_animation_layers {
struct Binding {
    int32_t node_offset;
    UObject* target_class;
};

class Snapshot {
    UClass* instance_class_ = nullptr;
    int32_t target_offset_ = 0;
    std::vector<Binding> bindings_;
    std::vector<UObject*> overrides_;
    AssetLoadRoots roots_;

    static UObject* node_object(const void* object, int32_t node_offset, int32_t field_offset) {
        UObject* value = nullptr;
        std::memcpy(&value, static_cast<const std::byte*>(object) + node_offset + field_offset, sizeof value);
        return value;
    }
    static int32_t object_offset(UScriptStruct* type, const wchar_t* name) {
        auto* property = field(type, name, sizeof(UObject*));
        const auto offset = property->GetOffset_Internal();
        if (!property->IsA<FObjectProperty>() || offset < 0 ||
            offset + int32_t(sizeof(UObject*)) > type->GetPropertiesSize())
            throw std::runtime_error("Linked animation layer object layout mismatch");
        return offset;
    }
public:
    explicit Snapshot(UObject* component) {
        Call get(component, L"GetAnimInstance", 1); get.run();
        auto* instance = get.get<UObject*>();
        if (!instance) return;
        instance_class_ = instance->GetClassPrivate();
        auto* defaults = instance_class_->GetClassDefaultObject().Get();
        if (!defaults) throw std::runtime_error("Animation class defaults are unavailable");
        auto* node_type = static_cast<UScriptStruct*>(find(L"/Script/Engine.AnimNode_LinkedAnimLayer"));
        target_offset_ = object_offset(node_type, L"TargetInstance");
        const auto class_offset = object_offset(node_type, L"InstanceClass");
        roots_.keep(instance_class_);
        for (auto* property : instance_class_->ForEachProperty()) {
            if (!property->IsA<FStructProperty>() ||
                static_cast<FStructProperty*>(property)->GetStruct().Get() != node_type) continue;
            const auto offset = property->GetOffset_Internal();
            if (property->GetArrayDim() != 1 || offset < 0 ||
                property->GetElementSize() != node_type->GetPropertiesSize() ||
                offset + property->GetElementSize() > instance_class_->GetPropertiesSize())
                throw std::runtime_error("Linked animation layer node layout mismatch");
            auto* target = node_object(instance, offset, target_offset_);
            if (!target || target == instance) continue;
            if (bindings_.size() >= 128) throw std::runtime_error("Too many linked animation layers");
            auto* target_class = target->GetClassPrivate();
            bindings_.push_back({offset, target_class});
            roots_.keep(target_class);
            auto* default_class = node_object(defaults, offset, class_offset);
            if (target_class != default_class &&
                std::find(overrides_.begin(), overrides_.end(), target_class) == overrides_.end())
                overrides_.push_back(target_class);
        }
    }

    void restore(UObject* component) const {
        if (overrides_.empty()) return;
        Call get(component, L"GetAnimInstance", 1); get.run();
        auto* current = get.get<UObject*>();
        if (!current || current->GetClassPrivate() != instance_class_)
            throw std::runtime_error("Mesh replacement changed the player animation class");
        for (auto* type : overrides_) {
            const bool missing = std::any_of(bindings_.begin(), bindings_.end(), [&](const Binding& binding) {
                if (binding.target_class != type) return false;
                auto* target = node_object(current, binding.node_offset, target_offset_);
                return !target || target->GetClassPrivate() != type;
            });
            if (!missing) continue;
            Call link(component, L"LinkAnimClassLayers", 1);
            link.set(L"InClass", type); link.run();
        }
        // Verify the complete previous layer assignment, including defaults that
        // could otherwise be displaced by a class implementing several layers.
        for (const auto& binding : bindings_) {
            auto* target = node_object(current, binding.node_offset, target_offset_);
            if (!target || target->GetClassPrivate() != binding.target_class)
                throw std::runtime_error("Mesh replacement did not preserve an active animation layer");
        }
    }
};
}

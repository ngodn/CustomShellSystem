// Transforms belong to one preview instance, never a shared light template.
bool InventoryUI::light_available() const {
    if(!active_ || extension_active_ || closing_ || native_picker_ || !confirm_action_.is_null() ||
       !physics_modal_control_.empty() ||
       !inventory_bool(main_.Get(),L"bOpen") || !display_.Get() || !camera_component_.Get()) return false;
    auto* native=inventory_object(main_.Get(),L"WBP_MGT_Character");
    if(!native || !native->GetFunctionByNameInChain(L"IsMenuOpen")) return false;
    Call open(native,L"IsMenuOpen",1);open.run();
    if(open.get<bool>()) return false;
    // The native page closes and invalidates its inventory before CSS owns
    // Inspect. Refuse the shortcut if that lifecycle has not settled yet.
    for(auto* name:{L"WBP_IL_Inventory_Top",L"WBP_IL_Character_Secondary",
                    L"WBP_IL_Character_Navigation",L"WBP_IL_Character_Back"}) {
        auto* listener=inventory_object(native,name);
        auto* enabled=listener?listener->GetPropertyByNameInChain(L"bEnabled"):nullptr;
        if(!enabled || !enabled->IsA<FBoolProperty>() || inventory_bool(listener,L"bEnabled")) return false;
    }
    return inventory_object(display_.Get(),L"RectLight_Left")!=nullptr;
}
void InventoryUI::light_start() {
    if(!light_available())
        throw std::runtime_error("Light controls require the CSS character preview");
    Call selected(switcher_.Get(),L"GetActiveWidget",1);selected.run();
    if(selected.get<UObject*>()!=page_.Get())
        throw std::runtime_error("Light controls require the selected CSS page");
    if(light_edit_) return;
    auto* component=inventory_object(display_.Get(),L"RectLight_Left");
    auto* character=inventory_object(display_.Get(),L"CharacterChildActor");
    if(!component || !character || component->GetClassPrivate()->GetPathName()!=L"/Script/Engine.RectLightComponent" ||
       component->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))
        throw std::runtime_error("Preview key light unavailable");
    auto vector=[](UObject* object,const wchar_t* name) {
        Call call(object,name,1);call.run();return call.get<LightVector>();
    };
    InventoryLightOrbit orbit;
    orbit.pivot=vector(character,L"K2_GetComponentLocation");
    orbit.position=vector(component,L"K2_GetComponentLocation");
    orbit.forward=vector(component,L"GetForwardVector");
    orbit.right=vector(component,L"GetRightVector");
    orbit.up=vector(component,L"GetUpVector");
    orbit.screen_right=vector(camera_component_.Get(),L"GetRightVector");
    orbit.screen_up=vector(camera_component_.Get(),L"GetUpVector");
    orbit.validate();
    if(light_component_.Get()!=component) {
        light_stop();
        const auto location=read<LightVector>(component,L"RelativeLocation");
        const auto rotation=read<LightVector>(component,L"RelativeRotation");
        if(!InventoryLightOrbit::finite(location) || !InventoryLightOrbit::finite(rotation))
            throw std::runtime_error("Invalid preview light restore transform");
        light_location_before_=location;light_rotation_before_=rotation;
    }
    light_component_=component;light_orbit_=orbit;light_edit_=true;
    motion_.reset();drag_pan_=drag_rotate_=false;
#ifdef CSS_INVENTORY_DEV
    capture_duration_=0;
#endif
}
void InventoryUI::light_toggle() {
    if(light_edit_) {
        // Returning to view controls retains the edited light. Keep the first
        // original transform until explicit reset, page exit or actor replacement.
        light_edit_=false;motion_.reset();drag_pan_=drag_rotate_=false;
    } else {
        if(!light_available()) return;
        light_start();
    }
    dirty_=true;
}
void InventoryUI::light_reset() {
    const bool editing=light_edit_;
    light_stop();
    if(editing && light_available()) light_start();
    dirty_=true;
}
void InventoryUI::light_move(double horizontal,double vertical) {
    if(!light_edit_ || (!horizontal && !vertical)) return;
    auto* component=light_component_.Get();
    if(!active_ || extension_active_ || !inventory_bool(main_.Get(),L"bOpen") ||
       !component || component!=inventory_object(display_.Get(),L"RectLight_Left")) {
        light_stop();return;
    }
    auto next=light_orbit_;next.move(horizontal,vertical);
    auto* math=find(L"/Script/Engine.Default__KismetMathLibrary");
    Call rotation(math,L"MakeRotationFromAxes",4);
    rotation.set(L"Forward",next.direction(next.forward));
    rotation.set(L"Right",next.direction(next.right));
    rotation.set(L"Up",next.direction(next.up));rotation.run();
    Call move(component,L"K2_SetWorldLocationAndRotation",5);
    move.set(L"NewLocation",next.location());move.set(L"NewRotation",rotation.get<LightVector>());
    move.set(L"bSweep",false);move.set(L"bTeleport",true);move.run();
    light_orbit_=next;
}
void InventoryUI::light_stop() {
    if(auto* component=light_component_.Get()) {
        Call restore(component,L"K2_SetRelativeLocationAndRotation",5);
        restore.set(L"NewLocation",light_location_before_);restore.set(L"NewRotation",light_rotation_before_);
        restore.set(L"bSweep",false);restore.set(L"bTeleport",true);restore.run();
    }
    light_component_.Reset();light_edit_=false;light_orbit_={};
    motion_.reset();drag_pan_=drag_rotate_=false;
}

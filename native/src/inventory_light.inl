// The mode entry is development-only until native Inspect routing is verified.
// Transforms belong to one preview instance, never a shared light template.
void InventoryUI::light_start() {
    if(!active_ || extension_active_ || !inventory_bool(main_.Get(),L"bOpen") ||
       !display_.Get() || !camera_component_.Get())
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
    light_location_before_=read<LightVector>(component,L"RelativeLocation");
    light_rotation_before_=read<LightVector>(component,L"RelativeRotation");
    if(!InventoryLightOrbit::finite(light_location_before_) || !InventoryLightOrbit::finite(light_rotation_before_))
        throw std::runtime_error("Invalid preview light restore transform");
    light_component_=component;light_orbit_=orbit;light_edit_=true;
    motion_.reset();drag_pan_=drag_rotate_=false;
#ifdef CSS_INVENTORY_DEV
    capture_duration_=0;
#endif
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

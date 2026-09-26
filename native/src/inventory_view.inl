namespace css {
namespace {
template<class T> void inventory_value(UObject* object,const wchar_t* name,const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto* p=field(object,name,sizeof(T));
    std::memcpy(reinterpret_cast<std::byte*>(object)+p->GetOffset_Internal(),&value,sizeof(T));
}
constexpr Color inventory_ink{.287441f,.250158f,.223228f,1};
constexpr Color inventory_dim{.18f,.16f,.13f,1};
struct InventoryLayout : Layout {
    UObject* title_font;
    UObject* label(const std::string& text,double x,double y,double w,double h,float size,Color color=inventory_ink) {
        auto* widget=Layout::label(text,x,y,w,h,size,color);
        font_size(widget,size*float(scale),size>=22?title_font:serif);
        return widget;
    }
    UObject* button(const std::string& text,double x,double y,double w,double h,bool active=false,bool enabled=true,float size=20) {
        auto* widget=Layout::button(text,x,y,w,h,false,enabled,size);
        invoke(widget,L"SetBackgroundColor",L"InBackgroundColor",Color{1,1,1,1});
        Call child(widget,L"GetContent",1); child.run(); auto* text_block=child.get<UObject*>();
        font_size(text_block,size*float(scale),size>=22?title_font:serif);
        invoke(text_block,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{enabled?(active?Color{.55f,.48f,.34f,1}:inventory_ink):inventory_dim});
        return widget;
    }
    void selection_mark(double x,double y,bool selected) {
        auto* frame=box(x,y,12,12,Color{.31f,.275f,.225f,1});invoke(frame,L"SetRenderTransformAngle",L"Angle",45.f);
        auto* inner=box(x+2,y+2,8,8,Color{.01f,.008f,.006f,1});invoke(inner,L"SetRenderTransformAngle",L"Angle",45.f);
        if(selected) {auto* dot=box(x+4,y+4,4,4,Color{.42f,.34f,.22f,1});invoke(dot,L"SetRenderTransformAngle",L"Angle",45.f);}
    }
};
enum class BodyPhysicsRegion : uint8_t {
    Chest,
    Glute,
    Thigh,
    Belly,
    Unknown
};

static inline BodyPhysicsRegion classify_body_physics_region(const std::string& raw_id, const std::string& raw_name = "") {
    std::string id = raw_id;
    for(char& c : id) c = char(std::tolower(static_cast<unsigned char>(c)));
    std::string name = raw_name;
    for(char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));

    if(id.find("chest") != std::string::npos || id.find("breast") != std::string::npos ||
       id.find("boob") != std::string::npos || id.find("bust") != std::string::npos ||
       name.find("chest") != std::string::npos || name.find("breast") != std::string::npos ||
       name.find("boob") != std::string::npos || name.find("bust") != std::string::npos) {
        return BodyPhysicsRegion::Chest;
    }
    if(id.find("glute") != std::string::npos || id.find("butt") != std::string::npos ||
       name.find("glute") != std::string::npos || name.find("butt") != std::string::npos) {
        return BodyPhysicsRegion::Glute;
    }
    if(id.find("thigh") != std::string::npos || id.find("hip") != std::string::npos ||
       name.find("thigh") != std::string::npos || name.find("hip") != std::string::npos) {
        return BodyPhysicsRegion::Thigh;
    }
    if(id.find("belly") != std::string::npos || id.find("waist") != std::string::npos ||
       id.find("abdomen") != std::string::npos || id.find("stomach") != std::string::npos ||
       name.find("belly") != std::string::npos || name.find("waist") != std::string::npos ||
       name.find("abdomen") != std::string::npos || name.find("stomach") != std::string::npos) {
        return BodyPhysicsRegion::Belly;
    }
    return BodyPhysicsRegion::Unknown;
}

static inline bool is_body_physics_control(const Control& control) {
    if(control.kind != ControlKind::Spring && control.kind != ControlKind::Dynamics && control.kind != ControlKind::Rig) {
        return false;
    }
    return classify_body_physics_region(control.id, control.name) != BodyPhysicsRegion::Unknown;
}

static inline bool is_chest_or_glute_control(const Control& control) {
    return is_body_physics_control(control);
}

struct BodyPhysicsPresetDef {
    const char* id;
    const char* name;
    const char* subtitle;
    // Rig values
    float chest_freq, chest_damp, chest_motion;
    float glute_freq, glute_damp, glute_motion;
    float thigh_freq, thigh_damp, thigh_motion;
    float belly_freq, belly_damp, belly_motion;
    // Spring values
    float spring_chest_freq, spring_chest_damp, spring_chest_travel;
    float spring_glute_freq, spring_glute_damp, spring_glute_travel;
    float spring_thigh_freq, spring_thigh_damp, spring_thigh_travel;
    float spring_belly_freq, spring_belly_damp, spring_belly_travel;
};

static const BodyPhysicsPresetDef kBodyPhysicsPresets[] = {
    {"firm", "Firm", "High stiffness & damping, perky sculpted look",
     // Rig: Chest, Glute, Thigh, Belly
     2.60f, 0.65f, 0.60f,
     2.50f, 0.65f, 0.65f,
     2.80f, 0.72f, 0.40f,
     2.70f, 0.68f, 0.45f,
     // Spring: Chest, Glute, Thigh, Belly
     2.60f, 0.65f, 1.20f,
     2.50f, 0.65f, 1.50f,
     2.80f, 0.72f, 0.80f,
     2.70f, 0.68f, 0.90f},

    {"natural", "Natural", "Realistic soft-tissue sway, balanced & restrained",
     // Rig: Chest, Glute, Thigh, Belly
     2.15f, 0.48f, 1.00f,
     2.10f, 0.50f, 1.00f,
     2.35f, 0.58f, 0.75f,
     2.20f, 0.52f, 0.85f,
     // Spring: Chest, Glute, Thigh, Belly
     2.15f, 0.48f, 2.20f,
     2.10f, 0.50f, 2.20f,
     2.35f, 0.58f, 1.60f,
     2.20f, 0.52f, 1.80f},

    {"bouncy", "Bouncy", "Playful, energetic motion with plenty of bounce",
     // Rig: Chest, Glute, Thigh, Belly
     1.70f, 0.28f, 1.80f,
     1.65f, 0.30f, 1.80f,
     1.85f, 0.38f, 1.35f,
     1.75f, 0.32f, 1.50f,
     // Spring: Chest, Glute, Thigh, Belly
     1.70f, 0.28f, 4.50f,
     1.65f, 0.30f, 4.50f,
     1.85f, 0.38f, 3.20f,
     1.75f, 0.32f, 3.80f},

    {"soft", "Soft / Saggy", "Heavier relaxed tissue, slow and swinging",
     // Rig: Chest, Glute, Thigh, Belly
     1.25f, 0.18f, 2.60f,
     1.20f, 0.20f, 2.50f,
     1.40f, 0.25f, 2.00f,
     1.30f, 0.22f, 2.20f,
     // Spring: Chest, Glute, Thigh, Belly
     1.25f, 0.18f, 7.50f,
     1.20f, 0.20f, 7.00f,
     1.40f, 0.25f, 5.50f,
     1.30f, 0.22f, 6.50f},

    {"earthquake", "OMG! Earthquake!", "Maximum exaggerated comedic jiggle & wobble",
     // Rig: Chest, Glute, Thigh, Belly
     0.85f, 0.06f, 4.20f,
     0.85f, 0.08f, 4.20f,
     0.95f, 0.10f, 3.50f,
     0.90f, 0.08f, 3.80f,
     // Spring: Chest, Glute, Thigh, Belly
     0.85f, 0.06f, 14.0f,
     0.85f, 0.08f, 14.0f,
     0.95f, 0.10f, 10.0f,
     0.90f, 0.08f, 13.0f}
};

static inline std::string detect_body_physics_preset(const Control& control, const ControlValue& held) {
    const auto region = classify_body_physics_region(control.id, control.name);
    const bool is_spring = control.kind == ControlKind::Spring;
    for(const auto& p : kBodyPhysicsPresets) {
        float tf = 0, td = 0;
        if(is_spring) {
            switch(region) {
                case BodyPhysicsRegion::Chest: tf = p.spring_chest_freq; td = p.spring_chest_damp; break;
                case BodyPhysicsRegion::Glute: tf = p.spring_glute_freq; td = p.spring_glute_damp; break;
                case BodyPhysicsRegion::Thigh: tf = p.spring_thigh_freq; td = p.spring_thigh_damp; break;
                case BodyPhysicsRegion::Belly: tf = p.spring_belly_freq; td = p.spring_belly_damp; break;
                default: tf = p.spring_chest_freq; td = p.spring_chest_damp; break;
            }
        } else {
            switch(region) {
                case BodyPhysicsRegion::Chest: tf = p.chest_freq; td = p.chest_damp; break;
                case BodyPhysicsRegion::Glute: tf = p.glute_freq; td = p.glute_damp; break;
                case BodyPhysicsRegion::Thigh: tf = p.thigh_freq; td = p.thigh_damp; break;
                case BodyPhysicsRegion::Belly: tf = p.belly_freq; td = p.belly_damp; break;
                default: tf = p.chest_freq; td = p.chest_damp; break;
            }
        }
        if(std::abs(held[0] - tf) < 0.18f && std::abs(held[1] - td) < 0.08f) {
            return p.id;
        }
    }
    return "custom";
}

static inline std::string get_body_physics_preset_name(const std::string& id) {
    for(const auto& p : kBodyPhysicsPresets) {
        if(p.id == id) return p.name;
    }
    return "Custom";
}

struct HairPhysicsPresetDef {
    const char* id;
    const char* name;
    const char* subtitle;
    float stiffness;
    float damping;
    float gravity;
};

static const HairPhysicsPresetDef kHairPhysicsPresets[] = {
    {"firm", "Firm", "Clean, disciplined ponytail with hairspray hold", 260.0f, 26.0f, 0.04f},
    {"natural", "Natural", "Eve's athletic flow, quick and steady", 180.0f, 16.0f, 0.08f},
    {"silky", "Silky", "Soft, elegant hair with loose fluid sway", 110.0f, 10.0f, 0.14f},
    {"heavy", "Heavy", "Dense weighted hair, hugs back and resists lift", 190.0f, 22.0f, 0.35f},
    {"floaty", "Floaty", "Light strands with a slow trailing wave", 55.0f, 6.0f, 0.00f}
};

static inline bool is_hair_physics_control(const Control& control) {
    if(control.kind != ControlKind::Rig) return false;
    if(control.rig && control.rig->body) return false;
    std::string id = control.id;
    for(char& c : id) c = char(std::tolower(static_cast<unsigned char>(c)));
    return id.find("hair") != std::string::npos || id.find("ponytail") != std::string::npos;
}

static inline std::string detect_hair_physics_preset(const Control& control, const ControlValue& held) {
    for(const auto& p : kHairPhysicsPresets) {
        if(std::abs(held[0] - p.stiffness) < 18.0f &&
           std::abs(held[1] - p.damping) < 2.5f &&
           std::abs(held[2] - p.gravity) < 0.04f) {
            return p.id;
        }
    }
    return "custom";
}

static inline std::string get_hair_physics_preset_name(const std::string& id) {
    for(const auto& p : kHairPhysicsPresets) {
        if(p.id == id) return p.name;
    }
    return "Custom";
}
// 0.4: a short, deterministic strip of colours for one part, so a controller can pick
// one without anybody having to think in RGB. What the author chose comes first, then
// what each palette gives this part, then a hue ring and a brightness ramp off the
// author's own colour. Exact RGB is still there behind a toggle for when you want it.
std::vector<ColorSwatch> color_swatches(const ControlSet& options,const Control& control) {
    if(!control.swatches.empty()) return control.swatches;
    std::vector<ColorSwatch> out;
    auto add=[&](ControlValue value) {
        for(size_t i=0;i<3;++i) value[i]=std::clamp(value[i],control.minimum,control.maximum);
        for(const auto& had:out)
            if(std::abs(had.color[0]-value[0])+std::abs(had.color[1]-value[1])+std::abs(had.color[2]-value[2])<.03f) return;
        // Four rows of six. The panel has the room for it now that it starts under the
        // top bar, and a wider strip is the whole point of picking rather than mixing.
        if(out.size()<24) out.push_back({"",value,false});
    };
    add(control.value);
    for(const auto& palette:options.palettes) {
        auto found=palette.values.find(control.id);
        if(found!=palette.values.end()) add(found->second);
    }
    // A hue-locked part is a material rather than a colour, so its strip stays on its
    // own hue and offers depth instead: rotating it is the thing that looks broken.
    if(control.hue_locked) {
        for(float scale:{.5f,.65f,.8f,.9f,1.1f,1.25f,1.45f,1.7f}) add(apply_tint({0,1,scale},control.value,false));
        for(float saturation:{.35f,.6f,.8f,1.25f,1.6f}) add(apply_tint({0,saturation,1},control.value,true));
    } else {
        for(int step=1;step<12;++step) add(apply_tint({step*30.f,1,1},control.value,false));
        for(float scale:{.55f,.75f,.9f,1.15f,1.4f,1.7f}) add(apply_tint({0,1,scale},control.value,false));
        for(float saturation:{.4f,.7f,1.35f}) add(apply_tint({0,saturation,1},control.value,false));
    }
    return out;
}
size_t nearest_swatch(const std::vector<ColorSwatch>& swatches,const ControlValue& value,bool inherited=false) {
    if(inherited && swatches.front().reset) return 0;
    size_t best=0; float closest=1e9f;
    for(size_t i=0;i<swatches.size();++i) {
        if(swatches[i].reset) continue;
        float distance=0;
        for(size_t c=0;c<3;++c) distance+=(swatches[i].color[c]-value[c])*(swatches[i].color[c]-value[c]);
        if(distance<closest) { closest=distance; best=i; }
    }
    return best;
}
// The menu asks this for every mapped key every frame; each key's FName is made once.
FName key_name(const std::string& key) {
    static std::unordered_map<std::string,FName> names;
    if(auto it=names.find(key);it!=names.end()) return it->second;
    return names.emplace(key,FName(wide(key).c_str())).first->second;
}
bool inventory_key(UObject* pc,const std::string& key) {
    Call call(pc,L"IsInputKeyDown",2); auto* p=call.param(L"Key");
    member(call.data(p),p->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",key_name(key));
    call.run(); return call.get<bool>();
}
std::string inventory_text(UObject* widget,int limit=256) {
    if(!widget) return {};
    Call text(widget,L"GetText",1); text.run();
    Call convert(find(L"/Script/Engine.Default__KismetTextLibrary"),L"Conv_TextToString",2);
    convert.copy(L"InText",text,L"ReturnValue"); convert.run();
    const auto& value=*static_cast<FString*>(convert.data(convert.param(L"ReturnValue")));
    const auto& chars=value.GetCharArray();
    if(chars.Num()>limit) throw std::runtime_error("Text is too long");
    return chars.Num()?narrow(std::wstring(chars.GetData())):std::string{};
}
}
}
#include "inventory_native.inl"
namespace css {
void InventoryUI::bind_inputs() {
    bindings_.clear();
    auto* pc=controller_.Get(); auto* handler=inventory_object(pc,L"User Interface Handler Component");
    auto* mapping=inventory_object(handler,L"InputMapping");
    auto* p=mapping?mapping->GetPropertyByNameInChain(L"Mappings"):nullptr;
    if(!p || !p->IsA<FArrayProperty>()) throw std::runtime_error("Native inventory input mapping is unavailable");
    Call subsystem(find(L"/Script/Engine.Default__SubsystemBlueprintLibrary"),L"GetLocalPlayerSubSystemFromPlayerController",3);
    subsystem.set(L"PlayerController",pc); subsystem.set(L"Class",static_cast<UClass*>(find(L"/Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem"))); subsystem.run();
    auto* input=subsystem.get<UObject*>();
    if(!input) throw std::runtime_error("Native player input subsystem is unavailable");
    const std::map<std::wstring,std::string> actions={
        {L"IA_Menu_Up","up"},{L"IA_Menu_Down","down"},
        {L"IA_Menu_Left_Primary","left"},{L"IA_Menu_Right_Primary","right"},
        {L"IA_Menu_Left_Tertiary","previous_section"},{L"IA_Menu_Right_Tertiary","next_section"},
        {L"IA_Menu_Confirm_Primary_Press","accept"},{L"IA_Menu_Confirm_Secondary_Press","secondary"},
        {L"IA_Menu_Confirm_Tertiary_Press","tertiary"},{L"IA_Menu_Back","close"},
        {L"IA_Menu_Inspect","toggle_light"}};
    auto* a=static_cast<FArrayProperty*>(p); FScriptArrayHelper values(a,reinterpret_cast<std::byte*>(mapping)+p->GetOffset_Internal());
    if(values.Num()<0 || values.Num()>256) throw std::runtime_error("Native input map exceeds bound");
    auto* ap=field(find(L"/Script/EnhancedInput.EnhancedActionKeyMapping"),L"Action",8);
    auto* kn=field(find(L"/Script/InputCore.Key"),L"KeyName",sizeof(FName));
    std::set<UObject*> seen;
    std::set<std::string> reserved_keys={"Home","Gamepad_RightThumbstick"};
    for(int i=0;i<values.Num();++i) {
        UObject* action{}; std::memcpy(&action,values.GetRawPtr(i)+ap->GetOffset_Internal(),8);
        if(!action || !seen.insert(action).second) continue;
        Binding binding; binding.input_action=action;
        if(actions.contains(action->GetName())) binding.action=actions.at(action->GetName());
        Call query(input,L"QueryKeysMappedToAction",2); query.set(L"Action",action); query.run();
        auto* out=query.param(L"ReturnValue");
        if(!out->IsA<FArrayProperty>()) throw std::runtime_error("Mapped input keys are not an array");
        auto* array=static_cast<FArrayProperty*>(out); FScriptArrayHelper keys(array,query.data(out));
        if(keys.Num()<0 || keys.Num()>32 || kn->GetOffset_Internal()+8>array->GetInner()->GetElementSize()) throw std::runtime_error("Mapped input key layout mismatch");
        for(int n=0;n<keys.Num();++n) {
            FName key{}; std::memcpy(&key,keys.GetRawPtr(n)+kn->GetOffset_Internal(),sizeof(key));
            auto name=narrow(key.ToString());
            if(binding.action!="toggle_light" && binding.action!="tertiary") reserved_keys.insert(name);
            // The analog sticks belong to the character view on this page.
            if(name=="Gamepad_LeftX" || name=="Gamepad_LeftY" || name=="Gamepad_RightX" || name=="Gamepad_RightY") continue;
            binding.keys.push_back(name);
        }
        if(!binding.action.empty()) bindings_.push_back(std::move(binding));
    }
    // Y controls lighting; Select/View takes the former Y row action. These
    // shortcuts are local to CSS and do not alter the game's input settings.
    inventory_light_keys(bindings_,reserved_keys);
    // Keyboard Inspect must also avoid the unchanged tertiary keyboard action.
    for(const auto& binding:bindings_) if(binding.action=="tertiary")
        for(const auto& key:binding.keys) reserved_keys.insert(key);
    for(auto& binding:bindings_) if(binding.action=="toggle_light")
        std::erase_if(binding.keys,[&](const auto& key){return reserved_keys.contains(key);});
    bindings_.push_back({"reset_view",{"Home","Gamepad_RightThumbstick"},false,0,{}});
}
void InventoryUI::build(const Catalog& catalog,const State& state,Appearance& appearance) {
    auto* page=page_.Get(); auto* canvas=canvas_.Get(); auto* pc=controller_.Get();
    if(!page || !canvas || !pc) return;
#ifdef CSS_INVENTORY_DEV
    LARGE_INTEGER build_start,frequency; QueryPerformanceCounter(&build_start); QueryPerformanceFrequency(&frequency);
    created_widgets_=0;
#endif
    // A reordered list must keep focus on the same outfit, not its old index.
    std::string focused_outfit;
    if(section_==0 && !enter_transition_ && row_>=0 && row_<int(rows_.size())) {
        const auto& action=rows_[row_].tertiary;
        if(action.is_object() && action.value("action","")=="favorite") focused_outfit=action.value("outfit","");
    }
    // This page lives inside the game's scaled menu canvas, not the viewport.
    Call geometry(switcher_.Get(),L"GetCachedGeometry",1); geometry.run();
    Call dimensions(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
    dimensions.copy(L"Geometry",geometry,L"ReturnValue"); dimensions.run();
    auto viewport=dimensions.get<Vec2>();
    if(viewport.y<240 || viewport.x<320) return;
    layout_size_={viewport.x,viewport.y};
    if(!native_page(viewport.x,viewport.y)) return;
    // Widgets persist between builds, so a button held down across a build is still the
    // same press: carry each one's state over instead of seeing a fresh click.
    std::map<UObject*,bool> held;
    for(const auto& hit:hits_) if(auto* w=hit.widget.Get()) held[w]=hit.down;
    hits_.clear(); rows_.clear(); sliders_.clear(); name_input_.Reset(); native_search_input_.Reset();
    for(auto* stack:{&tab_items_,&list_,&panel_head_,&panel_,&actions_,&footer_,&camera_bar_}) stack->used=0;
    auto bind=[&](const WeakObject& widget,Json action) {
        auto* w=widget.Get(); if(!w || action.is_null()) return;
        auto it=held.find(w);
        hits_.push_back({widget,std::move(action),it!=held.end() && it->second,{},{}});
    };
    auto* white=load(native_white);
    // While the search picker is open it owns the details window; the page underneath
    // still lays out its list but leaves the window alone.
    bool panel_open=true;
    auto has_focus=[](UObject* widget) { if(!widget) return false; Call focus(widget,L"HasKeyboardFocus",1); focus.run(); return focus.get<bool>(); };
    const Color gold_chip{.42f,.34f,.22f,1};

    // ---- tabs
    const char* sections[]={"Shell","Customize","Locomotion","Misc","Profile"};
    constexpr int section_count=5;
    UObject* selected_tab=nullptr;
    for(int i=0;i<section_count;++i) {
        auto& tab=native_take(tab_items_,NativeKind::tab);
        auto* widget=tab.widget.Get();
        if(tab.text!=sections[i]) { native_text_property(widget,L"Text",sections[i]); invoke(widget,L"UpdateText"); tab.text=sections[i]; }
        native_state(tab,i==section_);
        if(i==section_) selected_tab=widget;
        bind(tab.hit,{{"action","ui_section"},{"section",i}});
    }
    native_finish(tab_items_);
    for(const bool next:{false,true}) {
        const auto& strip=next?strip_next_:strip_previous_;
        bind(strip,{{"action","ui_press"},{"binding",next?"next_section":"previous_section"}});
        if(!hits_.empty() && hits_.back().widget.Get()==strip.Get()) hits_.back().glyph=next?strip_next_glyph_:strip_previous_glyph_;
    }
    if(shown_section_!=section_ && selected_tab) if(auto* strip=strip_scroll_.Get()) {
        Call reveal(strip,L"ScrollWidgetIntoView",4); reveal.set(L"WidgetToFind",selected_tab);
        reveal.set(L"AnimateScroll",true); reveal.set(L"ScrollDestination",uint8_t{2}); reveal.set(L"Padding",0.f); reveal.run();
    }

    // ---- shared pieces
    auto selection=state.selections.find(appearance.shell);
    const Outfit* worn=nullptr;
    if(selection!=state.selections.end()) for(const auto& outfit:catalog.outfits) if(outfit.id==selection->second.outfit) worn=&outfit;
    auto thumbnail=[&](const Outfit& outfit)->UObject* {
        if(outfit.thumbnail.empty() || !fs::exists(outfit.thumbnail)) return nullptr;
        auto key=path_utf8(outfit.thumbnail); auto& cached=textures_[key]; auto* texture=cached.Get();
        if(!texture) {
            Call import(find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"ImportFileAsTexture2D",3);
            import.set(L"WorldContextObject",pc); import.set(L"Filename",FString(outfit.thumbnail.c_str())); import.run(); texture=import.get<UObject*>(); cached=texture;
        }
        return texture;
    };
    // A native list row: thumbnail or colour chip in its icon slot, the name, the E badge
    // for what is worn or active, and the selected glow.
    // `reserve` keeps the icon slot open when there is no picture, so names in a list where
    // some rows carry a chip still line up.
    struct RowLook { UObject* icon=nullptr; const Color* chip=nullptr; bool badge=false, enabled=true, reserve=false; };
    auto fill_row=[&](NativeItem& item,const std::string& title,const RowLook& look,bool selected) {
        auto* widget=item.widget.Get();
        native_text(item.text_block.Get(),item.text,title);
        if(item.badge!=int(look.badge)) { native_visibility(native_part(widget,L"O_Equipped"),look.badge?shown_self_passive:uint8_t{2}); item.badge=look.badge; }
        const int mode=look.icon?1:look.chip?2:look.reserve?3:0;
        if(item.icon_shown!=mode) {
            auto* box=native_part(widget,L"SizeBox_Icon");
            auto* image=native_part(widget,L"Image_Icon");
            native_visibility(box,mode?shown_self_passive:collapsed);
            if(mode) { invoke(box,L"SetWidthOverride",L"InWidthOverride",110.f); invoke(box,L"SetHeightOverride",L"InHeightOverride",110.f); }
            // A picture fills the slot; a colour chip sits small in its middle, the size of
            // the E badge beside it, so it reads as a swatch rather than a blank tile.
            native_visibility(image,mode==3?uint8_t{2}:shown_self_passive);
            native_padding(inventory_object(image,L"Slot"),mode==2?Margin{31,31,31,31}:Margin{0,0,0,0});
            item.icon_shown=mode;
        }
        if(mode==1 || mode==2) {
            auto* image=native_part(widget,L"Image_Icon");
            UObject* texture=look.icon?look.icon:white;
            const Color tint=look.chip?*look.chip:Color{1,1,1,1};
            if(item.icon!=texture) { native_brush(image,texture); item.icon=texture; item.chip={-1,-1,-1,-1}; }
            const std::array<float,4> wanted{tint.r,tint.g,tint.b,tint.a};
            // The row blueprint drives Image_Icon's colour, so the chip tints the brush.
            if(item.chip!=wanted) { invoke(image,L"SetBrushTintColor",L"TintColor",SlateColor{tint}); item.chip=wanted; }
        }
        if(item.enabled!=int(look.enabled)) { invoke(widget,L"SetRenderOpacity",L"InOpacity",look.enabled?1.f:.45f); item.enabled=look.enabled; }
        native_state(item,selected);
    };
    RowLook row_look; row_look.reserve=true;   // one name column on every tab, pictures or not
    WeakObject heading;   // the header just placed, until a row claims it
    auto section=[&](const std::string& title) {
        auto& item=native_take(list_,NativeKind::header);
        native_text(item.text_block.Get(),item.text,title);
        heading=item.widget;
    };
    auto list_note=[&](const std::string& text) {
        auto& item=native_take(list_,NativeKind::paragraph);
        native_text(item.text_block.Get(),item.text,text);
    };
    auto row=[&](int index,const std::string& title,Json accept,Json previous=Json{},Json next=Json{},Json secondary=Json{},Json tertiary=Json{}) {
        auto& item=native_take(list_,NativeKind::row);
        fill_row(item,title,row_look,index==row_);
        bind(item.hit,{{"action","ui_row"},{"row",index},{"apply",false}});
        rows_.push_back({item.widget,item.hit,accept,previous,next,secondary,tertiary,heading});
        heading.Reset();
        const bool reserve=row_look.reserve;
        row_look=RowLook{}; row_look.reserve=reserve;
    };
    UObject* detail_texture=nullptr;
    UObject* panel_focus=nullptr;   // what the details window scrolls to keep in view
    auto note=[&](const std::string& text) {
        if(!panel_open) return;
        auto& item=native_take(panel_,NativeKind::paragraph);
        native_text(item.text_block.Get(),item.text,text);
    };
    // The window's description area has no scroll and grows with its text, so a long author
    // description goes to the top of the scrolling part instead, where all of it stays readable.
    auto detail=[&](const std::string& title,const std::string& subtitle,std::string body) {
        if(!panel_open) return;
        constexpr size_t description_limit=360;
        if(body.size()>description_limit) { note(body); body.clear(); }
        auto* d=details_.Get();
        if(detail_title_!=title) { text_value(native_part(d,L"MyHeader"),title); detail_title_=title; }
        if(detail_sub_!=subtitle) {
            auto* box=native_part(d,L"Size_SubHeader");
            if(!subtitle.empty()) text_value(native_part(d,L"MySubHeader"),subtitle);
            native_visibility(box,subtitle.empty()?collapsed:shown_self_passive);
            detail_sub_=subtitle;
        }
        if(detail_body_!=body) { native_call_text(d,L"SetDescription",L"InText",body); detail_body_=body; }
    };
    struct Choice { std::string id,label;Json action; };
    auto choice_rows=[&](const std::vector<Choice>& choices,const std::string& selected) {
        if(!panel_open) return;
        for(const auto& choice:choices) {
            auto& item=native_take(panel_,NativeKind::row);
            RowLook look; look.badge=choice.id==selected; look.enabled=!choice.action.is_null();
            fill_row(item,choice.label,look,choice.id==selected);
            if(choice.id==selected) panel_focus=item.widget.Get();
            bind(item.hit,choice.action);
        }
    };
    auto divider=[&](const std::string& title) {
        if(!panel_open) return;
        auto& item=native_take(panel_,NativeKind::divider);
        native_text(item.text_block.Get(),item.text,title);
    };
    // A selector row: name, the current value between two arrows. Left/Right (and the
    // arrows) step it; the row itself is highlighted when it is the one Left/Right drives.
    auto option=[&](const std::string& name,const std::string& value,bool focused,Json minus,Json plus) {
        if(!panel_open) return;
        auto& item=native_take(panel_,NativeKind::option);
        native_text(item.text_block.Get(),item.text,name);
        native_text(item.value_block.Get(),item.value,value);
        native_state(item,focused);
        if(focused) panel_focus=item.widget.Get();
        bind(item.hit_left,minus); bind(item.hit_right,plus);
    };
    // The options menu slider row. The game's bar only steps with its arrows; CSS reads the
    // mouse over the bar itself so a drag still sets the value directly.
    auto slider=[&](const std::string& name,float value,float low,float high,float step,const std::string& readout,
                     bool focused,Json action,bool scalar,const std::string& unit,Json minus,Json plus) {
        if(!panel_open) return;
        auto& item=native_take(panel_,NativeKind::slider);
        native_text(item.text_block.Get(),item.text,name);
        native_text(item.value_block.Get(),item.value,readout);
        const float fill=high>low?std::clamp((value-low)/(high-low),0.f,1.f):0.f;
        if(std::abs(item.fill-fill)>1e-4f) { invoke(item.extra.Get(),L"UpdateProgressBar",L"InPercent",fill); item.fill=fill; }
        native_state(item,focused);
        if(focused) panel_focus=item.widget.Get();
        bind(item.hit_left,minus); bind(item.hit_right,plus);
        action["refresh"]=false;
        sliders_.push_back({item.extra,item.value_block,item.widget,std::move(action),value,scalar,unit,low,high,step});
    };
    auto step_action=[](Json action,int delta) { action["delta"]=delta; action.erase("value"); action.erase("refresh"); return action; };
    // An empty binding is a click-only action: it shows the left mouse button, and is left
    // out on a controller, which has nothing to press for it.
    auto action_button=[&](const std::string& binding,const std::string& label,Json action,uint8_t icon,bool enabled=true) {
        if(!panel_open || (binding.empty() && gamepad_)) return;
        auto& item=native_take(actions_,NativeKind::action);
        native_text(item.text_block.Get(),item.text,label);
        const auto glyph=binding+"/"+std::to_string(icon);
        if(item.glyph!=glyph) {
            auto* prompt=item.extra.Get();
            native_visibility(prompt,shown_passive);
            if(binding.empty()) native_glyph(prompt,"",45,0);   // 0: LeftMouseButton
            else native_glyph(prompt,binding,icon);
            native_visibility(item.cells.front().Get(),collapsed);
            item.glyph=glyph;
        }
        if(item.enabled!=int(enabled)) { invoke(item.widget.Get(),L"SetRenderOpacity",L"InOpacity",enabled?1.f:.45f); item.enabled=enabled; }
        if(enabled) { bind(item.hit,std::move(action)); if(!binding.empty() && !hits_.empty()) hits_.back().glyph=item.extra; }
    };
    // A prompt on one of the bottom bars: one glyph, or two (A / D on a keyboard).
    auto bar_prompt=[&](NativeStack& bar,const std::string& label,Json action,
                        const std::string& first,uint8_t first_icon,uint8_t first_key,
                        const std::string& second={},uint8_t second_icon=0,uint8_t second_key=255) {
        auto& item=native_take(bar,NativeKind::action);
        native_text(item.text_block.Get(),item.text,label);
        const auto glyph=first+"/"+std::to_string(first_icon)+"/"+std::to_string(first_key)+"|"+second+"/"+std::to_string(second_icon)+"/"+std::to_string(second_key);
        if(item.glyph!=glyph) {
            native_glyph(item.extra.Get(),first,first_icon,first_key);
            auto* extra=item.cells.front().Get();
            const bool two=!second.empty() || second_key!=255;
            native_visibility(extra,two?shown_passive:collapsed);
            if(two) native_glyph(extra,second,second_icon,second_key);
            item.glyph=glyph;
        }
        // A prompt that names keys but has no action of its own does what its key does when
        // clicked: each glyph its own key, the label the forward one (S, D).
        if(action.is_null() && !first.empty()) {
            auto key=[](const std::string& binding) { return Json{{"action","ui_press"},{"binding",binding}}; };
            action=key(second.empty()?first:second);
            bind(item.hit,action);
            if(hits_.empty()) return;
            hits_.back().glyph=second.empty()?item.extra:item.cells.front();
            if(!second.empty()) hits_.back().parts={{item.extra,key(first)},{item.cells.front(),key(second)}};
            return;
        }
        bind(item.hit,std::move(action));
        if(!hits_.empty() && hits_.back().widget.Get()==item.hit.Get()) hits_.back().glyph=item.extra;
    };
    std::string hint_vertical, hint_horizontal;

    auto direction_hint=[&](bool horizontal,const std::string& label) { (horizontal?hint_horizontal:hint_vertical)=label; };
    auto keyboard_icon=[](const char* key)->uint8_t {
        for(const auto& [name,value]:inventory_keyboard_icons) if(std::string_view(name)==key) return uint8_t(value);
        return 255;
    };

    // ---- sections
    if(native_picker_) {
        // Search, inline in the details window: the query, then up to eight matches a page.
        detail(native_picker_title_.empty()?"Browse":native_picker_title_,"Search by name or keyword",
               "Type to narrow the list. Up / Down to move, then Select.");
        auto& input=native_take(panel_head_,NativeKind::input);
        native_search_input_=input.extra;
        if(input.value!=native_search_query_) {
            if(!has_focus(input.extra.Get())) text_value(input.extra.Get(),native_search_query_);
            input.value=native_search_query_;
        }
        const auto& search=native_options_;
        // The field and the count stay above the results while they scroll.
        auto& count=native_take(panel_head_,NativeKind::paragraph);
        native_text(count.text_block.Get(),count.text,
            search.matches.empty()?"No matches. Try fewer or shorter words."
            :search.matches.size()==search.options.size()?std::to_string(search.options.size())+" options"
            :std::to_string(search.matches.size())+" of "+std::to_string(search.options.size())+" match");
        // Every match in one scrolling list, bounded so a huge catalog stays a few widgets.
        constexpr size_t shown=100;
        const size_t first=search.selected<shown?0:search.selected-shown+1;
        for(size_t i=first;i<std::min(first+shown,search.matches.size());++i) {
            const auto& found=search.options[search.matches[i]];
            auto& item=native_take(panel_,NativeKind::row);
            fill_row(item,found.at("label").get<std::string>(),RowLook{},i==search.selected);
            if(i==search.selected) panel_focus=item.widget.Get();
            bind(item.hit,{{"action","ui_pick_row"},{"row",i}});
        }
        action_button("accept","Select",{{"action","ui_pick_apply"}},3);
        action_button("close","Back",{{"action","ui_pick_cancel"}},5);
        panel_open=false;
    }
    if(section_==0) {
        const auto ordered=catalog.display_order(worn?worn->id:"",state.favorites);
        const int row_before=row_;
        for(size_t i=0;i<ordered.size();++i) if(ordered[i]->id==focused_outfit) row_=int(i)+3;
        const int total=int(ordered.size())+3;
        row_=std::clamp(row_,0,total-1);
        section("Appearance");
        const Json harbinger{{"action","harbinger_mirror"},{"value",!state.harbinger_mirror}};
        row(0,"Harbinger look",harbinger,harbinger,harbinger);
        row_look.badge=!worn;
        const Json browse_shells{{"action","ui_browse_shells"}};
        row(1,"Original appearance",{{"action","restore"}},{},{},browse_shells);
        const Outfit* originals=nullptr;
        for(const auto& outfit:catalog.outfits) if(outfit.id==original_shells_id) originals=&outfit;
        size_t original_index=0;
        const bool original_worn=originals && worn==originals;
        if(original_worn) for(size_t i=0;i<originals->variants.size();++i)
            if(originals->variants[i].id==selection->second.variant) original_index=i;
        auto wear_original=[&](size_t i) {
            return originals && catalog.compatible(originals->id,appearance.shell)
                ?Json{{"action","select"},{"outfit",originals->id},{"variant",originals->variants[i].id}}:Json{};
        };
        const auto count=originals?originals->variants.size():0;
        row_look.badge=original_worn;
        row(2,"Use Original Shell",wear_original(original_index),count?wear_original((original_index+count-1)%count):Json{},
            count?wear_original(original_worn?(original_index+1)%count:0):Json{});
        // Favorites first under their own header, the way the game groups shells.
        bool in_favorites=false, headed=false;
        for(size_t p=0;p<ordered.size();++p) {
            const auto& outfit=*ordered[p]; size_t v=0;
            const bool favorite=state.favorites.contains(outfit.id);
            if(!headed || (in_favorites && !favorite)) {
                section(favorite?"Favorites":"Custom Shells");
                in_favorites=favorite; headed=true;
            }
            const bool chosen=worn==&outfit;
            if(chosen) for(size_t j=0;j<outfit.variants.size();++j) if(outfit.variants[j].id==selection->second.variant) v=j;
            const bool compatible=catalog.compatible(outfit.id,appearance.shell);
            auto wear=[&](size_t index) { return compatible?Json{{"action","select"},{"outfit",outfit.id},{"variant",outfit.variants[index].id}}:Json{}; };
            row_look.icon=thumbnail(outfit); row_look.badge=chosen; row_look.enabled=compatible;
            row(int(p)+3,outfit.name,wear(v),wear((v+outfit.variants.size()-1)%outfit.variants.size()),wear((v+1)%outfit.variants.size()),browse_shells,{{"action","favorite"},{"outfit",outfit.id}});
        }
        if(ordered.empty()) list_note(catalog.empty_message());
        (void)row_before;
        if(row_==0) {
            detail("Harbinger look",state.harbinger_mirror?"Carry from shell":"Keeps its own",
                   state.harbinger_mirror
                     ?"When you sever out of your shell into the Harbinger, it carries your current shell's look, so losing your shell mid-fight keeps your appearance. Cosmetic only."
                     :"The Harbinger keeps its own saved look. Turn this on to carry your shell's look over automatically when you sever.");
            option("Harbinger",state.harbinger_mirror?"Carry from shell":"Keeps its own",true,harbinger,harbinger);
            action_button("accept",state.harbinger_mirror?"Give Harbinger its own":"Carry look into Harbinger",rows_[0].accept,3);
        } else if(row_==1) {
            detail("Original appearance","Your current shell","Restore the appearance supplied by the game and any installed base replacements. Your shell's abilities stay the same.");
            action_button("accept","Restore original",rows_[1].accept,3);
            action_button("secondary","Search catalog...",{{"action","ui_browse_shells"}},4);
        } else if(row_==2) {
            detail("Use Original Shell",original_worn?originals->variants[original_index].name:"Appearance only",
                "Wear an official shell's appearance. Your current shell keeps its abilities and progress.");
            if(!originals) note("Official shell appearances are unavailable in this session.");
            else {
                std::vector<Choice> choices;
                for(size_t i=0;i<count;++i) choices.push_back({originals->variants[i].id,originals->variants[i].name,wear_original(i)});
                choice_rows(choices,original_worn?originals->variants[original_index].id:"");
                direction_hint(true,"Choose a shell");
                action_button("accept","Wear",rows_[2].accept,3,!rows_[2].accept.is_null());
            }
        } else {
            const auto& outfit=*ordered[row_-3];
            detail_texture=thumbnail(outfit);
            detail(outfit.name,"By "+outfit.author,outfit.description.empty()?"Choose an outfit variant. Appearance changes keep your current shell's abilities.":outfit.description);
            const auto& selected=rows_[row_];
            if(outfit.variants.size()>1) {
                divider("Variant");
                std::vector<Choice> choices;
                for(const auto& v:outfit.variants) choices.push_back({v.id,v.name,
                    catalog.compatible(outfit.id,appearance.shell)?Json{{"action","select"},{"outfit",outfit.id},{"variant",v.id}}:Json{}});
                choice_rows(choices,worn==&outfit?selection->second.variant:"");
                direction_hint(true,"Change variant");
            } else note(outfit.variants.front().name);
            if(!catalog.compatible(outfit.id,appearance.shell)) note("This outfit does not fit the shell you are wearing.");
            action_button("accept","Wear",selected.accept,3,!selected.accept.is_null());
            action_button("tertiary",state.favorites.contains(outfit.id)?"Remove favorite":"Add favorite",selected.tertiary,2);
            action_button("secondary","Search catalog...",{{"action","ui_browse_shells"}},4);
        }
    } else if(section_==1) {
        if(!worn || worn->controls_for(selection->second.variant).controls.empty()) {
            detail("Customize","Nothing to adjust","Wear an outfit that has adjustable parts, and they show up here.");
            list_note("Wear an outfit that has adjustable parts.");
        } else {
            const auto& options=worn->controls_for(selection->second.variant); const auto& custom=selection->second.custom;
            auto values=control_values(options,custom);
            auto palette_action=[&](size_t i) { return Json{{"action","palette"},{"palette",i?options.palettes[i-1].id:"original"}}; };
            struct TemplateItem { std::string id,name,subtitle,kind_name,description; Json action; bool is_palette; };
            std::vector<TemplateItem> tmpl_items;
            tmpl_items.push_back({"original","Original","The author's materials","Original",
                "Original restores the author's own materials exactly, and cannot be tinted.",palette_action(0),true});
            for(size_t i=0;i<options.palettes.size();++i) {
                const auto& p=options.palettes[i];
                tmpl_items.push_back({p.id,p.name,"Colour palette","Palette","The author's colour palette: "+p.name,palette_action(i+1),true});
            }
            for(const auto& t:worn->templates) {
                std::string kind_str="Combination";
                if(t.kind==TemplateKind::Palette) kind_str="Palette";
                else if(t.kind==TemplateKind::Archetype) kind_str="Archetype";
                else if(t.kind==TemplateKind::Physics) kind_str="Physics";
                else if(t.kind==TemplateKind::Hair) kind_str="Hair";
                else if(t.kind==TemplateKind::Jewelry) kind_str="Jewelry";
                else if(t.kind==TemplateKind::Glow) kind_str="Glow";
                else if(t.kind==TemplateKind::Accessory) kind_str="Accessory";
                else if(t.kind==TemplateKind::Fabric) kind_str="Fabric";
                else if(t.kind==TemplateKind::Anatomy) kind_str="Anatomy";
                tmpl_items.push_back({t.id,t.name,kind_str+" Preset",kind_str,
                    t.data.value("description",std::string("Author preset for outfit combination, body archetype, or physics.")),
                    Json{{"action","template"},{"template",t.id}},false});
            }
            size_t active_tmpl=0;
            for(size_t i=0;i<tmpl_items.size();++i) if(tmpl_items[i].is_palette && tmpl_items[i].id==custom.palette) { active_tmpl=i; break; }
            const auto& cur_tmpl=tmpl_items[active_tmpl];
            const size_t total_templates=tmpl_items.size();
            auto tmpl_step=[&](int dir) { return tmpl_items[(active_tmpl+total_templates+dir)%total_templates].action; };
            const bool has_dyed_palette=custom.palette!="original";
            // 0.4: the tab is a template/palette, then one section per group, each opening with the
            // tint that moves everything under it. See docs/control-convention.md.
            struct Entry { bool tint; ControlGroup group; int control; };
            std::vector<Entry> entries;
            entries.push_back({false,ControlGroup::Outfit,-1});
            for(auto group:{ControlGroup::Outfit,ControlGroup::Body}) {
                std::vector<int> members;
                for(size_t i=0;i<options.controls.size();++i) if(options.controls[i].group==group) members.push_back(int(i));
                if(members.empty()) continue;
                const bool tintable=has_dyed_palette && std::any_of(members.begin(),members.end(),[&](int i){return !options.controls[i].scalar;});
                if(tintable) entries.push_back({true,group,-1});
                for(int i:members) entries.push_back({false,group,i});
            }
            row_=std::clamp(row_,0,int(entries.size())-1);
            auto swatch_of=[&](const Control& c) {
                auto v=values.contains(c.id)?values.at(c.id):c.value;
                if(c.kind==ControlKind::Glow) { const float t=std::clamp(v[0]/std::max(c.maximum,.001f),0.f,1.f); return Color{1.f*t+.15f,0.85f*t+.08f,0.3f*t+.03f,1}; }
                if(c.kind==ControlKind::Opacity) { const float a=std::clamp(v[0],0.f,1.f); return Color{0.65f*a+.2f,0.65f*a+.2f,0.7f*a+.2f,1}; }
                if(c.scalar) { const float t=std::clamp(v[0]/std::max(c.maximum,.001f),0.f,1.f); return Color{gold_chip.r*t+.02f,gold_chip.g*t+.02f,gold_chip.b*t+.02f,1}; }
                // A part with the author's own swatches shows the one it is on: "Default" is the
                // original texture's colour, not the white multiplier underneath it.
                if(!c.swatches.empty()) {
                    const auto& chosen=c.swatches[nearest_swatch(c.swatches,v,!custom.values.contains(c.id))].color;
                    return Color{srgb_linear(chosen[0]),srgb_linear(chosen[1]),srgb_linear(chosen[2]),1};
                }
                return Color{srgb_linear(v[0]),srgb_linear(v[1]),srgb_linear(v[2]),1};
            };
            auto tint_of=[&](ControlGroup group) { auto found=custom.tints.find(control_group_name(group)); return found==custom.tints.end()?ColorTint{}:found->second; };
            section("Template");
            const Json browse_templates{{"action","ui_browse_templates"}};
            row(0,cur_tmpl.name,palette_action(0),tmpl_step(-1),tmpl_step(1),browse_templates);
            // Reset all asks first, from the prompt and from its key alike.
            const Json confirm_reset_all{{"action","ui_confirm"},{"title","Reset all customization"},{"message","Reset every change on this outfit back to the author's defaults?"},{"target",palette_action(0)}};
            std::vector<Color> chips(entries.size());
            for(size_t i=1;i<entries.size();++i) {
                const auto& entry=entries[i];
                if(entry.tint) {
                    section(entry.group==ControlGroup::Body?"Body":"Outfit");
                    Json reset={{"action","reset_tint"},{"group",control_group_name(entry.group)}};
                    Json minus={{"action","tint"},{"group",control_group_name(entry.group)},{"field",tint_field_index_==0?"hue":tint_field_index_==1?"saturation":"brightness"},{"delta",-1}},plus=minus; plus["delta"]=1;
                    row(int(i),"Tint",reset,minus,plus,{{"action","ui_tint_field"}});
                    continue;
                }
                if(i==1 || entries[i-1].group!=entry.group) if(!(i>1 && entries[i-1].tint)) section(entry.group==ControlGroup::Body?"Body":"Outfit");
                const auto& c=options.controls[entry.control];
                const auto held_value=values.contains(c.id)?values.at(c.id):c.value;
                chips[i]=swatch_of(c);
                if(c.kind==ControlKind::Color || c.kind==ControlKind::Intensity || c.kind==ControlKind::Scalar || c.kind==ControlKind::Glow || c.kind==ControlKind::Opacity) row_look.chip=&chips[i];
                Json accept={{"action","reset_control"},{"control",c.id}};
                if(c.kind==ControlKind::Toggle) {
                    const bool on=held_value[0]>=.5f;
                    accept={{"action","control"},{"control",c.id},{"channel",0},{"value",on?0:1}};
                    row_look.badge=on;
                }
                const bool has_body_presets=is_body_physics_control(c);
                const bool has_hair_presets=is_hair_physics_control(c);
                const bool has_presets=has_body_presets || has_hair_presets;
                const int fieldcount=has_presets?1:control_channel_count(c);
                const int channel=channel_%fieldcount;
                Json minus, plus;
                auto preset_steps=[&](const auto& presets,const std::string& current) {
                    const int total=int(std::size(presets)); int index=-1;
                    for(int p=0;p<total;++p) if(presets[p].id==current) index=p;
                    minus=Json{{"action","physics_preset"},{"preset",presets[index<=0?total-1:index-1].id},{"control",c.id}};
                    plus=Json{{"action","physics_preset"},{"preset",presets[index<0 || index>=total-1?0:index+1].id},{"control",c.id}};
                };
                if(has_body_presets) preset_steps(kBodyPhysicsPresets,detect_body_physics_preset(c,held_value));
                else if(has_hair_presets) preset_steps(kHairPhysicsPresets,detect_hair_physics_preset(c,held_value));
                else { minus={{"action","control"},{"control",c.id},{"channel",channel},{"delta",-1}}; plus=minus; plus["delta"]=1; }
                if(!c.scalar && !exact_color_) {
                    // Left and Right walk the strip instead of nudging one channel.
                    const auto strip=color_swatches(options,c);
                    const auto here=nearest_swatch(strip,custom.values.contains(c.id)?custom.values.at(c.id):values.contains(c.id)?values.at(c.id):c.value,!custom.values.contains(c.id));
                    auto pick=[&](size_t index) {
                        if(strip[index].reset) return Json{{"action","reset_control"},{"control",c.id}};
                        const auto& v=strip[index].color;
                        return Json{{"action","control"},{"control",c.id},{"rgb",{v[0],v[1],v[2]}}};
                    };
                    minus=pick((here+strip.size()-1)%strip.size());
                    plus=pick((here+1)%strip.size());
                }
                // Secondary and tertiary do what the details window's prompts say for this kind.
                const bool physics_kind=c.kind==ControlKind::Spring || c.kind==ControlKind::Dynamics || c.kind==ControlKind::Rig;
                const bool colour=!c.scalar && c.kind!=ControlKind::Toggle && c.kind!=ControlKind::Choice && !physics_kind
                                  && c.kind!=ControlKind::Glow && c.kind!=ControlKind::Opacity;
                Json secondary=Json{{"action","ui_channel"},{"count",fieldcount}}, tertiary=confirm_reset_all;
                if(has_presets) { accept={{"action","ui_physics_modal"},{"control",c.id}}; secondary={{"action","reset_control"},{"control",c.id}}; }
                else if(c.kind==ControlKind::Toggle) secondary=Json{};
                else if(c.kind==ControlKind::Choice) secondary=c.options.size()>4?Json{{"action","ui_browse_choice"},{"control",c.id}}:Json{};
                else if(colour) {
                    const Json exact{{"action","ui_exact"}};
                    if(exact_color_) tertiary=exact; else secondary=exact;
                }
                row(int(i),c.name,accept,minus,plus,secondary,tertiary);
            }
            const auto& entry=entries[row_];
            if(row_==0) {
                detail("Templates & Presets",worn->name,"Choose an author combination, material palette, body archetype or physics preset. Left / Right cycles them.");
                divider("Templates");
                std::vector<Choice> choices;
                for(size_t i=0;i<tmpl_items.size();++i) choices.push_back({std::to_string(i),tmpl_items[i].name,tmpl_items[i].action});
                choice_rows(choices,std::to_string(active_tmpl));
                direction_hint(true,"Cycle template");
                action_button("accept","Restore original",palette_action(0),3);
                action_button("secondary","Browse templates...",browse_templates,4);
            } else if(entry.tint) {
                const auto tint=tint_of(entry.group);
                detail(entry.group==ControlGroup::Body?"Body tint":"Outfit tint",worn->name,
                       "Shift every part in this group together. Metal, gems and skin keep their own hue and take only the brightness and saturation, so a recolour cannot turn gold green.");
                const char* fields[]={"Hue","Saturation","Brightness"};
                const char* keys[]={"hue","saturation","brightness"};
                const float lows[]={-180,0,0}, highs[]={180,2,2}, steps[]={5,.05f,.05f};
                const float current[]={tint.hue,tint.saturation,tint.brightness};
                for(int field=0;field<3;++field) {
                    Json base={{"action","tint"},{"group",control_group_name(entry.group)},{"field",keys[field]}};
                    slider(fields[field],current[field],lows[field],highs[field],steps[field],
                           field?std::to_string(int(std::lround(current[field]*100)))+"%":std::to_string(int(current[field])),
                           field==tint_field_index_,base,field!=0,field?"%":"",step_action(base,-1),step_action(base,1));
                }
                direction_hint(true,"Adjust selected slider");
                action_button("secondary","Select next slider",rows_[row_].secondary,4);
                action_button("accept","Reset tint",rows_[row_].accept,3);
            } else {
                const auto& control=options.controls[entry.control]; auto value=values.contains(control.id)?values.at(control.id):control.value;
                auto set_to=[&](double v) { return Json{{"action","control"},{"control",control.id},{"channel",0},{"value",v}}; };
                auto channel_base=[&](int channel) { return Json{{"action","control"},{"control",control.id},{"channel",channel}}; };
                const bool has_body_presets=is_body_physics_control(control), has_hair_presets=is_hair_physics_control(control);
                const bool physics=control.kind==ControlKind::Spring || control.kind==ControlKind::Dynamics || control.kind==ControlKind::Rig;
                const bool rig=control.kind==ControlKind::Rig;
                // Physics sliders: the preset panel's "Customize sliders", or a physics part
                // without presets. Bounce / Settle / Travel (springs) or Stiffness /
                // Damping / Gravity (hair), and a Motion switch on a rig.
                auto physics_sliders=[&](bool modal) {
                    const bool spring=control.kind==ControlKind::Spring;
                    const bool body=body_rig_control(control) || spring || has_body_presets;
                    const int fields=spring?(control.spring_clamp || modal?3:2):3;
                    const int stops=fields+(rig?1:0)+(modal?1:0);
                    const int focus=modal?physics_modal_channel_%stops:channel_%(fields+(rig?1:0));
                    const char* names[]={body?"Bounce":"Stiffness",body?"Settle":"Damping",spring?"Travel":body?"Amount":"Gravity"};
                    for(int field=0;field<fields;++field) {
                        const auto range=control_channel(control,field);
                        std::string readout=spring&&field==1?std::to_string(int(std::lround(value[1]*100)))+"%"
                                           :slider_text(value[field],true)+(field==0&&body?" Hz":spring&&field==2?" cm":"");
                        auto base=channel_base(field); if(modal) base["ui_channel"]=field;
                        slider(names[field],value[field],range.minimum,range.maximum,range.step,readout,focus==field,base,true,
                               spring&&field==1?"%":field==0&&body?" Hz":spring&&field==2?" cm":"",step_action(base,-1),step_action(base,1));
                    }
                    if(rig) {
                        const Json flip={{"action","control"},{"control",control.id},{"channel",3},{"value",value[3]==1?0:1}};
                        option("Motion",value[3]==1?"On":"Off",focus==fields,flip,flip);
                    }
                    if(modal && panel_open) {
                        auto& reset=native_take(panel_,NativeKind::row);
                        fill_row(reset,"Reset part defaults",RowLook{},focus==stops-1);
                        if(focus==stops-1) panel_focus=reset.widget.Get();
                        bind(reset.hit,{{"action","reset_control"},{"control",control.id}});
                    }
                };
                if(control.kind==ControlKind::Toggle) {
                    const bool on=value[0]>=.5f;
                    detail(control.name,worn->name,"Show or hide this part of the outfit. Your saved looks keep it.");
                    option("Visibility",on?"Shown":"Hidden",true,set_to(on?0:1),set_to(on?0:1));
                    direction_hint(true,on?"Hide this part":"Show this part");
                    action_button("accept",on?"Hide":"Show",set_to(on?0:1),3);
                    action_button("tertiary","Reset all",confirm_reset_all,2);
                } else if(control.kind==ControlKind::Choice) {
                    const int here=std::clamp(int(std::lround(value[0])),0,int(control.options.size())-1);
                    detail(control.name,worn->name,"Choose which of the author's textures this part wears.");
                    std::vector<Choice> choices;
                    for(size_t i=0;i<control.options.size();++i) choices.push_back({std::to_string(i),control.options[i].name,set_to(double(i))});
                    choice_rows(choices,std::to_string(here));
                    direction_hint(true,"Choose");
                    if(control.options.size()>4) action_button("secondary","Search choices...",rows_[row_].secondary,4);
                    action_button("accept","Reset part",rows_[row_].accept,3);
                    action_button("tertiary","Reset all",confirm_reset_all,2);
                } else if(physics && (has_body_presets || has_hair_presets) && physics_modal_control_!=control.id) {
                    // Presets: one selector for the whole set, its description underneath.
                    const bool hair=has_hair_presets && !has_body_presets;
                    const std::string pid=hair?detect_hair_physics_preset(control,value):detect_body_physics_preset(control,value);
                    std::string name="Custom sliders", about="Your own values. Customize sliders tunes them, a preset replaces them.";
                    if(hair) { for(const auto& p:kHairPhysicsPresets) if(pid==p.id) { name=p.name; about=p.subtitle; } }
                    else { for(const auto& p:kBodyPhysicsPresets) if(pid==p.id) { name=p.name; about=p.subtitle; } }
                    detail(control.name,worn->name,hair
                        ?"Choose a hair motion preset. Customize sliders tunes stiffness, damping and gravity yourself."
                        :"Choose a motion preset. Customize sliders tunes bounce, settling and travel yourself.");
                    option("Preset",name,true,rows_[row_].previous,rows_[row_].next);
                    note(about);
                    if(rig) {
                        const Json flip={{"action","control"},{"control",control.id},{"channel",3},{"value",value[3]==1?0:1}};
                        option("Motion",value[3]==1?"On":"Off",false,flip,flip);
                    }
                    action_button("accept","Customize sliders",{{"action","ui_physics_modal"},{"control",control.id}},3);
                    action_button("secondary","Reset part",{{"action","reset_control"},{"control",control.id}},4);
                    action_button("tertiary","Reset all",confirm_reset_all,2);
                    direction_hint(true,"Cycle preset");
                } else if(physics) {
                    const bool modal=physics_modal_control_==control.id;
                    const bool spring=control.kind==ControlKind::Spring;
                    detail(control.name,worn->name,spring
                        ?"Bounce is how quickly this part moves, Settle how quickly it stops, Travel how far it swings. If it keeps moving after you stop, turn Settle up."
                        :body_rig_control(control) || has_body_presets
                        ?"Bounce sets the speed, Settle how quickly it calms, Amount how strongly it answers your movement."
                        :"Stiffness pulls this part back toward its rest direction, Damping calms it, Gravity pulls it down; negative values pull up.");
                    physics_sliders(modal);
                    direction_hint(true,"Adjust selected slider");
                    if(modal) action_button("close","Done",{{"action","ui_physics_modal_close"}},5);
                    else {
                        action_button("secondary","Select next setting",Json{{"action","ui_channel"},{"count",control_channel_count(control)}},4);
                        action_button("accept","Reset part",rows_[row_].accept,3);
                        action_button("tertiary","Reset all",confirm_reset_all,2);
                    }
                } else if(control.kind==ControlKind::Glow) {
                    detail(control.name,worn->name,"Set how brightly this part glows. Turn intensity up for a stronger glow; the pulse makes it breathe during combat.");
                    const int fieldcount=control.pulse_hz>0?2:1;
                    const int selected=channel_%fieldcount;
                    const char* fields[]={"Intensity","Pulse rate"};
                    const float lows[]={control.minimum,0.f}, highs[]={control.maximum,5.f}, sizes[]={control.step,0.1f};
                    for(int field=0;field<fieldcount;++field) {
                        auto base=channel_base(field);
                        slider(fields[field],value[field],lows[field],highs[field],sizes[field],
                               field==0?slider_text(value[0],true)+" cd/m²":slider_text(value[1],true)+" Hz",field==selected,base,true,
                               field==0?" cd/m²":" Hz",step_action(base,-1),step_action(base,1));
                    }
                    direction_hint(true,"Adjust glow intensity");
                    if(fieldcount>1) action_button("secondary","Select next slider",Json{{"action","ui_channel"},{"count",fieldcount}},4);
                    action_button("accept","Reset part",rows_[row_].accept,3);
                    action_button("tertiary","Reset all",confirm_reset_all,2);
                } else if(control.kind==ControlKind::Opacity) {
                    detail(control.name,worn->name,"Set how see-through this part is. 0% is fully transparent; 100% is fully solid.");
                    auto base=channel_base(0);
                    slider("Opacity",value[0],control.minimum,control.maximum,control.step,std::to_string(int(std::lround(value[0]*100)))+"%",
                           true,base,false,"%",step_action(base,-1),step_action(base,1));
                    direction_hint(true,"Adjust opacity");
                    action_button("accept","Reset part",rows_[row_].accept,3);
                    action_button("tertiary","Reset all",confirm_reset_all,2);
                } else if(!control.scalar && !exact_color_) {
                    const auto strip=color_swatches(options,control);
                    const auto here=nearest_swatch(strip,custom.values.contains(control.id)?custom.values.at(control.id):value,!custom.values.contains(control.id));
                    detail(control.name,worn->name,!control.swatches.empty()
                        ?"Choose a shade. Default restores this part's current palette or original texture."
                        :control.hue_locked
                        ?"Choose a shade. This part keeps its own hue on purpose: it reads as a material rather than a colour, and rotating it is what makes a recolour look wrong."
                        :"Choose a colour. The first is the author's, then this part in each palette, then hues and shades of it.");
                    // Colour chips on the Inventory's own tile art, the chosen one framed with
                    // its selection frame.
                    if(panel_open) {
                    auto& grid=native_take(panel_,NativeKind::swatches);
                    std::string signature=std::to_string(here);
                    for(const auto& swatch:strip) signature+="|"+std::to_string(swatch.color[0])+","+std::to_string(swatch.color[1])+","+std::to_string(swatch.color[2])+(swatch.reset?"r":"");
                    auto* grid_widget=grid.widget.Get();
                    auto* tree=inventory_object(page,L"WidgetTree");
                    constexpr size_t parts=5, columns=6;
                    if(grid.cells.size()!=strip.size()*parts) {
                        invoke(grid_widget,L"ClearChildren"); grid.cells.clear(); grid.value.clear();
                        auto* serif=load("/Game/Sparta/UI/Fonts/CrimsonText-Regular_Font.CrimsonText-Regular_Font");
                        for(size_t i=0;i<strip.size();++i) {
                            auto* cell=construct(L"/Script/UMG.Overlay",tree);
                            Call add(grid_widget,L"AddChildToUniformGrid",4); add.set(L"content",cell);
                            add.set(L"InRow",int32_t(i/columns)); add.set(L"InColumn",int32_t(i%columns)); add.run();
                            auto* size=construct(L"/Script/UMG.SizeBox",tree);
                            invoke(size,L"SetWidthOverride",L"InWidthOverride",120.f); invoke(size,L"SetHeightOverride",L"InHeightOverride",120.f);
                            native_add(cell,size);
                            native_fill(native_add(cell,native_image(tree,native_texture("T_UI_Inv_Item_BG"))));
                            auto* color=native_image(tree,white);
                            native_padding(native_fill(native_add(cell,color)),Margin{12,12,12,12});
                            auto* chosen=native_image(tree,native_texture("T_UI_Inv_Item_Select"));
                            native_fill(native_add(cell,chosen));
                            auto* label=native_text_block(tree,22,serif,Color{.02f,.018f,.015f,1},false);
                            auto* label_slot=native_add(cell,label);
                            invoke(label_slot,L"SetHorizontalAlignment",L"InHorizontalAlignment",uint8_t{2});
                            invoke(label_slot,L"SetVerticalAlignment",L"InVerticalAlignment",uint8_t{2});
                            auto* button=native_flat_button(tree);
                            native_fill(native_add(cell,button));
                            grid.cells.insert(grid.cells.end(),{WeakObject(color),WeakObject(chosen),WeakObject(label),WeakObject(button),WeakObject(cell)});
                        }
                    }
                    if(grid.value!=signature) {
                        for(size_t i=0;i<strip.size();++i) {
                            const auto& swatch=strip[i]; const auto& c=swatch.color;
                            invoke(grid.cells[i*parts].Get(),L"SetColorAndOpacity",L"InColorAndOpacity",Color{srgb_linear(c[0]),srgb_linear(c[1]),srgb_linear(c[2]),1});
                            native_visibility(grid.cells[i*parts+1].Get(),i==here?shown_passive:uint8_t{2});
                            auto* label=grid.cells[i*parts+2].Get();
                            text_value(label,swatch.reset?"Default":"");
                            // Dark text on a light swatch, light text on a dark one.
                            const bool light=.2126f*c[0]+.7152f*c[1]+.0722f*c[2]>.5f;
                            invoke(label,L"SetColorAndOpacity",L"InColorAndOpacity",SlateColor{light?Color{.02f,.018f,.015f,1}:Color{.86f,.82f,.74f,1}});
                        }
                        grid.value=signature;
                    }
                    if(here<strip.size()) panel_focus=grid.cells[here*parts+4].Get();
                    for(size_t i=0;i<strip.size();++i) {
                        const auto& c=strip[i].color;
                        bind(grid.cells[i*parts+3],strip[i].reset?Json{{"action","reset_control"},{"control",control.id}}
                             :Json{{"action","control"},{"control",control.id},{"rgb",{c[0],c[1],c[2]}}});
                    }
                    }
                    if(!control.swatches.empty()) note(strip[here].name);
                    direction_hint(true,"Choose a colour");
                    action_button("secondary","Exact colour",rows_[row_].secondary,4);
                    action_button("accept","Reset part",rows_[row_].accept,3);
                    action_button("tertiary","Reset all",confirm_reset_all,2);
                } else {
                    const bool shape=control.kind==ControlKind::Shape;
                    detail(control.name,worn->name,
                           shape?"Adjust this part of your character. Reset part restores the outfit's original shape. Save a profile to keep your changes."
                           :control.scalar?"Adjust the intensity for this part."
                           :"Adjust Red, Green and Blue. Select a channel, then adjust it with Left / Right or its slider.");
                    const char* channels[]={"Red","Green","Blue"};
                    for(int channel=0;channel<(control.scalar?1:3);++channel) {
                        auto base=channel_base(channel);
                        slider(shape?"Amount":control.scalar?"Intensity":channels[channel],value[channel],control.minimum,control.maximum,control.step,
                               slider_text(value[channel],control.scalar),control.scalar || channel==channel_%3,base,control.scalar,"",
                               step_action(base,-1),step_action(base,1));
                    }
                    direction_hint(true,shape?"Adjust shape":control.scalar?"Adjust intensity":"Adjust selected channel");
                    if(!control.scalar) action_button("secondary","Select next channel",rows_[row_].secondary,4);
                    action_button("accept","Reset part",rows_[row_].accept,3);
                    action_button("tertiary",control.scalar?"Reset all":"Back to swatches",rows_[row_].tertiary,2);
                }
            }
        }
    } else if(section_==2) {
        constexpr AnimationSlot slots[]={AnimationSlot::Idle,AnimationSlot::Walk,AnimationSlot::Jog,AnimationSlot::Sprint,AnimationSlot::Beacon};
        constexpr const char* titles[]={"Idle animation","Walk animation","Jog animation","Sprint animation","Beacon teleport animation"};
        const bool has_variant=worn && catalog.find(worn->id,selection->second.variant);
        const std::string outfit=has_variant?worn->id:"",variant=has_variant?selection->second.variant:"";
        row_=std::clamp(row_,0,4);
        std::vector<AnimationMenu> menus;
        for(const auto slot:slots) menus.push_back(animation_menu(catalog.animation_options(outfit,variant,slot),slot,
            has_variant?state.animation_choices.find(outfit,variant,slot):nullptr,state.walk_animation=="feminine"));
        auto choose=[&](AnimationSlot slot,const std::string& id)->Json {
            if(has_variant) return {{"action","animation_choice"},{"outfit",outfit},{"variant",variant},{"slot",animation_slot_name(slot)},{"value",id}};
            if(slot==AnimationSlot::Walk) return {{"action","walk_animation"},{"value",id==feminine_animation_id?"feminine":"normal"}};
            return {};
        };
        section("Movement");
        for(int i=0;i<5;++i) {
            const auto& menu=menus[i];
            const auto next=choose(slots[i],menu.step(1)),previous=choose(slots[i],menu.step(-1));
            if(i==4) section("Travel");
            row(i,titles[i],next,previous,next);
        }
        const auto slot=slots[row_];const auto& menu=menus[row_];
        const auto& current=menu.items[menu.selected];
        const auto& options=catalog.animation_options(outfit,variant,slot);
        std::string body;
        if(!current.available) body="This saved animation is unavailable. Default is used until the option returns or you choose another.";
        else if(current.id==feminine_animation_id)
            body=slot==AnimationSlot::Idle?"The Cultist Spear Lady's standing pose. Your walking choice stays separate."
                                         :"The Cultist Spear Lady's walk at its authored pace. Your standing pose stays separate.";
        else if(slot==AnimationSlot::Idle) body="Choose a standing pose. Options can follow your weapon or hide it for a relaxed pose. Combat keeps its own animations.";
        else if(slot==AnimationSlot::Beacon) body="Choose the kneeling and rising animations for beacon travel. Travel timing and controls stay the same.";
        else body="Choose this outfit's movement animation. Default keeps the game's animation or an installed movement mod.";
        if(has_variant) body+=" Saved per outfit variant in profiles.";
        else body="Wear an outfit to use its custom animations. The CSS feminine walk is also available here.";
        detail(titles[row_],has_variant?worn->name:"Movement",body);
        std::vector<Choice> choices;
        for(const auto& item:menu.items) choices.push_back({item.id,item.name,item.available?choose(slot,item.id):Json{}});
        choice_rows(choices,current.id);
        direction_hint(true,"Choose an animation");
        if(has_variant && options.empty()) note("No custom animation supplied for this option.");
        if(slot==AnimationSlot::Walk && appearance.walk.walk_mod_active()) {
            const auto name=appearance.walk.walk_mod_name();
            note(current.id=="original"?"Installed walk mod: "+name:"This choice takes priority over "+name+".");
        }
    } else if(section_==3) {
        // MISC visibility: one row per item category, each cycling a visibility mode. Nothing
        // here touches the body mesh; the runtime hides only the item actors (misc_visibility.inl).
        struct MiscDef { const char* key,*title,*detail; };
        static const MiscDef defs[]={
            {"seal","Seals","Your seal, worn on the waist and forearm. Hide it for a cleaner look, or only while you are actually using it."},
            {"sidearm","Sidearms","Nail Shotgun, Ballistazooka, Crossbow, Machine Gun and the like, carried on the back until drawn."},
            {"stowed_weapons","Weapons","Axatana, blades, hammers and other primary weapons, sheathed on the body until drawn."},
            {"accessories","Accessories & Shell Tools","Eredrim's Diapason, flower crowns, capes, pouches and relics. Usable shell tools show when their ability fires."},
        };
        auto mode_label=[](const std::string& m)->std::string {
            if(m=="hidden") return "Always Hidden";
            if(m=="in_use") return "Only When In Use";
            return "Default (game)";
        };
        auto rule_for=[&](const std::string& key)->MiscRule { auto it=state.misc_rules.find(key); return it!=state.misc_rules.end()?it->second:MiscRule{}; };
        row_=std::clamp(row_,0,4);
        section("Visibility");
        for(int i=0;i<4;++i) {
            const MiscRule rule=rule_for(defs[i].key);
            Json next{{"action","misc_mode"},{"category",defs[i].key},{"delta",1}};
            Json prev{{"action","misc_mode"},{"category",defs[i].key},{"delta",-1}};
            row_look.badge=rule.mode!="default";
            row(i,defs[i].title,next,prev,next);
        }
        section("Position");
        const Json kda{{"action","keep_default_attachments"},{"value",!state.keep_default_attachments}};
        const char* kda_label=state.keep_default_attachments?"Default (game)":"Auto (avoid clipping)";
        row(4,"Sidearm position",kda,kda,kda);
        if(row_<4) {
            const MiscRule current=rule_for(defs[row_].key);
            detail(defs[row_].title,mode_label(current.mode),defs[row_].detail);
            choice_rows({
                {"default","Default (game)",{{"action","misc_mode"},{"category",defs[row_].key},{"mode","default"}}},
                {"in_use","Only When In Use",{{"action","misc_mode"},{"category",defs[row_].key},{"mode","in_use"}}},
                {"hidden","Always Hidden",{{"action","misc_mode"},{"category",defs[row_].key},{"mode","hidden"}}},
            },current.mode);
            direction_hint(true,"Choose visibility");
        } else {
            detail("Sidearm position",kda_label,
                   "Auto moves your holstered sidearm and gear out from a larger custom shell so they "
                   "do not clip through it. Default leaves them where the game puts them.");
            choice_rows({
                {"auto","Auto (avoid clipping)",{{"action","keep_default_attachments"},{"value",false}}},
                {"default","Default (game)",{{"action","keep_default_attachments"},{"value",true}}},
            },state.keep_default_attachments?"default":"auto");
            direction_hint(true,"Choose position");
        }
    } else {
        std::vector<std::string> names; for(const auto& [name,_]:state.presets) names.push_back(name);
        row_=std::clamp(row_,0,int(names.size()));
        section("New");
        row(0,"New profile",{{"action","ui_save_profile"}});
        if(!names.empty()) section("Saved profiles");
        for(size_t i=0;i<names.size();++i) {
            Json rep_act{{"action","ui_confirm"},{"title","Overwrite profile"},{"message","Overwrite profile '"+names[i]+"' with your current character snapshot?"},{"target",Json{{"action","save_look"},{"name",names[i]}}}};
            Json del_act{{"action","ui_confirm"},{"title","Delete profile"},{"message","Delete saved profile '"+names[i]+"'? This cannot be undone."},{"target",Json{{"action","delete_look"},{"name",names[i]}}}};
            row(int(i)+1,names[i],{{"action","load_look"},{"name",names[i]}},{},{},rep_act,del_act);
        }
        auto selected=row_?names[row_-1]:std::string{};
        detail(row_?selected:"New profile","Character profile and settings",row_?"Load this profile, replace it with your current character, or give it a new name.":"Choose a name, then save. A controller can save with the suggested name.");
        std::string suggested=selected;
        if(suggested.empty()) { int n=1; do { suggested="profile."+std::to_string(n++); } while(state.presets.contains(suggested)); }
        divider("Name");
        if(panel_open) {
            auto& input=native_take(panel_head_,NativeKind::input);
            name_input_=input.extra;
            // Only rewrite the field when what it should suggest changed, never under typing.
            if(input.value!=suggested && !has_focus(input.extra.Get())) { text_value(input.extra.Get(),suggested); input.value=suggested; }
        }
        note("Letters, numbers, periods, underscores or hyphens");
        if(!row_) action_button("accept","Save profile",{{"action","ui_save_profile"}},3);
        else {
            action_button("accept","Load profile",rows_[row_].accept,3);
            action_button("secondary","Replace with current character",{{"action","ui_confirm"},{"title","Overwrite profile"},{"message","Overwrite profile '"+selected+"' with your current character snapshot?"},{"target",rows_[row_].secondary}},4);
            action_button("","Rename",{{"action","ui_rename_profile"},{"name",selected}},0);
            action_button("tertiary","Delete profile",{{"action","ui_confirm"},{"title","Delete profile"},{"message","Delete saved profile '"+selected+"'? This cannot be undone."},{"target",rows_[row_].tertiary}},2);
        }
    }
    // What Left/Right does here closes the details window's prompt list, where the
    // Inventory lists what the selected item can do.
    if(panel_open && !hint_horizontal.empty()) {
        if(gamepad_) bar_prompt(actions_,hint_horizontal,Json{},"",11,255);
        else bar_prompt(actions_,hint_horizontal,Json{},"left",15,255,"right",16,255);
    }
    native_finish(list_); native_finish(panel_head_); native_finish(panel_); native_finish(actions_);
    // The details window's big icon: the outfit thumbnail on the black shell backing.
    if(native_picker_) detail_texture=nullptr;   // the picker's window describes the search, not an outfit
    if(detail_icon_!=detail_texture) {
        auto* d=details_.Get();
        auto* icon=native_part(d,L"MyIcon"); auto* backing=native_part(d,L"WidgetSwitcher_IconBG");
        if(detail_texture) {
            native_brush(native_part(icon,L"Image_LazyIcon"),detail_texture);
            invoke(backing,L"SetActiveWidgetIndex",L"Index",int32_t{1});
        }
        native_visibility(icon,detail_texture?shown_self_passive:collapsed);
        native_visibility(backing,detail_texture?shown_self_passive:collapsed);
        detail_icon_=detail_texture;
    }
    auto reveal=[](UObject* scroll,UObject* target,bool animate,uint8_t destination) {
        if(!scroll || !target) return;
        Call call(scroll,L"ScrollWidgetIntoView",4);
        call.set(L"WidgetToFind",target); call.set(L"AnimateScroll",animate);
        // About one row, so the selection never sits under a scroll box's edge fade.
        call.set(L"ScrollDestination",destination); call.set(L"Padding",120.f); call.run();
    };
    if(revealed_row_!=row_ || shown_section_!=section_) {
        if(row_>=0 && row_<int(rows_.size())) {
            // Going up onto the first row of a group shows its header too; going down, the
            // header is already above.
            const bool upward=shown_section_!=section_ || row_<revealed_row_;
            auto* target=upward && rows_[row_].heading.Get()?rows_[row_].heading.Get():rows_[row_].marker.Get();
            if(shown_section_==section_) { pending_reveals_[0]={}; reveal(list_scroll_.Get(),target,true,0); }
            else pending_reveals_[0]={list_scroll_,WeakObject(target),0,2};
        }
        revealed_row_=row_;
    }
    // A new subject in the details window opens at its top, or centred on its current choice
    // when that sits further down (Center clamps at the top, so short lists stay put). Moving
    // between choices after that only scrolls as far as needed.
    {
        std::string context=std::to_string(section_)+"/"+std::to_string(row_)+"/"+physics_modal_control_+(native_picker_?"/search":"");
        const bool fresh=context!=panel_context_;
        if(auto* scroll=panel_scroll_.Get(); scroll && (fresh || panel_focus!=panel_revealed_)) {
            pending_reveals_[1]={};   // a newer subject or choice replaces one still waiting
            if(fresh) invoke(scroll,L"ScrollToStart");
            if(panel_focus && fresh) pending_reveals_[1]={panel_scroll_,WeakObject(panel_focus),2,2};
            else if(panel_focus) reveal(scroll,panel_focus,true,0);
        }
        panel_context_=std::move(context); panel_revealed_=panel_focus;
    }
    panel_fit_pending_=true;
    shown_section_=section_;
    // ---- bottom bars
    bar_prompt(footer_,"Close",{{"action","ui_close"}},"close",5,255);
    const char* browse=section_==0?"Browse shells":section_==1?"Browse parts":section_==2?"Browse animations":section_==3?"Browse categories":"Browse profiles";
    if(gamepad_) bar_prompt(footer_,hint_vertical.empty()?browse:hint_vertical,Json{},"",10,255);
    else bar_prompt(footer_,browse,Json{},"up",13,255,"down",14,255);
    native_finish(footer_);
    bar_prompt(camera_bar_,light_edit_?"Reset light":"Reset view",{{"action",light_edit_?"ui_reset_light":"ui_reset_view"}},"",29,keyboard_icon("Home"));
    if(light_edit_ || light_available()) bar_prompt(camera_bar_,light_edit_?"View controls":"Lighting",{{"action","ui_toggle_light"}},"toggle_light",2,255);
    if(light_edit_) bar_prompt(camera_bar_,"Move light",Json{},"",30,keyboard_icon("LeftMouseButton"));
    else if(gamepad_) {
        bar_prompt(camera_bar_,"Rotate / zoom",Json{},"",30,255);
        bar_prompt(camera_bar_,"Move framing",Json{},"",21,255);
    } else {
        bar_prompt(camera_bar_,"Rotate",Json{},"",30,keyboard_icon("RightMouseButton"));
        bar_prompt(camera_bar_,"Zoom",Json{},"",30,keyboard_icon("MouseScrollUp"));
        bar_prompt(camera_bar_,"Move",Json{},"",21,keyboard_icon("LeftMouseButton"));
    }
    native_finish(camera_bar_);
    if(!confirm_action_.is_null()) native_dialog(); else native_dialog_close();
    page_widgets_=0; nested_widgets_=0;
    for(auto* stack:{&tab_items_,&list_,&panel_head_,&panel_,&actions_,&footer_,&camera_bar_}) { page_widgets_+=int(stack->used); for(const auto& cell:stack->cells) nested_widgets_+=int(cell.kinds.size()); }
    if(enter_transition_) { transition_started_=GetTickCount64(); enter_transition_=false; }
    last_message_.clear(); dirty_=false;
#ifdef CSS_INVENTORY_DEV
    LARGE_INTEGER build_end; QueryPerformanceCounter(&build_end);
    const double ms=double(build_end.QuadPart-build_start.QuadPart)*1000./double(frequency.QuadPart);
    if(ms>1.) {
        if(slow_builds_.size()>=32) slow_builds_.erase(slow_builds_.begin());
        slow_builds_.push_back({{"ms",std::round(ms*100)/100},{"created",created_widgets_},{"section",section_},{"row",row_}});
    }
#endif
}
void InventoryUI::build_native_picker_results() { dirty_=true; }
void InventoryUI::close_menu() {
    if(!main_.Get() || !inventory_bool(main_.Get(),L"bOpen")) return;
    camera_stop();
    auto* handler=inventory_object(controller_.Get(),L"User Interface Handler Component");
    // Inventory uses this path to close. Opening must go through native input.
    Call close(handler,L"HandleGameMenu",2); close.set(L"SubTabIndex",int32_t{0}); close.set(L"AllowClose",true); close.run();
    active_=was_active_=closing_=false; transition_started_=0;
}
void InventoryUI::animate(uint64_t now) {
    if(!transition_started_) return;
    const double t=std::clamp((now-transition_started_)/(closing_?160.:500.),0.,1.);
    const double eased=1.-std::pow(1.-t,3.);
    const double opacity=closing_?1.-eased:eased;
    for(const auto& item:transition_widgets_) if(auto* widget=item.widget.Get()) {
        invoke(widget,L"SetRenderOpacity",L"InOpacity",float(opacity));
        invoke(widget,L"SetRenderTranslation",L"Translation",Vec2{item.offset[0]*(1.-opacity),item.offset[1]*(1.-opacity)});
    }
    if(t>=1.) { transition_started_=0; if(closing_) close_menu(); }
}
Json InventoryUI::dispatch(Json action,const State& state) {
    if(action.is_null()) return {};
    auto name=action.value("action","");
    if(name=="ui_section") { const int next=std::clamp(action.at("section").get<int>(),0,4); if(next==section_) return {}; section_=next; enter_transition_=true; row_=0; scroll_offset_=0; physics_modal_control_.clear(); if(auto* s=scroll_.Get()) invoke(s,L"SetScrollOffset",L"NewScrollOffset",0.f); dirty_=true; return {}; }
    if(name=="ui_press") { auto result=press(action.value("binding",""),state); return result?*result:Json{}; }
    if(name=="ui_row") { row_=std::clamp(action.at("row").get<int>(),0,std::max(0,int(rows_.size())-1)); physics_modal_control_.clear(); dirty_=true; if(section_!=1 && action.value("apply",false) && !rows_.empty()) return dispatch(rows_[row_].accept,state); return {}; }
    // The channel count comes from the page, because a spring has two and a colour three.
    if(name=="ui_channel") { channel_=(channel_+1)%std::clamp(action.value("count",3),1,4); dirty_=true; return {}; }
    if(name=="ui_tint_field") { tint_field_index_=(tint_field_index_+1)%3; dirty_=true; return {}; }
    if(name=="ui_exact") { exact_color_=!exact_color_; dirty_=true; return {}; }
    if(name=="ui_toggle_light") { light_toggle(); return {}; }
    if(name=="ui_reset_light") { light_reset(); return {}; }
    if(name=="ui_reset_view") {
        // Reset the camera without recapturing or resetting an edited light.
        if(!light_edit_) { motion_.reset();drag_pan_=drag_rotate_=false;camera_move({yaw_before_-yaw_,-zoom_,-pan_,-frame_}); }
        return {};
    }
    if(name=="ui_close") {
        physics_modal_control_.clear();
        if(active_ && !closing_) { closing_=true; transition_started_=GetTickCount64(); }
        return {};
    }
    if(name=="ui_physics_modal") {
        physics_modal_control_ = action.value("control", std::string{});
        physics_modal_channel_ = 0;
        dirty_ = true;
        return {};
    }
    if(name=="ui_physics_modal_close") {
        physics_modal_control_.clear();
        dirty_ = true;
        return {};
    }
    if(name=="ui_confirm") {
        confirm_action_={
            {"action",action.at("target")},
            {"title",action.value("title",std::string("Confirm action"))},
            {"message",action.value("message",std::string("Are you sure you want to proceed?"))}
        };
        dirty_=true; return {};
    }
    if(name=="ui_confirm_cancel") { confirm_action_=nullptr; dirty_=true; return {}; }
    if(name=="ui_confirm_proceed") {
        auto target=confirm_action_.value("action",Json{});
        confirm_action_=nullptr; dirty_=true;
        return dispatch(target,state);
    }
    if(name=="ui_browse_shells" && catalog_) {
        Json opts=Json::array();
        for(const auto& o:catalog_->outfits) {
            if(o.id==original_shells_id) continue;
            opts.push_back({{"id",o.id},{"label",o.name+" ("+o.author+")"}});
        }
        native_options_.reset(opts);
        native_picker_=true;
        native_picker_title_="Browse Outfits & Shells";
        native_picker_kind_="outfit";
        native_picker_target_.clear();
        native_search_query_.clear();
        dirty_=true;
        return {};
    }
    if(name=="ui_browse_templates" && catalog_ && appearance_) {
        auto sel=state.selections.find(appearance_->shell);
        const Outfit* worn_outfit=nullptr;
        if(sel!=state.selections.end()) for(const auto& o:catalog_->outfits) if(o.id==sel->second.outfit) worn_outfit=&o;
        if(!worn_outfit) return {};
        const auto& opts_set=worn_outfit->controls_for(sel->second.variant);
        Json opts=Json::array();
        opts.push_back({{"id","palette:original"},{"label","[Original] Author Default Materials"}});
        for(const auto& p:opts_set.palettes) {
            opts.push_back({{"id","palette:"+p.id},{"label","[Palette] "+p.name}});
        }
        for(const auto& t:worn_outfit->templates) {
            std::string tag=t.kind==TemplateKind::Combination?"Combination":
                            t.kind==TemplateKind::Archetype?"Archetype":
                            t.kind==TemplateKind::Physics?"Physics":
                            t.kind==TemplateKind::Hair?"Hair":
                            t.kind==TemplateKind::Jewelry?"Jewelry":
                            t.kind==TemplateKind::Glow?"Glow":"Template";
            opts.push_back({{"id","template:"+t.id},{"label","["+tag+"] "+t.name}});
        }
        native_options_.reset(opts);
        native_picker_=true;
        native_picker_title_="Browse Templates & Presets";
        native_picker_kind_="template";
        native_picker_target_.clear();
        native_search_query_.clear();
        dirty_=true;
        return {};
    }
    if(name=="ui_browse_choice" && catalog_ && appearance_) {
        auto sel=state.selections.find(appearance_->shell);
        const Outfit* worn_outfit=nullptr;
        if(sel!=state.selections.end()) for(const auto& o:catalog_->outfits) if(o.id==sel->second.outfit) worn_outfit=&o;
        if(!worn_outfit) return {};
        const auto& opts_set=worn_outfit->controls_for(sel->second.variant);
        std::string cid=action.at("control").get<std::string>();
        const Control* ctrl=opts_set.find(cid);
        if(!ctrl || ctrl->kind!=ControlKind::Choice) return {};
        Json opts=Json::array();
        for(size_t i=0;i<ctrl->options.size();++i) {
            opts.push_back({{"id",std::to_string(i)},{"label",ctrl->options[i].name}});
        }
        native_options_.reset(opts);
        native_picker_=true;
        native_picker_title_="Choose "+ctrl->name;
        native_picker_kind_="choice";
        native_picker_target_=cid;
        native_search_query_.clear();
        dirty_=true;
        return {};
    }
    if(name=="ui_pick_row") {
        native_options_.selected=std::min(action.at("row").get<size_t>(),native_options_.matches.empty()?size_t{}:native_options_.matches.size()-1);
        build_native_picker_results();
        return {};
    }
    if(name=="ui_pick_cancel") {
        native_picker_=false; dirty_=true; return {};
    }
    if(name=="ui_pick_apply") {
        const auto val=native_options_.value();
        if(val.is_null()) return {};
        native_picker_=false; dirty_=true;
        if(native_picker_kind_=="outfit") {
            const std::string outfit_id=val.get<std::string>();
            std::string variant_id;
            if(catalog_) for(const auto& o:catalog_->outfits) if(o.id==outfit_id && !o.variants.empty()) { variant_id=o.variants.front().id; break; }
            return {{"action","select"},{"outfit",outfit_id},{"variant",variant_id}};
        } else if(native_picker_kind_=="choice") {
            int idx=val.is_number()?val.get<int>():std::stoi(val.get<std::string>());
            return {{"action","control"},{"control",native_picker_target_},{"channel",0},{"value",double(idx)}};
        } else if(native_picker_kind_=="template") {
            std::string tid=val.get<std::string>();
            if(tid.starts_with("palette:")) return {{"action","palette"},{"palette",tid.substr(8)}};
            if(tid.starts_with("template:")) return {{"action","template"},{"template",tid.substr(9)}};
            return {{"action","template"},{"template",tid}};
        }
        return {};
    }
    if(name=="ui_save_template" || name=="ui_save_profile") return {{"action","save_look"},{"name",inventory_text(name_input_.Get())}};
    if(name=="ui_rename_template" || name=="ui_rename_profile") return {{"action","rename_look"},{"name",action.at("name")},{"new_name",inventory_text(name_input_.Get())}};
    return action;
}
// Re-resolve reflected storage on each use. Only the live menu instance is
// changed; no camera template or shared default is written.
static float* inventory_camera_fov(UObject* state) {
    auto* property=state?state->GetPropertyByNameInChain(L"CameraSettings"):nullptr;
    if(!property || !property->IsA<FStructProperty>())
        throw std::runtime_error("Menu camera settings unavailable");
    auto* type=static_cast<FStructProperty*>(property)->GetStruct().Get();
    auto* fov=type->GetPropertyByNameInChain(L"FOV");
    auto* enabled=type->GetPropertyByNameInChain(L"bOverrideFOV");
    auto* method=type->GetPropertyByNameInChain(L"ViewCalculationMethod");
    auto bounded=[&](FProperty* field) {
        return field && field->GetArrayDim()==1 && field->GetOffset_Internal()>=0 &&
            field->GetSize()>0 && field->GetOffset_Internal()+field->GetSize()<=property->GetElementSize();
    };
    if(!bounded(fov) || !fov->IsA<FFloatProperty>() || !bounded(enabled) || !enabled->IsA<FBoolProperty>() ||
       !bounded(method) || method->GetElementSize()!=1 || (!method->IsA<FByteProperty>() && !method->IsA<FEnumProperty>()))
        throw std::runtime_error("Menu camera settings layout mismatch");
    auto* data=reinterpret_cast<std::byte*>(state)+property->GetOffset_Internal();
    if(!static_cast<FBoolProperty*>(enabled)->GetPropertyValueInContainer(data) ||
       *reinterpret_cast<uint8_t*>(data+method->GetOffset_Internal())!=0)
        throw std::runtime_error("Menu camera does not use the expected FOV override");
    return reinterpret_cast<float*>(data+fov->GetOffset_Internal());
}
void InventoryUI::camera_bind_state() {
    auto* manager=inventory_object(controller_.Get(),L"PlayerCameraManager");
    auto* instance=inventory_object(manager,L"ActiveCameraInstance");
    auto* state=inventory_object(instance,L"CameraState");
    if(state==camera_state_.Get()) return;
    camera_restore_state();
    if(!state || !camera_actor_.Get() || state->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)) ||
       state->GetClassPrivate()->GetPathName()!=L"/Game/Sparta/Core/Camera/CameraStates/CameraState_Menu.CameraState_Menu_C") return;
    auto* fov=inventory_camera_fov(state);
    if(!std::isfinite(*fov) || *fov<=0 || *fov>=180) throw std::runtime_error("Invalid menu camera FOV");
    Call target(state,L"GetCameraTargetActor",1); target.run();
    Call lens(camera_component_.Get(),L"GetHorizontalFieldOfView",1); lens.run();
    camera_target_before_=target.get<UObject*>(); camera_fov_before_=*fov;
    camera_state_=state;
    invoke(state,L"SetCameraTargetActor",L"Actor",camera_actor_.Get());
    *fov=lens.get<float>();
}
void InventoryUI::camera_restore_state() {
    if(auto* state=camera_state_.Get()) {
        *inventory_camera_fov(state)=camera_fov_before_;
        invoke(state,L"SetCameraTargetActor",L"Actor",camera_target_before_.Get());
    }
    camera_state_.Reset(); camera_target_before_.Reset();
}
void InventoryUI::backdrop_start() {
    auto* camera=camera_component_.Get();
    if(!camera) return;
    auto vector=[](UObject* object,const wchar_t* function) {
        Call call(object,function,1); call.run(); return call.get<BackdropVector>();
    };
    backdrop_forward_=vector(camera,L"GetForwardVector");
    backdrop_right_=vector(camera,L"GetRightVector");
    backdrop_up_=vector(camera,L"GetUpVector");
    for(auto* name:{L"BG_Front",L"BG_Back"}) {
        auto* component=inventory_object(display_.Get(),name);
        auto* mesh=inventory_object(component,L"StaticMesh");
        // This game's centered 100 cm XY plane is the measured layout contract.
        if(!mesh || mesh->GetPathName()!=L"/Engine/BasicShapes/Plane.Plane") continue;
        BackdropLayer layer;
        layer.component=component;
        layer.relative_location=read<BackdropVector>(component,L"RelativeLocation");
        layer.relative_scale=read<BackdropVector>(component,L"RelativeScale3D");
        layer.world_location=vector(component,L"K2_GetComponentLocation");
        layer.axis_x=vector(component,L"GetForwardVector");
        layer.axis_y=vector(component,L"GetRightVector");
        auto scale=vector(component,L"K2_GetComponentScale");
        layer.half_x=50*std::abs(scale[0]); layer.half_y=50*std::abs(scale[1]);
        backdrop_layers_.push_back(layer);
    }
    backdrop_aspect_=0;
    backdrop_update();
}
void InventoryUI::backdrop_update() {
    auto* camera=camera_component_.Get();
    if(!camera || backdrop_layers_.empty() || layout_size_[0]<=0 || layout_size_[1]<=0) return;
    const double aspect=layout_size_[0]/layout_size_[1];
    Call lens(camera,L"GetHorizontalFieldOfView",1); lens.run();
    for(auto& layer:backdrop_layers_) if(auto* component=layer.component.Get()) {
        const double factor=backdrop_coverage(camera_world_location_,backdrop_forward_,backdrop_right_,
            backdrop_up_,lens.get<float>(),aspect,layer.world_location,layer.axis_x,layer.axis_y,layer.half_x,layer.half_y);
        auto scale=layer.relative_scale;
        scale[0]*=factor; scale[1]*=factor;
        invoke(component,L"SetRelativeScale3D",L"NewScale3D",scale);
        // The planes belong to the camera's parent component, not the moving
        // child camera actor. Follow the same screen-plane translation as CSS.
        auto location=layer.world_location;
        for(int i=0;i<3;++i) location[i]-=backdrop_right_[i]*pan_+backdrop_up_[i]*frame_;
        Call move(component,L"K2_SetWorldLocation",4); move.set(L"NewLocation",location);
        move.set(L"bSweep",false); move.set(L"bTeleport",true); move.run();
    }
    backdrop_aspect_=aspect;
}
void InventoryUI::backdrop_stop() {
    for(auto& layer:backdrop_layers_) if(auto* component=layer.component.Get()) {
        invoke(component,L"SetRelativeScale3D",L"NewScale3D",layer.relative_scale);
        Call move(component,L"K2_SetRelativeLocation",4); move.set(L"NewLocation",layer.relative_location);
        move.set(L"bSweep",false); move.set(L"bTeleport",true); move.run();
    }
    backdrop_layers_.clear(); backdrop_aspect_=0;
}
void InventoryUI::camera_start() {
    auto* handler=inventory_object(controller_.Get(),L"User Interface Handler Component");
    auto* display=inventory_object(handler,L"ActiveDisplayMenu");
    if(!display || !display->GetPropertyByNameInChain(L"TargetYaw")) return;
    display_=display; yaw_before_=read<double>(display,L"TargetYaw"); yaw_=yaw_before_;
    location_before_=read<std::array<double,3>>(display,L"TargetLocation");
    auto* camera=inventory_object(inventory_object(display,L"CameraActor_DisplayMenu"),L"ChildActor");
    auto* component=inventory_object(camera,L"CameraComponent");
    if(component && component->GetPropertyByNameInChain(L"CurrentFocalLength")) {
        camera_actor_=camera;
        Call actor_location(camera,L"K2_GetActorLocation",1); actor_location.run();
        camera_actor_location_before_=actor_location.get<std::array<double,3>>();
        camera_component_=component; lens_before_=read<float>(component,L"CurrentFocalLength");
        camera_rotation_before_=read<std::array<double,3>>(component,L"RelativeRotation");
        camera_location_before_=read<std::array<double,3>>(component,L"RelativeLocation");
        Call rotation(component,L"K2_GetComponentRotation",1); rotation.run(); camera_world_rotation_=rotation.get<std::array<double,3>>();
        Call location(component,L"K2_GetComponentLocation",1); location.run(); camera_world_location_=location.get<std::array<double,3>>();
    }
    // LevelTick skips UpdateCameraManager while paused unless this is enabled.
    // Keep the original through CSS re-entry and other Inventory tabs; restoring
    // it on a tab switch would freeze their camera transitions as well.
    if(camera_component_.Get() && !camera_tick_controller_.Get()) {
        auto* pc=controller_.Get();
        auto* property=pc?pc->GetPropertyByNameInChain(L"bShouldPerformFullTickWhenPaused"):nullptr;
        if(!property || !property->IsA<FBoolProperty>())
            throw std::runtime_error("Inventory paused camera property unavailable");
        auto* flag=static_cast<FBoolProperty*>(property);
        camera_tick_before_=flag->GetPropertyValueInContainer(pc);
        camera_tick_controller_=pc;
        flag->SetPropertyValueInContainer(pc,true);
    }
    zoom_=frame_=pan_=0; motion_.reset(); drag_pan_=drag_rotate_=false; mouse_left_=mouse_right_=false;
    backdrop_start();
}
void InventoryUI::camera_tick_restore() {
    if(auto* pc=camera_tick_controller_.Get()) {
        auto* property=pc->GetPropertyByNameInChain(L"bShouldPerformFullTickWhenPaused");
        if(!property || !property->IsA<FBoolProperty>())
            throw std::runtime_error("Inventory paused camera restoration unavailable");
        static_cast<FBoolProperty*>(property)->SetPropertyValueInContainer(pc,camera_tick_before_);
    }
    camera_tick_controller_.Reset();
}
void InventoryUI::camera_stop() {
    light_stop();
    backdrop_stop();
    camera_restore_state();
    motion_.reset(); drag_pan_=drag_rotate_=false;
    if(auto* display=display_.Get()) { invoke(display,L"UpdateDisplayYaw",L"NewValue",yaw_before_); invoke(display,L"UpdateDisplayVector",L"NewValue",location_before_); }
    if(auto* camera=camera_component_.Get()) {
        invoke(camera,L"SetCurrentFocalLength",L"InFocalLength",lens_before_);
        Call rotate(camera,L"K2_SetRelativeRotation",4); rotate.set(L"NewRotation",camera_rotation_before_); rotate.set(L"bSweep",false); rotate.set(L"bTeleport",true); rotate.run();
        Call location(camera,L"K2_SetRelativeLocation",4); location.set(L"NewLocation",camera_location_before_); location.set(L"bSweep",false); location.set(L"bTeleport",true); location.run();
    }
    if(auto* actor=camera_actor_.Get()) {
        Call location(actor,L"K2_SetActorLocation",5); location.set(L"NewLocation",camera_actor_location_before_);
        location.set(L"bSweep",false); location.set(L"bTeleport",true); location.run();
    }
    display_.Reset(); camera_component_.Reset(); camera_actor_.Reset();
}
void InventoryUI::camera_update(double delta,bool invert_x) {
    auto* display=display_.Get(); auto* handler=inventory_object(controller_.Get(),L"User Interface Handler Component");
    if(!display || !handler) return;
    auto right=read<std::array<double,2>>(handler,L"InputAxis_Thumbstick_Right");
    auto left=read<std::array<double,2>>(handler,L"InputAxis_Thumbstick_Left");
    const auto movement=motion_.step(right,left,delta,invert_x);
    if(light_edit_) light_move(movement[0],movement[1]*80/.65);
    else camera_move(movement);
    auto* pc=controller_.Get();
    const bool left_down=inventory_key(pc,"LeftMouseButton"),right_down=inventory_key(pc,"RightMouseButton");
    Call mouse(pc,L"GetMousePosition",3); mouse.run();
    if(!gamepad_ && mouse.get<bool>()) {
        Call size(find(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetViewportSize",2);
        size.set(L"WorldContextObject",pc); size.run(); auto viewport=size.get<Vec2>();
        if(viewport.x>0 && viewport.y>0 && layout_size_[1]>0) {
            const double width=layout_size_[0]/layout_size_[1]*1080.;
            std::array<double,2> point{mouse.get<float>(L"LocationX")/viewport.x*width,mouse.get<float>(L"LocationY")/viewport.y*1080.};
            const bool center=point[0]>490 && point[0]<width-440 && point[1]>90 && point[1]<970;
            if(!left_down) drag_pan_=false;
            else if(!mouse_left_ && center) drag_pan_=true;
            if(!right_down) drag_rotate_=false;
            else if(!mouse_right_ && center) drag_rotate_=true;
            std::array<double,4> movement{};
            if((drag_pan_ && mouse_left_) || (drag_rotate_ && mouse_right_)) {
                auto dx=point[0]-mouse_before_[0],dy=point[1]-mouse_before_[1];
                if(std::abs(dx)<150 && std::abs(dy)<150) {
                    if(light_edit_) light_move(dx*.45*(invert_x?-1:1),dy*.45);
                    else {
                        if(drag_rotate_) movement[0]=dx*.45*(invert_x?-1:1);
                        if(drag_pan_) { movement[2]=dx*.24/(1+zoom_); movement[3]=-dy*.24/(1+zoom_); }
                    }
                }
            }
            if(center) {
                Call wheel(pc,L"GetInputAnalogKeyState",2); auto* key=wheel.param(L"Key");
                member(wheel.data(key),key->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(L"MouseWheelAxis"));
                wheel.run(); movement[1]=std::clamp(double(wheel.get<float>()),-1.,1.)*.08;
            }
            camera_move(movement); mouse_before_=point;
        }
    } else drag_pan_=drag_rotate_=false;
    mouse_left_=left_down; mouse_right_=right_down;
}
void InventoryUI::camera_move(const std::array<double,4>& movement) {
    if(light_edit_) return;
    auto* display=display_.Get(); if(!display) return;
    auto [x,z,h,v]=movement;
    if(x) {
        // Native UpdateYaw uses scalar FInterpTo, so wrapping at 180 reverses it.
        yaw_+=x; invoke(display,L"UpdateDisplayYaw",L"NewValue",yaw_);
        // CSS smooths velocity. Settle the native interpolation to avoid a second
        // half-second lag on top of that response. Only touch the active CSS page.
        invoke(display,L"UpdateYaw",L"DeltaTime",1.);
    }
    if(auto* camera=camera_component_.Get()) {
        if(z) {
            zoom_=std::clamp(zoom_+z,-.45,1.); invoke(camera,L"SetCurrentFocalLength",L"InFocalLength",float(lens_before_*(1+zoom_)));
            if(auto* state=camera_state_.Get()) {
                Call lens(camera,L"GetHorizontalFieldOfView",1); lens.run();
                *inventory_camera_fov(state)=lens.get<float>();
            }
        }
        if(h || v) {
            pan_=std::clamp(pan_+h,-90.,90.); frame_=std::clamp(frame_+v,-70.,70.);
            // Move the camera opposite the stick in its screen plane. The
            // character follows the stick without leaving the native light rig.
            const double p=camera_world_rotation_[0]*pi/180., y=camera_world_rotation_[1]*pi/180.;
            std::array<double,3> right_axis{-std::sin(y),std::cos(y),0};
            std::array<double,3> up_axis{-std::sin(p)*std::cos(y),-std::sin(p)*std::sin(y),std::cos(p)};
            auto position=camera_actor_location_before_;
            for(int i=0;i<3;++i) position[i]-=right_axis[i]*pan_+up_axis[i]*frame_;
            if(auto* actor=camera_actor_.Get()) {
                Call move(actor,L"K2_SetActorLocation",5); move.set(L"NewLocation",position); move.set(L"bSweep",false); move.set(L"bTeleport",true); move.run();
            }
        }
    }
    const double aspect=layout_size_[1]>0?layout_size_[0]/layout_size_[1]:0;
    if(z || h || v || aspect!=backdrop_aspect_) backdrop_update();
}
Json InventoryUI::poll(void* engine,const Catalog& catalog,const State& state,Appearance& appearance,float,bool focused) {
#ifdef CSS_INVENTORY_DEV
    cinema_update(focused);
#endif
    if(!enabled_) return {};
    catalog_=&catalog; appearance_=&appearance;
    auto now=GetTickCount64();
    if(main_.Get() && now>=discover_after_) {
        discover_after_=now+500;
        auto* player=appearance.player(engine); auto* current=inventory_object(player,L"Controller");
        auto* handler=inventory_object(current,L"User Interface Handler Component");
        auto* current_main=inventory_object(inventory_object(handler,L"WBP_Menu_Game"),L"WBP_Menu_Main");
        if(current!=controller_.Get() || current_main!=main_.Get()) detach();
    }
    if(!main_.Get()) {
        if(now<discover_after_) return {}; discover_after_=now+500;
        auto* player=appearance.player(engine); auto* pc=inventory_object(player,L"Controller");
        auto* handler=inventory_object(pc,L"User Interface Handler Component");
        auto* main=inventory_object(inventory_object(handler,L"WBP_Menu_Game"),L"WBP_Menu_Main");
        if(!main || !inventory_bool(main,L"bOpen")) return {};
        command(engine,{{"action","inventory_attach"}}); command(engine,{{"action","inventory_order"}});
    }
    auto* main=main_.Get(); auto* switcher=switcher_.Get();
    if(!main || !switcher || !page_.Get()) { detach(); return {}; }
    // Menu closed: one cached flag read, no engine call.
    const bool open=inventory_bool(main,L"bOpen");
    if(!open) camera_tick_restore();
    UObject* shown=nullptr;
    if(open) { Call selected(switcher,L"GetActiveWidget",1); selected.run(); shown=selected.get<UObject*>(); }
    active_=open && shown==page_.Get();
    if(active_ && !was_active_) { appearance.player(engine); bind_inputs(); camera_start(); dirty_=enter_transition_=true; closing_=false; for(auto& b:bindings_) { b.down=true; b.repeat=now+400; } }
    if(!active_ && was_active_) { camera_stop(); closing_=false; transition_started_=0; }
    if(active_) camera_bind_state();
    was_active_=active_;
    double elapsed=last_tick_?std::clamp((now-last_tick_)/1000.,0.,.05):0.; last_tick_=now;
#ifdef CSS_INVENTORY_DEV
    if(!active_ || !focused) capture_duration_=0;
#endif
    if(!active_) return {};
    if(now>=layout_check_) {
        layout_check_=now+500;
        Call geometry(switcher,L"GetCachedGeometry",1); geometry.run();
        Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2); size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        auto current=size.get<Vec2>();
        if(std::abs(current.x-layout_size_[0])>.5 || std::abs(current.y-layout_size_[1])>.5) dirty_=true;
    }
    if(auto* prompt=input_prompt_.Get()) {
        const bool gamepad=read<uint8_t>(prompt,L"InputType")==1;
        if(gamepad!=gamepad_) { gamepad_=gamepad; dirty_=true; }
    }
    bool editing_native=false;
    if(native_picker_) if(auto* search=native_search_input_.Get()) {
        try {
            const auto query=inventory_text(search,256);
            if(query!=native_search_query_) {
                native_search_query_=query;
                if(native_options_.filter(query)) build_native_picker_results();
            }
        } catch(...) {}
        editing_native=true;
    }
#ifdef CSS_INVENTORY_DEV
    if(frozen_) dirty_=false;   // dev prototyping: keep hands off the page
#endif
    // The search field is a kept widget now, so a rebuild while typing leaves it alone.
    (void)editing_native;
    native_fit_panel();   // measures the previous build, which has laid out by now
    native_reveal_pending();
    if(dirty_) build(catalog,state,appearance);
    animate(GetTickCount64());
    if(!active_ || closing_) return {};
    // Scripted filming continues without desktop focus; input still requires it.
#ifdef CSS_INVENTORY_DEV
    if(capture_duration_) {
        double t=std::clamp(double(now-capture_start_)/capture_duration_,0.,1.);
        const double smooth=t*t*(3-2*t);
        auto target=capture_from_;
        for(int i=0;i<4;++i) target[i]+=(capture_to_[i]-target[i])*smooth;
        camera_move({target[0]-yaw_,target[1]-zoom_,target[2]-pan_,target[3]-frame_});
        if(t>=1.) capture_duration_=0;
    }
#endif
    if(!focused) { motion_.reset(); drag_pan_=drag_rotate_=false; return {}; }
    bool typing=false;
    if(auto* input=name_input_.Get()) { Call focus(input,L"HasKeyboardFocus",1); focus.run(); typing=focus.get<bool>(); }
    if(native_picker_) if(auto* search=native_search_input_.Get()) { Call focus(search,L"HasKeyboardFocus",1); focus.run(); typing=typing || focus.get<bool>(); }
    bool character_controls=true;
    if(native_picker_ || !confirm_action_.is_null() || !physics_modal_control_.empty()) character_controls=false;
    if(!typing && character_controls) camera_update(elapsed,state.invert_orbit_x);
    else { motion_.reset(); drag_pan_=drag_rotate_=false; }
    if(native_picker_ && !typing && now>=native_wheel_after_) {
        Call wheel(controller_.Get(),L"GetInputAnalogKeyState",2);auto* key=wheel.param(L"Key");
        member(wheel.data(key),key->GetElementSize(),find(L"/Script/InputCore.Key"),L"KeyName",FName(L"MouseWheelAxis"));wheel.run();
        const float scroll=wheel.get<float>();
        if(std::abs(scroll)>.01f) {
            native_wheel_after_=now+100;
            native_options_.move(scroll<0?1:-1);
            build_native_picker_results();
        }
    }
    if(typing) for(auto& binding:bindings_) {
        bool down=false,allowed=false;
        for(const auto& key:binding.keys) if(inventory_key(controller_.Get(),key)) {
            down=true;if((native_picker_ || !physics_modal_control_.empty()) && (key.starts_with("Gamepad_") || key=="Escape")) allowed=true;
        }
        const bool repeat=binding.action=="up" || binding.action=="down";
        const bool trigger=allowed && down && (!binding.down || (repeat && now>=binding.repeat));
        if(down && !binding.down) binding.repeat=now+360;
        else if(trigger) binding.repeat=now+110;
        else if(!allowed) binding.repeat=now+360;
        binding.down=down;
        if(trigger) {
            if(native_picker_) {
                if(binding.action=="up") { native_options_.move(-1); build_native_picker_results(); return {}; }
                if(binding.action=="down") { native_options_.move(1); build_native_picker_results(); return {}; }
                if(binding.action=="accept") return dispatch({{"action","ui_pick_apply"}},state);
                if(binding.action=="close") return dispatch({{"action","ui_pick_cancel"}},state);
            }
            if(!physics_modal_control_.empty()) {
                if(binding.action=="close") return dispatch({{"action","ui_physics_modal_close"}},state);
            }
        }
    }
    if(!typing) for(auto& binding:bindings_) {
        bool down=false; for(const auto& key:binding.keys) if(inventory_key(controller_.Get(),key)) { down=true; break; }
        bool repeat=binding.action=="up" || binding.action=="down" || binding.action=="left" || binding.action=="right";
        bool triggered=down && (!binding.down || (repeat && now>=binding.repeat));
        if(down && !binding.down) binding.repeat=now+360; else if(triggered) binding.repeat=now+110;
        binding.down=down;
        if(!triggered) continue;
        if(auto result=press(binding.action,state)) return *result;
    }
    // Buttons and sliders only change under the mouse, so they are read while a mouse button
    // is down and for one frame after release (the slider's final value, a quick click).
    // The OS button state, not the controller's: it holds in every input mode the menu can
    // be in. GetAsyncKeyState reads physical buttons, so swapped buttons are swapped back.
    const bool swapped=GetSystemMetrics(SM_SWAPBUTTON)!=0;
    const bool left_now=(GetAsyncKeyState(swapped?VK_RBUTTON:VK_LBUTTON)&0x8000)!=0;
    const bool mouse_now=left_now || (GetAsyncKeyState(swapped?VK_LBUTTON:VK_RBUTTON)&0x8000)!=0;
    const bool mouse=mouse_now || mouse_was_down_;
    const bool left_pressed=left_now && !left_was_down_;
    mouse_was_down_=mouse_now; left_was_down_=left_now;
    if(!mouse) { for(auto& hit:hits_) hit.down=false; drag_slider_=-1; return {}; }
    // The game's slider bar only steps with its arrows. A left press that lands on a bar
    // starts a drag, and while the button is held the pointer's place along the bar sets
    // the value directly, snapped to the control's step.
    auto along_bar=[&](const Slider& slider,bool& inside)->double {
        inside=false;
        auto* bar=slider.widget.Get(); if(!bar) return 0;
        Call geometry(bar,L"GetCachedGeometry",1); geometry.run();
        Call pointer(find(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetMousePositionOnPlatform",1); pointer.run();
        Call local(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"AbsoluteToLocal",3);
        local.copy(L"Geometry",geometry,L"ReturnValue"); local.set(L"AbsoluteCoordinate",pointer.get<Vec2>()); local.run();
        Call size(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"GetLocalSize",2);
        size.copy(L"Geometry",geometry,L"ReturnValue"); size.run();
        const auto at=local.get<Vec2>(); const auto extent=size.get<Vec2>();
        if(extent.x<1 || extent.y<1) return 0;
        inside=at.x>=0 && at.x<=extent.x && at.y>=-extent.y*.75 && at.y<=extent.y*1.75;
        return std::clamp(at.x/extent.x,0.,1.);
    };
    if(!left_now) drag_slider_=-1;
    else if(left_pressed) {
        drag_slider_=-1;
        for(size_t i=0;i<sliders_.size();++i) { bool inside=false; along_bar(sliders_[i],inside); if(inside) { drag_slider_=int(i); break; } }
    }
    if(drag_slider_>=0 && drag_slider_<int(sliders_.size())) {
        auto& slider=sliders_[drag_slider_];
        bool inside=false; const double fraction=along_bar(slider,inside);
        double v=slider.minimum+fraction*(slider.maximum-slider.minimum);
        if(slider.step>0) v=slider.minimum+std::round((v-slider.minimum)/slider.step)*slider.step;
        v=std::clamp(v,double(slider.minimum),double(slider.maximum));
        if(std::abs(v-slider.previous)>1e-5) {
            // Hue is degrees, not a 0 to 100 proportion, so it reads as a whole number.
            const bool tint_slider=slider.action.contains("field");
            const bool degrees=tint_slider && slider.action.at("field")=="hue";
            slider.previous=float(v);
            text_value(slider.label.Get(),degrees?std::to_string(int(v)):
                       slider.unit=="%"?std::to_string(int(std::lround(v*100)))+"%":
                       slider_text(float(v),slider.scalar)+slider.unit);
            if(auto* bar=slider.widget.Get()) invoke(bar,L"UpdateProgressBar",L"InPercent",float(fraction));
            // The dragged slider becomes the one Left/Right adjusts.
            if(tint_slider) {
                const auto field=slider.action.at("field").get<std::string>();
                tint_field_index_=field=="hue"?0:field=="saturation"?1:2;
            } else if(slider.action.contains("channel")) {
                const int channel=slider.action.value("ui_channel",slider.action.at("channel").get<int>());
                if(slider.action.contains("ui_channel")) physics_modal_channel_=channel; else channel_=channel;
            }
            auto action=slider.action; action["value"]=v; return action;
        }
        return {};
    }
    for(auto& hit:hits_) if(auto* widget=hit.widget.Get()) {
        Call pressed(widget,L"IsPressed",1); pressed.run(); bool down=pressed.get<bool>();
        bool click=down && !hit.down; hit.down=down;
        if(!click) continue;
        // A two-key prompt (W / S, A / D): the glyph under the pointer picks the direction.
        if(!hit.parts.empty()) {
            Call pointer(find(L"/Script/UMG.Default__WidgetLayoutLibrary"),L"GetMousePositionOnPlatform",1); pointer.run();
            for(const auto& [part,action]:hit.parts) if(auto* glyph=part.Get()) {
                Call geometry(glyph,L"GetCachedGeometry",1); geometry.run();
                Call under(find(L"/Script/UMG.Default__SlateBlueprintLibrary"),L"IsUnderLocation",3);
                under.copy(L"Geometry",geometry,L"ReturnValue"); under.set(L"AbsoluteCoordinate",pointer.get<Vec2>()); under.run();
                if(under.get<bool>()) { invoke(glyph,L"TriggerInputAnim"); return dispatch(action,state); }
            }
        }
        if(auto* glyph=hit.glyph.Get()) invoke(glyph,L"TriggerInputAnim");
        return dispatch(hit.action,state);
    }
    return {};
}
// What one menu key does on the page right now. A click on a key's glyph comes here too,
// so a prompt clicked with the mouse does exactly what its key does. nullopt: the key
// means nothing here.
std::optional<Json> InventoryUI::press(const std::string& key,const State& state) {
    const bool character_controls=!native_picker_ && confirm_action_.is_null() && physics_modal_control_.empty();
    if(!confirm_action_.is_null()) {
        if(key=="accept") return dispatch({{"action","ui_confirm_proceed"}},state);
        if(key=="close") return dispatch({{"action","ui_confirm_cancel"}},state);
        return std::nullopt;
    }
    if(native_picker_) {
        if(key=="up") { native_options_.move(-1); build_native_picker_results(); return Json{}; }
        if(key=="down") { native_options_.move(1); build_native_picker_results(); return Json{}; }
        if(key=="accept") return dispatch({{"action","ui_pick_apply"}},state);
        if(key=="close") return dispatch({{"action","ui_pick_cancel"}},state);
        return std::nullopt;
    }
    if(!physics_modal_control_.empty()) {
        if(key=="close") return dispatch({{"action","ui_physics_modal_close"}},state);
        // Stops: the sliders, a Motion switch on a rig, then Reset part defaults.
        const Control* ctrl=nullptr; const Selection* worn_selection=nullptr; const Outfit* worn_outfit=nullptr;
        if(catalog_ && appearance_) if(auto sel=state.selections.find(appearance_->shell);sel!=state.selections.end()) {
            worn_selection=&sel->second;
            for(const auto& o:catalog_->outfits) if(o.id==sel->second.outfit) worn_outfit=&o;
            if(worn_outfit) ctrl=worn_outfit->controls_for(sel->second.variant).find(physics_modal_control_);
        }
        if(!ctrl) return dispatch({{"action","ui_physics_modal_close"}},state);
        const bool rig=ctrl->kind==ControlKind::Rig;
        const int fields=3, motion=rig?fields:-1, reset=fields+(rig?1:0), stops=reset+1;
        const int focus=physics_modal_channel_%stops;
        if(key=="up" || key=="down") {
            physics_modal_channel_=(focus+(key=="down"?1:stops-1))%stops;
            dirty_=true; return Json{};
        }
        const bool sideways=key=="left" || key=="right";
        if(sideways && focus<fields) return dispatch({{"action","control"},{"control",physics_modal_control_},{"channel",focus},{"ui_channel",focus},{"delta",key=="right"?1:-1}},state);
        // Left/Right on the Motion switch flips it, like Accept.
        if(key=="accept" || (sideways && focus==motion)) {
            if(focus==motion) {
                const auto& opts=worn_outfit->controls_for(worn_selection->variant);
                auto vals=control_values(opts,worn_selection->custom);
                const float current=vals.contains(ctrl->id)?vals.at(ctrl->id)[3]:ctrl->value[3];
                return dispatch({{"action","control"},{"control",physics_modal_control_},{"channel",3},{"value",current==1.f?0.f:1.f}},state);
            }
            if(sideways) return std::nullopt;
            if(focus==reset) return dispatch({{"action","reset_control"},{"control",physics_modal_control_}},state);
            return dispatch({{"action","ui_physics_modal_close"}},state);
        }
        return std::nullopt;
    }
    if(key=="close") return dispatch({{"action","ui_close"}},state);
    if(key=="reset_view") return dispatch({{"action",light_edit_?"ui_reset_light":"ui_reset_view"}},state);
    if(key=="toggle_light") {
        if(character_controls) return dispatch({{"action","ui_toggle_light"}},state);
        return std::nullopt;
    }
    if(key=="previous_section" || key=="next_section") return dispatch({{"action","ui_section"},{"section",(section_+(key=="next_section"?1:4))%5}},state);
    if(rows_.empty()) return std::nullopt;
    if(key=="up" || key=="down") {
        row_=std::clamp(row_+(key=="up"?-1:1),0,int(rows_.size())-1);
        dirty_=true; return Json{};   // the build scrolls the row into view
    }
    const auto& row=rows_[row_];
    auto action=key=="left"?row.previous:key=="right"?row.next:key=="accept"?row.accept:key=="secondary"?row.secondary:row.tertiary;
    dirty_=true;
    return dispatch(action,state);
    return std::nullopt;
}
Json InventoryUI::diagnostics() const {
    Json value={{"attached",tab_.Get()!=nullptr},{"active",active_},{"section",section_},{"row",row_},{"rows",rows_.size()},{"camera",display_.Get()!=nullptr},{"page_widgets",page_widgets_},{"nested_widgets",nested_widgets_},
#ifdef CSS_INVENTORY_DEV
        {"slow_builds",slow_builds_},
#endif
        {"layout_size",layout_size_},{"yaw",yaw_},{"pan",pan_},{"zoom",zoom_},{"frame",frame_},{"gamepad",gamepad_}};
    #ifdef CSS_INVENTORY_DEV
    if(auto* scroll=choice_scroll_.Get()) {
        Call offset(scroll,L"GetScrollOffset",1);offset.run();
        value["choice_list"]={{"path",narrow(scroll->GetPathName())},{"key",choice_key_},
            {"selected",choice_selected_},{"count",choice_count_},{"offset",offset.get<float>()}};
    }
    value["picker"]=native_picker_;
    value["detail"]=detail_title_;
    value["physics_modal"]=physics_modal_control_;
    value["confirm_dialog"]=!confirm_action_.is_null();
    value["native_picker"]=native_picker_;
    if(native_picker_) {value["native_query"]=native_search_query_;value["native_matches"]=native_options_.matches.size();value["native_selected"]=native_options_.value();}
    value["menu_open"]=main_.Get() && inventory_bool(main_.Get(),L"bOpen");
    for(const auto& b:bindings_) value["bindings"][b.action]=b.keys;
    #endif
    return value;
}
}
namespace css {
void InventoryUI::message(const std::string& value) {
    if(value==last_message_) return;
    const bool template_status=(value.starts_with("Template ") || value.starts_with("Profile ")) && section_!=4;
    const bool routine=template_status || value.starts_with("Wearing ") || value=="Settings updated." || value=="Original appearance restored." || value.starts_with("Your saved appearance") || value.starts_with("Choose an appearance") || value.starts_with("No CSS outfit packages found.");
    if(auto* widget=status_.Get()) { text_value(widget,routine?"":value); last_message_=value; }
}
}

// Included after attachment_follower.inl and walk_override.inl, so it can use
// attached_children(), attach_socket(), hide_game_object(), read<>, Call and narrow().
//
// MISC visibility. Per category the player picks Default (never touch), Always Hidden, or
// Only When In Use. There is no combat/locomotion guessing: CSS only ever hides an item while
// it RESTS on its stowed or cosmetic socket. Draw a weapon (or fire Eredrim's Diapason) and it
// leaves that socket for the hand, where CSS never touches it, so it shows on its own. That is
// exactly "only when in use". "Always Hidden" is the same plus hiding the drawn copy too.
//
// Bounded and body-safe: it enumerates only the mesh's DIRECT children and hides the owning
// ACTOR, never the body (which is the parent, never a child) and never a pawn-owned component.
// Proven live before this code existed; see investigation/2026-09-23.

// Lowercase helper.
static std::string misc_lower(std::string s) {
    for(char& c:s) c=char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// MISC's own direct-children enumeration. Unlike the shared attached_children (which throws
// above 128 children), this tolerates any count: a fully dressed custom body can have hundreds
// of attached components, and MISC must never abort the whole pass over one crowded mesh.
static std::vector<WeakObject> misc_children(UObject* component, int* raw_count=nullptr) {
    std::vector<WeakObject> result;
    if(!component) return result;
    Call call(component,L"GetChildrenComponents",2); call.set(L"bIncludeAllDescendants",false); call.run();
    auto* property=call.param(L"Children");
    if(!property || !property->IsA<FArrayProperty>()) return result;
    auto* array=static_cast<FArrayProperty*>(property);
    if(!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize()!=sizeof(UObject*)) return result;
    FScriptArrayHelper values(array,call.data(property));
    const int n=values.Num();
    if(raw_count) *raw_count=n;
    if(n<0 || n>4096) return result;   // a sane ceiling, far above any real character
    result.reserve(n);
    for(int i=0;i<n;++i) { UObject* child{}; std::memcpy(&child,values.GetRawPtr(i),sizeof(child)); result.emplace_back(child); }
    return result;
}

// A socket that belongs to the skeleton itself or to a held weapon (hand/prop), as opposed to a
// named attachment socket a shell hangs gear on. Items on these are never treated as accessories;
// a held weapon on Socket_Prop_R is classified separately as a drawn weapon.
static bool misc_body_socket(const std::string& sl) {
    if(sl.empty()||sl=="none"||sl=="root") return true;
    for(const char* k:{"hand","prop","foot","ball_","spine","pelvis","head","neck","clavicle",
                       "upperarm","lowerarm","arm_","thigh","calf","hips","finger","thumb","index",
                       "middle","ring","pinky","wrist","elbow","knee","ankle","toe","chest","breast",
                       "butt","cheek","eye","jaw","tongue","ear","hair","tail"})
        if(sl.find(k)!=std::string::npos) return true;
    return false;
}
// Category of an item resting on a socket, or "" for anything MISC must not touch. Generic across
// shells: seals and stowed weapons are matched by socket, and anything else on a named (non-body,
// non-hand) socket is a shell ornament or tool. Body/hand sockets and audio/VFX never classify.
static std::string misc_category(const std::string& socket, const std::string& owner_class) {
    const std::string sl=misc_lower(socket), ol=misc_lower(owner_class);
    auto in=[](const std::string& h,const char* n){ return h.find(n)!=std::string::npos; };
    // Seal first: its socket also contains "prop" and "stowed".
    if(in(sl,"seal")||in(ol,"seal")) return "seal";
    if(in(sl,"stowed")) {   // an item holstered on the body
        if(in(ol,"nailshotgun")||in(ol,"ballistazooka")||in(ol,"crossbow")||in(ol,"machinegun")
           ||in(ol,"parasite")||in(ol,"ballista")||in(ol,"shotgun")
           ||in(sl,"nailshotgun")||in(sl,"ballistazooka")||in(sl,"crossbow")||in(sl,"machinegun")||in(sl,"parasite"))
            return "sidearm";
        if(in(sl,"shellitem")||in(ol,"pouch")||in(ol,"relic")||in(ol,"charm")||in(ol,"totem")||in(ol,"idol"))
            return "accessories";
        return "stowed_weapons";   // remaining stowed items are holstered melee weapons
    }
    // Any other item on a named, non-body socket is a shell ornament or usable shell tool
    // (Eredrim's Diapason, Tiel's dagger charm, a flower crown, a cape, a pouch...).
    if(!misc_body_socket(sl)) return "accessories";
    return "";
}

// Category of a weapon actor by its class alone (used for the drawn, in-hand weapon, which has
// no stowed socket). Only weapon categories can be "drawn"; seals/accessories return "".
static std::string misc_weapon_category(const std::string& owner_class) {
    const std::string ol=misc_lower(owner_class);
    auto in=[&](const char* n){ return ol.find(n)!=std::string::npos; };
    if(in("nailshotgun")||in("ballistazooka")||in("crossbow")||in("machinegun")||in("parasite")
       ||in("ballista")||in("shotgun")) return "sidearm";
    if(in("axatana")||in("katana")||in("sword")||in("hammer")||in("mace")||in("blade")
       ||in("trebuchaxe")||in("axe")||in("spear")||in("halberd")||in("scythe")||in("glaive")) return "stowed_weapons";
    return "";
}

// Set a component's game-hidden flag WITHOUT propagating to its children. bHiddenInGame is what
// actually stops the render, and it is what the ground-truth diagnostic showed the game leaving
// on for some weapons even after the actor was hidden. Propagation is off on purpose: hiding a
// weapon mesh with propagation exposed its physics/collision shapes (red wireframe).
static void set_component_hidden(UObject* comp, bool hide) {
    if(!comp) return;
    Call set(comp,L"SetHiddenInGame",2);
    set.set(L"NewHidden",hide);
    set.set(L"bPropagateToChildren",false);
    set.run();
}
static bool component_game_hidden(UObject* comp) {
    if(!comp) return false;
    try {
        if(auto* p=comp->GetPropertyByNameInChain(L"bHiddenInGame"); p && p->IsA<FBoolProperty>())
            return static_cast<FBoolProperty*>(p)->GetPropertyValue(reinterpret_cast<std::byte*>(comp)+p->GetOffset_Internal());
    } catch(...) {}
    return false;
}
// Every mesh component an actor owns. A single weapon or shell tool can carry several meshes
// (blade + guard, or a base plus a nested piece), and only some are direct children of the body
// mesh, so hiding just the enumerated child leaves the rest rendering. Bounded and defensive.
static std::vector<WeakObject> actor_mesh_components(UObject* actor) {
    std::vector<WeakObject> out;
    if(!actor) return out;
    UObject* mesh_class=find_optional(L"/Script/Engine.MeshComponent");   // base of skeletal + static mesh, cached
    if(!mesh_class) return out;
    try {
        Call call(actor,L"K2_GetComponentsByClass",2);
        call.set(L"ComponentClass",mesh_class);
        call.run();
        auto* property=call.param(L"ReturnValue");
        if(!property || !property->IsA<FArrayProperty>()) return out;
        auto* array=static_cast<FArrayProperty*>(property);
        if(!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize()!=sizeof(UObject*)) return out;
        FScriptArrayHelper values(array,call.data(property));
        const int n=values.Num();
        if(n<0 || n>256) return out;
        for(int i=0;i<n;++i) { UObject* c{}; std::memcpy(&c,values.GetRawPtr(i),sizeof(c)); if(c) out.emplace_back(c); }
    } catch(...) {}
    return out;
}
// Hide an item: set bHiddenInGame on EVERY mesh the owner carries (that is what actually stops
// the render, even when the actor flag does not reach a nested mesh) and hide the owner actor
// (keeps its collision down so nothing draws underneath). Show reverses both. Falls back to the
// single enumerated component if the actor exposes no component list.
static void set_item_hidden(UObject* comp, UObject* owner, bool hide) {
    bool any=false;
    if(owner) for(auto& c:actor_mesh_components(owner)) { if(auto* m=c.Get()) { try { set_component_hidden(m,hide); any=true; } catch(...) {} } }
    if(!any) { try { set_component_hidden(comp,hide); } catch(...) {} }
    try { if(owner) hide_game_object(owner,hide); } catch(...) {}
}

// Is the player mid-action (an attack, dodge, parry or similar montage is playing)? A melee
// weapon that lives in the hand rather than a stowed socket is "in use" only during such an
// action, not while it is merely held at idle, so this is what "only when in use" keys off for
// a drawn weapon.
static bool misc_action_active(UObject* pawn) {
    if(!pawn) return false;
    try {
        auto* mesh=read<UObject*>(pawn,L"Mesh");
        if(!mesh) return false;
        Call ai(mesh,L"GetAnimInstance",1); ai.run();
        auto* anim=ai.get<UObject*>();
        if(!anim) return false;
        Call playing(anim,L"IsAnyMontagePlaying",1); playing.run();
        return playing.get<bool>();
    } catch(...) { return false; }
}

// Authoritative category per equipped weapon actor, read from the game's own EquippedWeapons map
// (slot gameplay-tag -> weapon actor). No name guessing: the tag says exactly what each item is.
//   Weapon.Slot.Shell.*    -> accessories (a shell tool, e.g. Eredrim's Diapason)
//   Weapon.Slot.Sidearm    -> sidearm
//   Weapon.Slot.*Seal*     -> seal
//   Weapon.Slot.Primary/Secondary -> stowed_weapons (the primary weapon)
//   Weapon.Slot.Body.*     -> fists/kicks, skipped
struct MiscSlot { std::string category; bool shell_item=false; };
static std::map<UObject*,MiscSlot> misc_weapon_slots(UObject* pawn) {
    std::map<UObject*,MiscSlot> out;
    if(!pawn) return out;
    UObject* wc=nullptr;
    try { wc=read<UObject*>(pawn,L"WeaponsComponent"); } catch(...) { return out; }
    if(!wc) return out;
    try {
        auto* p=wc->GetPropertyByNameInChain(L"EquippedWeapons");
        if(!p || !p->IsA<FMapProperty>()) return out;
        auto* mp=static_cast<FMapProperty*>(p);
        auto* keyp=mp->GetKeyProp(); auto* valp=mp->GetValueProp();
        if(!keyp || !valp || !valp->IsA<FObjectProperty>()) return out;
        const auto& layout=mp->GetMapLayout();
        if(keyp->GetSize()<int(sizeof(FName)) || valp->GetSize()!=sizeof(UObject*) ||
           layout.ValueOffset<keyp->GetSize() || layout.SetLayout.Size<=0 || layout.SetLayout.Size>65536 ||
           layout.ValueOffset>layout.SetLayout.Size-int(sizeof(UObject*))) return out;
        auto* scriptmap=reinterpret_cast<FScriptMap*>(reinterpret_cast<std::byte*>(wc)+p->GetOffset_Internal());
        const int end=scriptmap->GetMaxIndex();
        if(end<0 || end>4096) return out;
        for(int i=0;i<end;++i) {
            if(!scriptmap->IsValidIndex(i)) continue;
            auto* pair=static_cast<std::byte*>(scriptmap->GetData(i,layout));
            const std::string tag=misc_lower(narrow(reinterpret_cast<FName*>(pair)->ToString()));   // FGameplayTag.TagName is the first member
            UObject* weapon=nullptr; std::memcpy(&weapon,pair+layout.ValueOffset,sizeof(weapon));
            if(!weapon) continue;
            auto in=[&](const char* n){ return tag.find(n)!=std::string::npos; };
            MiscSlot slot;
            if(in("body")||in("fist")||in("leg")||in("kick")) continue;
            // Weapon.Slot.Shell.* is a shell's own tool; Weapon.Slot.Charges is a shell's charge
            // item (Gragu's Revered Heart). Both are shell-specific, so both get their own row.
            else if(in("shell")||in("charges")) { slot.category="accessories"; slot.shell_item=true; }
            else if(in("sidearm")) slot.category="sidearm";
            else if(in("seal")) slot.category="seal";
            else if(in("primary")||in("secondary")) slot.category="stowed_weapons";
            else continue;
            out[weapon]=slot;
        }
    } catch(...) {}
    return out;
}
// Is the item sitting on a hand/prop socket (held) rather than a holster/ornament socket (at rest)?
static bool misc_held_in_hand(const std::string& socket) {
    const std::string sl=misc_lower(socket);
    if(sl.find("stowed")!=std::string::npos) return false;   // holstered
    return sl.find("hand")!=std::string::npos || sl.find("prop")!=std::string::npos;
}

void MiscVisibility::enumerate(const std::vector<UObject*>& containers, UObject* pawn) {
    if(!pawn) { candidates_.clear(); return; }
    const auto slots=misc_weapon_slots(pawn);   // owner actor -> authoritative category
    std::vector<Item> found;
    // Items can hang off the worn body mesh OR the attachment proxy CSS re-parents them to when
    // the swapped body lacks their socket bone. Keep only non-pawn mesh components: those are the
    // weapons, seals, sidearms and shell tools. Everything else (233-odd body parts, audio, VFX)
    // is skipped, so the per-frame pass only ever touches the ~10 real items.
    for(auto* container:containers) {
        if(!container) continue;
        for(auto& child_weak:misc_children(container)) {
            auto* child=child_weak.Get();
            if(!child) continue;
            std::string cc; if(auto* cls=child->GetClassPrivate()) cc=narrow(cls->GetName());
            if(cc.find("Mesh")==std::string::npos) continue;
            UObject* owner=nullptr;
            try { Call oc(child,L"GetOwner",1); oc.run(); owner=oc.get<UObject*>(); } catch(...) { continue; }
            if(!owner || owner==pawn) continue;
            std::string category; bool shell_item=false;
            std::string owner_class; if(auto* cls=owner->GetClassPrivate()) owner_class=narrow(cls->GetName());
            if(auto it=slots.find(owner); it!=slots.end()) { category=it->second.category; shell_item=it->second.shell_item; }   // authoritative
            else category=misc_category(narrow(attach_socket(child).ToString()),owner_class);   // a non-weapon accessory (flower crown, cape): fall back to the socket
            if(category.empty()) continue;
            std::string key=misc_lower(owner_class);
            if(key.ends_with("_c")) key.resize(key.size()-2);
            found.push_back({WeakObject(child),WeakObject(owner),category,key,shell_item});
        }
    }
    candidates_=std::move(found);
}

void MiscVisibility::evaluate(const std::map<std::string,MiscRule>& rules, bool action_active) {
    auto rule_of=[&](const std::string& cat)->const MiscRule* { auto it=rules.find(cat); return it==rules.end()?nullptr:&it->second; };
    auto has=[&](std::vector<Item>& v,UObject* comp){ for(auto& h:v) if(h.component.Get()==comp) return true; return false; };
    std::vector<Item> want;
    want.reserve(candidates_.size());   // bounded candidate set; avoid per-frame reallocation growth
    for(auto& cand:candidates_) {
        auto* comp=cand.component.Get();
        if(!comp) continue;
        // The item's own rule first (a shell item row on the MISC tab), else its category's.
        const MiscRule* r=cand.shell_item?rule_of("item:"+cand.key):nullptr;
        if(!r) r=rule_of(cand.category);   // category is authoritative, decided at enumerate
        if(!r) continue;
        bool hide=false;
        if(r->mode=="hidden") hide=true;
        else if(r->mode=="shown") hide=false;   // an item row keeping its item while its category hides
        else if(r->mode=="in_use") {
            // Re-read the socket each frame: it changes the instant the item is drawn or used.
            // Resting on its holster/ornament socket -> hidden; it shows when the game moves it
            // off that socket to use it, so each item reveals only when IT is used.
            if(!misc_held_in_hand(narrow(attach_socket(comp).ToString()))) hide = true;
            // A sidearm/ranged weapon only ever leaves its holster to be aimed or fired, so being
            // in the hand already means "in use" -> show it (aiming is not a montage, so gating a
            // drawn sidearm on action_active hid it while the player was aiming without firing).
            else if(cand.category=="sidearm") hide = false;
            // A melee weapon is held in the hand even at idle, so it only counts as in use while
            // an action (swing/parry/ability) montage is playing.
            else hide = !action_active;
        }
        if(hide && !has(want,comp)) want.push_back(cand);
    }
    // Show back what is no longer wanted hidden.
    for(auto& h:hidden_) {
        auto* comp=h.component.Get();
        if(comp && !has(want,comp)) set_item_hidden(comp,h.owner.Get(),false);
    }
    // Hide what should be hidden: newly wanted, or an item the game turned back on this frame.
    for(auto& w:want) {
        auto* comp=w.component.Get();
        if(comp && (!has(hidden_,comp) || !component_game_hidden(comp))) set_item_hidden(comp,w.owner.Get(),true);
    }
    hidden_=std::move(want);
}

// Names and one-line descriptions for the shell items the game ships (from its own text where
// it has one). Anything else gets its class name made readable.
static MiscShellItem misc_shell_item_info(const std::string& key) {
    static const MiscShellItem known[]={
        {"wp_alienheart","Revered Heart","Gragu's heart, carried on the belt and eaten to restore health. Hidden, it still heals."},
        {"wp_eredrim_diapason","Diapason","Eredrim's bell, hung on the body until his ability rings it."},
        {"wp_eredrim_diapason_obsidian","Diapason","Eredrim's bell, hung on the body until his ability rings it."},
        {"wp_genessa_catalyst","Catalyst","Genessa's incense burner, stowed on the body until her ability uses it."},
        {"wp_knightlady_hook_left","Hook","Proxima's hook, carried on the left until thrown."},
        {"wp_thornarmor","Thorns","Thorn's spines, worn over the body."},
        {"wp_tiel_dagger_right","Dagger","Tiel's charm dagger, worn on the body."},
        {"wp_tiel_dagger_right_ghost","Dagger","Tiel's charm dagger, worn on the body."},
    };
    for(const auto& item:known) if(item.key==key) return item;
    std::string name=key.starts_with("wp_")?key.substr(3):key; bool cap=true;
    for(char& c:name) { if(c=='_') { c=' '; cap=true; } else if(cap) { c=char(std::toupper(static_cast<unsigned char>(c))); cap=false; } }
    return {key,name,name+" is carried on the body. Hide it, or show it only while it is used."};
}
std::vector<MiscShellItem> MiscVisibility::shell_items() const {
    std::vector<MiscShellItem> out;
    for(const auto& cand:candidates_) {
        if(!cand.shell_item || !cand.component.Get()) continue;
        if(std::any_of(out.begin(),out.end(),[&](const auto& i){ return i.key==cand.key; })) continue;
        out.push_back(misc_shell_item_info(cand.key));
    }
    return out;
}
void MiscVisibility::restore() {
    for(auto& h:hidden_) { auto* comp=h.component.Get(); if(comp) set_item_hidden(comp,h.owner.Get(),false); }
    hidden_.clear();
}

// Ground-truth diagnostic: report every MISC-relevant item on a mesh and the visibility state
// the game currently holds for it, so weapon-hiding behaviour can be observed in real gameplay
// instead of guessed at. Read-only.
static Json misc_report_items(const std::vector<UObject*>& containers, UObject* pawn) {
    Json out=Json::object();
    Json items=Json::array();
    Json counts=Json::object();
    if(!pawn) { out["items"]=items; return out; }
    auto read_bool=[](UObject* o,const wchar_t* name)->int {
        if(!o) return -1;
        try { if(auto* p=o->GetPropertyByNameInChain(name); p && p->IsA<FBoolProperty>())
            return static_cast<FBoolProperty*>(p)->GetPropertyValue(reinterpret_cast<std::byte*>(o)+p->GetOffset_Internal())?1:0; } catch(...) {}
        return -1;
    };
    int idx=0;
    for(auto* container:containers) {
        int raw=-1;
        std::vector<WeakObject> kids=misc_children(container,&raw);
        for(auto& child_weak:kids) {
            auto* child=child_weak.Get();
            if(!child) continue;
            UObject* owner=nullptr;
            try { Call oc(child,L"GetOwner",1); oc.run(); owner=oc.get<UObject*>(); } catch(...) { continue; }
            if(!owner || owner==pawn) continue;
            std::string owner_class; if(auto* cls=owner->GetClassPrivate()) owner_class=narrow(cls->GetName());
            const std::string socket=narrow(attach_socket(child).ToString());
            std::string cat=misc_category(socket,owner_class);
            std::string drawn=misc_weapon_category(owner_class);
            if(cat.empty() && drawn.empty()) continue;
            items.push_back({
                {"container",idx},{"owner",owner_class},{"socket",socket},
                {"rest_category",cat},{"weapon_category",drawn},
                {"actor_bHidden",read_bool(owner,L"bHidden")},
                {"comp_bHiddenInGame",read_bool(child,L"bHiddenInGame")},
                {"comp_bVisible",read_bool(child,L"bVisible")},
            });
        }
        counts[std::to_string(idx)]=raw;
        ++idx;
    }
    out["container_child_counts"]=counts;
    out["items"]=items;
    // Full dump: every non-pawn-owned mesh component attached to any container, with its owner,
    // socket, visibility and world location. This is the complete picture of what is on the body
    // so nothing (a dropped-looking blade, an unclassified mesh) can be missed.
    Json all=Json::array();
    for(auto* container:containers) {
        if(!container) continue;
        for(auto& child_weak:misc_children(container)) {
            auto* child=child_weak.Get(); if(!child) continue;
            std::string cc; if(auto* c=child->GetClassPrivate()) cc=narrow(c->GetName());
            if(cc.find("Mesh")==std::string::npos) continue;   // only visible mesh components
            UObject* owner=nullptr; try { Call oc(child,L"GetOwner",1); oc.run(); owner=oc.get<UObject*>(); } catch(...) {}
            if(!owner || owner==pawn) continue;
            std::string oc_name; if(auto* c=owner->GetClassPrivate()) oc_name=narrow(c->GetName());
            std::array<double,3> loc{}; try { Call gl(child,L"K2_GetComponentLocation",1); gl.run(); loc=gl.get<std::array<double,3>>(); } catch(...) {}
            all.push_back({{"owner",oc_name},{"component",cc},{"socket",narrow(attach_socket(child).ToString())},
                {"cH",read_bool(child,L"bHiddenInGame")},{"cV",read_bool(child,L"bVisible")},
                {"z",int(loc[2])}});
        }
    }
    out["all_meshes"]=all;
    return out;
}

Json Appearance::misc_report() const {
    auto* pawn=observed_pawn_.Get();
    Json r=Json::object();
    r["pawn_valid"]=pawn!=nullptr;
    if(!pawn) return r;
    auto* mesh=read<UObject*>(pawn,L"Mesh");
    auto* proxy=attachments_.proxy();
    r["mesh_valid"]=mesh!=nullptr;
    r["proxy_valid"]=proxy!=nullptr;
    if(mesh) { if(auto* cls=mesh->GetClassPrivate()) r["mesh_class"]=narrow(cls->GetName()); }
    Json items=misc_report_items({mesh, proxy}, pawn);
    for(auto& [k,v]:items.items()) r[k]=v;
    return r;
}

// A cheap fingerprint of everything attached to the bodies MISC watches: the containers and the
// pointers in their AttachChildren arrays, read in place. Equipping, swapping, drawing a new
// weapon or opening the menu changes it, so the candidate lists are rebuilt the same frame
// instead of being re-walked (a PE per child, hundreds of children) twenty times a second.
static void misc_mix(uint64_t& hash, const void* pointer) {
    hash^=reinterpret_cast<uintptr_t>(pointer); hash*=0x100000001b3ull;
}
static void misc_mix_children(uint64_t& hash, UObject* container) {
    misc_mix(hash,container);
    if(!container) return;
    auto* property=optional_field(container,L"AttachChildren");
    if(!property || !property->IsA<FArrayProperty>()) return;
    auto* array=static_cast<FArrayProperty*>(property);
    if(!array->GetInner()->IsA<FObjectProperty>() || array->GetInner()->GetElementSize()!=sizeof(UObject*)) return;
    FScriptArrayHelper values(array,reinterpret_cast<std::byte*>(container)+property->GetOffset_Internal());
    const int n=values.Num();
    misc_mix(hash,reinterpret_cast<const void*>(static_cast<uintptr_t>(n)));
    if(n<=0 || n>4096) return;
    for(int i=0;i<n;++i) { UObject* child{}; std::memcpy(&child,values.GetRawPtr(i),sizeof(child)); misc_mix(hash,child); }
}
bool Appearance::misc_layout_changed() {
    if(std::exchange(misc_rules_changed_,false)) { misc_signature_=0; return true; }
    auto* pawn=observed_pawn_.Get();
    if(!pawn || misc_rules_.empty()) return false;
    uint64_t hash=0xcbf29ce484222325ull;
    misc_mix(hash,pawn);
    misc_mix_children(hash,read<UObject*>(pawn,L"Mesh"));
    misc_mix_children(hash,attachments_.proxy());
    misc_mix(hash,active_display_menu(pawn));
    misc_mix_children(hash,menu_display_mesh_.Get());
    misc_mix_children(hash,menu_attachments_.proxy());
    const bool changed=hash!=misc_signature_;
    misc_signature_=hash;
    return changed;
}

void Appearance::sync_misc() {   // on a layout change: rebuild candidate lists
    auto* pawn=observed_pawn_.Get();
    if(!pawn) { misc_.restore(); menu_misc_.restore(); return; }
    // With no rule set nothing is enforced, but the world list still feeds the MISC tab's
    // shell item rows, so the worn shell's items appear before any rule exists.
    if(misc_rules_.empty()) { misc_.restore(); menu_misc_.restore(); misc_.enumerate({read<UObject*>(pawn,L"Mesh"), attachments_.proxy()}, pawn); return; }
    if(auto* display=menu_character(pawn)) {
        auto* display_mesh=read<UObject*>(display,L"Mesh");
        menu_display_mesh_=display_mesh;
        menu_misc_.enumerate({display_mesh, menu_attachments_.proxy()}, display);
    } else menu_display_mesh_=nullptr;
    misc_.enumerate({read<UObject*>(pawn,L"Mesh"), attachments_.proxy()}, pawn);
}

void Appearance::tick_misc() {   // every frame: decide + enforce on the cached items
    auto* pawn=observed_pawn_.Get();
    if(!pawn || misc_rules_.empty()) return;
    // Only "only when in use" asks whether an action is playing; skip the two engine calls otherwise.
    bool in_use=false;
    for(const auto& [category,rule]:misc_rules_) in_use=in_use || rule.mode=="in_use";
    const bool action_active=in_use && misc_action_active(pawn);
    misc_.evaluate(misc_rules_, action_active);
    // The wardrobe preview stands idle, so no action is ever active for it.
    menu_misc_.evaluate(misc_rules_, false);
}

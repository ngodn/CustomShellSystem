#include "api.hpp"
#include "data.hpp"
#include "engine.hpp"
#include "recovery.hpp"
#include "player_recovery.hpp"
#include "startup.hpp"
#include <windows.h>
#include <chrono>
#include <optional>
#include <imgui.h>
#include <UE4SSProgram.hpp>
#include <USMapGenerator/Generator.hpp>
#ifdef CSS_INVENTORY_DEV
#include "frame_profile.hpp"
#endif

namespace css {
struct Core {
    CssHost host;
    fs::path root;
    fs::path package_root;
    Catalog catalog;
    bool original_shells_attempted=false;
    State state;
    Appearance appearance;
    InventoryUI inventory;
    ExtensionBridge extension_bridge;
    ExtensionClient extensions;
    HudService hud_;
    CssxHost recovery_host{CSSX_ABI,sizeof(CssxHost),this,extension_request,nullptr};
    PlayerRecovery player_recovery{&recovery_host};
    void* current_engine=nullptr;
    bool extension_attempted=false;
    static int extension_request(void* context,const char* bytes,CssxSink sink,void* output) {
        auto& core=*static_cast<Core*>(context);
        try {
            const auto request=Json::parse(bytes);const auto op=request.at("op").get<std::string>();
            Json result;
            if(op=="log") {core.host.log(request.at("message").get<std::string>().c_str());}
            else if(op=="menu.close") {core.inventory.close();result=true;}
            else if(op=="menu.status") result=core.inventory.diagnostics();
            else if(op=="input.focus") {
                DWORD pid=0;const auto window=GetForegroundWindow();
                if(window) GetWindowThreadProcessId(window,&pid);
                result=window && pid==GetCurrentProcessId();
            }
            else if(op=="hud.minimap.build") { if(!core.current_engine) throw std::runtime_error("Game thread is not initialized"); result=minimap_build(core.current_engine,request.value("config",Json::object())); }
            else if(op=="hud.minimap.update") { minimap_update(request.value("update",Json::object())); result=true; }
            else if(op=="hud.minimap.destroy") { minimap_destroy(); result=true; }
            else if(op=="hud.markers") { if(!core.current_engine) throw std::runtime_error("Game thread is not initialized"); result=markers_collect(core.current_engine,request.value("radius",500.0)); }
            else if(core.current_engine) result=core.extension_bridge.request(core.current_engine,core.appearance,request);
            else throw std::runtime_error("Game thread is not initialized");
            auto data=result.dump();sink(output,data.data(),data.size());return 1;
        } catch(const std::exception& error) {auto data=Json{{"error",error.what()}}.dump();sink(output,data.data(),data.size());return 0;}
    }
    Json inventory_command;
#ifdef CSS_INVENTORY_DEV
    FrameProfile frame_profile;
    void publish_frame_profile() {
        Json rows=Json::array();
        for(const auto& row:frame_profile.rows())
            rows.push_back({{"engine_ms",row.engine_ms},{"interval_ms",row.interval_ms},
                {"core_ms",row.core_ms},{"phase_ms",row.phase_ms},{"failed",row.failed}});
        atomic_json(root/"runtime/frame-profile.json",{{"id",frame_profile.id()},
            {"stop_reason",frame_profile.reason()},{"cssx_loaded",extensions.ready()},
            {"phases",{"recovery","cssx_tick","hud_prepare","cssx_render","inventory"}},
            {"rows",rows}},false);
    }
    Json extension_command;
    Json probe_command;
    Json motion_probe;
    uint64_t motion_probe_until=0,motion_probe_start=0;
    void sample_motion(uint64_t now) {
        if(!motion_probe_until) return;
        try {
            auto query=[&](const Json& value){return extension_bridge.request(current_engine,appearance,value);};
            const auto player=query({{"op","player"}});
            if(player.at("pawn")!=motion_probe.at("pawn")) throw std::runtime_error("Player changed during motion sampling");
            auto mesh=query({{"op","get"},{"target",player.at("pawn")},{"property","Mesh"}});
            if(mesh!=motion_probe.at("mesh")) throw std::runtime_error("Mesh component changed during motion sampling");
            Json frame={{"ms",now-motion_probe_start},{"bones",Json::object()}};
            for(const auto& bone:motion_probe.at("bones")) {
                auto result=query({{"op","call"},{"target",mesh},{"function","GetSocketTransform"},
                    {"args",{{"InSocketName",bone},{"TransformSpace",3}}}});
                frame["bones"][bone.get<std::string>()]=result.at("ReturnValue");
            }
            motion_probe["frames"].push_back(std::move(frame));
            if(now<motion_probe_until && motion_probe["frames"].size()<1800) return;
        } catch(const std::exception& error) { motion_probe["error"]=error.what(); }
        motion_probe_until=0;
        atomic_json(root/"runtime/motion-sample.json",motion_probe);
        motion_probe=Json{};
    }
#endif
    bool inventory_failed=false;
    bool content_path_checked=false;
    uint64_t inventory_retry_after=0;
    std::string message;
    std::string last_request, last_shell, last_pawn, applied_id, last_status;
    uint64_t last_publish_ms = 0;
    bool status_dirty = true;
    std::string selected_outfit, selected_variant;
    std::string last_living_shell;   // most recent CharacterId.Player.Shell.* worn, for Harbinger carry-over.
    bool dirty = false, apply_pending = false, restore_pending = false, rescan_pending = false, forget_current = false;
    bool favorites_only = false;
    bool ui_refresh = false;
    bool inspect_pending = false, transition_inspect_pending = false;
    uint64_t next_poll = 0;
    char search[128]{}, preset_name[96]{};
    double last_apply_ms = 0;
    std::optional<Customization> pending_custom;
    bool custom_only = false, refresh_custom = true;
    uint64_t save_after = 0, last_player_revision = 0, maintenance_after = 0, maintenance_next = 0;
    Recovery recovery;
#ifdef CSS_TRANSITION_TESTS
    std::optional<bool> test_cursor_pending;
#endif
    std::string maintenance_error;
    std::string attachment_error;
    uint64_t attachments_after=0;
    void sync_attachments_safely(uint64_t now) {
        if(now<attachments_after) return;
        attachments_after=now+250;
        try {
            appearance.sync_attachments();
            if(!applied_id.empty()) {
                const auto split=applied_id.find('/');
                if(split!=std::string::npos) for(const auto& outfit:catalog.outfits)
                    if(outfit.id==applied_id.substr(0,split)) appearance.sync_items(outfit,applied_id.substr(split+1));
            }
            attachment_error.clear();
        }
        catch(const std::exception& error) {
            if(attachment_error!=error.what()) {
                attachment_error=error.what();host.log(("Accessory recovery deferred: "+attachment_error).c_str());
            }
            attachments_after=now+1000;
        }
    }
    // Stowed props get their own pass, every frame rather than four times a second:
    // the body swings through a welded seal within a single stride, so a 250 ms
    // correction would just make it flicker.
    std::string seal_error;
    uint64_t seal_after=0;
#ifdef CSS_INVENTORY_DEV
    uint64_t seal_report_after=0;
#endif
    void sync_seals_safely(uint64_t now) {
        if(now<seal_after) return;
        try {
            appearance.sync_seals();seal_error.clear();
#ifdef CSS_INVENTORY_DEV
            if(now>=seal_report_after) { seal_report_after=now+500; atomic_json(root/"runtime/seal.json",appearance.seal_diagnostics(),false); }
#endif
        }
        catch(const std::exception& error) {
            if(seal_error!=error.what()) {seal_error=error.what();host.log(("Stowed item collision deferred: "+seal_error).c_str());}
            seal_after=now+1000;
        }
    }
    std::string walk_error;
    uint64_t walk_after=0;
    void sync_walk_safely(uint64_t now) {
        if(now<walk_after) return;
        walk_after=now+250;
        try {appearance.walk.walk_mod_active(root.parent_path());appearance.sync_walk(use_feminine_animation(state,appearance.shell,AnimationSlot::Idle),use_feminine_animation(state,appearance.shell,AnimationSlot::Walk),false,false);walk_error.clear();}
        catch(const std::exception& error) {
            if(walk_error!=error.what()) {walk_error=error.what();host.log(("Walk animation deferred: "+walk_error).c_str());}
            walk_after=now+1000;
        }
    }
    void sync_menu_safely() {
        if(GetTickCount64()<maintenance_after) return;
        try { appearance.sync_menu(); maintenance_error.clear(); }
        catch(const std::exception& error) {
            if(maintenance_error!=error.what()) { maintenance_error=error.what(); host.log(("Menu preview deferred: "+maintenance_error).c_str()); }
            maintenance_after=GetTickCount64()+1000;
        }
    }

    static fs::path package_directory() {
        wchar_t executable[32768]{};
        auto size=GetModuleFileNameW(nullptr,executable,32768);
        if(!size || size>=32768) throw std::runtime_error("Cannot locate the game's outfit folder");
        return wardrobe_packages(fs::path(executable));
    }
    Catalog load_catalog(const fs::path& folder) const {
        auto path=folder.generic_u8string();
        host.log(("CSS outfit folder: "+std::string(path.begin(),path.end())).c_str());
        try {
            auto result=Catalog::load(root/"catalog",folder,root/"cache/packages");
            host.log(("CSS catalog: "+std::to_string(result.outfits.size())+" outfits; "+
                std::to_string(result.diagnostics.value("pak_files",0))+" pak files scanned; "+
                std::to_string(result.diagnostics.value("rejected",0))+" rejected").c_str());
            for(const auto& file:result.diagnostics["files"])
                host.log(("CSS package "+file.at("status").get<std::string>()+": "+file.at("path").get<std::string>()+
                    (file.contains("reason")?" ("+file.at("reason").get<std::string>()+")":"")).c_str());
            for(const auto& error:result.diagnostics["errors"])
                host.log(("CSS package folder error: "+error.at("path").get<std::string>()+" ("+error.at("reason").get<std::string>()+")").c_str());
            return result;
        } catch(const std::exception& e) { host.log((std::string("CSS catalog failed: ")+e.what()).c_str()); throw; }
    }
    explicit Core(const CssHost& h) : host(h), root(h.root), package_root(package_directory()), catalog(load_catalog(package_root)), message(wardrobe_startup_message(catalog.outfits.size())) {
        extension_bridge.configure_hooks(reinterpret_cast<void*>(h.log));
        hud_.set_logger([this](const std::string& m){ host.log(m.c_str()); });
        hud_.set_object_minter([this](RC::Unreal::UObject* w){ return extension_bridge.track(w); });
        minimap_set_logger([this](const std::string& m){ host.log(m.c_str()); });
        player_recovery.watch();
        inventory.assets(root);
        bool recovered=false;
        state=load_state(root / "state/state.json", &recovered);
        if(recovered) message="Recovered CSS state from its backup.";
        if (fs::exists(root / "request.json")) {
            try { last_request = read_json(root / "request.json").at("id").get<std::string>(); }
            catch (...) {} // Ignore stale malformed requests on a fresh core.
        }
        if (state.enabled && !catalog.outfits.empty()) message = "Your saved appearance is ready. Open Inventory and choose CSS.";
    }
    void report(std::string text) {
        if (message == text) return;
        message = std::move(text); host.log(message.c_str());
        status_dirty = true;
    }
    void save() { atomic_json(root / "state/state.json", state.json()); dirty = false; status_dirty = true; }
    void request(const Json& command) {
        const auto action = command.at("action").get<std::string>();
#ifdef CSS_INVENTORY_DEV
        if(action=="frame_profile") {
            frame_profile.arm(command.at("id").get<std::string>(),command.value("seconds",10.));
            return;
        }
        if(action=="motion_sample") {
            if(motion_probe_until) throw std::runtime_error("Motion sample already running");
            auto bones=command.at("bones").get<std::vector<std::string>>();
            if(bones.empty() || bones.size()>12) throw std::runtime_error("Motion sampling requires 1 to 12 bones");
            for(const auto& name:bones) if(name.empty() || name.size()>96) throw std::runtime_error("Invalid sample bone");
            const auto player=extension_bridge.request(current_engine,appearance,{{"op","player"}});
            const auto mesh=extension_bridge.request(current_engine,appearance,{{"op","get"},{"target",player.at("pawn")},{"property","Mesh"}});
            for(const auto& name:bones) {
                auto index=extension_bridge.request(current_engine,appearance,{{"op","call"},{"target",mesh},
                    {"function","GetBoneIndex"},{"args",{{"BoneName",name}}}});
                if(index.at("ReturnValue").get<int>()<0) throw std::runtime_error("Sample bone missing: "+name);
            }
            motion_probe_start=GetTickCount64();
            motion_probe_until=motion_probe_start+uint64_t(std::clamp(command.value("seconds",5.),.1,15.)*1000);
            motion_probe={{"id",command.at("id")},{"pawn",player.at("pawn")},{"mesh",mesh},{"bones",bones},{"frames",Json::array()}};
            return;
        }
        if (action.starts_with("inventory_")) { inventory_command=command; return; }
        if (action=="cssx_debug") {extension_command=command;return;}
        if (action=="seal_tune") {
            atomic_json(root/"runtime/seal-tune.json",
                        appearance.tune_seals(command.value("lift",-1.),command.value("clearance",-1.),command.value("max_push",-1.)),false);
            report("Stowed item correction retuned.");
            return;
        }
        // CSS's own probe into the live game. CSSX exists so other people can build
        // extensions; this is the same reflected bridge pointed straight at CSS's own
        // appearance state, for working out what the engine is really doing.
        if (action=="css_probe") {probe_command=command;return;}
#endif
        if (action == "export_mappings") {
            RC::OutTheShade::generate_usmap(); report("Exported runtime mappings for asset tooling.");
        }
        else if(action=="inspect_transition") transition_inspect_pending=true;
        else if (action == "inspect") {
            inspect_pending=true;
        }
#ifdef CSS_TRANSITION_TESTS
        else if(action=="test_cursor") { test_cursor_pending=command.at("visible").get<bool>(); }
        else if(action=="test_effect") { appearance.test_effect(command.at("begin").get<bool>(),command.value("parameters",false)); }
        else if(action=="test_reset_mesh") { appearance.test_reset_mesh(); }
#endif
        else if (action == "favorite") {
            auto id = command.at("outfit").get<std::string>();
            if (state.favorites.contains(id)) state.favorites.erase(id); else state.favorites.insert(id);
            dirty = true; ui_refresh = true;
        }
        else if (action == "save_look" || action == "save_profile") {
            auto name = command.at("name").get<std::string>();
            if (!valid_id(name) || (state.presets.size() >= 64 && !state.presets.contains(name))) throw std::runtime_error("Invalid profile slot");
            state.presets[name] = Preset{state.selections, state.walk_animation, state.animation_choices}; dirty = true; ui_refresh = true; report("Current character profile saved to " + name);
        }
        else if (action == "load_look" || action == "load_profile") {
            auto name = command.at("name").get<std::string>();
            const auto& preset = state.presets.at(name);
            const auto& selections = preset.selections;
            auto selected = selections.find(appearance.shell);
            if (selected == selections.end()) { restore_pending = true; forget_current = true; }
            else { selected_outfit = selected->second.outfit; selected_variant = selected->second.variant; pending_custom=selected->second.custom; apply_pending = true; }
            if(state.walk_animation!=preset.walk_animation) { state.walk_animation=preset.walk_animation; dirty=true; }
            if(state.animation_choices!=preset.animation_choices) { state.animation_choices=preset.animation_choices; dirty=true; }
            ui_refresh = true; report("Profile loaded: " + name);
        }
        else if(action=="delete_look" || action=="delete_profile") {
            state.presets.erase(command.at("name").get<std::string>());
            dirty=true; ui_refresh=true; report("Profile deleted.");
        }
        else if(action=="rename_look" || action=="rename_profile") {
            auto before=command.at("name").get<std::string>(), after=command.at("new_name").get<std::string>();
            if(!valid_id(after)) throw std::runtime_error("Use letters, numbers, periods, underscores or hyphens in the profile name.");
            if(before!=after) {
                if(state.presets.contains(after)) throw std::runtime_error("A profile with that name already exists.");
                auto copy=state.presets.at(before); state.presets.emplace(after,std::move(copy)); state.presets.erase(before);
                dirty=true; ui_refresh=true; report("Profile renamed.");
            }
        }
        else if(action=="animation_choice") {
            const auto slot=animation_slot_from_name(command.at("slot").get<std::string>());
            if(!slot) throw std::runtime_error("Unknown animation slot");
            const auto outfit=command.at("outfit").get<std::string>();
            const auto variant=command.at("variant").get<std::string>();
            const auto choice=command.at("value").get<std::string>();
            if(set_animation_choice(state,catalog,appearance.shell,outfit,variant,*slot,choice)) dirty=true;
            walk_after=0;ui_refresh=true;
            report("Animation choice saved for this outfit.");
        }
        else if (action == "walk_animation") {
            auto value = command.at("value").get<std::string>();
            if(set_legacy_walk_choice(state,catalog,appearance.shell,value)) dirty=true;
            walk_after=0;ui_refresh = true;
            const bool has_mod = appearance.walk.walk_mod_active(root.parent_path());
            const auto& mod_name = appearance.walk.walk_mod_name();
            if(value=="feminine") {
                if(has_mod) report("Feminine walk enabled. "+mod_name+" by argisht is installed; CSS holds the walk animation while this is on.");
                else report("Feminine walk enabled.");
            } else {
                if(has_mod) report("Default walk restored. "+mod_name+" by argisht is active and handles walking again.");
                else report("Default walk restored.");
            }
        }
        else if (action == "harbinger_mirror") {
            const bool value = command.at("value").get<bool>();
            if(state.harbinger_mirror!=value) { state.harbinger_mirror=value; dirty=true; }
            // Re-reconcile now so the change is visible without waiting for the next sever.
            if(!appearance.shell.empty()) { last_shell.clear(); apply_pending=true; }
            ui_refresh = true;
            report(value ? "Harbinger will wear your shell's outfit."
                         : "Harbinger keeps its own saved outfit.");
        }
        // 0.3.3 preview offered these two. They are answered so an old binding or a
        // stale UI does not error, but the setting no longer exists.
        else if (action == "jog_animation" || action == "sprint_animation") {
            ui_refresh = true;
            report("Jogging and sprinting use the game's own animation in this version.");
        }
        else if (action == "enable") { state.enabled = true; apply_pending = true; dirty = true; report("CSS enabled. Open Inventory and choose CSS."); }
        else if (action == "disable") { state.enabled = false; restore_pending = true; dirty = true; }
        else if (action == "restore") { restore_pending = true; forget_current = true; }
        else if (action == "rescan") { rescan_pending = true; }
        // 1.0 renamed "color"/"reset_color" to "control"/"reset_control", since a control
        // can now be a switch, a texture or a spring. The old names still work: scripts and
        // bindings written against 0.4 keep running.
        else if(action=="palette" || action=="control" || action=="reset_control" ||
                action=="color" || action=="reset_color" ||
                action=="tint" || action=="reset_tint" ||
                action=="template") {
            const bool clearing=action=="reset_control" || action=="reset_color";
            auto selected=state.selections.find(appearance.shell);
            if(selected==state.selections.end()) throw std::runtime_error("Wear an appearance before changing it");
            const Outfit* outfit=nullptr;
            for(const auto& o:catalog.outfits) if(o.id==selected->second.outfit) outfit=&o;
            if(!outfit) throw std::runtime_error("The selected outfit is missing");
            const auto& options=outfit->controls_for(selected->second.variant);
            auto custom=selected->second.custom;
            if(action=="palette") custom=choose_palette(options,custom,command.at("palette").get<std::string>());
            else if(action=="template") {
                auto template_id=command.at("template").get<std::string>();
                const Template* tmpl=nullptr;
                for(const auto& t:outfit->templates) if(t.id==template_id) { tmpl=&t; break; }
                if(!tmpl) throw std::runtime_error("Unknown template: "+template_id);
                if(tmpl->data.contains("palette") && tmpl->data.at("palette").is_string()) {
                    custom=choose_palette(options,custom,tmpl->data.at("palette").get<std::string>());
                }
                if(tmpl->data.contains("values") && tmpl->data.at("values").is_object()) {
                    for(const auto& [k, v] : tmpl->data.at("values").items()) {
                        auto* ctrl = options.find(k);
                        if(ctrl) {
                            auto cv = ctrl->value;
                            if(v.is_number()) {
                                cv[0] = v.get<float>();
                            } else if(v.is_array()) {
                                for(size_t i=0; i<std::min(v.size(), size_t(3)); ++i) cv[i] = v[i].get<float>();
                            }
                            custom.values[k] = cv;
                        }
                    }
                }
                if(tmpl->data.contains("tints") && tmpl->data.at("tints").is_object()) {
                    for(const auto& [grp, t] : tmpl->data.at("tints").items()) {
                        if(t.is_object()) {
                            ColorTint tint{};
                            if(t.contains("hue")) tint.hue = t.at("hue").get<float>();
                            if(t.contains("saturation")) tint.saturation = t.at("saturation").get<float>();
                            if(t.contains("brightness")) tint.brightness = t.at("brightness").get<float>();
                            if(!tint.neutral()) custom.tints[grp] = tint;
                            else custom.tints.erase(grp);
                        }
                    }
                }
                report("Applied template: " + tmpl->name);
            }
            else if(action=="tint" || action=="reset_tint") {
                auto group=command.at("group").get<std::string>();
                if(group!="outfit" && group!="body") throw std::runtime_error("Unknown tint group");
                if(custom.palette=="original") throw std::runtime_error("Choose a palette before tinting; Original is not dyed.");
                if(action=="reset_tint") custom.tints.erase(group);
                else {
                    auto tint=custom.tints.contains(group)?custom.tints.at(group):ColorTint{};
                    auto field=command.at("field").get<std::string>();
                    auto* target=field=="hue"?&tint.hue:field=="saturation"?&tint.saturation:field=="brightness"?&tint.brightness:nullptr;
                    if(!target) throw std::runtime_error("Unknown tint field");
                    const float step=field=="hue"?5.f:.05f;
                    const float low=field=="hue"?-180.f:0.f, high=field=="hue"?180.f:2.f;
                    if(command.contains("value")) *target=command.at("value").get<float>();
                    else *target+=command.at("delta").get<float>()*step;
                    *target=std::clamp(*target,low,high);
                    if(tint.neutral()) custom.tints.erase(group); else custom.tints[group]=tint;
                }
            }
            else {
                auto id=command.at("control").get<std::string>();
                auto* control=options.find(id);
                if(!control) throw std::runtime_error("Unknown part");
                if(clearing) custom.values.erase(id);
                else {
                    // Read the value before the group tint, or the tint would be folded
                    // into the override and then applied to it a second time.
                    auto untinted=custom; untinted.tints.clear();
                    auto values=control_values(options,untinted);
                    auto value=values.contains(id)?values.at(id):control->value;
                    if(command.contains("rgb")) {
                        // A whole colour at once, which is what picking a swatch is.
                        const auto& rgb=command.at("rgb");
                        if(control->scalar || !rgb.is_array() || rgb.size()!=3) throw std::runtime_error("Invalid colour value");
                        for(int i=0;i<3;++i)
                            value[i]=std::clamp(rgb[i].get<float>(),control->minimum,control->maximum);
                    } else {
                    int channel=command.value("channel",0);
                    const auto slider=control_channel(*control,channel);
                    if(command.contains("value")) value[channel]=command.at("value").get<float>();
                    else value[channel]=std::clamp(value[channel]+command.at("delta").get<float>()*slider.step,slider.minimum,slider.maximum);
                    }
                    custom.values[id]=value;
                }
            }
            control_values(options,custom);
            pending_custom=std::move(custom); custom_only=true; refresh_custom=command.value("refresh",true);
            apply_pending=true; save_after=GetTickCount64()+600;
        }
        else if (action == "select") {
            const auto outfit = command.at("outfit").get<std::string>();
            const auto variant = command.at("variant").get<std::string>();
            if (!catalog.find(outfit, variant)) throw std::runtime_error("Outfit or variant is not installed");
            selected_outfit = outfit; selected_variant = variant; apply_pending = true;
            pending_custom.reset(); custom_only=false;
        } else if (action != "status") throw std::runtime_error("Unknown CSS command");
    }
    void tick(void* engine, float delta) {
        current_engine=engine;
#ifdef CSS_INVENTORY_DEV
        auto measured=[&](FrameProfile::Phase phase,auto&& call) { frame_profile.measure(phase,call); };
        measured(FrameProfile::recovery,[&] { player_recovery.tick(delta); });
#else
        player_recovery.tick(delta);
#endif
        if(!extension_attempted) {
            extension_attempted=true;
            try {if(fs::exists(root/"cores/cssx_core.dll") || fs::exists(root/"cssx.json")) {extensions.start(root,{CSSX_ABI,sizeof(CssxHost),this,extension_request,static_cast<const CssxHudApi*>(hud_.api())});inventory.extensions(&extensions);}}
            catch(const std::exception& e) {host.log((std::string("CSSX startup: ")+e.what()).c_str());}
        }
        if(extensions.ready()) {
#ifdef CSS_INVENTORY_DEV
            measured(FrameProfile::cssx_tick,[&] { extensions.tick(delta); });
            if(extensions.needs_frame()) {
                CssxFrame frame;
                measured(FrameProfile::hud_prepare,[&] { hud_.update(engine,frame); });
                frame.seconds=delta;
                measured(FrameProfile::cssx_render,[&] { extensions.render(frame); });
            }
#else
            extensions.tick(delta);
            // ABI 2: drive per-frame HUD extensions. The core resolves the HUD and
            // computes the frame inputs; each extension pushes cheap updates back.
            if(extensions.needs_frame()) {
                CssxFrame frame; hud_.update(engine,frame); frame.seconds=delta;
                extensions.render(frame);
            }
#endif
        }
        if(!content_path_checked) {
            content_path_checked=true;
            try {
                const auto content=engine_content_directory();
                auto candidate=wardrobe_packages({},content);
                std::error_code error;
                if(!content.is_absolute() || !fs::is_directory(content/"Paks",error)) throw std::runtime_error("Engine content folder is not accessible; keeping executable lookup");
                auto path=candidate.generic_u8string();
                host.log(("CSS engine outfit folder: "+std::string(path.begin(),path.end())).c_str());
                if(candidate!=package_root) {
                    auto resolved=load_catalog(candidate);
                    package_root=std::move(candidate); catalog=std::move(resolved); ui_refresh=true;
                    original_shells_attempted=false;
                    message=wardrobe_startup_message(catalog.outfits.size());
                }
            } catch(const std::exception& e) { host.log((std::string("CSS content lookup fallback: ")+e.what()).c_str()); }
        }
        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
        bool focused = foreground_pid == GetCurrentProcessId();
        if(inventory_failed && GetTickCount64()>=inventory_retry_after) {
            try { inventory.detach(); inventory_failed=false; }
            catch(...) { inventory_retry_after=GetTickCount64()+2000; }
        }
        if(!inventory_failed) {
            Json inventory_action;
            try {
#ifdef CSS_INVENTORY_DEV
                measured(FrameProfile::inventory,[&] {
                    inventory_action=inventory.poll(engine,catalog,state,appearance,delta,focused);
                    inventory.message(message);
                });
#else
                inventory_action=inventory.poll(engine,catalog,state,appearance,delta,focused);
                inventory.message(message);
#endif
            } catch(const std::exception& error) {
                inventory_failed=true;
                inventory_retry_after=GetTickCount64()+2000;
                try { inventory.detach(); } catch(...) {}
                report(std::string("Inventory CSS unavailable: ")+error.what());
            }
            if(!inventory_action.is_null()) {
                try { request(inventory_action); next_poll=0; }
                catch(const std::exception& error) { report(error.what()); }
            }
        }
        auto now = GetTickCount64();
#ifdef CSS_INVENTORY_DEV
        sample_motion(now);
#endif
        // Cosmetic maintenance (material-reset repair + menu-preview sync) are recovery
        // checks, not per-frame work. Rate-limit them to ~7 Hz: a stock/material reset
        // still corrects within ~150 ms, imperceptibly, while staying off the frame
        // budget. maintenance_after remains the longer error backoff.
        if(now>=maintenance_next) {
            maintenance_next=now+150;
            if(state.enabled && !apply_pending && now>=maintenance_after) {
                try {
                    if(state.selections.contains(appearance.shell) && appearance.repair_materials_needed()) {
                        apply_pending=true;
                        host.log("Restoring cosmetic materials after gameplay material reset");
                    }
                } catch(const std::exception& error) {
                    maintenance_after=now+1000;
                    host.log(error.what());
                }
            }
            if(state.enabled && !apply_pending) sync_menu_safely();
        }
        sync_attachments_safely(now);
        sync_seals_safely(now);
        sync_walk_safely(now);
        if (now < next_poll && !ui_refresh && !apply_pending) return;
        next_poll = now + 250;
        auto command_file = root / "request.json";
        if (fs::exists(command_file)) {
            auto command = read_json(command_file);
            auto id = command.at("id").get<std::string>();
            if (id != last_request) {
                last_request = id;
                request(command);
            }
        }
#ifdef CSS_TRANSITION_TESTS
        if(test_cursor_pending) { appearance.test_cursor(engine,*test_cursor_pending); test_cursor_pending.reset(); }
#endif
        if (rescan_pending) {
            rescan_pending = false;
            auto updated = load_catalog(package_root);
            catalog = std::move(updated);
            original_shells_attempted=false;
            ui_refresh=true;
            report(catalog.outfits.empty()?wardrobe_startup_message(0):"Catalog reloaded.");
        }
        if(!original_shells_attempted && appearance.player(engine)) {
            original_shells_attempted=true;
            try {
                auto originals=discover_original_shells();
                catalog.diagnostics["original_shells"]=originals.variants.size();
                auto& choices=catalog.diagnostics["original_shell_choices"];choices=Json::array();
                for(const auto& variant:originals.variants)
                    choices.push_back({{"id",variant.id},{"name",variant.name},{"mesh",variant.mesh}});
                catalog.outfits.push_back(std::move(originals));
                inventory.refresh();ui_refresh=true;
            } catch(const std::exception& error) {
                catalog.diagnostics["original_shells_error"]=error.what();
                host.log((std::string("Official shell appearances unavailable: ")+error.what()).c_str());
            }
        }
        if (restore_pending) {
            appearance.restore(); recovery.clear(); restore_pending = false; applied_id.clear();
            pending_custom.reset(); custom_only=false; apply_pending=false; selected_outfit.clear(); selected_variant.clear();
            appearance.player(engine);
            if (forget_current && !appearance.shell.empty()) state.selections.erase(appearance.shell);
            forget_current = false;
            dirty = true; ui_refresh = true; report("Original appearance restored.");
        }
        if (state.enabled || apply_pending) {
            appearance.player(engine);
            // Player tags come in two families, each per body: a worn shell
            // (CharacterId.Player.Shell.<Name>) and the bare Harbinger it severs into
            // on death (CharacterId.Player.Darkform.<Name>). Remember the living shell
            // last worn; when "carry into Harbinger" is on, a Darkform reconciles against
            // that shell's saved outfit instead of its own, so a mid-combat sever keeps
            // you dressed without a menu trip. It only mirrors when the shell's outfit is
            // actually compatible with the Harbinger's skeleton, else it falls back to
            // whatever the Darkform itself has (or the game's own look).
            static constexpr const char* SHELL_PREFIX = "CharacterId.Player.Shell";
            static constexpr const char* DARKFORM_PREFIX = "CharacterId.Player.Darkform";
            if(appearance.shell.starts_with(SHELL_PREFIX)) last_living_shell=appearance.shell;
            auto outfit_key=[&](const std::string& tag)->std::string {
                if(state.harbinger_mirror && tag.starts_with(DARKFORM_PREFIX)) {
                    if(!last_living_shell.empty()) {
                        auto it=state.selections.find(last_living_shell);
                        if(it!=state.selections.end() && catalog.compatible(it->second.outfit,tag)) return last_living_shell;
                    } else if(tag.size() > std::string_view(DARKFORM_PREFIX).size()) {
                        std::string inferred = std::string(SHELL_PREFIX) + tag.substr(std::string_view(DARKFORM_PREFIX).size());
                        auto it=state.selections.find(inferred);
                        if(it!=state.selections.end() && catalog.compatible(it->second.outfit,tag)) return inferred;
                    }
                }
                return tag;
            };
            const bool changed=appearance.shell!=last_shell || appearance.pawn_name!=last_pawn || appearance.player_revision!=last_player_revision;
            if(changed) {
                last_shell=appearance.shell; last_pawn=appearance.pawn_name; last_player_revision=appearance.player_revision;
                applied_id.clear();
                status_dirty = true;
            }
            const bool stock_reset=appearance.repair_mesh_needed();
            const std::string reconcile_key=outfit_key(appearance.shell);
            recovery.observe(state.enabled,state.auto_apply,state.selections.contains(reconcile_key),changed,stock_reset);
            if(stock_reset || !appearance.active()) applied_id.clear();
            if(!apply_pending && recovery.due(now) && appearance.ready_to_apply()) {
                apply_pending=true;
                host.log(("Reconciling saved appearance after player/mesh transition: shell "+
                          (appearance.shell.empty()?std::string("<none>"):appearance.shell)+
                          ", saved "+(state.selections.contains(reconcile_key)
                              ? state.selections.at(reconcile_key).outfit+"/"+state.selections.at(reconcile_key).variant
                              : std::string("<nothing for this shell>"))).c_str());
            }
            if (apply_pending && !appearance.shell.empty()) {
                apply_pending = false;
                // The shell this reconcile is for. Applying loads a mesh, and that takes
                // long enough for a shell switch to finish underneath it, so everything
                // below is resolved and recorded against this one rather than against
                // whatever the player happens to be wearing by the time it lands.
                const auto shell = appearance.shell;
                Selection requested;
                if (!selected_outfit.empty()) {
                    requested = {std::move(selected_outfit), std::move(selected_variant), {}};
                    if(state.remembered_custom.contains(requested.outfit)) requested.custom=state.remembered_custom.at(requested.outfit);
                    for(const auto& outfit:catalog.outfits) if(outfit.id==requested.outfit)
                        requested.custom=compatible_values(outfit.controls_for(requested.variant),requested.custom);
                    selected_outfit.clear(); selected_variant.clear();
                    if (!catalog.compatible(requested.outfit, shell))
                        throw std::runtime_error("This outfit does not support the current shell: " + shell);
                } else if (auto selected = state.selections.find(outfit_key(shell)); selected != state.selections.end()) {
                    requested = selected->second;
                    // The same filter the wear path uses. A saved look can name a part the
                    // installed package no longer has, and 0.4 makes that likelier because
                    // a variant can carry its own recipe. Dropping the part it cannot place
                    // leaves you dressed; letting it through leaves you in nothing.
                    for(const auto& outfit:catalog.outfits) if(outfit.id==requested.outfit)
                        requested.custom=compatible_values(outfit.controls_for(requested.variant),requested.custom);
                } else {
                    host.log(("No saved appearance for shell "+shell+"; leaving it alone").c_str());
                }
                if (!requested.outfit.empty()) {
                    if(pending_custom) { requested.custom=*pending_custom; pending_custom.reset(); }
                    auto* variant = catalog.find(requested.outfit, requested.variant);
                    if (!variant || !catalog.compatible(requested.outfit, shell))
                        throw std::runtime_error("Saved outfit is missing or incompatible");
                    auto start = std::chrono::steady_clock::now();
                    appearance.set_attachment_offsets(variant->attachments);
                    if (appearance.apply(engine, variant->mesh, variant->materials)) {
                        try {
                            appearance.set_ground_offset(variant->ground_offset_cm);
                            for(const auto& outfit:catalog.outfits) if(outfit.id==requested.outfit) {
                                // Items before controls: an accessory can hide body
                                // sections, and a toggle may then show one of them again.
                                appearance.sync_items(outfit,requested.variant);
                                appearance.customize(outfit,requested.variant,requested.custom);
                            }
                        } catch(...) {
                            applied_id.clear(); appearance.restore(); throw;
                        }
                        if (appearance.shell != shell) {
                            // A different shell arrived while this was loading. This outfit
                            // was chosen for the old one and says nothing about the new one,
                            // so it is not recorded against it. Forgetting the last shell
                            // makes the next tick treat this as a fresh transition and
                            // reconcile the new shell properly.
                            applied_id.clear(); last_shell.clear();
                            host.log(("Shell became " + appearance.shell + " while applying for " + shell +
                                      "; not saving it, reconciling again").c_str());
                        } else {
                            sync_menu_safely();
                            attachments_after=0;sync_attachments_safely(now);
                            recovery.clear();
                            last_apply_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                            state.selections[shell] = requested;
                            if(state.harbinger_mirror && shell.starts_with(DARKFORM_PREFIX) && !last_living_shell.empty()) {
                                state.selections[last_living_shell] = requested;
                            }
                            state.remembered_custom[requested.outfit]=requested.custom;
                            state.enabled = true; dirty = true; ui_refresh = !custom_only || refresh_custom;
                            applied_id = requested.outfit + "/" + requested.variant;
                            host.log(("Appearance verified: " + applied_id).c_str());
                            report(custom_only?"Settings updated.":"Wearing " + variant->name + ".");
                            custom_only=false;
                        }
                    } else { recovery.failed(now); report("No playable character mesh is ready."); }
                }
            }
        }
        if(ui_refresh) inventory.refresh();
        ui_refresh = false;
        if (dirty && now>=save_after) save();
        if(transition_inspect_pending) {
            auto result=appearance.transition_state(engine); result["id"]=last_request;
            result["inventory"]=inventory.diagnostics(); result["recovery_pending"]=recovery.pending();
            atomic_json(root/"runtime/transition.json",result,false); transition_inspect_pending=false;
        }
        if(inspect_pending) {
            auto result=appearance.transition_state(engine); result["id"]=last_request; result["inventory"]=inventory.diagnostics();
            atomic_json(root / "runtime/inspection.json",result,false); inspect_pending=false;
        }
        if(!inventory_command.is_null()) {
            auto pending=std::exchange(inventory_command,Json{});
            auto result=inventory.command(engine,pending); result["id"]=last_request;
            atomic_json(root/"runtime/inventory.json",result,false);
        }
#ifdef CSS_INVENTORY_DEV
        if(!probe_command.is_null()) {
            auto pending=std::exchange(probe_command,Json{});Json result={{"id",pending.at("id")}};
            try {result["result"]=extension_bridge.request(current_engine,appearance,pending.at("request"));result["ok"]=true;}
            catch(const std::exception& error){result["ok"]=false;result["error"]=error.what();}
            atomic_json(root/"runtime/css-probe.json",result,false);
        }
        if(!extension_command.is_null()) {
            auto pending=std::exchange(extension_command,Json{});Json result={{"id",pending.at("id")}};
            try {
                const auto& value=pending.at("request");
                result["result"]=pending.value("host",false)?cssx::Client(&recovery_host).request(value):extensions.request(value);
                result["ok"]=true;
            } catch(const std::exception& error){result["ok"]=false;result["error"]=error.what();}
            atomic_json(root/"runtime/cssx-debug.json",result,false);
        }
#endif
        if(status_dirty || (now - last_publish_ms >= 500)) {
            status_dirty = false;
            last_publish_ms = now;
            publish();
        }
    }
    void publish() {
        Json status = {{"schema", 1}, {"enabled", state.enabled},
                    {"shell", appearance.shell}, {"pawn", appearance.pawn_name}, {"mesh", appearance.current_mesh},
                    {"applied", applied_id}, {"message", message}, {"last_request", last_request},
                    {"apply_ms", last_apply_ms}, {"pid", GetCurrentProcessId()}};
        status["worn_items"]=appearance.worn_item_count();
        status["recovery_pending"]=recovery.pending();
        status["maintenance_error"]=maintenance_error;
        auto path=package_root.generic_u8string();
        status["catalog"]=catalog.diagnostics;
        status["catalog"]["folder"]=std::string(path.begin(),path.end());
        status["catalog"]["outfits"]=catalog.outfits.size();
        status["inventory"] = inventory.diagnostics();
        status["inventory_failed"] = inventory_failed;
        status["material_debug"] = appearance.material_debug;
        status["walk_mod_active"] = appearance.walk.walk_mod_active();
        status["walk_mod_name"] = appearance.walk.walk_mod_name();
        if(auto selected=state.selections.find(appearance.shell);selected!=state.selections.end()) status["customize"]=selected->second.custom.json();
        auto serialized = status.dump();
        if (serialized != last_status) {
            atomic_json(root / "runtime/status.json", status, false);
            last_status = std::move(serialized);
        }
    }
    void render() {
        auto* context = RC::UE4SSProgram::get_current_imgui_context();
        if (!context) return;
        ImGui::SetCurrentContext(context);
        ImGuiMemAllocFunc alloc{}; ImGuiMemFreeFunc free{}; void* user{};
        RC::UE4SSProgram::get_current_imgui_allocator_functions(&alloc, &free, &user);
        ImGui::SetAllocatorFunctions(alloc, free, user);
        ImGui::TextUnformatted("CSS / Custom Shell System");
        ImGui::TextUnformatted("Open Inventory and select the CSS tab.");
        ImGui::TextWrapped("%s", message.c_str());
        ImGui::Text("Outfits: %zu",catalog.outfits.size());
        ImGui::Text("Last appearance change: %.2f ms",last_apply_ms);

    }
};
}
namespace {
void* create(const CssHost* host) noexcept {
    if (!host || host->abi != css_abi) return nullptr;
    try { return new css::Core(*host); }
    catch (const std::exception& error) { host->log(error.what()); return nullptr; }
}
void tick(void* ptr, void* engine, float delta) noexcept {
    auto& core = *static_cast<css::Core*>(ptr);
#ifdef CSS_INVENTORY_DEV
    if(core.frame_profile.active()) core.frame_profile.begin(delta);
    bool failed=false;
#endif
    try { core.tick(engine, delta); }
    catch (const std::exception& error) {
#ifdef CSS_INVENTORY_DEV
        failed=true;
#endif
        core.recovery.failed(GetTickCount64());
        core.apply_pending = false; core.pending_custom.reset(); core.custom_only=false;
        core.selected_outfit.clear(); core.selected_variant.clear();
        core.report(error.what());
        try { core.publish(); } catch (...) {}
    }
#ifdef CSS_INVENTORY_DEV
    if(core.frame_profile.active() && core.frame_profile.end(failed))
        try { core.publish_frame_profile(); }
        catch(const std::exception& error) { core.host.log(error.what()); }
#endif
}
void render(void* ptr) noexcept {
    auto& core = *static_cast<css::Core*>(ptr);
    try { core.render(); } catch (const std::exception& error) { core.report(error.what()); }
}
bool stop(void* ptr) noexcept {
    auto& core = *static_cast<css::Core*>(ptr);
    try {
        if(!core.extensions.stop()) {core.report("CSSX cleanup pending; reload deferred.");return false;}
        if(!core.extension_bridge.stop_hooks()) {core.report("CSSX hook cleanup pending; reload deferred.");return false;}
        core.hud_.release();
        css::minimap_destroy();
        core.inventory.detach();
        core.appearance.walk.release();
        core.appearance.restore(); if (core.dirty) core.save();
        core.last_pawn.clear(); core.apply_pending = core.state.enabled;
        return true;
    }
    catch (const std::exception& error) { core.report(error.what()); return false; }
}
void destroy(void* ptr) noexcept { delete static_cast<css::Core*>(ptr); }
const CssCore api{css_abi, create, tick, render, stop, destroy};
}
extern "C" __declspec(dllexport) const CssCore* css_get_api() noexcept { return &api; }

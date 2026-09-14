#include "api.hpp"
#include "data.hpp"
#include "engine.hpp"
#include "recovery.hpp"
#include "startup.hpp"
#include <windows.h>
#include <chrono>
#include <optional>
#include <imgui.h>
#include <UE4SSProgram.hpp>
#include <USMapGenerator/Generator.hpp>

namespace css {
struct Core {
    CssHost host;
    fs::path root;
    fs::path package_root;
    Catalog catalog;
    State state;
    Appearance appearance;
    InventoryUI inventory;
    ExtensionBridge extension_bridge;
    ExtensionClient extensions;
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
            else if(core.current_engine) result=core.extension_bridge.request(core.current_engine,core.appearance,request);
            else throw std::runtime_error("Game thread is not initialized");
            auto data=result.dump();sink(output,data.data(),data.size());return 1;
        } catch(const std::exception& error) {auto data=Json{{"error",error.what()}}.dump();sink(output,data.data(),data.size());return 0;}
    }
    Json inventory_command;
#ifdef CSS_INVENTORY_DEV
    Json extension_command;
#endif
    bool inventory_failed=false;
    bool content_path_checked=false;
    uint64_t inventory_retry_after=0;
    std::string message;
    std::string last_request, last_shell, last_pawn, applied_id, last_status;
    std::string selected_outfit, selected_variant;
    bool dirty = false, apply_pending = false, restore_pending = false, rescan_pending = false, forget_current = false;
    bool favorites_only = false;
    bool ui_refresh = false;
    bool inspect_pending = false, transition_inspect_pending = false;
    uint64_t next_poll = 0;
    char search[128]{}, preset_name[96]{};
    double last_apply_ms = 0;
    std::optional<Customization> pending_colors;
    bool color_only = false, refresh_colors = true;
    uint64_t save_after = 0, last_player_revision = 0, maintenance_after = 0;
    Recovery recovery;
#ifdef CSS_TRANSITION_TESTS
    std::optional<bool> test_cursor_pending;
#endif
    std::string maintenance_error;
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
    }
    void save() { atomic_json(root / "state/state.json", state.json()); dirty = false; }
    void request(const Json& command) {
        const auto action = command.at("action").get<std::string>();
#ifdef CSS_INVENTORY_DEV
        if (action.starts_with("inventory_")) { inventory_command=command; return; }
        if (action=="cssx_debug") {extension_command=command;return;}
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
        else if(action=="test_effect") { appearance.test_effect(command.at("begin").get<bool>()); }
        else if(action=="test_reset_mesh") { appearance.test_reset_mesh(); }
#endif
        else if (action == "favorite") {
            auto id = command.at("outfit").get<std::string>();
            if (state.favorites.contains(id)) state.favorites.erase(id); else state.favorites.insert(id);
            dirty = true; ui_refresh = true;
        }
        else if (action == "save_look") {
            auto name = command.at("name").get<std::string>();
            if (!valid_id(name) || (state.presets.size() >= 64 && !state.presets.contains(name))) throw std::runtime_error("Invalid look slot");
            state.presets[name] = state.selections; dirty = true; ui_refresh = true; report("Current appearance saved to " + name);
        }
        else if (action == "load_look") {
            auto name = command.at("name").get<std::string>();
            const auto& selections = state.presets.at(name);
            auto selected = selections.find(appearance.shell);
            if (selected == selections.end()) { restore_pending = true; forget_current = true; }
            else { selected_outfit = selected->second.outfit; selected_variant = selected->second.variant; pending_colors=selected->second.colors; apply_pending = true; }
            ui_refresh = true;
        }
        else if(action=="delete_look") {
            state.presets.erase(command.at("name").get<std::string>());
            dirty=true; ui_refresh=true; report("Template deleted.");
        }
        else if(action=="rename_look") {
            auto before=command.at("name").get<std::string>(), after=command.at("new_name").get<std::string>();
            if(!valid_id(after)) throw std::runtime_error("Use letters, numbers, periods, underscores or hyphens in the template name.");
            if(before!=after) {
                if(state.presets.contains(after)) throw std::runtime_error("A template with that name already exists.");
                auto copy=state.presets.at(before); state.presets.emplace(after,std::move(copy)); state.presets.erase(before);
                dirty=true; ui_refresh=true; report("Template renamed.");
            }
        }
        else if (action == "enable") { state.enabled = true; apply_pending = true; dirty = true; report("CSS enabled. Open Inventory and choose CSS."); }
        else if (action == "disable") { state.enabled = false; restore_pending = true; dirty = true; }
        else if (action == "restore") { restore_pending = true; forget_current = true; }
        else if (action == "rescan") { rescan_pending = true; }
        else if(action=="palette" || action=="color" || action=="reset_color") {
            auto selected=state.selections.find(appearance.shell);
            if(selected==state.selections.end()) throw std::runtime_error("Wear an appearance before changing its colors");
            const Outfit* outfit=nullptr;
            for(const auto& o:catalog.outfits) if(o.id==selected->second.outfit) outfit=&o;
            if(!outfit) throw std::runtime_error("The selected outfit is missing");
            const auto& options=outfit->colors_for(selected->second.variant);
            auto custom=selected->second.colors;
            if(action=="palette") { custom.palette=command.at("palette"); custom.values.clear(); }
            else {
                auto id=command.at("control").get<std::string>();
                auto* control=options.find(id);
                if(!control) throw std::runtime_error("Unknown color part");
                if(action=="reset_color") custom.values.erase(id);
                else {
                    auto values=color_values(options,custom);
                    auto value=values.contains(id)?values.at(id):control->value;
                    int channel=command.value("channel",0);
                    if(channel<0 || channel>(control->scalar?0:2)) throw std::runtime_error("Invalid color channel");
                    if(command.contains("value")) value[channel]=command.at("value").get<float>();
                    else value[channel]=std::clamp(value[channel]+command.at("delta").get<float>()*control->step,control->minimum,control->maximum);
                    custom.values[id]=value;
                }
            }
            color_values(options,custom);
            pending_colors=std::move(custom); color_only=true; refresh_colors=command.value("refresh",true);
            apply_pending=true; save_after=GetTickCount64()+600;
        }
        else if (action == "select") {
            const auto outfit = command.at("outfit").get<std::string>();
            const auto variant = command.at("variant").get<std::string>();
            if (!catalog.find(outfit, variant)) throw std::runtime_error("Outfit or variant is not installed");
            selected_outfit = outfit; selected_variant = variant; apply_pending = true;
            pending_colors.reset(); color_only=false;
        } else if (action != "status") throw std::runtime_error("Unknown CSS command");
    }
    void tick(void* engine, float delta) {
        current_engine=engine;
        if(!extension_attempted) {
            extension_attempted=true;
            try {if(fs::exists(root/"cores/cssx_core.dll") || fs::exists(root/"cssx.json")) {extensions.start(root,{CSSX_ABI,sizeof(CssxHost),this,extension_request});inventory.extensions(&extensions);}}
            catch(const std::exception& e) {host.log((std::string("CSSX startup: ")+e.what()).c_str());}
        }
        if(extensions.ready()) extensions.tick(delta);
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
                inventory_action=inventory.poll(engine,catalog,state,appearance,delta,focused);
                inventory.message(message);
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
            ui_refresh=true;
            report(catalog.outfits.empty()?wardrobe_startup_message(0):"Catalog reloaded.");
        }
        if (restore_pending) {
            appearance.restore(); recovery.clear(); restore_pending = false; applied_id.clear();
            pending_colors.reset(); color_only=false; apply_pending=false; selected_outfit.clear(); selected_variant.clear();
            appearance.player(engine);
            if (forget_current && !appearance.shell.empty()) state.selections.erase(appearance.shell);
            forget_current = false;
            dirty = true; ui_refresh = true; report("Original appearance restored.");
        }
        if (state.enabled || apply_pending) {
            appearance.player(engine);
            const bool changed=appearance.shell!=last_shell || appearance.pawn_name!=last_pawn || appearance.player_revision!=last_player_revision;
            if(changed) {
                last_shell=appearance.shell; last_pawn=appearance.pawn_name; last_player_revision=appearance.player_revision;
                applied_id.clear();
            }
            const bool stock_reset=appearance.repair_mesh_needed();
            recovery.observe(state.enabled,state.auto_apply,state.selections.contains(appearance.shell),changed,stock_reset);
            if(stock_reset || !appearance.active()) applied_id.clear();
            if(!apply_pending && recovery.due(now) && appearance.ready_to_apply()) {
                apply_pending=true;
                host.log("Reconciling saved appearance after player/mesh transition");
            }
            if (apply_pending && !appearance.shell.empty()) {
                apply_pending = false;
                Selection requested;
                if (!selected_outfit.empty()) {
                    requested = {std::move(selected_outfit), std::move(selected_variant), {}};
                    if(state.remembered_colors.contains(requested.outfit)) requested.colors=state.remembered_colors.at(requested.outfit);
                    for(const auto& outfit:catalog.outfits) if(outfit.id==requested.outfit)
                        requested.colors=compatible_colors(outfit.colors_for(requested.variant),requested.colors);
                    selected_outfit.clear(); selected_variant.clear();
                    if (!catalog.compatible(requested.outfit, appearance.shell))
                        throw std::runtime_error("This outfit does not support the current shell: " + appearance.shell);
                } else if (auto selected = state.selections.find(appearance.shell); selected != state.selections.end()) {
                    requested = selected->second;
                }
                if (!requested.outfit.empty()) {
                    if(pending_colors) { requested.colors=*pending_colors; pending_colors.reset(); }
                    auto* variant = catalog.find(requested.outfit, requested.variant);
                    if (!variant || !catalog.compatible(requested.outfit, appearance.shell))
                        throw std::runtime_error("Saved outfit is missing or incompatible");
                    auto start = std::chrono::steady_clock::now();
                    if (appearance.apply(engine, variant->mesh, variant->materials)) {
                        try {
                            for(const auto& outfit:catalog.outfits) if(outfit.id==requested.outfit) appearance.customize(outfit,requested.variant,requested.colors);
                        } catch(...) {
                            applied_id.clear(); appearance.restore(); throw;
                        }
                        sync_menu_safely();
                        recovery.clear();
                        last_apply_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                        state.selections[appearance.shell] = requested;
                        state.remembered_colors[requested.outfit]=requested.colors;
                        state.enabled = true; dirty = true; ui_refresh = !color_only || refresh_colors;
                        applied_id = requested.outfit + "/" + requested.variant;
                        host.log(("Appearance verified: " + applied_id).c_str());
                        report(color_only?"Colors updated.":"Wearing " + variant->name + ".");
                        color_only=false;
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
        if(!extension_command.is_null()) {
            auto pending=std::exchange(extension_command,Json{});Json result={{"id",pending.at("id")}};
            try {
                const auto& value=pending.at("request");
                result["result"]=pending.value("host",false)?extension_bridge.request(engine,appearance,value):extensions.request(value);
                result["ok"]=true;
            } catch(const std::exception& error){result["ok"]=false;result["error"]=error.what();}
            atomic_json(root/"runtime/cssx-debug.json",result,false);
        }
#endif
        publish();
    }
    void publish() {
        Json status = {{"schema", 1}, {"enabled", state.enabled},
                    {"shell", appearance.shell}, {"pawn", appearance.pawn_name}, {"mesh", appearance.current_mesh},
                    {"applied", applied_id}, {"message", message}, {"last_request", last_request},
                    {"apply_ms", last_apply_ms}, {"pid", GetCurrentProcessId()}};
        status["recovery_pending"]=recovery.pending();
        status["maintenance_error"]=maintenance_error;
        auto path=package_root.generic_u8string();
        status["catalog"]=catalog.diagnostics;
        status["catalog"]["folder"]=std::string(path.begin(),path.end());
        status["catalog"]["outfits"]=catalog.outfits.size();
        status["inventory"] = inventory.diagnostics();
        status["inventory_failed"] = inventory_failed;
        status["material_debug"] = appearance.material_debug;
        if(auto selected=state.selections.find(appearance.shell);selected!=state.selections.end()) status["colors"]=selected->second.colors.json();
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
    try { core.tick(engine, delta); }
    catch (const std::exception& error) {
        core.recovery.failed(GetTickCount64());
        core.apply_pending = false; core.pending_colors.reset(); core.color_only=false;
        core.selected_outfit.clear(); core.selected_variant.clear();
        core.report(error.what());
        try { core.publish(); } catch (...) {}
    }
}
void render(void* ptr) noexcept {
    auto& core = *static_cast<css::Core*>(ptr);
    try { core.render(); } catch (const std::exception& error) { core.report(error.what()); }
}
bool stop(void* ptr) noexcept {
    auto& core = *static_cast<css::Core*>(ptr);
    try {
        if(!core.extensions.stop()) {core.report("CSSX cleanup pending; reload deferred.");return false;}
        core.inventory.detach();
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

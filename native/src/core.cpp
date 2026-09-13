#include "api.hpp"
#include "data.hpp"
#include "engine.hpp"
#include <windows.h>
#include <chrono>
#include <imgui.h>
#include <UE4SSProgram.hpp>
#include <USMapGenerator/Generator.hpp>

namespace css {
struct Core {
    CssHost host;
    fs::path root;
    Catalog catalog;
    State state;
    Appearance appearance;
    Wardrobe wardrobe;
    std::string message = "CSS is off. Enable it when a save is loaded.";
    std::string last_request, last_shell, last_pawn, applied_id, last_status;
    std::string selected_outfit, selected_variant, last_wardrobe_message;
    bool dirty = false, apply_pending = false, restore_pending = false, rescan_pending = false, forget_current = false;
    bool favorites_only = false;
    bool wardrobe_pending = false, wardrobe_close = false, wardrobe_refresh = false;
    bool n_down = false, escape_down = false;
    bool inspect_pending = false;
    uint64_t next_poll = 0;
    char search[128]{}, preset_name[96]{};
    double last_apply_ms = 0;

    Catalog load_catalog() const {
        return Catalog::load(root/"catalog",(root/"../../../../../Content/Paks/~mods").lexically_normal(),root/"cache/packages");
    }
    explicit Core(const CssHost& h) : host(h), root(h.root), catalog(load_catalog()) {
        auto file = root / "state/state.json";
        if (fs::exists(file)) {
            try { auto saved=read_json(file); state = State::parse(saved); dirty=state.json()!=saved; }
            catch (const std::exception&) {
                auto backup = file; backup += ".bak";
                // Preserve corrupt bytes before a later successful save.
                fs::copy_file(file, file.string() + ".corrupt-" + std::to_string(GetTickCount64()));
                if (!fs::exists(backup)) throw;
                state = State::parse(read_json(backup));
                message = "Recovered CSS state from its backup.";
            }
        }
        if (fs::exists(root / "request.json")) {
            try { last_request = read_json(root / "request.json").at("id").get<std::string>(); }
            catch (...) {} // Ignore stale malformed requests on a fresh core.
        }
        if (state.enabled) message = "Your saved appearance is ready. Press N for the wardrobe.";
    }
    void report(std::string text) {
        if (message == text) return;
        message = std::move(text); host.log(message.c_str());
    }
    void save() { atomic_json(root / "state/state.json", state.json()); dirty = false; }
    void request(const Json& command) {
        const auto action = command.at("action").get<std::string>();
        if (action == "export_mappings") {
            RC::OutTheShade::generate_usmap(); report("Exported runtime mappings for asset tooling.");
        }
        else if (action == "inspect") {
            inspect_pending=true;
        }
        else if (action == "open") wardrobe_pending = true;
        else if (action == "close") wardrobe_close = true;
        else if (action == "front") wardrobe.rotate(0,true);
        else if (action == "rotate") wardrobe.rotate(command.at("degrees").get<double>());
        else if (action == "filter") { wardrobe.filter(command.at("category").get<int>()); wardrobe_refresh = true; }
        else if (action == "page") { wardrobe.page(command.at("delta").get<int>()); wardrobe_refresh = true; }
        else if (action == "favorite") {
            auto id = command.at("outfit").get<std::string>();
            if (state.favorites.contains(id)) state.favorites.erase(id); else state.favorites.insert(id);
            dirty = true; wardrobe_refresh = true;
        }
        else if (action == "save_look") {
            auto name = command.at("name").get<std::string>();
            if (!valid_id(name) || (state.presets.size() >= 64 && !state.presets.contains(name))) throw std::runtime_error("Invalid look slot");
            state.presets[name] = state.selections; dirty = true; wardrobe_refresh = true; report("Current appearance saved to " + name);
        }
        else if (action == "load_look") {
            auto name = command.at("name").get<std::string>();
            const auto& selections = state.presets.at(name);
            auto selected = selections.find(appearance.shell);
            if (selected == selections.end()) { restore_pending = true; forget_current = true; }
            else { selected_outfit = selected->second.outfit; selected_variant = selected->second.variant; apply_pending = true; }
            wardrobe_refresh = true;
        }
        else if (action == "enable") { state.enabled = true; apply_pending = true; dirty = true; report("CSS enabled. Press N to open your wardrobe."); }
        else if (action == "disable") { state.enabled = false; restore_pending = true; dirty = true; wardrobe_close = true; }
        else if (action == "restore") { restore_pending = true; forget_current = true; }
        else if (action == "rescan") { rescan_pending = true; }
        else if (action == "select") {
            const auto outfit = command.at("outfit").get<std::string>();
            const auto variant = command.at("variant").get<std::string>();
            if (!catalog.find(outfit, variant)) throw std::runtime_error("Outfit or variant is not installed");
            selected_outfit = outfit; selected_variant = variant; apply_pending = true;
        } else if (action != "status") throw std::runtime_error("Unknown CSS command");
    }
    void tick(void* engine, float delta) {
        wardrobe.configure(state.invert_orbit_x,state.invert_orbit_y);
        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
        bool focused = foreground_pid == GetCurrentProcessId();
        bool n = focused && (GetAsyncKeyState('N') & 0x8000) != 0;
        bool escape = focused && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (n && !n_down && !(GetAsyncKeyState(VK_CONTROL) & 0x8000) && !(GetAsyncKeyState(VK_MENU) & 0x8000)) {
            if (wardrobe.opened()) wardrobe_close = true; else wardrobe_pending = true;
        }
        if (escape && !escape_down && wardrobe.opened()) wardrobe_close = true;
        n_down = n; escape_down = escape;
        if (wardrobe_close) { wardrobe.close(); wardrobe_close = false; wardrobe_pending = false; }
        auto action = wardrobe.poll(delta, focused);
        if (!action.is_null()) { request(action); next_poll = 0; }
        auto now = GetTickCount64();
        if (now < next_poll && !wardrobe_pending && !wardrobe_refresh) return;
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
        if (rescan_pending) {
            rescan_pending = false;
            auto updated = load_catalog();
            catalog = std::move(updated);
            report("Catalog reloaded.");
        }
        if (restore_pending) {
            appearance.restore(); restore_pending = false; applied_id.clear();
            appearance.player(engine);
            if (forget_current && !appearance.shell.empty()) state.selections.erase(appearance.shell);
            forget_current = false;
            dirty = true; wardrobe_refresh = true; report("Original appearance restored.");
        }
        if (state.enabled || apply_pending) {
            appearance.player(engine);
            if (appearance.shell != last_shell || appearance.pawn_name != last_pawn) {
                last_shell = appearance.shell; last_pawn = appearance.pawn_name; applied_id.clear();
                if (state.enabled && state.auto_apply && state.selections.contains(last_shell)) apply_pending = true;
            }
            if (apply_pending && !appearance.shell.empty()) {
                apply_pending = false;
                Selection requested;
                if (!selected_outfit.empty()) {
                    requested = {std::move(selected_outfit), std::move(selected_variant)};
                    selected_outfit.clear(); selected_variant.clear();
                    if (!catalog.compatible(requested.outfit, appearance.shell))
                        throw std::runtime_error("This outfit does not support the current shell: " + appearance.shell);
                } else if (auto selected = state.selections.find(appearance.shell); selected != state.selections.end()) {
                    requested = selected->second;
                }
                if (!requested.outfit.empty()) {
                    auto* variant = catalog.find(requested.outfit, requested.variant);
                    if (!variant || !catalog.compatible(requested.outfit, appearance.shell))
                        throw std::runtime_error("Saved outfit is missing or incompatible");
                    auto start = std::chrono::steady_clock::now();
                    if (appearance.apply(engine, variant->mesh, variant->materials)) {
                        last_apply_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                        state.selections[appearance.shell] = requested;
                        state.enabled = true; dirty = true; wardrobe_refresh = true;
                        applied_id = requested.outfit + "/" + requested.variant;
                        host.log(("Appearance verified: " + applied_id).c_str());
                        report("Wearing " + variant->name + ".");
                    } else report("No playable character mesh is ready.");
                }
            }
        }
        if (wardrobe_close) { wardrobe.close(); wardrobe_close = false; }
        if (wardrobe_pending || (wardrobe_refresh && wardrobe.opened())) {
            wardrobe_pending = false; wardrobe_refresh = false;
            wardrobe.open(engine, catalog, state, appearance, message, root / "assets");
        }
        wardrobe_refresh = false;
        if (wardrobe.opened() && last_wardrobe_message != message) {
            wardrobe.message(message); last_wardrobe_message = message;
        }
        if (dirty) save();
        if(inspect_pending) {
            auto result=wardrobe.inspect(appearance.player(engine)); result["id"]=last_request;
            atomic_json(root / "runtime/inspection.json",result,false); inspect_pending=false;
        }
        publish();
    }
    void publish() {
        Json status = {{"schema", 1}, {"enabled", state.enabled},
                    {"shell", appearance.shell}, {"pawn", appearance.pawn_name}, {"mesh", appearance.current_mesh},
                    {"applied", applied_id}, {"message", message}, {"last_request", last_request},
                    {"apply_ms", last_apply_ms}, {"pid", GetCurrentProcessId()}};
        status["wardrobe_open"] = wardrobe.opened();
        status["wardrobe"] = wardrobe.diagnostics();
        status["material_debug"] = appearance.material_debug;
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
        if (ImGui::Button("Open in-game wardrobe (N)")) wardrobe_pending = true;
        ImGui::TextWrapped("%s", message.c_str());
        if (ImGui::Checkbox("Enable CSS", &state.enabled)) {
            dirty = true;
            if (state.enabled) { apply_pending = true; report("CSS enabled. Load a save and choose an outfit."); }
            else restore_pending = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore original")) { restore_pending = true; forget_current = true; }
        ImGui::SameLine();
        if (ImGui::Button("Reload catalog")) rescan_pending = true;
        if (ImGui::Checkbox("Restore my selection after loading or changing shells", &state.auto_apply)) dirty = true;
        if (ImGui::Checkbox("Invert vertical wardrobe orbit", &state.invert_orbit_y)) dirty = true;
        if (ImGui::Checkbox("Invert horizontal wardrobe orbit", &state.invert_orbit_x)) dirty = true;
        ImGui::Separator();
        ImGui::Text("Current shell: %s", appearance.shell.empty() ? "Enable CSS in a loaded save" : appearance.shell.c_str());
        ImGui::InputText("Search", search, sizeof(search));
        ImGui::SameLine(); ImGui::Checkbox("Favorites only", &favorites_only);
        for (const auto& outfit : catalog.outfits) {
            bool favorite = state.favorites.contains(outfit.id);
            if (favorites_only && !favorite) continue;
            if (*search && outfit.name.find(search) == std::string::npos && outfit.author.find(search) == std::string::npos) continue;
            ImGui::PushID(outfit.id.c_str());
            if (ImGui::CollapsingHeader(outfit.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("%s", outfit.description.c_str());
                ImGui::Text("By %s / %s", outfit.author.c_str(), outfit.category.c_str());
                if (ImGui::Checkbox("Favorite", &favorite)) {
                    if (favorite) state.favorites.insert(outfit.id); else state.favorites.erase(outfit.id);
                    dirty = true;
                }
                bool compatible = catalog.compatible(outfit.id, appearance.shell);
                if (!compatible) ImGui::TextUnformatted("Switch to a supported shell in the game to wear this outfit.");
                for (const auto& variant : outfit.variants) {
                    ImGui::BeginDisabled(!compatible);
                    if (ImGui::Button(variant.name.c_str())) {
                        selected_outfit = outfit.id; selected_variant = variant.id; apply_pending = true;
                    }
                    ImGui::EndDisabled();
                }
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::InputText("Preset name", preset_name, sizeof(preset_name));
        ImGui::SameLine();
        if (ImGui::Button("Save preset")) {
            if (valid_id(preset_name) && state.presets.size() < 64) { state.presets[preset_name] = state.selections; dirty = true; }
            else report("Use a preset name containing letters, digits, periods, underscores or hyphens.");
        }
        for (const auto& [name, selections] : state.presets) {
            ImGui::PushID(name.c_str());
            if (ImGui::Button(("Load " + name).c_str())) { state.selections = selections; state.enabled = true; apply_pending = true; dirty = true; }
            ImGui::PopID();
        }
        ImGui::Text("Last appearance change: %.2f ms", last_apply_ms);
        ImGui::TextWrapped("Cosmetic selection does not unlock weapons, seals or shells. Your existing cheat menu can unlock gameplay equipment separately.");
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
        try { core.wardrobe.close(); } catch (...) {}
        core.apply_pending = false; core.report(error.what());
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
        core.wardrobe.close();
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

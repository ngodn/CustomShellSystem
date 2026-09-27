#include "ccs_abi.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <memory>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/GameplayStatics.hpp>
#include <nlohmann/json.hpp>

namespace {

std::filesystem::path g_log_path;
std::mutex g_log_mutex;

void log_line(const char* msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    try {
        std::ofstream out(g_log_path, std::ios::app);
        if (out.is_open()) {
            out << msg << "\n";
        }
    } catch (...) {}
}

class CcsLoader final : public RC::CppUserModBase {
public:
    CcsLoader() {
        ModName = STR("CustomCombatSystem");
        ModVersion = STR("1.0.0");
        ModDescription = STR("Custom Combat System (CCS) for Mortal Shell II");
        ModAuthors = STR("eins0fx");

        // Resolve paths
        wchar_t module_path[MAX_PATH]{};
        HMODULE hmod = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&log_line), &hmod);
        GetModuleFileNameW(hmod, module_path, MAX_PATH);

        root_ = std::filesystem::path(module_path).parent_path().parent_path();
        g_log_path = root_ / "CCS.log";

        log_line("CCS Loader initializing...");

        // Setup loader context
        host_.mod_root = root_.c_str();
        host_.log_info = [](const char* m) { log_line(m); };
        host_.log_warn = [](const char* m) { log_line(m); };
        host_.log_error = [](const char* m) { log_line(m); };
        host_.register_hook = nullptr;

        // Load core DLL
        load_core();
    }

    void on_unreal_init() override {
        RC::Unreal::Hook::RegisterEngineTickPostCallback([this](RC::Unreal::UEngine* engine, float delta) {
            on_engine_tick(engine, delta);
        });
        log_line("CCS Loader registered engine tick callback.");
    }

    ~CcsLoader() override {
        if (api_ && api_->shutdown) {
            api_->shutdown();
        }
        if (core_module_) {
            FreeLibrary(core_module_);
            core_module_ = nullptr;
        }
    }

private:
    void load_core() {
        std::string core_name = "ccs_core.dll";

        // Try reading core.json if present
        std::filesystem::path core_json_path = root_ / "core.json";
        if (std::filesystem::exists(core_json_path)) {
            try {
                std::ifstream f(core_json_path);
                nlohmann::json j;
                f >> j;
                if (j.contains("file") && j["file"].is_string()) {
                    core_name = j["file"].get<std::string>();
                }
            } catch (...) {}
        }

        std::filesystem::path core_path = root_ / "core" / core_name;
        if (!std::filesystem::exists(core_path)) {
            // Fallback search in core directory for any ccs_core*.dll
            std::filesystem::path core_dir = root_ / "core";
            if (std::filesystem::exists(core_dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(core_dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".dll" &&
                        entry.path().filename().string().find("ccs_core") != std::string::npos) {
                        core_path = entry.path();
                        break;
                    }
                }
            }
        }

        if (!std::filesystem::exists(core_path)) {
            log_line(("CCS Core DLL not found at: " + core_path.string()).c_str());
            return;
        }

        core_module_ = LoadLibraryW(core_path.c_str());
        if (!core_module_) {
            log_line(("Failed to load Core DLL, error: " + std::to_string(GetLastError())).c_str());
            return;
        }

        auto get_api = reinterpret_cast<const CcsCoreApi* (*)(void)>(GetProcAddress(core_module_, "ccs_get_core_api"));
        if (!get_api) {
            log_line("Failed to get ccs_get_core_api export from Core DLL");
            FreeLibrary(core_module_);
            core_module_ = nullptr;
            return;
        }

        api_ = get_api();
        if (!api_ || api_->abi_version != CCS_ABI_VERSION) {
            log_line("CCS Core ABI version mismatch");
            FreeLibrary(core_module_);
            core_module_ = nullptr;
            api_ = nullptr;
            return;
        }

        if (api_->init(&host_) != 0) {
            log_line("CCS Core init() failed");
            FreeLibrary(core_module_);
            core_module_ = nullptr;
            api_ = nullptr;
            return;
        }

        log_line("CCS Core loaded and initialized successfully.");
    }

    void on_engine_tick(RC::Unreal::UEngine* /*engine*/, float delta) {
        if (!api_ || !api_->tick) return;

        // Hotkey check (F7)
        if (GetAsyncKeyState(VK_F7) & 1) {
            if (api_->on_hotkey) api_->on_hotkey(VK_F7);
        }

        // Resolve player context
        CcsPlayerContext ctx{};
        api_->tick(&ctx, static_cast<double>(delta));
    }

    std::filesystem::path root_;
    CcsLoaderContext host_{};
    HMODULE core_module_{nullptr};
    const CcsCoreApi* api_{nullptr};
};

} // namespace

#define CCS_MOD_API __declspec(dllexport)

extern "C" {
CCS_MOD_API RC::CppUserModBase* start_mod() {
    return new CcsLoader();
}

CCS_MOD_API void uninstall_mod(RC::CppUserModBase* mod) {
    delete mod;
}
}

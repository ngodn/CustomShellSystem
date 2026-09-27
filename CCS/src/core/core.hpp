#pragma once
#include "ccs_abi.h"
#include "settings.hpp"
#include "storage.hpp"
#include "catalog.hpp"
#include "writer.hpp"
#ifdef CCS_FRAME_PROFILE
#include "frame_profile.hpp"
#endif
#if defined(CCS_DISCOVERY_PROBE)
#include "discovery_probe.hpp"
#elif defined(CCS_ATTACK_PROBE)
#include "attack_probe.hpp"
#elif defined(CCS_SWAP_PROBE)
#include "swap_probe.hpp"
#else
#define CCS_PRODUCT 1
#include "combat.hpp"
#include "menu.hpp"
#include "discovery.hpp"
#include "status_writer.hpp"
#endif
#include <memory>
#include <filesystem>
#include <unordered_set>

namespace ccs {
class Core {
public:
    explicit Core(const CcsLoaderContext* loader);
    ~Core();
    void tick(const CcsPlayerContext* player_ctx, double delta_seconds);
    void on_hotkey(uint32_t key_code);
    void shutdown();
    bool stop();
    void report_error(const char* message) noexcept;
    const char* get_status_json();
private:
    std::filesystem::path root_dir_;
    std::unique_ptr<runtime::Writer> writer_;
    const CcsHookHost* hooks_{};
    std::string status_json_;
    bool initialized_{false}, error_reported_{false}, fault_cleanup_complete_{};
    uint64_t fault_cleanup_after_{};
#ifdef CCS_FRAME_PROFILE
    std::unique_ptr<runtime::FrameProfile> timing_;
#endif
#if defined(CCS_DISCOVERY_PROBE)
    std::unique_ptr<DiscoveryProbe> probe_;
#elif defined(CCS_ATTACK_PROBE)
    std::unique_ptr<AttackProbe> attack_probe_;
#elif defined(CCS_SWAP_PROBE)
    std::unique_ptr<SwapProbe> swap_probe_;
#else
    std::unique_ptr<runtime::Settings> settings_;
    std::unique_ptr<runtime::Storage> storage_;
    runtime::Catalog catalog_;
    std::string catalog_error_;
    std::unique_ptr<Combat> combat_;
    std::unique_ptr<Menu> menu_;
    Discovery discovery_;
    bool discovery_reported_{};
    std::unique_ptr<runtime::StatusWriter> status_;
    uint64_t status_after_{};
    std::vector<std::string> preset_names_;
    std::string selected_preset_, save_name_, last_message_, current_weapon_;
    const void* current_weapon_object_{};
    uint64_t weapon_check_{};
    uint64_t presets_listed_{};
    nlohmann::json options_;                  // the move options, built once from the catalog
    nlohmann::json model() const;
    uint64_t model_revision();
    uint64_t model_revision_{1}, model_signature_{};
    void handle_event(const nlohmann::json& event);
    void apply_slots_from_settings();
    void save_settings_or_log();
    void list_presets(uint64_t now, bool force);
    void apply_preset(const std::string& name);   // resolves each move, fills every slot, refreshes the page
    std::string move_label(const std::string& id) const;
    std::string move_group(const std::string& id) const;
    std::string move_icon(const std::string& id) const;
    std::string enemy_icon(const std::string& source) const;
    std::unordered_set<std::string> enemy_icons_;
    std::string move_description(const std::string& id) const;
#endif
};
}

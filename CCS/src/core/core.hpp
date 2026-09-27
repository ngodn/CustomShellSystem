#pragma once
#include "ccs_abi.h"
#include "settings.hpp"
#include "storage.hpp"
#include "persistence.hpp"
#include "writer.hpp"
#include "moveset_manager.hpp"
#include "menu.hpp"
#ifdef CCS_FRAME_PROFILE
#include "frame_profile.hpp"
#endif
#ifdef CCS_EXPERIMENTAL_MENU
#include "loaded_moves.hpp"
#include "content_monitor.hpp"
#endif
#ifdef CCS_DISCOVERY_PROBE
#include "discovery_probe.hpp"
#endif
#ifdef CCS_ATTACK_PROBE
#include "attack_probe.hpp"
#endif
#include <memory>
#include <filesystem>

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
    std::unique_ptr<runtime::Settings> settings_;
    std::unique_ptr<runtime::Storage> storage_;
    std::unique_ptr<runtime::Persistence> persistence_;
    std::unique_ptr<MovesetManager> movesets_;
    std::unique_ptr<Menu> menu_;
    const CcsHookHost* hooks_{};
#ifdef CCS_FRAME_PROFILE
    std::unique_ptr<runtime::FrameProfile> timing_;
#endif
#ifdef CCS_EXPERIMENTAL_MENU
    LoadedMoves loaded_moves_;
    std::string loaded_state_;
    uint64_t loaded_revision_{};
    std::unique_ptr<runtime::ContentMonitor> content_monitor_;
    std::shared_ptr<const runtime::ContentObservation> content_observation_;
    uint64_t content_poll_after_{}, content_request_after_{};
    void update_content(uint64_t now, bool visible);
#endif

#ifdef CCS_DISCOVERY_PROBE
    std::unique_ptr<DiscoveryProbe> probe_;
#endif
#ifdef CCS_ATTACK_PROBE
    std::unique_ptr<AttackProbe> attack_probe_;
#endif
    std::string status_json_;
    bool initialized_{false};
#if !defined(CCS_DISCOVERY_PROBE) && !defined(CCS_ATTACK_PROBE)
    bool open_requested_{false};
#endif
    bool error_reported_{false};
    bool fault_cleanup_complete_{};
    uint64_t fault_cleanup_after_{};
};

} // namespace ccs

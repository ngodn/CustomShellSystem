#pragma once
#include "engine.hpp"
#include "ccs_hook_api.h"
#include "writer.hpp"
#include <vector>

namespace ccs {
// F7-armed montage swap test. Reads <mod root>/swap-test.json (owning ability class name -> montage path,
// optional play-rate scale), loads and roots the montages, validates their skeleton against the player mesh,
// then rewrites MontageToPlay (and Rate) in the PlayMontageAndWaitWithNotifies pre-hook frame for the
// player's abilities only. F7 again removes the hook and releases the roots. Every swap is logged.
class SwapProbe {
public:
    SwapProbe(const CcsHookHost* host, const std::filesystem::path& root);
    void toggle();
    void tick(void* engine);
    bool stop();
    nlohmann::json status() const;
private:
    struct Entry {
        engine::FName class_name{};
        engine::ObjectHandle montage;
        bool rooted{};
        std::string class_text, montage_path;
        uint64_t hits{};
    };
    struct Row { engine::FName class_name, original, replacement; float rate_in{}, rate_out{}; uint64_t time_us{}; };
    static void callback(void*, void*, void*, void*) noexcept;
    void observe(void* frame);
    void arm(void* engine);
    void load_config();
    void release_roots();
    void drain();
    nlohmann::json host_stats() const;
    bool player_current() const;
    bool player_outer(engine::UObject* object) const;
    void emit(nlohmann::json record);
    const CcsHookHost* host_{};
    std::filesystem::path root_;
    runtime::Writer writer_;
    engine::ObjectHandle engine_, world_, pc_, pawn_, asc_, item_, shell_, function_, skeleton_;
    std::array<engine::FProperty*, 7> inputs_{};
    std::vector<Entry> entries_;
    float rate_scale_{1.0f};
    uint64_t token_{};
    std::array<Row, 64> rows_{};
    size_t head_{}, count_{};
    uint64_t session_{}, run_{}, requested_{}, retry_{}, seen_{}, swapped_{}, skipped_{}, failures_{}, maximum_us_{}, remove_after_{};
    bool wanted_{}, active_{}, reported_{true}, output_failed_{}, terminal_pending_{};
    std::string state_{"idle"}, reason_, error_;
};
}

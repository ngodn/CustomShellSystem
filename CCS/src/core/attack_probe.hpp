#pragma once
#include "engine.hpp"
#include "ccs_hook_api.h"
#include "writer.hpp"

namespace ccs {
class AttackProbe {
public:
    AttackProbe(const CcsHookHost* host, const std::filesystem::path& output);
    void toggle();
    void tick(void* engine);
    bool stop();
    nlohmann::json status() const;
private:
    struct ObjectObservation {
        engine::ObjectHandle retained;
        std::array<engine::FName, 8> names{};
        int32_t index{-1}, serial{};
        unsigned name_count{};
        bool live_at_callback{}, ancestry_complete{};
    };
    struct Row {
        ObjectObservation ability, ability_class, montage;
        engine::FName task, section, event_tag;
        uint64_t time_us{};
        float rate{}, start_time{};
        bool stop_with_ability{}, ability_active{};
    };
    static void callback(void*, void*, void*, void*) noexcept;
    void observe(void* frame);
    void begin(void* engine);
    void drain();
    static ObjectObservation observe_object(engine::UObject* object);
    static nlohmann::json describe_observation(const ObjectObservation& value);
    bool player_current() const;
    bool player_outer(engine::UObject* object) const;
    void emit(nlohmann::json record);
    const CcsHookHost* host_{};
    runtime::Writer writer_;
    engine::ObjectHandle engine_, world_, pc_, pawn_, asc_, item_, shell_, function_;
    std::array<engine::FProperty*, 7> inputs_{};
    std::array<engine::ObjectHandle, 3> event_owners_{};
    engine::FProperty* active_property_{};
    int32_t event_offset_{};
    std::array<Row, 64> rows_{};
    size_t head_{}, count_{};
    uint64_t session_{}, run_{}, token_{}, started_{}, requested_{}, retry_{}, seen_{}, recorded_{}, skipped_{}, failures_{}, maximum_us_{};
    bool wanted_{}, reported_{true}, output_failed_{}, terminal_pending_{};
    uint64_t remove_after_{};
    uint64_t identity_skipped_{};
    uint64_t unretained_calls_{};
    std::string state_{"idle"}, reason_, error_;
};
}

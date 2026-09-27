#pragma once
#include "engine.hpp"
#include "ccs_hook_api.h"
#include "writer.hpp"

namespace ccs {
// Read-only native call-site probe (schema 4). Three loader-owned pre-hooks:
//   0 factory: AbilityTask_PlayMontageAndWaitWithNotifies:PlayMontageAndWaitWithNotifies (EX_CallMath from blueprints)
//   1 ready:   GameplayTask:ReadyForActivation, filtered to the montage task class (EX_FinalFunction, context = task)
//   2 control: SpartaGameplayAbility:GetSpartaCharacterFromActorInfo, counted only (EX_FinalFunction, frequent)
// Nothing is modified; parameters are read, copied into a fixed buffer and drained on the tick.
class AttackProbe {
public:
    AttackProbe(const CcsHookHost* host, const std::filesystem::path& output);
    void toggle();
    void tick(void* engine);
    bool stop();
    nlohmann::json status() const;
private:
    enum Slot : unsigned { Factory = 0, Ready = 1, Control = 2, SlotCount = 3 };
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
        uint8_t source{};
    };
    struct HookSlot {
        engine::ObjectHandle function;
        uint64_t token{}, seen{}, matched{};
        uintptr_t func_before{}, func_after{};
        std::string path;
    };
    static void callback_factory(void*, void*, void*, void*) noexcept;
    static void callback_ready(void*, void*, void*, void*) noexcept;
    static void callback_control(void*, void*, void*, void*) noexcept;
    void guarded(unsigned slot, void* object, void* frame) noexcept;
    void observe_factory(void* frame);
    void observe_ready(void* object);
    void finish_row(Row& row, engine::UObject* ability, engine::UObject* montage);
    void begin(void* engine);
    void register_hook(unsigned slot, engine::UFunction* function, CcsNativePreHook callback);
    void drain();
    static ObjectObservation observe_object(engine::UObject* object);
    static nlohmann::json describe_observation(const ObjectObservation& value);
    nlohmann::json describe_hooks() const;
    nlohmann::json host_stats() const;
    bool player_current() const;
    bool player_outer(engine::UObject* object) const;
    void emit(nlohmann::json record);
    const CcsHookHost* host_{};
    runtime::Writer writer_;
    engine::ObjectHandle engine_, world_, pc_, pawn_, asc_, item_, shell_;
    std::array<HookSlot, SlotCount> hooks_{};
    std::array<engine::FProperty*, 7> inputs_{};
    std::array<engine::ObjectHandle, 3> event_owners_{};
    engine::ObjectHandle task_class_;
    engine::FProperty* task_montage_{}; engine::FProperty* task_rate_{}; engine::FProperty* task_section_{};
    engine::FProperty* task_stop_{}; engine::FProperty* task_start_{}; engine::FProperty* task_ability_{};
    engine::FProperty* task_instance_{};
    engine::FProperty* active_property_{};
    int32_t event_offset_{};
    std::array<Row, 64> rows_{};
    size_t head_{}, count_{};
    uint64_t session_{}, run_{}, started_{}, requested_{}, retry_{}, seen_{}, recorded_{}, skipped_{}, failures_{}, maximum_us_{};
    bool wanted_{}, reported_{true}, output_failed_{}, terminal_pending_{};
    uint64_t remove_after_{};
    uint64_t identity_skipped_{};
    uint64_t unretained_calls_{};
    std::string state_{"idle"}, reason_, error_;
};
}

#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <exception>
#include <mutex>
#include <optional>
#include <thread>
#include <string>
#include <vector>

namespace ccs::runtime {
class FrameProfile {
public:
    using Clock = std::chrono::steady_clock;
    enum class Phase : size_t { Context, Menu, Discovery, Probe, GateWait, Count };
    static constexpr size_t phase_count = static_cast<size_t>(Phase::Count);
    static constexpr size_t maximum_capacity = 16384;
    static uint64_t now_ns() noexcept;
    FrameProfile(std::filesystem::path directory, const char* source, size_t capacity = maximum_capacity);
    ~FrameProfile();
    FrameProfile(const FrameProfile&) = delete;
    FrameProfile& operator=(const FrameProfile&) = delete;
    bool arm(uint64_t now, uint64_t duration = 30'000'000'000ull);
    void begin(double engine_delta, uint64_t now);
    void record(Phase phase, uint64_t duration) noexcept;
    void end(uint64_t now, bool failed = false);
    void cancel(uint64_t now);
    bool active() const { return active_.has_value(); }
    bool recording() const { return recording_; }
    uint64_t completed() const { return completed_.load(); }
    uint64_t failures() const { return failures_.load(); }
    class Scope {
        FrameProfile& profile_;
        int exceptions_;
    public:
        Scope(FrameProfile& profile, double delta, uint64_t now = now_ns()) : profile_(profile), exceptions_(std::uncaught_exceptions()) { profile_.begin(delta, now); }
        ~Scope() noexcept {
            try { profile_.end(now_ns(), std::uncaught_exceptions() > exceptions_); }
            catch (...) { ++profile_.failures_; }
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };
    template<class Function> void measure(Phase phase, Function&& function) {
        if (!recording_) { function(); return; }
        const auto started = now_ns();
        try { function(); }
        catch (...) { record(phase, now_ns() - started); throw; }
        record(phase, now_ns() - started);
    }
private:
    struct Row {
        uint64_t started{}, elapsed{}, interval{}, engine{};
        std::array<uint64_t, phase_count> phases{};
        uint32_t measured{};
        bool interval_valid{}, engine_valid{}, failed{};
    };
    enum class State { Free, Collecting, Ready, Writing };
    enum class Reason { Duration, Capacity, Cancelled, Shutdown };
    struct Batch {
        State state{State::Free};
        Reason reason{Reason::Duration};
        uint64_t started{}, until{}, finished{};
        size_t count{};
        std::vector<Row> rows;
    };
    bool submit();
    void worker_loop();
    void publish(const Batch& batch);
    std::filesystem::path directory_;
    std::string source_;
    std::array<Batch, 2> batches_;
    std::mutex gate_;
    std::condition_variable cv_;
    std::optional<size_t> active_;
    Row row_;
    uint64_t previous_{};
    bool have_previous_{}, recording_{}, finishing_{}, stopping_{};
    std::atomic<uint64_t> completed_{}, failures_{};
    std::thread worker_;
};
}

#include "frame_profile.hpp"
#include "common.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>
#include <nlohmann/json.hpp>

namespace ccs::runtime {
namespace {
using Json = nlohmann::json;
constexpr std::array<const char*, FrameProfile::phase_count> phase_names{"context", "menu", "discovery", "probe", "gate_wait"};
Json summarize(std::vector<uint64_t> samples) {
    if (samples.empty()) return {{"count", 0}};
    std::sort(samples.begin(), samples.end());
    long double sum = 0;
    for (auto value : samples) sum += value;
    const auto quantile = [&](size_t percent) {
        const auto rank = (samples.size() * percent + 99) / 100;
        return static_cast<double>(samples[rank - 1]) / 1000;
    };
    return {{"count", samples.size()}, {"mean_us", static_cast<double>(sum / samples.size()) / 1000},
        {"p95_us", quantile(95)}, {"p99_us", quantile(99)},
        {"minimum_us", static_cast<double>(samples.front()) / 1000},
        {"maximum_us", static_cast<double>(samples.back()) / 1000}};
}
}
uint64_t FrameProfile::now_ns() noexcept {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}
FrameProfile::FrameProfile(std::filesystem::path directory, const char* source, size_t capacity)
    : directory_(std::move(directory)), source_(source ? source : "") {
    if ((source_ != "core" && source_ != "loader" && source_ != "test") || !capacity || capacity > maximum_capacity)
        throw std::runtime_error("Invalid frame profiler configuration");
    for (auto& batch : batches_) batch.rows.resize(capacity);
    worker_ = std::thread([this] { worker_loop(); });
}
FrameProfile::~FrameProfile() {
    {
        std::lock_guard lock(gate_);
        if (active_) {
            auto& batch = batches_[*active_];
            batch.reason = Reason::Shutdown; batch.finished = now_ns(); batch.state = State::Ready;
            active_.reset();
        }
        stopping_ = true;
    }
    cv_.notify_one();
    worker_.join();
}
bool FrameProfile::arm(uint64_t now, uint64_t duration) {
    if (active_ || duration < 3'000'000'000ull || duration > 30'000'000'000ull ||
        now > std::numeric_limits<uint64_t>::max() - duration) return false;
    std::unique_lock lock(gate_, std::try_to_lock);
    if (!lock || stopping_) return false;
    for (size_t i = 0; i < batches_.size(); ++i) {
        auto& batch = batches_[i];
        if (batch.state != State::Free) continue;
        batch.state = State::Collecting; batch.started = now; batch.until = now + duration;
        batch.finished = 0; batch.count = 0;
        active_ = i; previous_ = 0; have_previous_ = false; recording_ = false; finishing_ = false;
        return true;
    }
    return false;
}
bool FrameProfile::submit() {
    std::unique_lock lock(gate_, std::try_to_lock);
    if (!lock) return false;
    batches_[*active_].state = State::Ready;
    active_.reset(); finishing_ = false;
    lock.unlock(); cv_.notify_one();
    return true;
}
void FrameProfile::begin(double delta, uint64_t now) {
    recording_ = false;
    if (!active_) return;
    if (finishing_) { submit(); return; }
    row_ = {}; row_.started = now;
    row_.interval_valid = have_previous_ && now >= previous_;
    if (row_.interval_valid) row_.interval = now - previous_;
    row_.engine_valid = std::isfinite(delta) && delta >= 0 && delta <= 60;
    if (row_.engine_valid) row_.engine = static_cast<uint64_t>(delta * 1'000'000'000.0);
    recording_ = true;
}
void FrameProfile::record(Phase phase, uint64_t duration) noexcept {
    const auto index = static_cast<size_t>(phase);
    if (!recording_ || index >= phase_count) return;
    auto& value = row_.phases[index];
    value = duration > std::numeric_limits<uint64_t>::max() - value ? std::numeric_limits<uint64_t>::max() : value + duration;
    row_.measured |= 1u << index;
}
void FrameProfile::end(uint64_t now, bool failed) {
    if (!recording_ || !active_) return;
    recording_ = false;
    row_.elapsed = now >= row_.started ? now - row_.started : 0;
    row_.failed = failed || now < row_.started;
    auto& batch = batches_[*active_];
    batch.rows[batch.count++] = row_;
    previous_ = row_.started; have_previous_ = true;
    if (now >= batch.until || batch.count == batch.rows.size()) {
        batch.reason = batch.count == batch.rows.size() ? Reason::Capacity : Reason::Duration;
        batch.finished = now; finishing_ = true; submit();
    }
}
void FrameProfile::cancel(uint64_t now) {
    if (!active_) return;
    if (recording_) end(now, true);
    if (!active_) return;
    auto& batch = batches_[*active_];
    batch.reason = Reason::Cancelled; batch.finished = now; finishing_ = true; submit();
}
void FrameProfile::worker_loop() {
    for (;;) {
        size_t index = batches_.size();
        {
            std::unique_lock lock(gate_);
            cv_.wait(lock, [&] {
                return stopping_ || std::any_of(batches_.begin(), batches_.end(), [](const Batch& b) { return b.state == State::Ready; });
            });
            for (size_t i = 0; i < batches_.size(); ++i)
                if (batches_[i].state == State::Ready && (index == batches_.size() || batches_[i].started < batches_[index].started)) index = i;
            if (index == batches_.size()) { if (stopping_) return; continue; }
            batches_[index].state = State::Writing;
        }
        try { publish(batches_[index]); ++completed_; }
        catch (...) { ++failures_; }
        {
            std::lock_guard lock(gate_);
            batches_[index].state = State::Free;
        }
    }
}
void FrameProfile::publish(const Batch& batch) {
    constexpr std::array<const char*, 4> reasons{"duration", "frame_capacity", "cancelled", "shutdown"};
    Json report{{"schema", 1}, {"source", source_}, {"started_ns", batch.started}, {"finished_ns", batch.finished},
        {"stop_reason", reasons[static_cast<size_t>(batch.reason)]}, {"frames", batch.count},
        {"scope", "Game-thread callback elapsed time and engine-tick cadence. Not GPU presentation timing."},
        {"percentile_method", "nearest rank"}};
    std::vector<uint64_t> elapsed, intervals, engine;
    elapsed.reserve(batch.count); intervals.reserve(batch.count); engine.reserve(batch.count);
    uint64_t failed = 0;
    std::vector<size_t> worst(batch.count);
    for (size_t i = 0; i < batch.count; ++i) {
        const auto& row = batch.rows[i];
        elapsed.push_back(row.elapsed);
        if (row.interval_valid) intervals.push_back(row.interval);
        if (row.engine_valid) engine.push_back(row.engine);
        failed += row.failed; worst[i] = i;
    }
    report["elapsed"] = summarize(std::move(elapsed)); report["tick_interval"] = summarize(std::move(intervals));
    report["engine_delta"] = summarize(std::move(engine)); report["failed_frames"] = failed;
    for (size_t p = 0; p < phase_count; ++p) {
        std::vector<uint64_t> samples; samples.reserve(batch.count);
        for (size_t i = 0; i < batch.count; ++i)
            if (batch.rows[i].measured & (1u << p)) samples.push_back(batch.rows[i].phases[p]);
        report["phases"][phase_names[p]] = summarize(std::move(samples));
    }
    const auto take = std::min<size_t>(16, worst.size());
    std::partial_sort(worst.begin(), worst.begin() + take, worst.end(), [&](size_t a, size_t b) {
        return batch.rows[a].interval > batch.rows[b].interval;
    });
    report["largest_intervals"] = Json::array();
    for (size_t i = 0; i < take; ++i) {
        const auto& row = batch.rows[worst[i]];
        if (!row.interval_valid || worst[i] == 0) continue;
        // The preceding callback's CCS work is inside this start-to-start interval.
        const auto& previous = batch.rows[worst[i] - 1];
        report["largest_intervals"].push_back({{"frame", worst[i]}, {"interval_ns", row.interval},
            {"preceding_callback_ns", previous.elapsed}, {"preceding_phases_ns", previous.phases},
            {"current_callback_ns", row.elapsed}, {"current_phases_ns", row.phases}});
    }
    std::ostringstream csv;
    csv.imbue(std::locale::classic());
    csv << "frame,started_ns,elapsed_ns,interval_ns,interval_valid,engine_ns,engine_valid,failed,measured_mask";
    for (const auto* name : phase_names) csv << ',' << name << "_ns";
    csv << '\n';
    for (size_t i = 0; i < batch.count; ++i) {
        const auto& r = batch.rows[i];
        csv << i << ',' << r.started << ',' << r.elapsed << ',' << r.interval << ',' << r.interval_valid << ','
            << r.engine << ',' << r.engine_valid << ',' << r.failed << ',' << r.measured;
        for (auto value : r.phases) csv << ',' << value;
        csv << '\n';
    }
    const auto filename = source_ + "-" + std::to_string(batch.started);
    if (!write_file_atomic(directory_ / (filename + ".csv"), csv.str()) ||
        !write_file_atomic(directory_ / (filename + ".json"), report.dump(2)))
        throw std::runtime_error("Frame profile write failed");
}
}
